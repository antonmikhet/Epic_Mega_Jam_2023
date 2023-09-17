// Copyright Sam Bonifacio 2021. All Rights Reserved.

#pragma once

#include "CollisionQueryParams.h"
#include "CoreMinimal.h"
#include "TetherCableSimulationOptions.h"
#include "TetherSegmentSimulationOptions.h"
#include "TetherSimulationSegmentSeries.h"
#include "PhysicsEngine/BodyInstance.h"
#include "TetherSimulationParams.generated.h"

struct FTetherSimulationModel;

USTRUCT()
struct TETHER_API FTetherSegmentSimulationParams
{
	GENERATED_BODY()
	
	// If true, the segment following this point (if any) should be simulated
	UPROPERTY()
	bool bShouldSimulateSegment = true;

	UPROPERTY()
	FTetherSegmentSimulationOptions SimulationOptions;
};

FORCEINLINE uint32 GetTypeHash(const FTetherSegmentSimulationParams& InParams)
{
	const uint32 Hash = HashCombine(GetTypeHash(InParams.bShouldSimulateSegment), GetTypeHash(InParams.SimulationOptions));
	return Hash;
}

UENUM()
enum class ETetherSimulationTimingMode : uint8
{
	SimulateInSequence, // Simulate all segments, one at a time, by the desired time
	ProgressSimulation // Progresses the simulation by the desired time, whichever segment is active
};

USTRUCT()
struct TETHER_API FTetherSimulationParams
{
	GENERATED_BODY()
	
	FTetherSimulationParams() {};

	FTetherSimulationParams(UWorld* InWorld);

	UPROPERTY()
	TWeakObjectPtr<UWorld> World;

	UPROPERTY()
	FString SimulationName = FString();

	UPROPERTY()
	ETetherSimulationTimingMode TimingMode = ETetherSimulationTimingMode::ProgressSimulation;

	UPROPERTY()
	float CollisionWidth = 0.f;

	UPROPERTY()
	float DesiredParticleDistance = 0.f;
	
	UPROPERTY()
	FVector CableForce = FVector::ZeroVector;

	UPROPERTY()
	FTetherCableSimulationOptions SimulationOptions;

	UPROPERTY()
	TArray<FTetherSegmentSimulationParams> SegmentParams;

	FCollisionQueryParams CollisionQueryParams;

	TArray<FBodyInstance*> BodyInstances;

	// Note: Be careful about accessing the owning actor and component on the worker thread
	// They may be destroyed on the main thread while the simulation is running
	TWeakObjectPtr<const AActor> OwningActor;
	TWeakObjectPtr<UPrimitiveComponent> Component;

	bool ShouldSimulateSegment(int32 SegmentIndex) const
	{
		if(!SegmentParams.IsValidIndex(SegmentIndex))
		{
			// If there is no segment params available, default to should simulate true
			return true;
		}
		
		return SegmentParams[SegmentIndex].bShouldSimulateSegment;
	}

	TArray<struct FTetherSimulationSegment*> GetSegmentsToSimulate(FTetherSimulationModel& Model) const;

	TArray<FTetherProxySimulationSegmentSeries> MakeSegmentSeriesToSimulate(FTetherSimulationModel& Model, bool bIncludeEmptySegments = false) const;

	int32 GetNumSegmentsToSimulate() const;

	// Sets all segments with indices contained in the given array to simulate, and all other segments to not simulate
	void SetSegmentsToSimulate(TArray<int32>& SegmentIndices);
};

FORCEINLINE uint32 GetTypeHash(const FTetherSimulationParams& InParams)
{
	uint32 Hash = HashCombine(GetTypeHash(InParams.World), GetTypeHash(InParams.SimulationName));
	Hash = HashCombine(Hash, GetTypeHash(InParams.CableForce));
	Hash = HashCombine(Hash, GetTypeHash(InParams.SimulationOptions));
	for(const FTetherSegmentSimulationParams& SegmentParams : InParams.SegmentParams)
	{
		Hash = HashCombine(Hash, GetTypeHash(SegmentParams));
	}

	// Manually hash relevant parts of CollisionQueryParams
	// May not be reliable
	Hash = HashCombine(Hash, GetTypeHash(InParams.CollisionQueryParams.bTraceComplex));
	for(const uint32 Actor : InParams.CollisionQueryParams.GetIgnoredActors())
	{
		Hash = HashCombine(Hash, GetTypeHash(Actor));
	}
	for(const uint32 Component : InParams.CollisionQueryParams.GetIgnoredComponents())
	{
		Hash = HashCombine(Hash, GetTypeHash(Component));
	}
	
	return Hash;
}

FORCEINLINE uint32 GetBodyInstanceHash(const FTetherSimulationParams& InParams)
{
	uint32 Hash = GetTypeHash(InParams.BodyInstances.Num());
	for(const FBodyInstance* Instance : InParams.BodyInstances)
	{
		const FTransform Transform = Instance->GetUnrealWorldTransform();
		Hash = HashCombine(Hash, GetTypeHash(Transform.GetLocation()));
		Hash = HashCombine(Hash, GetTypeHash(Transform.GetRotation().Euler()));
		Hash = HashCombine(Hash, GetTypeHash(Transform.GetScale3D()));
	}
	return Hash;
}

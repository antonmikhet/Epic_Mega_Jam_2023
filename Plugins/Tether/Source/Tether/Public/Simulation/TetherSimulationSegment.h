// Copyright Sam Bonifacio 2021. All Rights Reserved.

#pragma once
#include "SplineSegmentInfo.h"
#include "TetherSimulationSegment.generated.h"

USTRUCT()
struct TETHER_API FTetherSimulationParticle
{
	GENERATED_BODY()

	FTetherSimulationParticle(){}
	
	FTetherSimulationParticle(bool bInFree, FVector InPosition)
		: bFree(bInFree)
		, Position(InPosition)
		, OldPosition(InPosition)
	{}

	UPROPERTY()
	uint32 ParticleUniqueId = -1;
	
	/** If this point is free (simulating) or fixed to something */
	UPROPERTY()
	bool bFree = true;
	/** Current position of point */
	UPROPERTY()
	FVector Position = FVector::ZeroVector;
	/** Position of point on previous iteration */
	UPROPERTY()
	FVector OldPosition = FVector::ZeroVector;
};

FORCEINLINE uint32 GetTypeHash(const FTetherSimulationParticle& InParticle)
{
	const uint32 Hash = HashCombine(GetTypeHash(InParticle.bFree), GetTypeHash(InParticle.Position));
	return HashCombine(Hash, GetTypeHash(InParticle.OldPosition));
}

USTRUCT()
struct TETHER_API FTetherSimulationSegment
{
	GENERATED_BODY()
	
	FTetherSimulationSegment()
	{
	}

	FTetherSimulationSegment(bool bStartInvalidated)
	{
		SetInvalidated(bStartInvalidated);
	}

	// Unique ID of the segment - also the array index in the Model
	UPROPERTY()
	uint32 SegmentUniqueId = 0;

	// Segment info in world space
	UPROPERTY()
	FSplineSegmentInfo SplineSegmentInfo;

	// Physical length of cable segment including slack
	UPROPERTY()
	float Length = 0.f;

	UPROPERTY()
	TArray<FTetherSimulationParticle> Particles = {};

	UPROPERTY()
	float SimulationTime = 0.f;

	int32 GetNumParticles() const { return Particles.Num(); }

	int32 GetNumParticleSegments() const { return GetNumParticles() - 1; }

	TArray<FVector> GetParticleLocations() const;

	float GetParticleSegmentLength() const { return Length / (float)GetNumParticleSegments(); }

	FTetherSimulationParticle& GetParticle(int32 Index) { return Particles[Index]; }

	bool IsValid() const;

	void SetInvalidated(bool bInvalidate);
	bool IsInvalidated() const { return bInvalidated; }

	void BuildParticles(float ParticleSegmentLength, bool bStartFixed = true, bool bEndFixed = true);

private:

	// Is the segment invalid and in need of being resimulated
	// False if the segment is up to date, or if there is an async simulation running with the
	UPROPERTY()
	bool bInvalidated = false; 

};

FORCEINLINE uint32 GetTypeHash(const FTetherSimulationSegment& InSegment)
{
	uint32 Hash = GetTypeHash(InSegment.SplineSegmentInfo);
	Hash = HashCombine(Hash, GetTypeHash(InSegment.Length));
	for(const FTetherSimulationParticle& Particle : InSegment.Particles)
	{
		Hash = HashCombine(Hash, GetTypeHash(Particle));
	}
	return HashCombine(Hash, GetTypeHash(InSegment.IsInvalidated()));
}

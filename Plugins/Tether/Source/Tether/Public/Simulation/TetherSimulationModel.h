// Copyright Sam Bonifacio 2021. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TetherSimulationSegment.h"
#include "TetherSimulationSegmentSeries.h"

#include "TetherSimulationModel.generated.h"

USTRUCT()
struct TETHER_API FTetherSimulationModel : public FTetherSimulationSegmentSeries
{
	GENERATED_BODY()

	// Base transform of the cable that the simulation was started with
	UPROPERTY()
	FTransform SimulationBaseWorldTransform = FTransform();

	UPROPERTY()
	TArray<FTetherSimulationSegment> Segments = {};

	virtual int32 GetNumSegments() const override { return Segments.Num(); }
	virtual FTetherSimulationSegment* GetSegment(int32 SegmentIndex) override { return &Segments[SegmentIndex]; }
	virtual const FTetherSimulationSegment* GetSegmentConst(int32 SegmentIndex) const override { return &Segments[SegmentIndex]; }

	void UpdateNumSegments(int32 SegmentNum);
	void UpdateSegmentUniqueIds();

	TArray<int32> GetInvalidatedSegments() const;

	// Gets all particle locations without duplicates
	TArray<FVector> GetParticleLocations() const;

	// Makes a new series including all segments up until and including the given index
	FTetherProxySimulationSegmentSeries MakeSeriesUpTo(int32 LastSegmentIndex);

	virtual bool IsValid() const override;
};

FORCEINLINE uint32 GetTypeHash(const FTetherSimulationModel& InSimulationModel)
{
	uint32 Hash = HashCombine(GetTypeHash(InSimulationModel.SimulationBaseWorldTransform.GetTranslation()), GetTypeHash(InSimulationModel.SimulationBaseWorldTransform.GetRotation().Euler()));
	Hash = HashCombine(Hash, GetTypeHash(InSimulationModel.SimulationBaseWorldTransform.GetScale3D()));
	Hash = HashCombine(Hash, GetTypeHash(InSimulationModel.Segments.Num()));
	for(const FTetherSimulationSegment& Segment : InSimulationModel.Segments)
	{
		Hash = HashCombine(Hash, GetTypeHash(Segment));
	}
	return Hash;
}

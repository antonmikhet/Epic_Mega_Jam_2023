// Copyright Sam Bonifacio 2021. All Rights Reserved.

#include "Simulation/TetherSimulationModel.h"

void FTetherSimulationModel::UpdateNumSegments(int32 SegmentNum)
{
	if(Segments.Num() > SegmentNum)
	{
		Segments.SetNum(SegmentNum);
	}
	else if(Segments.Num() < SegmentNum)
	{
		const int32 SegmentsToAdd = SegmentNum - Segments.Num();
		for(int32 i=0; i< SegmentsToAdd; i++)
		{
			const int32 NewIndex = Segments.Add(FTetherSimulationSegment(true));
			Segments[NewIndex].SegmentUniqueId = NewIndex;
		}
	}
	
}

void FTetherSimulationModel::UpdateSegmentUniqueIds()
{
	// Update unique IDs of all segments to match the array index in the model
	for (int32 i = 0; i < Segments.Num(); i++)
	{
		Segments[i].SegmentUniqueId = i;
	}
}

TArray<int32> FTetherSimulationModel::GetInvalidatedSegments() const
{
	TArray<int32> Result;
	for(int i=0; i< Segments.Num(); i++)
	{
		if(Segments[i].IsInvalidated())
		{
			Result.Add(i);
		}
	}
	return Result;
}

TArray<FVector> FTetherSimulationModel::GetParticleLocations() const
{
	TArray<FVector> Result;
	for(int i=0; i< Segments.Num(); i++)
	{
		if(i>0)
		{
			// Remove last particle of previous segment, since the first particle of the next segment will be the same
			Result.RemoveAt(Result.Num() - 1);
		}
		Result.Append(Segments[i].GetParticleLocations());
	}
	return Result;
}

FTetherProxySimulationSegmentSeries FTetherSimulationModel::MakeSeriesUpTo(int32 LastSegmentIndex)
{
	FTetherProxySimulationSegmentSeries Series;
	for(int32 i = 0; i<= LastSegmentIndex; i++)
	{
		FTetherSimulationSegment& Segment = Segments[i];
		Series.Segments.Add(&Segment);
	}
	return Series;
}

bool FTetherSimulationModel::IsValid() const
{
	// Check base FTetherSimulationSegmentSeries validation
	if(!Super::IsValid())
	{
		return false;
	}

	// Check segment unique IDs match indices
	for(int32 i=0; i< Segments.Num(); i++)
	{
		if(Segments[i].SegmentUniqueId != i)
		{
			return false;
		}
	}

	return true;
}

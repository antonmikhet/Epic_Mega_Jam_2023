// Copyright Sam Bonifacio 2021. All Rights Reserved.

#include "Simulation/TetherSimulationParams.h"
#include "Engine/World.h"
#include "Simulation/TetherSimulationModel.h"

FTetherSimulationParams::FTetherSimulationParams(UWorld* InWorld)
	: World(InWorld)
{
	if (!ensure(IsInGameThread()))
	{
		// This constructor is only available in the game thread
		return;
	}

	if (CableForce.IsZero())
	{
		CableForce = FVector(0.f, 0.f, InWorld->GetGravityZ());
	}

}

TArray<FTetherSimulationSegment*> FTetherSimulationParams::GetSegmentsToSimulate(FTetherSimulationModel& Model) const
{
	TArray<FTetherSimulationSegment*> Result;
	for(int32 i=0; i<Model.Segments.Num();i++)
	{
		if(ShouldSimulateSegment(i))
		{
			FTetherSimulationSegment& Segment = Model.Segments[i];
			Result.Add(&Segment);
		}
	}
	return Result;
}

TArray<FTetherProxySimulationSegmentSeries> FTetherSimulationParams::MakeSegmentSeriesToSimulate(FTetherSimulationModel& Model, bool bIncludeEmptySegments) const
{
	// First, build series of connected segments
	TArray<FTetherProxySimulationSegmentSeries> AllSeries;
	
	for(int32 i=0; i<Model.Segments.Num();i++)
	{
		bool bStartSeries = false;
		if(i==0 || SegmentParams[i].SimulationOptions.bFixedAnchorPoint)
		{
			bStartSeries = true;
		}

		if(bStartSeries)
		{
			AllSeries.AddDefaulted();
		}

		FTetherSimulationSegment& Segment = Model.Segments[i];
		// We assume the segment unique ID matches the index of the segment in the model
		ensure(i == Segment.SegmentUniqueId); 
		AllSeries.Last().Segments.Add(&Segment);
	}

	// Remove series that don't have any segments that should be simulated
	for(int32 i = AllSeries.Num() - 1; i >= 0; i--)
	{
		FTetherProxySimulationSegmentSeries& Series = AllSeries[i];
		bool bShouldSimulateSeries = false;
		for(FTetherSimulationSegment* Segment : Series.Segments)
		{
			const bool bShouldSimulateSegment = ShouldSimulateSegment(Segment->SegmentUniqueId) && (bIncludeEmptySegments || Segment->GetNumParticles() > 0);
			if(bShouldSimulateSegment)
			{
				bShouldSimulateSeries = true;
				break;
			}
		}
		if(!bShouldSimulateSeries)
		{
			AllSeries.RemoveAt(i);
		}
	}

	return AllSeries;
}

int32 FTetherSimulationParams::GetNumSegmentsToSimulate() const
{
	int32 NumSegmentsToSimulate = 0;
	for(int32 i = 0; i< SegmentParams.Num(); i++)
	{
		if(ShouldSimulateSegment(i))
		{
			NumSegmentsToSimulate++;
		}
	}
	return NumSegmentsToSimulate;
}

void FTetherSimulationParams::SetSegmentsToSimulate(TArray<int32>& SegmentIndices)
{
	for(int32 i=0; i< SegmentParams.Num();i++)
	{
		SegmentParams[i].bShouldSimulateSegment = SegmentIndices.Contains(i);
	}

	// The last segment should never simulate, because it's only a single point
	SegmentParams.Last().bShouldSimulateSegment = false;
}

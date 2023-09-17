// Copyright Sam Bonifacio 2021. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "TetherSimulationSegmentSeries.h"

struct FTetherSimulationResultInfo;
struct FTetherSimulationModel;
struct FTetherSimulationParams;

struct FTetherSimulationContext
{	
	FTetherSimulationModel& Model;
	const FTetherSimulationParams& Params;
	FTetherSimulationResultInfo& ResultInfo;

	FTetherSimulationContext(FTetherSimulationModel& InModel, const FTetherSimulationParams& InParams, FTetherSimulationResultInfo& InResultInfo)
		: Model(InModel)
		, Params(InParams)
		, ResultInfo(InResultInfo)
	{
	}
};

struct FTetherSimulationSubstepContext
{	
	FTetherSimulationContext& SimulationContext;
	int32 SubstepNum = -1;
	TArray<FTetherProxySimulationSegmentSeries>& SegmentsToSimulate;

	FTetherSimulationSubstepContext(FTetherSimulationContext& InSimulationContext, TArray<FTetherProxySimulationSegmentSeries>& InSegmentsToSimulate)
		: SimulationContext(InSimulationContext)
		, SegmentsToSimulate(InSegmentsToSimulate)
	{
		
	}
};

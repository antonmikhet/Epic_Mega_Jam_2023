// Copyright Sam Bonifacio 2021. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "TetherSimulationResultInfo.h"
#include "TetherSimulationSegmentSeries.h"

class FProgressCancel;
struct FTetherSimulationContext;
struct FTetherSimulationSubstepContext;
struct FTetherSimulationSegment;
struct FTetherSimulationModel;
struct FTetherSimulationParams;

DECLARE_LOG_CATEGORY_EXTERN(LogTetherSimulation, Log, All);

class TETHER_API FTetherSimulation
{
public:

	/**
	 * Simulate the specified model for the specified amount of time
	 * @param	SimulationTime	Time in seconds to simulate. If zero, simulate the entire maximum duration in the params.
	 */
	static FTetherSimulationResultInfo PerformSimulation(FTetherSimulationModel& Model, float SimulationTime, const FTetherSimulationParams& Params, FProgressCancel* Progress = nullptr);

private:

	static void PerformSimulationSubstep(FTetherSimulationContext& SimulationContext, TArray<FTetherProxySimulationSegmentSeries> SegmentsToSimulate, float SubstepTime, int32 SubstepNum);

	static void VerletIntegrateSegment(FTetherSimulationSubstepContext& SubstepContext, FTetherProxySimulationSegmentSeries& Segment, float SubstepTime);

	static void SolveConstraintsForSegment(FTetherSimulationSubstepContext& SubstepContext, FTetherProxySimulationSegmentSeries& Segment, float ForceMultiplier);

	static void PerformCollision(FTetherSimulationSubstepContext& SubstepContext, FTetherProxySimulationSegmentSeries& SimulatingSegmentSeries, float ForceMultiplier);
	
};

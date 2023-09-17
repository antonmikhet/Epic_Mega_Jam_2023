// Copyright Sam Bonifacio 2021. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"

struct TETHER_API FTetherSimulationResultInfo
{

	FTetherSimulationResultInfo(){}
	
	FTetherSimulationResultInfo(float InSimulatedTime)
		: SimulatedTime(InSimulatedTime)
	{
	}
	
	/**
	 * Indices of cable segments that were simulated
	 */
	TArray<int32> SimulatedSegments;

	/**
	 * Time that was actually simulated
	 */ 
	float SimulatedTime = 0.f;
	
	/**
	 * Simulation time left over that wasn't enough to do another substep
	 */
	float SimulationTimeRemainder = 0.f;

	/**
	 * Components that were hit during the simulation
	 */
	TArray<TWeakObjectPtr<class UPrimitiveComponent>> HitComponents;

	/**
	 * Number of collision hits during the simulation
	 * Useful for debugging simulation determinism
	 */
	int32 NumCollisionHits = 0;
};

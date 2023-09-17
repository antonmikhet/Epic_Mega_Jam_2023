// Copyright Sam Bonifacio 2021. All Rights Reserved.

#pragma once
#include "TetherSimulationInstanceResources.generated.h"

struct FTetherSimulationModel;
struct FTetherSimulationParams;

/**
 * Manages resources for a single simulation instance that need to be accessible by reference on the simulation worker thread
 * This needs to be created before a simulation runs and destroyed when it is finished, both on the game thread
 */
USTRUCT()
struct TETHER_API FTetherSimulationInstanceResources
{
    GENERATED_BODY()

    UPROPERTY()
    bool bIsInitialized = false;

	UPROPERTY()
	TWeakObjectPtr<UWorld> World;
    
    UPROPERTY()
    class UBodySetup* BodySetup = nullptr;

    TArray<struct FBodyInstance*> BodyInstances = {};

    FTetherSimulationInstanceResources();

    ~FTetherSimulationInstanceResources();

    void InitializeResources(FTetherSimulationModel& Model, FTetherSimulationParams& Params);
    void ReleaseResources();
};

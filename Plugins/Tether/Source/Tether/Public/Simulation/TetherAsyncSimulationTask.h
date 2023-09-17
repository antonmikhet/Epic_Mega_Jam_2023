// Copyright Sam Bonifacio 2021. All Rights Reserved.

#pragma once

#include "TetherSimulationInstanceResources.h"
#include "TetherSimulationModel.h"
#include "TetherSimulationParams.h"
#include "TetherSimulationResultInfo.h"
#include "Async/AsyncWork.h"
#include "TaskTypes.h"

DECLARE_DELEGATE_TwoParams(FOnTetherAsyncSimulationCompleteDelegate, const FTetherSimulationModel&, const FTetherSimulationResultInfo&);

class TETHER_API FTetherAsyncSimulationTask : public FAbortableBackgroundTask
{
	friend class FAutoDeleteAsyncTask<FTetherAsyncSimulationTask>;

public:
	FTetherAsyncSimulationTask(FOnTetherAsyncSimulationCompleteDelegate Callback, FTetherSimulationModel& Model, float SimulationTime, const FTetherSimulationParams& Params);

	~FTetherAsyncSimulationTask();

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FTetherAsyncSimulationTask, STATGROUP_ThreadPoolAsyncTasks);
	}

	void DoWork();

private:
	
	FOnTetherAsyncSimulationCompleteDelegate Callback;
	FTetherSimulationModel Model;
	float SimulationTime;
	FTetherSimulationParams Params;

	// This needs to be a pointer so the resources can outlive the task itself and be destroyed back on the game thread
	FTetherSimulationInstanceResources* Resources = nullptr;
};

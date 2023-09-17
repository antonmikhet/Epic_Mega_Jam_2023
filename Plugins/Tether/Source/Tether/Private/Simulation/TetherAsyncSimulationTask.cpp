// Copyright Sam Bonifacio 2021. All Rights Reserved.

#include "Simulation/TetherAsyncSimulationTask.h"
#include "Simulation/TetherSimulation.h"
#include "Async/Async.h"
#include "TetherLogs.h"

FTetherAsyncSimulationTask::FTetherAsyncSimulationTask(FOnTetherAsyncSimulationCompleteDelegate Callback, FTetherSimulationModel& InModel, float SimulationTime, const FTetherSimulationParams& InParams)
	: Callback(Callback)
	, Model(InModel)
	, SimulationTime(SimulationTime)
	, Params(InParams)
{
	Resources = new FTetherSimulationInstanceResources();
	Resources->InitializeResources(Model, Params);
}

FTetherAsyncSimulationTask::~FTetherAsyncSimulationTask()
{
	// If this gets hit, there may be a leak from resources not being cleaned up
	ensure(!Resources);
}

void FTetherAsyncSimulationTask::DoWork()
{
	ensure(!IsInGameThread());
	UE_LOG(LogTetherCable, Verbose, TEXT("FTetherAsyncSimulationTask: Work started, IsInGameThread: %i"), (int32)IsInGameThread());

	ensure(IsValid(Params.World.Get()) || !Params.SimulationOptions.bEnableCollision);

	FTetherSimulationResultInfo ResultInfo = FTetherSimulation::PerformSimulation(Model, SimulationTime, Params, GetProgress());

	FOnTetherAsyncSimulationCompleteDelegate OutCallback = Callback;
	FTetherSimulationModel OutModel = Model;

	FTetherSimulationInstanceResources* CaptureResources = Resources;
	Resources = nullptr;
	
	const bool bCancelled = GetProgress()->Cancelled();

	UE_LOG(LogTetherCable, Verbose, TEXT("FTetherAsyncSimulationTask: Calling back to game thread"));
	AsyncTask(ENamedThreads::GameThread, [OutCallback, OutModel, ResultInfo, CaptureResources, bCancelled]()
	{
		UE_LOG(LogTetherCable, Verbose, TEXT("FTetherAsyncSimulationTask: Back on game thread"));
		check(CaptureResources);
		CaptureResources->ReleaseResources();
		delete CaptureResources;
		if(!bCancelled)
		{
			OutCallback.ExecuteIfBound(OutModel, ResultInfo);
		}
	});
	
}

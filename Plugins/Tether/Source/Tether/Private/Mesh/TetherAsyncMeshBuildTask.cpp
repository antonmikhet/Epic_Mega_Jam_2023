// Copyright Sam Bonifacio 2021. All Rights Reserved.

#include "Mesh/TetherAsyncMeshBuildTask.h"


#if WITH_EDITOR

#include "Async/Async.h"
#include "Mesh/CableMeshGeneration.h"
#include "TetherLogs.h"
#include "Engine/StaticMesh.h"

FTetherAsyncMeshBuildTask::FTetherAsyncMeshBuildTask(FTetherAsyncMeshBuildTaskParams InParams)
    : Params(InParams)
{
}


void FTetherAsyncMeshBuildTask::DoWork()
{
	UE_LOG(LogTetherCable, Verbose, TEXT("FTetherAsyncMeshBuildTask: Work started, IsInGameThread: %i"), (int32)IsInGameThread());

    if(GetProgress()->Cancelled())
    {
        return;
    }

    bool bBuildSuccess;
    {
        TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("Build Mesh Description"))
	    bBuildSuccess = Params.MeshGenerator->BuildMeshDescription(Params.CurveDescription, Params.CableWidth, Params.MeshDescription, GetProgress());
    }

    if(!bBuildSuccess)
    {
        UE_LOG(LogTetherCable, Error, TEXT("FTetherAsyncMeshBuildTask: Build failed: BuildMeshDescription returned false"));
    }

	const bool bCancelled = GetProgress()->Cancelled();

    UE_LOG(LogTetherCable, Verbose, TEXT("FTetherAsyncMeshBuildTask: Calling back to game thread"));

    if(!IsInGameThread())
    {
        Async(EAsyncExecution::TaskGraphMainThread, [this, bCancelled]()
        {
            BuildOnGameThread(bCancelled);
        });
    }
    else
    {
        BuildOnGameThread(bCancelled);
    }

}

void FTetherAsyncMeshBuildTask::BuildOnGameThread(bool bCancelled)
{
    UE_LOG(LogTetherCable, VeryVerbose, TEXT("FTetherAsyncMeshBuildTask: Back on game thread"));

    if (bCancelled)
    {
        UE_LOG(LogTetherCable, Verbose, TEXT("FTetherAsyncMeshBuildTask: Cancelled"));
        // Don't build or fire complete callback if cancelled beforehand
        return;
    }

    ensure(Params.StaticMesh.IsValid());

    Params.MeshGenerator->SetStaticMeshProperties(Params.StaticMesh.Get());
    
    Params.StaticMesh->CommitMeshDescription(0);

    // Force GIsSilent true to suppress dialogs as the parameter on Build() appears to be broken
    const bool bPreviousGIsSilent = GIsSilent;
    if (Params.bSilent)
    {
        GIsSilent = true;
    }

    UE_LOG(LogTetherCable, Verbose, TEXT("FTetherAsyncMeshBuildTask: Running StaticMesh build"));
    //UStaticMesh::BatchBuild({Params.StaticMesh}, Params.bSilent);
    Params.StaticMesh->Build(Params.bSilent);
    Params.StaticMesh->UpdateUVChannelData(true);
    UE_LOG(LogTetherCable, Verbose, TEXT("FTetherAsyncMeshBuildTask: StaticMesh build complete"));

    if (Params.bSilent)
    {
        GIsSilent = bPreviousGIsSilent;
    }

    Params.Callback.ExecuteIfBound();
}
#endif

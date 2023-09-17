// Copyright Sam Bonifacio 2021. All Rights Reserved.

#pragma once

#if WITH_EDITOR
#include "CableMeshGenerationCurveDescription.h"
#include "Async/AsyncWork.h"
#include "TaskTypes.h"
#include "TetherMeshGenerator.h"

DECLARE_DELEGATE(FOnTetherAsyncMeshBuildCompleteDelegate);

struct TETHER_API FTetherAsyncMeshBuildTaskParams
{
    FOnTetherAsyncMeshBuildCompleteDelegate Callback;

    TWeakObjectPtr<UStaticMesh> StaticMesh = nullptr;
	
    FMeshDescription* MeshDescription = nullptr;

    TWeakObjectPtr<UTetherMeshGenerator> MeshGenerator = nullptr;

    FCableMeshGenerationCurveDescription CurveDescription;

    float CableWidth = 0.f;

    bool bSilent = false;
};

class TETHER_API FTetherAsyncMeshBuildTask : public FAbortableBackgroundTask
{
    friend class FAutoDeleteAsyncTask<FTetherAsyncMeshBuildTask>;

public:
    FTetherAsyncMeshBuildTask(FTetherAsyncMeshBuildTaskParams Params);

    FORCEINLINE TStatId GetStatId() const
    {
        RETURN_QUICK_DECLARE_CYCLE_STAT(FTetherAsyncMeshBuildTask, STATGROUP_ThreadPoolAsyncTasks);
    }

    void DoWork();

private:

    FTetherAsyncMeshBuildTaskParams Params;

    void BuildOnGameThread(bool bCancelled);

};
#endif

// Copyright Sam Bonifacio 2021. All Rights Reserved.

#include "Simulation/TetherCableSimulationOptions.h"
#include "Simulation/TetherSimulation.h"
#include "Misc/EngineVersionComparison.h"

static TAutoConsoleVariable<int32> CVarSelfCollision(
	TEXT("Tether.SelfCollision"),
#if UE_VERSION_OLDER_THAN(5,0,0)
	1,
#else
	0,
#endif
	TEXT(""),
	ECVF_RenderThreadSafe);

bool FTetherCableSimulationOptions::ShouldUseSelfCollision() const
{
	return bEnableCollision && bEnableSelfCollision && CVarSelfCollision.GetValueOnAnyThread() > 0;
}

void FTetherCableSimulationOptions::CheckSelfCollisionOptions() const
{
	if(bEnableCollision && bEnableSelfCollision && CVarSelfCollision.GetValueOnAnyThread() < 1)
	{
		UE_LOG(LogTetherSimulation, Display, TEXT("Cable has self-collision enabled, but CVar Tether.SelfCollision is disabled"));
	}
}

// Copyright Sam Bonifacio 2021. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"

class TETHER_API FTetherPhysicsUtils
{
public:

	static uint32 HashPhyiscsBodies(UWorld* World);

	static void CheckBodyInstances(const TArray<struct FBodyInstance*>& BodyInstances);

};

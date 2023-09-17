// Copyright Sam Bonifacio 2021. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"

class TETHER_API FTetherDebugDrawHelpers
{
public:

	static void DrawPoints(const UWorld* World, TArray<FVector>& WorldPoints, float Size = 10.f, FColor Color = FColor::White, float Lifetime = 0.f);
	static void DrawSpheres(const UWorld* World, TArray<FVector>& WorldPoints, float Radius = 10.f, FColor Color = FColor::White, float Lifetime = 0.f);
	
};

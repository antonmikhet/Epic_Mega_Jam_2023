// Copyright Sam Bonifacio 2021. All Rights Reserved.

#include "TetherDebugDrawHelpers.h"
#include "DrawDebugHelpers.h"

void FTetherDebugDrawHelpers::DrawPoints(const UWorld* World, TArray<FVector>& WorldPoints, float Size, FColor Color, float Lifetime)
{
	for(FVector& Point : WorldPoints)
	{
		::DrawDebugPoint(World, Point, Size, Color, false, Lifetime, SDPG_Foreground);
	}
}

void FTetherDebugDrawHelpers::DrawSpheres(const UWorld* World, TArray<FVector>& WorldPoints, float Radius, FColor Color,
	float Lifetime)
{
	for (FVector& Point : WorldPoints)
	{
		::DrawDebugSphere(World, Point, Radius, 8, Color, false, Lifetime, SDPG_Foreground);
	}
}

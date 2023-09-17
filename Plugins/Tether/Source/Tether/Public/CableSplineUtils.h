// Copyright Sam Bonifacio 2021. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "SplineSegmentInfo.h"

class USplineComponent;

class TETHER_API FCableSplineUtils
{
public:

	static void CreateSplineFromPoints(FInterpCurveVector& Curve, TArray<FVector>& Points);

	static TArray<FVector> CreatePointsFromSpline(FInterpCurveVector& Curve, int32 NumPoints);

	// Returns segment info in world space
	static FSplineSegmentInfo SegmentInfoFromSplineComponent(USplineComponent* SplineComponent, int32 SegmentIndex, FTransform SplineWorldTransform);

	static FInterpCurveVector SplineFromSegmentInfo(const FSplineSegmentInfo& SegmentInfo);

	// From SplineComponent
	static FInterpCurveFloat ReparamTableFromSplineCurve(const FInterpCurveVector& SplineCurve, int32 ReparamStepsPerSegment = 10);
	// From SplineComponent
	static float GetSplineSegmentLength(const FInterpCurveVector& SplineCurve, const int32 Index, const float Param, bool bClosedLoop = false, const FVector& Scale3D = FVector(1.0f));

	// Inverse transforms array of points from world to local space
	static TArray<FVector> WorldPointsToLocal(const TArray<FVector>& WorldPoints, FTransform WorldTransform);

	// Transforms an array of points from local to world space
	static TArray<FVector> LocalPointsToWorld(const TArray<FVector>& LocalPoints, FTransform WorldTransform);

	static TArray<bool> FindContactPoints(const UWorld* World, const TArray<FVector>& WorldPoints, float CheckRadius, const TArray<const AActor*> IgnoreActors);

	// Calculate info for each point to be made available to the mesh generator
	// This information is typically baked into vertex colors and UVs and made available to materials for purposes such as vertex animation
	// Note: This needs the points to be in world-space so that it can correctly trace contact points against the world
	static void CalculatePointInfo(const UWorld* World, const TArray<FVector>& WorldPoints, float CheckRadius, const TArray<const AActor*> IgnoreActors, TArray<struct FCableMeshGenerationPoint>& PointInfos);

	static float CalculateLength(const TArray<FVector>& Points);
	
	
};

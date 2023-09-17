// Copyright Sam Bonifacio 2021. All Rights Reserved.

#include "CurveSimplification.h"


TArray<FVector> FCurveSimplification::SimplifyCurve(const TArray<FVector>& InPoints, float MaxDistance)
{
    if(InPoints.Num() < 3)
    {
        return InPoints;
    }

    // Ramer-Douglas-Peucker implementation

    const int32 NumPoints = InPoints.Num();
    const int32 LastIndex = NumPoints - 1;
    
    // Find the point with the maximum distance
    float GreatestDistanceSquared = 0.f;
    int32 GreatestDistanceIndex = 0;

    for (int i=1; i<LastIndex;i++)
    {
        const float DistanceSquared = FMath::PointDistToSegmentSquared(InPoints[i], InPoints[0], InPoints[LastIndex] );
        if (DistanceSquared > GreatestDistanceSquared)
        {
            GreatestDistanceIndex = i;
            GreatestDistanceSquared = DistanceSquared;
        }
    }
    
    TArray<FVector> ResultList;
    
    // If distance is greater than max distance, recursively simplify
    if (GreatestDistanceSquared > MaxDistance*MaxDistance )
	{
        // Recursive call both sides around index, including the index in both
        TArray<FVector> RecursiveInput1 = InPoints;
        RecursiveInput1.RemoveAt(GreatestDistanceIndex+1, RecursiveInput1.Num()-(GreatestDistanceIndex+1)); // Remove elements after index
        TArray<FVector> RecursiveInput2 = InPoints;
        RecursiveInput2.RemoveAt(0, GreatestDistanceIndex); // Remove elements before index
        
        const TArray<FVector> RecursiveResults1 = SimplifyCurve(RecursiveInput1, MaxDistance);
        const TArray<FVector> RecursiveResults2 = SimplifyCurve(RecursiveInput2, MaxDistance);

        // Build the result list
        TArray<FVector> MergedResult = RecursiveResults1;
        MergedResult.RemoveAt(MergedResult.Num()-1);
        MergedResult.Append(RecursiveResults2);
        return MergedResult;
 
    }
	else
	{
        // Return first and last points
        return {InPoints[0], InPoints[LastIndex]};
    }
}

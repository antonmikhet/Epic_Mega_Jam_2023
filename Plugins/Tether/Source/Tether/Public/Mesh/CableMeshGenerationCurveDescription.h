// Copyright Sam Bonifacio 2021. All Rights Reserved.

#pragma once

#include "BasicMeshGenerationOptions.h"
#include "CableMeshGenerationCurveDescription.generated.h"

/**
 * Data associated with a point along a cable used for mesh generation
 */
USTRUCT()
struct TETHER_API FCableMeshGenerationPointInfo
{
	GENERATED_BODY()

	UPROPERTY()
	float DistanceToNearestContactPoint = 0.f;

	UPROPERTY()
	float Looseness = 0.f;

	UPROPERTY()
	float DistanceToSegmentLine = 0.f;

	UPROPERTY()
	float SlackRatio = 1.f;

	UPROPERTY()
	float SegmentLength = 0.f;

	UPROPERTY()
	float SegmentLineDistance = 0.f;

	bool operator==(const FCableMeshGenerationPointInfo& Other) const
	{
		return
            DistanceToNearestContactPoint == Other.DistanceToNearestContactPoint
            &&
            Looseness == Other.Looseness
            &&
            DistanceToSegmentLine == Other.DistanceToSegmentLine
            &&
            SlackRatio == Other.SlackRatio
            &&
            SegmentLength == Other.SegmentLength
			&&
			SegmentLineDistance == Other.SegmentLineDistance;
	}

	bool operator!=(const FCableMeshGenerationPointInfo& Other) const
	{
		return !(*this == Other);
	}

	static FCableMeshGenerationPointInfo Lerp(const FCableMeshGenerationPointInfo& A, const FCableMeshGenerationPointInfo& B, float Alpha)
	{
		FCableMeshGenerationPointInfo Result;
		Result.DistanceToNearestContactPoint = FMath::Lerp(A.DistanceToNearestContactPoint, B.DistanceToNearestContactPoint, Alpha);
		Result.Looseness = FMath::Lerp(A.Looseness, B.Looseness, Alpha);
		Result.DistanceToSegmentLine = FMath::Lerp(A.DistanceToSegmentLine, B.DistanceToSegmentLine, Alpha);
		Result.SlackRatio = FMath::Lerp(A.SlackRatio, B.SlackRatio, Alpha);
		Result.SegmentLength = FMath::Lerp(A.SegmentLength, B.SegmentLength, Alpha);
		Result.SegmentLineDistance = FMath::Lerp(A.SegmentLineDistance, B.SegmentLineDistance, Alpha);
		return Result;
	}
};

/** 
 * Describes a point along a cable for mesh generation purposes
 */
USTRUCT()
struct TETHER_API FCableMeshGenerationPoint
{
	GENERATED_BODY()

	UPROPERTY()
	FVector Location = FVector::ZeroVector;

	UPROPERTY()
	FQuat Rotation = FQuat::Identity;
	
	UPROPERTY()
	FCableMeshGenerationPointInfo Info;

	FCableMeshGenerationPoint() {}
	
	FCableMeshGenerationPoint(const FVector& InLocation)
		: Location (InLocation)
	{
		
	}

	bool operator==(const FCableMeshGenerationPoint& Other) const
	{
		return
			Location.Equals(Other.Location, 0.1f)
			&&
			Rotation == Other.Rotation
			&&
			Info == Other.Info;
	}

	bool operator!=(const FCableMeshGenerationPoint& Other) const
	{
		return !(*this == Other);
	}
};

/**
 *  All data required to generate a cable mesh
 *  Points and tangents are in local space of the mesh being generated
 */
USTRUCT()
struct TETHER_API FCableMeshGenerationCurveDescription
{
	GENERATED_BODY()
	
	// The points of each loop on the curve
	UPROPERTY()
	TArray<FCableMeshGenerationPoint> Points;

	// Start tangent, if non-zero
	UPROPERTY()
	FVector StartTangent = FVector::ZeroVector;

	// End tangent, if non-zero
	UPROPERTY()
	FVector EndTangent = FVector::ZeroVector;

	FCableMeshGenerationCurveDescription() {};

	FCableMeshGenerationCurveDescription(const TArray<FVector>& PointLocations);

	void CalcPointRotations();

	TArray<FVector> GetPointLocations() const;

	FCableMeshGenerationPointInfo EvalPointInfo(float Key) const;

	bool PointsEqual(const FCableMeshGenerationCurveDescription& Other) const
	{
		if(Points.Num() != Other.Points.Num())
		{
			return false;
		}
		for(int i =0;i<Points.Num();i++)
		{
			if(Points != Other.Points)
			{
				return false;
			}
		}
		return true;
	}

	bool operator==(const FCableMeshGenerationCurveDescription& Other) const
	{
		return
			PointsEqual(Other)
			&&
			StartTangent == Other.StartTangent
			&&
			EndTangent == Other.EndTangent;
	}

	bool operator!=(const FCableMeshGenerationCurveDescription& Other) const
	{
		return !(*this == Other);
	}
};

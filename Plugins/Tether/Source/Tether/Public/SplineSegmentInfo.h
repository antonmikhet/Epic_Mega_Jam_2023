// Copyright Sam Bonifacio 2021. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SplineSegmentInfo.generated.h"

USTRUCT()
struct TETHER_API FSplineSegmentInfo
{
	GENERATED_BODY()

	UPROPERTY()
	FVector StartLocation = FVector::ZeroVector;

	UPROPERTY()
	FVector StartLeaveTangent = FVector::ZeroVector;

	UPROPERTY()
	FVector EndLocation = FVector::ZeroVector;

	// Note: This end tangent maintains the forward vector of the segment, it does not point backward along the segment
	// To get the tangent backward along the segment this should be inverted
	UPROPERTY()
	FVector EndArriveTangent = FVector::ZeroVector;

	bool operator==(const FSplineSegmentInfo& Other) const
	{
		return Equals(Other, KINDA_SMALL_NUMBER, true, true);
	}

	bool operator!=(const FSplineSegmentInfo& Other) const
	{
		return !(*this == Other);
	}

	bool Equals(const FSplineSegmentInfo& Other, float Tolerance = KINDA_SMALL_NUMBER, bool bConsiderStartTangent = true, bool bConsiderEndTangent = true) const
	{
		return StartLocation.Equals(Other.StartLocation, Tolerance)
			&& EndLocation.Equals(Other.EndLocation, Tolerance)
			&& (!bConsiderStartTangent || StartLeaveTangent.Equals(Other.StartLeaveTangent, Tolerance))
			&& (!bConsiderEndTangent || EndArriveTangent.Equals(Other.EndArriveTangent, Tolerance));
	}

	bool ContainsNaN() const
	{
		return StartLocation.ContainsNaN() || StartLeaveTangent.ContainsNaN() || EndLocation.ContainsNaN() || EndArriveTangent.ContainsNaN();
	}

	FString ToString() const
	{
		return FString::Printf(TEXT("StartLocation: %s, StartLeaveTangent: %s, EndLocation: %s, EndArriveTangent: %s"), *StartLocation.ToString(), *StartLeaveTangent.ToString(), *EndLocation.ToString(), *EndArriveTangent.ToString());
	}
};

FORCEINLINE uint32 GetTypeHash(const FSplineSegmentInfo& InSegment)
{
	uint32 Hash = HashCombine(GetTypeHash(InSegment.StartLocation), GetTypeHash(InSegment.StartLeaveTangent));
	Hash = HashCombine(Hash, GetTypeHash(InSegment.EndLocation));
	return HashCombine(Hash, GetTypeHash(InSegment.EndArriveTangent));
}

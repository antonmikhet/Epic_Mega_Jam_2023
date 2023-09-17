// Copyright Sam Bonifacio 2021. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "TetherSegmentSimulationOptions.generated.h"

USTRUCT(BlueprintType)
struct TETHER_API FTetherSegmentSimulationOptions
{
	GENERATED_BODY()

public:

	/**
	 * If true, the point of the cable will be fixed to the spline point. If false it will fall freely.
	 */
	UPROPERTY(Category = "TetherProperties", EditAnywhere, BlueprintReadWrite)
	bool bFixedAnchorPoint = true;

	/**
	 * Force the cable to respect the Guide Spline's tangents when using stiffness constraints, if Fixed Anchor Point is true
	 */
	UPROPERTY(Category = "TetherProperties", EditAnywhere, BlueprintReadWrite, meta = (EditCondition=bFixedAnchorPoint))
	bool bUseSplineTangents = false;

	bool ShouldUseSplineTangents() const { return bUseSplineTangents && bFixedAnchorPoint; }

};

FORCEINLINE uint32 GetTypeHash(const FTetherSegmentSimulationOptions& InOptions)
{
	uint32 Hash = GetTypeHash(InOptions.bUseSplineTangents);
	Hash = HashCombine(Hash, GetTypeHash(InOptions.bFixedAnchorPoint));
	return Hash;
}

// Copyright Sam Bonifacio 2021. All Rights Reserved.

#pragma once

#include "BasicMeshGenerationOptions.h"
#include "CoreMinimal.h"

#include "BasicMeshProperties.generated.h"

/**
 * Properties used for basic mesh generation
 */
USTRUCT(BlueprintType)
struct TETHER_API FBasicMeshProperties
{
	GENERATED_BODY()
	
	/**
	 *  Multiplier for the number of mesh loops to be created along the cable
	 *  Higher values create more accurate curves
	 *  Distance between loops = Cable Width / Loop Resolution
	 */
	UPROPERTY(Category = "TetherProperties", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.01", UIMax = "100"))
	float LoopResolution = 2.f;

	/**
	 *  If greater than zero, the loops along the cable mesh will be optimized away if the cable is straight
	 *  Increasing this will cull more loops, optimizing more but reducing the quality of the mesh
	 *  Max distance = Cable Width * Curve Simplification Multiplier
	 *  If a loop is within the max distance to the line between the loops on either side, it will be culled
	 */
	UPROPERTY(Category = "TetherProperties", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", UIMax = "5"))
	float CurveSimplificationMultiplier = 0.02f;

	UPROPERTY(Category = "TetherProperties", EditAnywhere, BlueprintReadWrite)
	FBasicMeshGenerationOptions MeshGenerationOptions;

	bool operator==(const FBasicMeshProperties& Other) const;

	bool operator!=(const FBasicMeshProperties& Other) const
	{
		return !(*this==Other);
	}
};

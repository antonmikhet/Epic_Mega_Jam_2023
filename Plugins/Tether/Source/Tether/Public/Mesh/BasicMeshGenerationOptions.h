// Copyright Sam Bonifacio 2021. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "BasicMeshGenerationOptions.generated.h"

/**
 *  Set of mesh generation options that define how to turn a set of points representing a curve into a cable mesh
 *  Does not contain the curve points
 */
USTRUCT(BlueprintType)
struct TETHER_API FBasicMeshGenerationOptions
{
	GENERATED_BODY()

public:
	/* Number of faces around the cable to generate */
	UPROPERTY(Category="TetherProperties", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "2", UIMax = "32"))
	int32 NumSides = 16;

	/**
	 * If true, use the Cable Mesh Width below. If false, use the Cable Width.
	 */
	UPROPERTY(Category = "TetherProperties", EditAnywhere, BlueprintReadWrite)
	bool bOverrideCableWidth = false;
	
	/* Diameter of the cable */
	UPROPERTY(Category = "TetherProperties", EditAnywhere, BlueprintReadWrite, meta = (EditCondition=bOverrideCableWidth, ClampMin = "0.01", UIMin = "0.01", UIMax = "50.0"))
	float CableMeshWidth = 10.f;

	/**
	 * If true, auto tile the UVs along the length of the cable.
	 */
	UPROPERTY(Category = "TetherProperties", EditAnywhere, BlueprintReadWrite)
	bool bAutoTile = true;

	UPROPERTY(Category = "TetherProperties", EditAnywhere, BlueprintReadWrite, meta = (EditCondition=bAutoTile))
	bool bSnapToNearestFullTile = true;
	
	/**
	 * Tile material UVs along the length of the cable
	 * Alternatively can be done in the material
	 */
	UPROPERTY(Category = "TetherProperties", EditAnywhere, BlueprintReadWrite, meta = (EditCondition="!bAutoTile", UIMin = "0.1", UIMax = "8"))
	float TileUVs = 1.f;

	bool operator==(const FBasicMeshGenerationOptions& Other) const;

	bool operator!=(const FBasicMeshGenerationOptions& Other) const
	{
		return !(*this == Other);
	}
};

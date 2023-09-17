// Copyright Sam Bonifacio 2021. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Mesh/TetherMeshGenerator.h"
#include "TMG_CustomMesh.generated.h"

USTRUCT(BlueprintType)
struct TETHER_API FCustomMeshProperties
{
	GENERATED_BODY()
	
	/**
	 *  Source Static Mesh asset to repeat along the cable
	 *  Needs to be x-axis aligned
	 */
	UPROPERTY(Category = "TetherProperties", EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<class UStaticMesh> SourceMeshReference;

	/**
	 *  Rotation (in degrees) of the mesh around the center of the cable
	 */
	UPROPERTY(Category = "TetherProperties", EditAnywhere, BlueprintReadWrite)
	float OffsetRotation = 0.f;

	/**
	 *  Number of times to repeat the Source Mesh along the length of the cable
	 *  0 = Calculate automatically to best fit with minimum stretching based on Source Mesh dimensions
	 */
	UPROPERTY(Category = "TetherProperties", EditAnywhere, BlueprintReadWrite)
	int32 NumInstances = 0;

	/**
	 *  If true, width of mesh will be fitted to the Cable Width
	 */
	UPROPERTY(Category = "TetherProperties", EditAnywhere, BlueprintReadWrite)
	bool bFitToCableWidth = true;

};


/**
 * 
 */
UCLASS(meta = (DisplayName = "Custom Cable Mesh"))
class TETHER_API UTMG_CustomMesh : public UTetherMeshGenerator
{
	GENERATED_BODY()

public:
	
	UPROPERTY(Category = "TetherProperties", EditAnywhere, Config)
	FCustomMeshProperties Properties;

	UTMG_CustomMesh();

#if WITH_EDITOR
	virtual void PrepareResources() const override;

	virtual bool BuildDynamicMeshGeometry(const FCableMeshGenerationCurveDescription& CurveDescription, float CableWidth, ::TArray<FDynamicMeshVertex>& OutVertices, ::TArray<int>& OutIndices, TArray<int32>* OutPolyGroups, FProgressCancel* Progress) const override;

	virtual bool BuildMeshDescription(const FCableMeshGenerationCurveDescription& CurveDescription, float CableWidth, FMeshDescription* MeshDescription, FProgressCancel* Progress) const override;

	void BuildCustomMeshInstances(const FCableMeshGenerationCurveDescription& CurveDescription, float WidthScaleFactor,
    int32 NumInstancesToBuild, ::TArray<FDynamicMeshVertex>& OutVertices, ::TArray<int>& OutIndices, TArray<int32>* OutPolyGroups, FProgressCancel* Progress) const;

	void BuildCustomMeshInstancesMeshDescription(const FCableMeshGenerationCurveDescription& CurveDescription, float WidthScaleFactor,
int32 NumInstancesToBuild, FMeshDescription* TargetMeshDescription, FProgressCancel* Progress) const;

	virtual int32 GetNumMaterials(struct FTetherCableProperties& CableProperties) const override;
	virtual UMaterialInterface* GetDefaultMaterial(FTetherCableProperties& CableProperties, int32 MaterialSlotIndex) const override;
	virtual UMaterialInterface* GetMaterial(FTetherCableProperties& CableProperties, int32 MaterialSlotIndex) const override;
	virtual void SetStaticMeshProperties(UStaticMesh* StaticMesh) const override;
#endif
	
};

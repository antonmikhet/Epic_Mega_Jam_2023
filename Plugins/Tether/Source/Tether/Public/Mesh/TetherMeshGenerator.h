// Copyright Sam Bonifacio 2021. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "UObject/NoExportTypes.h"
#include "TetherMeshGenerator.generated.h"

class UMaterialInterface;
class FProgressCancel;
struct FCableMeshGenerationCurveDescription;

DECLARE_MULTICAST_DELEGATE_OneParam(FMeshGeneratorModified, FPropertyChangedEvent&);

/**
 * 
 */
UCLASS(Abstract, DefaultToInstanced, EditInlineNew, config = TetherModeSettings)
class TETHER_API UTetherMeshGenerator : public UObject
{
	GENERATED_BODY()

public:

#if WITH_EDITOR
	FMeshGeneratorModified OnModified;

	virtual void PrepareResources() const;

	virtual void OptimizeCurvePoints(TArray<FVector>& CurvePoints, float CableWidth) const;
	
	virtual bool BuildDynamicMeshGeometry(const FCableMeshGenerationCurveDescription& CurveDescription, float CableWidth, TArray<struct FDynamicMeshVertex>& OutVertices, TArray<int32>& OutIndices, TArray<int32>* OutPolyGroups, FProgressCancel* Progress = nullptr) const;

	virtual bool BuildMeshDescription(const FCableMeshGenerationCurveDescription& CurveDescription, float CableWidth, struct FMeshDescription* MeshDescription, FProgressCancel* Progress = nullptr) const;

	virtual void SetStaticMeshProperties(class UStaticMesh* StaticMesh) const;

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	virtual int32 GetNumMaterials(struct FTetherCableProperties& CableProperties) const;
	virtual UMaterialInterface* GetDefaultMaterial(struct FTetherCableProperties& CableProperties, int32 MaterialSlotIndex) const;
	virtual UMaterialInterface* GetMaterial(struct FTetherCableProperties& CableProperties, int32 MaterialSlotIndex) const;

	bool IsSlowBuild() const { return bSlowBuild; }

protected:

	bool bSlowBuild = false;
#endif
	
};

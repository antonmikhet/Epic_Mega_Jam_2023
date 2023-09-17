// Copyright Sam Bonifacio 2021. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "BasicMeshProperties.h"
#include "Mesh/TetherMeshGenerator.h"
#include "TMG_Basic.generated.h"

/**
 * 
 */
UCLASS(meta = (DisplayName = "Basic Cable Mesh"))
class TETHER_API UTMG_Basic : public UTetherMeshGenerator
{
	GENERATED_BODY()

public:

	UPROPERTY(Category = "TetherProperties", EditAnywhere, Config)
	FBasicMeshProperties Properties;

#if WITH_EDITOR
	virtual void OptimizeCurvePoints(TArray<FVector>& CurvePoints, float CableWidth) const override;

	virtual bool BuildDynamicMeshGeometry(const FCableMeshGenerationCurveDescription& CurveDescription, float CableWidth, ::TArray<FDynamicMeshVertex>& OutVertices, ::TArray<int>& OutIndices, TArray<int32>* OutPolyGroups, FProgressCancel* Progress) const override;
#endif
};

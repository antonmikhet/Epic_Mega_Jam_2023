// Copyright Sam Bonifacio 2021. All Rights Reserved.

#pragma once

#include "CableMeshGenerationCurveDescription.h"
#include "CoreMinimal.h"
#include "Components/MeshComponent.h"
#include "Misc/EngineVersionComparison.h"
#include "TetherCableMeshComponent.generated.h"

/**
 * Dynamic mesh component used for previewing the cable before it is committed to a static mesh
 * This is heavily based off of UCableComponent from the CableComponent plugin
 */
UCLASS()
class TETHER_API UTetherCableMeshComponent : public UMeshComponent
{
	GENERATED_BODY()

public:

	UTetherCableMeshComponent();

	// Set parameters and update geometry
	void SetMeshGenerationParams(const FCableMeshGenerationCurveDescription& InMeshGenerationParamsLocalSpace, const FBasicMeshGenerationOptions& InOptions);

	TArray<FVector> GetCablePointsWorldSpace() const;
	TArray<FVector> GetCablePointsLocalSpace() const;

protected:

	virtual void SendRenderDynamicData_Concurrent() override;

#if UE_VERSION_OLDER_THAN(4,25,0)
	virtual void CreateRenderState_Concurrent() override;
#else
	virtual void CreateRenderState_Concurrent(FRegisterComponentContext* Context) override;
#endif

	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;

	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;

	virtual int32 GetNumMaterials() const override { return 1; }

private:

	FCableMeshGenerationCurveDescription MeshGenerationParamsLocalSpace;
	FBasicMeshGenerationOptions BasicMeshGenerationOptions;

	friend class FTetherCableSceneProxy;
};

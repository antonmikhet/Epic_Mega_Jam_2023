// Copyright Sam Bonifacio 2021. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mesh/BasicMeshProperties.h"
#include "Mesh/CableMeshGenerationType.h"
#include "Mesh/TetherMeshGenerator.h"
#include "Mesh/TMG_CustomMesh.h"
#include "Simulation/TetherCableSimulationOptions.h"
#include "TetherCableProperties.generated.h"

USTRUCT(BlueprintType)
struct TETHER_API FTetherCableProperties
{
	GENERATED_BODY()

	/**
	 * The diameter of the cable, which by default controls the width of the cable for simulating collision, the width of the generated mesh, and also affects the density of particles (or *Particle Segment Length*) along the cable during simulation.
	 */
	UPROPERTY(Category = "TetherProperties", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.1", UIMin = "1.0", UIMax = "50.0"))
	float CableWidth = 10.f;

	UPROPERTY(Category = "TetherProperties", EditAnywhere, BlueprintReadWrite)
	FTetherCableSimulationOptions SimulationOptions;

	/**
	 * Mesh generation type to use for this cable
	 */
	UPROPERTY(Category = "TetherProperties", EditAnywhere, BlueprintReadWrite)
	ECableMeshGenerationType MeshType = ECableMeshGenerationType::Basic;

	UPROPERTY(Category = "TetherProperties", EditAnywhere, BlueprintReadWrite, meta = (EditCondition =  "MeshType == ECableMeshGenerationType::Basic", DisplayName = "Basic Mesh"))
	FBasicMeshProperties BasicMeshProperties;

	UPROPERTY(Category = "TetherProperties", EditAnywhere, BlueprintReadWrite, meta = (EditCondition =  "MeshType == ECableMeshGenerationType::CustomMesh", DisplayName = "Custom Mesh"))
	FCustomMeshProperties CustomMeshProperties;
	
	//UPROPERTY(Category = "TetherProperties", EditAnywhere, BlueprintReadWrite, Instanced, NoClear)
	//UTetherMeshGenerator* MeshGenerator;

	UPROPERTY(Category = "TetherProperties", EditAnywhere, BlueprintReadWrite, meta = (NoResimulate))
	TArray<class UMaterialInterface*> Materials;

	FTetherCableProperties();

	float GetDesiredParticleDistance() const;
	float GetCollisionWidth() const;
};

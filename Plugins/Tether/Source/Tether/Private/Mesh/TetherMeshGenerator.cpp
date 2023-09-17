// Copyright Sam Bonifacio 2021. All Rights Reserved.

#include "Mesh/TetherMeshGenerator.h"

#include "TetherCableProperties.h"
#include "Engine/StaticMesh.h"
#include "Mesh/CableMeshGeneration.h"
#include "Mesh/TetherMeshUtils.h"
#include "Misc/EngineVersionComparison.h"

#if WITH_EDITOR
void UTetherMeshGenerator::PrepareResources() const
{
}

void UTetherMeshGenerator::OptimizeCurvePoints(TArray<FVector>& CurvePoints, float CableWidth) const
{
}

bool UTetherMeshGenerator::BuildDynamicMeshGeometry(const FCableMeshGenerationCurveDescription& CurveDescription, float CableWidth, TArray<FDynamicMeshVertex>& OutVertices, TArray<int32>& OutIndices, TArray<int32>* OutPolyGroups, FProgressCancel* Progress) const
{
	return false;
}

bool UTetherMeshGenerator::BuildMeshDescription(const FCableMeshGenerationCurveDescription& CurveDescription, float CableWidth, FMeshDescription* MeshDescription, FProgressCancel* Progress) const
{
	// Default implementation is to try build dynamic mesh and convert it if possible
	// This can be overridden to directly build the mesh description instead
	if(IsInGameThread())
	{
		checkNoRecursion();
	}
	TArray<FDynamicMeshVertex> Verts;
	TArray<int32> Indices;
	TArray<int32> PolyGroups;
	const bool bBuildDynamicSuccess = BuildDynamicMeshGeometry(CurveDescription, CableWidth, Verts, Indices, &PolyGroups, Progress);
	if(bBuildDynamicSuccess)
	{
		FCableMeshGeneration::ConvertToMeshDescription(Verts, Indices, &PolyGroups, MeshDescription);
		return true;
	}
	return false;
}

void UTetherMeshGenerator::SetStaticMeshProperties(UStaticMesh* StaticMesh) const
{
	// By default just set a single material

	TArray<FStaticMaterial>& Materials = FTetherMeshUtils::GetStaticMaterials(StaticMesh);
	
	Materials.Reset();
	Materials.Add(FStaticMaterial(nullptr, "CableMaterial", "CableMaterial"));
}

void UTetherMeshGenerator::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	OnModified.Broadcast(PropertyChangedEvent);
}

int32 UTetherMeshGenerator::GetNumMaterials(struct FTetherCableProperties& CableProperties) const
{
	return 1;
}

UMaterialInterface* UTetherMeshGenerator::GetDefaultMaterial(FTetherCableProperties& CableProperties, int32 MaterialSlotIndex) const
{
	return nullptr;
}

UMaterialInterface* UTetherMeshGenerator::GetMaterial(FTetherCableProperties& CableProperties, int32 MaterialSlotIndex) const
{
	if(CableProperties.Materials.IsValidIndex(MaterialSlotIndex) && CableProperties.Materials[MaterialSlotIndex] != nullptr)
	{
		return CableProperties.Materials[MaterialSlotIndex];
	}
	return nullptr;
}

#endif

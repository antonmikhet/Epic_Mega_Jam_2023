// Copyright Sam Bonifacio 2021. All Rights Reserved.

#include "Mesh/TetherMeshUtils.h"
#include "Misc/EngineVersionComparison.h"
#include "Engine/StaticMesh.h"

TArray<FStaticMaterial>& FTetherMeshUtils::GetStaticMaterials(UStaticMesh* StaticMesh)
{
#if UE_VERSION_OLDER_THAN(4, 27, 0)
	return StaticMesh->StaticMaterials;
#else
	return StaticMesh->GetStaticMaterials();
#endif
}

// Copyright Sam Bonifacio 2021. All Rights Reserved.

#include "Mesh/BasicMeshGenerationOptions.h"

bool FBasicMeshGenerationOptions::operator==(const FBasicMeshGenerationOptions& Other) const
{
	return
		NumSides == Other.NumSides
		&& bOverrideCableWidth == Other.bOverrideCableWidth
		&& CableMeshWidth == Other.CableMeshWidth
		&& TileUVs == Other.TileUVs;
}

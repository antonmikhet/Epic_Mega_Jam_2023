// Copyright Sam Bonifacio 2021. All Rights Reserved.

#include "Mesh/BasicMeshProperties.h"

bool FBasicMeshProperties::operator==(const FBasicMeshProperties& Other) const
{
	return
		LoopResolution == Other.LoopResolution
		&& CurveSimplificationMultiplier == Other.CurveSimplificationMultiplier
		&& MeshGenerationOptions == Other.MeshGenerationOptions;
}

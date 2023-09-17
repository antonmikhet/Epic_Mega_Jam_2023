// Copyright Sam Bonifacio 2021. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FStaticMaterial;
class UStaticMesh;

class FTetherMeshUtils
{
public:

	static TArray<FStaticMaterial>& GetStaticMaterials(UStaticMesh* StaticMesh);
	
};

// Copyright Sam Bonifacio 2021. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "UObject/ObjectMacros.h"

class UTetherMeshGenerator;

UENUM(BlueprintType)
enum class ECableMeshGenerationType : uint8
{
	Basic,
	CustomMesh
};

class TETHER_API FCableMeshGenerationType
{
public:
	/**
	 * Get the Mesh Generator subclass for the given type enum
	 */
	static TSubclassOf<UTetherMeshGenerator> GetMeshGeneratorClass(ECableMeshGenerationType Type);
};

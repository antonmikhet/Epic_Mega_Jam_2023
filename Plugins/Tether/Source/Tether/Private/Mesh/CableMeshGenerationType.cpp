// Copyright Sam Bonifacio 2021. All Rights Reserved.


#include "Mesh/CableMeshGenerationType.h"

#include "Mesh/TMG_Basic.h"
#include "Mesh/TMG_CustomMesh.h"

TSubclassOf<UTetherMeshGenerator> FCableMeshGenerationType::GetMeshGeneratorClass(ECableMeshGenerationType Type)
{
	switch(Type)
	{
		case ECableMeshGenerationType::Basic:
			return UTMG_Basic::StaticClass();
		case ECableMeshGenerationType::CustomMesh:
			return UTMG_CustomMesh::StaticClass();
		default:
			return nullptr;
	}
}

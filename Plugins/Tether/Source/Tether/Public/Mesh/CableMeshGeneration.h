// Copyright Sam Bonifacio 2021. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"

#include "BasicMeshGenerationOptions.h"
#include "MeshDescription.h"
#include "Misc/EngineVersionComparison.h"

struct FCableMeshGenerationPointInfo;
struct FCableMeshGenerationCurveDescription;
struct FDynamicMeshVertex;

#if UE_VERSION_OLDER_THAN(5,0,0)
typedef FVector2D FVector2f;
typedef FVector FVector3f;
typedef FVector4 FVector4f;
#endif

/**
 * Static utility class for mesh generation helpers
 */
class TETHER_API FCableMeshGeneration
{
public:

	static void BuildBasicMeshDescription(FMeshDescription* MeshDescription, const FCableMeshGenerationCurveDescription& Params, const FBasicMeshGenerationOptions& Options);

	static void BuildDynamicBasicMesh(const FCableMeshGenerationCurveDescription& CurveDescription, const FBasicMeshGenerationOptions& Options, TArray<FDynamicMeshVertex>& OutVertices, TArray<int32>& OutIndices, bool bCalculateLength = false);

	static void ConvertToMeshDescription(const TArray<FDynamicMeshVertex>& DynamicVerts, const TArray<int32>& DynamicIndices, const TArray<int32>* PolyGroups, FMeshDescription* MeshDescription);

	static void ApplyPointInfo(FDynamicMeshVertex& Vert, const FCableMeshGenerationPointInfo& PointInfo, float AlongFrac );

	static void AddDynamicVertToMeshDescription(const FDynamicMeshVertex& Vert, FMeshDescription* MeshDescription, TVertexAttributesRef<FVector3f>& VertexPositions,
TVertexInstanceAttributesRef<FVector3f>& Tangents, TVertexInstanceAttributesRef<FVector3f>& Normals, TVertexInstanceAttributesRef<float>& BinormalSigns, TVertexInstanceAttributesRef<FVector4f>& Colors,
TVertexInstanceAttributesRef<FVector2f>& UVs);
	
};

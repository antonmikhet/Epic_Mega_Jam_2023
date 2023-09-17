// Copyright Sam Bonifacio 2021. All Rights Reserved.


#include "Mesh/TMG_Basic.h"
#include "CableSplineUtils.h"
#include "CurveSimplification.h"
#include "Mesh/CableMeshGeneration.h"
#include "Mesh/CableMeshGenerationCurveDescription.h"

#if WITH_EDITOR
void UTMG_Basic::OptimizeCurvePoints(TArray<FVector>& PointLocations, float CableWidth) const
{
	FInterpCurveVector Curve;
	FCableSplineUtils::CreateSplineFromPoints(Curve, PointLocations);

	const float Length = FCableSplineUtils::CalculateLength(PointLocations);
	const float MeshSegmentLength = CableWidth / Properties.LoopResolution;
	const int32 MeshPointsNum = (Length / MeshSegmentLength) + 1;

	PointLocations = FCableSplineUtils::CreatePointsFromSpline(Curve, MeshPointsNum);

	// Run curve simplification
	if(Properties.CurveSimplificationMultiplier > 0.f)
	{
		const float MaxDistance = CableWidth * Properties.CurveSimplificationMultiplier;
		PointLocations = FCurveSimplification::SimplifyCurve(PointLocations, MaxDistance);
	}
}

bool UTMG_Basic::BuildDynamicMeshGeometry(const FCableMeshGenerationCurveDescription& CurveDescription, float CableWidth, ::TArray<FDynamicMeshVertex>& OutVertices, ::TArray<int>& OutIndices, TArray<int32>* OutPolyGroups, FProgressCancel* Progress) const
{
	FBasicMeshGenerationOptions Options = Properties.MeshGenerationOptions;
	
	if(!Options.bOverrideCableWidth)
	{
		Options.CableMeshWidth = CableWidth;
	}
	
	FCableMeshGeneration::BuildDynamicBasicMesh(CurveDescription, Options, OutVertices, OutIndices, true);

	return true;
}
#endif

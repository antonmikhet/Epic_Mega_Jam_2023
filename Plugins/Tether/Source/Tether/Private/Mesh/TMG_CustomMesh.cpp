// Copyright Sam Bonifacio 2021. All Rights Reserved.


#include "Mesh/TMG_CustomMesh.h"

#include "CableSplineUtils.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "TetherCableProperties.h"
#include "Engine/StaticMesh.h"
#include "Mesh/CableMeshGeneration.h"
#include "Mesh/CableMeshGenerationCurveDescription.h"
#include "Mesh/TetherMeshUtils.h"

UTMG_CustomMesh::UTMG_CustomMesh()
{
#if WITH_EDITOR
	bSlowBuild = true;
#endif
}

#if WITH_EDITOR
void UTMG_CustomMesh::PrepareResources() const
{
	ensure(IsInGameThread());
	if (IsValid(Properties.SourceMeshReference.LoadSynchronous()))
	{
		// Load the mesh description on GT so that it's available on worker threads from here on
		Properties.SourceMeshReference.Get()->GetMeshDescription(0);
	}
}

bool UTMG_CustomMesh::BuildDynamicMeshGeometry(const FCableMeshGenerationCurveDescription& CurveDescription, float CableWidth, ::TArray<FDynamicMeshVertex>& OutVertices, ::TArray<int>& OutIndices, TArray<int32>* OutPolyGroups, FProgressCancel* Progress) const
{
	if (!IsValid(Properties.SourceMeshReference.Get()))
	{
		return false;
	}

	const FBox Bounds = Properties.SourceMeshReference->GetBounds().GetBox();

	const float BoundsWidth = FMath::Max(Bounds.GetExtent().Y, Bounds.GetExtent().Z) * 2.f;

	const float WidthScaleFactor = Properties.bFitToCableWidth ? CableWidth / BoundsWidth : 1.0f;

	int32 NumInstancesToBuild = Properties.NumInstances;
	if(NumInstancesToBuild == 0)
	{
		// Auto calc best number of instances
		const float SourceLength = Bounds.GetExtent().X * 2.f * WidthScaleFactor;
		const TArray<FVector> PointLocations = CurveDescription.GetPointLocations();
		const float TotalLength = FCableSplineUtils::CalculateLength(PointLocations);
		NumInstancesToBuild = FMath::RoundToInt(TotalLength / SourceLength);
		if(NumInstancesToBuild < 1)
		{
			NumInstancesToBuild = 1;
		}
	}

	BuildCustomMeshInstances(CurveDescription, WidthScaleFactor, NumInstancesToBuild, OutVertices, OutIndices, OutPolyGroups, Progress);
	return true;
}

bool UTMG_CustomMesh::BuildMeshDescription(const FCableMeshGenerationCurveDescription& CurveDescription, float CableWidth, FMeshDescription* MeshDescription, FProgressCancel* Progress) const
{
	if (!IsValid(Properties.SourceMeshReference.Get()))
	{
		return false;
	}

	const FBox Bounds = Properties.SourceMeshReference->GetBounds().GetBox();

	const float BoundsWidth = FMath::Max(Bounds.GetExtent().Y, Bounds.GetExtent().Z) * 2.f;

	const float WidthScaleFactor = Properties.bFitToCableWidth ? CableWidth / BoundsWidth : 1.0f;

	int32 NumInstancesToBuild = Properties.NumInstances;
	if(NumInstancesToBuild == 0)
	{
		// Auto calc best number of instances
		const float SourceLength = Bounds.GetExtent().X * 2.f * WidthScaleFactor;
		const TArray<FVector> PointLocations = CurveDescription.GetPointLocations();
		const float TotalLength = FCableSplineUtils::CalculateLength(PointLocations);
		NumInstancesToBuild = FMath::RoundToInt(TotalLength / SourceLength);
		if(NumInstancesToBuild < 1)
		{
			NumInstancesToBuild = 1;
		}
	}

	BuildCustomMeshInstancesMeshDescription(CurveDescription, WidthScaleFactor, NumInstancesToBuild, MeshDescription, Progress);
	return true;
}

void UTMG_CustomMesh::BuildCustomMeshInstances(const FCableMeshGenerationCurveDescription& CurveDescription, float WidthScaleFactor,
                                               int32 NumInstancesToBuild, ::TArray<FDynamicMeshVertex>& OutVertices, ::TArray<int>& OutIndices, TArray<int32>* OutPolyGroups, FProgressCancel* Progress) const
{
	FBox Bounds = Properties.SourceMeshReference->GetBounds().GetBox();
	
	FInterpCurveVector SplineCurve;
	FInterpCurveQuat RotationCurve;
	TArray<FVector> PointLocations = CurveDescription.GetPointLocations();
	FCableSplineUtils::CreateSplineFromPoints(SplineCurve, PointLocations);
	for(int32 PointIdx = 0; PointIdx < CurveDescription.Points.Num(); PointIdx++)
	{
		RotationCurve.AddPoint(PointIdx, CurveDescription.Points[PointIdx].Rotation);
	}
	if(!CurveDescription.StartTangent.IsZero())
	{
		const FVector Tangent = CurveDescription.StartTangent.GetSafeNormal() * FVector::Dist(PointLocations[0], PointLocations[1]);
		SplineCurve.Points[0].LeaveTangent = Tangent;
		//RotationCurve.Points[0].LeaveTangent = Tangent;
	}
	if(!CurveDescription.EndTangent.IsZero())
	{
		const FVector Tangent = CurveDescription.EndTangent.GetSafeNormal() * FVector::Dist(PointLocations.Last(), PointLocations.Last(1));
		SplineCurve.Points.Last().ArriveTangent = Tangent;
		//RotationCurve.Points.Last().ArriveTangent = Tangent;
	}

	FMeshDescription* MeshDescription = Properties.SourceMeshReference->GetMeshDescription(0);

	FStaticMeshAttributes AttributeGetter(*MeshDescription);

	TVertexAttributesRef<FVector3f> VertexPositions = AttributeGetter.GetVertexPositions();
	TVertexInstanceAttributesRef<FVector3f> Tangents = AttributeGetter.GetVertexInstanceTangents();
	//TVertexInstanceAttributesRef<float> BinormalSigns = AttributeGetter.GetVertexInstanceBinormalSigns();
	TVertexInstanceAttributesRef<FVector3f> Normals = AttributeGetter.GetVertexInstanceNormals();
	//TVertexInstanceAttributesRef<FVector4> Colors = AttributeGetter.GetVertexInstanceColors();
	TVertexInstanceAttributesRef<FVector2f> UVs = AttributeGetter.GetVertexInstanceUVs();
	TPolygonGroupAttributesRef<FName> PolyGroupMaterialSlotNames = AttributeGetter.GetPolygonGroupMaterialSlotNames();

	const FQuat OffsetRotationQuat = FQuat::MakeFromEuler(FVector(Properties.OffsetRotation, 0.f, 0.f));

	for(int32 InstanceIdx = 0; InstanceIdx < NumInstancesToBuild; InstanceIdx++)
	{
		for (int32 i = 0; i < MeshDescription->VertexInstances().Num(); i++)
		{
			FVertexInstanceID VertInstance = FVertexInstanceID(i); /*MeshDescription->VertexInstances();*/
			FVertexID Vert = MeshDescription->GetVertexInstanceVertex(VertInstance);

			FVector3f SourceVertPosition = VertexPositions[Vert];
			const float SourceVertAlongFrac = FMath::GetMappedRangeValueUnclamped(FVector2D(Bounds.Min.X, Bounds.Max.X), FVector2D(0.f, 1.f), SourceVertPosition.X);
			const float CableAlongFrac = (SourceVertAlongFrac + InstanceIdx) / NumInstancesToBuild;
			float SplineKey = SplineCurve.Points.Last().InVal * CableAlongFrac;

			const FCableMeshGenerationPointInfo PointInfo = CurveDescription.EvalPointInfo(SplineKey);
			
			FVector SplinePos = SplineCurve.Eval(SplineKey);
			FVector SplineTangent = SplineCurve.EvalDerivative(SplineKey);
			FVector SourceOffset = FVector(0.f, SourceVertPosition.Y, SourceVertPosition.Z);

			FQuat Rot = RotationCurve.Eval(SplineKey) * OffsetRotationQuat;
			FVector TargetOffset = Rot.RotateVector(SourceOffset) * WidthScaleFactor;

			FVector3f TargetPosition = FVector3f(SplinePos + TargetOffset);

			FDynamicMeshVertex DynamicVert(TargetPosition);
			DynamicVert.SetTangents(FVector3f( Rot.RotateVector(FVector(Tangents[VertInstance]))), FVector3f::ZeroVector, FVector3f( Rot.RotateVector(FVector(Normals[VertInstance]))));
			DynamicVert.TextureCoordinate[0] = UVs[VertInstance];

			FCableMeshGeneration::ApplyPointInfo(DynamicVert, PointInfo, CableAlongFrac);
			
			OutVertices.Add(DynamicVert);
		}

		const int32 InstanceOffset = OutIndices.Num();

		for (int32 i = 0; i < MeshDescription->Polygons().Num(); i++)
		{
			const FPolygonID PolygonID(i);
			TArray<FVertexInstanceID> PolyVerts = MeshDescription->GetPolygonVertexInstances(PolygonID);
			for (int32 PolyVertIdx = 0; PolyVertIdx < PolyVerts.Num(); PolyVertIdx++)
			{
				OutIndices.Add(InstanceOffset + PolyVerts[PolyVertIdx].GetValue());
			}
			if(OutPolyGroups)
			{
				const FPolygonGroupID PolyGroup = MeshDescription->GetPolygonPolygonGroup(PolygonID);
				FName Name = PolyGroupMaterialSlotNames[PolyGroup];
				OutPolyGroups->Add(PolyGroup.GetValue());
			}
		}
	}

}

void UTMG_CustomMesh::BuildCustomMeshInstancesMeshDescription(const FCableMeshGenerationCurveDescription& CurveDescription, float WidthScaleFactor, int32 NumInstancesToBuild, FMeshDescription* TargetMeshDescription, FProgressCancel* Progress) const
{
	FBox Bounds = Properties.SourceMeshReference->GetBounds().GetBox();
	
	FInterpCurveVector SplineCurve;
	FInterpCurveQuat RotationCurve;
	TArray<FVector> PointLocations = CurveDescription.GetPointLocations();
	FCableSplineUtils::CreateSplineFromPoints(SplineCurve, PointLocations);
	for(int32 PointIdx = 0; PointIdx < CurveDescription.Points.Num(); PointIdx++)
	{
		RotationCurve.AddPoint(PointIdx, CurveDescription.Points[PointIdx].Rotation);
	}
	if(!CurveDescription.StartTangent.IsZero())
	{
		const FVector Tangent = CurveDescription.StartTangent.GetSafeNormal() * FVector::Dist(PointLocations[0], PointLocations[1]);
		SplineCurve.Points[0].LeaveTangent = Tangent;
	}
	if(!CurveDescription.EndTangent.IsZero())
	{
		const FVector Tangent = CurveDescription.EndTangent.GetSafeNormal() * FVector::Dist(PointLocations.Last(), PointLocations.Last(1));
		SplineCurve.Points.Last().ArriveTangent = Tangent;
	}

	FMeshDescription* SourceMeshDescription = Properties.SourceMeshReference->GetMeshDescription(0);

	FStaticMeshAttributes SourceAttributeGetter(*SourceMeshDescription);

	TVertexAttributesRef<FVector3f> SourceVertexPositions = SourceAttributeGetter.GetVertexPositions();
	TVertexInstanceAttributesRef<FVector3f> SourceTangents = SourceAttributeGetter.GetVertexInstanceTangents();
	//TVertexInstanceAttributesRef<float> BinormalSigns = AttributeGetter.GetVertexInstanceBinormalSigns();
	TVertexInstanceAttributesRef<FVector3f> SourceNormals = SourceAttributeGetter.GetVertexInstanceNormals();
	//TVertexInstanceAttributesRef<FVector4> Colors = AttributeGetter.GetVertexInstanceColors();
	TVertexInstanceAttributesRef<FVector2f> SourceUVs = SourceAttributeGetter.GetVertexInstanceUVs();
	TPolygonGroupAttributesRef<FName> PolyGroupMaterialSlotNames = SourceAttributeGetter.GetPolygonGroupMaterialSlotNames();


	FStaticMeshAttributes TargetAttributeGetter(*TargetMeshDescription);

	TVertexAttributesRef<FVector3f> TargetVertexPositions = TargetAttributeGetter.GetVertexPositions();
	TVertexInstanceAttributesRef<FVector3f> TargetTangents = TargetAttributeGetter.GetVertexInstanceTangents();
	TVertexInstanceAttributesRef<float> TargetBinormalSigns = TargetAttributeGetter.GetVertexInstanceBinormalSigns();
	TVertexInstanceAttributesRef<FVector3f> TargetNormals = TargetAttributeGetter.GetVertexInstanceNormals();
	TVertexInstanceAttributesRef<FVector4f> TargetColors = TargetAttributeGetter.GetVertexInstanceColors();
	TVertexInstanceAttributesRef<FVector2f> TargetUVs = TargetAttributeGetter.GetVertexInstanceUVs();

	const FQuat OffsetRotationQuat = FQuat::MakeFromEuler(FVector(Properties.OffsetRotation, 0.f, 0.f));

	uint32 NumIndices = 0;

	TargetMeshDescription->PolygonGroups() = SourceMeshDescription->PolygonGroups();
	TargetMeshDescription->PolygonGroupAttributes() = SourceMeshDescription->PolygonGroupAttributes();
	
	for(int32 InstanceIdx = 0; InstanceIdx < NumInstancesToBuild; InstanceIdx++)
	{
		for (int32 i = 0; i < SourceMeshDescription->VertexInstances().Num(); i++)
		{
			FVertexInstanceID VertInstance = FVertexInstanceID(i); /*MeshDescription->VertexInstances();*/
			FVertexID Vert = SourceMeshDescription->GetVertexInstanceVertex(VertInstance);

			FVector SourceVertPosition = FVector(SourceVertexPositions[Vert]);
			const float SourceVertAlongFrac = FMath::GetMappedRangeValueClamped(FVector2D(Bounds.Min.X, Bounds.Max.X), FVector2D(0.f, 1.f), SourceVertPosition.X);
			const float CableAlongFrac = (SourceVertAlongFrac + InstanceIdx) / NumInstancesToBuild;
			float SplineKey = SplineCurve.Points.Last().InVal * CableAlongFrac;

			const FCableMeshGenerationPointInfo PointInfo = CurveDescription.EvalPointInfo(SplineKey);
			
			FVector SplinePos = SplineCurve.Eval(SplineKey);
			FVector SplineTangent = SplineCurve.EvalDerivative(SplineKey);
			FVector SourceOffset = FVector(0.f, SourceVertPosition.Y, SourceVertPosition.Z);

			FQuat Rot = RotationCurve.Eval(SplineKey) * OffsetRotationQuat;
			FVector TargetOffset = Rot.RotateVector(SourceOffset) * WidthScaleFactor;

			FVector3f TargetPosition = FVector3f(SplinePos + TargetOffset);

			FDynamicMeshVertex DynamicVert(TargetPosition);
			DynamicVert.SetTangents(FVector3f( Rot.RotateVector(FVector(SourceTangents[VertInstance]))), FVector3f::ZeroVector, FVector3f( Rot.RotateVector(FVector(SourceNormals[VertInstance]))));
			DynamicVert.TextureCoordinate[0] = SourceUVs[VertInstance];

			FCableMeshGeneration::ApplyPointInfo(DynamicVert, PointInfo, CableAlongFrac);

			FCableMeshGeneration::AddDynamicVertToMeshDescription(DynamicVert, TargetMeshDescription, TargetVertexPositions, TargetTangents, TargetNormals, TargetBinormalSigns, TargetColors, TargetUVs );
			
		}

		const int32 InstanceOffset = NumIndices;

		for (int32 i = 0; i < SourceMeshDescription->Polygons().Num(); i++)
		{
			const FPolygonID SourcePolygonID(i);

			TArray<FVertexInstanceID> SourceVerts = SourceMeshDescription->GetPolygonVertexInstances(SourcePolygonID);
			const FPolygonGroupID PolyGroup = SourceMeshDescription->GetPolygonPolygonGroup(SourcePolygonID);

#if UE_BUILD_DEBUG
			ensure(SourceVerts.Num() == 3);
			ensure(TargetMeshDescription->IsPolygonGroupValid(PolyGroup));
#endif

			TArray<FVertexInstanceID> Tri = { FVertexInstanceID(InstanceOffset + SourceVerts[0].GetValue()), FVertexInstanceID(InstanceOffset + SourceVerts[1].GetValue()), FVertexInstanceID(InstanceOffset + SourceVerts[2].GetValue()) };
			TargetMeshDescription->CreatePolygon(PolyGroup, Tri);
			NumIndices+=3;
		}
	}

	ensure(TargetMeshDescription->VertexInstances().Num() == SourceMeshDescription->VertexInstances().Num() * NumInstancesToBuild);
	ensure(TargetMeshDescription->Triangles().Num() == SourceMeshDescription->Triangles().Num() * NumInstancesToBuild);
	ensure(TargetMeshDescription->Polygons().Num() == SourceMeshDescription->Polygons().Num() * NumInstancesToBuild);
}

int32 UTMG_CustomMesh::GetNumMaterials(FTetherCableProperties& CableProperties) const
{
	UStaticMesh* StaticMesh = CableProperties.CustomMeshProperties.SourceMeshReference.LoadSynchronous();
	if(!IsValid(StaticMesh))
	{
		return 0;
	}
	return FTetherMeshUtils::GetStaticMaterials(StaticMesh).Num();
}

UMaterialInterface* UTMG_CustomMesh::GetDefaultMaterial(FTetherCableProperties& CableProperties, int32 MaterialSlotIndex) const
{
	return CableProperties.CustomMeshProperties.SourceMeshReference->GetMaterial(MaterialSlotIndex);
}

UMaterialInterface* UTMG_CustomMesh::GetMaterial(FTetherCableProperties& CableProperties, int32 MaterialSlotIndex) const
{
	UMaterialInterface* OverrideMaterial = Super::GetMaterial(CableProperties, MaterialSlotIndex);
	if(OverrideMaterial)
	{
		return OverrideMaterial;
	}
	if(!CableProperties.CustomMeshProperties.SourceMeshReference)
	{
		return nullptr;
	}

	return CableProperties.CustomMeshProperties.SourceMeshReference->GetMaterial(MaterialSlotIndex);
}

void UTMG_CustomMesh::SetStaticMeshProperties(UStaticMesh* StaticMesh) const
{
	TArray<FStaticMaterial>& StaticMaterials = FTetherMeshUtils::GetStaticMaterials(StaticMesh);
	
	StaticMaterials.Reset();
	if (!Properties.SourceMeshReference)
	{
		return;
	}
	// Copy materials from source mesh
	StaticMaterials = FTetherMeshUtils::GetStaticMaterials(Properties.SourceMeshReference.LoadSynchronous());
	
	// Copy section info, which has the ability to remap materials making it important to preserve
	StaticMesh->GetSectionInfoMap() = Properties.SourceMeshReference->GetSectionInfoMap();
	
}
#endif

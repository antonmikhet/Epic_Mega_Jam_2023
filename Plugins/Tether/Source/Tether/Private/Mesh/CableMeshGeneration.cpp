// Copyright Sam Bonifacio 2021. All Rights Reserved.

#include "Mesh/CableMeshGeneration.h"
#include "DrawDebugHelpers.h"
#include "DynamicMeshBuilder.h"
#include "StaticMeshAttributes.h"
#include "RenderUtils.h"
#include "Mesh/CableMeshGenerationCurveDescription.h"
#include "Misc/EngineVersionComparison.h"

int32 GetVertIndex(int32 AlongIdx, int32 NumSides, int32 AroundIdx)
{
	return (AlongIdx * (NumSides + 1)) + AroundIdx;
}

void FCableMeshGeneration::BuildBasicMeshDescription(FMeshDescription* MeshDescription, const FCableMeshGenerationCurveDescription& Params, const FBasicMeshGenerationOptions& Options)
{
	if (!ensure(MeshDescription))
	{
		return;
	}

	// First, build the actual mesh in dynamic format, since we want the mesh generation to be cheapest for dynamic use
	
	TArray<FDynamicMeshVertex> DynamicVerts;
	TArray<int32> DynamicIndices;

	BuildDynamicBasicMesh(Params, Options, DynamicVerts, DynamicIndices, true);

	// Then convert to mesh description format
	ConvertToMeshDescription(DynamicVerts, DynamicIndices, nullptr, MeshDescription);

}

void FCableMeshGeneration::BuildDynamicBasicMesh(const FCableMeshGenerationCurveDescription& Params, const FBasicMeshGenerationOptions& Options, TArray<FDynamicMeshVertex>& OutVertices, TArray<int32>& OutIndices, bool bCalculateLength)
{
	const TArray<FCableMeshGenerationPoint>& Points = Params.Points;
	
	//const FColor VertexColor(255, 255, 255);
	const int32 NumPoints = Params.Points.Num();
	const int32 SegmentCount = NumPoints - 1;

	const int32 NumSides = Options.NumSides;
	const float CableWidth = Options.CableMeshWidth;

	// Build vertices

	// We double up the first and last vert of the ring, because the UVs are different
	const int32 NumRingVerts = NumSides + 1;

	float TotalLength = 0.f;
	TArray<float> PointDistances = {0}; // Start with 0 length for 1st point
	if(bCalculateLength || Options.bAutoTile)
	{
		for(int32 i=1; i< Points.Num(); i++)
		{
			const float Dist = FVector::Dist(Points[i-1].Location, Points[i].Location);
			TotalLength += Dist;
			PointDistances.Add(TotalLength);
		}
	}

	float TileMaterial = Options.TileUVs;

	if(Options.bAutoTile)
	{
		const float Circ = PI * CableWidth;
		TileMaterial = TotalLength / Circ;
		if(Options.bSnapToNearestFullTile)
		{
			TileMaterial = FMath::RoundToFloat(TileMaterial);
		}
	}

	// For each point along spline..
	for (int32 PointIdx = 0; PointIdx < NumPoints; PointIdx++)
	{
		float AlongFrac;

		if(bCalculateLength)
		{
			// Distance along cable using calculated lengths
			AlongFrac = PointDistances[PointIdx] / TotalLength;
		}
		else
		{
			// Distance along cable, assuming evenly spaced points
			AlongFrac = (float)PointIdx / (float)SegmentCount;
		}

		// Calculate forward direction for this loop
		const FVector ForwardDir = Points[PointIdx].Rotation.GetForwardVector();
		const FVector RightDir = Points[PointIdx].Rotation.GetRightVector();
		const FVector UpDir = Points[PointIdx].Rotation.GetUpVector();

		const FCableMeshGenerationPoint& PointInfo = Params.Points[PointIdx];

		//const uint8 R = Params.ContactPointDistances.IsValidIndex(PointIdx) ? Params.ContactPointDistances[PointIdx] * 255.f * 2.f : 0;
		//const uint8 G = Params.PointLooseness.IsValidIndex(PointIdx) ? Params.PointLooseness[PointIdx] * 255.f * 1.f : 0;
		const float R = PointInfo.Info.DistanceToNearestContactPoint;
		const float G = PointInfo.Info.Looseness;
		const float B = 0.f;//PointInfo.SlackRatio;
		const FLinearColor PointVertLinearColor = FLinearColor(R, G, B);
		const FColor PointVertColor = PointVertLinearColor.ToFColor(false); //FColor(R, G, 255);
		
		// Generate a ring of verts
		for (int32 VertIdx = 0; VertIdx < NumRingVerts; VertIdx++)
		{
			const float AroundFrac = float(VertIdx) / float(NumSides);
			// Find angle around the ring
			const float RadAngle = 2.f * PI * AroundFrac;
			// Find direction from center of cable to this vertex
			const FVector OutDir = (FMath::Cos(RadAngle) * UpDir) + (FMath::Sin(RadAngle) * RightDir);

			FDynamicMeshVertex Vert;
			Vert.Position = FVector3f(Points[PointIdx].Location + (OutDir * 0.5f * CableWidth));
			Vert.TextureCoordinate[0] = FVector2f(AlongFrac * TileMaterial, AroundFrac);
			Vert.TextureCoordinate[1] = FVector2f(PointInfo.Info.SegmentLineDistance, /*PointInfo.SegmentLength*/ PointInfo.Info.SlackRatio);
			Vert.TextureCoordinate[2] = FVector2f(AlongFrac, 0.f);
			Vert.Color = PointVertColor;
			Vert.SetTangents(FVector3f(ForwardDir), FVector3f(OutDir ^ ForwardDir), FVector3f(OutDir));
			OutVertices.Add(Vert);
		}
	}

	// Build triangles
	for (int32 SegIdx = 0; SegIdx < SegmentCount; SegIdx++)
	{
		for (int32 SideIdx = 0; SideIdx < NumSides; SideIdx++)
		{
			int32 TL = GetVertIndex(SegIdx, NumSides, SideIdx);
			int32 BL = GetVertIndex(SegIdx, NumSides, SideIdx + 1);
			int32 TR = GetVertIndex(SegIdx + 1, NumSides, SideIdx);
			int32 BR = GetVertIndex(SegIdx + 1, NumSides, SideIdx + 1);

			OutIndices.Add(TL);
			OutIndices.Add(BL);
			OutIndices.Add(TR);

			OutIndices.Add(TR);
			OutIndices.Add(BL);
			OutIndices.Add(BR);
		}
	}
}

void FCableMeshGeneration::ConvertToMeshDescription(const TArray<FDynamicMeshVertex>& DynamicVerts, const TArray<int32>& DynamicIndices, const TArray<int32>* PolyGroups, FMeshDescription* MeshDescription)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("FCableMeshGeneration::ConvertToMeshDescription"))
	
	FStaticMeshAttributes AttributeGetter(*MeshDescription);

	TVertexAttributesRef<FVector3f> VertexPositions = AttributeGetter.GetVertexPositions();
	TVertexInstanceAttributesRef<FVector3f> Tangents = AttributeGetter.GetVertexInstanceTangents();
	TVertexInstanceAttributesRef<float> BinormalSigns = AttributeGetter.GetVertexInstanceBinormalSigns();
	TVertexInstanceAttributesRef<FVector3f> Normals = AttributeGetter.GetVertexInstanceNormals();
	TVertexInstanceAttributesRef<FVector4f> Colors = AttributeGetter.GetVertexInstanceColors();
	TVertexInstanceAttributesRef<FVector2f> UVs = AttributeGetter.GetVertexInstanceUVs();

	// Convert verts
	for(int32 i=0; i<DynamicVerts.Num() ;i++)
	{
		const FDynamicMeshVertex& Vert = DynamicVerts[i];
		const FVertexID VertexID = MeshDescription->CreateVertex();
		const FVertexInstanceID VertexInstanceID = MeshDescription->CreateVertexInstance(VertexID);
		VertexPositions[VertexID] = Vert.Position;
		const FVector TangentX = Vert.TangentX.ToFVector();
		const FVector TangentZ = Vert.TangentZ.ToFVector();
		Tangents[VertexInstanceID] = FVector3f(TangentX);
		Normals[VertexInstanceID] = FVector3f(TangentZ);
		BinormalSigns[VertexInstanceID] = GetBasisDeterminantSign(TangentX, TangentZ ^ TangentX, TangentZ);
		Colors[VertexInstanceID] = FLinearColor(Vert.Color);
#if UE_VERSION_OLDER_THAN(5,0,0)
		UVs.SetNumIndices(3);
#else
		UVs.SetNumChannels(3);
#endif
		for(int32 Channel=0; Channel < 3; Channel++)
		{
			UVs.Set(VertexInstanceID, Channel, Vert.TextureCoordinate[Channel]);
		}
	}

	// Convert tris

	// Create initial poly group
	MeshDescription->CreatePolygonGroupWithID(FPolygonGroupID(0));

	for(int32 i=0; i<DynamicIndices.Num(); i+=3)
	{
		const FVertexInstanceID A = FVertexInstanceID(DynamicIndices[i]);
		const FVertexInstanceID B = FVertexInstanceID(DynamicIndices[i + 1]);
		const FVertexInstanceID C = FVertexInstanceID(DynamicIndices[i + 2]);

		FPolygonGroupID PolyGroup = FPolygonGroupID(0);
		if(PolyGroups)
		{
			// If poly groups were passed in, use them
			const int32 PolyIndex = i/3;
			if(PolyGroups->IsValidIndex(PolyIndex))
			{
				PolyGroup = FPolygonGroupID((*PolyGroups)[i/3]);
				// Create new poly group on mesh description if required
				if(!MeshDescription->IsPolygonGroupValid(PolyGroup))
				{
					MeshDescription->CreatePolygonGroupWithID(PolyGroup);
				}
			}
		}

		TArray<FVertexInstanceID> Tri = { A, B, C };
		MeshDescription->CreatePolygon(PolyGroup, Tri);
	}
}

void FCableMeshGeneration::ApplyPointInfo(FDynamicMeshVertex& Vert, const FCableMeshGenerationPointInfo& PointInfo, float AlongFrac)
{
	const float R = PointInfo.DistanceToNearestContactPoint;
	const float G = PointInfo.Looseness;
	const float B = 0.f;
	const FLinearColor PointVertLinearColor = FLinearColor(R, G, B);
	const FColor PointVertColor = PointVertLinearColor.ToFColor(false);

	Vert.TextureCoordinate[1] = FVector2f(PointInfo.SegmentLineDistance,PointInfo.SlackRatio);
	Vert.TextureCoordinate[2] = FVector2f(AlongFrac, 0.f);
	Vert.Color = PointVertColor;
}

void FCableMeshGeneration::AddDynamicVertToMeshDescription(const FDynamicMeshVertex& Vert, FMeshDescription* MeshDescription, TVertexAttributesRef<FVector3f>& VertexPositions, TVertexInstanceAttributesRef<FVector3f>& Tangents, TVertexInstanceAttributesRef<FVector3f>& Normals, TVertexInstanceAttributesRef<float>& BinormalSigns, TVertexInstanceAttributesRef<FVector4f>& Colors, TVertexInstanceAttributesRef<FVector2f>& UVs)
{
	const FVertexID VertexID = MeshDescription->CreateVertex();
	const FVertexInstanceID VertexInstanceID = MeshDescription->CreateVertexInstance(VertexID);
	VertexPositions[VertexID] = Vert.Position;
	const FVector TangentX = Vert.TangentX.ToFVector();
	const FVector TangentZ = Vert.TangentZ.ToFVector();
	Tangents[VertexInstanceID] = FVector3f( TangentX);
	Normals[VertexInstanceID] = FVector3f(TangentZ);
	BinormalSigns[VertexInstanceID] = GetBasisDeterminantSign(TangentX, TangentZ ^ TangentX, TangentZ);
	Colors[VertexInstanceID] = FLinearColor(Vert.Color);
#if UE_VERSION_OLDER_THAN(5,0,0)
	UVs.SetNumIndices(3);
#else
	UVs.SetNumChannels(3);
#endif
	for(int32 Channel=0; Channel < 3; Channel++)
	{
		UVs.Set(VertexInstanceID, Channel, Vert.TextureCoordinate[Channel]);
	}
}

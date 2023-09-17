// Copyright Sam Bonifacio 2021. All Rights Reserved.


#include "Mesh/TetherCableMeshComponent.h"


#include "CableSplineUtils.h"
#include "Mesh/CableMeshGeneration.h"
#include "StaticMeshResources.h"
#include "Engine/Engine.h"
#include "Materials/Material.h"
#include "Mesh/CableMeshGenerationCurveDescription.h"
#include "Misc/EngineVersionComparison.h"

/** Index Buffer */
class FTetherCableIndexBuffer : public FIndexBuffer
{
public:
	virtual void InitRHI() override
	{
		FRHIResourceCreateInfo CreateInfo(TEXT("FTetherCableIndexBuffer"));
		IndexBufferRHI = RHICreateIndexBuffer(sizeof(int32), NumIndices * sizeof(int32), BUF_Dynamic, CreateInfo);
	}

	int32 NumIndices;
};

/** Dynamic data sent to render thread */
struct FTetherCableDynamicData
{
	/** Array of points */
	TArray<FVector> CablePoints;
};

class FTetherCableSceneProxy final : public FPrimitiveSceneProxy
{
public:
	SIZE_T GetTypeHash() const override
	{
		static size_t UniquePointer;
		return reinterpret_cast<size_t>(&UniquePointer);
	}

	FTetherCableSceneProxy(UTetherCableMeshComponent* Component)
		: FPrimitiveSceneProxy(Component)
		, Material(nullptr)
		, VertexFactory(GetScene().GetFeatureLevel(), "FTetherCableSceneProxy")
		, DynamicData(nullptr)
		, MaterialRelevance(Component->GetMaterialRelevance(GetScene().GetFeatureLevel()))
		, NumSegments(Component->GetCablePointsWorldSpace().Num() - 1)
		, Options(Component->BasicMeshGenerationOptions)
		, StartTangent(Component->MeshGenerationParamsLocalSpace.StartTangent)
		, EndTangent(Component->MeshGenerationParamsLocalSpace.EndTangent)
	{
		ensure(NumSegments > 0);
		ensure(Options.NumSides > 0);
		
		VertexBuffers.InitWithDummyData(&VertexFactory, GetRequiredVertexCount());

		IndexBuffer.NumIndices = GetRequiredIndexCount();

		// Enqueue initialization of render resource
		BeginInitResource(&IndexBuffer);

		// Grab material
		Material = Component->GetMaterial(0);
		if (Material == nullptr)
		{
			Material = UMaterial::GetDefaultMaterial(MD_Surface);
		}
	}

	virtual ~FTetherCableSceneProxy()
	{
		VertexBuffers.PositionVertexBuffer.ReleaseResource();
		VertexBuffers.StaticMeshVertexBuffer.ReleaseResource();
		VertexBuffers.ColorVertexBuffer.ReleaseResource();
		IndexBuffer.ReleaseResource();
		VertexFactory.ReleaseResource();

		if (DynamicData != nullptr)
		{
			delete DynamicData;
		}
	}

	int32 GetRequiredVertexCount() const
	{
		return (NumSegments + 1) * (Options.NumSides + 1);
	}

	int32 GetRequiredIndexCount() const
	{
		return (NumSegments * Options.NumSides * 2) * 3;
	}

	int32 GetVertIndex(int32 AlongIdx, int32 AroundIdx) const
	{
		return (AlongIdx * (Options.NumSides + 1)) + AroundIdx;
	}

	void BuildCableMesh(const TArray<FVector>& InPoints, TArray<FDynamicMeshVertex>& OutVertices, TArray<int32>& OutIndices)
	{
		FCableMeshGenerationCurveDescription Params(InPoints);
		Params.StartTangent = StartTangent;
		Params.EndTangent = EndTangent;
		Params.CalcPointRotations();
		FCableMeshGeneration::BuildDynamicBasicMesh(Params, Options, OutVertices, OutIndices, true);
	}

#if UE_VERSION_OLDER_THAN(5,0,0)
	static void* LockVertexBuffer(FRHIVertexBuffer* VertexBuffer, uint32 Offset, uint32 SizeRHI, EResourceLockMode LockMode)
	{
		return RHILockVertexBuffer(VertexBuffer, Offset, SizeRHI, LockMode);
	}
#else
	static void* LockVertexBuffer(FRHIBuffer* VertexBuffer, uint32 Offset, uint32 SizeRHI, EResourceLockMode LockMode)
	{
		return RHILockBuffer(VertexBuffer, Offset, SizeRHI, LockMode);
	}
#endif

#if UE_VERSION_OLDER_THAN(5,0,0)
	static void UnlockVertexBuffer(FRHIVertexBuffer* VertexBuffer)
	{
		RHIUnlockVertexBuffer(VertexBuffer);
	}
#else
	static void UnlockVertexBuffer(FRHIBuffer* VertexBuffer)
	{
		RHIUnlockBuffer(VertexBuffer);
	}
#endif

	/** Called on render thread to assign new dynamic data */
	void SetDynamicData_RenderThread(FTetherCableDynamicData* NewDynamicData)
	{
		check(IsInRenderingThread());

		// Free existing data if present
		if (DynamicData)
		{
			delete DynamicData;
			DynamicData = nullptr;
		}
		DynamicData = NewDynamicData;

		// Build mesh from cable points
		TArray<FDynamicMeshVertex> Vertices;
		TArray<int32> Indices;
		BuildCableMesh(NewDynamicData->CablePoints, Vertices, Indices);

		check(Vertices.Num() == GetRequiredVertexCount());
		check(Indices.Num() == GetRequiredIndexCount());

		for (int i = 0; i < Vertices.Num(); i++)
		{
			const FDynamicMeshVertex& Vertex = Vertices[i];

			VertexBuffers.PositionVertexBuffer.VertexPosition(i) = Vertex.Position;
			VertexBuffers.StaticMeshVertexBuffer.SetVertexTangents(i, FVector3f(Vertex.TangentX.ToFVector()), Vertex.GetTangentY(), FVector3f( Vertex.TangentZ.ToFVector()));
			VertexBuffers.StaticMeshVertexBuffer.SetVertexUV(i, 0, Vertex.TextureCoordinate[0]);
			VertexBuffers.ColorVertexBuffer.VertexColor(i) = Vertex.Color;
		}

		{
			auto& VertexBuffer = VertexBuffers.PositionVertexBuffer;
			void* VertexBufferData = LockVertexBuffer(VertexBuffer.VertexBufferRHI, 0, VertexBuffer.GetNumVertices() * VertexBuffer.GetStride(), RLM_WriteOnly);
			FMemory::Memcpy(VertexBufferData, VertexBuffer.GetVertexData(), VertexBuffer.GetNumVertices() * VertexBuffer.GetStride());
			UnlockVertexBuffer(VertexBuffer.VertexBufferRHI);
		}

		{
			auto& VertexBuffer = VertexBuffers.ColorVertexBuffer;
			void* VertexBufferData = LockVertexBuffer(VertexBuffer.VertexBufferRHI, 0, VertexBuffer.GetNumVertices() * VertexBuffer.GetStride(), RLM_WriteOnly);
			FMemory::Memcpy(VertexBufferData, VertexBuffer.GetVertexData(), VertexBuffer.GetNumVertices() * VertexBuffer.GetStride());
			UnlockVertexBuffer(VertexBuffer.VertexBufferRHI);
		}

		{
			auto& VertexBuffer = VertexBuffers.StaticMeshVertexBuffer;
			void* VertexBufferData = LockVertexBuffer(VertexBuffer.TangentsVertexBuffer.VertexBufferRHI, 0, VertexBuffer.GetTangentSize(), RLM_WriteOnly);
			FMemory::Memcpy(VertexBufferData, VertexBuffer.GetTangentData(), VertexBuffer.GetTangentSize());
			UnlockVertexBuffer(VertexBuffer.TangentsVertexBuffer.VertexBufferRHI);
		}

		{
			auto& VertexBuffer = VertexBuffers.StaticMeshVertexBuffer;
			void* VertexBufferData = LockVertexBuffer(VertexBuffer.TexCoordVertexBuffer.VertexBufferRHI, 0, VertexBuffer.GetTexCoordSize(), RLM_WriteOnly);
			FMemory::Memcpy(VertexBufferData, VertexBuffer.GetTexCoordData(), VertexBuffer.GetTexCoordSize());
			UnlockVertexBuffer(VertexBuffer.TexCoordVertexBuffer.VertexBufferRHI);
		}

#if UE_VERSION_OLDER_THAN(5,0,0)
		void* IndexBufferData = RHILockIndexBuffer(IndexBuffer.IndexBufferRHI, 0, Indices.Num() * sizeof(int32), RLM_WriteOnly);
#else
		void* IndexBufferData = RHILockBuffer(IndexBuffer.IndexBufferRHI, 0, Indices.Num() * sizeof(int32), RLM_WriteOnly);
#endif
		FMemory::Memcpy(IndexBufferData, &Indices[0], Indices.Num() * sizeof(int32));
#if UE_VERSION_OLDER_THAN(5,0,0)
		RHIUnlockIndexBuffer(IndexBuffer.IndexBufferRHI);
#else
		RHIUnlockBuffer(IndexBuffer.IndexBufferRHI);
#endif
	}

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_CableSceneProxy_GetDynamicMeshElements);

		const bool bWireframe = AllowDebugViewmodes() && ViewFamily.EngineShowFlags.Wireframe;

		auto WireframeMaterialInstance = new FColoredMaterialRenderProxy(
			GEngine->WireframeMaterial ? GEngine->WireframeMaterial->GetRenderProxy() : nullptr,
			FLinearColor(0, 0.5f, 1.f)
		);

		Collector.RegisterOneFrameMaterialProxy(WireframeMaterialInstance);

		FMaterialRenderProxy* MaterialProxy = nullptr;
		if (bWireframe)
		{
			MaterialProxy = WireframeMaterialInstance;
		}
		else
		{
			MaterialProxy = Material->GetRenderProxy();
		}

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			if (VisibilityMap & (1 << ViewIndex))
			{
				const FSceneView* View = Views[ViewIndex];
				// Draw the mesh.
				FMeshBatch& Mesh = Collector.AllocateMesh();
				FMeshBatchElement& BatchElement = Mesh.Elements[0];
				BatchElement.IndexBuffer = &IndexBuffer;
				Mesh.bWireframe = bWireframe;
				Mesh.VertexFactory = &VertexFactory;
				Mesh.MaterialRenderProxy = MaterialProxy;

				bool bHasPrecomputedVolumetricLightmap;
				FMatrix PreviousLocalToWorld;
				int32 SingleCaptureIndex;
				bool bOutputVelocity;
				GetScene().GetPrimitiveUniformShaderParameters_RenderThread(GetPrimitiveSceneInfo(), bHasPrecomputedVolumetricLightmap, PreviousLocalToWorld, SingleCaptureIndex, bOutputVelocity);

				FDynamicPrimitiveUniformBuffer& DynamicPrimitiveUniformBuffer = Collector.AllocateOneFrameResource<FDynamicPrimitiveUniformBuffer>();
				DynamicPrimitiveUniformBuffer.Set(GetLocalToWorld(), PreviousLocalToWorld, GetBounds(), GetLocalBounds(), true, bHasPrecomputedVolumetricLightmap, DrawsVelocity(), bOutputVelocity);
				BatchElement.PrimitiveUniformBufferResource = &DynamicPrimitiveUniformBuffer.UniformBuffer;

				BatchElement.FirstIndex = 0;
				BatchElement.NumPrimitives = GetRequiredIndexCount() / 3;
				BatchElement.MinVertexIndex = 0;
				BatchElement.MaxVertexIndex = GetRequiredVertexCount();
				Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
				Mesh.Type = PT_TriangleList;
				Mesh.DepthPriorityGroup = SDPG_World;
				Mesh.bCanApplyViewModeOverrides = false;
				Collector.AddMesh(ViewIndex, Mesh);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
				// Render bounds
				RenderBounds(Collector.GetPDI(ViewIndex), ViewFamily.EngineShowFlags, GetBounds(), IsSelected());
#endif
			}
		}
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
	{
		FPrimitiveViewRelevance Result;
		Result.bDrawRelevance = IsShown(View);
		Result.bShadowRelevance = IsShadowCast(View);
		Result.bDynamicRelevance = true;
		MaterialRelevance.SetPrimitiveViewRelevance(Result);
		return Result;
	}

	virtual uint32 GetMemoryFootprint(void) const override { return(sizeof(*this) + GetAllocatedSize()); }

	uint32 GetAllocatedSize(void) const { return(FPrimitiveSceneProxy::GetAllocatedSize()); }

private:

	UMaterialInterface* Material;
	FStaticMeshVertexBuffers VertexBuffers;
	FTetherCableIndexBuffer IndexBuffer;
	FLocalVertexFactory VertexFactory;

	FTetherCableDynamicData* DynamicData;

	FMaterialRelevance MaterialRelevance;

	int32 NumSegments;

	FBasicMeshGenerationOptions Options;

	FVector StartTangent;
	FVector EndTangent;
};

UTetherCableMeshComponent::UTetherCableMeshComponent()
{
	const TArray<FVector> Points = {FVector::ZeroVector, FVector::ZeroVector};
	MeshGenerationParamsLocalSpace = FCableMeshGenerationCurveDescription(Points);
}

void UTetherCableMeshComponent::SetMeshGenerationParams(const FCableMeshGenerationCurveDescription& InMeshGenerationParamsLocalSpace, const FBasicMeshGenerationOptions& InOptions)
{
	FCableMeshGenerationCurveDescription StagedParams = InMeshGenerationParamsLocalSpace;

	// Whatever happens, need to make sure we have at least 2 points
	if (StagedParams.Points.Num() == 0)
	{
		StagedParams.Points.AddZeroed(2); // Set both points to zero
	}
	else if(StagedParams.Points.Num() == 1)
	{
		const FCableMeshGenerationPoint FirstPoint = StagedParams.Points[0];
		StagedParams.Points.Add(FirstPoint); // Copy first point
	}
	
	bool bMeshBuffersDirty = false;
	
	if(BasicMeshGenerationOptions != InOptions)
	{
		bMeshBuffersDirty = true;
	}
	else if(MeshGenerationParamsLocalSpace.Points.Num() != StagedParams.Points.Num())
	{
		bMeshBuffersDirty = true;
	}
	else if(MeshGenerationParamsLocalSpace.StartTangent != StagedParams.StartTangent)
	{
		bMeshBuffersDirty = true;
	}
	else if(MeshGenerationParamsLocalSpace.EndTangent != StagedParams.EndTangent)
	{
		bMeshBuffersDirty = true;
	}

	MeshGenerationParamsLocalSpace = StagedParams;
	BasicMeshGenerationOptions = InOptions;

	check(MeshGenerationParamsLocalSpace.Points.Num() > 1);

	if(bMeshBuffersDirty && IsRegistered())
	{
		ReregisterComponent();
	}

	// Need to send new data to render thread
	MarkRenderDynamicDataDirty();

	// Call this because bounds have changed
	UpdateComponentToWorld();
}

TArray<FVector> UTetherCableMeshComponent::GetCablePointsWorldSpace() const
{
	return FCableSplineUtils::LocalPointsToWorld(MeshGenerationParamsLocalSpace.GetPointLocations(), GetComponentTransform());
}

TArray<FVector> UTetherCableMeshComponent::GetCablePointsLocalSpace() const
{
	return MeshGenerationParamsLocalSpace.GetPointLocations();
}

void UTetherCableMeshComponent::SendRenderDynamicData_Concurrent()
{
	if (SceneProxy)
	{
		// Allocate cable dynamic data
		FTetherCableDynamicData* DynamicData = new FTetherCableDynamicData;

		DynamicData->CablePoints = GetCablePointsLocalSpace();

		// Enqueue command to send to render thread
		FTetherCableSceneProxy* CableSceneProxy = (FTetherCableSceneProxy*)SceneProxy;
		ENQUEUE_RENDER_COMMAND(FSendTetherCableDynamicData)(
			[CableSceneProxy, DynamicData](FRHICommandListImmediate& RHICmdList)
		{
			CableSceneProxy->SetDynamicData_RenderThread(DynamicData);
		});
	}
}

#if UE_VERSION_OLDER_THAN(4,25,0)
void UTetherCableMeshComponent::CreateRenderState_Concurrent()
#else
void UTetherCableMeshComponent::CreateRenderState_Concurrent(FRegisterComponentContext* Context)
#endif
{
#if UE_VERSION_OLDER_THAN(4,25,0)
	Super::CreateRenderState_Concurrent();
#else
	Super::CreateRenderState_Concurrent(Context);
#endif

	SendRenderDynamicData_Concurrent();
}

FBoxSphereBounds UTetherCableMeshComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	// Calculate bounding box of cable points
	FBox CableBox(ForceInit);
	TArray<FVector> CablePoints;
	
#if UE_VERSION_OLDER_THAN(5,0,0)
	CablePoints = GetCablePointsWorldSpace();
#else
	CablePoints = GetCablePointsLocalSpace();
#endif
	
	for (int32 ParticleIdx = 0; ParticleIdx < CablePoints.Num(); ParticleIdx++)
	{
		CableBox += CablePoints[ParticleIdx];
	}

	// Expand by cable radius (half cable width)
	const FBoxSphereBounds LocalBounds(CableBox.ExpandBy(0.5f * BasicMeshGenerationOptions.CableMeshWidth));
	
#if UE_VERSION_OLDER_THAN(5,0,0)
	return LocalBounds;
#else
	return LocalBounds.TransformBy(LocalToWorld);
#endif
	
}

FPrimitiveSceneProxy* UTetherCableMeshComponent::CreateSceneProxy()
{
	return new FTetherCableSceneProxy(this);
}

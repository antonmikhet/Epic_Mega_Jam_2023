// Copyright Sam Bonifacio 2021. All Rights Reserved.


#include "CableSplineUtils.h"
#include "TetherCableActor.h"
#include "TetherLogs.h"
#include "Engine/StaticMesh.h"
#include "Mesh/TetherCableMeshComponent.h"
#include "Mesh/TMG_Basic.h"
#include "Misc/EngineVersionComparison.h"
#if WITH_EDITOR
#include "Editor.h"
#endif

#if WITH_EDITOR

void ATetherCableActor::SetMeshType(ECableMeshGenerationType Type)
{
	CableProperties.MeshType = Type;
	
	// Setup mesh generator and forward appropriate properties for selected type
	// Ideally this could be done with inline editable instanced subobjects, but those can't save props to super object's config (and are apparently broken in general) so we store properties on the cable and forward them for now
	const TSubclassOf<UTetherMeshGenerator> DesiredMeshGeneratorClass = FCableMeshGenerationType::GetMeshGeneratorClass(CableProperties.MeshType);
	if(!MeshGenerator || !MeshGenerator->IsA(DesiredMeshGeneratorClass))
	{
		MeshGenerator = NewObject<UTetherMeshGenerator>(this, DesiredMeshGeneratorClass);
	}
	if(UTMG_Basic* BasicMeshGen = Cast<UTMG_Basic>(GetMeshGenerator()))
	{
		BasicMeshGen->Properties = CableProperties.BasicMeshProperties;
	}
	else if(UTMG_CustomMesh* CustomMeshGen = Cast<UTMG_CustomMesh>(GetMeshGenerator()))
	{
		CustomMeshGen->Properties = CableProperties.CustomMeshProperties;
	}
}

bool ATetherCableActor::IsDynamicPreview() const
{
	return DynamicPreviewMesh->IsVisible();
}

bool ATetherCableActor::QueueBuildAndShowStaticMesh(bool bForceEvenIfNotModified, bool bSynchronous)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("ATetherCableActor::BuildAndShowStaticMesh"))
	UE_LOG(LogTetherCable, Verbose, TEXT("%s: QueueBuildAndShowStaticMesh, bForceEvenIfNotModified: %i, bSynchronous: %i"), *GetHumanReadableName(), (int32)bForceEvenIfNotModified, (int32) bSynchronous);
	UE_LOG(LogTetherCable, Verbose, TEXT("%s: Attempting switching to static mesh"), *GetHumanReadableName());

	ensure(CanBeModified());

	if(!ensure(IsValid(this) && !IsActorBeingDestroyed()))
	{
		return false;
	}

	if(bLockCurrentState)
	{
		UE_LOG(LogTetherCable, Verbose, TEXT("%s: bLockCurrentState is true, abort mesh build"), *GetHumanReadableName());
		return false;
	}
	
	if(bEditing)
	{
		UE_LOG(LogTetherCable, Verbose, TEXT("%s: Cable is being placed, so don't build"), *GetHumanReadableName());
		return false;
	}

	if (!ensure(ActiveSimulationModel.HasAnyParticles()))
	{
		return false;
	}

	if (IsRunningAsyncSimulation())
	{
		ensure(!bSynchronous); // Doesn't really make sense to queue a synchronous build
		UE_LOG(LogTetherCable, Verbose, TEXT("%s: Queuing static mesh after simulation finishes"), *GetHumanReadableName());
		// Queue up static mesh build after simulation
		BuildAfterCurrentSimulation = bForceEvenIfNotModified ? AlwaysBuild : BuildIfModified;
		return false;
	}

	if (!ActiveSimulationModel.HasAnyParticles())
	{
		// If still no particles, abort
		UE_LOG(LogTetherCable, Verbose, TEXT("%s: Aborting build because there are no particles"), *GetHumanReadableName());
		return false;
	}

	// Build mesh if not up to date
	const FCableMeshGenerationCurveDescription MeshParamsLocalSpaceSimplified = MakeMeshGenerationCurveDescriptionFromCurrentSimulationState(true);
	BuildStaticMesh(MeshParamsLocalSpaceSimplified, FOnTetherAsyncMeshBuildCompleteDelegate::CreateWeakLambda(this, [this]()
	{
		// Hide dynamic preview and show static
		UpdateMeshVisibilities(true, true);

		UE_LOG(LogTetherCable, Verbose, TEXT("%s: Build succeeded"), *GetHumanReadableName());
	}), bForceEvenIfNotModified, bSynchronous);

	return true;
}

bool ATetherCableActor::CanAutoBuild() const
{
	return true;
}

void ATetherCableActor::BuildMesh()
{
	UE_LOG(LogTetherCable, VeryVerbose, TEXT("%s: BuildMesh -> QueueBuildAndShowStaticMesh"), *GetHumanReadableName());
	QueueBuildAndShowStaticMesh(false, true);
}

void ATetherCableActor::CancelAsyncMeshBuild()
{
	UE_LOG(LogTetherCable, Verbose, TEXT("%s: CancelAsyncMeshBuild"), *GetHumanReadableName());
	if(CurrentMeshBuildTask)
	{
		CurrentMeshBuildTask->CancelAndDelete();
		CurrentMeshBuildTask = nullptr;
		UE_LOG(LogTetherCable, Verbose, TEXT("%s: Cancelled async mesh build task, will be deleted"), *GetHumanReadableName());
		bBuildingMesh = false;
	}

}

const FCableMeshGenerationCurveDescription* ATetherCableActor::GetBuiltCurveDescription() const
{
	return &BuiltCurveDescriptionLocalSpaceSimplified;
}

void ATetherCableActor::UpdateStaticMeshObjectProperties()
{
	if(!ensure(StaticMesh))
	{
		return;
	}

	ensure(CanBeModified());

	// These properties are kept up to date on internal static meshes, even those saved with older versions of the plugin

	// TextExportTransient otherwise duplicating in editor will crash
	StaticMesh->SetFlags(RF_TextExportTransient);

	// Not DuplicateTransient, which should be controlled by the UPROPERTY
	StaticMesh->ClearFlags(RF_NonPIEDuplicateTransient | RF_DuplicateTransient);

	constexpr int32 LightmapCoordinateIndex = 3;
#if UE_VERSION_OLDER_THAN(4, 27, 0)
	StaticMesh->LightMapCoordinateIndex = LightmapCoordinateIndex;
#else
	StaticMesh->SetLightMapCoordinateIndex(LightmapCoordinateIndex);
#endif

	if(!ensure(StaticMesh->GetNumSourceModels() > 0))
	{
		return;
	}
	
	FStaticMeshSourceModel& SrcModel = StaticMesh->GetSourceModel(0);
	SrcModel.BuildSettings.SrcLightmapIndex = 0;
	SrcModel.BuildSettings.DstLightmapIndex = 3;

}

FBasicMeshGenerationOptions ATetherCableActor::GetPreviewMeshGenerationOptions() const
{
	FBasicMeshGenerationOptions Options;
	if(UTMG_Basic* BasicMeshGen = Cast<UTMG_Basic>(GetMeshGenerator()))
	{
		Options = BasicMeshGen->Properties.MeshGenerationOptions;
	}
	if(!Options.bOverrideCableWidth)
	{
		Options.CableMeshWidth = CableProperties.CableWidth;
	}
	return Options;
}

void ATetherCableActor::UpdateMeshVisibilities(bool bUseStaticMesh, bool bUpdateDynamicMeshIfVisible)
{
	ensure(CanBeModified());
	
	bool bUseDynamicMesh = !bUseStaticMesh;

	if(!bVisible || !ActiveSimulationModel.HasAnyParticles())
	{
		// If there are no simulation particles, don't show any mesh
		bUseDynamicMesh = false;
		bUseStaticMesh = false;
	}
	
	bool bModified = false;
	if(DynamicPreviewMesh->IsVisible() != bUseDynamicMesh)
	{
		DynamicPreviewMesh->SetVisibility(bUseDynamicMesh);
		bModified = true;
		if(bUseDynamicMesh && bUpdateDynamicMeshIfVisible)
		{
			DynamicPreviewMesh->SetMeshGenerationParams(MakeMeshGenerationCurveDescriptionFromCurrentSimulationState(false), GetPreviewMeshGenerationOptions());
		}
	}
	if(StaticMeshComponent->IsVisible() != bUseStaticMesh)
	{
		StaticMeshComponent->SetVisibility(bUseStaticMesh);
		bModified = true;
	}
	if(bModified)
	{
		UE_LOG(LogTetherCable, Verbose, TEXT("%s: Updated visibility mode: %s"), *GetHumanReadableName(), bUseStaticMesh ? TEXT("Static") : bUseDynamicMesh ? TEXT("Dynamic Preview") : TEXT("Hidden"));
	}
}
void ATetherCableActor::UpdateMeshVisibilitiesConditional(bool bUpdateDynamicMeshIfVisible)
{
	UpdateMeshVisibilities(IsStaticMeshUpToDate(), bUpdateDynamicMeshIfVisible);
}

UTetherMeshGenerator* ATetherCableActor::GetMeshGenerator() const
{
	return MeshGenerator;
}

void ATetherCableActor::CreateStaticMesh()
{
	UE_LOG(LogTetherCable, Verbose, TEXT("%s: Creating internal static mesh"), *GetHumanReadableName());

	ensure(CanBeModified());

	// Mark as TextExportTransient otherwise duplicating the Static Mesh subobject along with the actor causes a crash
	// Since it can be regenerated, it's not necessary to export
	// This doesn't seem to work if it's on the UPROPERTY flags - it needs to be done with object flags
	StaticMesh = NewObject<UStaticMesh>(this, "CableInternalStaticMesh" ,RF_TextExportTransient);
	StaticMesh->InitResources();

	const FGuid LightingGuid = FGuid::NewGuid();
#if UE_VERSION_OLDER_THAN(4, 27, 0)
	StaticMesh->LightingGuid = LightingGuid;
#else
	StaticMesh->SetLightingGuid(LightingGuid);
#endif

	// Add source to new StaticMesh
	FStaticMeshSourceModel& SrcModel = StaticMesh->AddSourceModel();
	SrcModel.BuildSettings.bRecomputeNormals = false;
	SrcModel.BuildSettings.bRecomputeTangents = false;
	SrcModel.BuildSettings.bRemoveDegenerates = false;
	SrcModel.BuildSettings.bUseHighPrecisionTangentBasis = false;
	SrcModel.BuildSettings.bUseFullPrecisionUVs = false;
	SrcModel.BuildSettings.bGenerateLightmapUVs = true;
	SrcModel.BuildSettings.MinLightmapResolution = 128;
	
	UpdateStaticMeshObjectProperties();

	// Set default lightmap res
	constexpr int32 LightMapResolution = 128;
#if UE_VERSION_OLDER_THAN(4, 27, 0)
	StaticMesh->LightMapResolution = LightMapResolution;
#else
	StaticMesh->SetLightMapResolution(LightMapResolution);
#endif

	StaticMesh->ImportVersion = EImportStaticMeshVersion::LastVersion;

}

bool ATetherCableActor::IsStaticMeshUpToDate() const
{
	const FCableMeshGenerationCurveDescription MeshParamsLocalSpaceSimplified = MakeMeshGenerationCurveDescriptionFromCurrentSimulationState(true);
	return MeshParamsLocalSpaceSimplified == BuiltCurveDescriptionLocalSpaceSimplified && !bMeshPropertiesModified;
}

void ATetherCableActor::BuildStaticMesh(const FCableMeshGenerationCurveDescription& CurveDescriptionLocalSpace, FOnTetherAsyncMeshBuildCompleteDelegate Callback, bool bForceEvenIfNotModified, bool bSynchronous )
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("ATetherCableActor::BuildStaticMesh"))
	
	check(IsInGameThread());

	checkNoRecursion();

	ensure(CanBeModified());

	if(IsBuildingMesh())
	{
		return;
	}
	
	// If curve description and properties have not changed, cancel no-op
	const bool bCurveDescriptionModified = CurveDescriptionLocalSpace != BuiltCurveDescriptionLocalSpaceSimplified;
	UE_LOG(LogTetherCable, Verbose, TEXT("%s: Curve description modified: %i, mesh properties modified: %i"), *GetHumanReadableName(), bCurveDescriptionModified, bMeshPropertiesModified);
	if (!bCurveDescriptionModified && !bMeshPropertiesModified)
	{
		if(bForceEvenIfNotModified)
		{
			UE_LOG(LogTetherCable, Verbose, TEXT("%s: Params match last built, but bForceEvenIfNotModified true so continue build anyway"), *GetHumanReadableName());
		}
		else
		{
			UE_LOG(LogTetherCable, Verbose, TEXT("%s: Cancelling build because params match last built"), *GetHumanReadableName());
			return;
		}
	}

	// Don't build while placing
	if (!ensure(!bEditing))
	{
		return;
	}

	UE_LOG(LogTetherCable, Verbose, TEXT("%s: Building static mesh because parameters were modified"), *GetHumanReadableName());

	if (!IsValid(StaticMesh))
	{
		CreateStaticMesh();
	}

	FMeshDescription* MeshDescription = StaticMesh->CreateMeshDescription(0);

	if (!ensure(MeshDescription))
	{
		return;
	}

	ensure(StaticMesh->GetOuter() == this);

	UE_LOG(LogTetherCable, Verbose, TEXT("%s: Updating static mesh geometry"), *GetHumanReadableName());

	if(!GetMeshGenerator())
	{
		UE_LOG(LogTetherCable, Error, TEXT("%s: No mesh generator, aborting build"), *GetHumanReadableName());
		return;
	}

	StaticMeshComponent->SetStaticMesh(nullptr);

	GetMeshGenerator()->PrepareResources();
	
	FTetherAsyncMeshBuildTaskParams Params;
	Params.CurveDescription = CurveDescriptionLocalSpace;
	Params.MeshGenerator = GetMeshGenerator();
	Params.MeshDescription = MeshDescription;
	Params.StaticMesh = StaticMesh;
	Params.CableWidth = CableProperties.CableWidth;
	Params.bSilent = CanSilentBuild(); // Don't create a dialog if mesh generator is quick enough
	const FOnTetherAsyncMeshBuildCompleteDelegate MeshBuildTaskCompleteCallback = FOnTetherAsyncMeshBuildCompleteDelegate::CreateWeakLambda(this, [this, CurveDescriptionLocalSpace, Callback, bSynchronous]()
	{
		ensure(IsInGameThread());
		ensure(IsValid(this));
		ensure(IsValidLowLevelFast());
		
		if(CurrentMeshBuildTask)
		{
			check(CurrentMeshBuildTask->IsDone());
			CurrentMeshBuildTask = nullptr;
		}
		
		// Build complete

		bBuildingMesh = false;

		UE_LOG(LogTetherCable, Verbose, TEXT("%s: Finished build"), *GetHumanReadableName());

		ensure(IsValid(StaticMeshComponent) && IsValid(StaticMesh));
		const bool bSetSuccess =  StaticMeshComponent->SetStaticMesh(StaticMesh);
		ensure(bSetSuccess);

		ensure(IsValid(this));
		BuiltCurveDescriptionLocalSpaceSimplified = CurveDescriptionLocalSpace;
		bMeshPropertiesModified = false;

		UE_LOG(LogTetherCable, Verbose, TEXT("%s: Static mesh geometry updated"), *GetHumanReadableName());

		// Clear lightmap data so that the new mesh doesn't have black splotches all over it
		StaticMeshComponent->InvalidateLightingCacheDetailed(true, false);
		GEditor->RedrawAllViewports();
		
		Callback.ExecuteIfBound();

		if(!bSynchronous)
		{
			UE_LOG(LogTetherCable, VeryVerbose, TEXT("%s: BuildStaticMesh: Build complete -> QueueBuildAndShowStaticMesh"), *GetHumanReadableName());
			QueueBuildAndShowStaticMesh(false, false);
		}

	});
	Params.Callback = MeshBuildTaskCompleteCallback;

	// Show dynamic preview mesh while building
	UpdateMeshVisibilities(false, true);

	UE_LOG(LogTetherCable, VeryVerbose, TEXT("%s: BuildStaticMesh: Starting mesh build task, bSynchronous: %i"), *GetHumanReadableName(), bSynchronous);

	ensure(CurrentMeshBuildTask == nullptr);

	FAsyncTaskExecuterWithAbort<FTetherAsyncMeshBuildTask>* NewMeshBuildTask = new FAsyncTaskExecuterWithAbort<FTetherAsyncMeshBuildTask>(Params);
	bBuildingMesh = true;
	if(bSynchronous)
	{
		NewMeshBuildTask->StartSynchronousTask();
	}
	else
	{
		CurrentMeshBuildTask = NewMeshBuildTask;
		NewMeshBuildTask->StartBackgroundTask();
	}

}

bool ATetherCableActor::CanSilentBuild() const
{
	if (!GetMeshGenerator())
	{
		return false;
	}

	return !GetMeshGenerator()->IsSlowBuild();
}

void ATetherCableActor::HandleMeshGenerator(UTetherMeshGenerator* NewMeshGenerator)
{
	if(!NewMeshGenerator)
	{
		return;
	}
	
	NewMeshGenerator->OnModified.AddWeakLambda(this, [this](FPropertyChangedEvent& PropertyChangedEvent)
    {
        // Handle mesh generator property change as our own to trigger mesh rebuild
        UE_LOG(LogTetherCable, Verbose, TEXT("%s: Mesh generator properties modified"), *GetHumanReadableName());
        HandlePropertyChanged(PropertyChangedEvent);
    });
}

void ATetherCableActor::BlockUntilMeshBuildComplete()
{
	// This doesn't really work currently, since it also blocks the GT return handler from finishing the task
	check(0);
	bool bBlocked = false;
	if(IsBuildingMesh())
	{
		UE_LOG(LogTetherCable, Log, TEXT("%s: Blocking main thread while mesh build finishes"), *GetHumanReadableName());
		const float StartTime = GetWorld()->GetRealTimeSeconds();
		while (IsBuildingMesh())
		{
			FPlatformProcess::Sleep(.01f);
			bBlocked = true;
		}
		UE_LOG(LogTetherCable, Log, TEXT("%s: Blocked for %f seconds"), *GetHumanReadableName(), GetWorld()->GetRealTimeSeconds() - StartTime);
	}
}

FCableMeshGenerationCurveDescription ATetherCableActor::MakeMeshGenerationCurveDescriptionFromCurrentSimulationState(bool bSimplifyCurve) const
{
	if(!ActiveSimulationModel.HasAnyParticles())
	{
		return FCableMeshGenerationCurveDescription();
	}
	
	TArray<FVector> WorldPoints = ActiveSimulationModel.GetParticleLocations();

	// Give mesh generator a chance to optimize points
	if(GetMeshGenerator())
	{
		GetMeshGenerator()->OptimizeCurvePoints(WorldPoints, CableProperties.CableWidth);
	}

	// Convert points from world to local space
	const FTransform MeshTransform = DynamicPreviewMesh->GetComponentTransform();
	const TArray<FVector> LocalPoints = FCableSplineUtils::WorldPointsToLocal(WorldPoints, MeshTransform);

	// Construct curve description from points
	FCableMeshGenerationCurveDescription CurveDescription(LocalPoints);

	FCableSplineUtils::CalculatePointInfo(GetWorld(), WorldPoints, CableProperties.GetCollisionWidth() * 0.5f * 1.1f, { this }, CurveDescription.Points);

	ensure(CurveDescription.Points.Num() > 0);

	if(GetPointSegmentDefinitions()[0]->SimulationOptions.ShouldUseSplineTangents())
	{
		ensure(!ActiveSimulationModel.Segments[0].SplineSegmentInfo.StartLeaveTangent.IsZero());
		CurveDescription.StartTangent = MeshTransform.InverseTransformVector(ActiveSimulationModel.Segments[0].SplineSegmentInfo.StartLeaveTangent);
		ensure(!CurveDescription.StartTangent.IsZero());
		//::DrawDebugLine(GetWorld(), WorldPoints[0], WorldPoints[0] + MeshTransform.TransformVector(CurveDescription.StartTangent), FColor::White, true, 5.f, SDPG_Foreground, 5.f);
	}

	const int32 EndPointIndex = GetNumGuideSplinePoints() - 1;
	if(GetPointSegmentDefinitions()[EndPointIndex]->SimulationOptions.ShouldUseSplineTangents())
	{
		ensure(!ActiveSimulationModel.Segments.Last().SplineSegmentInfo.EndArriveTangent.IsZero());
		CurveDescription.EndTangent = MeshTransform.InverseTransformVector(ActiveSimulationModel.Segments.Last().SplineSegmentInfo.EndArriveTangent);
		ensure(!CurveDescription.EndTangent.IsZero());
		//::DrawDebugLine(GetWorld(), WorldPoints.Last(), WorldPoints.Last() + MeshTransform.TransformVector(CurveDescription.EndTangent), FColor::White, true, 5.f, SDPG_Foreground, 5.f);
	}

	CurveDescription.CalcPointRotations();

	return CurveDescription;
}
#endif

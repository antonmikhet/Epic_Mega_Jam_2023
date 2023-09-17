// Copyright Sam Bonifacio 2021. All Rights Reserved.


#include "TetherCableActor.h"
#include "TetherLogs.h"
#include "Mesh/TetherCableMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Mesh/TetherMeshUtils.h"

#if WITH_EDITOR
#include "Editor.h"
#include "LevelEditorViewport.h"
#include "CableSplineUtils.h"
#include "DrawDebugHelpers.h"
#include "TetherDebugDrawHelpers.h"
#include "Mesh/CableMeshGenerationCurveDescription.h"
#include "Simulation/TetherAsyncSimulationTask.h"
#include "Simulation/TetherSimulation.h"
#include "Simulation/TetherSimulationParams.h"
#include "EngineUtils.h"
#include "Engine/Selection.h"
#include "Mesh/TMG_Basic.h"
#include "Materials/Material.h"
#endif

ATetherCableActor::ATetherCableActor()
{
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SceneRoot->SetMobility(EComponentMobility::Static);
	RootComponent = SceneRoot;
	
#if WITH_EDITOR
	// Create guide spline if in editor
	GuideSpline = CreateDefaultSubobject<UTetherGuideSplineComponent>(TEXT("GuideSpline"));
	GuideSpline->bIsEditorOnly = true;
	GuideSpline->SetMobility(EComponentMobility::Static);
	GuideSpline->SetupAttachment(SceneRoot);
	GuideSpline->SetSplinePointType(0, ESplinePointType::Linear, true);

	if(!HasAnyFlags(RF_WasLoaded))
	{
		// Ensure there's only 1 point so far - any more will be added later
		// Since Spline Component starts with 2 points we need to remove 1 of them
		// Don't do this when loaded, because removing the point changes the outer of the point definition sub-object and will crash if it's being loaded at that point
		// But it can't be in PostActorCreated, because in that case exporting text for duplication won't properly know what's "default" and will crash when invalid text is then being pasted into the new actor
		GetGuideSplineComponent()->RemoveSplinePoint(1, true);
		ensure(GetGuideSplineComponent()->GetNumberOfSplinePoints() == 1);
	}
#endif

#if WITH_EDITOR
	// Create dynamic preview mesh if in editor
	DynamicPreviewMesh = CreateDefaultSubobject<UTetherCableMeshComponent>(TEXT("TetherDynamicMesh"));
	DynamicPreviewMesh->bIsEditorOnly = true;
	DynamicPreviewMesh->SetupAttachment(SceneRoot);
	DynamicPreviewMesh->SetAbsolute(true, true, true);
#endif

	StaticMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMeshComponent"));
	StaticMeshComponent->SetMobility(EComponentMobility::Static);
	StaticMeshComponent->SetupAttachment(SceneRoot);
	StaticMeshComponent->SetAbsolute(true, true, true);

	PrimaryActorTick.bCanEverTick = true;

#if WITH_EDITOR
	if(CanBeModified())
	{
		GuideSpline->OnModified.AddWeakLambda(this, [this]()
	    {
	        ensure(IsInGameThread());
	        // The spline component is still operating on itself at this time and might be in an invalid state, so set a flag and worry about it next tick
	        bGuideSplineModified = true;
	    });

		GuideSpline->OnPointRemoved.AddWeakLambda(this, [this](int32 Index)
	    {
	        ensure(IsInGameThread());
	        // The spline component is still operating on itself at this time and might be in an invalid state, so set a flag and worry about it next tick
	        PendingRemovedGuideSplinePoints.AddUnique(Index);
	    });
		
		RealtimeSimulationCompleteDelegate = FOnTetherAsyncSimulationCompleteDelegate::CreateUObject(this, &ThisClass::RealtimeSimulationComplete);

		// Check !IsRunninGame() so that this doesn't happen in Standalone, which still has WITH_EDITOR
		if(!HasAnyFlags(EObjectFlags::RF_ClassDefaultObject))
		{
			FEditorDelegates::OnLightingBuildStarted.AddUObject(this, &ThisClass::OnLightingBuildStarted);
			USelection::SelectObjectEvent.AddWeakLambda(this, [this](UObject* Object)
	        {
	            if(Object != this)
	            {
	                // Ignore other objects
	                return;
	            }

	            if(GEditor->GetSelectedActors()->IsSelected(this))
	            {
	                // Ignore if selected, we only care when we're deselected
	                return;
	            }
				
	            if(ActiveSimulationModel.HasAnyParticles() && CanAutoBuild())
	            {
	                UE_LOG(LogTetherCable, VeryVerbose, TEXT("%s: Deselected -> QueueBuildAndShowStaticMesh"), *GetHumanReadableName());
	                QueueBuildAndShowStaticMesh();
	            }
	        });
			GEditor->OnEndObjectMovement().AddUObject(this, &ThisClass::OnEditorEndObjectMovement);
		}
	}
#endif
}

#if WITH_EDITOR

void ATetherCableActor::SetEditing(bool bIsEditing)
{
	if (bEditing != bIsEditing)
	{
		bEditing = bIsEditing;
		if (!bEditing)
		{
			InvalidateAllSegments();
			UpdateAndRebuildModifiedSegments(false,BuildIfModified);
		}
	}
}

void ATetherCableActor::HandlePropertyChanged(FPropertyChangedEvent& PropertyChangedEvent)
{
	if(GetWorld()->IsGameWorld())
	{
		return;
	}
	
	const EPropertyChangeType::Type ChangeType = PropertyChangedEvent.ChangeType;
	
	UE_LOG(LogTetherCable, Verbose, TEXT("%s: Property modified, invalidate all segments - ChangeType %i"), *GetHumanReadableName(), (int32)ChangeType);
	
	const bool bNoResimulate = PropertyChangedEvent.Property && PropertyChangedEvent.Property->HasMetaData("NoResimulate");

	if(bRealtimeSimulating)
	{
		// Realtime preview is enabled
		// Cancel existing async simulation if any so it doesn't break the realtime sim when it returns
		CancelAsyncSimulation();
		// Reset the simulation state of the active model so we're starting fresh
		if(!bNoResimulate)
		{
			RealtimeSimulationTimeRemainder = 0.f;
			InvalidateAndResimulate(false, DoNotBuild);
		}
	}
	else
	{
		// If the property change was not interactive (e.g. by slider movement) force build static mesh
		// We don't want to do this on slider changes because the mesh building is slow and opens a progress bar which takes focus and interrupts the slider movement
		const bool bBuildMesh = ChangeType != EPropertyChangeType::Interactive
            && ChangeType != EPropertyChangeType::Duplicate
            && ChangeType != EPropertyChangeType::Unspecified;
		
		if(!bNoResimulate)
		{
			bMeshPropertiesModified = true;

			UE_LOG(LogTetherCable, VeryVerbose, TEXT("%s: HandlePropertyChanged -> InvalidateAndResimulate"), *GetHumanReadableName());
			InvalidateAndResimulate(false, bBuildMesh ? BuildIfModified : DoNotBuild);
			if (bBuildMesh)
			{
				UE_LOG(LogTetherCable, VeryVerbose, TEXT("%s: HandlePropertyChanged -> QueueBuildAndShowStaticMesh"), *GetHumanReadableName());
				QueueBuildAndShowStaticMesh();
			}
		}
		else
		{
			// Even if the property is marked NoResimulate, still check for modified segments
			// This catches the case where LockCurrentState is being turned off, but something has been modified while it was locked so it needs to be rebuilt
			UE_LOG(LogTetherCable, VeryVerbose, TEXT("%s: HandlePropertyChanged -> UpdateAndRebuildModifiedSegments"), *GetHumanReadableName());
			UpdateAndRebuildModifiedSegments(false, bBuildMesh ? BuildIfModified : DoNotBuild, true);
		}
	}
}

const TCHAR* ToString(ESplinePointType::Type PointType)
{
	switch(PointType)
	{
	case ESplinePointType::Linear:
		return TEXT("Linear");
	case ESplinePointType::Curve:
		return TEXT("Curve");
	case ESplinePointType::Constant:
		return TEXT("Constant");
	case ESplinePointType::CurveClamped:
		return TEXT("CurveClamped");
	case ESplinePointType::CurveCustomTangent:
		return TEXT("CurveCustomTangent");
	default:
		return TEXT("Unknown");
	}
}

void ATetherCableActor::DumpCableInfo()
{
	UE_LOG(LogTetherCable, Log, TEXT("%s:"), *GetName());
	UE_LOG(LogTetherCable, Log, TEXT("    %i guide spline points:"), GetNumGuideSplinePoints());
	UE_LOG(LogTetherCable, Log, TEXT("    %i point segment definitions:"), GetPointSegmentDefinitions().Num());
	for(int32 i = 0; i < GetPointSegmentDefinitions().Num(); i++)
	{
		UE_LOG(LogTetherCable, Log, TEXT("        Point Segment %i:"), i);
		if(ActiveSimulationModel.Segments.IsValidIndex(i))
		{
			FTetherSimulationSegment& SimulationSegment = ActiveSimulationModel.Segments[i];
			UE_LOG(LogTetherCable, Log, TEXT("            Time Simulated: %f"), SimulationSegment.SimulationTime);
			UE_LOG(LogTetherCable, Log, TEXT("            Invalidated: %i"), SimulationSegment.IsInvalidated());
		}
		else
		{
			UE_LOG(LogTetherCable, Log, TEXT("            Not simulated"));
		}
		const ESplinePointType::Type SplinePointType = GetGuideSplineComponent()->GetSplinePointType(i);
		UE_LOG(LogTetherCable, Log, TEXT("            Spline Point Type: %s"), ToString(SplinePointType));

		UTetherPointSegmentDefinition* PointSegmentDefinition = GetPointSegmentDefinitions()[i];
		UE_LOG(LogTetherCable, Log, TEXT("            PointSegmentDefinition: %s"), *PointSegmentDefinition->GetName());
		UE_LOG(LogTetherCable, Log, TEXT("            Fixed Anchor Point: %i"), (int32)PointSegmentDefinition->SimulationOptions.bFixedAnchorPoint);
		UE_LOG(LogTetherCable, Log, TEXT("            Use Spline Tangents: %i"), (int32)PointSegmentDefinition->SimulationOptions.bUseSplineTangents);
	}
}

void ATetherCableActor::OnEditorEndObjectMovement(UObject& Obj)
{
	if(UActorComponent* Comp = Cast<UActorComponent>(&Obj))
	{
		if(Comp->GetOwner() == this)
		{
			// Component moved
			// This includes spline points moved while a component is selected
			// Update and rebuild with bBuildMesh true
			UpdateAndRebuildModifiedSegments(false, BuildIfModified, true);
		}
	}
}

#endif

void ATetherCableActor::PostActorCreated()
{
	UE_LOG(LogTetherCable, Verbose, TEXT("%s: PostActorCreated"), *GetHumanReadableName());
	
	Super::PostActorCreated();
	TimeCreated = FDateTime::UtcNow();
}

void ATetherCableActor::PostLoad()
{
	UE_LOG(LogTetherCable, Verbose, TEXT("%s: PostLoad"), *GetHumanReadableName());
	Super::PostLoad();

#if WITH_EDITOR
	if(CanBeModified())
	{
		UE_LOG(LogTetherCable, Verbose, TEXT("%s: Cable loaded, run initial update"), *GetHumanReadableName());

		if (IsValid(StaticMesh))
		{
			// If we loaded a static mesh, update its object flags in case they were saved with an older version
			UpdateStaticMeshObjectProperties();
		}
		
		UpdateAndRebuildModifiedSegments(false);
	}
#endif
}

void ATetherCableActor::PostInitializeComponents()
{
	UE_LOG(LogTetherCable, Verbose, TEXT("%s: PostInitializeComponents"), *GetHumanReadableName());
	Super::PostInitializeComponents();
}

void ATetherCableActor::PostInitProperties()
{
	UE_LOG(LogTetherCable, Verbose, TEXT("%s: PostInitProperties"), *GetHumanReadableName());
	Super::PostInitProperties();
}

void ATetherCableActor::OnConstruction(const FTransform& Transform)
{
	UE_LOG(LogTetherCable, Verbose, TEXT("%s: OnConstruction"), *GetHumanReadableName());
	Super::OnConstruction(Transform);

	if (bLockCurrentState)
	{
		// Meshes should follow actor transform when state locked

		if(ensure(StaticMeshComponent))
		{
			StaticMeshComponent->SetAbsolute(false, false, false);
			StaticMeshComponent->SetRelativeTransform(FTransform::Identity);
		}
#if WITH_EDITOR
		if(!IsRunningGame() && ensure(DynamicPreviewMesh))
		{
			DynamicPreviewMesh->SetAbsolute(false, false, false);
			DynamicPreviewMesh->SetRelativeTransform(FTransform::Identity);
		}
#endif
	}

	// Note: If not state locked, we don't set absolute true or update transforms here, because if the actor was moved while state locked it would return the meshes to the location of the last simulation
	// Instead, we wait for the next simulation to complete and update them then in HandleSimulationComplete
	
#if WITH_EDITOR
	if(CanBeModified())
	{
		SetMeshType(CableProperties.MeshType);

		HandleMeshGenerator(GetMeshGenerator());

		// Set material overrides on meshes (requires mesh generator)
		for(int32 i=0; i< GetMeshGenerator()->GetNumMaterials(CableProperties); i++)
		{
			UMaterialInterface* Material = GetMeshGenerator()->GetMaterial(CableProperties, i);
			if(!IsValid(Material))
			{
				// Replace null or invalid materials with default material
				Material = UMaterial::GetDefaultMaterial(MD_Surface);
			}
			ensure(IsValid(Material));
			DynamicPreviewMesh->SetMaterial(i, Material);
			StaticMeshComponent->SetMaterial(i, Material);
		}
	}

	// Cables in duplicated worlds won't have their own static mesh yet, so build it in case a duplicated map is being saved
	// Skip this if already running an async simulation (happens for cables that are duplicated within a world)
	if(!IsValid(StaticMesh) && BuiltCurveDescriptionLocalSpaceSimplified.Points.Num() > 0 && !IsRunningAsyncSimulation())
	{
		QueueBuildAndShowStaticMesh(true, true);
	}
#endif
	
}

void ATetherCableActor::BeginPlay()
{
	Super::BeginPlay();
}

void ATetherCableActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if(!CanBeModified())
	{
		return;
	}

#if WITH_EDITOR
	if(bRealtimeSimulating)
	{
		if(ActiveSimulationModel.GetNumSegments() > 0)
		{
			const float LastSegmentSimulationTime = ActiveSimulationModel.GetLastSegmentConst()->SimulationTime;
			const bool bCanSimulateMore = LastSegmentSimulationTime + CableProperties.SimulationOptions.SubstepTime <= CableProperties.SimulationOptions.SimulationDuration;
			UE_LOG(LogTetherCable, VeryVerbose, TEXT("%s: LastSegmentSimulationTime: %f, CanSimulateMore: %i"), *GetHumanReadableName(), LastSegmentSimulationTime, (int32)bCanSimulateMore);
			if (bCanSimulateMore)
			{
				const float ScaledDeltaTime = DeltaTime * RealtimeInEditorTimeDilation;
				RealtimeSimulationTimeRemainder += ScaledDeltaTime;
				UE_LOG(LogTetherCable, VeryVerbose, TEXT("%s: ScaledDeltaTime: %f, RealtimeSimulationTimeRemainder: %f"), *GetHumanReadableName(), ScaledDeltaTime, RealtimeSimulationTimeRemainder);
				if (!IsRunningAsyncSimulation())
				{
					const float SubstepTime = CableProperties.SimulationOptions.SubstepTime;
					if(RealtimeSimulationTimeRemainder >= SubstepTime)
					{
						// Clamp TimeToSimulate to max DeltaTime * 2
						// If left unclamped, a slow simulation that takes longer than DeltaTime to simulate gets more and more choppy as it falls further behind
						// But if clamped only to DeltaTime, the simulation will also fall behind and never be able to catch up
						// Simulate at the very least SubstepTime, otherwise simulation will never progress if DeltaTime is always less than SubstepTime
						const float TimeToSimulate = FMath::Clamp(RealtimeSimulationTimeRemainder, SubstepTime, DeltaTime * 2.f);
						UE_LOG(LogTetherCable, VeryVerbose, TEXT("%s: Running realtime sim for %f seconds"), *GetHumanReadableName(), TimeToSimulate);
						if (bSynchronousRealtime)
						{
							const FTetherSimulationResultInfo ResultInfo = PerformSimulation(ActiveSimulationModel, TimeToSimulate, false, DoNotBuild, false);
							RealtimeSimulationComplete(ActiveSimulationModel, ResultInfo);
						}
						else
						{
							const bool bSuccess = PerformAsyncSimulation(ActiveSimulationModel, TimeToSimulate, false, DoNotBuild, RealtimeSimulationCompleteDelegate);
							ensure(bSuccess);
						}
					}
				}
			}
			else
			{
				// Turn off realtime since simulation has hit the end
				SetRealtimeSimulating(false);
			}
		}
		else
		{
			UE_LOG(LogTetherCable, VeryVerbose, TEXT("%s: Not realtime simulating because there are no segments"), *GetHumanReadableName());
		}
	}

	if(bDebugVisualizeSimulationParticles)
	{
		TArray<FVector> ParticleLocations = ActiveSimulationModel.GetParticleLocations();
		FTetherDebugDrawHelpers::DrawPoints(GetWorld(), ParticleLocations);
	}
	
	if(bDebugVisualizeSimulationCollision)
	{
		DrawDebugSimulationParticleCollision(0.f);
	}

	if(bDebugVisualizeBuiltStaticMeshPoints || bDebugVisualizeContactPoints)
	{
		UWorld* World = GetWorld();
		const FTransform MeshTransform = DynamicPreviewMesh->GetComponentTransform();
		for(const FCableMeshGenerationPoint& Point : BuiltCurveDescriptionLocalSpaceSimplified.Points)
		{
			const FVector WorldLocation = MeshTransform.TransformPosition(Point.Location);
			const FQuat WorldQuat = MeshTransform.TransformRotation(Point.Rotation);
			const FRotator WorldRot = WorldQuat.Rotator();
			if(bDebugVisualizeBuiltStaticMeshPoints)
			{
				::DrawDebugPoint(World, WorldLocation, 10.f, FColor::White, false, 0.f, SDPG_Foreground);
				::DrawDebugCoordinateSystem(World, WorldLocation, WorldRot, 20.f, false, 0.f, SDPG_Foreground, 1.f);
			}
			if(bDebugVisualizeContactPoints && Point.Info.DistanceToNearestContactPoint == 0.f)
			{
				::DrawDebugPoint(World, WorldLocation, 20.f, FColor::Red, false, 0.f, SDPG_Foreground);
			}
		}
	}

	if(bDebugVisualizeComponentLocations)
	{
		::DrawDebugPoint(GetWorld(), GetActorLocation(), 40.f, FColor::Black, false, 0.f, SDPG_Foreground);
		::DrawDebugPoint(GetWorld(), ActiveSimulationModel.SimulationBaseWorldTransform.GetLocation(), 30.f, FColor::Green, false, 0.f, SDPG_Foreground);
		::DrawDebugPoint(GetWorld(), StaticMeshComponent->GetComponentLocation(), 10.f, FColor::White, false, 0.f, SDPG_Foreground);
	}

	if(bDebugVisualizeTangents)
	{
		for(FTetherSimulationSegment& Segment : ActiveSimulationModel.Segments)
		{
			FSplineSegmentInfo& SplineInfo = Segment.SplineSegmentInfo;
			::DrawDebugLine(GetWorld(), SplineInfo.StartLocation, SplineInfo.StartLocation + SplineInfo.StartLeaveTangent.GetSafeNormal() * 100.f, FColor::Blue, false, 0.f, SDPG_Foreground, 3.f);
			::DrawDebugLine(GetWorld(), SplineInfo.EndLocation, SplineInfo.EndLocation + SplineInfo.EndArriveTangent.GetSafeNormal() * -100.f, FColor::Blue, false, 0.f, SDPG_Foreground, 3.f);
		}
	}
#endif
}

#if UE_VERSION_OLDER_THAN(5,0,0)
void ATetherCableActor::PreSave(const ITargetPlatform* TargetPlatform)
#else
void ATetherCableActor::PreSave(FObjectPreSaveContext ObjectSaveContext)
#endif
{
#if WITH_EDITOR
	if(CanBeModified())
	{
		EnsureUpToDate();
	}
#endif

	if(ensureAlways(StaticMesh))
	{
		TArray<FStaticMaterial>& StaticMaterials = FTetherMeshUtils::GetStaticMaterials(StaticMesh);
		for (int32 MaterialIndex = 0; MaterialIndex < StaticMaterials.Num(); ++MaterialIndex)
		{
			const FMeshUVChannelInfo& UVChannelData = StaticMaterials[MaterialIndex].UVChannelData;
			ensureMsgf(UVChannelData.bInitialized, TEXT("%s StaticMesh material %i UVChannelData not initialized"), *GetFullName(), MaterialIndex);
		}
	}
#if UE_VERSION_OLDER_THAN(5,0,0)
	Super::PreSave(TargetPlatform);
#else
	Super::PreSave(ObjectSaveContext);
#endif
}

void ATetherCableActor::PostDuplicate(bool bDuplicateForPIE)
{
	UE_LOG(LogTetherCable, Verbose, TEXT("%s: PostDuplicate"), *GetHumanReadableName());
	
	Super::PostDuplicate(bDuplicateForPIE);

	if(!bDuplicateForPIE)
	{
		// Null out static mesh because it seems to still point to the source actor's one after duplication
#if UE_VERSION_OLDER_THAN(4,25,0)
		if(StaticMesh)
		{
			// NonPIEDuplicateTransient doesn't seem to work in 4.24, so null manually
			StaticMesh = nullptr;
		}
#endif
		
		ensure(!StaticMesh);

#if WITH_EDITOR
		// This gets set to true from the initial PostMove after duplicating, but we don't want to build the static mesh while duplicating
		BuildAfterCurrentSimulation = DoNotBuild;
#endif

		// Force update of Guide Spline to make sure it duplicated correctly
		GetGuideSplineComponent()->UpdateSpline();
	}

}

#if WITH_EDITOR

bool ATetherCableActor::CanEditChange(const FProperty* InProperty) const
{
	// Respect Super
	if(!Super::CanEditChange(InProperty))
	{
		return false;
	}

	// Allow all editing if not in game
	if(!GetWorld()->IsGameWorld())
	{
		return true;
	}

	// If in game, allow editing properties which will not resimulate the cable
	const bool bNoResimulate = InProperty && InProperty->HasMetaData("NoResimulate");
	return bNoResimulate;
}

void ATetherCableActor::PostEditMove(bool bFinished)
{
	Super::PostEditMove(bFinished);

	if(!CanBeModified())
	{
		return;
	}

	// Unfortunately, bFinished appears to be true when duplicate dragging, even though the move has not actually been finished yet
	// So we need to do another check to make sure we're not being duplicate dragged
	FViewport* Viewport = GEditor->GetActiveViewport();
	bool bDuplicateDragging = false;
	if(IsAltDown(Viewport) && Viewport->KeyState(EKeys::LeftMouseButton))
	{
		bDuplicateDragging = true;
	}

	const bool bBuildMesh = bFinished && !bDuplicateDragging;

	if(bBuildMesh)
	{
		UE_LOG(LogTetherCable, Verbose, TEXT("%s: Finished moving, switch to static mesh"), *GetHumanReadableName());
	}

	UE_LOG(LogTetherCable, Verbose, TEXT("%s: PostEditMove -> InvalidateAndResimulate"), *GetHumanReadableName());
	InvalidateAndResimulate(false, bBuildMesh ? BuildIfModified : DoNotBuild);
}

void ATetherCableActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	HandlePropertyChanged(PropertyChangedEvent);
}

void ATetherCableActor::PostEditUndo()
{
	Super::PostEditUndo();

	if(GetPointSegmentDefinitions().Num() == 0)
	{
		// We undid the actual actor creation itself, don't continue - the actor itself will be removed
		return;
	}

	UE_LOG(LogTetherCable, Verbose, TEXT("%s: Undo detected, resimulate"), *GetHumanReadableName());

	// Reset built static mesh params
	BuiltCurveDescriptionLocalSpaceSimplified = FCableMeshGenerationCurveDescription();
	InvalidateAndResimulate(false, BuildIfModified);
}

#endif

void ATetherCableActor::Destroyed()
{
#if WITH_EDITOR
	UE_LOG(LogTetherCable, Verbose, TEXT("%s: Cable was destroyed"), *GetHumanReadableName());
	CancelAsyncSimulation();
	CancelAsyncMeshBuild();
#endif

	Super::Destroyed();
}

#if WITH_EDITOR

FTransform ATetherCableActor::GetCableRootTransform() const
{
	return FTransform(RootComponent->GetRelativeRotation(), RootComponent->GetRelativeLocation(), RootComponent->GetRelativeScale3D());
}

FVector ATetherCableActor::CableRootTransformPosition(FVector Position) const
{
	return GetCableRootTransform().TransformPosition(Position);
}

FVector ATetherCableActor::CableRootTransformVector(FVector Vector) const
{
	return GetCableRootTransform().TransformVector(Vector);
}

void ATetherCableActor::OnLightingBuildStarted()
{
	UE_LOG(LogTetherCable, VeryVerbose, TEXT("%s: OnLightingBuildStarted -> EnsureUpToDate"), *GetHumanReadableName());
	EnsureUpToDate();
}

void ATetherCableActor::EnsureUpToDate()
{
	UE_LOG(LogTetherCable, Verbose, TEXT("%s: EnsureUpToDate"), *GetHumanReadableName());
	
	if(IsRunningAsyncSimulation())
	{
		CancelAsyncSimulation();
		//BlockUntilSimulationComplete();
	}

	if (IsBuildingMesh())
	{
		CancelAsyncMeshBuild();
		//BlockUntilMeshBuildComplete();
	}

	UpdateAndRebuildModifiedSegments(true, BuildIfModified, true);
}

#endif

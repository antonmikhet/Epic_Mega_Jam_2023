// Copyright Sam Bonifacio 2021. All Rights Reserved.

#include "TetherEdMode.h"

#include "CableSplineUtils.h"
#include "ComponentVisualizer.h"
#include "TetherModeToolkit.h"
#include "Toolkits/ToolkitManager.h"
#include "EditorModeManager.h"
#include "EditorViewportClient.h"
#include "DrawDebugHelpers.h"
#include "TetherCableActor.h"
#include "TetherModeSettings.h"
#include "EngineUtils.h"
#include "HModel.h"
#include "LevelEditor.h"
#include "SLevelViewport.h"
#include "STetherModeViewportOverlay.h"
#include "Engine/Selection.h"
#include "ComponentVisualizers/Public/SplineComponentVisualizer.h"
#include "TetherEditorLogs.h"
#include "Editor/UnrealEdEngine.h"
#include "Modules/ModuleManager.h"

const FEditorModeID FTetherEdMode::EM_Tether(TEXT("EM_Tether"));

FTetherEdMode::FTetherEdMode()
{
	SetModeSettings(NewObject<UTetherModeSettings>(GetTransientPackage(), TEXT("TetherModeSettings"), RF_Transactional));

	// Start at 1000 units if nothing is hit
	LastTraceResult.Distance = 1000.f;
}

FTetherEdMode::~FTetherEdMode()
{
	USelection::SelectionChangedEvent.RemoveAll(this);
}

void FTetherEdMode::SetModeSettings(UTetherModeSettings* Settings)
{
	ModeSettings = Settings;
}

bool FTetherEdMode::IsEditing() const
{
	return IsValid(EditingCable);
}

void FTetherEdMode::StartEditing(ATetherCableActor* Cable, int32 Point, bool bTransactional)
{
	if(!ensure(!IsEditing()))
	{
		return;
	}

	UE_LOG(LogTether, Verbose, TEXT("Start editing cable"));

	if(Point < 0)
	{
		Point = Cable->GetNumGuideSplinePoints() - 1;
	}

	if(bTransactional)
	{
		UE_LOG(LogTether, Verbose, TEXT("Begin transaction (Edit Cable)"));
		GEngine->BeginTransaction(TEXT("Tether"), INVTEXT("Edit Cable"), Cable);
	}

	EditingCable = Cable;
	EditingCable->Modify();
	EditingCable->SetEditing(true);

	GEditor->SelectActor(EditingCable, true, true, true, true);

	EditingPoint = Point;
	UE_LOG(LogTether, Verbose, TEXT("Started editing point %i"), EditingPoint);
}

void FTetherEdMode::StartNewSegment(FVector EndPointWorldLocation, bool bTransactional)
{
	UE_LOG(LogTether, Log, TEXT("Start new segment"));
	if (bTransactional)
	{
		UE_LOG(LogTether, Verbose, TEXT("End transaction"));
		GEngine->EndTransaction();
		UE_LOG(LogTether, Verbose, TEXT("Begin transaction (Add Cable Segment)"));
		GEngine->BeginTransaction(TEXT("Tether"), INVTEXT("Add Cable Segment"), EditingCable);
	}
	EditingCable->Modify();
	EditingPoint++;
	EditingCable->AddGuideSplinePoint(EditingPoint, EndPointWorldLocation);
	UE_LOG(LogTether, Verbose, TEXT("Now editing point %i"), EditingPoint);
}

void FTetherEdMode::CancelSegmentAndCommitCable(FEditorViewportClient* ViewportClient)
{
	if(EditingCable->GetNumGuideSplineSegments() > 1)
	{
		// Remove segment and commit
		EditingCable->RemovePreviewEndpoint();
		CommitCurrentCable(ViewportClient);
	}
	else
	{
		// If cancelling the only segment, cancel the whole cable
		CancelCable(ViewportClient);
	}
}

bool FTetherEdMode::IsEditingTangentMouseDown(const FEditorViewportClient* ViewportClient) const
{
	return IsKeyDown(ViewportClient, EKeys::LeftMouseButton);
}

bool FTetherEdMode::IsEditingTangentNormalAligned(const FEditorViewportClient* ViewportClient) const
{
	return IsKeyDown(ViewportClient, NormalAlignedTangentKey());
}

void FTetherEdMode::SetStartCableModeActive(bool bActive)
{
	bStartingCable = bActive;
	if (bActive)
	{
		// Deselect anything
		GEditor->SelectNone(true, true);
	}
}

bool FTetherEdMode::IsStartingCable() const
{
	return bStartingCable;
}

bool FTetherEdMode::CableToolsActive() const
{
	return GetModeSettings()->bAutoStartCable || bStartingCable || IsEditing();
}

void FTetherEdMode::CreateCableAtLocation(FVector WorldLocation)
{
	UE_LOG(LogTether, Log, TEXT("Start new cable"));
	UE_LOG(LogTether, Verbose, TEXT("Begin transaction (Create Cable)"));
	GEngine->BeginTransaction(TEXT("Tether"), INVTEXT("Create Cable"), nullptr);
	GEditor->SelectNone(true, true);

	ATetherCableActor* NewCable = CreateCableActor(WorldLocation);
	StartEditing(NewCable, -1, false);

	bStartingCable = false;
}

void FTetherEdMode::CommitCurrentCable(FEditorViewportClient* ViewportClient)
{
	// Must have at least 2 points
	ensure(GetEditingCable()->GetNumGuideSplinePoints() > 1);
	
	UE_LOG(LogTether, Log, TEXT("Commit cable"));
	EditingCable->Modify();
	EditingCable->SetEditing(false);
	EditingCable = nullptr;
	UE_LOG(LogTether, Verbose, TEXT("End transaction"));
	GEngine->EndTransaction();
	if(ViewportClient)
	{
		ViewportClient->Invalidate();
	}
}

void FTetherEdMode::Enter()
{
	FEdMode::Enter();

	if (!Toolkit.IsValid())
	{
		Toolkit = MakeShareable(new FTetherModeToolkit);
		Toolkit->Init(Owner->GetToolkitHost());
	}

	TArray<AActor*> SelectedActors;
	
	// Deselect anything that's not a cable
	GEditor->GetSelectedActors()->GetSelectedObjects<AActor>(SelectedActors);
	for(AActor* Actor : SelectedActors)
	{
		if(!IsSelectionAllowed(Actor, false))
		{
			GEditor->GetSelectedActors()->Deselect(Actor);
		}
	}
	GEditor->DeselectAllSurfaces();

	FLevelEditorModule& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
	const TSharedPtr<IAssetViewport> LViewport = LevelEditor.GetFirstActiveViewport();
	const TSharedRef<STetherModeViewportOverlay> ViewportOverlayRef = SNew(STetherModeViewportOverlay);
	ViewportOverlayRef->ViewportClient = &LViewport->GetAssetViewportClient();
	ViewportOverlay = ViewportOverlayRef;
	LViewport->AddOverlayWidget(ViewportOverlayRef);

	USelection::SelectionChangedEvent.AddRaw(this, &FTetherEdMode::OnEditorSelectionChanged);
}

void FTetherEdMode::Exit()
{
	bStartingCable = false;
	
	FToolkitManager::Get().CloseToolkit(Toolkit.ToSharedRef());
	Toolkit.Reset();

	if(ViewportOverlay.IsValid())
	{
		FLevelEditorModule& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
		const TSharedPtr<IAssetViewport> LViewport = LevelEditor.GetFirstActiveViewport();
		if(LViewport.IsValid())
		{
			LViewport->RemoveOverlayWidget(ViewportOverlay.ToSharedRef());
		}
		ViewportOverlay = nullptr;
	}

	USelection::SelectionChangedEvent.RemoveAll(this);

	FEdMode::Exit();
}

bool FTetherEdMode::HandleClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy,
	const FViewportClick& Click)
{
	return FEdMode::HandleClick(InViewportClient, HitProxy, Click);
}

bool FTetherEdMode::InputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event)
{
	if(CableToolsActive())
	{
		const float DeltaSlack = IsKeyDown(ViewportClient, FineAdjustSlackKey()) ? 2.5f : 25.f;

		if (IsEditing() && Key == EKeys::Escape && Event == IE_Pressed)
		{
			CancelSegmentAndCommitCable(ViewportClient);
			return true;
		}

		if (IsEditing() && Key == EKeys::MouseScrollDown && Event == IE_Pressed)
		{
			EditingCable->AddSegmentSlack(FMath::Max(0, EditingPoint - 1), DeltaSlack, !ShouldSuspendSimulation(ViewportClient));
			ViewportClient->Invalidate();
			return true;
		}

		if (IsEditing() && Key == EKeys::MouseScrollUp && Event == IE_Pressed)
		{
			EditingCable->AddSegmentSlack(FMath::Max(0, EditingPoint - 1), DeltaSlack * -1.f, !ShouldSuspendSimulation(ViewportClient));
			ViewportClient->Invalidate();
			return true;
		}

		if (Key == EKeys::LeftMouseButton)
		{
			// Since we care about mouse down (not just up) we can't do this in HandleClick
			if (Event == IE_Pressed)
			{
				if (HandleLeftMouseButtonDown(ViewportClient))
				{
					return true;
				}
			}
			else if (Event == IE_Released)
			{
				if (HandleLeftMouseButtonUp(ViewportClient))
				{
					return true;
				}
			}
		}

		if(IsEditing() && Key == SuspendSimulationKey() && Event == IE_Released)
		{
			// Rebuild if suspend key released
			EditingCable->UpdateAndRebuildModifiedSegments(false, DoNotBuild, true);
		}
	}

	return FEdMode::InputKey(ViewportClient, Viewport, Key, Event);
}

bool FTetherEdMode::MouseMove(FEditorViewportClient* ViewportClient, FViewport* Viewport, int32 x, int32 y)
{
	if(CableToolsActive())
	{
		if (IsEditingTangentMouseDown(ViewportClient))
		{
			return true;
		}
	}
	
	return FEdMode::MouseMove(ViewportClient, Viewport, x, y);
}

void FTetherEdMode::Tick(FEditorViewportClient* ViewportClient, float DeltaTime)
{
	FEdMode::Tick(ViewportClient, DeltaTime);

	// Check viewport client is as we expect
	//ensure(ViewportClient == GetViewportClient());

	if(CableToolsActive())
	{
		FHitResult Hit;
		TraceCursorToWorld(ViewportClient, Hit);
		const FVector AdjustedHitLocation = GetAdjustedHitLocation(ViewportClient, Hit);

		//::DrawDebugPoint(GetWorld(), AdjustedHitLocation, 5.f, FColor::White, false, 0.f);

		const bool bEditingTangentAltDown = IsEditingTangentNormalAligned(ViewportClient);
		const bool bEditingTangentMouseDown = IsEditingTangentMouseDown(ViewportClient);
		const bool bEditingTangent = bEditingTangentAltDown || bEditingTangentMouseDown;
		const bool bSuspendSimulation = ShouldSuspendSimulation(ViewportClient);

		if(IsEditing() && !bEditingTangent)
		{
			// Clear tangent
			SetTangent(FVector::ZeroVector);
		}
		
		if (IsEditing() && !bEditingTangentMouseDown)
		{
			// Update endpoint preview unless mouse down is held (we still want to move the endpoint if just Alt is being held)
			UE_LOG(LogTether, VeryVerbose, TEXT("Update Preview Point"));
			const bool bUpdated = EditingCable->UpdatePreviewPoint(EditingPoint, AdjustedHitLocation, !bSuspendSimulation);
			if (bUpdated)
			{
				ViewportClient->Invalidate();
			}
		}

		if (IsEditing() && bEditingTangent)
		{
			if(bEditingTangentMouseDown)
			{
				// Adjust point location based on cached point hit - the point location itself may change if switching between normal-aligned tangents and user-specified
				const FVector AdjustedPointLocation = GetAdjustedHitLocation(ViewportClient, CachedMouseDownHit);
				GetEditingCable()->SetPointLocation(EditingPoint, AdjustedPointLocation, !bSuspendSimulation);
			}

			FVector Tangent;

			if (bEditingTangentAltDown)
			{
				UE_LOG(LogTether, VeryVerbose, TEXT("Calculate normal-aligned tangent"));
				// If mouse is down, use CachedMouseDownHit, otherwise use hit from cursor
				FHitResult& HitForNormal = bEditingTangentMouseDown ? CachedMouseDownHit : Hit;
				// Align tangent to surface normal
				// If editing the first point, we want the tangent to come out at the surface normal
				// If further than the first point, reverse the surface normal so that the cable goes into the surface
				if (EditingPoint == 0)
				{
					Tangent = HitForNormal.ImpactNormal * 100.f;
				}
				else
				{
					Tangent = HitForNormal.ImpactNormal * -100.f;
				}
			}
			else
			{
				UE_LOG(LogTether, VeryVerbose, TEXT("Calculate user-specified tangent"));
				// Align tangent to user-specified direction on surface plane
				const FPlane PointHitPlane = FPlane(CachedMouseDownHit.Location, CachedMouseDownHit.ImpactNormal);

				const FViewportCursorLocation Cursor = ViewportClient->GetCursorWorldLocationFromMousePos();

				const FVector PlaneHitPoint = FMath::RayPlaneIntersection(Cursor.GetOrigin(), Cursor.GetDirection(), PointHitPlane);
				
				const FVector PointLocation = GetEditingCable()->GetGuideSplineComponent()->GetLocationAtSplinePoint(EditingPoint, ESplineCoordinateSpace::World);
				
				const FVector TangentPoint = PlaneHitPoint + CachedMouseDownHit.ImpactNormal * GetCableRadius();
				
				Tangent = TangentPoint - PointLocation;
			}

			SetTangent(Tangent);

			// Preview tangent
			const FSplineSegmentInfo& SplineInfo = FCableSplineUtils::SegmentInfoFromSplineComponent(GetEditingCable()->GetGuideSplineComponent(), GetEditingCable()->GetNumGuideSplinePoints() - 1, GetEditingCable()->GetTransform());
			::DrawDebugLine(GetWorld(), SplineInfo.EndLocation, SplineInfo.EndLocation + SplineInfo.EndArriveTangent.GetSafeNormal() * 100.f, FColor::Blue, false, 0.f, SDPG_Foreground, 3.f);
		}
	}
}

void FTetherEdMode::AddReferencedObjects(FReferenceCollector& Collector)
{
	FEdMode::AddReferencedObjects(Collector);

	Collector.AddReferencedObject(ModeSettings);
}

bool FTetherEdMode::IsSelectionAllowed(AActor* InActor, bool bInSelection) const
{
	if(!FEdMode::IsSelectionAllowed(InActor, bInSelection))
	{
		return false;
	}

	if(CableToolsActive() && bInSelection)
	{
		return InActor->IsA<ATetherCableActor>();
	}
	
	return true;
}

bool FTetherEdMode::ShouldSelectActor(AActor* InActor) const
{
	return IsSelectionAllowed(InActor, true) && InActor != EditingCable;
}

void FTetherEdMode::OnEditorSelectionChanged(UObject* Object)
{
	UE_LOG(LogTether, Verbose, TEXT("Selection modified"));
}

bool FTetherEdMode::HandleLeftMouseButtonDown(FEditorViewportClient* ViewportClient)
{
	if (!IsEditing())
	{
		// If we are not placing, these things should be selected instead of starting a new cable
		// but ending a cable still takes priority
		
		// Make a new hit proxy so that we can see what we clicked before tracing into the world
		const int32	HitX = ViewportClient->Viewport->GetMouseX();
		const int32	HitY = ViewportClient->Viewport->GetMouseY();
		HHitProxy* HitProxy = ViewportClient->Viewport->GetHitProxy(HitX, HitY);

		UE_LOG(LogTether, Verbose, TEXT("HitProxy: %s"), HitProxy ? HitProxy->GetType()->GetName() : TEXT("INVALID"));

		if(!HitProxy)
		{
			// No hit proxy
			return false;
		}

		// Allow selecting cable actors themselves
		if (HitProxy->IsA(HActor::StaticGetType()))
		{
			HActor* ActorHit = HitProxyCast<HActor>(HitProxy);
			if (ActorHit)
			{
				if (ShouldSelectActor(ActorHit->Actor))
				{
					// If we clicked a cable, ignore the click here so it can be selected normally
					return false;
				}

				// If clicked actor is not a cable, we'll continue the trace and maybe start a new cable
			}
		}
		else if(HitProxy->IsA(HModel::StaticGetType()))
		{
			// Hit proxy is HModel (BSP)
			// Continue trace and start new cable
		}
		else
		{
			// Hit proxy is something else, ignore click and handle normally
			// (for spline key selection, axis widgets, etc)
			return false;
		}
	}

	FHitResult Hit;
	TraceCursorToWorld(ViewportClient, Hit);
	const FVector AdjustedHitLocation = GetAdjustedHitLocation(ViewportClient, Hit);

	CachedMouseDownHit = Hit;

	UE_LOG(LogTether, Verbose, TEXT("Hit %s"), Hit.GetComponent() ? *Hit.GetComponent()->GetName() : TEXT("INVALID"));
	
	AActor* HitActor = Hit.GetActor();
	if (IsValid(HitActor) && HitActor->IsA<ATetherCableActor>())
	{
		// We can still hit cable actors from the trace even if we didn't get an Actor hit proxy
		// In that case select the cable
		GEditor->SelectActor(HitActor, true, true);
		return true;
	}
	//::DrawDebugPoint(InViewportClient->GetWorld(), WorldLocation, 10.f, FColor::White, false, 20.f);

	if(!IsEditing())
	{
		// If we're not placing yet, start a new cable at the clicked point
		CreateCableAtLocation(AdjustedHitLocation);
		return true;
	}

	return true;
}

bool FTetherEdMode::HandleLeftMouseButtonUp(FEditorViewportClient* ViewportClient)
{
	FHitResult Hit;
	TraceCursorToWorld(ViewportClient, Hit);
	const FVector AdjustedHitLocation = GetAdjustedHitLocation(ViewportClient, Hit);

	UE_LOG(LogTether, Verbose, TEXT("Hit %s"), Hit.GetComponent() ? *Hit.GetComponent()->GetName() : TEXT("INVALID"));
	
	if(IsEditing())
	{
		const bool bNewSegment = IsKeyDown(ViewportClient, ContinueCableKey()) || GetEditingCable()->GetNumGuideSplinePoints() < 2/* || (IsEditingPointUsingTangents() && EditingPoint >= GetEditingCable()->GetNumGuideSplinePoints() - 1 )*/;
		if (bNewSegment)
		{
			const bool bIsFirstSegment = GetEditingCable()->GetNumGuideSplineSegments() == 0;
			StartNewSegment(AdjustedHitLocation, !bIsFirstSegment);
			return true;
		}
		else
		{
			CommitCurrentCable(ViewportClient);
			return true;
		}
	}

	CachedMouseDownHit = FHitResult();

	return false;
}

bool FTetherEdMode::IsEditingPointUsingTangents() const
{
	if(!ensure(EditingPoint < GetEditingCable()->GetPointSegmentDefinitions().Num()))
	{
		return false;
	}
	
	return GetEditingCable()->GetPointSegmentDefinitions()[EditingPoint]->SimulationOptions.bUseSplineTangents;
}

bool FTetherEdMode::TraceCursorToWorld(FEditorViewportClient* ViewportClient, FHitResult& OutHit)
{
	if(ViewportClient->Viewport->GetSizeXY() == FIntPoint::ZeroValue)
	{
		// Return if size is zero, otherwise GetCursorWorldLocationFromMousePos crashes
		return false;
	}
	
	// Trace cursor to world-space
	const FViewportCursorLocation Cursor = ViewportClient->GetCursorWorldLocationFromMousePos();

	const FVector RayStart = Cursor.GetOrigin();
	const FVector RayEnd = RayStart + Cursor.GetDirection() * HALF_WORLD_MAX;

	FCollisionQueryParams Params;
	Params.bTraceComplex = true;
	if(IsValid(EditingCable))
	{
		// Ignore in progress cable, otherwise it will block hits under the cursor
		Params.AddIgnoredActor(EditingCable);
	}

	ViewportClient->GetWorld()->LineTraceSingleByChannel(OutHit, RayStart, RayEnd, ECollisionChannel::ECC_Visibility, Params);

	if(!OutHit.bBlockingHit)
	{
		// Fake result based on distance of previous hit
		OutHit.Location = RayStart + Cursor.GetDirection() * LastTraceResult.Distance;
		OutHit.Distance = FVector::Distance(RayStart, OutHit.Location);
	}

	LastTraceResult = OutHit;

	return OutHit.bBlockingHit;
}

ATetherCableActor* FTetherEdMode::CreateCableActor(FVector WorldLocation)
{
	ATetherCableActor* NewCable = GetWorld()->SpawnActor<ATetherCableActor>(WorldLocation, FRotator::ZeroRotator);

	NewCable->CableProperties = ModeSettings->CableProperties;

	// Update unique name of new Actor, otherwise the name is always incremented even though previous versions may have been removed
	FActorLabelUtilities::SetActorLabelUnique(NewCable, ATetherCableActor::StaticClass()->GetName());
	return NewCable;
}

void FTetherEdMode::CommitCableWithEndLocation(FEditorViewportClient* ViewportClient, FVector WorldLocation)
{
	if(!IsEditingTangentMouseDown(ViewportClient))
	{
		// Update end location (but not if we're editing a tangent)
		EditingCable->SetPointLocation(EditingPoint, WorldLocation);
	}
	CommitCurrentCable(ViewportClient);
}

void FTetherEdMode::CancelCable(FEditorViewportClient* ViewportClient)
{
	UE_LOG(LogTether, Log, TEXT("Cancel current cable"));
	EditingCable->Destroy(false, false);
	EditingCable = nullptr;
	UE_LOG(LogTether, Verbose, TEXT("Cancel transaction"));
	GEngine->CancelTransaction(0);
	// Notify selection change because the selected cable was destroyed
	GEditor->NoteSelectionChange(true);
	if(ViewportClient)
	{
		ViewportClient->Invalidate();
	}
}

void FTetherEdMode::SetTangent(FVector Tangent)
{
	UTetherGuideSplineComponent* SplineComp = GetEditingCable()->GetGuideSplineComponent();
	SplineComp->Modify();
	
	const int32 CurrentSegmentStartPoint = EditingPoint - 1;
	const int32 CurrentSegmentEndPoint = EditingPoint;

	FInterpCurvePointVector& EditedPoint = SplineComp->GetSplinePointsPosition().Points[CurrentSegmentEndPoint];
	FInterpCurvePointQuat& EditedRotPoint = SplineComp->GetSplinePointsRotation().Points[CurrentSegmentEndPoint];

	bool& bUseSplineTangents = GetEditingCable()->GetPointSegmentDefinitions()[CurrentSegmentEndPoint]->SimulationOptions.bUseSplineTangents;
	
	if (Tangent.Size() < 20.f)
	{
		UE_LOG(LogTether, VeryVerbose, TEXT("Clear tangents: %i"), CurrentSegmentEndPoint);
		Tangent = FVector::ZeroVector;
		GetEditingCable()->GetPointSegmentDefinitions()[CurrentSegmentEndPoint]->SimulationOptions.bUseSplineTangents = false;
		SplineComp->SetSplinePointType(CurrentSegmentEndPoint, ESplinePointType::Linear, true);
		EditedPoint.InterpMode = CIM_Linear;
		EditedPoint.LeaveTangent = FVector::ZeroVector;
		EditedPoint.ArriveTangent = FVector::ZeroVector;
		if(bUseSplineTangents)
		{
			bUseSplineTangents = false;
		}
	}
	else
	{
		SplineComp->SetSplinePointType(CurrentSegmentStartPoint, ESplinePointType::CurveCustomTangent, true);
		SplineComp->SetSplinePointType(CurrentSegmentEndPoint, ESplinePointType::Curve, true);

		if (!bUseSplineTangents)
		{
			UE_LOG(LogTether, Log, TEXT("Enabling spline tangents on point %i"), CurrentSegmentEndPoint);
			bUseSplineTangents = true;
		}

		// Set point tangent as user controlled
		EditedPoint.InterpMode = CIM_CurveUser;

		const FVector LocalTangent = SplineComp->GetComponentTransform().GetRotation().Inverse().RotateVector(Tangent);
		EditedPoint.LeaveTangent = LocalTangent;
		EditedPoint.ArriveTangent = LocalTangent;

		const FQuat WorldRot = Tangent.Rotation().Quaternion();

		// Spline rotation
		const FQuat LocalRot = SplineComp->GetComponentTransform().GetRotation().Inverse() * WorldRot; // convert world-space rotation to local-space
		EditedRotPoint.OutVal = LocalRot;
	}

	SplineComp->UpdateSpline();
	SplineComp->bSplineHasBeenEdited = true;

	GEditor->RedrawLevelEditingViewports(true);
}

FVector FTetherEdMode::ProjectHitLocationAlongNormal(FHitResult& Hit, float Distance) const
{
	return Hit.Location + Hit.ImpactNormal * Distance;
}

FVector FTetherEdMode::GetAdjustedHitLocation(FEditorViewportClient* ViewportClient, FHitResult& Hit) const
{
	if(IsEditingTangentNormalAligned(ViewportClient))
	{
		// Don't modify hit location if using surface-aligned tangents
		return Hit.Location;
	}
	
	return ProjectHitLocationAlongNormal(Hit, GetCableRadius());
}

float FTetherEdMode::GetCableRadius() const
{
	const FTetherCableProperties& Properties = GetEditingCable() ? GetEditingCable()->CableProperties : GetDefault<UTetherModeSettings>()->CableProperties;
	return Properties.GetCollisionWidth() * 0.5f;
}

bool FTetherEdMode::IsKeyDown(const FEditorViewportClient* ViewportClient, FKey Key) const
{
	return ViewportClient->Viewport->KeyState(Key);
}

bool FTetherEdMode::ShouldSuspendSimulation(const FEditorViewportClient* ViewportClient) const
{
	return IsKeyDown(ViewportClient, SuspendSimulationKey() );
}

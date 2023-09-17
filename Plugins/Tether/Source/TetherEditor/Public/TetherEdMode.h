// Copyright Sam Bonifacio 2021. All Rights Reserved.

#pragma once

#include "EdMode.h"

class ATetherCableActor;

class TETHEREDITOR_API FTetherEdMode : public FEdMode
{
public:

	const static FEditorModeID EM_Tether;

	FTetherEdMode();

	virtual ~FTetherEdMode() override;

	class UTetherModeSettings* GetModeSettings() const { return ModeSettings; }
	void SetModeSettings(UTetherModeSettings* Settings);

	bool IsEditing() const;

	void StartEditing(ATetherCableActor* Cable, int32 Point = -1, bool bTransactional = true);

	void StartNewSegment(FVector EndPointWorldLocation, bool bTransactional);

	void CancelSegmentAndCommitCable(FEditorViewportClient* ViewportClient);

	ATetherCableActor* GetEditingCable() const { return EditingCable; }

	bool IsEditingTangentMouseDown(const FEditorViewportClient* ViewportClient) const;
	bool IsEditingTangentNormalAligned(const FEditorViewportClient* ViewportClient) const;

	void SetStartCableModeActive(bool bActive = true);
	bool IsStartingCable() const;
	bool CableToolsActive() const;

	void CreateCableAtLocation(FVector WorldLocation);
	void CommitCurrentCable(FEditorViewportClient* ViewportClient = nullptr);

protected:

	virtual bool UsesToolkits() const override { return true; }
	virtual bool ShowModeWidgets() const override { return true; }

	virtual void Enter() override;
	virtual void Exit() override;

	virtual bool HandleClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click) override;
	virtual bool InputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event) override;
	virtual bool MouseMove(FEditorViewportClient* ViewportClient, FViewport* Viewport, int32 x, int32 y) override;

	virtual void Tick(FEditorViewportClient* ViewportClient, float DeltaTime) override;

	/** FGCObject interface */
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

	virtual bool IsSelectionAllowed(AActor* InActor, bool bInSelection) const override;

	bool ShouldSelectActor(AActor* InActor) const;

	// Note: Bind this function ourselves because FEdMode::SelectionChanged does not get called in all circumstances
	virtual void OnEditorSelectionChanged(UObject* Object);

private:
	
	class UTetherModeSettings* ModeSettings = nullptr;

	TSharedPtr<class STetherModeViewportOverlay> ViewportOverlay;

	bool bStartingCable = false;
	ATetherCableActor* EditingCable = nullptr;
	int32 EditingPoint = 0;
	
	FHitResult LastTraceResult;

	// Cached hit where the user started mouse drag
	FHitResult CachedMouseDownHit;

	bool HandleLeftMouseButtonDown(FEditorViewportClient* ViewportClient);
	bool HandleLeftMouseButtonUp(FEditorViewportClient* ViewportClient);

	bool IsEditingPointUsingTangents() const;

	bool TraceCursorToWorld(FEditorViewportClient* ViewportClient, FHitResult& OutHit);

	ATetherCableActor* CreateCableActor(FVector WorldLocation);

	void CommitCableWithEndLocation(FEditorViewportClient* ViewportClient, FVector WorldLocation);

	void CancelCable(FEditorViewportClient* ViewportClient);

	void SetTangent(FVector Tangent);

	FVector ProjectHitLocationAlongNormal(FHitResult& Hit, float Distance) const;

	FVector GetAdjustedHitLocation(FEditorViewportClient* ViewportClient, FHitResult& Hit) const;

	float GetCableRadius() const;

	bool IsKeyDown(const FEditorViewportClient* ViewportClient, FKey Key) const;

	bool ShouldSuspendSimulation(const FEditorViewportClient* ViewportClient) const;

	static FKey ContinueCableKey() { return EKeys::LeftShift; }
	static FKey FineAdjustSlackKey() { return EKeys::LeftShift; }
	static FKey NormalAlignedTangentKey() { return EKeys::LeftAlt; }
	static FKey SuspendSimulationKey() { return EKeys::LeftControl; }
};

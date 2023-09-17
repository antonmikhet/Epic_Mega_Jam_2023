// Copyright Sam Bonifacio 2021. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "IDetailCustomization.h"
#include "Layout/Visibility.h"

class ATetherCableActor;
class IDetailLayoutBuilder;

// Custom details for TetherCableActor
class FTetherCableActorDetails : public IDetailCustomization
{
public:
	
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

protected:

	/** IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;
	
private:

	TArray<TWeakObjectPtr<ATetherCableActor>> CachedSelectedCables;

	FReply Resimulate();
	FReply BuildMesh();
	FReply Cancel();
	FReply RealtimeSimulate();
	FReply StopRealtime();
	FReply ConvertToStaticMesh();

	bool IsInGameWorld() const;
	bool IsSelectedCableAsyncSimulating() const;
	bool IsSelectedCableBuildingMesh() const;
	bool CanSelectedCableManualBuild() const;
	bool IsSelectedCableMeshOutdated() const;
	bool IsSelectedCableRealtimeSimulating() const;
	bool IsSelectedCableSimulationOutdated() const;
	bool IsSelectedCableDynamicPreviewing() const;
	bool DoesSelectedCableNeedMoreSplinePoints() const;
	EVisibility GetThrobberVisibility() const { return IsSelectedCableAsyncSimulating() || IsSelectedCableBuildingMesh() ? EVisibility::Visible : EVisibility::Collapsed; }

	TArray<ATetherCableActor*> GetSelectedCables() const;
};

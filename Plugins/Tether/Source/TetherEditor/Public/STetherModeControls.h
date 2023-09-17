// Copyright Sam Bonifacio 2021. All Rights Reserved.

#pragma once

#include "IDetailRootObjectCustomization.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "CoreMinimal.h"
#include "Misc/EngineVersionComparison.h"

class ATetherCableActor;
class IDetailsView;

class FTetherModeSettingsRootObjectCustomization : public IDetailRootObjectCustomization
{
public:

#if UE_VERSION_OLDER_THAN(4,25,0)
	virtual TSharedPtr<SWidget> CustomizeObjectHeader(const UObject* InRootObject) override;
	virtual bool IsObjectVisible(const UObject* InRootObject) const override { return true; }
	virtual bool ShouldDisplayHeader(const UObject* InRootObject) const override { return false; }
#else
	virtual TSharedPtr<SWidget> CustomizeObjectHeader(const FDetailsObjectSet& InRootObjectSet) override;
	virtual bool AreObjectsVisible(const FDetailsObjectSet& InRootObjectSet) const override { return true; }
	virtual bool ShouldDisplayHeader(const FDetailsObjectSet& InRootObjectSet) const override { return false; }
#endif
};

class STetherModeControls : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(STetherModeControls) {}
	SLATE_END_ARGS()

	~STetherModeControls();

	void Construct(const FArguments& InArgs);

protected:

	TSharedPtr<IDetailsView> ModeControlsDetailView;
	TSharedPtr<IDetailsView> CablePropertiesDetailView;

	class FTetherEdMode* GetEditorMode() const;

private:

	bool CanEditCables() const { return false; }

	TArray<ATetherCableActor*> GetSelectedCables() const;

	void OnSelectionChanged(UObject* Object);

	void UpdateSelection();

	FReply StartNewCable();
	FReply Deselect();
	FReply Exit();
	
};

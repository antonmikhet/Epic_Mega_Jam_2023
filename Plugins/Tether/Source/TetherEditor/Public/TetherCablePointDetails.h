// Copyright Sam Bonifacio 2021. All Rights Reserved.

#pragma once

#include "IDetailCustomNodeBuilder.h"

class FTetherGuideSplineComponentVisualizer;
class ATetherCableActor;

class TETHEREDITOR_API FTetherCablePointDetails : public IDetailCustomNodeBuilder, public TSharedFromThis<FTetherCablePointDetails>
{
public:

	FTetherCablePointDetails(ATetherCableActor* InCable);

	virtual void Tick(float DeltaTime) override;
	virtual bool RequiresTick() const override { return true; }
	virtual void SetOnRebuildChildren(FSimpleDelegate InOnRegenerateChildren) override { OnRegenerateChildren = InOnRegenerateChildren; }
	virtual void GenerateHeaderRowContent(FDetailWidgetRow& NodeRow) override {}
	virtual void GenerateChildContent(IDetailChildrenBuilder& ChildrenBuilder) override;
	virtual bool InitiallyCollapsed() const override { return false; }
	virtual FName GetName() const override;
private:

	ATetherCableActor* Cable;
	FTetherGuideSplineComponentVisualizer* SplineVisualizer;

	FSimpleDelegate OnRegenerateChildren;

	TArray<int32> SelectedKeys;

	TArray< class UObject*> GetSelectedSegmentDefinitions() const;

	FText GetSelectedPointsText() const;

	class FReply SelectAllPoints();
};

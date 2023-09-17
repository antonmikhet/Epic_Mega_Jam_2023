// Copyright Sam Bonifacio 2021. All Rights Reserved.

#pragma once

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class IDetailsView;

class STetherModeViewportOverlay : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(STetherModeViewportOverlay) {}
	SLATE_END_ARGS()

	class FEditorViewportClient* ViewportClient = nullptr;

	STetherModeViewportOverlay();

	void Construct(const FArguments& InArgs);

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

protected:

	TSharedPtr<IDetailsView> DetailsPanel;

	class FTetherEdMode* GetEditorMode() const;

};

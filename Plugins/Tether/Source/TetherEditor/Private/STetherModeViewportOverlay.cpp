// Copyright Sam Bonifacio 2021. All Rights Reserved.

#include "STetherModeViewportOverlay.h"
#include "Editor.h"
#include "EditorModeManager.h"
#include "TetherEdMode.h"
#include "TetherPromptText.h"

STetherModeViewportOverlay::STetherModeViewportOverlay()
{
}

void STetherModeViewportOverlay::Construct(const FArguments& InArgs)
{
	SetVisibility(EVisibility::HitTestInvisible);
	ChildSlot
	.VAlign(VAlign_Bottom)
	.HAlign(HAlign_Center)
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.Padding(10)
		[
			SNew(STextBlock)
			.TextStyle(FEditorStyle::Get(), "FoliageEditMode.ActiveToolName.Text")
			.Justification(ETextJustify::Center)
			.Text_Lambda([this]() -> FText
			{
				return FTetherPromptText::GetCurrentPromptText(GetEditorMode(), ViewportClient);
			})
		]
	];
}

int32 STetherModeViewportOverlay::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
	const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	int32 Result = SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	return Result;
}

FTetherEdMode* STetherModeViewportOverlay::GetEditorMode() const
{
	return (FTetherEdMode*)GLevelEditorModeTools().GetActiveMode(FTetherEdMode::EM_Tether);
}

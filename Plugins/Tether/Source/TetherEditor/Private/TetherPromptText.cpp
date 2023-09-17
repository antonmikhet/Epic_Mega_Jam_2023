// Copyright Sam Bonifacio 2021. All Rights Reserved.

#include "TetherPromptText.h"
#include "TetherEdMode.h"

FText FTetherPromptText::GetCurrentPromptText(const FTetherEdMode* EdMode, const FEditorViewportClient* ViewportClient)
{
	if(!EdMode->CableToolsActive())
	{
		return INVTEXT("Start a cable, or enable Auto Start Cable");
	}
	
	if (!EdMode->IsEditing())
	{
		return INVTEXT("Click to start placing a new cable\nClick and drag to control tangents");
	}
	
	if (EdMode->IsEditingTangentMouseDown(ViewportClient))
	{
		return INVTEXT("Release to commit point with tangent\nHold Alt to align tangent to surface normal\nHold Shift and release to start new segment");
	}
	else
	{
		return INVTEXT("Click to finish cable, Shift-click to start new segment\nClick and drag to control tangents\nMouse wheel to adjust slack");
	}
}

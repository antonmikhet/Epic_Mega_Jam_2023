// Copyright Sam Bonifacio 2021. All Rights Reserved.

#include "TetherModeToolkit.h"
#include "STetherModeControls.h"
#include "TetherEdMode.h"
#include "EditorModeManager.h"

void FTetherModeToolkit::Init(const TSharedPtr<class IToolkitHost>& InitToolkitHost)
{
	FModeToolkit::Init(InitToolkitHost);
}

FName FTetherModeToolkit::GetToolkitFName() const
{
	return FName("TetherMode");
}

FText FTetherModeToolkit::GetBaseToolkitName() const
{
	return NSLOCTEXT("Tether", "ToolkitName", "Tether Mode");
}

FEdMode* FTetherModeToolkit::GetEditorMode() const
{
	return GLevelEditorModeTools().GetActiveMode(FTetherEdMode::EM_Tether);
}

TSharedPtr<class SWidget> FTetherModeToolkit::GetInlineContent() const
{
	return SNew(STetherModeControls);
}

// Copyright Sam Bonifacio 2021. All Rights Reserved.

#include "TetherEditorStatics.h"

#include "EditorModeManager.h"
#include "TetherEdMode.h"
#include "Engine/Selection.h"

TArray<ATetherCableActor*> FTetherEditorStatics::GetSelectedCables()
{
    TArray<ATetherCableActor*> Cables;
    GEditor->GetSelectedActors()->GetSelectedObjects<ATetherCableActor>(Cables);
    return Cables;
}

FTetherEdMode* FTetherEditorStatics::ActivateTetherMode()
{
	if (!GLevelEditorModeTools().IsModeActive(FTetherEdMode::EM_Tether))
	{
		GLevelEditorModeTools().ActivateMode(FTetherEdMode::EM_Tether);
	}
	FTetherEdMode* EdMode = GLevelEditorModeTools().GetActiveModeTyped<FTetherEdMode>(FTetherEdMode::EM_Tether);
	return EdMode;
}

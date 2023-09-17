// Copyright Sam Bonifacio 2021. All Rights Reserved.

#pragma once

#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "TetherModeSettings.h"

class FTetherEdMode;

/**
 * Editor automation utilities
 */
class TETHEREDITOR_API FTetherEditorAutomation
{
public:

    static FString GetTempPath(const FAutomationTestBase* Test);
	static void ClearTemp(const FAutomationTestBase* Test);
	static void LoadEntry();
	static UWorld* CreateMap(const FAutomationTestBase* Test, FString Name = TEXT("NewMap"), bool bSave = true);
	static UTetherModeSettings* CreateModeSettings();
	static FTetherEdMode* ActivateMode();
	static class ATetherCableActor* CreateCable(UWorld* World, uint8 Points = 2, bool bEditing = false);
	static class ATetherCableActor* FindCable(UWorld* World);
	static void PrepareCableForMapClose(const ATetherCableActor* Cable);
	static void EndPlay();
	static void TickDirectoryWatcher();

};

#endif
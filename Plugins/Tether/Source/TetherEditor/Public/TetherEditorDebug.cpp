// Copyright Sam Bonifacio 2021. All Rights Reserved.

#include "TetherEditorDebug.h"
#include "TetherCableActor.h"
#include "TetherEditorStatics.h"
#include "TetherEditorLogs.h"

static FAutoConsoleCommand DumpSelectedCableInfoCommand(
    TEXT("Tether.DumpSelectedCableInfo"),
    TEXT("Dumps info about the selected Tether Cable Actor to the output log"),
    FConsoleCommandDelegate::CreateStatic([]()
    {
        const TArray<ATetherCableActor*> SelectedCables = FTetherEditorStatics::GetSelectedCables();
        UE_LOG(LogTether, Log, TEXT("Dumping info for %i cable(s)"), SelectedCables.Num());
        for (ATetherCableActor* Cable : SelectedCables)
        {
            Cable->DumpCableInfo();
        }
    }),
    ECVF_Default);

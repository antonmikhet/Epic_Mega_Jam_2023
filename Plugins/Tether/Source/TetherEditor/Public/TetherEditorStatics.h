// Copyright Sam Bonifacio 2021. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TetherCableActor.h"

class FTetherEdMode;

class FTetherEditorStatics
{
public:

    static TArray<ATetherCableActor*> GetSelectedCables();

	static FTetherEdMode* ActivateTetherMode();
};

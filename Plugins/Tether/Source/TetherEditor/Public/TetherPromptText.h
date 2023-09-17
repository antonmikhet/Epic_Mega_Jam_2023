// Copyright Sam Bonifacio 2021. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"

class FTetherEdMode;
class FEditorViewportClient;

class TETHEREDITOR_API FTetherPromptText
{

public:

	static FText GetCurrentPromptText(const FTetherEdMode* EdMode, const FEditorViewportClient* ViewportClient);

};

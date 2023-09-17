// Copyright Sam Bonifacio 2021. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TetherModeSettings.h"
#include "TetherTestModeSettings.generated.h"

/**
 * Mode settings subclass for testing which ignores config
 */
UCLASS(Config=TetherTestModeSettings) // This unfortunately doesn't stop the subclass using config, but it will prevent loading the normal one at least, as this ini file won't exist
class TETHEREDITOR_API UTetherTestModeSettings : public UTetherModeSettings
{
	GENERATED_BODY()

protected:
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
};

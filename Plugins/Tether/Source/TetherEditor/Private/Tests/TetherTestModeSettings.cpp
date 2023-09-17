// Copyright Sam Bonifacio 2021. All Rights Reserved.


#include "Tests/TetherTestModeSettings.h"

void UTetherTestModeSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// This should never happen on the test settings
	check(0);
}

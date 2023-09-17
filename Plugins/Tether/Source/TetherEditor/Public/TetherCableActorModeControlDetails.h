// Copyright Sam Bonifacio 2021. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "TetherCableActorDetails.h"

class IDetailLayoutBuilder;

// Special custom TetherCableActor details subclass for the mode controls panel
class FTetherCableActorModeControlDetails : public FTetherCableActorDetails
{
public:

	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

private:

	/** IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;
};

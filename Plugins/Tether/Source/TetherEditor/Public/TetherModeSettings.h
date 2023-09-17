// Copyright Sam Bonifacio 2021. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TetherCableActorDetails.h"
#include "Tether/Public/TetherCableProperties.h"
#include "UObject/NoExportTypes.h"
#include "TetherModeSettings.generated.h"

class FTetherModeControlDetails : public IDetailCustomization
{
public:

	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

private:

	/** IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;
};


// Special custom TetherCableActor details subclass for the mode controls panel
class FTetherModeSettingsCablePropertiesDetails : public FTetherCableActorDetails
{
public:

	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

private:

	/** IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;
};


/**
 * Class containing editor mode settings
 */
UCLASS(config = TetherModeSettings, PerObjectConfig)
class TETHEREDITOR_API UTetherModeSettings : public UObject
{
	GENERATED_BODY()

public:

	UTetherModeSettings();

	/**
	 * If true, clicking in the viewport when in the Tether mode will start a new cable
	 * If false, you'll be able to select other Actors as usual, but will have to click Start New Cable to create cables
	 */
	UPROPERTY(Category = "ModeSettings", EditAnywhere, Config)
	bool bAutoStartCable;

	/**
	 * Default properties for newly created cables
	 */
	UPROPERTY(Category = "TetherProperties", EditAnywhere, Config)
	FTetherCableProperties CableProperties;

protected:
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	virtual void OverridePerObjectConfigSection(FString& SectionName) override;
	
};

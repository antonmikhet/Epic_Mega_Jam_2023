// Copyright Sam Bonifacio 2021. All Rights Reserved.


#include "TetherModeSettings.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "TetherCableActor.h"
#include "MaterialList.h"
#include "Misc/ConfigCacheIni.h"

TSharedRef<IDetailCustomization> FTetherModeControlDetails::MakeInstance()
{
	return MakeShareable(new FTetherModeControlDetails);
}

void FTetherModeControlDetails::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	TArray<FName> Categories;
	DetailLayout.GetCategoryNames(Categories);
	for (FName Category : Categories)
	{
		if (Category != "ModeSettings")
		{
			DetailLayout.HideCategory(Category);
		}
	}
}

TSharedRef<IDetailCustomization> FTetherModeSettingsCablePropertiesDetails::MakeInstance()
{
	return MakeShareable(new FTetherModeSettingsCablePropertiesDetails);
}

void FTetherModeSettingsCablePropertiesDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<FName> Categories;
	DetailBuilder.GetCategoryNames(Categories);
	for (FName Category : Categories)
	{
		if (Category != "TetherProperties")
		{
			DetailBuilder.HideCategory(Category);
		}
	}
}

UTetherModeSettings::UTetherModeSettings()
	: bAutoStartCable(true)
{
}

void UTetherModeSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Save with a new Config Cache because using GConfig will cause SaveConfig to copy the values to the CDO
	// We don't want to copy the values to the CDO because then the real default values are lost and we lose the "revert to default" buttons on each property
	FConfigCacheIni Config(EConfigCacheType::DiskBacked);
	SaveConfig(CPF_Config, nullptr, &Config);
	Config.Flush(false);

}

void UTetherModeSettings::OverridePerObjectConfigSection(FString& SectionName)
{
	SectionName = "TetherModeSettings";
}

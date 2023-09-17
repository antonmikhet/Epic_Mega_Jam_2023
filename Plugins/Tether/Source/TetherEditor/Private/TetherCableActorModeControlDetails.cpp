// Copyright Sam Bonifacio 2021. All Rights Reserved.

#include "TetherCableActorModeControlDetails.h"
#include "DetailLayoutBuilder.h"
#include "TetherCableActor.h"

TSharedRef<IDetailCustomization> FTetherCableActorModeControlDetails::MakeInstance()
{
	return MakeShareable(new FTetherCableActorModeControlDetails);
}

void FTetherCableActorModeControlDetails::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	FTetherCableActorDetails::CustomizeDetails(DetailLayout);

	// Hide transform category
	DetailLayout.HideCategory("TransformCommon");

	TArray<FName> Categories;
	DetailLayout.GetCategoryNames(Categories);
	for(FName Category : Categories)
	{
		if(Category != "TetherProperties")
		{
			DetailLayout.HideCategory(Category);
		}
	}

	DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(ATetherCableActor, bLockCurrentState)));
}

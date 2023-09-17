// Copyright Sam Bonifacio 2021. All Rights Reserved.

#include "TetherCablePointDetails.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "TetherCableActor.h"
#include "TetherGuideSplineComponentVisualizer.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Input/SButton.h"

FTetherCablePointDetails::FTetherCablePointDetails(ATetherCableActor* InCable)
{
	const TSharedPtr<FComponentVisualizer> Visualizer = GUnrealEd->FindComponentVisualizer(UTetherGuideSplineComponent::StaticClass());
	SplineVisualizer = (FTetherGuideSplineComponentVisualizer*)Visualizer.Get();
	check(SplineVisualizer);
	
	Cable = InCable;
}

void FTetherCablePointDetails::Tick(float DeltaTime)
{
	TSet<int32> KeysSet = SplineVisualizer->GetSelectedKeys();
	TArray<int32> KeysArray;
	for(int32 Key : KeysSet)
	{
		KeysArray.Add(Key);
	}

	if(SelectedKeys != KeysArray)
	{
		SelectedKeys = KeysArray;
		OnRegenerateChildren.ExecuteIfBound();
	}
}

void FTetherCablePointDetails::GenerateChildContent(IDetailChildrenBuilder& ChildrenBuilder)
{
	if(SelectedKeys.Num() == 0)
	{
		ChildrenBuilder.AddCustomRow(INVTEXT("None selected"))
		.ValueContent()
		.MaxDesiredWidth(200.f)
		.MinDesiredWidth(200.f)
			[
				SNew(SBox)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(INVTEXT("No cable points are selected."))
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
			];
	}
	else
	{
		ChildrenBuilder.AddCustomRow(INVTEXT("Selected points"))
		.ValueContent()
		.MaxDesiredWidth(200.f)
		.MinDesiredWidth(200.f)
			[
				SNew(SBox)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(this, &FTetherCablePointDetails::GetSelectedPointsText)
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
			];
	}

	ChildrenBuilder.AddCustomRow(INVTEXT("Select All"))
	.ValueContent()
	.MaxDesiredWidth(200.f)
	.MinDesiredWidth(200.f)
	[
		SNew(SButton)
		.ContentPadding(2)
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		.OnClicked(this, &FTetherCablePointDetails::SelectAllPoints)
		[
			SNew(STextBlock)
			.Text(INVTEXT("Select All"))
		]
	];
	
	for (TFieldIterator<FProperty> PropIt(UTetherPointSegmentDefinition::StaticClass()); PropIt; ++PropIt)
	{
		FProperty* Property = *PropIt;
		ChildrenBuilder.AddExternalObjectProperty(GetSelectedSegmentDefinitions(), Property->GetFName(), FAddPropertyParams());
	}
}

FName FTetherCablePointDetails::GetName() const
{
	static const FName Name("TetherCablePointDetails");
	return Name;
}

TArray<UObject*> FTetherCablePointDetails::GetSelectedSegmentDefinitions() const
{
	TArray<UObject*> Result;
	for(int32 Key : SelectedKeys)
	{
		UTetherPointSegmentDefinition* Def = Cable->GetPointSegmentDefinitions().IsValidIndex(Key) ? Cable->GetPointSegmentDefinitions()[Key] : nullptr;
		if(IsValid(Def))
		{
			Result.Add(Def);
		}
	}
	return Result;
}

FText FTetherCablePointDetails::GetSelectedPointsText() const
{
	// Create string like e.g {0, 1, 4}
	FString PointsString = "{";
	for(int32 i = 0; i< SelectedKeys.Num(); i++)
	{
		const int32 Key = SelectedKeys[i];
		PointsString.Append(FString::FromInt(Key));
		if(i < SelectedKeys.Num() - 1)
		{
			PointsString.Append(", ");
		}
	}
	PointsString.Append("}");
	FText Result = FText::Format(INVTEXT("Selected points: {0}"), FText::FromString( PointsString));
	return Result;
}

FReply FTetherCablePointDetails::SelectAllPoints()
{
	const TSharedPtr<FComponentVisualizer> Visualizer = GUnrealEd->FindComponentVisualizer(UTetherGuideSplineComponent::StaticClass());
	SplineVisualizer = (FTetherGuideSplineComponentVisualizer*)Visualizer.Get();
	check(SplineVisualizer);

	SplineVisualizer->SetEditedComponent(Cable->GetGuideSplineComponent());
	SplineVisualizer->SelectAllPoints();
	return FReply::Handled();
}

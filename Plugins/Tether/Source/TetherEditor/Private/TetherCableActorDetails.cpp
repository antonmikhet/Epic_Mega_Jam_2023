// Copyright Sam Bonifacio 2021. All Rights Reserved.

#include "TetherCableActorDetails.h"

#include "CablePropertiesMaterialList.h"
#include "DetailCategoryBuilder.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "MaterialList.h"
#include "TetherCableActor.h"
#include "TetherCablePointDetails.h"
#include "TetherDetailUtils.h"
#include "Misc/ScopedSlowTask.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "TetherEditorLogs.h"
#include "Misc/EngineVersionComparison.h"
#include "Framework/Docking/TabManager.h"

TSharedRef<IDetailCustomization> FTetherCableActorDetails::MakeInstance()
{
	return MakeShared<FTetherCableActorDetails>();
}

void FTetherCableActorDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray< TWeakObjectPtr<UObject> > ObjectsBeingCustomized;
	DetailBuilder.GetObjectsBeingCustomized(ObjectsBeingCustomized);

	for (TWeakObjectPtr<UObject>& Object : ObjectsBeingCustomized)
	{
		ATetherCableActor* Cable = Cast<ATetherCableActor>(Object.Get());
		if (ensure(Cable))
		{
			CachedSelectedCables.Add(Cable);
		}
	}
	
	DetailBuilder.EditCategory("Tether", FText::GetEmpty(), ECategoryPriority::Important);

	DetailBuilder.EditCategory("Tether")
		.AddCustomRow(INVTEXT("Status"))
		.ValueContent()
		.MaxDesiredWidth(200.f)
		.MinDesiredWidth(200.f)
		[
			SNew(SBox)
			.HAlign(HAlign_Center)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Center)
				.AutoWidth()
				.Padding(0.0f, 0.0f, 3.0f, 0.0f)
				[
					SNew(SBox)
					.WidthOverride(20)
					.HeightOverride(20)
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					.Visibility(this, &FTetherCableActorDetails::GetThrobberVisibility)
					[
						SNew(SCircularThrobber)
					]
				]
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Center)
				.AutoWidth()
				[
					SNew(SBox)
					.HeightOverride(20)
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					[
						SNew(STextBlock)
						.Font(IDetailLayoutBuilder::GetDetailFont())
						.Text_Lambda([this]()
						{
							if(DoesSelectedCableNeedMoreSplinePoints())
							{
								return INVTEXT("Not enough spline points");
							}
							if (IsSelectedCableSimulationOutdated())
							{
								return INVTEXT("Outdated - needs simulation");
							}
							if (IsSelectedCableRealtimeSimulating())
							{
								return INVTEXT("Simulating in realtime");
							}
							if(IsSelectedCableAsyncSimulating())
							{
								return INVTEXT("Simulating...");
							}
							if(IsSelectedCableBuildingMesh())
							{
								return INVTEXT("Building mesh...");
							}
							if(IsSelectedCableDynamicPreviewing())
							{
								return INVTEXT("Previewing - mesh not built");
							}
							return INVTEXT("Up to date!");
						})
						.ColorAndOpacity_Lambda([this]()
						{
							if (DoesSelectedCableNeedMoreSplinePoints() || IsSelectedCableSimulationOutdated())
							{
								return FSlateColor(FLinearColor(FColor::Orange));
							}
							if(IsSelectedCableRealtimeSimulating() || IsSelectedCableAsyncSimulating() || IsSelectedCableDynamicPreviewing())
							{
								return FSlateColor(FLinearColor::Yellow);
							}
							return FSlateColor(FLinearColor::White);
						})
					]
				]
			]
		];
	
	DetailBuilder.EditCategory("Tether")
		.AddCustomRow(INVTEXT("Resimulate"))
		.ValueContent()
		.MaxDesiredWidth(200.f)
		.MinDesiredWidth(200.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			[
				SNew(SButton)
				.ContentPadding(2)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				.OnClicked(this, &FTetherCableActorDetails::Resimulate)
				.Visibility_Lambda([this]() { return !IsSelectedCableAsyncSimulating() ? EVisibility::Visible : EVisibility::Collapsed; })
				.IsEnabled_Lambda([this]() { return !IsSelectedCableRealtimeSimulating() && !IsSelectedCableAsyncSimulating() && !IsInGameWorld() && !DoesSelectedCableNeedMoreSplinePoints(); })
				.ToolTipText(INVTEXT("Resimulate the selected cables. The original order of simulation is preserved so that the result is the same."))
				[
					SNew(STextBlock)
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.Text(INVTEXT("Resimulate"))
				]
			]
			+ SVerticalBox::Slot()
			[
				SNew(SButton)
				.ContentPadding(2)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				.OnClicked(this, &FTetherCableActorDetails::Cancel)
				.IsEnabled_Lambda([this]() { return !IsInGameWorld(); })
				.Visibility_Lambda([this]() { return !IsSelectedCableRealtimeSimulating() && IsSelectedCableAsyncSimulating() ? EVisibility::Visible : EVisibility::Collapsed; })
				[
					SNew(STextBlock)
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.Text(INVTEXT("Cancel"))
				]
			]
			+ SVerticalBox::Slot()
			[
				SNew(SButton)
				.ContentPadding(2)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				.OnClicked(this, &FTetherCableActorDetails::BuildMesh)
				.Visibility_Lambda([this]() { return CanSelectedCableManualBuild() ? EVisibility::Visible : EVisibility::Collapsed; })
				.IsEnabled_Lambda([this]() { return IsSelectedCableMeshOutdated() && !IsInGameWorld(); })
				.ToolTipText(INVTEXT("Build meshes for the selected cables"))
				[
					SNew(STextBlock)
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.Text(INVTEXT("Build Mesh"))
				]
			]
		];

	DetailBuilder.EditCategory("Tether", FText::GetEmpty())
		.AddCustomRow(INVTEXT("Convert to Static Mesh Actor"))
		.ValueContent()
		.MaxDesiredWidth(200.f)
		.MinDesiredWidth(200.f)
		[
			SNew(SButton)
			.ContentPadding(2)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.OnClicked(this, &FTetherCableActorDetails::ConvertToStaticMesh)
			.IsEnabled_Lambda([this]() { return !IsInGameWorld(); })
			.ToolTipText(INVTEXT("Open the Merge Actors window to convert the selected cables to a Static Mesh Actor. Note: Cables are automatically baked to meshes automatically, so this is not required under most circumstances."))
			[
				SNew(STextBlock)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.Text(INVTEXT("Convert to Static Mesh Actor"))
			]
		];

	if (ObjectsBeingCustomized.Num() == 1)
	{
		if (ATetherCableActor* Cable = Cast<ATetherCableActor>(ObjectsBeingCustomized[0]))
		{
			IDetailCategoryBuilder& Category = DetailBuilder.EditCategory("SelectedPoint", FText::GetEmpty(), ECategoryPriority::Important);
			const TSharedRef<FTetherCablePointDetails> SplinePointDetails = MakeShareable(new FTetherCablePointDetails(Cable));
			Category.AddCustomBuilder(SplinePointDetails);
		}
	}
	
	DetailBuilder.EditCategory("TetherProperties", FText::GetEmpty(), ECategoryPriority::TypeSpecific);

	DetailBuilder.EditCategory("TetherDebug", FText::GetEmpty(), ECategoryPriority::TypeSpecific)
		.AddCustomRow(INVTEXT("Realtime Simulate"))
		.ValueContent()
		.MaxDesiredWidth(200.f)
		.MinDesiredWidth(200.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			[
				SNew(SButton)
				.ContentPadding(2)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				.OnClicked(this, &FTetherCableActorDetails::RealtimeSimulate)
				.Visibility_Lambda([this]() {return !IsSelectedCableRealtimeSimulating() ? EVisibility::Visible : EVisibility::Collapsed; })
				.IsEnabled_Lambda([this]() {return !IsInGameWorld() && GetSelectedCables().Num() < 2; })
				.ToolTipText(INVTEXT("Start simulating in realtime \n"\
					"Useful for debugging, to see what's actually happening during the simulation \n"\
					"Can cause a significant performance hit depending on simulation options \n"\
					"Only one cable can be realtime simulated at a time"))
				[
					SNew(STextBlock)
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.Text(INVTEXT("Simulate in real-time"))
				]
			]
			+ SVerticalBox::Slot()
			[
				SNew(SButton)
				.ContentPadding(2)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				.OnClicked(this, &FTetherCableActorDetails::StopRealtime)
				.Visibility_Lambda([this]() {return IsSelectedCableRealtimeSimulating() ? EVisibility::Visible : EVisibility::Collapsed; })
				.IsEnabled_Lambda([this]() {return !IsInGameWorld(); })
				.ToolTipText(INVTEXT("Stop the realtime simulation"))
				[
					SNew(STextBlock)
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.Text(INVTEXT("Stop Realtime"))
				]
			]
		];

}

FReply FTetherCableActorDetails::Resimulate()
{
	TArray<ATetherCableActor*> SelectedCables = GetSelectedCables();

	Algo::Sort(SelectedCables, [](const ATetherCableActor* First, const ATetherCableActor* Second)
	{
		const bool bFirstBeforeSecond = First->ShouldSimulateBefore(Second);
		const bool bSecondBeforeFirst = Second->ShouldSimulateBefore(First);

		ensure(bFirstBeforeSecond != bSecondBeforeFirst);
		return bFirstBeforeSecond;
	});

	// If there is more than one cable, simulate them synchronously to preserve dependency
	const bool bSynchronous = SelectedCables.Num() > 1;

	FScopedSlowTask SlowTask(SelectedCables.Num(), INVTEXT("Resimulating Cables"));
	SlowTask.MakeDialog();
	for(ATetherCableActor* Cable : SelectedCables)
	{
		SlowTask.EnterProgressFrame(1, FText::Format(INVTEXT("Simulating {0}"), FText::FromString(Cable->GetHumanReadableName())));
		UE_LOG(LogTether, Log, TEXT("----- Start rebuild of %s, bSynchronous: %i -----"), *Cable->GetName(), (int32)bSynchronous);
		Cable->InvalidateAndResimulate(bSynchronous, AlwaysBuild); // Synchronous rebuild to enforce ordering
		// After calling the above, logically one of the following must now be true
		ensure(!Cable->IsDynamicPreview() || Cable->IsBuildingMesh() || Cable->IsMeshOutdated() || !Cable->SimulationModelHasAnyParticles());
	}

	return FReply::Handled();
}

FReply FTetherCableActorDetails::BuildMesh()
{
	TArray<ATetherCableActor*> SelectedCables = GetSelectedCables();

	for (ATetherCableActor* Cable : SelectedCables)
	{
		if (Cable->IsMeshOutdated())
		{
			Cable->BuildMesh();
		}
	}

	return FReply::Handled();
}


FReply FTetherCableActorDetails::Cancel()
{
	TArray<ATetherCableActor*> SelectedCables = GetSelectedCables();

	for (ATetherCableActor* Cable : SelectedCables)
	{
		if (Cable->IsRunningAsyncSimulation())
		{
			Cable->CancelAsyncSimulation();
		}
	}

	return FReply::Handled();
}

FReply FTetherCableActorDetails::RealtimeSimulate()
{
	TArray<ATetherCableActor*> SelectedCables = GetSelectedCables();

	// Only allow this on a single cable

	if(SelectedCables.Num() > 1)
	{
		return FReply::Handled();
	}

	for(ATetherCableActor* Cable : SelectedCables)
	{
		Cable->SetRealtimeSimulating(true);
	}

	return FReply::Handled();
}

FReply FTetherCableActorDetails::StopRealtime()
{
	TArray<ATetherCableActor*> SelectedCables = GetSelectedCables();

	for (ATetherCableActor* Cable : SelectedCables)
	{
		if(Cable->IsRealtimeSimulating())
		{
			Cable->SetRealtimeSimulating(false);
		}
	}

	return FReply::Handled();
}

FReply FTetherCableActorDetails::ConvertToStaticMesh()
{
	// For now just invoke merge actors window
	// Could streamline this in the future

#if UE_VERSION_OLDER_THAN(4,26,0)
	FGlobalTabmanager::Get().Get().InvokeTab(FTabId("MergeActors"));
#else
	FGlobalTabmanager::Get().Get().TryInvokeTab(FTabId("MergeActors"));
#endif
	
	return FReply::Handled();
}

bool FTetherCableActorDetails::IsInGameWorld() const
{
	TArray<ATetherCableActor*> SelectedCables = GetSelectedCables();

	for (ATetherCableActor* Cable : SelectedCables)
	{
		if (Cable->GetWorld()->IsGameWorld())
		{
			return true;
		}
	}

	return false;
}

bool FTetherCableActorDetails::IsSelectedCableAsyncSimulating() const
{
	TArray<ATetherCableActor*> SelectedCables = GetSelectedCables();
	for(ATetherCableActor* Cable : SelectedCables)
	{
		if(IsValid(Cable) && Cable->IsRunningAsyncSimulation())
		{
			return true;
		}
	}

	return false;
}

bool FTetherCableActorDetails::IsSelectedCableBuildingMesh() const
{
	TArray<ATetherCableActor*> SelectedCables = GetSelectedCables();
	for (ATetherCableActor* Cable : SelectedCables)
	{
		if (IsValid(Cable) && Cable->IsBuildingMesh())
		{
			return true;
		}
	}

	return false;
}

bool FTetherCableActorDetails::CanSelectedCableManualBuild() const
{
	TArray<ATetherCableActor*> SelectedCables = GetSelectedCables();
	for (ATetherCableActor* Cable : SelectedCables)
	{
		if (IsValid(Cable) && !Cable->CanAutoBuild())
		{
			return true;
		}
	}

	return false;
}

bool FTetherCableActorDetails::IsSelectedCableMeshOutdated() const
{
	TArray<ATetherCableActor*> SelectedCables = GetSelectedCables();
	for (ATetherCableActor* Cable : SelectedCables)
	{
		if (IsValid(Cable) && Cable->IsMeshOutdated() && !Cable->IsBuildingMesh())
		{
			return true;
		}
	}

	return false;
}

bool FTetherCableActorDetails::IsSelectedCableRealtimeSimulating() const
{
	TArray<ATetherCableActor*> SelectedCables = GetSelectedCables();
	for (ATetherCableActor* Cable : SelectedCables)
	{
		if (IsValid(Cable) && Cable->IsRealtimeSimulating())
		{
			return true;
		}
	}

	return false;
}

bool FTetherCableActorDetails::IsSelectedCableSimulationOutdated() const
{
	TArray<ATetherCableActor*> SelectedCables = GetSelectedCables();
	for (ATetherCableActor* Cable : SelectedCables)
	{
		if (IsValid(Cable) && Cable->IsSimulationOutdated())
		{
			return true;
		}
	}

	return false;
}

bool FTetherCableActorDetails::IsSelectedCableDynamicPreviewing() const
{
	TArray<ATetherCableActor*> SelectedCables = GetSelectedCables();
	for (ATetherCableActor* Cable : SelectedCables)
	{
		if (IsValid(Cable) && Cable->IsDynamicPreview())
		{
			return true;
		}
	}

	return false;
}

bool FTetherCableActorDetails::DoesSelectedCableNeedMoreSplinePoints() const
{
	TArray<ATetherCableActor*> SelectedCables = GetSelectedCables();
	for (ATetherCableActor* Cable : SelectedCables)
	{
		if (IsValid(Cable) && Cable->GetNumGuideSplinePoints() < 2)
		{
			return true;
		}
	}

	return false;
}

TArray<ATetherCableActor*> FTetherCableActorDetails::GetSelectedCables() const
{
	TArray<ATetherCableActor*> Result;
	for(auto Cable : CachedSelectedCables)
	{
		Result.Add(Cable.Get());
	}
	return Result;
}

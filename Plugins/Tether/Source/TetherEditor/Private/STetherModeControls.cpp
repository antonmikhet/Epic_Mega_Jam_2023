// Copyright Sam Bonifacio 2021. All Rights Reserved.

#include "STetherModeControls.h"
#include "DetailLayoutBuilder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "PropertyEditorModule.h"
#include "EditorModeManager.h"
#include "LevelEditor.h"
#include "SLevelViewport.h"
#include "TetherEdMode.h"
#include "TetherModeSettings.h"
#include "Engine/Selection.h"
#include "TetherCableActor.h"
#include "TetherCableActorModeControlDetails.h"
#include "TetherEditorStatics.h"
#include "TetherPromptText.h"
#include "Templates/SharedPointer.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Input/SButton.h"
#include "EditorFontGlyphs.h"
#include "EditorStyleSet.h"

#if UE_VERSION_OLDER_THAN(4,25,0)
TSharedPtr<SWidget> FTetherModeSettingsRootObjectCustomization::CustomizeObjectHeader(const UObject* InRootObject)
#else
TSharedPtr<SWidget> FTetherModeSettingsRootObjectCustomization::CustomizeObjectHeader(const FDetailsObjectSet& InRootObjectSet)
#endif
{
	return SNullWidget::NullWidget;
}

STetherModeControls::~STetherModeControls()
{
	USelection::SelectionChangedEvent.RemoveAll(this);
}

const FEditorViewportClient* GetViewportClient()
{
	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	return LevelEditorModule.GetFirstActiveLevelViewport()->GetViewportClient().Get();
}

void STetherModeControls::Construct(const FArguments& InArgs)
{
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
#if UE_VERSION_OLDER_THAN(5,0,0)
	FDetailsViewArgs DetailsViewArgs(false, false, false, FDetailsViewArgs::HideNameArea);
#else
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bAllowSearch = false;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
#endif
	DetailsViewArgs.bShowScrollBar = false;

	ModeControlsDetailView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	ModeControlsDetailView->RegisterInstancedCustomPropertyLayout(UTetherModeSettings::StaticClass(), FOnGetDetailCustomizationInstance::CreateLambda([this]()
	{
		return FTetherModeControlDetails::MakeInstance();
	}));

	ensure(GetEditorMode()->GetModeSettings());
	ModeControlsDetailView->SetObject(GetEditorMode()->GetModeSettings(), true);
	
	CablePropertiesDetailView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	CablePropertiesDetailView->RegisterInstancedCustomPropertyLayout(ATetherCableActor::StaticClass(), FOnGetDetailCustomizationInstance::CreateLambda([this]()
	{
		return FTetherCableActorModeControlDetails::MakeInstance();
	}));
	CablePropertiesDetailView->RegisterInstancedCustomPropertyLayout(UTetherModeSettings::StaticClass(), FOnGetDetailCustomizationInstance::CreateLambda([this]()
	{
		return FTetherModeSettingsCablePropertiesDetails::MakeInstance();
	}));

	// If we can't edit cables, can only edit default cable properties
	// Mark read-only if appropriate
	CablePropertiesDetailView->SetIsPropertyReadOnlyDelegate(FIsPropertyReadOnly::CreateLambda([this](const FPropertyAndParent&)
    {
		const bool bCanEditCableProperties = GetSelectedCables().Num() == 0 || CanEditCables();
        return !bCanEditCableProperties;
    }));

	UpdateSelection();
	
	USelection::SelectionChangedEvent.AddRaw(this, &STetherModeControls::OnSelectionChanged);
	
	ChildSlot
	[
		SNew(SScrollBox)
		+ SScrollBox::Slot()
		.Padding(0.0f)
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				[
					SNew(SBox)
					.MinDesiredHeight(85.f)
					[
						SNew(SExpandableArea)
						.HeaderPadding(FMargin(2.0f))
						.Padding(FMargin(10.f))
						.BorderImage(FEditorStyle::Get().GetBrush("DetailsView.CategoryTop"))
						.BorderBackgroundColor(FLinearColor(.6, .6, .6, 1.0f))
						.BodyBorderBackgroundColor(FLinearColor::Transparent)
						.AreaTitleFont(FEditorStyle::Get().GetFontStyle("EditorModesPanel.CategoryFontStyle"))
						.BodyContent()
						[
							SNew(STextBlock)
							.Text_Lambda([this]() { return FTetherPromptText::GetCurrentPromptText(GetEditorMode(), GetViewportClient()); })
							.Font(FEditorStyle::Get().GetFontStyle("EditorModesPanel.ToolDescriptionFont"))
							.AutoWrapText(true)
						]
						.HeaderContent()
						[
							SNew(STextBlock)
							.Text(INVTEXT("Tether Cable Placement"))
							.Justification(ETextJustify::Center)
							.Font(FEditorStyle::Get().GetFontStyle("EditorModesPanel.CategoryFontStyle"))
						]
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					ModeControlsDetailView.ToSharedRef()
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(10, 5)
				[
					SNew(SBox)
					.MinDesiredHeight(30.f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.FillWidth(1.f)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.TextStyle(FEditorStyle::Get(), "ContentBrowser.TopBar.Font")
							.Text_Lambda([this]() -> FText
							{
								const TArray<ATetherCableActor*> SelectedCableActors = GetSelectedCables();
								const int32 NumSelectedCables = SelectedCableActors.Num();
								const bool bCanEdit = CanEditCables();
								if(NumSelectedCables < 1)
								{
									return INVTEXT("Default properties");
								}
								else if(GetEditorMode()->IsEditing())
								{
									return FText::Format(INVTEXT("Placing {0}"), FText::FromString(SelectedCableActors[0]->GetHumanReadableName()));
								}
								else if(NumSelectedCables == 1)
								{
									const FTextFormat Format = bCanEdit? INVTEXT("Editing {0}") : INVTEXT("Selected {0}");
									return FText::Format(Format, FText::FromString(SelectedCableActors[0]->GetHumanReadableName()));
								}
								else
								{
									const FTextFormat Format = bCanEdit? INVTEXT("Editing {0} cables") : INVTEXT("Selected {0} cables");
									return FText::Format(Format, NumSelectedCables);
								}
							})
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SNew(SButton)
							.ContentPadding(FMargin(10, 2))
							.VAlign(VAlign_Center)
							.HAlign(HAlign_Center)
							.OnClicked(this, &STetherModeControls::StartNewCable)
							.Visibility_Lambda([this]() { return !GetEditorMode()->CableToolsActive() && GetSelectedCables().Num() == 0 ? EVisibility::Visible : EVisibility::Collapsed; })
							.ToolTipText(INVTEXT("Start new cable"))
							.ButtonStyle(FEditorStyle::Get(), "FlatButton.Primary")
							.ContentPadding(FMargin(10, 0))
							[
								SNew(SHorizontalBox)
								.Clipping(EWidgetClipping::ClipToBounds)
								+ SHorizontalBox::Slot()
								.VAlign(VAlign_Center)
								.Padding(3.f)
								.AutoWidth()
								[
									SNew(STextBlock)
									.TextStyle(FEditorStyle::Get(), "ContentBrowser.TopBar.Font")
									.Font(FEditorStyle::Get().GetFontStyle("FontAwesome.10"))
									.Text(FEditorFontGlyphs::Plus)
								]

								+ SHorizontalBox::Slot()
								.VAlign(VAlign_Center)
								.Padding(3.f)
								.AutoWidth()
								[
									SNew(STextBlock)
									.TextStyle(FEditorStyle::Get(), "ContentBrowser.TopBar.Font")
									.Text(INVTEXT("Start New Cable"))
								]
							]
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
						    SNew(SButton)
						    .ContentPadding(FMargin( 10, 2))
						    .VAlign(VAlign_Center)
						    .HAlign(HAlign_Center)
						    .OnClicked(this, &STetherModeControls::Exit)
						    .Visibility_Lambda([this]() { return GetEditorMode()->IsEditing() && GetEditorMode()->GetEditingCable()->GetNumGuideSplineSegments() > 1? EVisibility::Visible : EVisibility::Collapsed; })
						    .ToolTipText(INVTEXT("Finish the cable with the segments that have been committed so far (cancels the last segment that has not been committed)"))
						    .ButtonStyle(FEditorStyle::Get(), "FlatButton.Primary")
						    .ContentPadding(FMargin(10,0))
						    [
						        SNew(SHorizontalBox)
						        .Clipping(EWidgetClipping::ClipToBounds)
																				
						        + SHorizontalBox::Slot()
						        .VAlign(VAlign_Center)
						        .Padding(3.f)
						        .AutoWidth()
						        [
						        SNew(STextBlock)
						        .TextStyle(FEditorStyle::Get(), "ContentBrowser.TopBar.Font")
						        .Font( FEditorStyle::Get().GetFontStyle( "FontAwesome.10" ) )
						        .Text( FEditorFontGlyphs::Check )
						        ]

						        + SHorizontalBox::Slot()
						        .VAlign(VAlign_Center)
						        .Padding(3.f)
						        .AutoWidth()
						        [
						        SNew(STextBlock)
						        .TextStyle(FEditorStyle::Get(), "ContentBrowser.TopBar.Font")
						        .Text(INVTEXT("Finish"))
						        ]
						    ]
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SNew(SButton)
							.ContentPadding(FMargin( 10, 2))
							.VAlign(VAlign_Center)
							.HAlign(HAlign_Center)
							.OnClicked(this, &STetherModeControls::Exit)
							.Visibility_Lambda([this]() { return GetEditorMode()->IsStartingCable() || (GetEditorMode()->IsEditing() && GetEditorMode()->GetEditingCable()->GetNumGuideSplineSegments() < 2)? EVisibility::Visible : EVisibility::Collapsed; })
							.ToolTipText(INVTEXT("Cancel the cable currently being placed"))
							.ButtonStyle(FEditorStyle::Get(), "FlatButton.Danger")
							.ContentPadding(FMargin(10,0))
							[
								SNew(SHorizontalBox)
								.Clipping(EWidgetClipping::ClipToBounds)
														
								+ SHorizontalBox::Slot()
								.VAlign(VAlign_Center)
								.Padding(3.f)
								.AutoWidth()
								[
								SNew(STextBlock)
								.TextStyle(FEditorStyle::Get(), "ContentBrowser.TopBar.Font")
								.Font( FEditorStyle::Get().GetFontStyle( "FontAwesome.10" ) )
								.Text( FEditorFontGlyphs::Times )
								]

								+ SHorizontalBox::Slot()
								.VAlign(VAlign_Center)
								.Padding(3.f)
								.AutoWidth()
								[
								SNew(STextBlock)
								.TextStyle(FEditorStyle::Get(), "ContentBrowser.TopBar.Font")
								.Text(INVTEXT("Cancel"))
								]
							]
						]
						+ SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        [
                            SNew(SButton)
                            .ContentPadding(FMargin( 10, 2))
                            .VAlign(VAlign_Center)
                            .HAlign(HAlign_Center)
                            .OnClicked(this, &STetherModeControls::Deselect)
                            .Visibility_Lambda([this]() { return GetSelectedCables().Num() > 0 && !GetEditorMode()->IsEditing() ? EVisibility::Visible : EVisibility::Collapsed; })
                            .ToolTipText(INVTEXT("Deselect the cable currently being edited"))
                            .ButtonStyle(FEditorStyle::Get(), "FlatButton.Warning")
                            .ContentPadding(FMargin(10,0))
                            [
                                SNew(SHorizontalBox)
                                .Clipping(EWidgetClipping::ClipToBounds)
														
                                + SHorizontalBox::Slot()
                                .VAlign(VAlign_Center)
                                .Padding(3.f)
                                .AutoWidth()
                                [
                                SNew(STextBlock)
                                .TextStyle(FEditorStyle::Get(), "ContentBrowser.TopBar.Font")
                                .Font( FEditorStyle::Get().GetFontStyle( "FontAwesome.10" ) )
                                .Text( FEditorFontGlyphs::Times )
                                ]

                                + SHorizontalBox::Slot()
                                .VAlign(VAlign_Center)
                                .Padding(3.f)
                                .AutoWidth()
                                [
                                SNew(STextBlock)
                                .TextStyle(FEditorStyle::Get(), "ContentBrowser.TopBar.Font")
                                .Text(INVTEXT("Deselect"))
                                ]
                            ]
                        ]
					]
				
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					CablePropertiesDetailView.ToSharedRef()
				]
			]
		]
	];
}

FTetherEdMode* STetherModeControls::GetEditorMode() const
{
	return (FTetherEdMode*)GLevelEditorModeTools().GetActiveMode(FTetherEdMode::EM_Tether);
}

TArray<ATetherCableActor*> STetherModeControls::GetSelectedCables() const
{
	return FTetherEditorStatics::GetSelectedCables();
}

void STetherModeControls::OnSelectionChanged(UObject* Object)
{
	UpdateSelection();
}

void STetherModeControls::UpdateSelection()
{
	TArray<UObject*> Objects;
	
	const TArray<ATetherCableActor*> Cables = GetSelectedCables();
	if(CanEditCables() && Cables.Num() > 0)
	{
		Objects.Append(Cables);
	}
	else
	{
		class FTetherEdMode* EdMode = GetEditorMode();
		if (EdMode)
		{
			Objects.Add(EdMode->GetModeSettings());
		}
		
	}

	ensure(CablePropertiesDetailView.IsValid());
	CablePropertiesDetailView->SetObjects(Objects, true);
}

FReply STetherModeControls::StartNewCable()
{
	GetEditorMode()->SetStartCableModeActive();
	return FReply::Handled();
}

FReply STetherModeControls::Deselect()
{
	GEditor->SelectNone(true, true, true);
	return FReply::Handled();
}

FReply STetherModeControls::Exit()
{
	if(GetEditorMode()->IsStartingCable())
	{
		GetEditorMode()->SetStartCableModeActive(false);
	}
	else
	{
		GetEditorMode()->CancelSegmentAndCommitCable(nullptr);
	}
	return FReply::Handled();
}

// Copyright Sam Bonifacio 2021. All Rights Reserved.

#include "TetherEditor.h"

#include "CablePropertiesStructCustomization.h"
#include "EditorModeRegistry.h"
#include "TetherCableActor.h"
#include "TetherCableActorDetails.h"
#include "TetherCableProperties.h"
#include "TetherEdMode.h"
#include "TetherGuideSplineComponent.h"
#include "TetherGuideSplineComponentVisualizer.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"

#define LOCTEXT_NAMESPACE "FTetherEditorModule"

void FTetherEditorModule::StartupModule()
{
	FEditorModeRegistry::Get().RegisterMode<FTetherEdMode>(
		FTetherEdMode::EM_Tether,
		NSLOCTEXT("Tether_Mode", "Tether_ModeName", "Tether"),
		FSlateIcon("EditorStyle", "CurveEditor.Linear", "CurveEditor.Linear.Small"),
		true, 1000);

	FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomClassLayout(ATetherCableActor::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FTetherCableActorDetails::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout(FTetherCableProperties::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FCablePropertiesStructCustomization::MakeInstance));
	
	TSharedPtr<FTetherGuideSplineComponentVisualizer> Visualizer = MakeShareable(new FTetherGuideSplineComponentVisualizer);
	if(GUnrealEd)
	{
		GUnrealEd->RegisterComponentVisualizer(UTetherGuideSplineComponent::StaticClass()->GetFName(), Visualizer);
	}
	Visualizer->OnRegister();
}

void FTetherEditorModule::ShutdownModule()
{
	FEditorModeRegistry::Get().UnregisterMode(FTetherEdMode::EM_Tether);

	FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.UnregisterCustomClassLayout("TetherCableActor");

	if(GUnrealEd)
	{
		GUnrealEd->UnregisterComponentVisualizer(UTetherGuideSplineComponent::StaticClass()->GetFName());
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FTetherEditorModule, TetherEditor)

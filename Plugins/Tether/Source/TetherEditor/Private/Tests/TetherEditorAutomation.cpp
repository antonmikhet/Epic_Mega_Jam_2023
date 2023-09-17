// Copyright Sam Bonifacio 2021. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/TetherEditorAutomation.h"
#include "Misc/AutomationTest.h"
#include "Misc/EngineVersionComparison.h"
#include "CoreTypes.h"
#include "EditorModeManager.h"
#include "EngineUtils.h"
#include "ObjectTools.h"
#include "TetherCableActor.h"
#include "TetherEditorLogs.h"
#include "TetherEdMode.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
#include "Tests/TetherTestModeSettings.h"
#include "DirectoryWatcherModule.h"
#include "IDirectoryWatcher.h"

#if UE_VERSION_OLDER_THAN(4,26,0)
#include "AssetRegistryModule.h"
#else
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetViewUtils.h"
#endif

#if UE_VERSION_OLDER_THAN(5,0,0)
#include "EditorAssetLibrary.h"
#include "EditorLevelLibrary.h"
#else
#include "Subsystems/UnrealEditorSubsystem.h"
#include "LevelEditorSubsystem.h"

#endif

FString FTetherEditorAutomation::GetTempPath(const FAutomationTestBase* Test)
{
	return TEXT("/Game/Temp") / Test->GetTestName();
}

void FTetherEditorAutomation::ClearTemp(const FAutomationTestBase* Test)
{
	EndPlay();
	
	const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	const FString Path = GetTempPath(Test);
	
	TArray<FAssetData> AssetsToDelete;
	AssetRegistryModule.Get().GetAssetsByPath(FName(*Path), AssetsToDelete, true);
	for (FAssetData AssetData : AssetsToDelete)
	{
		#if UE_VERSION_OLDER_THAN(5,0,0)
		UWorld* EditorWorld = UEditorLevelLibrary::GetEditorWorld();
		#else
		UWorld* EditorWorld = GEditor->GetEditorSubsystem<UUnrealEditorSubsystem>()->GetEditorWorld();
		#endif
		if(AssetData == FAssetData(EditorWorld))
		{
			LoadEntry();
			break;
		}
	}
	ObjectTools::DeleteAssets(AssetsToDelete, false);

#if UE_VERSION_OLDER_THAN(4,26,0)
	if(UEditorAssetLibrary::DoesDirectoryExist(Path))
	{
		UEditorAssetLibrary::DeleteDirectory(Path);
	}
#else
	AssetViewUtils::DeleteFolders({Path});
#endif

}

void FTetherEditorAutomation::LoadEntry()
{
#if UE_VERSION_OLDER_THAN(5,0,0)
	UEditorLevelLibrary::LoadLevel(TEXT("/Engine/Maps/Entry"));
#else
	GEditor->GetEditorSubsystem<ULevelEditorSubsystem>()->LoadLevel(TEXT("/Engine/Maps/Entry"));
#endif
}

UWorld* FTetherEditorAutomation::CreateMap(const FAutomationTestBase* Test, FString Name, bool bSave)
{
	// Stop PIE before creating a map
	EndPlay();
	
	const FString Path = GetTempPath(Test) / Name;
	
	UE_LOG(LogTether, Display, TEXT("Creating world '%s' at '%s'"), *Name, *Path);


#if UE_VERSION_OLDER_THAN(5,0,0)
	ensure(UEditorLevelLibrary::NewLevel(Path));
	UWorld* World = UEditorLevelLibrary::GetEditorWorld();
#else
	ensure(GEditor->GetEditorSubsystem<ULevelEditorSubsystem>()->NewLevel(Path));
	UWorld* World = GEditor->GetEditorSubsystem<UUnrealEditorSubsystem>()->GetEditorWorld();
#endif

	if(bSave)
	{
#if UE_VERSION_OLDER_THAN(5,0,0)
	UEditorLevelLibrary::SaveCurrentLevel();
#else
	GEditor->GetEditorSubsystem<ULevelEditorSubsystem>()->SaveCurrentLevel();
#endif
	}
	

	return World;
}

UTetherModeSettings* FTetherEditorAutomation::CreateModeSettings()
{
	return NewObject<UTetherTestModeSettings>(GetTransientPackage(), TEXT("TetherTestModeSettings"), RF_Transactional | RF_Transient);
}

FTetherEdMode* FTetherEditorAutomation::ActivateMode()
{
	GLevelEditorModeTools().ActivateMode(FTetherEdMode::EM_Tether);
	FTetherEdMode* EdMode = static_cast<FTetherEdMode*>(GLevelEditorModeTools().GetActiveMode(FTetherEdMode::EM_Tether));
	EdMode->SetModeSettings(CreateModeSettings());
	return EdMode;
}

ATetherCableActor* FTetherEditorAutomation::CreateCable(UWorld* World, uint8 Points, bool bEditing)
{
	ATetherCableActor* Cable = World->SpawnActor<ATetherCableActor>();
	ensure(IsValid(Cable));
	Cable->Modify();
	Cable->SetEditing(true);
	for(uint8 i=1; i<Points;i++)
	{
		Cable->AddGuideSplinePoint(i, FVector(1000.f * i / (Points - 1)));
	}
	ensure(Cable->GetNumGuideSplinePoints() == Points);
	if(!bEditing)
	{
		Cable->SetEditing(false);
	}
	GEditor->SelectActor(Cable, true, true, true, true);
	return Cable;
}

ATetherCableActor* FTetherEditorAutomation::FindCable(UWorld* World)
{
	for (TActorIterator<ATetherCableActor> ActorItr(World); ActorItr; ++ActorItr)
	{
		return *ActorItr;
	}
	return nullptr;
}

void FTetherEditorAutomation::PrepareCableForMapClose(const ATetherCableActor* Cable)
{
#if !UE_VERSION_OLDER_THAN(5,0,0)
	// This is required when closing a map immediately after simulating a cable
	// Otherwise if the async task is still running when the world is destroyed there will be a GC error
	if(UStaticMesh* CableMesh = Cable->GetStaticMeshComponent()->GetStaticMesh())
	{
		// GCardRepresentationAsyncQueue->BlockUntilBuildComplete(CableMesh, false);
	}
#endif

}

void FTetherEditorAutomation::EndPlay()
{
	if(!ensure(GUnrealEd))
	{
		return;
	}
	if (GUnrealEd->PlayWorld)
	{
		GUnrealEd->EndPlayMap();
	}
}

void FTetherEditorAutomation::TickDirectoryWatcher()
{
	static FName DirectoryWatcherName("DirectoryWatcher");
	FDirectoryWatcherModule& DirectoryWatcherModule = FModuleManager::Get().LoadModuleChecked<FDirectoryWatcherModule>(DirectoryWatcherName);
	DirectoryWatcherModule.Get()->Tick(1.f);
}

#endif

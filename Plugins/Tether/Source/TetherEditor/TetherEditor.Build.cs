// Copyright Sam Bonifacio 2021. All Rights Reserved.

using UnrealBuildTool;

public class TetherEditor : ModuleRules
{
	public TetherEditor(ReadOnlyTargetRules Target) : base(Target)
	{
        if (Target.Type != TargetType.Editor)
        {
            throw new BuildException("Unable to instantiate TetherEditor module for non-editor targets.");
        }

        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"PropertyEditor",
				"ComponentVisualizers",
				"PropertyPath"
				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
                "UnrealEd",
                "EditorStyle",
                "InputCore",
                "Tether",
                "PropertyEditor",
                "EditorScriptingUtilities",
                "AssetTools",
                "PropertyPath",
#if UE_5_0_OR_LATER
                "EditorFramework",
                "EditorSubsystem",
                "LevelEditor",
#endif
				// ... add private dependencies that you statically link with here ...	
			}
			);
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}

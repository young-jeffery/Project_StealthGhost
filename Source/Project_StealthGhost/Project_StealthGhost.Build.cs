// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Project_StealthGhost : ModuleRules
{
	public Project_StealthGhost(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate",
			"NavigationSystem"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"Project_StealthGhost",
			"Project_StealthGhost/Variant_Platforming",
			"Project_StealthGhost/Variant_Platforming/Animation",
			"Project_StealthGhost/Variant_Combat",
			"Project_StealthGhost/Variant_Combat/AI",
			"Project_StealthGhost/Variant_Combat/Animation",
			"Project_StealthGhost/Variant_Combat/Gameplay",
			"Project_StealthGhost/Variant_Combat/Interfaces",
			"Project_StealthGhost/Variant_Combat/UI",
			"Project_StealthGhost/Variant_SideScrolling",
			"Project_StealthGhost/Variant_SideScrolling/AI",
			"Project_StealthGhost/Variant_SideScrolling/Gameplay",
			"Project_StealthGhost/Variant_SideScrolling/Interfaces",
			"Project_StealthGhost/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}

// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PolyQuest : ModuleRules
{
	public PolyQuest(ReadOnlyTargetRules Target) : base(Target)
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
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"PolyQuest",
			"PolyQuest/Variant_Platforming",
			"PolyQuest/Variant_Platforming/Animation",
			"PolyQuest/Variant_Combat",
			"PolyQuest/Variant_Combat/AI",
			"PolyQuest/Variant_Combat/Animation",
			"PolyQuest/Variant_Combat/Gameplay",
			"PolyQuest/Variant_Combat/Interfaces",
			"PolyQuest/Variant_Combat/UI",
			"PolyQuest/Variant_SideScrolling",
			"PolyQuest/Variant_SideScrolling/AI",
			"PolyQuest/Variant_SideScrolling/Gameplay",
			"PolyQuest/Variant_SideScrolling/Interfaces",
			"PolyQuest/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}

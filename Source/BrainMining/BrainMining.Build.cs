// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class BrainMining : ModuleRules
{
	public BrainMining(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"CardRoom",
			"EchoPassage",
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Niagara",
			"PhysicsCore",
			"Slate",
			"SlateCore"
		});

		PublicDependencyModuleNames.AddRange(new string[] { "GeometryCore", "GeometryFramework", "DynamicMesh" });
		PrivateDependencyModuleNames.Add("RenderCore");
        PrivateDependencyModuleNames.Add("Json");
        PrivateDependencyModuleNames.Add("ApplicationCore");
        RuntimeDependencies.Add("$(ProjectDir)/Content/UI/Gamepad/*.svg", StagedFileType.NonUFS);
        RuntimeDependencies.Add("$(ProjectDir)/Content/UI/Mobile/*.svg", StagedFileType.NonUFS);
		PrivateDependencyModuleNames.Add("AudioMixer");
		PrivateDependencyModuleNames.Add("MediaAssets");
		if (Target.Platform == UnrealTargetPlatform.Win64) PrivateDependencyModuleNames.Add("OverCrowd");
		PrivateDependencyModuleNames.Add("GameplayTags");
		PrivateDependencyModuleNames.AddRange(new[] { "GeometryAlgorithms", "AssetRegistry" });
		if (Target.bBuildEditor) PrivateDependencyModuleNames.AddRange(new[] { "UnrealEd", "MeshConversion", "MeshDescription", "StaticMeshDescription" });

		PublicIncludePaths.AddRange(new string[] {
			"BrainMining",
			"BrainMining/Variant_Horror",
			"BrainMining/Variant_Horror/UI",
			"BrainMining/Variant_Shooter",
			"BrainMining/Variant_Shooter/AI",
			"BrainMining/Variant_Shooter/UI",
			"BrainMining/Variant_Shooter/Weapons"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}

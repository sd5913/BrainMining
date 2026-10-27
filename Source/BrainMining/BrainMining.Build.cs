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
		PrivateDependencyModuleNames.AddRange(new[] { "RenderCore", "RHI", "MoviePlayer" });
		bool WithMobileASR = Target.Platform == UnrealTargetPlatform.Android || Target.Platform == UnrealTargetPlatform.Win64;
		PublicDefinitions.Add("BRAINMINING_WITH_ARM_ASR=" + (WithMobileASR ? "1" : "0"));
		if (WithMobileASR) PrivateDependencyModuleNames.Add("ArmASR");
        PrivateDependencyModuleNames.Add("Json");
        PrivateDependencyModuleNames.Add("ApplicationCore");
        RuntimeDependencies.Add("$(ProjectDir)/Content/UI/Gamepad/*.svg", StagedFileType.NonUFS);
        RuntimeDependencies.Add("$(ProjectDir)/Content/UI/Mobile/*.svg", StagedFileType.NonUFS);
        RuntimeDependencies.Add("$(ProjectDir)/Content/Localization/Tables/*.csv", StagedFileType.UFS);
		PrivateDependencyModuleNames.Add("AudioMixer");
		PrivateDependencyModuleNames.Add("MediaAssets");
		if (Target.Platform == UnrealTargetPlatform.Win64) PrivateDependencyModuleNames.Add("OverCrowd");
		PrivateDependencyModuleNames.Add("GameplayTags");
        bool WithSteam = Target.Platform == UnrealTargetPlatform.Win64 && (Target.bBuildEditor || Target.CustomConfig == "SteamPC");
        PublicDefinitions.Add("BRAINMINING_WITH_STEAM=" + (WithSteam ? "1" : "0"));
        if (WithSteam)
        {
            PrivateDependencyModuleNames.AddRange(new[] { "OnlineSubsystem", "OnlineSubsystemUtils" });
            AddEngineThirdPartyPrivateStaticDependencies(Target, "Steamworks");
            DynamicallyLoadedModuleNames.Add("OnlineSubsystemSteam");
        }
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

using UnrealBuildTool;

public class BrainMiningEditor : ModuleRules
{
	public BrainMiningEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "BrainMining" });
		PrivateDependencyModuleNames.AddRange(new[] { "UnrealEd", "Slate", "SlateCore", "PropertyEditor", "ToolMenus", "InputCore", "ApplicationCore", "Json", "AssetRegistry", "GeometryFramework", "GeometryCore", "ContentBrowser" });
	}
}

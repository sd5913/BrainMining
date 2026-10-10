using UnrealBuildTool;

public class BrainMiningEditor : ModuleRules
{
	public BrainMiningEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "BrainMining" });
		PrivateDependencyModuleNames.AddRange(new[] { "UnrealEd", "EditorFramework", "GeometryCore", "GeometryFramework", "RenderCore", "RHI", "Slate", "SlateCore", "PropertyEditor", "ToolMenus", "InputCore", "ApplicationCore", "Json", "AssetRegistry" });
	}
}

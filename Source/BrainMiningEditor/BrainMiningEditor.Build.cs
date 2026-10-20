using UnrealBuildTool;

public class BrainMiningEditor : ModuleRules
{
	public BrainMiningEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        // Editor test translation units use independent helper namespaces.
        bUseUnity = false;
		PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "BrainMining", "MeshDescription", "StaticMeshDescription" });
        PrivateDependencyModuleNames.AddRange(new[] { "GeometryAlgorithms", "DynamicMesh", "ImageCore" });
		PrivateDependencyModuleNames.AddRange(new[] { "UnrealEd", "EditorFramework", "GeometryCore", "GeometryFramework", "RenderCore", "RHI", "Slate", "SlateCore", "PropertyEditor", "ToolMenus", "InputCore", "ApplicationCore", "Json", "UMG", "UMGEditor", "Kismet", "AssetRegistry", "ContentBrowser" });
	}
}

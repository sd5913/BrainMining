using UnrealBuildTool;
public class CardRoom : ModuleRules
{
    public CardRoom(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "InputCore", "Niagara" });
        PrivateDependencyModuleNames.AddRange(new[] { "Slate", "SlateCore", "ApplicationCore", "MeshDescription", "StaticMeshDescription" });
    }
}

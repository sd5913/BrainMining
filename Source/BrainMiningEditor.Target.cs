// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class BrainMiningEditorTarget : TargetRules
{
	public BrainMiningEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		// MSVC cannot write SARIF output under the project's non-ASCII path.
		WindowsPlatform.bWriteSarif = false;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("BrainMining");
		ExtraModuleNames.Add("BrainMiningEditor");
	}
}

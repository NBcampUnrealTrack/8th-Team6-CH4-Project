using UnrealBuildTool;
using System.Collections.Generic;

public class SpaCh4EditorTarget : TargetRules
{
	public SpaCh4EditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_5;
		ExtraModuleNames.Add("SpaCh4");
	}
}

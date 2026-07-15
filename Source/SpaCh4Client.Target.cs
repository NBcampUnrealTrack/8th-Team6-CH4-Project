using UnrealBuildTool;
using System.Collections.Generic;

public class SpaCh4ClientTarget : TargetRules
{
	public SpaCh4ClientTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Client;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_5;
		ExtraModuleNames.Add("SpaCh4");
	}
}

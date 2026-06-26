using UnrealBuildTool;

public class SpaCh4 : ModuleRules
{
	public SpaCh4(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"UMG",
			"EnhancedInput"
		});
	}
}

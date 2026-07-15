// Copyright uuuuzz 2024-2026. All Rights Reserved.
// LOCAL ONLY — copy to Source/UEBridgeMCPEditor/Private/Tools/Build/ (gitignored).

#include "Tools/Build/McpLocalBuildToolsRegistration.h"

#include "Tools/Build/BuildAndRelaunchTool.h"
#include "Tools/Build/TriggerLiveCodingTool.h"
#include "Tools/McpToolRegistry.h"

void McpLocalBuildTools::Register(FMcpToolRegistry& Registry)
{
	Registry.RegisterToolClass(UTriggerLiveCodingTool::StaticClass());
	Registry.RegisterToolClass(UBuildAndRelaunchTool::StaticClass());
}

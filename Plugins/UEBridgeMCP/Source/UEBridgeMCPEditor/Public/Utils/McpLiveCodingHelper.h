// Copyright uuuuzz 2024-2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tools/McpToolResult.h"
#include "Dom/JsonObject.h"

struct FMcpLiveCodingCompileOptions
{
	/** If true, block until compile finishes or timeout is reached. */
	bool bWaitForCompletion = false;

	/** Max seconds to wait when bWaitForCompletion is true. */
	float TimeoutSeconds = 120.0f;
};

struct FMcpLiveCodingCompileOutcome
{
	bool bSuccess = false;
	FString Status;
	FString Message;
	TArray<FString> CompileErrors;
};

/**
 * Shared Live Coding compile helper used by build-and-relaunch and trigger-live-coding.
 * Keeps editor open and returns structured compiler errors for MCP agents.
 */
class FMcpLiveCodingHelper
{
public:
#if PLATFORM_WINDOWS
	static bool IsAvailable(FString& OutError);

	static FMcpLiveCodingCompileOutcome Compile(const FMcpLiveCodingCompileOptions& Options);

	static void AppendCompileErrorsToJson(
		const FMcpLiveCodingCompileOutcome& Outcome,
		TSharedPtr<FJsonObject>& ResultJson);

	static FMcpToolResult ToToolResult(const FMcpLiveCodingCompileOutcome& Outcome);
#endif
};

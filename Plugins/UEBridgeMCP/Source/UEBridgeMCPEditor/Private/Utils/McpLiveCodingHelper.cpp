// Copyright uuuuzz 2024-2026. All Rights Reserved.

#include "Utils/McpLiveCodingHelper.h"

#include "Log/McpLogCapture.h"
#include "Tools/McpToolResult.h"
#include "Modules/ModuleManager.h"

#if PLATFORM_WINDOWS
	#include "ILiveCodingModule.h"
#endif

#if PLATFORM_WINDOWS

namespace McpLiveCodingHelperPrivate
{
	static FString ResultToStatus(ELiveCodingCompileResult Result)
	{
		switch (Result)
		{
		case ELiveCodingCompileResult::Success:
			return TEXT("completed");
		case ELiveCodingCompileResult::NoChanges:
			return TEXT("no_changes");
		case ELiveCodingCompileResult::InProgress:
		case ELiveCodingCompileResult::CompileStillActive:
			return TEXT("compiling");
		case ELiveCodingCompileResult::Failure:
			return TEXT("failed");
		case ELiveCodingCompileResult::Cancelled:
			return TEXT("cancelled");
		case ELiveCodingCompileResult::NotStarted:
			return TEXT("not_started");
		default:
			return TEXT("unknown");
		}
	}

	static FString ResultToMessage(ELiveCodingCompileResult Result)
	{
		switch (Result)
		{
		case ELiveCodingCompileResult::Success:
			return TEXT("Live Coding compilation completed successfully");
		case ELiveCodingCompileResult::NoChanges:
			return TEXT("No code changes detected - nothing to compile");
		case ELiveCodingCompileResult::InProgress:
		case ELiveCodingCompileResult::CompileStillActive:
			return TEXT("Live Coding compilation is still in progress");
		case ELiveCodingCompileResult::Failure:
			return TEXT("Live Coding compilation failed. See compile_errors for details.");
		case ELiveCodingCompileResult::Cancelled:
			return TEXT("Live Coding compilation was cancelled");
		case ELiveCodingCompileResult::NotStarted:
			return TEXT("Live Coding monitor could not be started");
		default:
			return TEXT("Unexpected Live Coding compile result");
		}
	}

	static bool IsSuccessResult(ELiveCodingCompileResult Result)
	{
		return Result == ELiveCodingCompileResult::Success
			|| Result == ELiveCodingCompileResult::NoChanges;
	}

	static bool IsInProgressResult(ELiveCodingCompileResult Result)
	{
		return Result == ELiveCodingCompileResult::InProgress
			|| Result == ELiveCodingCompileResult::CompileStillActive;
	}

	static void CollectCompileErrors(FMcpLiveCodingCompileOutcome& Outcome)
	{
		static const TCHAR* CompileCategories[] =
		{
			TEXT("LogLiveCoding"),
			TEXT("LogCompile"),
			TEXT("LogUHT"),
			TEXT("LogBlueprint")
		};

		TSet<FString> Seen;
		for (const TCHAR* Category : CompileCategories)
		{
			const TArray<FMcpLogEntry> Errors = FMcpLogCapture::Get().GetLogs(
				Category,
				ELogVerbosity::Error,
				25,
				TEXT(""));

			for (const FMcpLogEntry& Entry : Errors)
			{
				if (Entry.Message.IsEmpty() || Seen.Contains(Entry.Message))
				{
					continue;
				}

				Seen.Add(Entry.Message);
				Outcome.CompileErrors.Add(Entry.Message);
			}
		}

		if (Outcome.CompileErrors.Num() == 0)
		{
			const TArray<FMcpLogEntry> GenericErrors = FMcpLogCapture::Get().GetLogs(
				TEXT(""),
				ELogVerbosity::Error,
				15,
				TEXT("error C"));

			for (const FMcpLogEntry& Entry : GenericErrors)
			{
				if (Entry.Message.IsEmpty() || Seen.Contains(Entry.Message))
				{
					continue;
				}

				Seen.Add(Entry.Message);
				Outcome.CompileErrors.Add(Entry.Message);
			}
		}
	}
}

bool FMcpLiveCodingHelper::IsAvailable(FString& OutError)
{
	ILiveCodingModule* LiveCodingModule = FModuleManager::GetModulePtr<ILiveCodingModule>(LIVE_CODING_MODULE_NAME);
	if (!LiveCodingModule)
	{
		OutError = TEXT("Live Coding module is not loaded");
		return false;
	}

	if (!LiveCodingModule->IsEnabledForSession())
	{
		OutError = TEXT("Live Coding is not enabled for this session. Enable it in Editor Preferences > General > Live Coding.");
		return false;
	}

	return true;
}

FMcpLiveCodingCompileOutcome FMcpLiveCodingHelper::Compile(const FMcpLiveCodingCompileOptions& Options)
{
	FMcpLiveCodingCompileOutcome Outcome;

	FString AvailabilityError;
	if (!IsAvailable(AvailabilityError))
	{
		Outcome.bSuccess = false;
		Outcome.Status = TEXT("unavailable");
		Outcome.Message = AvailabilityError;
		return Outcome;
	}

	ILiveCodingModule* LiveCodingModule = FModuleManager::GetModulePtr<ILiveCodingModule>(LIVE_CODING_MODULE_NAME);
	if (!LiveCodingModule)
	{
		Outcome.bSuccess = false;
		Outcome.Status = TEXT("unavailable");
		Outcome.Message = TEXT("Live Coding module is not loaded");
		return Outcome;
	}

	if (!Options.bWaitForCompletion)
	{
		LiveCodingModule->Compile(ELiveCodingCompileFlags::None, nullptr);
		Outcome.bSuccess = true;
		Outcome.Status = TEXT("triggered_async");
		Outcome.Message = TEXT("Live Coding compilation initiated. Poll with trigger-live-coding wait_for_completion=true.");
		return Outcome;
	}

	const double StartSeconds = FPlatformTime::Seconds();
	const double TimeoutSeconds = FMath::Max(1.0, static_cast<double>(Options.TimeoutSeconds));
	ELiveCodingCompileResult Result = ELiveCodingCompileResult::InProgress;

	LiveCodingModule->Compile(ELiveCodingCompileFlags::WaitForCompletion, &Result);

	while (McpLiveCodingHelperPrivate::IsInProgressResult(Result)
		&& (FPlatformTime::Seconds() - StartSeconds) < TimeoutSeconds)
	{
		FPlatformProcess::Sleep(0.25f);
		LiveCodingModule->Tick();
		LiveCodingModule->Compile(ELiveCodingCompileFlags::None, &Result);
	}

	if (McpLiveCodingHelperPrivate::IsInProgressResult(Result))
	{
		Outcome.bSuccess = false;
		Outcome.Status = TEXT("timeout");
		Outcome.Message = FString::Printf(
			TEXT("Live Coding compile did not finish within %.0f seconds"),
			Options.TimeoutSeconds);
		return Outcome;
	}

	Outcome.Status = McpLiveCodingHelperPrivate::ResultToStatus(Result);
	Outcome.Message = McpLiveCodingHelperPrivate::ResultToMessage(Result);
	Outcome.bSuccess = McpLiveCodingHelperPrivate::IsSuccessResult(Result);

	if (!Outcome.bSuccess)
	{
		McpLiveCodingHelperPrivate::CollectCompileErrors(Outcome);
	}

	return Outcome;
}

void FMcpLiveCodingHelper::AppendCompileErrorsToJson(
	const FMcpLiveCodingCompileOutcome& Outcome,
	TSharedPtr<FJsonObject>& ResultJson)
{
	if (!ResultJson.IsValid())
	{
		return;
	}

	if (Outcome.CompileErrors.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> ErrorArray;
		for (const FString& Error : Outcome.CompileErrors)
		{
			ErrorArray.Add(MakeShareable(new FJsonValueString(Error)));
		}
		ResultJson->SetArrayField(TEXT("compile_errors"), ErrorArray);
	}
}

FMcpToolResult FMcpLiveCodingHelper::ToToolResult(const FMcpLiveCodingCompileOutcome& Outcome)
{
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), Outcome.bSuccess);
	Result->SetStringField(TEXT("status"), Outcome.Status);
	Result->SetStringField(TEXT("message"), Outcome.Message);
	Result->SetStringField(TEXT("compile_mode"), TEXT("live_coding"));
	Result->SetStringField(TEXT("shortcut"), TEXT("Ctrl+Alt+F11"));
	AppendCompileErrorsToJson(Outcome, Result);
	return FMcpToolResult::Json(Result);
}

#endif

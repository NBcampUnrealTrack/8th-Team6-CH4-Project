#include "Debug/SPCheatManager.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Player/SPPlayerController.h"

namespace SPCheatConsole
{
	static void SetLocalSurvivorState(UWorld* World, ESurvivorState NewState)
	{
		if (!World)
		{
			return;
		}

		APlayerController* LocalPC = GEngine ? GEngine->GetFirstLocalPlayerController(World) : nullptr;
		if (!LocalPC)
		{
			UE_LOG(LogTemp, Warning, TEXT("SPCheat: no local PlayerController."));
			return;
		}

		ASurvivorCharacter* Survivor = Cast<ASurvivorCharacter>(LocalPC->GetPawn());
		if (!Survivor)
		{
			UE_LOG(LogTemp, Warning, TEXT("SPCheat: no Survivor pawn."));
			return;
		}

		if (Survivor->HasAuthority())
		{
			Survivor->SetSurvivorState(NewState);
			UE_LOG(LogTemp, Log, TEXT("SPCheat: set SurvivorState -> %d"), static_cast<int32>(NewState));
			return;
		}

		if (ASPPlayerController* SPPC = Cast<ASPPlayerController>(LocalPC))
		{
			SPPC->Server_CheatSetSurvivorState(NewState);
			UE_LOG(LogTemp, Log, TEXT("SPCheat: requested SurvivorState -> %d (server)"), static_cast<int32>(NewState));
		}
	}

	static FAutoConsoleCommandWithWorld CCmdDowned(
		TEXT("SP.Downed"),
		TEXT("Force local survivor into Downed state (prone crawl test)."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			SetLocalSurvivorState(World, ESurvivorState::Downed);
		}));

	static FAutoConsoleCommandWithWorld CCmdHealthy(
		TEXT("SP.Healthy"),
		TEXT("Force local survivor into Healthy state."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			SetLocalSurvivorState(World, ESurvivorState::Healthy);
		}));

	static FAutoConsoleCommandWithWorld CCmdInjured(
		TEXT("SP.Injured"),
		TEXT("Force local survivor into Injured state."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			SetLocalSurvivorState(World, ESurvivorState::Injured);
		}));
}

void USPCheatManager::SPDowned()
{
	ApplySurvivorState(ESurvivorState::Downed);
}

void USPCheatManager::SPHealthy()
{
	ApplySurvivorState(ESurvivorState::Healthy);
}

void USPCheatManager::SPInjured()
{
	ApplySurvivorState(ESurvivorState::Injured);
}

void USPCheatManager::ApplySurvivorState(ESurvivorState NewState)
{
	APlayerController* PC = GetOuterAPlayerController();
	if (!PC)
	{
		return;
	}

	SPCheatConsole::SetLocalSurvivorState(PC->GetWorld(), NewState);
}

#include "Player/SPPlayerController.h"

#include "EnhancedInputSubsystems.h"
#include "Data/SPInputConfigData.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Systems/MatchGameState.h"
#include "Systems/SPEOSSessionSubsystem.h"
#include "TimerManager.h"
#include "UI/GameResultWidget.h"

void ASPPlayerController::ReturnToMainMenu()
{
	if (!IsLocalController())
	{
		return;
	}
	if (bIsHostReturnToMainMenuPending && GameResultWidgetInstance)
	{
		GameResultWidgetInstance->SetInformationText(TEXT("다른 플레이어가 메인 메뉴로 나갈 때까지 호스트의 이동을 대기합니다."));
		//OnReturnToMainMenuStatusChanged.Broadcast(TEXT("다른 플레이어가 메인 메뉴로 나갈 때까지 호스트의 이동을 대기합니다."));
		return;
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (USPEOSSessionSubsystem* SessionSubsystem = GameInstance->GetSubsystem<USPEOSSessionSubsystem>())
		{
			SessionSubsystem->EndSession();
		}
	}

	const bool bIsListenServerHost = HasAuthority() && GetNetMode() == NM_ListenServer;
	if (bIsListenServerHost && HasConnectedRemotePlayers())
	{
		bIsHostReturnToMainMenuPending = true;
		GameResultWidgetInstance->SetInformationText(TEXT("다른 플레이어가 메인 메뉴로 나갈 때까지 호스트의 이동을 대기합니다."));
		//OnReturnToMainMenuStatusChanged.Broadcast(TEXT("다른 플레이어가 메인 메뉴로 나갈 때까지 호스트의 이동을 대기합니다."));
		GetWorldTimerManager().SetTimer(
			HostReturnToMainMenuTimerHandle,
			this,
			&ASPPlayerController::TryCompletePendingHostReturn,
			HostReturnCheckInterval,
			true);
		return;
	}

	CompleteReturnToMainMenu();
}

bool ASPPlayerController::IsReturnToMainMenuPending() const
{
	return bIsHostReturnToMainMenuPending;
}

void ASPPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (IsLocalController())
	{
		SetInputMode(FInputModeGameOnly());
		bShowMouseCursor = false;
		ResetIgnoreMoveInput();
		ResetIgnoreLookInput();

		if (UGameInstance* GameInstance = GetGameInstance())
		{
			if (USPEOSSessionSubsystem* SessionSubsystem = GameInstance->GetSubsystem<USPEOSSessionSubsystem>())
			{
				SessionSubsystem->StartSession();
			}
		}

		AGameStateBase* GS = UGameplayStatics::GetGameState(GetWorld());
		if (IsValid(GS))
		{
			AMatchGameState* MGS = Cast<AMatchGameState>(GS);
			if (IsValid(MGS))
			{
				MGS->OnMatchResultChanged.AddDynamic(this, &ASPPlayerController::OnMatchEnded);
			}
		}
	}
	if (!InputConfig)
	{
		return;
	}
	
	for (const FInputMappingContextEntry& Entry : InputConfig->MappingContexts)
	{
		AddInputMappingContext(Entry.MappingContext, Entry.Priority);
	}
}

void ASPPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(HostReturnToMainMenuTimerHandle);

	if (AMatchGameState* MatchGameState = GetWorld() ? GetWorld()->GetGameState<AMatchGameState>() : nullptr)
	{
		MatchGameState->OnMatchResultChanged.RemoveDynamic(this, &ASPPlayerController::OnMatchEnded);
	}

	Super::EndPlay(EndPlayReason);
}

void ASPPlayerController::AddInputMappingContext(UInputMappingContext* MappingContext, int32 Priority)
{
	if (!MappingContext)
	{
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = GetInputSubsystem())
	{
		Subsystem->AddMappingContext(MappingContext, Priority);
	}
}

void ASPPlayerController::RemoveInputMappingContext(UInputMappingContext* MappingContext)
{
	if (!MappingContext)
	{
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = GetInputSubsystem())
	{
		Subsystem->RemoveMappingContext(MappingContext);
	}
}

UEnhancedInputLocalPlayerSubsystem* ASPPlayerController::GetInputSubsystem() const
{
	return ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
}

void ASPPlayerController::OnMatchEnded(EMatchResult Result)
{
	if (Result == EMatchResult::None)
	{
		return;
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (USPEOSSessionSubsystem* SessionSubsystem = GameInstance->GetSubsystem<USPEOSSessionSubsystem>())
		{
			SessionSubsystem->EndSession();
		}
	}

	if (GameResultWidgetInstance)
	{
		return;
	}

	if (!GameResultWidgetClass)
	{
		return;
	}

	if (GameResultWidgetClass->HasAnyClassFlags(CLASS_Abstract))
	{
		return;
	}

	GameResultWidgetInstance = CreateWidget<UGameResultWidget>(this, GameResultWidgetClass);
	if (!GameResultWidgetInstance)
	{
		return;
	}

	GameResultWidgetInstance->AddToViewport(0);

	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(GameResultWidgetInstance->TakeWidget());
	SetInputMode(InputMode);
	bShowMouseCursor = true;
}

void ASPPlayerController::CompleteReturnToMainMenu()
{
	GetWorldTimerManager().ClearTimer(HostReturnToMainMenuTimerHandle);
	bIsHostReturnToMainMenuPending = false;
	GameResultWidgetInstance->SetInformationText(TEXT("메인 메뉴로 이동합니다."));
	//OnReturnToMainMenuStatusChanged.Broadcast(TEXT("메인 메뉴로 이동합니다."));

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (USPEOSSessionSubsystem* SessionSubsystem = GameInstance->GetSubsystem<USPEOSSessionSubsystem>())
		{
			SessionSubsystem->ReturnToMainMenu(MainMenuLevelPath);
		}
	}
}

void ASPPlayerController::TryCompletePendingHostReturn()
{
	if (!bIsHostReturnToMainMenuPending)
	{
		GetWorldTimerManager().ClearTimer(HostReturnToMainMenuTimerHandle);
		return;
	}

	if (!HasConnectedRemotePlayers())
	{
		CompleteReturnToMainMenu();
	}
}

bool ASPPlayerController::HasConnectedRemotePlayers() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PlayerController = It->Get();
		if (PlayerController && PlayerController != this)
		{
			return true;
		}
	}

	return false;
}

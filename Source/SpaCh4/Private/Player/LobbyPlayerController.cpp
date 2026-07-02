#include "Player/LobbyPlayerController.h"

#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Systems/LobbyGameMode.h"
#include "Systems/MatchGameState.h"
#include "UI/TestLobbyUIWidget.h"

void ALobbyPlayerController::LobbySetNickname(const FString& NewNickname)
{
	if (HasAuthority())
	{
		if (ALobbyGameMode* LobbyGameMode = GetWorld()->GetAuthGameMode<ALobbyGameMode>())
		{
			LobbyGameMode->SetLobbyPlayerNickname(this, NewNickname);
		}
		return;
	}

	ServerLobbySetNickname(NewNickname);
}

void ALobbyPlayerController::LobbySetRole(ELobbyPlayerRole NewRole)
{
	if (HasAuthority())
	{
		if (ALobbyGameMode* LobbyGameMode = GetWorld()->GetAuthGameMode<ALobbyGameMode>())
		{
			LobbyGameMode->SetLobbyPlayerRole(this, NewRole);
		}
		return;
	}

	ServerLobbySetRole(NewRole);
}

void ALobbyPlayerController::LobbySetReady(bool bNewReady)
{
	if (HasAuthority())
	{
		if (ALobbyGameMode* LobbyGameMode = GetWorld()->GetAuthGameMode<ALobbyGameMode>())
		{
			LobbyGameMode->SetLobbyPlayerReady(this, bNewReady);
		}
		return;
	}

	ServerLobbySetReady(bNewReady);
}

void ALobbyPlayerController::LobbySubmitSettings(const FString& NewNickname, ELobbyPlayerRole NewRole, bool bNewReady)
{
	if (HasAuthority())
	{
		if (ALobbyGameMode* LobbyGameMode = GetWorld()->GetAuthGameMode<ALobbyGameMode>())
		{
			LobbyGameMode->SubmitLobbyPlayerSettings(this, NewNickname, NewRole, bNewReady);
		}
		return;
	}

	ServerLobbySubmitSettings(NewNickname, NewRole, bNewReady);
}

void ALobbyPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (IsLocalController() == false)
	{
		return;
	}

	if (IsValid(LobbyUIWidgetClass) == true)
	{
		LobbyUIWidgetInstance = CreateWidget<UTestLobbyUIWidget>(this, LobbyUIWidgetClass);
		if (IsValid(LobbyUIWidgetInstance) == true)
		{
			LobbyUIWidgetInstance->AddToViewport();

			FInputModeUIOnly Mode;
			//Mode.SetWidgetToFocus(LobbyUIWidgetInstance->GetCachedWidget());
			SetInputMode(Mode);

			bShowMouseCursor = true;
			
			
		}
	}
	ALobbyGameState* LGS = Cast<ALobbyGameState>(UGameplayStatics::GetGameState(GetWorld()));
	if (IsValid(LGS))
	{
		LGS->OnLobbyPlayersChanged.AddDynamic(this, &ALobbyPlayerController::OnLobbyPlayersChanged);
		LGS->OnLobbyRoleCountChanged.AddDynamic(this, &ALobbyPlayerController::OnLobbyRoleCountChanged);
		LGS->OnLobbyReadyCountChanged.AddDynamic(this, &ALobbyPlayerController::OnLobbyReadyCountChanged);
		LGS->OnLobbyPhaseChanged.AddDynamic(this, &ALobbyPlayerController::OnLobbyPhaseChanged);
		LGS->OnLobbyCountdownChanged.AddDynamic(this, &ALobbyPlayerController::OnLobbyCountdownChanged);
	}
}

void ALobbyPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	FInputModeGameOnly Mode;
	SetInputMode(Mode);

	bShowMouseCursor = false;
	if (LobbyUIWidgetInstance)
	{
		LobbyUIWidgetInstance->RemoveFromParent();
		LobbyUIWidgetInstance = nullptr;
	}
}

void ALobbyPlayerController::OnLobbyPlayersChanged()
{
	if (IsValid(LobbyUIWidgetInstance))
	{
		ALobbyGameState* LGS = Cast<ALobbyGameState>(UGameplayStatics::GetGameState(GetWorld()));
		if (IsValid(LGS) == true){
			LobbyUIWidgetInstance->PlayerCountText->SetText(
				FText::FromString(
					FString::Printf(TEXT("로비 접속자 수 : %d"), LGS->GetConnectedPlayerCount())));
		}
	}
}

void ALobbyPlayerController::OnLobbyRoleCountChanged(int32 SurvivorCount, int32 KillerCount)
{
	if (IsValid(LobbyUIWidgetInstance))
	{
		LobbyUIWidgetInstance->GameRoleText->SetText(
			FText::FromString(
				FString::Printf(TEXT("생존자 : %d명 | 살인마 : %d 명"), SurvivorCount, KillerCount)));
	}
}

void ALobbyPlayerController::OnLobbyReadyCountChanged(int32 ReadyCount, int32 RequireReadyCount)
{
	if (IsValid(LobbyUIWidgetInstance))
	{
		LobbyUIWidgetInstance->PlayerReadyText->SetText(
			FText::FromString(
				FString::Printf(TEXT("준비 인원 : %d / %d"), ReadyCount, RequireReadyCount)));
	}
}

void ALobbyPlayerController::OnLobbyPhaseChanged(ELobbyPhase NewPhase)
{
	if (NewPhase == ELobbyPhase::Traveling)
	{
		LobbyUIWidgetInstance->NotificationText->SetText(
			FText::FromString(
				FString::Printf(TEXT("게임으로 이동증..."))));
	}
}

void ALobbyPlayerController::OnLobbyCountdownChanged(int32 CountdownRemainingTime)
{
	
	if (IsValid(LobbyUIWidgetInstance))
	{
		LobbyUIWidgetInstance->NotificationText->SetText(
			FText::FromString(
				FString::Printf(TEXT("게임 시작까지 %d초"), CountdownRemainingTime)));
	}
}

void ALobbyPlayerController::ServerLobbySetNickname_Implementation(const FString& NewNickname)
{
	if (ALobbyGameMode* LobbyGameMode = GetWorld()->GetAuthGameMode<ALobbyGameMode>())
	{
		LobbyGameMode->SetLobbyPlayerNickname(this, NewNickname);
	}
}

void ALobbyPlayerController::ServerLobbySetRole_Implementation(ELobbyPlayerRole NewRole)
{
	if (ALobbyGameMode* LobbyGameMode = GetWorld()->GetAuthGameMode<ALobbyGameMode>())
	{
		LobbyGameMode->SetLobbyPlayerRole(this, NewRole);
	}
}

void ALobbyPlayerController::ServerLobbySetReady_Implementation(bool bNewReady)
{
	if (ALobbyGameMode* LobbyGameMode = GetWorld()->GetAuthGameMode<ALobbyGameMode>())
	{
		LobbyGameMode->SetLobbyPlayerReady(this, bNewReady);
	}
}

void ALobbyPlayerController::ServerLobbySubmitSettings_Implementation(const FString& NewNickname,
                                                                      ELobbyPlayerRole NewRole, bool bNewReady)
{
	if (ALobbyGameMode* LobbyGameMode = GetWorld()->GetAuthGameMode<ALobbyGameMode>())
	{
		LobbyGameMode->SubmitLobbyPlayerSettings(this, NewNickname, NewRole, bNewReady);
	}
}

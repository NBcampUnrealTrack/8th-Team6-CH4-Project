#include "Systems/LobbyGameMode.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Player/LobbyPlayerController.h"
#include "TimerManager.h"
#include "Player/LDPlayerState.h"

ALobbyGameMode::ALobbyGameMode()
{
	GameStateClass = ALobbyGameState::StaticClass();
	PlayerControllerClass = ALobbyPlayerController::StaticClass();
	PlayerStateClass = ALDPlayerState::StaticClass();
	bUseSeamlessTravel = true;
}

void ALobbyGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (ALobbyGameState* LobbyGameState = GetLobbyGameState())
	{
		LobbyGameState->SetLobbyRules(SurvivorLimit, KillerLimit, RequiredReadyPlayerCount);
	}
}

void ALobbyGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	RegisterLobbyPlayer(NewPlayer);
	EvaluateCountdownState();
}

void ALobbyGameMode::Logout(AController* Exiting)
{
	if (APlayerController* PlayerController = Cast<APlayerController>(Exiting))
	{
		if (ALobbyGameState* LobbyGameState = GetLobbyGameState())
		{
			LobbyGameState->RemovePlayer(GetPlayerId(PlayerController));
		}
	}

	Super::Logout(Exiting);
	EvaluateCountdownState();
}

bool ALobbyGameMode::SetLobbyPlayerNickname(APlayerController* PlayerController, const FString& NewNickname)
{
	if (!HasAuthority() || !IsValid(PlayerController))
	{
		return false;
	}

	ALobbyGameState* LobbyGameState = GetLobbyGameState();
	if (!IsValid(LobbyGameState))
	{
		return false;
	}

	const int32 PlayerId = GetPlayerId(PlayerController);
	const FString SanitizedNickname = NewNickname.TrimStartAndEnd().Left(16);
	const bool bWasUpdated = LobbyGameState->SetPlayerNickname(PlayerId, SanitizedNickname);

	FLobbyPlayerInfo PlayerInfo;
	if (bWasUpdated && LobbyGameState->GetLobbyPlayerInfo(PlayerId, PlayerInfo))
	{
		ApplyLobbyInfoToPlayerState(PlayerController, PlayerInfo);
	}

	EvaluateCountdownState();
	return bWasUpdated;
}

bool ALobbyGameMode::SetLobbyPlayerRole(APlayerController* PlayerController, ELobbyPlayerRole NewRole)
{
	if (!HasAuthority() || !IsValid(PlayerController))
	{
		return false;
	}

	ALobbyGameState* LobbyGameState = GetLobbyGameState();
	if (!IsValid(LobbyGameState))
	{
		return false;
	}

	const int32 PlayerId = GetPlayerId(PlayerController);
	const bool bWasUpdated = LobbyGameState->SetPlayerRole(PlayerId, NewRole);

	FLobbyPlayerInfo PlayerInfo;
	if (bWasUpdated && LobbyGameState->GetLobbyPlayerInfo(PlayerId, PlayerInfo))
	{
		ApplyLobbyInfoToPlayerState(PlayerController, PlayerInfo);
	}

	EvaluateCountdownState();
	return bWasUpdated;
}

bool ALobbyGameMode::SetLobbyPlayerReady(APlayerController* PlayerController, bool bNewReady)
{
	if (!HasAuthority() || !IsValid(PlayerController))
	{
		return false;
	}

	ALobbyGameState* LobbyGameState = GetLobbyGameState();
	if (!IsValid(LobbyGameState))
	{
		return false;
	}

	const int32 PlayerId = GetPlayerId(PlayerController);

	FLobbyPlayerInfo PlayerInfo;
	if (!LobbyGameState->GetLobbyPlayerInfo(PlayerId, PlayerInfo))
	{
		return false;
	}

	if (bNewReady && (PlayerInfo.Nickname.IsEmpty() || PlayerInfo.SelectedRole == ELobbyPlayerRole::None))
	{
		return false;
	}

	const bool bWasUpdated = LobbyGameState->SetPlayerReady(PlayerId, bNewReady);
	EvaluateCountdownState();
	return bWasUpdated;
}

bool ALobbyGameMode::SubmitLobbyPlayerSettings(APlayerController* PlayerController, const FString& NewNickname, ELobbyPlayerRole NewRole, bool bNewReady)
{
	if (!HasAuthority() || !IsValid(PlayerController))
	{
		return false;
	}

	ALobbyGameState* LobbyGameState = GetLobbyGameState();
	if (!IsValid(LobbyGameState))
	{
		return false;
	}

	const int32 PlayerId = GetPlayerId(PlayerController);
	if (!LobbyGameState->CanSelectRole(PlayerId, NewRole))
	{
		return false;
	}

	const FString SanitizedNickname = NewNickname.TrimStartAndEnd().Left(16);
	if (bNewReady && (SanitizedNickname.IsEmpty() || NewRole == ELobbyPlayerRole::None))
	{
		return false;
	}

	const bool bNicknameUpdated = LobbyGameState->SetPlayerNickname(PlayerId, SanitizedNickname);
	const bool bRoleUpdated = LobbyGameState->SetPlayerRole(PlayerId, NewRole);
	const bool bReadyUpdated = LobbyGameState->SetPlayerReady(PlayerId, bNewReady);

	FLobbyPlayerInfo PlayerInfo;
	if (LobbyGameState->GetLobbyPlayerInfo(PlayerId, PlayerInfo))
	{
		ApplyLobbyInfoToPlayerState(PlayerController, PlayerInfo);
	}

	EvaluateCountdownState();
	return bNicknameUpdated || bRoleUpdated || bReadyUpdated;
}

bool ALobbyGameMode::CanStartCountdown() const
{
	const ALobbyGameState* LobbyGameState = GetLobbyGameState();
	if (!IsValid(LobbyGameState))
	{
		return false;
	}

	return LobbyGameState->GetConnectedPlayerCount() == RequiredReadyPlayerCount
		&& LobbyGameState->GetSurvivorCount() == SurvivorLimit
		&& LobbyGameState->GetKillerCount() == KillerLimit
		&& LobbyGameState->GetReadyCount() == RequiredReadyPlayerCount;
}

ALobbyGameState* ALobbyGameMode::GetLobbyGameState() const
{
	return GetGameState<ALobbyGameState>();
}

int32 ALobbyGameMode::GetPlayerId(const APlayerController* PlayerController) const
{
	if (!IsValid(PlayerController) || !IsValid(PlayerController->PlayerState))
	{
		return INDEX_NONE;
	}

	return PlayerController->PlayerState->GetPlayerId();
}

FString ALobbyGameMode::BuildDefaultNickname(const APlayerController* PlayerController) const
{
	const int32 PlayerId = GetPlayerId(PlayerController);
	return FString::Printf(TEXT("Player_%d"), PlayerId);
}

void ALobbyGameMode::RegisterLobbyPlayer(APlayerController* PlayerController)
{
	if (!HasAuthority() || !IsValid(PlayerController))
	{
		return;
	}

	ALobbyGameState* LobbyGameState = GetLobbyGameState();
	if (!IsValid(LobbyGameState))
	{
		return;
	}

	FLobbyPlayerInfo PlayerInfo;
	PlayerInfo.PlayerId = GetPlayerId(PlayerController);
	PlayerInfo.Nickname = BuildDefaultNickname(PlayerController);
	PlayerInfo.SelectedRole = ELobbyPlayerRole::None;
	PlayerInfo.bIsReady = false;

	LobbyGameState->AddOrUpdatePlayer(PlayerInfo);
	ApplyLobbyInfoToPlayerState(PlayerController, PlayerInfo);
}

void ALobbyGameMode::ApplyLobbyInfoToPlayerState(APlayerController* PlayerController, const FLobbyPlayerInfo& PlayerInfo) const
{
	if (!IsValid(PlayerController) || !IsValid(PlayerController->PlayerState))
	{
		return;
	}

	// 레벨 변경시 역활을 PlayerState의 Tag설정으로 구분
	ALDPlayerState* PlayerState = Cast<ALDPlayerState>(PlayerController->PlayerState);
	if (!IsValid(PlayerState))
	{
		return;
	}

	if (!PlayerInfo.Nickname.IsEmpty())
	{
		PlayerState->SetPlayerName(PlayerInfo.Nickname);
	}


	PlayerState->SetPlayerRole(PlayerInfo.SelectedRole);
}

void ALobbyGameMode::EvaluateCountdownState()
{
	if (CanStartCountdown())
	{
		StartCountdown();
		return;
	}

	CancelCountdown();
}

void ALobbyGameMode::StartCountdown()
{
	ALobbyGameState* LobbyGameState = GetLobbyGameState();
	if (!IsValid(LobbyGameState) || LobbyGameState->GetLobbyPhase() == ELobbyPhase::Countdown)
	{
		return;
	}

	LobbyGameState->SetLobbyPhase(ELobbyPhase::Countdown);
	LobbyGameState->SetCountdownRemainingTime(CountdownDuration);

	GetWorldTimerManager().ClearTimer(CountdownTimerHandle);
	GetWorldTimerManager().SetTimer(CountdownTimerHandle, this, &ALobbyGameMode::HandleCountdownTick, 1.0f, true);
}

void ALobbyGameMode::CancelCountdown()
{
	ALobbyGameState* LobbyGameState = GetLobbyGameState();
	if (!IsValid(LobbyGameState) || LobbyGameState->GetLobbyPhase() == ELobbyPhase::Traveling)
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(CountdownTimerHandle);
	LobbyGameState->SetCountdownRemainingTime(0);
	LobbyGameState->SetLobbyPhase(ELobbyPhase::WaitingForPlayers);
}

void ALobbyGameMode::HandleCountdownTick()
{
	ALobbyGameState* LobbyGameState = GetLobbyGameState();
	if (!IsValid(LobbyGameState))
	{
		GetWorldTimerManager().ClearTimer(CountdownTimerHandle);
		return;
	}

	if (!CanStartCountdown())
	{
		CancelCountdown();
		return;
	}

	const int32 NewCountdownRemainingTime = LobbyGameState->GetCountdownRemainingTime() - 1;
	LobbyGameState->SetCountdownRemainingTime(NewCountdownRemainingTime);

	if (NewCountdownRemainingTime <= 0)
	{
		TravelToMatchGameLevel();
	}
}

void ALobbyGameMode::TravelToMatchGameLevel()
{
	ALobbyGameState* LobbyGameState = GetLobbyGameState();
	if (!IsValid(LobbyGameState) || MatchLevelUrl.IsEmpty())
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(CountdownTimerHandle);
	LobbyGameState->SetLobbyPhase(ELobbyPhase::Traveling);


	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		APlayerController* PlayerController = Iterator->Get();
		if (!IsValid(PlayerController))
		{
			continue;
		}

		FLobbyPlayerInfo PlayerInfo;
		if (LobbyGameState->GetLobbyPlayerInfo(GetPlayerId(PlayerController), PlayerInfo))
		{
			ApplyLobbyInfoToPlayerState(PlayerController, PlayerInfo);

		}
	}

	GetWorld()->ServerTravel(MatchLevelUrl);
}

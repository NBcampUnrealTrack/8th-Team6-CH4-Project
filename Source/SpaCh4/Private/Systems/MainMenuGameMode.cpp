#include "Systems/MainMenuGameMode.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Player/LDPlayerState.h"
#include "Player/MainMenuPlayerController.h"
#include "Systems/Data/BalanceData.h"
#include "TimerManager.h"

AMainMenuGameMode::AMainMenuGameMode()
{
	PlayerControllerClass = AMainMenuPlayerController::StaticClass();
	GameStateClass = ALobbyGameState::StaticClass();
	PlayerStateClass = ALDPlayerState::StaticClass();
	DefaultPawnClass = nullptr;
	bUseSeamlessTravel = true;
}

void AMainMenuGameMode::BeginPlay()
{
	Super::BeginPlay();
	ApplyBalanceRules();
}

void AMainMenuGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	RegisterMainMenuPlayer(NewPlayer);
	EvaluateCountdownState();
}

void AMainMenuGameMode::Logout(AController* Exiting)
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

bool AMainMenuGameMode::SubmitMatchmakingRole(APlayerController* PlayerController, const FString& Nickname, ELobbyPlayerRole SelectedRole, FString& OutStatusMessage)
{
	if (!HasAuthority() || !IsValid(PlayerController))
	{
		OutStatusMessage = TEXT("서버에서만 역할을 등록할 수 있습니다.");
		return false;
	}

	const ALDPlayerState* LDPlayerState = PlayerController->GetPlayerState<ALDPlayerState>();
	if (!IsValid(LDPlayerState) || !LDPlayerState->IsLoadoutConfiguredForRole(SelectedRole))
	{
		OutStatusMessage = TEXT("매칭 시작 전에 설정에서 아이템과 퍽을 먼저 선택해야 합니다.");
		return false;
	}

	ALobbyGameState* LobbyGameState = GetLobbyGameState();
	if (!IsValid(LobbyGameState))
	{
		OutStatusMessage = TEXT("매치메이킹 상태를 찾을 수 없습니다.");
		return false;
	}

	const int32 PlayerId = GetPlayerId(PlayerController);
	if (!LobbyGameState->CanSelectRole(PlayerId, SelectedRole))
	{
		OutStatusMessage = SelectedRole == ELobbyPlayerRole::Survivor
			? TEXT("생존자 역할이 가득 찼습니다.")
			: TEXT("살인마 역할이 가득 찼습니다.");
		return false;
	}

	const FString SanitizedNickname = Nickname.TrimStartAndEnd().Left(16);
	const FString FinalNickname = SanitizedNickname.IsEmpty() ? BuildDefaultNickname(PlayerController) : SanitizedNickname;

	LobbyGameState->SetPlayerNickname(PlayerId, FinalNickname);
	LobbyGameState->SetPlayerRole(PlayerId, SelectedRole);
	LobbyGameState->SetPlayerReady(PlayerId, SelectedRole != ELobbyPlayerRole::None);

	FLobbyPlayerInfo PlayerInfo;
	if (LobbyGameState->GetLobbyPlayerInfo(PlayerId, PlayerInfo))
	{
		ApplyLobbyInfoToPlayerState(PlayerController, PlayerInfo);
	}

	EvaluateCountdownState();

	OutStatusMessage = FString::Printf(
		TEXT("현재 생존자 %d/%d명, 살인마 %d/%d명 대기중"),
		LobbyGameState->GetSurvivorCount(),
		LobbyGameState->GetSurvivorLimit(),
		LobbyGameState->GetKillerCount(),
		LobbyGameState->GetKillerLimit());
	return true;
}

bool AMainMenuGameMode::CanStartMatchCountdown() const
{
	const ALobbyGameState* LobbyGameState = GetLobbyGameState();
	if (!IsValid(LobbyGameState))
	{
		return false;
	}

	const int32 RequiredReadyCount = LobbyGameState->GetRequiredReadyPlayerCount();
	return LobbyGameState->GetConnectedPlayerCount() == RequiredReadyCount
		&& LobbyGameState->GetSurvivorCount() == LobbyGameState->GetSurvivorLimit()
		&& LobbyGameState->GetKillerCount() == LobbyGameState->GetKillerLimit()
		&& LobbyGameState->GetReadyCount() == RequiredReadyCount;
}

ALobbyGameState* AMainMenuGameMode::GetLobbyGameState() const
{
	return GetGameState<ALobbyGameState>();
}

int32 AMainMenuGameMode::GetPlayerId(const APlayerController* PlayerController) const
{
	if (!IsValid(PlayerController) || !IsValid(PlayerController->PlayerState))
	{
		return INDEX_NONE;
	}

	return PlayerController->PlayerState->GetPlayerId();
}

FString AMainMenuGameMode::BuildDefaultNickname(const APlayerController* PlayerController) const
{
	if (IsValid(PlayerController) && IsValid(PlayerController->PlayerState) && !PlayerController->PlayerState->GetPlayerName().IsEmpty())
	{
		return PlayerController->PlayerState->GetPlayerName();
	}

	return FString::Printf(TEXT("Player_%d"), GetPlayerId(PlayerController));
}

void AMainMenuGameMode::RegisterMainMenuPlayer(APlayerController* PlayerController)
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

void AMainMenuGameMode::ApplyLobbyInfoToPlayerState(APlayerController* PlayerController, const FLobbyPlayerInfo& PlayerInfo) const
{
	if (!IsValid(PlayerController) || !IsValid(PlayerController->PlayerState))
	{
		return;
	}

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

const UBalanceData* AMainMenuGameMode::GetActiveBalanceData() const
{
	if (IsValid(BalanceData))
	{
		return BalanceData;
	}

	return GetDefault<UBalanceData>();
}

void AMainMenuGameMode::ApplyBalanceRules()
{
	if (const UBalanceData* ActiveBalanceData = GetActiveBalanceData())
	{
		SurvivorLimit = FMath::Max(0, ActiveBalanceData->InitialSurvivorCount);
		KillerLimit = FMath::Max(0, ActiveBalanceData->InitialKillerCount);
		RequiredReadyPlayerCount = SurvivorLimit + KillerLimit;
	}

	if (ALobbyGameState* LobbyGameState = GetLobbyGameState())
	{
		LobbyGameState->SetLobbyRules(SurvivorLimit, KillerLimit, RequiredReadyPlayerCount);
	}
}

void AMainMenuGameMode::EvaluateCountdownState()
{
	if (CanStartMatchCountdown())
	{
		StartCountdown();
		return;
	}

	CancelCountdown();
}

void AMainMenuGameMode::StartCountdown()
{
	ALobbyGameState* LobbyGameState = GetLobbyGameState();
	if (!IsValid(LobbyGameState) || LobbyGameState->GetLobbyPhase() == ELobbyPhase::Countdown)
	{
		return;
	}

	LobbyGameState->SetLobbyPhase(ELobbyPhase::Countdown);
	LobbyGameState->SetCountdownRemainingTime(CountdownDuration);

	GetWorldTimerManager().ClearTimer(CountdownTimerHandle);
	GetWorldTimerManager().SetTimer(CountdownTimerHandle, this, &AMainMenuGameMode::HandleCountdownTick, 1.0f, true);
}

void AMainMenuGameMode::CancelCountdown()
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

void AMainMenuGameMode::HandleCountdownTick()
{
	ALobbyGameState* LobbyGameState = GetLobbyGameState();
	if (!IsValid(LobbyGameState))
	{
		GetWorldTimerManager().ClearTimer(CountdownTimerHandle);
		return;
	}

	if (!CanStartMatchCountdown())
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

void AMainMenuGameMode::TravelToMatchGameLevel()
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

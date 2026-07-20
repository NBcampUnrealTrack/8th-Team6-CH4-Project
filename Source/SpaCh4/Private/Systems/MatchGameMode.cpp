#include "Systems/MatchGameMode.h"

#include "EngineUtils.h"
#include "SkeletalMeshAttributes.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerState.h"
#include "Gameplay/Escape/SPHatchManagerSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Player/LDPlayerState.h"
#include "Player/SPPlayerController.h"
#include "Systems/SPEOSSessionSubsystem.h"
#include "UObject/ConstructorHelpers.h"

namespace MatchGameModeStationIds
{
	const FName StationA(TEXT("A"));
	const FName StationB(TEXT("B"));
}

AMatchGameMode::AMatchGameMode()
{
	GameStateClass = AMatchGameState::StaticClass();
	PlayerStateClass = ALDPlayerState::StaticClass();

	static ConstructorHelpers::FClassFinder<APlayerController> PlayerControllerBPClass(
		TEXT("/Game/Characters/BluePrint/Killer/BP_SPPlayerController"));
	if (PlayerControllerBPClass.Succeeded())
	{
		PlayerControllerClass = PlayerControllerBPClass.Class;
	}
	else
	{
		PlayerControllerClass = ASPPlayerController::StaticClass();
	}
}

void AMatchGameMode::BeginPlay()
{
	Super::BeginPlay();

	InitializeMatchState();

	if (bAutoStartMatch)
	{
		StartMatch();
	}
}

UClass* AMatchGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	if (!IsValid(InController)) return Super::GetDefaultPawnClassForController_Implementation(InController);
    
	ALDPlayerState* PlayerState = InController->GetPlayerState<ALDPlayerState>();
	if (!PlayerState) return Super::GetDefaultPawnClassForController_Implementation(InController);

	// 1. 로그로 역할 정보 직접 확인
	ELobbyPlayerRole CurrentRole = PlayerState->GetPlayerRole();
	UE_LOG(LogTemp, Warning, TEXT("GetDefaultPawnClass: Controller %s has Role %d"), *InController->GetName(), (int32)CurrentRole);

	// 2. 분기 처리
	if (CurrentRole == ELobbyPlayerRole::Killer)
	{
		if (IsValid(KillerPawnClass)) return KillerPawnClass;
	}
	else if (CurrentRole == ELobbyPlayerRole::Survivor)
	{
		if (IsValid(SurvivorPawnClass)) return SurvivorPawnClass;
	}

	return Super::GetDefaultPawnClassForController_Implementation(InController);
}

AActor* AMatchGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	if (IsValid(Player) == false)
	{
		return Super::ChoosePlayerStart_Implementation(Player);
	}
	ALDPlayerState* PlayerState = Player->GetPlayerState<ALDPlayerState>();
	if (PlayerState == nullptr)
	{
		return Super::ChoosePlayerStart_Implementation(Player);
	}
	// 플레이어의 역활을 저장하고
	FName PlayerRole = NAME_None;
	if (PlayerState->GetPlayerRole() == ELobbyPlayerRole::Survivor)
	{
		PlayerRole = "Survivor";
	}
	if (PlayerState->GetPlayerRole() == ELobbyPlayerRole::Killer)
	{
		PlayerRole = "Killer";
	}
	
	
	// 레밸 내 PlayerStart중 동일 태그를 가지는 PlayerStart를 가져옴
	TArray<APlayerStart*> CandidateStarts;
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		APlayerStart* PlayerStart = *It;
		if (PlayerStart && PlayerStart->PlayerStartTag == PlayerRole)
		{
			CandidateStarts.Add(PlayerStart);
		}
	}

	// 각 태그를 가진 PlayerStart중 랜덤 위치에 스폰되도록 함. 
	if (CandidateStarts.Num() > 0)
	{
		const int32 Index = FMath::RandRange(0, CandidateStarts.Num() - 1);
		return CandidateStarts[Index];
	}
	// 아니라면 그냥 PlayerStart에서?
	return Super::ChoosePlayerStart_Implementation(Player);
}

void AMatchGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	if (ALDPlayerState* LDPlayerState = NewPlayer->GetPlayerState<ALDPlayerState>())
	{
		// 수정: Role -> AssignedRole (이름을 바꾸면 충돌이 해결됩니다)
		ELobbyPlayerRole AssignedRole = (GetNumPlayers() == 1) ? ELobbyPlayerRole::Killer : ELobbyPlayerRole::Survivor;
        
		LDPlayerState->SetPlayerRole(AssignedRole);
        
		// RestartPlayer 호출 (캐릭터 스폰 강제 갱신)
		RestartPlayer(NewPlayer);

		UE_LOG(LogTemp, Warning, TEXT("PostLogin: Player %s assigned as %s"), 
			*NewPlayer->GetName(), 
			(AssignedRole == ELobbyPlayerRole::Killer) ? TEXT("KILLER") : TEXT("SURVIVOR"));
	}
    
	RegisterMatchPlayer(NewPlayer);
}

void AMatchGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
	RegisterMatchPlayer(NewPlayer);
}

void AMatchGameMode::Logout(AController* Exiting)
{
	if (AMatchGameState* MatchGameState = GetMatchGameState())
	{
		if (const ALDPlayerState* LDPlayerState = Exiting ? Exiting->GetPlayerState<ALDPlayerState>() : nullptr)
		{
			MatchGameState->SetMatchPlayerConnected(LDPlayerState->GetPlayerId(), false);
		}
	}

	Super::Logout(Exiting);
}

void AMatchGameMode::StartMatch()
{
	if (!HasAuthority())
	{
		return;
	}

	// 게임스테이트 초기화
	InitializeMatchState();

	AMatchGameState* MatchGameState = GetMatchGameState();
	if (!IsValid(MatchGameState))
	{
		return;
	}

	for (APlayerState* PlayerState : MatchGameState->PlayerArray)
	{
		if (ALDPlayerState* LDPlayerState = Cast<ALDPlayerState>(PlayerState))
		{
			LDPlayerState->ResetMatchStats();
		}
	}
	
	// 게임 상태 초기화
	MatchGameState->SetMatchResult(EMatchResult::None);
	MatchGameState->SetMatchPhase(EMatchPhase::Playing);
	
	// 남은 시간 타이머 실행.
	GetWorldTimerManager().ClearTimer(MatchTimerHandle);
	GetWorldTimerManager().SetTimer(MatchTimerHandle, this, &AMatchGameMode::HandleMatchTimerTick, 1.0f, true);
}

void AMatchGameMode::FinishMatch(EMatchResult Result)
{
	if (!HasAuthority())
	{
		return;
	}

	AMatchGameState* MatchGameState = GetMatchGameState();
	if (!IsValid(MatchGameState) || MatchGameState->GetMatchPhase() == EMatchPhase::Ended)
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(MatchTimerHandle);
	GetWorldTimerManager().ClearTimer(SessionShutdownTimerHandle);

	MatchGameState->SetMatchResult(Result);
	MatchGameState->SetMatchPhase(EMatchPhase::Ended);

	if (SessionShutdownDelay > 0.0f)
	{
		GetWorldTimerManager().SetTimer(SessionShutdownTimerHandle, this, &AMatchGameMode::HandleSessionShutdownTimer, SessionShutdownDelay, false);
	}
	else
	{
		HandleSessionShutdownTimer();
	}
}

bool AMatchGameMode::AddDeliveredValue(FName StationId, int32 Value)
{
	if (!HasAuthority() || Value <= 0)
	{
		return false;
	}

	AMatchGameState* MatchGameState = GetMatchGameState();
	if (!IsValid(MatchGameState) || MatchGameState->GetMatchPhase() == EMatchPhase::Ended)
	{
		return false;
	}

	if (StationId != MatchGameModeStationIds::StationA && StationId != MatchGameModeStationIds::StationB)
	{
		return false;
	}

	const int32 PreviousStationValue = MatchGameState->GetDeliveryStationValue(StationId);
	MatchGameState->AddDeliveredValue(StationId, Value);
	
	// 점수 추가에 성공하면 상태 새로고침.
	const bool bWasDelivered = MatchGameState->GetDeliveryStationValue(StationId) > PreviousStationValue;
	RefreshEscapeConditions();
	return bWasDelivered;
}

// 아이템 점수 가져오기
int32 AMatchGameMode::GetCollectibleValue(ECollectibleType Type) const
{
	const UBalanceData* ActiveBalanceData = GetActiveBalanceData();
	return IsValid(ActiveBalanceData) ? ActiveBalanceData->GetCollectibleValue(Type) : 0;
}

bool AMatchGameMode::CanActivateEscapeGates() const
{
	const AMatchGameState* MatchGameState = GetMatchGameState();
	return IsValid(MatchGameState) && MatchGameState->CanActivateEscapeGates();
}

bool AMatchGameMode::CanSpawnHatch() const
{
	const AMatchGameState* MatchGameState = GetMatchGameState();
	return IsValid(MatchGameState) && MatchGameState->CanSpawnHatch();
}

void AMatchGameMode::RegisterSurvivorEscaped(AController* SurvivorController)
{
	RegisterSurvivorEscapedByPlayerState(
		SurvivorController ? SurvivorController->GetPlayerState<ALDPlayerState>() : nullptr);
}

void AMatchGameMode::RegisterSurvivorEscapedByPlayerState(ALDPlayerState* SurvivorPlayerState)
{
	if (!HasAuthority())
	{
		return;
	}

	AMatchGameState* MatchGameState = GetMatchGameState();
	if (!IsValid(MatchGameState) || MatchGameState->GetMatchPhase() == EMatchPhase::Ended)
	{
		return;
	}

	if (!ApplySurvivorState(SurvivorPlayerState, ESurvivorState::Escaped))
	{
		return;
	}

	if (IsValid(SurvivorPlayerState))
	{
		SurvivorPlayerState->RecordEscaped(ESurvivorEscapeMethod::None);
	}

	RefreshEscapeConditions();
	TryFinishMatchFromSurvivorCounts();
}

void AMatchGameMode::RegisterSurvivorKilled(AController* SurvivorController)
{
	RegisterSurvivorKilledByPlayerState(
		SurvivorController ? SurvivorController->GetPlayerState<ALDPlayerState>() : nullptr);
}

void AMatchGameMode::RegisterSurvivorKilledByPlayerState(ALDPlayerState* SurvivorPlayerState)
{
	if (!HasAuthority())
	{
		return;
	}

	AMatchGameState* MatchGameState = GetMatchGameState();
	if (!IsValid(MatchGameState) || MatchGameState->GetMatchPhase() == EMatchPhase::Ended)
	{
		return;
	}

	if (!ApplySurvivorState(SurvivorPlayerState, ESurvivorState::Dead))
	{
		return;
	}
	
	if (IsValid(SurvivorPlayerState))
	{
		SurvivorPlayerState->RecordKilled();
	}

	for (APlayerState* PlayerState : MatchGameState->PlayerArray)
	{
		ALDPlayerState* KillerPlayerState = Cast<ALDPlayerState>(PlayerState);
		if (IsValid(KillerPlayerState) && KillerPlayerState->GetPlayerRole() == ELobbyPlayerRole::Killer)
		{
			KillerPlayerState->RecordKillerElimination();
			break;
		}
	}

	RefreshEscapeConditions();
	TryFinishMatchFromSurvivorCounts();
}

void AMatchGameMode::RegisterSurvivorStateChanged(AController* SurvivorController, const ESurvivorState NewSurvivorState)
{
	RegisterSurvivorStateChangedByPlayerState(
		SurvivorController ? SurvivorController->GetPlayerState<ALDPlayerState>() : nullptr,
		NewSurvivorState);
}

void AMatchGameMode::RegisterSurvivorStateChangedByPlayerState(
	ALDPlayerState* SurvivorPlayerState,
	const ESurvivorState NewSurvivorState)
{
	ApplySurvivorState(SurvivorPlayerState, NewSurvivorState);
}

bool AMatchGameMode::ApplySurvivorState(ALDPlayerState* SurvivorPlayerState, const ESurvivorState NewSurvivorState)
{
	if (!HasAuthority() || !IsValid(SurvivorPlayerState))
	{
		return false;
	}

	AMatchGameState* MatchGameState = GetMatchGameState();
	if (SurvivorPlayerState->GetPlayerRole() != ELobbyPlayerRole::Survivor
		|| SurvivorPlayerState->GetPlayerId() == INDEX_NONE
		|| !IsValid(MatchGameState))
	{
		return false;
	}

	FMatchPlayerState MatchPlayerState;
	if (!MatchGameState->GetMatchPlayerState(SurvivorPlayerState->GetPlayerId(), MatchPlayerState)
		|| MatchPlayerState.PlayerRole != ELobbyPlayerRole::Survivor)
	{
		return false;
	}
	if (MatchPlayerState.SurvivorState == NewSurvivorState)
	{
		return false;
	}

	MatchGameState->SetSurvivorState(SurvivorPlayerState->GetPlayerId(), NewSurvivorState);
	return true;
}

EMatchResult AMatchGameMode::CalculateMatchResult() const
{
	const AMatchGameState* MatchGameState = GetMatchGameState();
	if (!IsValid(MatchGameState))
	{
		return EMatchResult::None;
	}

	const int32 EscapedSurvivorCount = MatchGameState->GetEscapedSurvivorCount();
	if (EscapedSurvivorCount >= 3)
	{
		return EMatchResult::SurvivorPerfectWin;
	}

	if (EscapedSurvivorCount == 2)
	{
		return EMatchResult::SurvivorWin;
	}

	if (EscapedSurvivorCount == 1)
	{
		return EMatchResult::SurvivorMinorWin;
	}
	
	// 탈출인원이 하나도없으면 
	return EMatchResult::KillerWin;
}


AMatchGameState* AMatchGameMode::GetMatchGameState() const
{
	return GetGameState<AMatchGameState>();
}

const UBalanceData* AMatchGameMode::GetActiveBalanceData() const
{
	if (IsValid(BalanceData))
	{
		return BalanceData;
	}

	return GetDefault<UBalanceData>();
}

void AMatchGameMode::InitializeMatchState()
{
	AMatchGameState* MatchGameState = GetMatchGameState();
	const UBalanceData* ActiveBalanceData = GetActiveBalanceData();
	if (!IsValid(MatchGameState) || !IsValid(ActiveBalanceData))
	{
		return;
	}

	MatchGameState->ApplyBalanceSettings(
		ActiveBalanceData->MatchTimeLimit,
		ActiveBalanceData->DeliveryStationATargetValue,
		ActiveBalanceData->DeliveryStationBTargetValue,
		ActiveBalanceData->GetTotalRequiredValue());
	RegisterExistingMatchPlayersFromPlayerStates();

	MatchGameState->SetMatchPhase(EMatchPhase::Waiting);
	MatchGameState->SetMatchResult(EMatchResult::None);
	MatchGameState->SetCanActivateEscapeGates(false);
	MatchGameState->SetCanSpawnHatch(false);

	if (USPHatchManagerSubsystem* HatchManager = GetWorld()->GetSubsystem<USPHatchManagerSubsystem>())
	{
		HatchManager->ResetHatches();
	}
}

void AMatchGameMode::RegisterMatchPlayer(AController* PlayerController)
{
	AMatchGameState* MatchGameState = GetMatchGameState();
	const ALDPlayerState* LDPlayerState = PlayerController ? PlayerController->GetPlayerState<ALDPlayerState>() : nullptr;
	if (!IsValid(MatchGameState) || !IsValid(LDPlayerState))
	{
		return;
	}

	FString PlayerName = LDPlayerState->GetPlayerName();
	// 이름을 가져오지 못할때 임시이름
	if (PlayerName.IsEmpty())
	{
		PlayerName = FString::Printf(TEXT("Player_%d"), LDPlayerState->GetPlayerId());
	}

	MatchGameState->RegisterMatchPlayer(LDPlayerState->GetPlayerId(), PlayerName, LDPlayerState->GetPlayerRole());
}

void AMatchGameMode::RegisterExistingMatchPlayersFromPlayerStates()
{
	AMatchGameState* MatchGameState = GetMatchGameState();
	if (!IsValid(MatchGameState))
	{
		return;
	}

	for (APlayerState* PlayerState : MatchGameState->PlayerArray)
	{
		const ALDPlayerState* LDPlayerState = Cast<ALDPlayerState>(PlayerState);
		if (!IsValid(LDPlayerState))
		{
			continue;
		}

		FString PlayerName = LDPlayerState->GetPlayerName();
		if (PlayerName.IsEmpty())
		{
			PlayerName = FString::Printf(TEXT("Player_%d"), LDPlayerState->GetPlayerId());
		}

		MatchGameState->RegisterMatchPlayer(LDPlayerState->GetPlayerId(), PlayerName, LDPlayerState->GetPlayerRole());
	}
}
void AMatchGameMode::HandleMatchTimerTick()
{
	AMatchGameState* MatchGameState = GetMatchGameState();
	if (!IsValid(MatchGameState) || MatchGameState->GetMatchPhase() == EMatchPhase::Ended)
	{
		GetWorldTimerManager().ClearTimer(MatchTimerHandle);
		return;
	}

	const float NewRemainingTime = MatchGameState->GetRemainingTime() - 1.0f;
	MatchGameState->SetRemainingTime(NewRemainingTime);

	if (NewRemainingTime <= 0.0f)
	{
		const EMatchResult TimeoutResult = MatchGameState->GetEscapedSurvivorCount() > 0 ? CalculateMatchResult() : EMatchResult::KillerWin;
		FinishMatch(TimeoutResult);
	}
}

void AMatchGameMode::HandleSessionShutdownTimer()
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (USPEOSSessionSubsystem* SessionSubsystem = GameInstance->GetSubsystem<USPEOSSessionSubsystem>())
		{
			SessionSubsystem->EndSession();
		}
	}
}

void AMatchGameMode::RefreshEscapeConditions()
{
	AMatchGameState* MatchGameState = GetMatchGameState();
	const UBalanceData* ActiveBalanceData = GetActiveBalanceData();
	if (!IsValid(MatchGameState) || !IsValid(ActiveBalanceData))
	{
		return;
	}

	const bool bStationAComplete = MatchGameState->GetDeliveryStationValue(MatchGameModeStationIds::StationA) >= ActiveBalanceData->DeliveryStationATargetValue;
	const bool bStationBComplete = MatchGameState->GetDeliveryStationValue(MatchGameModeStationIds::StationB) >= ActiveBalanceData->DeliveryStationBTargetValue;
	const bool bShouldActivateEscapeGates = bStationAComplete && bStationBComplete;
	// 두 납품소의 점수합이 목표이상이면 탈출구 오픈하도록
	MatchGameState->SetCanActivateEscapeGates(bShouldActivateEscapeGates);
	
	if (bShouldActivateEscapeGates && MatchGameState->GetMatchPhase() == EMatchPhase::Playing)
	{
		MatchGameState->SetMatchPhase(EMatchPhase::EscapeActive);
	}
	// 한명남음 + 점수50%이상 채움 -> 개구멍 오픈
	const bool bOnlyOneSurvivorLeft = MatchGameState->GetAliveSurvivorCount() == 1;
	const bool bEnoughDeliveryForHatch = MatchGameState->GetTotalDeliveredValue() >= ActiveBalanceData->GetHatchRequiredDeliveredValue();
	const bool bShouldActivateHatch = bOnlyOneSurvivorLeft && bEnoughDeliveryForHatch;
	MatchGameState->SetCanSpawnHatch(bShouldActivateHatch);

	if (bShouldActivateHatch)
	{
		if (USPHatchManagerSubsystem* HatchManager = GetWorld()->GetSubsystem<USPHatchManagerSubsystem>())
		{
			HatchManager->RequestRandomActivation();
		}
	}
}

void AMatchGameMode::TryFinishMatchFromSurvivorCounts()
{
	const AMatchGameState* MatchGameState = GetMatchGameState();
	// 한명이라도 생존해있다면 결과내지않음.
	if (!IsValid(MatchGameState) || MatchGameState->GetAliveSurvivorCount() > 0)
	{
		return;
	}

	FinishMatch(CalculateNoSurvivorLeftResult());
}

// 생존자가 없을때(사망이든, 탈출이든) 승리 확인
EMatchResult AMatchGameMode::CalculateNoSurvivorLeftResult() const
{
	const AMatchGameState* MatchGameState = GetMatchGameState();
	if (!IsValid(MatchGameState))
	{
		return EMatchResult::None;
	}

	// 한명이라도 탈출한 경우에, 탈출한 수에 따른 생존자 승리판정 진행.
	if (MatchGameState->GetEscapedSurvivorCount() > 0)
	{
		return CalculateMatchResult();
	}
	// 탈출한 생존자가 없을때, 진행률에 따른 살인마의 승리 판정.
	return MatchGameState->GetDeliveryProgress() < 0.5f ? EMatchResult::KillerPerfectWin : EMatchResult::KillerWin;
}

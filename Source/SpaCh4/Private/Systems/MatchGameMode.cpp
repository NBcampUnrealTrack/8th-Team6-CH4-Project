#include "Systems/MatchGameMode.h"

#include "Engine/World.h"
#include "TimerManager.h"

namespace MatchGameModeStationIds
{
	const FName StationA(TEXT("A"));
	const FName StationB(TEXT("B"));
}

AMatchGameMode::AMatchGameMode()
{
	GameStateClass = AMatchGameState::StaticClass();
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

	MatchGameState->SetMatchResult(Result);
	MatchGameState->SetMatchPhase(EMatchPhase::Ended);
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

void AMatchGameMode::RegisterSurvivorEscaped(FName SurvivorId)
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

	const int32 NewAliveSurvivorCount = FMath::Max(0, MatchGameState->GetAliveSurvivorCount() - 1);
	const int32 NewEscapedSurvivorCount = MatchGameState->GetEscapedSurvivorCount() + 1;
	MatchGameState->SetSurvivorCounts(NewAliveSurvivorCount, NewEscapedSurvivorCount, MatchGameState->GetKilledSurvivorCount());
	RegisterSurvivorStateChanged(SurvivorId, EMatchSurvivorState::Escaped);

	RefreshEscapeConditions();
	TryFinishMatchFromSurvivorCounts();
}

void AMatchGameMode::RegisterSurvivorKilled(FName SurvivorId)
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

	const int32 NewAliveSurvivorCount = FMath::Max(0, MatchGameState->GetAliveSurvivorCount() - 1);
	const int32 NewKilledSurvivorCount = MatchGameState->GetKilledSurvivorCount() + 1;
	MatchGameState->SetSurvivorCounts(NewAliveSurvivorCount, MatchGameState->GetEscapedSurvivorCount(), NewKilledSurvivorCount);
	RegisterSurvivorStateChanged(SurvivorId, EMatchSurvivorState::Dead);

	RefreshEscapeConditions();
	TryFinishMatchFromSurvivorCounts();
}

void AMatchGameMode::RegisterSurvivorStateChanged(FName SurvivorId, EMatchSurvivorState NewSurvivorState)
{
	if (!HasAuthority())
	{
		return;
	}

	AMatchGameState* MatchGameState = GetMatchGameState();
	if (!IsValid(MatchGameState))
	{
		return;
	}

	MatchGameState->SetSurvivorState(SurvivorId, NewSurvivorState);
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
		ActiveBalanceData->GetTotalRequiredValue(),
		ActiveBalanceData->InitialSurvivorCount);

	MatchGameState->SetMatchPhase(EMatchPhase::Waiting);
	MatchGameState->SetMatchResult(EMatchResult::None);
	MatchGameState->SetCanActivateEscapeGates(false);
	MatchGameState->SetCanSpawnHatch(false);
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
	MatchGameState->SetCanSpawnHatch(bOnlyOneSurvivorLeft && bEnoughDeliveryForHatch);
}

void AMatchGameMode::TryFinishMatchFromSurvivorCounts()
{
	const AMatchGameState* MatchGameState = GetMatchGameState();
	if (!IsValid(MatchGameState) || MatchGameState->GetAliveSurvivorCount() > 0)
	{
		return;
	}

	FinishMatch(CalculateNoSurvivorLeftResult());
}

// 생존자거 없을때, 살인마의 승리 확인(완벽승리 / 일반승리)
EMatchResult AMatchGameMode::CalculateNoSurvivorLeftResult() const
{
	const AMatchGameState* MatchGameState = GetMatchGameState();
	if (!IsValid(MatchGameState))
	{
		return EMatchResult::None;
	}

	if (MatchGameState->GetEscapedSurvivorCount() > 0)
	{
		return CalculateMatchResult();
	}

	return MatchGameState->GetDeliveryProgress() < 0.5f ? EMatchResult::KillerPerfectWin : EMatchResult::KillerWin;
}

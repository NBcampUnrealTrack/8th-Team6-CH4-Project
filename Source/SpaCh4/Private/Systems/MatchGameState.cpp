#include "Systems/MatchGameState.h"

#include "Net/UnrealNetwork.h"

namespace MatchGameStateStationIds
{
	const FName StationA(TEXT("A"));
	const FName StationB(TEXT("B"));
}

AMatchGameState::AMatchGameState()
{
	bReplicates = true;
}

void AMatchGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AMatchGameState, MatchPhase);
	DOREPLIFETIME(AMatchGameState, MatchResult);
	DOREPLIFETIME(AMatchGameState, RemainingTime);
	DOREPLIFETIME(AMatchGameState, TimeLimit);
	DOREPLIFETIME(AMatchGameState, DeliveryStationAValue);
	DOREPLIFETIME(AMatchGameState, DeliveryStationBValue);
	DOREPLIFETIME(AMatchGameState, DeliveryStationATargetValue);
	DOREPLIFETIME(AMatchGameState, DeliveryStationBTargetValue);
	DOREPLIFETIME(AMatchGameState, TotalRequiredValue);
	DOREPLIFETIME(AMatchGameState, bCanActivateEscapeGates);
	DOREPLIFETIME(AMatchGameState, bCanSpawnHatch);
	DOREPLIFETIME(AMatchGameState, AliveSurvivorCount);
	DOREPLIFETIME(AMatchGameState, EscapedSurvivorCount);
	DOREPLIFETIME(AMatchGameState, KilledSurvivorCount);
	DOREPLIFETIME(AMatchGameState, SurvivorStates);
}

EMatchPhase AMatchGameState::GetMatchPhase() const
{
	return MatchPhase;
}

EMatchResult AMatchGameState::GetMatchResult() const
{
	return MatchResult;
}

float AMatchGameState::GetRemainingTime() const
{
	return RemainingTime;
}

float AMatchGameState::GetTimeLimit() const
{
	return TimeLimit;
}

int32 AMatchGameState::GetDeliveryStationValue(FName StationId) const
{
	if (StationId == MatchGameStateStationIds::StationA)
	{
		return DeliveryStationAValue;
	}

	if (StationId == MatchGameStateStationIds::StationB)
	{
		return DeliveryStationBValue;
	}

	return 0;
}

int32 AMatchGameState::GetDeliveryStationTargetValue(FName StationId) const
{
	if (StationId == MatchGameStateStationIds::StationA)
	{
		return DeliveryStationATargetValue;
	}

	if (StationId == MatchGameStateStationIds::StationB)
	{
		return DeliveryStationBTargetValue;
	}

	return 0;
}

int32 AMatchGameState::GetTotalDeliveredValue() const
{
	return DeliveryStationAValue + DeliveryStationBValue;
}

int32 AMatchGameState::GetTotalRequiredValue() const
{
	return TotalRequiredValue;
}

float AMatchGameState::GetDeliveryProgress() const
{
	if (TotalRequiredValue <= 0)
	{
		return 0.0f;
	}

	return FMath::Clamp(static_cast<float>(GetTotalDeliveredValue()) / static_cast<float>(TotalRequiredValue), 0.0f, 1.0f);
}

bool AMatchGameState::CanActivateEscapeGates() const
{
	return bCanActivateEscapeGates;
}

bool AMatchGameState::CanSpawnHatch() const
{
	return bCanSpawnHatch;
}

int32 AMatchGameState::GetAliveSurvivorCount() const
{
	return AliveSurvivorCount;
}

int32 AMatchGameState::GetEscapedSurvivorCount() const
{
	return EscapedSurvivorCount;
}

int32 AMatchGameState::GetKilledSurvivorCount() const
{
	return KilledSurvivorCount;
}

TArray<FSurvivorMatchState> AMatchGameState::GetSurvivorStates() const
{
	return SurvivorStates;
}

void AMatchGameState::ApplyBalanceSettings(float NewTimeLimit, int32 NewStationATargetValue, int32 NewStationBTargetValue, int32 NewTotalRequiredValue, int32 NewInitialSurvivorCount)
{
	TimeLimit = FMath::Max(0.0f, NewTimeLimit);
	RemainingTime = TimeLimit;
	DeliveryStationAValue = 0;
	DeliveryStationBValue = 0;
	DeliveryStationATargetValue = FMath::Max(0, NewStationATargetValue);
	DeliveryStationBTargetValue = FMath::Max(0, NewStationBTargetValue);
	TotalRequiredValue = FMath::Max(0, NewTotalRequiredValue);
	AliveSurvivorCount = FMath::Max(0, NewInitialSurvivorCount);
	EscapedSurvivorCount = 0;
	KilledSurvivorCount = 0;

	SurvivorStates.Reset();
	// 임시로 Survivor_1, 2, 3이 등록되도록 함.
	// 닉네임이나 고유 아이디 지정 및 등록한다면 이 부분을 분리해야함.
	for (int32 SurvivorIndex = 0; SurvivorIndex < AliveSurvivorCount; ++SurvivorIndex)
	{
		FSurvivorMatchState SurvivorMatchState;
		SurvivorMatchState.SurvivorId = FName(*FString::Printf(TEXT("Survivor_%d"), SurvivorIndex + 1));
		SurvivorStates.Add(SurvivorMatchState);
	}

	OnMatchTimerChanged.Broadcast(RemainingTime);
	BroadcastDeliveryProgressChanged();
	OnSurvivorCountChanged.Broadcast(AliveSurvivorCount, EscapedSurvivorCount, KilledSurvivorCount);
}

void AMatchGameState::SetMatchPhase(EMatchPhase NewMatchPhase)
{
	if (MatchPhase == NewMatchPhase)
	{
		return;
	}

	MatchPhase = NewMatchPhase;
	OnMatchPhaseChanged.Broadcast(MatchPhase);
}

void AMatchGameState::SetMatchResult(EMatchResult NewMatchResult)
{
	if (MatchResult == NewMatchResult)
	{
		return;
	}

	MatchResult = NewMatchResult;
	OnMatchResultChanged.Broadcast(MatchResult);
}

void AMatchGameState::SetRemainingTime(float NewRemainingTime)
{
	const float ClampedRemainingTime = FMath::Clamp(NewRemainingTime, 0.0f, TimeLimit);
	if (FMath::IsNearlyEqual(RemainingTime, ClampedRemainingTime))
	{
		return;
	}

	RemainingTime = ClampedRemainingTime;
	OnMatchTimerChanged.Broadcast(RemainingTime);
}

void AMatchGameState::AddDeliveredValue(FName StationId, int32 Value)
{
	if (Value <= 0)
	{
		return;
	}

	if (StationId == MatchGameStateStationIds::StationA)
	{
		DeliveryStationAValue = FMath::Clamp(DeliveryStationAValue + Value, 0, DeliveryStationATargetValue);
		BroadcastDeliveryProgressChanged();
		return;
	}

	if (StationId == MatchGameStateStationIds::StationB)
	{
		DeliveryStationBValue = FMath::Clamp(DeliveryStationBValue + Value, 0, DeliveryStationBTargetValue);
		BroadcastDeliveryProgressChanged();
	}
}

void AMatchGameState::SetCanActivateEscapeGates(bool bNewCanActivateEscapeGates)
{
	if (bCanActivateEscapeGates == bNewCanActivateEscapeGates)
	{
		return;
	}

	bCanActivateEscapeGates = bNewCanActivateEscapeGates;
	OnEscapeGateAvailabilityChanged.Broadcast(bCanActivateEscapeGates);
}

void AMatchGameState::SetCanSpawnHatch(bool bNewCanSpawnHatch)
{
	if (bCanSpawnHatch == bNewCanSpawnHatch)
	{
		return;
	}

	bCanSpawnHatch = bNewCanSpawnHatch;
	OnHatchAvailabilityChanged.Broadcast(bCanSpawnHatch);
}

void AMatchGameState::SetSurvivorCounts(int32 NewAliveSurvivorCount, int32 NewEscapedSurvivorCount, int32 NewKilledSurvivorCount)
{
	AliveSurvivorCount = FMath::Max(0, NewAliveSurvivorCount);
	EscapedSurvivorCount = FMath::Max(0, NewEscapedSurvivorCount);
	KilledSurvivorCount = FMath::Max(0, NewKilledSurvivorCount);

	OnSurvivorCountChanged.Broadcast(AliveSurvivorCount, EscapedSurvivorCount, KilledSurvivorCount);
}

void AMatchGameState::SetSurvivorState(FName SurvivorId, EMatchSurvivorState NewSurvivorState)
{
	if (SurvivorId.IsNone())
	{
		return;
	}

	for (FSurvivorMatchState& SurvivorState : SurvivorStates)
	{
		if (SurvivorState.SurvivorId == SurvivorId)
		{
			SurvivorState.SurvivorState = NewSurvivorState;
			OnSurvivorStateChanged.Broadcast(SurvivorId, NewSurvivorState);
			return;
		}
	}
	
	// 호출한 생존자가 없다면 새로 등록함
	FSurvivorMatchState NewMatchState;
	NewMatchState.SurvivorId = SurvivorId;
	NewMatchState.SurvivorState = NewSurvivorState;
	SurvivorStates.Add(NewMatchState);
	OnSurvivorStateChanged.Broadcast(SurvivorId, NewSurvivorState);
}

void AMatchGameState::OnRep_MatchPhase()
{
	OnMatchPhaseChanged.Broadcast(MatchPhase);
}

void AMatchGameState::OnRep_MatchResult()
{
	OnMatchResultChanged.Broadcast(MatchResult);
}

void AMatchGameState::OnRep_RemainingTime()
{
	OnMatchTimerChanged.Broadcast(RemainingTime);
}

void AMatchGameState::OnRep_DeliveryProgress()
{
	BroadcastDeliveryProgressChanged();
}

void AMatchGameState::OnRep_EscapeGateAvailability()
{
	OnEscapeGateAvailabilityChanged.Broadcast(bCanActivateEscapeGates);
}

void AMatchGameState::OnRep_HatchAvailability()
{
	
	OnHatchAvailabilityChanged.Broadcast(bCanSpawnHatch);
}

void AMatchGameState::OnRep_SurvivorCounts()
{
	OnSurvivorCountChanged.Broadcast(AliveSurvivorCount, EscapedSurvivorCount, KilledSurvivorCount);
}

void AMatchGameState::OnRep_SurvivorStates()
{
	for (const FSurvivorMatchState& SurvivorState : SurvivorStates)
	{
		OnSurvivorStateChanged.Broadcast(SurvivorState.SurvivorId, SurvivorState.SurvivorState);
	}
}

void AMatchGameState::BroadcastDeliveryProgressChanged()
{
	OnDeliveryProgressChanged.Broadcast(DeliveryStationAValue, DeliveryStationBValue, GetTotalDeliveredValue(), GetDeliveryProgress());
}

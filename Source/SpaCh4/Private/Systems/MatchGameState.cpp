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
	DOREPLIFETIME(AMatchGameState, MatchPlayers);
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

int32 AMatchGameState::GetDeliveryStationAValue() const
{
		return DeliveryStationAValue;
}

int32 AMatchGameState::GetDeliveryStationBValue() const
{
		return DeliveryStationBValue;
}

int32 AMatchGameState::GetDeliveryStationTargetValueByID(FName StationId) const
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

int32 AMatchGameState::GetDeliveryStationTargetValue() const
{
	return DeliveryStationAValue;
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
	TArray<FSurvivorMatchState> SurvivorStates;
	for (const FMatchPlayerState& MatchPlayer : MatchPlayers)
	{
		if (MatchPlayer.PlayerRole != ELobbyPlayerRole::Survivor)
		{
			continue;
		}

		FSurvivorMatchState SurvivorMatchState;
		SurvivorMatchState.SurvivorId = FName(*MatchPlayer.Nickname);
		SurvivorMatchState.SurvivorState = MatchPlayer.SurvivorState;
		SurvivorStates.Add(SurvivorMatchState);
	}

	return SurvivorStates;
}

TArray<FMatchPlayerState> AMatchGameState::GetMatchPlayers() const
{
	return MatchPlayers;
}

bool AMatchGameState::GetMatchPlayerState(FName Nickname, FMatchPlayerState& OutPlayerState) const
{
	if (Nickname.IsNone())
	{
		return false;
	}

	for (const FMatchPlayerState& MatchPlayer : MatchPlayers)
	{
		if (FName(*MatchPlayer.Nickname) == Nickname)
		{
			OutPlayerState = MatchPlayer;
			return true;
		}
	}

	return false;
}

void AMatchGameState::ApplyBalanceSettings(float NewTimeLimit, int32 NewStationATargetValue, int32 NewStationBTargetValue, int32 NewTotalRequiredValue)
{
	TimeLimit = FMath::Max(0.0f, NewTimeLimit);
	RemainingTime = TimeLimit;
	DeliveryStationAValue = 0;
	DeliveryStationBValue = 0;
	DeliveryStationATargetValue = FMath::Max(0, NewStationATargetValue);
	DeliveryStationBTargetValue = FMath::Max(0, NewStationBTargetValue);
	TotalRequiredValue = FMath::Max(0, NewTotalRequiredValue);
	AliveSurvivorCount = 0;
	EscapedSurvivorCount = 0;
	KilledSurvivorCount = 0;

	MatchPlayers.Reset();
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

void AMatchGameState::RegisterMatchPlayer(int32 PlayerId, const FString& Nickname, ELobbyPlayerRole PlayerRole)
{
	const FString NormalizedNickname = Nickname.IsEmpty() ? FString::Printf(TEXT("Player_%d"), PlayerId) : Nickname;

	for (FMatchPlayerState& MatchPlayer : MatchPlayers)
	{
		// 중복 접속체크
		const bool bSamePlayerId = PlayerId != INDEX_NONE && MatchPlayer.PlayerId == PlayerId;
		// 중복 닉네임 체크
		const bool bSameNickname = !NormalizedNickname.IsEmpty() && MatchPlayer.Nickname == NormalizedNickname;
		if (bSamePlayerId || bSameNickname)
		{
			MatchPlayer.PlayerId = PlayerId;
			MatchPlayer.Nickname = NormalizedNickname;
			MatchPlayer.PlayerRole = PlayerRole;
			MatchPlayer.bIsConnected = true;
			BroadcastMatchPlayersChanged();
			return;
		}
	}

	FMatchPlayerState NewMatchPlayer;
	NewMatchPlayer.PlayerId = PlayerId;
	NewMatchPlayer.Nickname = NormalizedNickname;
	NewMatchPlayer.PlayerRole = PlayerRole;
	NewMatchPlayer.bIsConnected = true;
	MatchPlayers.Add(NewMatchPlayer);
	BroadcastMatchPlayersChanged();
}

void AMatchGameState::SetMatchPlayerConnected(FName Nickname, bool bNewIsConnected)
{
	if (Nickname.IsNone())
	{
		return;
	}

	for (FMatchPlayerState& MatchPlayer : MatchPlayers)
	{
		if (FName(*MatchPlayer.Nickname) == Nickname)
		{
			MatchPlayer.bIsConnected = bNewIsConnected;
			BroadcastMatchPlayersChanged();
			return;
		}
	}
}

void AMatchGameState::SetSurvivorState(FName SurvivorId, ESurvivorState NewSurvivorState)
{
	if (SurvivorId.IsNone())
	{
		return;
	}

	for (FMatchPlayerState& MatchPlayer : MatchPlayers)
	{
		if (MatchPlayer.PlayerRole == ELobbyPlayerRole::Survivor && FName(*MatchPlayer.Nickname) == SurvivorId)
		{
			const UEnum* CharStateEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("ESurvivorState"), true);
			if (CharStateEnum)
			{
				FString EnumToString = CharStateEnum->GetNameStringByValue((int64)NewSurvivorState);
				UE_LOG(LogTemp, Warning, TEXT("Set Player %s State to %s"), *SurvivorId.ToString(), *EnumToString);
			}
			MatchPlayer.SurvivorState = NewSurvivorState;
			BroadcastMatchPlayersChanged();
			return;
		}
	}
}
void AMatchGameState::OnRep_MatchPhase()
{
	UE_LOG(LogTemp, Warning, TEXT("MatchPhase()"));
	OnMatchPhaseChanged.Broadcast(MatchPhase);
}

void AMatchGameState::OnRep_MatchResult()
{			
	const UEnum* CharStateEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EMatchResult"), true);
	if (CharStateEnum)
	{
		FString EnumToString = CharStateEnum->GetNameStringByValue((int64)MatchResult);
		UE_LOG(LogTemp, Warning, TEXT("MatchResult id %s"), *EnumToString);
	}
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
	UE_LOG(LogTemp, Warning, TEXT("EscapeGateAvailability()"));
	OnEscapeGateAvailabilityChanged.Broadcast(bCanActivateEscapeGates);
}

void AMatchGameState::OnRep_HatchAvailability()
{
	UE_LOG(LogTemp, Warning, TEXT("HatchAvailability()"));
	OnHatchAvailabilityChanged.Broadcast(bCanSpawnHatch);
}

void AMatchGameState::OnRep_SurvivorCounts()
{
	UE_LOG(LogTemp, Warning, TEXT("SurvivorCounts(%d, %d, %d)"), AliveSurvivorCount, EscapedSurvivorCount, KilledSurvivorCount);
	OnSurvivorCountChanged.Broadcast(AliveSurvivorCount, EscapedSurvivorCount, KilledSurvivorCount);
}

void AMatchGameState::OnRep_MatchPlayers()
{
	BroadcastMatchPlayersChanged();
}

void AMatchGameState::BroadcastDeliveryProgressChanged()
{
	UE_LOG(LogTemp, Warning, TEXT("DeliveryProgress(%d, %d, %d, %f)"),DeliveryStationAValue, DeliveryStationBValue, GetTotalDeliveredValue(), GetDeliveryProgress());
	OnDeliveryProgressChanged.Broadcast(DeliveryStationAValue, DeliveryStationBValue, GetTotalDeliveredValue(), GetDeliveryProgress());
}

void AMatchGameState::BroadcastMatchPlayersChanged()
{
	RefreshSurvivorCountsFromMatchPlayers();
	OnMatchPlayersChanged.Broadcast();

	for (const FMatchPlayerState& MatchPlayer : MatchPlayers)
	{
		if (MatchPlayer.PlayerRole == ELobbyPlayerRole::Survivor)
		{
			OnSurvivorStateChanged.Broadcast(FName(*MatchPlayer.Nickname), MatchPlayer.SurvivorState);
		}
	}
}

void AMatchGameState::RefreshSurvivorCountsFromMatchPlayers()
{
	int32 NewAliveSurvivorCount = 0;
	int32 NewEscapedSurvivorCount = 0;
	int32 NewKilledSurvivorCount = 0;

	for (const FMatchPlayerState& MatchPlayer : MatchPlayers)
	{
		if (MatchPlayer.PlayerRole != ELobbyPlayerRole::Survivor)
		{
			continue;
		}

		if (MatchPlayer.SurvivorState == ESurvivorState::Escaped)
		{
			++NewEscapedSurvivorCount;
		}
		else if (MatchPlayer.SurvivorState == ESurvivorState::Dead)
		{
			++NewKilledSurvivorCount;
		}
		else
		{
			++NewAliveSurvivorCount;
		}
	}

	const bool bCountsChanged = AliveSurvivorCount != NewAliveSurvivorCount
		|| EscapedSurvivorCount != NewEscapedSurvivorCount
		|| KilledSurvivorCount != NewKilledSurvivorCount;

	AliveSurvivorCount = NewAliveSurvivorCount;
	EscapedSurvivorCount = NewEscapedSurvivorCount;
	KilledSurvivorCount = NewKilledSurvivorCount;

	if (bCountsChanged)
	{
		OnSurvivorCountChanged.Broadcast(AliveSurvivorCount, EscapedSurvivorCount, KilledSurvivorCount);
	}
}

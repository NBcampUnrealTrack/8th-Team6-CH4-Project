#include "Systems/MatchGameState.h"

#include "Components/SPBackgroundMusicComponent.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

namespace MatchGameStateStationIds
{
	const FName StationA(TEXT("A"));
	const FName StationB(TEXT("B"));
}

AMatchGameState::AMatchGameState()
{
	bReplicates = true;
	BackgroundMusicComponent = CreateDefaultSubobject<USPBackgroundMusicComponent>(TEXT("BackgroundMusicComponent"));
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

int32 AMatchGameState::GetPlayerCagedCount(const int32 SurvivorPlayerId) const
{
	for (const FMatchPlayerState& MatchPlayer : MatchPlayers)
	{
		if (MatchPlayer.PlayerId == SurvivorPlayerId)
		{
			return MatchPlayer.CagedCount;
		}
	}
	return 0;
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
		SurvivorMatchState.PlayerId = MatchPlayer.PlayerId;
		SurvivorMatchState.Nickname = FName(*MatchPlayer.Nickname);
		SurvivorMatchState.CagedCount = MatchPlayer.CagedCount;
		SurvivorMatchState.SurvivorState = MatchPlayer.SurvivorState;
		SurvivorStates.Add(SurvivorMatchState);
	}

	return SurvivorStates;
}

TArray<FMatchPlayerState> AMatchGameState::GetMatchPlayers() const
{
	return MatchPlayers;
}

bool AMatchGameState::GetMatchPlayerState(const int32 PlayerId, FMatchPlayerState& OutPlayerState) const
{
	if (PlayerId == INDEX_NONE)
	{
		return false;
	}

	for (const FMatchPlayerState& MatchPlayer : MatchPlayers)
	{
		if (MatchPlayer.PlayerId == PlayerId)
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
	if (PlayerId == INDEX_NONE)
	{
		return;
	}

	const FString NormalizedNickname = Nickname.IsEmpty() ? FString::Printf(TEXT("Player_%d"), PlayerId) : Nickname;

	for (FMatchPlayerState& MatchPlayer : MatchPlayers)
	{
		if (MatchPlayer.PlayerId == PlayerId)
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

void AMatchGameState::SetMatchPlayerConnected(const int32 PlayerId, const bool bNewIsConnected)
{
	if (PlayerId == INDEX_NONE)
	{
		return;
	}

	for (FMatchPlayerState& MatchPlayer : MatchPlayers)
	{
		if (MatchPlayer.PlayerId == PlayerId)
		{
			MatchPlayer.bIsConnected = bNewIsConnected;
			BroadcastMatchPlayersChanged();
			return;
		}
	}
}

void AMatchGameState::SetSurvivorState(const int32 SurvivorPlayerId, const ESurvivorState NewSurvivorState)
{
	if (SurvivorPlayerId == INDEX_NONE)
	{
		return;
	}

	for (FMatchPlayerState& MatchPlayer : MatchPlayers)
	{
		if (MatchPlayer.PlayerRole == ELobbyPlayerRole::Survivor && MatchPlayer.PlayerId == SurvivorPlayerId)
		{
			const UEnum* CharStateEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("ESurvivorState"), true);
			if (CharStateEnum)
			{
				FString EnumToString = CharStateEnum->GetNameStringByValue((int64)NewSurvivorState);
				UE_LOG(LogTemp, Warning, TEXT("Set Player %d State to %s"), SurvivorPlayerId, *EnumToString);
			}
			MatchPlayer.SurvivorState = NewSurvivorState;
			// 감금당할떄 카운트 1 증가.
			if (NewSurvivorState == ESurvivorState::Caged)
				MatchPlayer.CagedCount+=1;
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
			OnSurvivorStateChanged.Broadcast(MatchPlayer.PlayerId, MatchPlayer.Nickname, MatchPlayer.SurvivorState);
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

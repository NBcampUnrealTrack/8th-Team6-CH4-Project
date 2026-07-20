#include "Player/LDPlayerState.h"

#include "Net/UnrealNetwork.h"

ALDPlayerState::ALDPlayerState()
{
	PlayerRole = ELobbyPlayerRole::None;
}

ELobbyPlayerRole ALDPlayerState::GetPlayerRole() const
{
	return PlayerRole;
}

void ALDPlayerState::SetPlayerRole(ELobbyPlayerRole NewRole)
{
	if (HasAuthority())
	{
		PlayerRole = NewRole;
	}
}

bool ALDPlayerState::SetPlayerLoadout(const FSPPlayerLoadout& NewLoadout)
{
	if (!HasAuthority() || !SPPlayerLoadout::IsComplete(NewLoadout))
	{
		return false;
	}

	PlayerLoadout = NewLoadout;
	OnPlayerLoadoutChanged.Broadcast(PlayerLoadout);
	ForceNetUpdate();
	return true;
}

const FSPPlayerLoadout& ALDPlayerState::GetPlayerLoadout() const
{
	return PlayerLoadout;
}

bool ALDPlayerState::IsLoadoutConfiguredForRole(const ELobbyPlayerRole PlayerRoleType) const
{
	if (PlayerRoleType == ELobbyPlayerRole::Survivor)
	{
		return SPPlayerLoadout::IsSurvivorLoadoutCompleteOnlyItem(PlayerLoadout);
		//return SPPlayerLoadout::IsSurvivorLoadoutComplete(PlayerLoadout);
	}
	if (PlayerRoleType == ELobbyPlayerRole::Killer)
	{
		return true;
		//return SPPlayerLoadout::IsKillerLoadoutComplete(PlayerLoadout);
	}
	return false;
}

FSurvivorMatchStats ALDPlayerState::GetSurvivorMatchStats() const
{
	return SurvivorStats;
}

FKillerMatchStats ALDPlayerState::GetKillerMatchStats() const
{
	return KillerStats;
}

void ALDPlayerState::ResetMatchStats()
{
	if (!HasAuthority())
	{
		return;
	}

	SurvivorStats = FSurvivorMatchStats();
	KillerStats = FKillerMatchStats();
	NotifyMatchStatsChanged();
}

void ALDPlayerState::RecordDelivery(const int32 AcceptedValue)
{
	if (!HasAuthority() || AcceptedValue <= 0)
	{
		return;
	}

	++SurvivorStats.SuccessfulDeliveryCount;
	SurvivorStats.DeliveredValue += AcceptedValue;
	NotifyMatchStatsChanged();
}

void ALDPlayerState::RecordSuccessfulSelfHeal()
{
	if (!HasAuthority())
	{
		return;
	}

	++SurvivorStats.SelfHealCount;
	NotifyMatchStatsChanged();
}

void ALDPlayerState::RecordCaged()
{
	if (!HasAuthority())
	{
		return;
	}

	++SurvivorStats.CagedCount;
	NotifyMatchStatsChanged();
}

void ALDPlayerState::RecordCageRescue()
{
	if (!HasAuthority())
	{
		return;
	}

	++SurvivorStats.CageRescueCount;
	NotifyMatchStatsChanged();
}

void ALDPlayerState::RecordEscaped(const ESurvivorEscapeMethod EscapeMethod)
{
	if (!HasAuthority() || SurvivorStats.bEscaped || SurvivorStats.bKilled)
	{
		return;
	}

	SurvivorStats.bEscaped = true;
	SurvivorStats.EscapeMethod = EscapeMethod;
	NotifyMatchStatsChanged();
}

void ALDPlayerState::RecordKilled()
{
	if (!HasAuthority() || SurvivorStats.bEscaped || SurvivorStats.bKilled)
	{
		return;
	}

	SurvivorStats.bKilled = true;
	NotifyMatchStatsChanged();
}

void ALDPlayerState::RecordKillerHit(const bool bCausedDown)
{
	if (!HasAuthority())
	{
		return;
	}

	++KillerStats.ValidHitCount;
	KillerStats.DownCount += bCausedDown ? 1 : 0;
	NotifyMatchStatsChanged();
}

void ALDPlayerState::RecordKillerCage()
{
	if (!HasAuthority())
	{
		return;
	}

	++KillerStats.CageCount;
	NotifyMatchStatsChanged();
}

void ALDPlayerState::RecordKillerElimination()
{
	if (!HasAuthority())
	{
		return;
	}

	++KillerStats.KilledSurvivorCount;
	NotifyMatchStatsChanged();
}

void ALDPlayerState::OnRep_PlayerLoadout()
{
	OnPlayerLoadoutChanged.Broadcast(PlayerLoadout);
}

void ALDPlayerState::OnRep_MatchStats()
{
	OnMatchStatsChanged.Broadcast();
}

void ALDPlayerState::NotifyMatchStatsChanged()
{
	OnMatchStatsChanged.Broadcast();
	ForceNetUpdate();
}

void ALDPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ALDPlayerState, PlayerRole);
	DOREPLIFETIME(ALDPlayerState, PlayerLoadout);
	DOREPLIFETIME(ALDPlayerState, SurvivorStats);
	DOREPLIFETIME(ALDPlayerState, KillerStats);
}

void ALDPlayerState::CopyProperties(APlayerState* NewPlayerState)
{
	Super::CopyProperties(NewPlayerState);
	if (ALDPlayerState* NewLDPlayerState = Cast<ALDPlayerState>(NewPlayerState))
	{
		// Stat정보는 초기화되어야 하므로 CopyProperties에서 제외함.
		NewLDPlayerState->SetPlayerName(GetPlayerName());
		NewLDPlayerState->PlayerRole = PlayerRole;
		NewLDPlayerState->PlayerLoadout = PlayerLoadout;
	}
}

void ALDPlayerState::OverrideWith(APlayerState* OldPlayerState)
{
	Super::OverrideWith(OldPlayerState);

	if (const ALDPlayerState* OldLDPlayerState = Cast<ALDPlayerState>(OldPlayerState))
	{
		SetPlayerName(OldLDPlayerState->GetPlayerName());
		PlayerRole = OldLDPlayerState->PlayerRole;
		PlayerLoadout = OldLDPlayerState->PlayerLoadout;
	}
}

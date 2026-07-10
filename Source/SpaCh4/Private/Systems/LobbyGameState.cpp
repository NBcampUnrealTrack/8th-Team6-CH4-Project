#include "Systems/LobbyGameState.h"

#include "Net/UnrealNetwork.h"

ALobbyGameState::ALobbyGameState()
{
	bReplicates = true;
}

void ALobbyGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ALobbyGameState, LobbyPlayers);
	DOREPLIFETIME(ALobbyGameState, SurvivorLimit);
	DOREPLIFETIME(ALobbyGameState, KillerLimit);
	DOREPLIFETIME(ALobbyGameState, RequiredReadyPlayerCount);
	DOREPLIFETIME(ALobbyGameState, LobbyPhase);
	DOREPLIFETIME(ALobbyGameState, CountdownRemainingTime);
}

TArray<FLobbyPlayerInfo> ALobbyGameState::GetLobbyPlayers() const
{
	return LobbyPlayers;
}

bool ALobbyGameState::GetLobbyPlayerInfo(int32 PlayerId, FLobbyPlayerInfo& OutPlayerInfo) const
{
	for (const FLobbyPlayerInfo& PlayerInfo : LobbyPlayers)
	{
		if (PlayerInfo.PlayerId == PlayerId)
		{
			OutPlayerInfo = PlayerInfo;
			return true;
		}
	}

	return false;
}

int32 ALobbyGameState::GetSurvivorCount() const
{
	int32 SurvivorCount = 0;
	for (const FLobbyPlayerInfo& PlayerInfo : LobbyPlayers)
	{
		if (PlayerInfo.SelectedRole == ELobbyPlayerRole::Survivor)
		{
			++SurvivorCount;
		}
	}

	return SurvivorCount;
}

int32 ALobbyGameState::GetKillerCount() const
{
	int32 KillerCount = 0;
	for (const FLobbyPlayerInfo& PlayerInfo : LobbyPlayers)
	{
		if (PlayerInfo.SelectedRole == ELobbyPlayerRole::Killer)
		{
			++KillerCount;
		}
	}

	return KillerCount;
}

int32 ALobbyGameState::GetReadyCount() const
{
	int32 ReadyCount = 0;
	for (const FLobbyPlayerInfo& PlayerInfo : LobbyPlayers)
	{
		if (PlayerInfo.bIsReady)
		{
			++ReadyCount;
		}
	}

	return ReadyCount;
}

int32 ALobbyGameState::GetConnectedPlayerCount() const
{
	return LobbyPlayers.Num();
}

int32 ALobbyGameState::GetRequiredReadyPlayerCount() const
{
	return RequiredReadyPlayerCount;
}

int32 ALobbyGameState::GetSurvivorLimit() const
{
	return SurvivorLimit;
}

int32 ALobbyGameState::GetKillerLimit() const
{
	return KillerLimit;
}

bool ALobbyGameState::CanSelectRole(int32 PlayerId, ELobbyPlayerRole PlayerRole) const
{
	if (PlayerRole == ELobbyPlayerRole::None)
	{
		return true;
	}

	int32 RoleCount = 0;
	for (const FLobbyPlayerInfo& PlayerInfo : LobbyPlayers)
	{
		if (PlayerInfo.SelectedRole == PlayerRole)
		{
			if (PlayerInfo.PlayerId == PlayerId)
			{
				return true;
			}

			++RoleCount;
		}
	}

	if (PlayerRole == ELobbyPlayerRole::Survivor)
	{
		return RoleCount < SurvivorLimit;
	}

	if (PlayerRole == ELobbyPlayerRole::Killer)
	{
		return RoleCount < KillerLimit;
	}

	return false;
}

bool ALobbyGameState::IsRoleFull(ELobbyPlayerRole PlayerRole) const
{
	if (PlayerRole == ELobbyPlayerRole::Survivor)
	{
		return GetSurvivorCount() >= SurvivorLimit;
	}

	if (PlayerRole == ELobbyPlayerRole::Killer)
	{
		return GetKillerCount() >= KillerLimit;
	}

	return false;
}

ELobbyPhase ALobbyGameState::GetLobbyPhase() const
{
	return LobbyPhase;
}

int32 ALobbyGameState::GetCountdownRemainingTime() const
{
	return CountdownRemainingTime;
}

void ALobbyGameState::SetLobbyRules(int32 NewSurvivorLimit, int32 NewKillerLimit, int32 NewRequiredReadyPlayerCount)
{
	SurvivorLimit = FMath::Max(0, NewSurvivorLimit);
	KillerLimit = FMath::Max(0, NewKillerLimit);
	RequiredReadyPlayerCount = FMath::Max(0, NewRequiredReadyPlayerCount);
	BroadcastLobbyPlayersChanged();
}

void ALobbyGameState::AddOrUpdatePlayer(const FLobbyPlayerInfo& PlayerInfo)
{
	for (FLobbyPlayerInfo& ExistingPlayerInfo : LobbyPlayers)
	{
		if (ExistingPlayerInfo.PlayerId == PlayerInfo.PlayerId)
		{
			ExistingPlayerInfo = PlayerInfo;
			BroadcastLobbyPlayersChanged();
			return;
		}
	}

	LobbyPlayers.Add(PlayerInfo);
	BroadcastLobbyPlayersChanged();
}

void ALobbyGameState::RemovePlayer(int32 PlayerId)
{
	const int32 RemovedCount = LobbyPlayers.RemoveAll([PlayerId](const FLobbyPlayerInfo& PlayerInfo)
	{
		return PlayerInfo.PlayerId == PlayerId;
	});

	if (RemovedCount > 0)
	{
		BroadcastLobbyPlayersChanged();
	}
}

bool ALobbyGameState::SetPlayerNickname(int32 PlayerId, const FString& NewNickname)
{
	for (FLobbyPlayerInfo& PlayerInfo : LobbyPlayers)
	{
		if (PlayerInfo.PlayerId == PlayerId)
		{
			PlayerInfo.Nickname = NewNickname;
			BroadcastLobbyPlayersChanged();
			return true;
		}
	}

	return false;
}

bool ALobbyGameState::SetPlayerRole(int32 PlayerId, ELobbyPlayerRole NewRole)
{
	if (!CanSelectRole(PlayerId, NewRole))
	{
		return false;
	}

	for (FLobbyPlayerInfo& PlayerInfo : LobbyPlayers)
	{
		if (PlayerInfo.PlayerId == PlayerId)
		{
			PlayerInfo.SelectedRole = NewRole;
			BroadcastLobbyPlayersChanged();
			return true;
		}
	}

	return false;
}

bool ALobbyGameState::SetPlayerReady(int32 PlayerId, bool bNewReady)
{
	
	for (FLobbyPlayerInfo& PlayerInfo : LobbyPlayers)
	{
		if (PlayerInfo.PlayerId == PlayerId)
		{
			PlayerInfo.bIsReady = bNewReady;
			BroadcastLobbyPlayersChanged();
			return true;
		}
	}

	return false;
}

void ALobbyGameState::SetLobbyPhase(ELobbyPhase NewLobbyPhase)
{
	if (LobbyPhase == NewLobbyPhase)
	{
		return;
	}

	LobbyPhase = NewLobbyPhase;
	OnLobbyPhaseChanged.Broadcast(LobbyPhase);
}

void ALobbyGameState::SetCountdownRemainingTime(int32 NewCountdownRemainingTime)
{
	const int32 ClampedCountdownRemainingTime = FMath::Max(0, NewCountdownRemainingTime);
	if (CountdownRemainingTime == ClampedCountdownRemainingTime)
	{
		return;
	}

	CountdownRemainingTime = ClampedCountdownRemainingTime;
	OnLobbyCountdownChanged.Broadcast(CountdownRemainingTime);
}

void ALobbyGameState::OnRep_LobbyPlayers()
{
	BroadcastLobbyPlayersChanged();
}

void ALobbyGameState::OnRep_LobbyPhase()
{
	OnLobbyPhaseChanged.Broadcast(LobbyPhase);
}

void ALobbyGameState::OnRep_CountdownRemainingTime()
{
	OnLobbyCountdownChanged.Broadcast(CountdownRemainingTime);
}

void ALobbyGameState::BroadcastLobbyPlayersChanged()
{
	OnLobbyPlayersChanged.Broadcast();
	OnLobbyRoleCountChanged.Broadcast(GetSurvivorCount(), GetKillerCount());
	OnLobbyReadyCountChanged.Broadcast(GetReadyCount(), RequiredReadyPlayerCount);
}

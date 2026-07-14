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
		return SPPlayerLoadout::IsSurvivorLoadoutComplete(PlayerLoadout);
	}
	if (PlayerRoleType == ELobbyPlayerRole::Killer)
	{
		return SPPlayerLoadout::IsKillerLoadoutComplete(PlayerLoadout);
	}
	return false;
}

void ALDPlayerState::OnRep_PlayerLoadout()
{
	OnPlayerLoadoutChanged.Broadcast(PlayerLoadout);
}

void ALDPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ALDPlayerState, PlayerRole);
	DOREPLIFETIME(ALDPlayerState, PlayerLoadout);
}

void ALDPlayerState::CopyProperties(APlayerState* NewPlayerState)
{
	Super::CopyProperties(NewPlayerState);
	if (ALDPlayerState* NewLDPlayerState = Cast<ALDPlayerState>(NewPlayerState))
	{
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

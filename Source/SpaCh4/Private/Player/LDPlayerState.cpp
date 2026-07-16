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

void ALDPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ALDPlayerState, PlayerRole);
}

void ALDPlayerState::CopyProperties(APlayerState* NewPlayerState)
{
	Super::CopyProperties(NewPlayerState);
	if (ALDPlayerState* NewLDPlayerState = Cast<ALDPlayerState>(NewPlayerState))
	{
		NewLDPlayerState->SetPlayerName(GetPlayerName());
		NewLDPlayerState->PlayerRole = PlayerRole;
	}
}

void ALDPlayerState::OverrideWith(APlayerState* OldPlayerState)
{
	Super::OverrideWith(OldPlayerState);

	if (const ALDPlayerState* OldLDPlayerState = Cast<ALDPlayerState>(OldPlayerState))
	{
		SetPlayerName(OldLDPlayerState->GetPlayerName());
		PlayerRole = OldLDPlayerState->PlayerRole;
	}
}

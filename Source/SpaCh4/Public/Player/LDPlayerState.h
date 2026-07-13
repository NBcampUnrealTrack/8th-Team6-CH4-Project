// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "Player/SPPlayerLoadout.h"
#include "Systems/LobbyGameState.h"
#include "LDPlayerState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSPPlayerStateLoadoutChangedSignature, const FSPPlayerLoadout&, Loadout);

/**
 * 
 */
UCLASS()
class SPACH4_API ALDPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	ALDPlayerState();
#pragma region LobbyRoles
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Lobby|Role")
	ELobbyPlayerRole GetPlayerRole() const;

	UFUNCTION(BlueprintCallable, Category = "Lobby|Role")
	void SetPlayerRole(ELobbyPlayerRole NewRole);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Loadout")
	bool SetPlayerLoadout(const FSPPlayerLoadout& NewLoadout);

	UFUNCTION(BlueprintPure, Category = "Loadout")
	const FSPPlayerLoadout& GetPlayerLoadout() const;

	UFUNCTION(BlueprintPure, Category = "Loadout")
	bool IsLoadoutConfiguredForRole(ELobbyPlayerRole PlayerRoleType) const;

	UPROPERTY(BlueprintAssignable, Category = "Loadout")
	FSPPlayerStateLoadoutChangedSignature OnPlayerLoadoutChanged;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void CopyProperties(APlayerState* NewPlayerState) override;
	virtual void OverrideWith(APlayerState* OldPlayerState) override;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Lobby|Role")
	ELobbyPlayerRole PlayerRole = ELobbyPlayerRole::None;
#pragma endregion

private:
	UFUNCTION()
	void OnRep_PlayerLoadout();

	UPROPERTY(ReplicatedUsing = OnRep_PlayerLoadout, BlueprintReadOnly, Category = "Loadout", meta = (AllowPrivateAccess = true))
	FSPPlayerLoadout PlayerLoadout;
	
};

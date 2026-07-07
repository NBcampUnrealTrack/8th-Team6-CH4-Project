// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "Systems/LobbyGameState.h"
#include "LDPlayerState.generated.h"

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

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void CopyProperties(APlayerState* NewPlayerState) override;
	virtual void OverrideWith(APlayerState* OldPlayerState) override;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Lobby|Role")
	ELobbyPlayerRole PlayerRole = ELobbyPlayerRole::None;
#pragma endregion
	
};

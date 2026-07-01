#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Systems/LobbyGameState.h"
#include "LobbyPlayerController.generated.h"

UCLASS()
class SPACH4_API ALobbyPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Lobby|Player")
	void LobbySetNickname(const FString& NewNickname);

	UFUNCTION(BlueprintCallable, Category = "Lobby|Role")
	void LobbySetRole(ELobbyPlayerRole NewRole);

	UFUNCTION(BlueprintCallable, Category = "Lobby|Ready")
	void LobbySetReady(bool bNewReady);

	UFUNCTION(BlueprintCallable, Category = "Lobby|Player")
	void LobbySubmitSettings(const FString& NewNickname, ELobbyPlayerRole NewRole, bool bNewReady);

protected:
	virtual void BeginPlay() override;
	
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lobby|UI")
	TSubclassOf<class UTestLobbyUIWidget> LobbyUIWidgetClass;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lobby|UI")
	TObjectPtr<class UTestLobbyUIWidget> LobbyUIWidgetInstance;
private:
	UFUNCTION(Server, Reliable)
	void ServerLobbySetNickname(const FString& NewNickname);

	UFUNCTION(Server, Reliable)
	void ServerLobbySetRole(ELobbyPlayerRole NewRole);

	UFUNCTION(Server, Reliable)
	void ServerLobbySetReady(bool bNewReady);

	UFUNCTION(Server, Reliable)
	void ServerLobbySubmitSettings(const FString& NewNickname, ELobbyPlayerRole NewRole, bool bNewReady);
	
	UFUNCTION()
	void OnLobbyPlayersChanged();
	UFUNCTION()
	void OnLobbyRoleCountChanged(int32 SurvivorCount, int32 KillerCount);
	UFUNCTION()
	void OnLobbyReadyCountChanged(int32 ReadyCount, int32 RequireReadyCount);
	UFUNCTION()
	void OnLobbyPhaseChanged(ELobbyPhase NewPhase);
	UFUNCTION()
	void OnLobbyCountdownChanged(int32 CountdownRemainingTime);
};

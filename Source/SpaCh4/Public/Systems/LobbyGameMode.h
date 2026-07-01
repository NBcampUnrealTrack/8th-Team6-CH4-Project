#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "LobbyGameState.h"
#include "LobbyGameMode.generated.h"

class APlayerController;
class APlayerState;

UCLASS()
class SPACH4_API ALobbyGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ALobbyGameMode();

	virtual void BeginPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Lobby|Player")
	bool SetLobbyPlayerNickname(APlayerController* PlayerController, const FString& NewNickname);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Lobby|Role")
	bool SetLobbyPlayerRole(APlayerController* PlayerController, ELobbyPlayerRole NewRole);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Lobby|Ready")
	bool SetLobbyPlayerReady(APlayerController* PlayerController, bool bNewReady);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Lobby|Player")
	bool SubmitLobbyPlayerSettings(APlayerController* PlayerController, const FString& NewNickname, ELobbyPlayerRole NewRole, bool bNewReady);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Lobby|Ready")
	bool CanStartCountdown() const;

protected:
	ALobbyGameState* GetLobbyGameState() const;
	int32 GetPlayerId(const APlayerController* PlayerController) const;
	FString BuildDefaultNickname(const APlayerController* PlayerController) const;
	void RegisterLobbyPlayer(APlayerController* PlayerController);
	void ApplyLobbyInfoToPlayerState(APlayerController* PlayerController, const FLobbyPlayerInfo& PlayerInfo) const;
	void EvaluateCountdownState();
	void StartCountdown();
	void CancelCountdown();
	void HandleCountdownTick();
	void TravelToMatchGameLevel();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lobby|Rules")
	int32 SurvivorLimit = 3;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lobby|Rules")
	int32 KillerLimit = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lobby|Rules")
	int32 RequiredReadyPlayerCount = 4;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lobby|Countdown")
	int32 CountdownDuration = 5;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lobby|Travel")
	FString MatchLevelUrl = TEXT("/Game/Developers/qkrwl/Collections/Maps/TestLevel");

	FTimerHandle CountdownTimerHandle;
};

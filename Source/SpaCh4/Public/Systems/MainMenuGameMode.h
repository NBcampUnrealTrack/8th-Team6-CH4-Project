#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Systems/LobbyGameState.h"
#include "MainMenuGameMode.generated.h"

class APlayerController;
class UBalanceData;

UCLASS()
class SPACH4_API AMainMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AMainMenuGameMode();

	virtual void BeginPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	bool SubmitMatchmakingRole(APlayerController* PlayerController, const FString& Nickname, ELobbyPlayerRole SelectedRole, FString& OutStatusMessage);
	bool CanStartMatchCountdown() const;

protected:
	ALobbyGameState* GetLobbyGameState() const;
	int32 GetPlayerId(const APlayerController* PlayerController) const;
	FString BuildDefaultNickname(const APlayerController* PlayerController) const;
	void RegisterMainMenuPlayer(APlayerController* PlayerController);
	void ApplyLobbyInfoToPlayerState(APlayerController* PlayerController, const FLobbyPlayerInfo& PlayerInfo) const;
	const UBalanceData* GetActiveBalanceData() const;
	void ApplyBalanceRules();
	void EvaluateCountdownState();
	void StartCountdown();
	void CancelCountdown();
	void HandleCountdownTick();
	void TravelToMatchGameLevel();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Balance")
	TObjectPtr<UBalanceData> BalanceData;
	
	// BalanceData가 없을 경우 기본값
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Matchmaking|Rules")
	int32 SurvivorLimit = 3;

	// BalanceData가 없을 경우 기본값
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Matchmaking|Rules")
	int32 KillerLimit = 1;

	// BalanceData가 없을 경우 기본값
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Matchmaking|Rules")
	int32 RequiredReadyPlayerCount = 4;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Matchmaking|Countdown")
	int32 CountdownDuration = 5;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Matchmaking|Travel")
	FString MatchLevelUrl = TEXT("/Game/Maps/L_MatchLevel");

	FTimerHandle CountdownTimerHandle;
};

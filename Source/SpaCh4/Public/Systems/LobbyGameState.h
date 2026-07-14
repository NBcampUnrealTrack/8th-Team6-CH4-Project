#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "LobbyGameState.generated.h"

UENUM(BlueprintType)
enum class ELobbyPlayerRole : uint8
{
	None,
	Survivor,
	Killer
};

UENUM(BlueprintType)
enum class ELobbyPhase : uint8
{
	WaitingForPlayers,
	Countdown,
	Traveling
};


USTRUCT(BlueprintType)
struct FSurvivorScore
{
	GENERATED_BODY()
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SurvivorScore")
	int32 DeliveryCount = 0;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SurvivorScore")
	int32 DeliveryScore = 0;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SurvivorScore")
	int32 InjuredCount = 0;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SurvivorScore")
	int32 MedkitUseCount = 0;
		
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SurvivorScore")
	int32 CagedCount = 0;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SurvivorScore")
	int32 CageRescueCount = 0;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SurvivorScore")
	bool bEscaped = false;
	
	
};

USTRUCT(BlueprintType)
struct FKillerScore
{
	GENERATED_BODY()
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "KillerScore")
	int32 AttackSuccessCount = 0;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "KillerScore")
	int32 CageCount = 0;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "KillerScore")
	int32 KilledSurvivorCount = 0;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "KillerScore")
	int32 InterruptDeliverCount = 0;
};


USTRUCT(BlueprintType)
struct FLobbyPlayerInfo
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lobby|Player")
	int32 PlayerId = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lobby|Player")
	FString Nickname;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lobby|Player")
	ELobbyPlayerRole SelectedRole = ELobbyPlayerRole::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lobby|Player")
	bool bIsReady = false;
	
	// 생존자 점수정보
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lobby|Player")
	FSurvivorScore SurvivorScore;
	
	// 살인마 점수정보
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lobby|Player")
	FKillerScore KillerScore;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLobbyPlayersChangedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLobbyRoleCountChangedSignature, int32, SurvivorCount, int32, KillerCount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLobbyReadyCountChangedSignature, int32, ReadyCount, int32, RequiredReadyCount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLobbyPhaseChangedSignature, ELobbyPhase, LobbyPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLobbyCountdownChangedSignature, int32, CountdownRemainingTime);

UCLASS()
class SPACH4_API ALobbyGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	ALobbyGameState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Lobby|Player")
	TArray<FLobbyPlayerInfo> GetLobbyPlayers() const;

	UFUNCTION(BlueprintCallable, Category = "Lobby|Player")
	bool GetLobbyPlayerInfo(int32 PlayerId, FLobbyPlayerInfo& OutPlayerInfo) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Lobby|Role")
	int32 GetSurvivorCount() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Lobby|Role")
	int32 GetKillerCount() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Lobby|Ready")
	int32 GetReadyCount() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Lobby|Player")
	int32 GetConnectedPlayerCount() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Lobby|Ready")
	int32 GetRequiredReadyPlayerCount() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Lobby|Role")
	int32 GetSurvivorLimit() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Lobby|Role")
	int32 GetKillerLimit() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Lobby|Role")
	bool CanSelectRole(int32 PlayerId, ELobbyPlayerRole PlayerRole) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Lobby|Role")
	bool IsRoleFull(ELobbyPlayerRole PlayerRole) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Lobby|Phase")
	ELobbyPhase GetLobbyPhase() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Lobby|Countdown")
	int32 GetCountdownRemainingTime() const;

	void SetLobbyRules(int32 NewSurvivorLimit, int32 NewKillerLimit, int32 NewRequiredReadyPlayerCount);
	void AddOrUpdatePlayer(const FLobbyPlayerInfo& PlayerInfo);
	void RemovePlayer(int32 PlayerId);
	bool SetPlayerNickname(int32 PlayerId, const FString& NewNickname);
	bool SetPlayerRole(int32 PlayerId, ELobbyPlayerRole NewRole);
	bool SetPlayerReady(int32 PlayerId, bool bNewReady);
	void SetLobbyPhase(ELobbyPhase NewLobbyPhase);
	void SetCountdownRemainingTime(int32 NewCountdownRemainingTime);

	UPROPERTY(BlueprintAssignable, Category = "Lobby|Events")
	FOnLobbyPlayersChangedSignature OnLobbyPlayersChanged;

	UPROPERTY(BlueprintAssignable, Category = "Lobby|Events")
	FOnLobbyRoleCountChangedSignature OnLobbyRoleCountChanged;

	UPROPERTY(BlueprintAssignable, Category = "Lobby|Events")
	FOnLobbyReadyCountChangedSignature OnLobbyReadyCountChanged;

	UPROPERTY(BlueprintAssignable, Category = "Lobby|Events")
	FOnLobbyPhaseChangedSignature OnLobbyPhaseChanged;

	UPROPERTY(BlueprintAssignable, Category = "Lobby|Events")
	FOnLobbyCountdownChangedSignature OnLobbyCountdownChanged;

protected:
	UFUNCTION()
	void OnRep_LobbyPlayers();

	UFUNCTION()
	void OnRep_LobbyPhase();

	UFUNCTION()
	void OnRep_CountdownRemainingTime();

	void BroadcastLobbyPlayersChanged();

	UPROPERTY(ReplicatedUsing = OnRep_LobbyPlayers, VisibleAnywhere, BlueprintReadOnly, Category = "Lobby|Player")
	TArray<FLobbyPlayerInfo> LobbyPlayers;

	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "Lobby|Role")
	int32 SurvivorLimit = 3;

	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "Lobby|Role")
	int32 KillerLimit = 1;

	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "Lobby|Ready")
	int32 RequiredReadyPlayerCount = 4;

	UPROPERTY(ReplicatedUsing = OnRep_LobbyPhase, VisibleAnywhere, BlueprintReadOnly, Category = "Lobby|Phase")
	ELobbyPhase LobbyPhase = ELobbyPhase::WaitingForPlayers;

	UPROPERTY(ReplicatedUsing = OnRep_CountdownRemainingTime, VisibleAnywhere, BlueprintReadOnly, Category = "Lobby|Countdown")
	int32 CountdownRemainingTime = 0;
};

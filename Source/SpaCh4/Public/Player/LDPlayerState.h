// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "Player/SPPlayerLoadout.h"
#include "Systems/LobbyGameState.h"
#include "LDPlayerState.generated.h"

UENUM(BlueprintType)
enum class ESurvivorEscapeMethod : uint8
{
	None UMETA(DisplayName = "탈출 실패"),
	Gate UMETA(DisplayName = "탈출구"),
	Hatch UMETA(DisplayName = "개구멍")
};

USTRUCT(BlueprintType)
struct FSurvivorMatchStats
{
	GENERATED_BODY()
	
	// 실제 가치가 반영된 납품 완료 횟수
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Survivor|Stats")
	int32 SuccessfulDeliveryCount = 0;

	// 납품소 목표치에 실제 반영된 가치
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Survivor|Stats")
	int32 DeliveredValue = 0;

	// 자가 치료를 완료한 횟수
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Survivor|Stats")
	int32 SelfHealCount = 0;
	
	// 감금 당한 횟수
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Survivor|Stats")
	int32 CagedCount = 0;
	
	// 아군 감금 해방 횟수(구조한 사람이 오름)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Survivor|Stats")
	int32 CageRescueCount = 0;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Survivor|Stats")
	bool bEscaped = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Survivor|Stats")
	bool bKilled = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Survivor|Stats")
	ESurvivorEscapeMethod EscapeMethod = ESurvivorEscapeMethod::None;
};

USTRUCT(BlueprintType)
struct FKillerMatchStats
{
	GENERATED_BODY()
	
	// 성공적인 공격 횟수(피격한 생존자의 상태가 나빠졌을때)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Killer|Stats")
	int32 ValidHitCount = 0;

	// 생존자 다운 성공 횟수
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Killer|Stats")
	int32 DownCount = 0;
	
	// 생존자 감금 성공 횟수
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Killer|Stats")
	int32 CageCount = 0;
	
	// 생존자 처치 카운트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Killer|Stats")
	int32 KilledSurvivorCount = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSPPlayerStateLoadoutChangedSignature, const FSPPlayerLoadout&, Loadout);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSPMatchStatsChangedSignature);

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

#pragma region GameStats
	UFUNCTION(BlueprintPure, Category = "Match|Stats")
	FSurvivorMatchStats GetSurvivorMatchStats() const;

	UFUNCTION(BlueprintPure, Category = "Match|Stats")
	FKillerMatchStats GetKillerMatchStats() const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Match|Stats")
	void ResetMatchStats();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Match|Stats|Survivor")
	void RecordDelivery(int32 AcceptedValue);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Match|Stats|Survivor")
	void RecordSuccessfulSelfHeal();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Match|Stats|Survivor")
	void RecordCaged();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Match|Stats|Survivor")
	void RecordCageRescue();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Match|Stats|Survivor")
	void RecordEscaped(ESurvivorEscapeMethod EscapeMethod);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Match|Stats|Survivor")
	void RecordKilled();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Match|Stats|Killer")
	void RecordKillerHit(bool bCausedDown);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Match|Stats|Killer")
	void RecordKillerCage();

	/* 생존자가 사망했을때 기록*/
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Match|Stats|Killer")
	void RecordKillerElimination();

	UPROPERTY(BlueprintAssignable, Category = "Match|Stats")
	FSPMatchStatsChangedSignature OnMatchStatsChanged;
#pragma endregion

private:
	UFUNCTION()
	void OnRep_PlayerLoadout();

	UFUNCTION()
	void OnRep_MatchStats();

	void NotifyMatchStatsChanged();

	UPROPERTY(ReplicatedUsing = OnRep_PlayerLoadout, BlueprintReadOnly, Category = "Loadout", meta = (AllowPrivateAccess = true))
	FSPPlayerLoadout PlayerLoadout;
	
	UPROPERTY(ReplicatedUsing = OnRep_MatchStats, VisibleAnywhere, BlueprintReadOnly, Category = "Match|Stats|Survivor", meta = (AllowPrivateAccess = true))
	FSurvivorMatchStats SurvivorStats;
	
	UPROPERTY(ReplicatedUsing = OnRep_MatchStats, VisibleAnywhere, BlueprintReadOnly, Category = "Match|Stats|Killer", meta = (AllowPrivateAccess = true))
	FKillerMatchStats KillerStats;
};

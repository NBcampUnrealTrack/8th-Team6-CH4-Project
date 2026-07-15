// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "Player/SPPlayerLoadout.h"
#include "Systems/LobbyGameState.h"
#include "LDPlayerState.generated.h"

USTRUCT(BlueprintType)
struct FSurvivorScore
{
	GENERATED_BODY()
	
	//납품 횟수
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Survivor|Score")
	int32 DeliveryCount = 0;
	
	// 납품 점수 획득(소형자주납품 vs 큰거한방?)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Survivor|Score")
	int32 DeliveryScore = 0;
	
	// 부상 횟수
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Survivor|Score")
	int32 InjuredCount = 0;
	
	// 자신 또는 아군 치유 횟수
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Survivor|Score")
	int32 MedkitUseCount = 0;
	
	// 감금 횟수
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Survivor|Score")
	int32 CagedCount = 0;
	
	// 아군 감금 해방 횟수
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Survivor|Score")
	int32 CageRescueCount = 0;
	
	// 탈출성공
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Survivor|Score")
	bool bEscaped = false;
	
	
};

USTRUCT(BlueprintType)
struct FKillerScore
{
	GENERATED_BODY()
	
	// 성공적인 공격 횟수
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Killer|Score")
	int32 AttackSuccessCount = 0;
	
	// 생존자 감금 성공 횟수
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Killer|Score")
	int32 CageCount = 0;
	
	// 생존자 처치 카운트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Killer|Score")
	int32 KilledSurvivorCount = 0;
	
	//납품 저지 횟수(n범위내에서 생존자의 납품이 취소될때?)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Killer|Score")
	int32 InterruptDeliverCount = 0;
};

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
	
	// 생존자 점수정보
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lobby|Player", meta = (AllowPrivateAccess = true))
	FSurvivorScore SurvivorScore;
	
	// 살인마 점수정보
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lobby|Player", meta = (AllowPrivateAccess = true))
	FKillerScore KillerScore;
};

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Data/BalanceData.h"
#include "MatchGameState.h"
#include "MatchGameMode.generated.h"

UCLASS()
class SPACH4_API AMatchGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AMatchGameMode();

	virtual void BeginPlay() override;
	
#pragma region PlayerClass
	// 플레이어의 기본Pawn을 결정하는 기본함수, PlayerController가 다르게 설정된다면 Login에서 설정해야함.
	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;
	// 플레이어 시작지점을 결정하는 내장함수
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	
protected:
	UPROPERTY(EditDefaultsOnly, Category = "Spawn")
	TSubclassOf<APawn> SurvivorPawnClass;

	UPROPERTY(EditDefaultsOnly, Category = "Spawn")
	TSubclassOf<APawn> KillerPawnClass;
	
#pragma endregion
	
public:
	
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Match|Flow")
	void StartMatch();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Match|Flow")
	void FinishMatch(EMatchResult Result);

	// 아이템 반납, 점수 추가
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Match|Delivery")
	bool AddDeliveredValue(FName StationId, int32 Value);
	
	// 아이템의 가치 확인
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Match|Balance")
	int32 GetCollectibleValue(ECollectibleType Type) const;

	// 탈출구 개방 조건 확인(또는 GameMode의 OnEscapeGateAvailabilityChanged 이벤트)
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Match|Escape")
	bool CanActivateEscapeGates() const;

	// 개구멍(개인탈출구) 생성 가능 여부 반환. (또는 GameState의 OnHatchAvailabilityChanged 이벤트)
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Match|Escape")
	bool CanSpawnHatch() const;

	// 생존자 탈출 등록
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Match|Survivor")
	void RegisterSurvivorEscaped(FName SurvivorId);

	// 생존자 사망 등록
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Match|Survivor")
	void RegisterSurvivorKilled(FName SurvivorId);

	// 생존자 상태 변경
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Match|Survivor")
	void RegisterSurvivorStateChanged(FName SurvivorId, ESurvivorState NewSurvivorState);

	// 결과 확인
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Match|Result")
	EMatchResult CalculateMatchResult() const;

protected:
	AMatchGameState* GetMatchGameState() const;
	const UBalanceData* GetActiveBalanceData() const;

	void InitializeMatchState();
	void HandleMatchTimerTick();
	// 탈출구 조건 체크
	void RefreshEscapeConditions();
	void TryFinishMatchFromSurvivorCounts();
	EMatchResult CalculateNoSurvivorLeftResult() const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Balance")
	TObjectPtr<UBalanceData> BalanceData;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Match")
	bool bAutoStartMatch = true;

	FTimerHandle MatchTimerHandle;
};

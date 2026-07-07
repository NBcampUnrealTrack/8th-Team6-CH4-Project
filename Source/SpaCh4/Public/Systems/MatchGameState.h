#pragma once

#include "CoreMinimal.h"
#include "Characters/Survivor/SurvivorCharacter.h"
#include "GameFramework/GameStateBase.h"
#include "MatchGameState.generated.h"

//게임 루프관련
UENUM(BlueprintType)
enum class EMatchPhase : uint8
{
	Waiting UMETA(DisplayName = "대기중"),
	Playing UMETA(DisplayName = "진행중"),
	EscapeActive UMETA(DisplayName = "탈출구 활성화"),
	Ended UMETA(DisplayName = "종료")
};


UENUM(BlueprintType)
enum class EMatchResult : uint8
{
	None UMETA(DisplayName = "None"),
	SurvivorPerfectWin UMETA(DisplayName = "생존자 완벽한 승리"),
	SurvivorWin UMETA(DisplayName = "생존자 승리"),
	SurvivorMinorWin UMETA(DisplayName = "생존자 초라한 승리"),
	KillerWin UMETA(DisplayName = "살인마 승리"),
	KillerPerfectWin UMETA(DisplayName = "살인마 완벽한 승리")
};


// 생존자의 상태관리를 위한 값
USTRUCT(BlueprintType)
struct FSurvivorMatchState
{
	GENERATED_BODY()
	
	// 생존자 이름(닉네임)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Match|Survivor")
	FName SurvivorId = NAME_None;

	// 생존자 상태
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Match|Survivor")
	ESurvivorState SurvivorState = ESurvivorState::Healthy;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMatchPhaseChangedSignature, EMatchPhase, MatchPhase);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMatchResultChangedSignature, EMatchResult, MatchResult);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMatchTimerChangedSignature, float, RemainingTime);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnDeliveryProgressChangedSignature, int32, StationAValue, int32, StationBValue, int32, TotalDeliveredValue, float, DeliveryProgress);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEscapeGateAvailabilityChangedSignature, bool, bCanActivateEscapeGates);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHatchAvailabilityChangedSignature, bool, bCanSpawnHatch);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnSurvivorCountChangedSignature, int32, AliveSurvivorCount, int32, EscapedSurvivorCount, int32, KilledSurvivorCount);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSurvivorStateChangedSignature, FName, SurvivorId, ESurvivorState, SurvivorState);

UCLASS()
class SPACH4_API AMatchGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	AMatchGameState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Match")
	EMatchPhase GetMatchPhase() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Match")
	EMatchResult GetMatchResult() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Match|Timer")
	float GetRemainingTime() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Match|Timer")
	float GetTimeLimit() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Match|Delivery")
	int32 GetDeliveryStationValue(FName StationId) const;
	
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Match|Delivery")
	int32 GetDeliveryStationAValue() const;
	
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Match|Delivery")
	int32 GetDeliveryStationBValue() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Match|Delivery")
	int32 GetDeliveryStationTargetValueByID(FName StationId) const;
	
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Match|Delivery")
	int32 GetDeliveryStationTargetValue() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Match|Delivery")
	int32 GetTotalDeliveredValue() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Match|Delivery")
	int32 GetTotalRequiredValue() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Match|Delivery")
	float GetDeliveryProgress() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Match|Escape")
	bool CanActivateEscapeGates() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Match|Escape")
	bool CanSpawnHatch() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Match|Survivor")
	int32 GetAliveSurvivorCount() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Match|Survivor")
	int32 GetEscapedSurvivorCount() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Match|Survivor")
	int32 GetKilledSurvivorCount() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Match|Survivor")
	TArray<FSurvivorMatchState> GetSurvivorStates() const;

	void ApplyBalanceSettings(float NewTimeLimit, int32 NewStationATargetValue, int32 NewStationBTargetValue, int32 NewTotalRequiredValue, int32 NewInitialSurvivorCount);
	void SetMatchPhase(EMatchPhase NewMatchPhase);
	void SetMatchResult(EMatchResult NewMatchResult);
	void SetRemainingTime(float NewRemainingTime);
	void AddDeliveredValue(FName StationId, int32 Value);
	void SetCanActivateEscapeGates(bool bNewCanActivateEscapeGates);
	void SetCanSpawnHatch(bool bNewCanSpawnHatch);
	void SetSurvivorCounts(int32 NewAliveSurvivorCount, int32 NewEscapedSurvivorCount, int32 NewKilledSurvivorCount);
	void SetSurvivorState(FName SurvivorId, ESurvivorState NewSurvivorState);

	UPROPERTY(BlueprintAssignable, Category = "Match|Events")
	FOnMatchPhaseChangedSignature OnMatchPhaseChanged;

	UPROPERTY(BlueprintAssignable, Category = "Match|Events")
	FOnMatchResultChangedSignature OnMatchResultChanged;

	UPROPERTY(BlueprintAssignable, Category = "Match|Events")
	FOnMatchTimerChangedSignature OnMatchTimerChanged;

	UPROPERTY(BlueprintAssignable, Category = "Match|Events")
	FOnDeliveryProgressChangedSignature OnDeliveryProgressChanged;

	UPROPERTY(BlueprintAssignable, Category = "Match|Events")
	FOnEscapeGateAvailabilityChangedSignature OnEscapeGateAvailabilityChanged;

	UPROPERTY(BlueprintAssignable, Category = "Match|Events")
	FOnHatchAvailabilityChangedSignature OnHatchAvailabilityChanged;

	UPROPERTY(BlueprintAssignable, Category = "Match|Events")
	FOnSurvivorCountChangedSignature OnSurvivorCountChanged;

	UPROPERTY(BlueprintAssignable, Category = "Match|Events")
	FOnSurvivorStateChangedSignature OnSurvivorStateChanged;

protected:
	UFUNCTION()
	void OnRep_MatchPhase();

	UFUNCTION()
	void OnRep_MatchResult();

	UFUNCTION()
	void OnRep_RemainingTime();

	// 점수 변동이 있을떄 호출
	UFUNCTION()
	void OnRep_DeliveryProgress();

	UFUNCTION()
	void OnRep_EscapeGateAvailability();

	UFUNCTION()
	void OnRep_HatchAvailability();

	UFUNCTION()
	void OnRep_SurvivorCounts();

	UFUNCTION()
	void OnRep_SurvivorStates();

	void BroadcastDeliveryProgressChanged();

	UPROPERTY(ReplicatedUsing = OnRep_MatchPhase, VisibleAnywhere, BlueprintReadOnly, Category = "Match")
	EMatchPhase MatchPhase = EMatchPhase::Waiting;

	UPROPERTY(ReplicatedUsing = OnRep_MatchResult, VisibleAnywhere, BlueprintReadOnly, Category = "Match")
	EMatchResult MatchResult = EMatchResult::None;

	UPROPERTY(ReplicatedUsing = OnRep_RemainingTime, VisibleAnywhere, BlueprintReadOnly, Category = "Match|Timer")
	float RemainingTime = 900.0f;

	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "Match|Timer")
	float TimeLimit = 900.0f;

	// 현재 각 납품소의 점수
	UPROPERTY(ReplicatedUsing = OnRep_DeliveryProgress, VisibleAnywhere, BlueprintReadOnly, Category = "Match|Delivery")
	int32 DeliveryStationAValue = 0;

	UPROPERTY(ReplicatedUsing = OnRep_DeliveryProgress, VisibleAnywhere, BlueprintReadOnly, Category = "Match|Delivery")
	int32 DeliveryStationBValue = 0;

	// 두개의 납품소는 구분되고, 둘다 납품 완료되어야 함.(200+200->400일때 탈출구 작동가능)
	UPROPERTY(ReplicatedUsing = OnRep_DeliveryProgress, VisibleAnywhere, BlueprintReadOnly, Category = "Match|Delivery")
	int32 DeliveryStationATargetValue = 200;

	UPROPERTY(ReplicatedUsing = OnRep_DeliveryProgress, VisibleAnywhere, BlueprintReadOnly, Category = "Match|Delivery")
	int32 DeliveryStationBTargetValue = 200;

	UPROPERTY(ReplicatedUsing = OnRep_DeliveryProgress, VisibleAnywhere, BlueprintReadOnly, Category = "Match|Delivery")
	int32 TotalRequiredValue = 400;

	UPROPERTY(ReplicatedUsing = OnRep_EscapeGateAvailability, VisibleAnywhere, BlueprintReadOnly, Category = "Match|Escape")
	bool bCanActivateEscapeGates = false;

	// 개구멍
	UPROPERTY(ReplicatedUsing = OnRep_HatchAvailability, VisibleAnywhere, BlueprintReadOnly, Category = "Match|Escape")
	bool bCanSpawnHatch = false;

	UPROPERTY(ReplicatedUsing = OnRep_SurvivorCounts, VisibleAnywhere, BlueprintReadOnly, Category = "Match|Survivor")
	int32 AliveSurvivorCount = 3;

	UPROPERTY(ReplicatedUsing = OnRep_SurvivorCounts, VisibleAnywhere, BlueprintReadOnly, Category = "Match|Survivor")
	int32 EscapedSurvivorCount = 0;

	UPROPERTY(ReplicatedUsing = OnRep_SurvivorCounts, VisibleAnywhere, BlueprintReadOnly, Category = "Match|Survivor")
	int32 KilledSurvivorCount = 0;

	// 생존자 리스트, 상태 관리
	UPROPERTY(ReplicatedUsing = OnRep_SurvivorStates, VisibleAnywhere, BlueprintReadOnly, Category = "Match|Survivor")
	TArray<FSurvivorMatchState> SurvivorStates;
};

#pragma once

#include "CoreMinimal.h"
#include "Characters/Survivor/SurvivorCharacter.h"
#include "GameFramework/GameStateBase.h"
#include "Systems/LobbyGameState.h"
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

USTRUCT(BlueprintType)
struct FMatchPlayerState
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Match|Player")
	int32 PlayerId = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Match|Player")
	FString Nickname;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Match|Player")
	ELobbyPlayerRole PlayerRole = ELobbyPlayerRole::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Match|Player")
	bool bIsConnected = true;

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

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMatchPlayersChangedSignature);

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

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Match|Player")
	TArray<FMatchPlayerState> GetMatchPlayers() const;

	UFUNCTION(BlueprintCallable, Category = "Match|Player")
	bool GetMatchPlayerState(FName Nickname, FMatchPlayerState& OutPlayerState) const;

	void ApplyBalanceSettings(float NewTimeLimit, int32 NewStationATargetValue, int32 NewStationBTargetValue, int32 NewTotalRequiredValue);
	void SetMatchPhase(EMatchPhase NewMatchPhase);
	void SetMatchResult(EMatchResult NewMatchResult);
	void SetRemainingTime(float NewRemainingTime);
	void AddDeliveredValue(FName StationId, int32 Value);
	void SetCanActivateEscapeGates(bool bNewCanActivateEscapeGates);
	void SetCanSpawnHatch(bool bNewCanSpawnHatch);
	void SetSurvivorCounts(int32 NewAliveSurvivorCount, int32 NewEscapedSurvivorCount, int32 NewKilledSurvivorCount);
	void RegisterMatchPlayer(int32 PlayerId, const FString& Nickname, ELobbyPlayerRole PlayerRole);
	void SetMatchPlayerConnected(FName Nickname, bool bNewIsConnected);
	void SetSurvivorState(FName SurvivorId, ESurvivorState NewSurvivorState);

	// 진행상황 갱신 EMatchPhase(진행, 탈출구, 종료)
	UPROPERTY(BlueprintAssignable, Category = "Match|Events")
	FOnMatchPhaseChangedSignature OnMatchPhaseChanged;
	
	// 게임 결과 MatchResult
	UPROPERTY(BlueprintAssignable, Category = "Match|Events")
	FOnMatchResultChangedSignature OnMatchResultChanged;

	// 남은 시간
	UPROPERTY(BlueprintAssignable, Category = "Match|Events")
	FOnMatchTimerChangedSignature OnMatchTimerChanged;

	// 진행상황(A점수, B점수, 총점, 진행률%)
	UPROPERTY(BlueprintAssignable, Category = "Match|Events")
	FOnDeliveryProgressChangedSignature OnDeliveryProgressChanged;

	// 탈출구 활성화
	UPROPERTY(BlueprintAssignable, Category = "Match|Events")
	FOnEscapeGateAvailabilityChangedSignature OnEscapeGateAvailabilityChanged;
	
	// 개구멍 활성화
	UPROPERTY(BlueprintAssignable, Category = "Match|Events")
	FOnHatchAvailabilityChangedSignature OnHatchAvailabilityChanged;

	// 생존자 수 변경(생존자수, 탈출자수, 사망자수 / 셋의 합은 총 플레이어수)
	UPROPERTY(BlueprintAssignable, Category = "Match|Events")
	FOnSurvivorCountChangedSignature OnSurvivorCountChanged;

	// 특정 플레이어의 상태 변경(생존자는 서버 - AMatchGameState::SetSurvivorState를 각자호출하고, 해당 이벤트에서 갱신하도록 함.)
	UPROPERTY(BlueprintAssignable, Category = "Match|Events")
	FOnSurvivorStateChangedSignature OnSurvivorStateChanged;

	// 
	UPROPERTY(BlueprintAssignable, Category = "Match|Events")
	FOnMatchPlayersChangedSignature OnMatchPlayersChanged;

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
	void OnRep_MatchPlayers();

	void BroadcastDeliveryProgressChanged();
	void BroadcastMatchPlayersChanged();
	void RefreshSurvivorCountsFromMatchPlayers();

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
	UPROPERTY(ReplicatedUsing = OnRep_MatchPlayers, VisibleAnywhere, BlueprintReadOnly, Category = "Match|Player")
	TArray<FMatchPlayerState> MatchPlayers;
};

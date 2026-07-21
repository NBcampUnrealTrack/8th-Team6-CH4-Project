#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "SPMovementComponent.generated.h"

class ASurvivorCharacter;
class USurvivorData;
enum class ESurvivorState : uint8;

UCLASS()
class SPACH4_API USPSurvivorCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	void SetWantsToRun(bool bNewWantsToRun);
	bool WantsToRun() const { return bWantsToRun; }

	virtual void UpdateFromCompressedFlags(uint8 Flags) override;
	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;

private:
	void ApplyWantsToRun();

	uint8 bWantsToRun : 1 = false;
};

UENUM(BlueprintType)
enum class ESpeedPotionPhase : uint8
{
	None,
	Boost,
	Fatigue
};

UCLASS(ClassGroup = (SP), meta = (BlueprintSpawnableComponent), Blueprintable, BlueprintType)
class SPACH4_API USPMovementComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USPMovementComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void HandleStateTransition(ESurvivorState OldState, ESurvivorState NewState);
	void SnapToTargetSpeed();
	void SetDeliveryMovePenaltyActive(bool bNewActive);

	void SetWantsToRun(bool bNewWantsToRun);
	bool CanActivateSpeedPotion() const;
	bool TryActivateSpeedPotion(bool bSkipFatigue);

	UFUNCTION(BlueprintPure, Category = "SP|Movement")
	float GetTargetMoveSpeed() const;

	UFUNCTION(BlueprintPure, Category = "SP|Movement")
	bool IsRunning() const;

protected:
	virtual void BeginPlay() override;

private:
	ASurvivorCharacter* GetSurvivor() const;
	const USurvivorData* GetSurvivorData() const;

	float ComputeTargetMoveSpeed() const;
	float GetBaseWalkSpeed() const;
	float GetSprintSpeedForState(ESurvivorState State) const;
	float GetCarryMoveSpeedMultiplier() const;
	float GetDeliveryMoveSpeedMultiplier() const;
	float GetSpeedPotionMultiplier() const;
	void ClampPlanarVelocityToSpeed(float MaxPlanarSpeed) const;

	UFUNCTION()
	void OnRep_DeliveryMovePenaltyActive();

	UFUNCTION()
	void OnRep_WantsToRun();

	UFUNCTION()
	void OnRep_SpeedPotionPhase();

	UFUNCTION()
	void OnRep_HitEscapeSprintState();

	void StartHitEscapeSprint(ESurvivorState PreviousState);
	void FinishHitEscapeSprint();
	void FinishSpeedPotionBoost();
	void FinishSpeedPotionFatigue();

	UPROPERTY(ReplicatedUsing = OnRep_WantsToRun)
	bool bWantsToRun{false};

	UPROPERTY(ReplicatedUsing = OnRep_SpeedPotionPhase)
	ESpeedPotionPhase SpeedPotionPhase = ESpeedPotionPhase::None;

	UPROPERTY(ReplicatedUsing = OnRep_DeliveryMovePenaltyActive)
	bool bDeliveryMovePenaltyActive{false};

	UPROPERTY(ReplicatedUsing = OnRep_HitEscapeSprintState)
	bool bHitEscapeSprintActive{false};

	UPROPERTY(ReplicatedUsing = OnRep_HitEscapeSprintState)
	float HitEscapeSprintSpeed{0.f};

	bool bSkipSpeedPotionFatigue{false};
	FTimerHandle HitEscapeSprintTimer;
	FTimerHandle SpeedPotionTimer;
};

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SPMovementComponent.generated.h"

class ASurvivorCharacter;
class USurvivorData;
enum class ESurvivorState : uint8;

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
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

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

	UFUNCTION()
	void OnRep_DeliveryMovePenaltyActive();

	void StartHitEscapeSprint(ESurvivorState PreviousState);
	void FinishSpeedPotionBoost();
	void FinishSpeedPotionFatigue();
	
	UPROPERTY(EditDefaultsOnly, Category = "SP|Movement")
	float MoveSpeedInterpSpeed{8.f};
	
	UPROPERTY(Replicated)
	bool bWantsToRun{false};

	UPROPERTY(Replicated)
	ESpeedPotionPhase SpeedPotionPhase = ESpeedPotionPhase::None;

	UPROPERTY(ReplicatedUsing = OnRep_DeliveryMovePenaltyActive)
	bool bDeliveryMovePenaltyActive{false};

	bool bHitEscapeSprintActive{false};
	float HitEscapeSprintRemaining{0.f};
	float HitEscapeSprintSpeed{0.f};
	bool bSkipSpeedPotionFatigue{false};
	FTimerHandle SpeedPotionTimer;
};

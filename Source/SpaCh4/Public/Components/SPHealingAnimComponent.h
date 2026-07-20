#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "SPHealingAnimComponent.generated.h"

class UAnimMontage;
class UAnimInstance;
class ASurvivorCharacter;

DECLARE_MULTICAST_DELEGATE(FOnHealingLoopBegan);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnHealingChannelEnded, bool /*bCompleted*/);

UENUM()
enum class EHealingAnimPhase : uint8
{
	None,
	Knee,
	Loop,
	Return
};

UCLASS(ClassGroup = (SP), meta = (BlueprintSpawnableComponent))
class SPACH4_API USPHealingAnimComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USPHealingAnimComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	bool IsHealing() const { return bIsHealing || CurrentPhase == EHealingAnimPhase::Return; }
	bool IsChannelingHealing() const { return bIsHealing; }

	void BeginHealingChannel();
	void EndHealingChannel(bool bCompleted);
	void CancelHealingChannel();

	void SetAutoCompleteLoop(bool bEnabled) { bAutoCompleteLoop = bEnabled; }

	void BeginHealingChannelDebug();
	void CancelHealingChannelDebug();

	FOnHealingLoopBegan OnHealingLoopBegan;
	FOnHealingChannelEnded OnHealingChannelEnded;

protected:
	void EnsureMontagesLoaded();

private:
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_BeginHealingChannel();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_EndHealingChannel(bool bCompleted);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_CancelHealingChannel();

	UFUNCTION()
	void OnHealingMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	ASurvivorCharacter* GetSurvivor() const;
	UAnimInstance* GetAnimInstance() const;
	bool IsHealingChannelActive() const;
	void StartHealingChannelInternal();
	void EnterHealingState();
	void ExitHealingState(bool bPlayReturnAnim);
	void FinishHealingChannel();
	void TransitionKneeToLoop();
	void PlayLoopMontage(float BlendInTime = -1.f, bool bCrossfadeFromPrevious = false);
	void PlayReturnMontage(float BlendInTime = -1.f);
	void ConfigureLoopMontage(UAnimInstance* AnimInstance);
	void ClearLoopMontage(UAnimInstance* AnimInstance);
	void RestartLoopMontage();
	void BlendOutLoopForReturn(UAnimInstance* AnimInstance, float BlendOutTime);
	void PlayHealingMontage(UAnimMontage* Montage, EHealingAnimPhase Phase, float BlendInTime = -1.f, bool bStopPreviousMontage = true);
	void StopActiveHealingMontage(float BlendOutTime = -1.f);
	void StopAllHealingMontages(float BlendOutTime = 0.f);
	void RestoreMovementAfterHealing();
	void SetHealingTickEnabled(bool bEnabled);
	void ClearHealingTimers();
	void HandleReturnMontageFallback();
	void UpdateLoopEndTime();
	bool ShouldEndLoopChannel() const;

	UPROPERTY(EditDefaultsOnly, Category = "SP|Healing")
	TSoftObjectPtr<UAnimMontage> HealingKneeMontageAsset;

	UPROPERTY(EditDefaultsOnly, Category = "SP|Healing")
	TSoftObjectPtr<UAnimMontage> HealingLoopMontageAsset;

	UPROPERTY(EditDefaultsOnly, Category = "SP|Healing")
	TSoftObjectPtr<UAnimMontage> HealingReturnMontageAsset;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> HealingKneeMontage;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> HealingLoopMontage;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> HealingReturnMontage;

	UPROPERTY(EditDefaultsOnly, Category = "SP|Healing")
	float HealingMontageBlendOut = 0.2f;

	UPROPERTY(EditDefaultsOnly, Category = "SP|Healing")
	float HealingKneeToLoopBlendIn = 0.12f;

	UPROPERTY(EditDefaultsOnly, Category = "SP|Healing")
	float HealingToReturnBlendIn = 0.15f;

	/** Crossfade when Loop wraps from end back to start. */
	UPROPERTY(EditDefaultsOnly, Category = "SP|Healing", meta = (ClampMin = "0.05"))
	float HealingLoopRestartBlendIn = 0.15f;

	/** Loop phase duration before auto-complete (plays Return). */
	UPROPERTY(EditDefaultsOnly, Category = "SP|Healing", meta = (ClampMin = "0.1"))
	float HealingLoopDuration = 3.0f;

	UPROPERTY(Replicated)
	bool bIsHealing = false;

	TObjectPtr<UAnimMontage> ActiveHealingMontage;
	FOnMontageEnded HealingMontageEndedDelegate;
	EHealingAnimPhase CurrentPhase = EHealingAnimPhase::None;
	bool bLoopTransitionTriggered = false;
	bool bLoopCrossfadeQueued = false;
	bool bIsEndingHealing = false;
	bool bAutoCompleteLoop = true;
	float LoopEndWorldTime = 0.f;
	float ReturnMontageNotPlayingTime = 0.f;
	FTimerHandle ReturnMontageFallbackTimer;
	TEnumAsByte<EMovementMode> CachedMovementMode = MOVE_Walking;
	bool bHasCachedMovementMode = false;
	float CachedGravityScale = 1.f;
};

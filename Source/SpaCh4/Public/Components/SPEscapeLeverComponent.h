#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "SPEscapeLeverComponent.generated.h"

class UAnimMontage;
class UAnimInstance;
class ASPEscapeGate;
class ASurvivorCharacter;

UENUM()
enum class ELeverAnimPhase : uint8
{
	None,
	Pulldown,
	Watch,
	Return
};

UCLASS(ClassGroup = (SP), meta = (BlueprintSpawnableComponent))
class SPACH4_API USPEscapeLeverComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USPEscapeLeverComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	bool IsPullingLever() const { return bIsPullingLever || CurrentPhase == ELeverAnimPhase::Return; }
	bool IsChannelingLever() const { return bIsPullingLever; }

	void BeginLeverChannel(ASPEscapeGate* Gate);
	void EndLeverChannel(bool bCompleted);
	void CancelLeverChannel();

	void BeginLeverChannelDebug();
	void EndLeverChannelDebug();
	void CancelLeverChannelDebug();

protected:
	void EnsureMontagesLoaded();

private:
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_BeginLeverChannel(ASPEscapeGate* Gate);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_BeginLeverChannelDebug();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_EndLeverChannel(bool bCompleted);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_CancelLeverChannel();

	UFUNCTION()
	void OnLeverMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	ASurvivorCharacter* GetSurvivor() const;
	UAnimInstance* GetAnimInstance() const;
	void PlayLeverMontage(UAnimMontage* Montage, ELeverAnimPhase Phase, float BlendInTime = -1.f, bool bStopPreviousMontage = true);
	void StopActiveLeverMontage(float BlendOutTime = -1.f);
	void StopAllLeverMontages(float BlendOutTime = 0.f);
	void EnterLeverState();
	void ExitLeverState(bool bPlayReturnAnim);
	void FinishLeverChannel();
	void StartLeverChannelInternal(ASPEscapeGate* Gate);
	void TransitionPulldownToWatch();
	void PlayWatchMontage(float BlendInTime = -1.f, bool bCrossfadeFromPrevious = false);
	void PlayReturnMontage(float BlendInTime = -1.f);
	void ConfigureWatchMontageLoop(UAnimInstance* AnimInstance);
	void ClearWatchMontageLoop(UAnimInstance* AnimInstance);
	void RestartWatchMontageLoop();
	bool IsLeverChannelActive() const;
	void AbortReturnMontage();
	void RestoreMovementAfterLever();
	void SetLeverTickEnabled(bool bEnabled);
	void ClearLeverTimers();
	void HandleReturnMontageFallback();
	void UpdateChannelEndTime();
	bool ShouldEndWatchChannel() const;
	void BlendOutWatchForReturn(UAnimInstance* AnimInstance, float BlendOutTime);

	UPROPERTY(EditDefaultsOnly, Category = "SP|Escape|Lever")
	TObjectPtr<UAnimMontage> LeverPulldownMontage;

	UPROPERTY(EditDefaultsOnly, Category = "SP|Escape|Lever")
	TObjectPtr<UAnimMontage> LeverWatchMontage;

	UPROPERTY(EditDefaultsOnly, Category = "SP|Escape|Lever")
	TObjectPtr<UAnimMontage> LeverReturnMontage;

	UPROPERTY(EditDefaultsOnly, Category = "SP|Escape|Lever")
	float LeverMontageBlendOut = 0.2f;

	UPROPERTY(EditDefaultsOnly, Category = "SP|Escape|Lever")
	float LeverPulldownToWatchBlendIn = 0.12f;

	UPROPERTY(EditDefaultsOnly, Category = "SP|Escape|Lever")
	float LeverToReturnBlendIn = 0.15f;

	UPROPERTY(EditDefaultsOnly, Category = "SP|Escape|Lever")
	float LeverDebugChannelDuration = 8.0f;

	UPROPERTY(Replicated)
	bool bIsPullingLever = false;

	TWeakObjectPtr<ASPEscapeGate> CurrentGate;
	TObjectPtr<UAnimMontage> ActiveLeverMontage;
	FOnMontageEnded LeverMontageEndedDelegate;
	ELeverAnimPhase CurrentPhase = ELeverAnimPhase::None;
	bool bWatchTransitionTriggered = false;
	bool bIsEndingChannel = false;
	float ChannelEndWorldTime = 0.f;
	TEnumAsByte<EMovementMode> CachedMovementMode = MOVE_Walking;
	bool bHasCachedMovementMode = false;
	float CachedGravityScale = 1.f;
	float ReturnMontageNotPlayingTime = 0.f;
	FTimerHandle ReturnMontageFallbackTimer;
};

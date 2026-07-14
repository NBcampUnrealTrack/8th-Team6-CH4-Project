#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SPKillerCarryAnimComponent.generated.h"

class UAnimMontage;
class UBlendProfile;
class UKillerData;

DECLARE_MULTICAST_DELEGATE(FOnKillerCarryPickupAttach);
DECLARE_MULTICAST_DELEGATE(FOnKillerCarryPickupFinished);

/**
 * Killer hoist/carry animation:
 * 1) Full-body AS_PickUP_Montage
 * 2) Mid-montage attach window (survivor → hand_r)
 * 3) Upper-body AS_Carrying_Montage loop over motion-matched legs
 */
UCLASS(ClassGroup = (SP), meta = (BlueprintSpawnableComponent))
class SPACH4_API USPKillerCarryAnimComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USPKillerCarryAnimComponent();

	bool IsPlayingPickupAnim() const { return bIsPlayingPickupAnim; }
	bool IsPlayingCarryingAnim() const { return bIsPlayingCarryingAnim; }
	bool WantsCarryingAnim() const { return bWantsCarryingAnim; }

	void BeginPickupAnim();
	void BeginCarryingAnim();
	void EndCarryAnims();

	/** Keep / restart carrying montage while Carrying (drop/cage clears via EndCarryAnims). */
	void EnsureCarryingMontagePlaying();

	FOnKillerCarryPickupAttach OnPickupAttachRequested;
	FOnKillerCarryPickupFinished OnPickupFinished;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	const UKillerData* ResolveKillerData() const;
	UAnimMontage* ResolvePickupMontage() const;
	UAnimMontage* ResolveCarryingMontage() const;
	const TArray<FName>& ResolveCarryBlendRootBones() const;
	float ResolveAttachNormalizedTime() const;

	UBlendProfile* GetOrCreateCarryBlendProfile();
	UAnimMontage* PrepareMontageForSlot(UAnimMontage* SourceMontage, FName SlotName);

private:
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_BeginPickupAnim();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_BeginCarryingAnim();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_EndCarryAnims();

	UFUNCTION()
	void OnPickupMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	UFUNCTION()
	void OnCarryingMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	void OnPickupAttachTimeout();
	void OnPickupMontageTimeout();

	void PlayPickupMontage();
	void PlayCarryingMontage();
	void StopActiveMontages(float BlendOut);
	void ConfigureCarryingMontageLoop(UAnimInstance* AnimInstance, UAnimMontage* Montage) const;
	bool ShouldKeepCarryingAnim() const;

	void ClearPickupState();
	void ClearCarryingState();

	class ACharacter* GetOwnerCharacter() const;
	class UAnimInstance* GetAnimInstance() const;

	UPROPERTY(EditDefaultsOnly, Category = "SP|Killer|Carry|Data")
	TObjectPtr<UKillerData> KillerDataOverride;

	UPROPERTY(EditDefaultsOnly, Category = "SP|Killer|Carry")
	TSoftObjectPtr<UAnimMontage> FallbackPickupMontage;

	UPROPERTY(EditDefaultsOnly, Category = "SP|Killer|Carry")
	TSoftObjectPtr<UAnimMontage> FallbackCarryingMontage;

	UPROPERTY(EditDefaultsOnly, Category = "SP|Killer|Carry")
	TArray<FName> CarryBlendRootBones = { TEXT("spine_01"), TEXT("clavicle_l"), TEXT("clavicle_r") };

	UPROPERTY(EditDefaultsOnly, Category = "SP|Killer|Carry")
	FName MontageSlotName = TEXT("UpperBody");

	/** Full-body pickup stays on DefaultSlot; carrying uses MontageSlotName (UpperBody). */
	UPROPERTY(EditDefaultsOnly, Category = "SP|Killer|Carry")
	FName PickupMontageSlotName = TEXT("DefaultSlot");

	UPROPERTY(EditDefaultsOnly, Category = "SP|Killer|Carry")
	float PickupBlendIn = 0.1f;

	UPROPERTY(EditDefaultsOnly, Category = "SP|Killer|Carry")
	float PickupBlendOut = 0.15f;

	UPROPERTY(EditDefaultsOnly, Category = "SP|Killer|Carry")
	float CarryingBlendIn = 0.12f;

	UPROPERTY(EditDefaultsOnly, Category = "SP|Killer|Carry")
	float CarryingBlendOut = 0.2f;

	UPROPERTY(Transient)
	bool bIsPlayingPickupAnim = false;

	UPROPERTY(Transient)
	bool bIsPlayingCarryingAnim = false;

	/** True from BeginCarryingAnim until EndCarryAnims (drop / cage). */
	UPROPERTY(Transient)
	bool bWantsCarryingAnim = false;

	UPROPERTY(Transient)
	bool bAttachRequested = false;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActivePickupMontage;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveCarryingMontage;

	UPROPERTY(Transient)
	TObjectPtr<UBlendProfile> RuntimeCarryBlendProfile;

	FTimerHandle PickupAttachTimer;
	FTimerHandle PickupResetTimer;
	FOnMontageEnded PickupMontageEndedDelegate;
	FOnMontageEnded CarryingMontageEndedDelegate;
};

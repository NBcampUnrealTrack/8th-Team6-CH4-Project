#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SPKillerAttackAnimComponent.generated.h"

class UAnimMontage;
class UAnimSequence;
class UBlendProfile;
class UKillerData;

UCLASS(ClassGroup = (SP), meta = (BlueprintSpawnableComponent))
class SPACH4_API USPKillerAttackAnimComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USPKillerAttackAnimComponent();

	bool IsPlayingArmAttackAnim() const { return bIsPlayingArmAttackAnim; }
	bool IsPlayingGroggyAnim() const { return bIsPlayingGroggyAnim; }

	void BeginArmAttackAnim();
	void EndArmAttackAnim();
	void CancelArmAttackAnim();

	/** Plays the post-attack groggy montage on all clients. Returns montage length (0 if unavailable). */
	float BeginGroggyAnim();
	void EndGroggyAnim();

	/** Fired when the arm attack montage finishes naturally (all clients). */
	float GetArmAttackMontageLength() const;

	DECLARE_MULTICAST_DELEGATE(FOnArmAttackMontageFinished);
	FOnArmAttackMontageFinished OnArmAttackMontageFinished;

protected:
	const UKillerData* ResolveKillerData() const;
	UAnimSequence* ResolveArmAttackSequence() const;
	UAnimMontage* ResolveGroggyMontage() const;
	const TArray<FName>& ResolveArmBlendRootBones() const;

	void EnsureAttackSequenceLoaded();
	UBlendProfile* GetOrCreateArmBlendProfile();

private:
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_BeginArmAttackAnim();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_EndArmAttackAnim();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_BeginGroggyAnim();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_EndGroggyAnim();

	UFUNCTION()
	void OnArmAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	UFUNCTION()
	void OnGroggyMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	void OnArmAttackMontageTimeout();
	void OnGroggyMontageTimeout();
	void ClearArmAttackMontageState();
	void ClearGroggyMontageState();
	void ScheduleArmAttackResetTimer(float MontageDuration);
	void ScheduleGroggyResetTimer(float MontageDuration);

	void PlayArmAttackMontage();
	void StopArmAttackMontage();
	float PlayGroggyMontage();
	void StopGroggyMontage();

	class ACharacter* GetOwnerCharacter() const;
	class UAnimInstance* GetAnimInstance() const;

	UPROPERTY(EditDefaultsOnly, Category = "SP|Killer|Attack|Data")
	TObjectPtr<UKillerData> KillerDataOverride;

	UPROPERTY(EditDefaultsOnly, Category = "SP|Killer|Attack|Data")
	TSoftObjectPtr<UAnimSequence> FallbackArmAttackSequence;

	UPROPERTY(EditDefaultsOnly, Category = "SP|Killer|Attack|Data")
	TSoftObjectPtr<UAnimMontage> FallbackGroggyMontage;

	UPROPERTY(EditDefaultsOnly, Category = "SP|Killer|Attack")
	TObjectPtr<UAnimMontage> GroggyMontage;

	UPROPERTY(EditDefaultsOnly, Category = "SP|Killer|Attack")
	FName GroggyMontageSlotName = TEXT("DefaultSlot");

	UPROPERTY(EditDefaultsOnly, Category = "SP|Killer|Attack")
	float GroggyBlendIn = 0.12f;

	UPROPERTY(EditDefaultsOnly, Category = "SP|Killer|Attack")
	float GroggyBlendOut = 0.2f;

	UPROPERTY(EditDefaultsOnly, Category = "SP|Killer|Attack")
	TObjectPtr<UAnimSequence> ArmAttackSequence;

	UPROPERTY(EditDefaultsOnly, Category = "SP|Killer|Attack")
	TArray<FName> ArmBlendRootBones = {
		TEXT("clavicle_l"),
		TEXT("clavicle_r"),
	};

	UPROPERTY(EditDefaultsOnly, Category = "SP|Killer|Attack")
	TObjectPtr<UBlendProfile> ArmBlendProfile;

	UPROPERTY(EditDefaultsOnly, Category = "SP|Killer|Attack")
	FName AttackMontageSlotName = TEXT("DefaultSlot");

	UPROPERTY(EditDefaultsOnly, Category = "SP|Killer|Attack")
	float ArmAttackBlendIn = 0.08f;

	UPROPERTY(EditDefaultsOnly, Category = "SP|Killer|Attack")
	float ArmAttackBlendOut = 0.12f;

	UPROPERTY(Transient)
	bool bIsPlayingArmAttackAnim = false;

	UPROPERTY(Transient)
	bool bIsPlayingGroggyAnim = false;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveGroggyMontage;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> CachedArmAttackMontage;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveArmAttackMontage;

	UPROPERTY(Transient)
	TObjectPtr<UBlendProfile> RuntimeArmBlendProfile;

	FTimerHandle ArmAttackResetTimer;
	FTimerHandle GroggyResetTimer;
	FOnMontageEnded ArmAttackMontageEndedDelegate;
	FOnMontageEnded GroggyMontageEndedDelegate;
};

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SPKillerCarrySoundComponent.generated.h"

class AKillerCharacter;
class USPKillerCarryAnimComponent;
class UAudioComponent;
class USoundBase;

UCLASS(ClassGroup = (SP), meta = (BlueprintSpawnableComponent))
class SPACH4_API USPKillerCarrySoundComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USPKillerCarrySoundComponent();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void BindCarryAnimDelegates();
	void UnbindCarryAnimDelegates();

	void HandlePickupBegan();
	void HandlePickupAttach();
	void HandlePickupFinished();
	void HandleCarryEnded();

	void StartPickupSound();
	void StopPickupSound(bool bPlayDropSound);
	void StartCarryingLoopSound();
	void StopCarryingLoopSound();

	void PlayOneShotSound(USoundBase* Sound) const;
	float GetPickupSoundDuration() const;

	AKillerCharacter* GetKiller() const;
	USPKillerCarryAnimComponent* GetCarryAnimComponent() const;
	USceneComponent* GetAttachComponent() const;
	bool ShouldPlayAudio() const;

	UPROPERTY(EditDefaultsOnly, Category = "SP|CarrySound")
	TObjectPtr<USoundBase> PickupSound;

	UPROPERTY(EditDefaultsOnly, Category = "SP|CarrySound")
	TObjectPtr<USoundBase> PickupAttachSound;

	UPROPERTY(EditDefaultsOnly, Category = "SP|CarrySound")
	TObjectPtr<USoundBase> CarryingLoopSound;

	UPROPERTY(EditDefaultsOnly, Category = "SP|CarrySound")
	TObjectPtr<USoundBase> DropSound;

	UPROPERTY(EditDefaultsOnly, Category = "SP|CarrySound", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float PickupDurationOverride = 0.f;

	UPROPERTY(EditDefaultsOnly, Category = "SP|CarrySound", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float VolumeMultiplier = 1.f;

	UPROPERTY(EditDefaultsOnly, Category = "SP|CarrySound", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float PitchMultiplier = 1.f;

	UPROPERTY(EditDefaultsOnly, Category = "SP|CarrySound", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float PickupFadeOutDuration = 0.35f;

	UPROPERTY(EditDefaultsOnly, Category = "SP|CarrySound", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float CarryingFadeOutDuration = 0.25f;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> PickupAudioComponent;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> CarryingAudioComponent;

	FTimerHandle PickupFadeTimerHandle;
	FTimerHandle PickupStopTimerHandle;

	bool bIsPlayingPickupSound = false;
	bool bIsPlayingCarryingLoop = false;
};

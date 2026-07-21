#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Characters/Killer/KillerCharacter.h"
#include "SPKillerGroggySoundComponent.generated.h"

class UAudioComponent;
class USoundBase;

UCLASS(ClassGroup = (SP), meta = (BlueprintSpawnableComponent))
class SPACH4_API USPKillerGroggySoundComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USPKillerGroggySoundComponent();

	void SyncToKillerState(EKillerState NewState);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void HandleKillerStateTransition(EKillerState OldState, EKillerState NewState);
	void StartGroggySound();
	void StopGroggySound(bool bPlayExitSound = true);

	AKillerCharacter* GetKiller() const;
	USceneComponent* GetAttachComponent() const;
	float GetGroggyDuration() const;
	bool ShouldPlayAudio() const;
	void PlayEnterSound() const;
	void PlayExitSound() const;

	UPROPERTY(EditDefaultsOnly, Category = "SP|GroggySound")
	TObjectPtr<USoundBase> GroggySound;

	UPROPERTY(EditDefaultsOnly, Category = "SP|GroggySound")
	TObjectPtr<USoundBase> GroggyEnterSound;

	UPROPERTY(EditDefaultsOnly, Category = "SP|GroggySound")
	TObjectPtr<USoundBase> GroggyExitSound;

	UPROPERTY(EditDefaultsOnly, Category = "SP|GroggySound", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float GroggyDurationOverride = 0.f;

	UPROPERTY(EditDefaultsOnly, Category = "SP|GroggySound", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float VolumeMultiplier = 1.f;

	UPROPERTY(EditDefaultsOnly, Category = "SP|GroggySound", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float PitchMultiplier = 1.f;

	UPROPERTY(EditDefaultsOnly, Category = "SP|GroggySound", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float FadeOutDuration = 0.35f;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> GroggyAudioComponent;

	FTimerHandle GroggyStopTimerHandle;
	FTimerHandle GroggyFadeTimerHandle;

	EKillerState LastKnownState = EKillerState::Idle;
	bool bHasLastKnownState = false;
	bool bIsPlayingGroggySound = false;
};

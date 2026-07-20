#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SPHatchSoundComponent.generated.h"

class ASPHatch;
class UAudioComponent;
class USoundBase;

UCLASS(ClassGroup = (SP), meta = (BlueprintSpawnableComponent))
class SPACH4_API USPHatchSoundComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USPHatchSoundComponent();

	void NotifyHatchStateChanged(bool bHatchActive, bool bHatchOpened);

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void PlayOpenSounds() const;
	void StartWindLoopSound();
	void StopWindLoopSound();
	void PlayOneShotSound(USoundBase* Sound) const;

	ASPHatch* GetHatch() const;
	USceneComponent* GetAttachComponent() const;
	bool ShouldPlayAudio() const;

	/** One-shot door open SFX when the hatch finishes opening. */
	UPROPERTY(EditDefaultsOnly, Category = "SP|HatchSound")
	TObjectPtr<USoundBase> DoorOpenSound;

	/** Ambient wind while the hatch stays open. Set the asset to Looping. */
	UPROPERTY(EditDefaultsOnly, Category = "SP|HatchSound")
	TObjectPtr<USoundBase> WindLoopSound;

	UPROPERTY(EditDefaultsOnly, Category = "SP|HatchSound", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float VolumeMultiplier = 1.f;

	UPROPERTY(EditDefaultsOnly, Category = "SP|HatchSound", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float PitchMultiplier = 1.f;

	UPROPERTY(EditDefaultsOnly, Category = "SP|HatchSound", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float WindFadeOutDuration = 0.5f;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> WindAudioComponent;

	bool bIsPlayingOpenPresentation = false;
};

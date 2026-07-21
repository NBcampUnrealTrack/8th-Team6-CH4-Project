#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SPEscapeLeverSoundComponent.generated.h"

class ASPEscapeGate;
class UAudioComponent;
class USoundBase;

UCLASS(ClassGroup = (SP), meta = (BlueprintSpawnableComponent))
class SPACH4_API USPEscapeLeverSoundComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USPEscapeLeverSoundComponent();

	void NotifyChannelStarted();
	void NotifyChannelStopped(bool bCompleted);

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void StartChannelLoopSound();
	void StopChannelLoopSound(bool bPlayGateOpenSound);

	void PlayGateOpenSound() const;
	void PlayOneShotSound(USoundBase* Sound) const;

	ASPEscapeGate* GetEscapeGate() const;
	USceneComponent* GetAttachComponent() const;
	bool ShouldPlayAudio() const;

	UPROPERTY(EditDefaultsOnly, Category = "SP|LeverSound")
	TObjectPtr<USoundBase> ChannelLoopSound;

	UPROPERTY(EditDefaultsOnly, Category = "SP|LeverSound")
	TObjectPtr<USoundBase> GateOpenSound;

	UPROPERTY(EditDefaultsOnly, Category = "SP|LeverSound", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float VolumeMultiplier = 1.f;

	UPROPERTY(EditDefaultsOnly, Category = "SP|LeverSound", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float PitchMultiplier = 1.f;

	UPROPERTY(EditDefaultsOnly, Category = "SP|LeverSound", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float ChannelFadeOutDuration = 0.35f;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> ChannelAudioComponent;

	bool bIsPlayingChannelLoop = false;
};

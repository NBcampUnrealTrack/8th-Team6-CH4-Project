#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SPHealingSoundComponent.generated.h"

class ASurvivorCharacter;
class UAudioComponent;
class USoundBase;
class USPHealingAnimComponent;

UCLASS(ClassGroup = (SP), meta = (BlueprintSpawnableComponent))
class SPACH4_API USPHealingSoundComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USPHealingSoundComponent();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void BindHealingAnimDelegates();
	void UnbindHealingAnimDelegates();
	void HandleHealingBegan();
	void HandleHealingEnded(bool bCompleted);

	void StartHealingLoopSound();
	void StopHealingLoopSound();

	ASurvivorCharacter* GetSurvivor() const;
	USPHealingAnimComponent* GetHealingAnimComponent() const;
	USceneComponent* GetAttachComponent() const;
	bool ShouldPlayAudio() const;

	UPROPERTY(EditDefaultsOnly, Category = "SP|HealingSound")
	TObjectPtr<USoundBase> HealingLoopSound;

	UPROPERTY(EditDefaultsOnly, Category = "SP|HealingSound", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float VolumeMultiplier = 1.f;

	UPROPERTY(EditDefaultsOnly, Category = "SP|HealingSound", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float PitchMultiplier = 1.f;

	UPROPERTY(EditDefaultsOnly, Category = "SP|HealingSound", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float HealingFadeOutDuration = 0.35f;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> LoopAudioComponent;

	bool bIsPlayingLoop = false;
};

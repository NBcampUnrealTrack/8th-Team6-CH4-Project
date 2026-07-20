#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Characters/Survivor/SurvivorCharacter.h"
#include "SPInjuredSoundComponent.generated.h"

class UAudioComponent;
class USoundBase;

UCLASS(ClassGroup = (SP), meta = (BlueprintSpawnableComponent))
class SPACH4_API USPInjuredSoundComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USPInjuredSoundComponent();

	void HandleStateTransition(ESurvivorState OldState, ESurvivorState NewState);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	ASurvivorCharacter* GetSurvivor() const;
	void StartInjuredSound();
	void StopInjuredSound();
	void PlayEnterSound() const;
	void PlayExitSound() const;
	USceneComponent* GetAttachComponent() const;
	bool ShouldPlayAudio() const;

	UPROPERTY(EditDefaultsOnly, Category = "SP|InjuredSound")
	TObjectPtr<USoundBase> InjuredLoopSound;

	UPROPERTY(EditDefaultsOnly, Category = "SP|InjuredSound")
	TObjectPtr<USoundBase> InjuredEnterSound;

	UPROPERTY(EditDefaultsOnly, Category = "SP|InjuredSound")
	TObjectPtr<USoundBase> InjuredExitSound;

	UPROPERTY(EditDefaultsOnly, Category = "SP|InjuredSound", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float VolumeMultiplier = 1.f;

	UPROPERTY(EditDefaultsOnly, Category = "SP|InjuredSound", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float PitchMultiplier = 1.f;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> LoopAudioComponent;

	bool bIsPlayingLoop = false;
};

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SPDownedCrawlSoundComponent.generated.h"

class ASurvivorCharacter;
class UAudioComponent;
class USoundBase;

UCLASS(ClassGroup = (SP), meta = (BlueprintSpawnableComponent))
class SPACH4_API USPDownedCrawlSoundComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USPDownedCrawlSoundComponent();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	ASurvivorCharacter* GetSurvivor() const;
	USceneComponent* GetAttachComponent() const;
	bool ShouldPlayAudio() const;
	bool IsCrawling() const;
	void StartCrawlSound();
	void StopCrawlSound();

	UPROPERTY(EditDefaultsOnly, Category = "SP|DownedCrawlSound")
	TObjectPtr<USoundBase> CrawlLoopSound;

	UPROPERTY(EditDefaultsOnly, Category = "SP|DownedCrawlSound", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MoveSpeedThreshold = 10.f;

	UPROPERTY(EditDefaultsOnly, Category = "SP|DownedCrawlSound", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float VolumeMultiplier = 1.f;

	UPROPERTY(EditDefaultsOnly, Category = "SP|DownedCrawlSound", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float PitchMultiplier = 1.f;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> CrawlAudioComponent;

	bool bIsPlayingCrawlSound = false;
};

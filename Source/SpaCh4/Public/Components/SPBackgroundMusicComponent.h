#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SPBackgroundMusicComponent.generated.h"

class UAudioComponent;
class USoundBase;
enum class EMatchPhase : uint8;

UCLASS(ClassGroup = (SP), meta = (BlueprintSpawnableComponent))
class SPACH4_API USPBackgroundMusicComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USPBackgroundMusicComponent();

	void NotifyChaseMusicStarted();
	void NotifyChaseMusicStopped();
	void SyncToMatchPhase(EMatchPhase NewMatchPhase);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleMatchPhaseChanged(EMatchPhase NewMatchPhase);

	void ResolveBackgroundMusicAsset();
	UObject* GetAudioContextObject() const;
	bool ShouldPlayForPhase(EMatchPhase MatchPhase) const;
	bool ShouldPlayAudio() const;
	void EnsureAudioComponent();
	void StartBackgroundMusic();
	void StartBackgroundMusicInternal();
	void StopBackgroundMusic();
	void DuckBackgroundMusic();
	void RestoreBackgroundMusic();

	UPROPERTY(EditDefaultsOnly, Category = "SP|BackgroundMusic")
	TObjectPtr<USoundBase> BackgroundMusic;

	UPROPERTY(EditDefaultsOnly, Category = "SP|BackgroundMusic", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float VolumeMultiplier = 0.6f;

	UPROPERTY(EditDefaultsOnly, Category = "SP|BackgroundMusic", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float PitchMultiplier = 1.f;

	UPROPERTY(EditDefaultsOnly, Category = "SP|BackgroundMusic", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float FadeInDuration = 1.5f;

	UPROPERTY(EditDefaultsOnly, Category = "SP|BackgroundMusic", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float FadeOutDuration = 2.f;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> BackgroundAudioComponent;

	bool bMatchMusicEnabled = false;
	bool bIsAudible = false;
	bool bStartPending = false;
	int32 ChaseSuppressCount = 0;
};

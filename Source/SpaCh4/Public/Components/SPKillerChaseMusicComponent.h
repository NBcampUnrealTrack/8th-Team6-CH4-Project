#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SPKillerChaseMusicComponent.generated.h"

class AKillerCharacter;
class ASurvivorCharacter;
class UAudioComponent;
class USoundBase;
class USphereComponent;

UCLASS(ClassGroup = (SP), meta = (BlueprintSpawnableComponent))
class SPACH4_API USPKillerChaseMusicComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USPKillerChaseMusicComponent();

	USphereComponent* GetDetectionSphere() const { return DetectionSphere; }

protected:
	virtual void OnRegister() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	bool CanManageDetectionSphere() const;
	void SetupDetectionSphere();
	void UpdateDetectionSphereVisuals();
	float ResolveDefaultDetectionRadius() const;
	float GetDetectionRadius() const;
	void ApplyDetectionRadiusToSphere();

	UFUNCTION()
	void OnDetectionBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void OnDetectionEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);

	bool IsValidChaseTarget(const ASurvivorCharacter* Survivor) const;
	bool HasValidOverlappingSurvivor() const;
	bool ShouldLocalViewerHearChaseMusic() const;
	bool ShouldPlayAudio() const;

	void UpdateChaseMusicState();
	void StartChaseMusic();
	void StopChaseMusic();
	void NotifyBackgroundMusicChaseState(bool bChaseActive);

	AKillerCharacter* GetKiller() const;
	UObject* GetAudioContextObject() const;
	void ResolveChaseMusicAsset();
	void RefreshExistingOverlaps();
	void PlayEnterSound() const;
	void PlayExitSound() const;

	UPROPERTY(VisibleAnywhere, Category = "SP|ChaseMusic", meta = (DisplayName = "Chase Detection Sphere"))
	TObjectPtr<USphereComponent> DetectionSphere;

	UPROPERTY(EditAnywhere, Category = "SP|ChaseMusic", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm", ToolTip = "추격 감지 반경. 0이면 KillerData 값 사용. ChaseDetectionSphere에서 직접 수정해도 됩니다."))
	float DetectionRadius = 0.f;

	UPROPERTY(EditDefaultsOnly, Category = "SP|ChaseMusic")
	TObjectPtr<USoundBase> ChaseMusic;

	UPROPERTY(EditDefaultsOnly, Category = "SP|ChaseMusic")
	TObjectPtr<USoundBase> ChaseEnterSound;

	UPROPERTY(EditDefaultsOnly, Category = "SP|ChaseMusic")
	TObjectPtr<USoundBase> ChaseExitSound;

	UPROPERTY(EditDefaultsOnly, Category = "SP|ChaseMusic|Debug")
	bool bShowDetectionSphereInEditor = true;

	UPROPERTY(EditDefaultsOnly, Category = "SP|ChaseMusic|Debug")
	bool bShowDetectionSphereInGame = false;

	UPROPERTY(EditDefaultsOnly, Category = "SP|ChaseMusic")
	bool bPlayForLocalSurvivor = true;

	UPROPERTY(EditDefaultsOnly, Category = "SP|ChaseMusic")
	bool bPlayForKillerOwner = true;

	UPROPERTY(EditDefaultsOnly, Category = "SP|ChaseMusic", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float VolumeMultiplier = 1.f;

	UPROPERTY(EditDefaultsOnly, Category = "SP|ChaseMusic", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float PitchMultiplier = 1.f;

	UPROPERTY(EditDefaultsOnly, Category = "SP|ChaseMusic", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float FadeInDuration = 0.5f;

	UPROPERTY(EditDefaultsOnly, Category = "SP|ChaseMusic", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float FadeOutDuration = 0.75f;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> ChaseAudioComponent;

	TSet<TWeakObjectPtr<ASurvivorCharacter>> OverlappingSurvivors;

	bool bIsPlayingChaseMusic = false;
};

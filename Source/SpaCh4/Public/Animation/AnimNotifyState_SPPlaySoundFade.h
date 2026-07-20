#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AnimNotifyState_SPPlaySoundFade.generated.h"

class UAudioComponent;
class USkeletalMeshComponent;
class USoundBase;

UCLASS(meta = (DisplayName = "SP Play Sound Fade"))
class SPACH4_API UAnimNotifyState_SPPlaySoundFade : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UAnimNotifyState_SPPlaySoundFade();

	virtual void NotifyBegin(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		float TotalDuration,
		const FAnimNotifyEventReference& EventReference) override;

	virtual void NotifyEnd(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SP|Sound")
	TObjectPtr<USoundBase> Sound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SP|Sound", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float VolumeMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SP|Sound", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float PitchMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SP|Sound", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float FadeOutDuration = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SP|Sound")
	FName AttachSocketName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SP|Sound")
	bool bPreviewIgnoreAttenuation = true;

private:
	bool ShouldPlayAudio(const USkeletalMeshComponent* MeshComp) const;
	bool IsEditorPreviewWorld(const USkeletalMeshComponent* MeshComp) const;
	void ConfigureForEditorPreview(UAudioComponent* AudioComponent, const USkeletalMeshComponent* MeshComp) const;
	void StopActiveSound(USkeletalMeshComponent* MeshComp, float FadeTime) const;

	UPROPERTY(Transient)
	mutable TMap<TWeakObjectPtr<USkeletalMeshComponent>, TWeakObjectPtr<UAudioComponent>> ActiveAudioComponents;
};

#include "Animation/AnimNotifyState_SPPlaySoundFade.h"

#include "Components/AudioComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

UAnimNotifyState_SPPlaySoundFade::UAnimNotifyState_SPPlaySoundFade()
{
	bShouldFireInEditor = true;
}

void UAnimNotifyState_SPPlaySoundFade::NotifyBegin(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (!MeshComp || !Sound || !ShouldPlayAudio(MeshComp))
	{
		return;
	}

	StopActiveSound(MeshComp, 0.f);

	UAudioComponent* AudioComponent = UGameplayStatics::SpawnSoundAttached(
		Sound,
		MeshComp,
		AttachSocketName,
		FVector::ZeroVector,
		EAttachLocation::SnapToTarget,
		false,
		VolumeMultiplier,
		PitchMultiplier);

	if (!AudioComponent)
	{
		return;
	}

	ConfigureForEditorPreview(AudioComponent, MeshComp);
	ActiveAudioComponents.Add(MeshComp, AudioComponent);
}

void UAnimNotifyState_SPPlaySoundFade::NotifyEnd(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (!MeshComp)
	{
		return;
	}

	StopActiveSound(MeshComp, FadeOutDuration);
	ActiveAudioComponents.Remove(MeshComp);
}

FString UAnimNotifyState_SPPlaySoundFade::GetNotifyName_Implementation() const
{
	if (Sound)
	{
		return FString::Printf(TEXT("SP Play Sound Fade: %s"), *Sound->GetName());
	}

	return TEXT("SP Play Sound Fade");
}

bool UAnimNotifyState_SPPlaySoundFade::ShouldPlayAudio(const USkeletalMeshComponent* MeshComp) const
{
	const UWorld* World = MeshComp ? MeshComp->GetWorld() : nullptr;
	return World && World->GetNetMode() != NM_DedicatedServer;
}

bool UAnimNotifyState_SPPlaySoundFade::IsEditorPreviewWorld(const USkeletalMeshComponent* MeshComp) const
{
	const UWorld* World = MeshComp ? MeshComp->GetWorld() : nullptr;
	return World && World->WorldType == EWorldType::EditorPreview;
}

void UAnimNotifyState_SPPlaySoundFade::ConfigureForEditorPreview(
	UAudioComponent* AudioComponent,
	const USkeletalMeshComponent* MeshComp) const
{
	if (!AudioComponent || !bPreviewIgnoreAttenuation || !IsEditorPreviewWorld(MeshComp))
	{
		return;
	}

	AudioComponent->bAllowSpatialization = false;
	AudioComponent->bOverrideAttenuation = true;
	AudioComponent->AttenuationSettings = nullptr;
}

void UAnimNotifyState_SPPlaySoundFade::StopActiveSound(USkeletalMeshComponent* MeshComp, const float FadeTime) const
{
	if (!MeshComp)
	{
		return;
	}

	const TWeakObjectPtr<UAudioComponent>* FoundComponent = ActiveAudioComponents.Find(MeshComp);
	if (!FoundComponent)
	{
		return;
	}

	if (UAudioComponent* AudioComponent = FoundComponent->Get())
	{
		if (AudioComponent->IsPlaying())
		{
			if (FadeTime > KINDA_SMALL_NUMBER)
			{
				AudioComponent->FadeOut(FadeTime, 0.f);
			}
			else
			{
				AudioComponent->Stop();
			}
		}
	}
}

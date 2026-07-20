#include "Components/SPHatchSoundComponent.h"

#include "Components/AudioComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Gameplay/Escape/SPHatch.h"
#include "Kismet/GameplayStatics.h"

USPHatchSoundComponent::USPHatchSoundComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USPHatchSoundComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopWindLoopSound();
	Super::EndPlay(EndPlayReason);
}

void USPHatchSoundComponent::NotifyHatchStateChanged(const bool bHatchActive, const bool bHatchOpened)
{
	const bool bShouldPlayOpenPresentation = bHatchActive && bHatchOpened;

	if (bShouldPlayOpenPresentation)
	{
		if (!bIsPlayingOpenPresentation)
		{
			PlayOpenSounds();
			StartWindLoopSound();
			bIsPlayingOpenPresentation = true;
		}
		return;
	}

	if (bIsPlayingOpenPresentation)
	{
		StopWindLoopSound();
		bIsPlayingOpenPresentation = false;
	}
}

void USPHatchSoundComponent::PlayOpenSounds() const
{
	PlayOneShotSound(DoorOpenSound);
}

void USPHatchSoundComponent::StartWindLoopSound()
{
	if (!ShouldPlayAudio() || !WindLoopSound)
	{
		return;
	}

	AActor* Owner = GetOwner();
	USceneComponent* AttachComponent = GetAttachComponent();
	if (!Owner || !AttachComponent)
	{
		return;
	}

	if (!WindAudioComponent)
	{
		WindAudioComponent = NewObject<UAudioComponent>(Owner, TEXT("HatchWindLoopAudio"));
		WindAudioComponent->bAutoActivate = false;
		WindAudioComponent->bAutoDestroy = false;
		WindAudioComponent->SetupAttachment(AttachComponent);
		WindAudioComponent->RegisterComponent();
	}

	WindAudioComponent->Stop();
	WindAudioComponent->SetSound(WindLoopSound);
	WindAudioComponent->SetVolumeMultiplier(VolumeMultiplier);
	WindAudioComponent->SetPitchMultiplier(PitchMultiplier);
	WindAudioComponent->Play();
}

void USPHatchSoundComponent::StopWindLoopSound()
{
	if (!WindAudioComponent || !WindAudioComponent->IsPlaying())
	{
		return;
	}

	if (WindFadeOutDuration > 0.f)
	{
		WindAudioComponent->FadeOut(WindFadeOutDuration, 0.f);
	}
	else
	{
		WindAudioComponent->Stop();
	}
}

void USPHatchSoundComponent::PlayOneShotSound(USoundBase* Sound) const
{
	if (!Sound || !ShouldPlayAudio())
	{
		return;
	}

	USceneComponent* AttachComponent = GetAttachComponent();
	if (!AttachComponent)
	{
		return;
	}

	UGameplayStatics::SpawnSoundAttached(
		Sound,
		AttachComponent,
		NAME_None,
		FVector::ZeroVector,
		EAttachLocation::SnapToTarget,
		false,
		VolumeMultiplier,
		PitchMultiplier);
}

ASPHatch* USPHatchSoundComponent::GetHatch() const
{
	return Cast<ASPHatch>(GetOwner());
}

USceneComponent* USPHatchSoundComponent::GetAttachComponent() const
{
	if (const ASPHatch* Hatch = GetHatch())
	{
		if (UStaticMeshComponent* DoorMesh = Hatch->GetDoorMesh())
		{
			return DoorMesh;
		}

		if (UStaticMeshComponent* TrayMesh = Hatch->GetTrayMesh())
		{
			return TrayMesh;
		}

		return Hatch->GetRootComponent();
	}

	return GetOwner() ? GetOwner()->GetRootComponent() : nullptr;
}

bool USPHatchSoundComponent::ShouldPlayAudio() const
{
	const UWorld* World = GetWorld();
	return World && World->GetNetMode() != NM_DedicatedServer;
}

#include "Components/SPKillerCarrySoundComponent.h"

#include "Characters/Killer/KillerCharacter.h"
#include "Components/AudioComponent.h"
#include "Components/SPKillerCarryAnimComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Systems/Data/KillerData.h"
#include "TimerManager.h"

USPKillerCarrySoundComponent::USPKillerCarrySoundComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USPKillerCarrySoundComponent::BeginPlay()
{
	Super::BeginPlay();
	BindCarryAnimDelegates();
}

void USPKillerCarrySoundComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindCarryAnimDelegates();
	StopPickupSound(false);
	StopCarryingLoopSound();
	Super::EndPlay(EndPlayReason);
}

void USPKillerCarrySoundComponent::BindCarryAnimDelegates()
{
	if (USPKillerCarryAnimComponent* CarryAnimComponent = GetCarryAnimComponent())
	{
		CarryAnimComponent->OnPickupBegan.AddUObject(this, &USPKillerCarrySoundComponent::HandlePickupBegan);
		CarryAnimComponent->OnPickupAttachRequested.AddUObject(this, &USPKillerCarrySoundComponent::HandlePickupAttach);
		CarryAnimComponent->OnPickupFinished.AddUObject(this, &USPKillerCarrySoundComponent::HandlePickupFinished);
		CarryAnimComponent->OnCarryEnded.AddUObject(this, &USPKillerCarrySoundComponent::HandleCarryEnded);
	}
}

void USPKillerCarrySoundComponent::UnbindCarryAnimDelegates()
{
	if (USPKillerCarryAnimComponent* CarryAnimComponent = GetCarryAnimComponent())
	{
		CarryAnimComponent->OnPickupBegan.RemoveAll(this);
		CarryAnimComponent->OnPickupAttachRequested.RemoveAll(this);
		CarryAnimComponent->OnPickupFinished.RemoveAll(this);
		CarryAnimComponent->OnCarryEnded.RemoveAll(this);
	}
}

void USPKillerCarrySoundComponent::HandlePickupBegan()
{
	StartPickupSound();
}

void USPKillerCarrySoundComponent::HandlePickupAttach()
{
	PlayOneShotSound(PickupAttachSound);
}

void USPKillerCarrySoundComponent::HandlePickupFinished()
{
	StopPickupSound(false);
	StartCarryingLoopSound();
}

void USPKillerCarrySoundComponent::HandleCarryEnded()
{
	StopCarryingLoopSound();
	PlayOneShotSound(DropSound);
}

float USPKillerCarrySoundComponent::GetPickupSoundDuration() const
{
	if (PickupDurationOverride > 0.f)
	{
		return PickupDurationOverride;
	}

	if (const USPKillerCarryAnimComponent* CarryAnimComponent = GetCarryAnimComponent())
	{
		return CarryAnimComponent->GetPickupMontagePlayLength();
	}

	if (const AKillerCharacter* Killer = GetKiller())
	{
		if (const UKillerData* Data = Killer->GetKillerData())
		{
			return Data->PickupDuration;
		}
	}

	return 1.25f;
}

void USPKillerCarrySoundComponent::StartPickupSound()
{
	if (bIsPlayingPickupSound || !ShouldPlayAudio() || !PickupSound)
	{
		return;
	}

	AActor* Owner = GetOwner();
	USceneComponent* AttachComponent = GetAttachComponent();
	UWorld* World = GetWorld();
	if (!Owner || !AttachComponent || !World)
	{
		return;
	}

	World->GetTimerManager().ClearTimer(PickupFadeTimerHandle);
	World->GetTimerManager().ClearTimer(PickupStopTimerHandle);

	if (!PickupAudioComponent)
	{
		PickupAudioComponent = NewObject<UAudioComponent>(Owner, TEXT("PickupCarryAudio"));
		PickupAudioComponent->bAutoActivate = false;
		PickupAudioComponent->bAutoDestroy = false;
		PickupAudioComponent->SetupAttachment(AttachComponent);
		PickupAudioComponent->RegisterComponent();
	}

	PickupAudioComponent->Stop();
	PickupAudioComponent->SetSound(PickupSound);
	PickupAudioComponent->SetVolumeMultiplier(VolumeMultiplier);
	PickupAudioComponent->SetPitchMultiplier(PitchMultiplier);
	PickupAudioComponent->Play();
	bIsPlayingPickupSound = true;

	const float Duration = GetPickupSoundDuration();
	const float FadeStartDelay = FMath::Max(0.f, Duration - PickupFadeOutDuration);

	if (PickupFadeOutDuration > 0.f)
	{
		World->GetTimerManager().SetTimer(
			PickupFadeTimerHandle,
			[this]()
			{
				if (PickupAudioComponent && PickupAudioComponent->IsPlaying())
				{
					PickupAudioComponent->FadeOut(PickupFadeOutDuration, 0.f);
				}
			},
			FadeStartDelay,
			false);
	}

	if (Duration > 0.f)
	{
		World->GetTimerManager().SetTimer(
			PickupStopTimerHandle,
			[this]()
			{
				StopPickupSound(false);
			},
			Duration,
			false);
	}
}

void USPKillerCarrySoundComponent::StopPickupSound(const bool bPlayDropSound)
{
	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().ClearTimer(PickupFadeTimerHandle);
		World->GetTimerManager().ClearTimer(PickupStopTimerHandle);
	}

	if (bPlayDropSound)
	{
		PlayOneShotSound(DropSound);
	}

	if (PickupAudioComponent && PickupAudioComponent->IsPlaying())
	{
		if (PickupFadeOutDuration > 0.f)
		{
			PickupAudioComponent->FadeOut(PickupFadeOutDuration, 0.f);
		}
		else
		{
			PickupAudioComponent->Stop();
		}
	}

	bIsPlayingPickupSound = false;
}

void USPKillerCarrySoundComponent::StartCarryingLoopSound()
{
	if (bIsPlayingCarryingLoop || !ShouldPlayAudio() || !CarryingLoopSound)
	{
		return;
	}

	AActor* Owner = GetOwner();
	USceneComponent* AttachComponent = GetAttachComponent();
	if (!Owner || !AttachComponent)
	{
		return;
	}

	if (!CarryingAudioComponent)
	{
		CarryingAudioComponent = NewObject<UAudioComponent>(Owner, TEXT("CarryingLoopAudio"));
		CarryingAudioComponent->bAutoActivate = false;
		CarryingAudioComponent->bAutoDestroy = false;
		CarryingAudioComponent->SetupAttachment(AttachComponent);
		CarryingAudioComponent->RegisterComponent();
	}

	CarryingAudioComponent->SetSound(CarryingLoopSound);
	CarryingAudioComponent->SetVolumeMultiplier(VolumeMultiplier);
	CarryingAudioComponent->SetPitchMultiplier(PitchMultiplier);
	CarryingAudioComponent->Play();
	bIsPlayingCarryingLoop = true;
}

void USPKillerCarrySoundComponent::StopCarryingLoopSound()
{
	if (!bIsPlayingCarryingLoop)
	{
		return;
	}

	if (CarryingAudioComponent && CarryingAudioComponent->IsPlaying())
	{
		if (CarryingFadeOutDuration > 0.f)
		{
			CarryingAudioComponent->FadeOut(CarryingFadeOutDuration, 0.f);
		}
		else
		{
			CarryingAudioComponent->Stop();
		}
	}

	bIsPlayingCarryingLoop = false;
}

void USPKillerCarrySoundComponent::PlayOneShotSound(USoundBase* Sound) const
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

AKillerCharacter* USPKillerCarrySoundComponent::GetKiller() const
{
	return Cast<AKillerCharacter>(GetOwner());
}

USPKillerCarryAnimComponent* USPKillerCarrySoundComponent::GetCarryAnimComponent() const
{
	if (const AKillerCharacter* Killer = GetKiller())
	{
		return Killer->GetCarryAnimComponent();
	}

	return GetOwner() ? GetOwner()->FindComponentByClass<USPKillerCarryAnimComponent>() : nullptr;
}

USceneComponent* USPKillerCarrySoundComponent::GetAttachComponent() const
{
	if (const AKillerCharacter* Killer = GetKiller())
	{
		if (USkeletalMeshComponent* Mesh = Killer->GetMesh())
		{
			return Mesh;
		}

		return Killer->GetRootComponent();
	}

	return nullptr;
}

bool USPKillerCarrySoundComponent::ShouldPlayAudio() const
{
	const UWorld* World = GetWorld();
	return World && World->GetNetMode() != NM_DedicatedServer;
}

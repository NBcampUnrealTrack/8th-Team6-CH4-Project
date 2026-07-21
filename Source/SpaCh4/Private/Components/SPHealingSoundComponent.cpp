#include "Components/SPHealingSoundComponent.h"

#include "Characters/Survivor/SurvivorCharacter.h"
#include "Components/AudioComponent.h"
#include "Components/SPHealingAnimComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"

USPHealingSoundComponent::USPHealingSoundComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USPHealingSoundComponent::BeginPlay()
{
	Super::BeginPlay();
	BindHealingAnimDelegates();
}

void USPHealingSoundComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindHealingAnimDelegates();
	StopHealingLoopSound();
	Super::EndPlay(EndPlayReason);
}

void USPHealingSoundComponent::BindHealingAnimDelegates()
{
	if (USPHealingAnimComponent* HealingAnimComponent = GetHealingAnimComponent())
	{
		HealingAnimComponent->OnHealingLoopBegan.AddUObject(this, &USPHealingSoundComponent::HandleHealingBegan);
		HealingAnimComponent->OnHealingChannelEnded.AddUObject(this, &USPHealingSoundComponent::HandleHealingEnded);
	}
}

void USPHealingSoundComponent::UnbindHealingAnimDelegates()
{
	if (USPHealingAnimComponent* HealingAnimComponent = GetHealingAnimComponent())
	{
		HealingAnimComponent->OnHealingLoopBegan.RemoveAll(this);
		HealingAnimComponent->OnHealingChannelEnded.RemoveAll(this);
	}
}

void USPHealingSoundComponent::HandleHealingBegan()
{
	StartHealingLoopSound();
}

void USPHealingSoundComponent::HandleHealingEnded(bool /*bCompleted*/)
{
	StopHealingLoopSound();
}

void USPHealingSoundComponent::StartHealingLoopSound()
{
	if (bIsPlayingLoop || !ShouldPlayAudio() || !HealingLoopSound)
	{
		return;
	}

	AActor* Owner = GetOwner();
	USceneComponent* AttachComponent = GetAttachComponent();
	if (!Owner || !AttachComponent)
	{
		return;
	}

	if (!LoopAudioComponent)
	{
		LoopAudioComponent = NewObject<UAudioComponent>(Owner, TEXT("HealingLoopAudio"));
		LoopAudioComponent->bAutoActivate = false;
		LoopAudioComponent->bAutoDestroy = false;
		LoopAudioComponent->SetupAttachment(AttachComponent);
		LoopAudioComponent->RegisterComponent();
	}

	LoopAudioComponent->Stop();
	LoopAudioComponent->SetSound(HealingLoopSound);
	LoopAudioComponent->SetVolumeMultiplier(VolumeMultiplier);
	LoopAudioComponent->SetPitchMultiplier(PitchMultiplier);
	LoopAudioComponent->Play();
	bIsPlayingLoop = true;
}

void USPHealingSoundComponent::StopHealingLoopSound()
{
	if (!bIsPlayingLoop)
	{
		return;
	}

	if (LoopAudioComponent && LoopAudioComponent->IsPlaying())
	{
		if (HealingFadeOutDuration > 0.f)
		{
			LoopAudioComponent->FadeOut(HealingFadeOutDuration, 0.f);
		}
		else
		{
			LoopAudioComponent->Stop();
		}
	}

	bIsPlayingLoop = false;
}

ASurvivorCharacter* USPHealingSoundComponent::GetSurvivor() const
{
	return Cast<ASurvivorCharacter>(GetOwner());
}

USPHealingAnimComponent* USPHealingSoundComponent::GetHealingAnimComponent() const
{
	if (const ASurvivorCharacter* Survivor = GetSurvivor())
	{
		return Survivor->GetHealingAnimComponent();
	}

	return nullptr;
}

USceneComponent* USPHealingSoundComponent::GetAttachComponent() const
{
	if (const ASurvivorCharacter* Survivor = GetSurvivor())
	{
		if (USkeletalMeshComponent* Mesh = Survivor->GetMesh())
		{
			return Mesh;
		}

		return Survivor->GetRootComponent();
	}

	return nullptr;
}

bool USPHealingSoundComponent::ShouldPlayAudio() const
{
	const UWorld* World = GetWorld();
	return World && World->GetNetMode() != NM_DedicatedServer;
}

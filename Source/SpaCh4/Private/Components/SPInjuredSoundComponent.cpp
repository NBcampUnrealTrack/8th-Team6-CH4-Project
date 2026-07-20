#include "Components/SPInjuredSoundComponent.h"

#include "Characters/Survivor/SurvivorCharacter.h"
#include "Components/AudioComponent.h"
#include "Components/SceneComponent.h"
#include "Kismet/GameplayStatics.h"

USPInjuredSoundComponent::USPInjuredSoundComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USPInjuredSoundComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!ShouldPlayAudio())
	{
		return;
	}

	if (const ASurvivorCharacter* Survivor = GetSurvivor())
	{
		if (Survivor->GetSurvivorState() == ESurvivorState::Injured)
		{
			StartInjuredSound();
		}
	}
}

void USPInjuredSoundComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopInjuredSound();
	Super::EndPlay(EndPlayReason);
}

void USPInjuredSoundComponent::HandleStateTransition(const ESurvivorState OldState, const ESurvivorState NewState)
{
	if (!ShouldPlayAudio())
	{
		return;
	}

	const bool bWasInjured = OldState == ESurvivorState::Injured;
	const bool bIsInjured = NewState == ESurvivorState::Injured;

	if (!bWasInjured && bIsInjured)
	{
		StartInjuredSound();
	}
	else if (bWasInjured && !bIsInjured)
	{
		StopInjuredSound();
	}
}

void USPInjuredSoundComponent::StartInjuredSound()
{
	if (bIsPlayingLoop)
	{
		return;
	}

	PlayEnterSound();

	if (!InjuredLoopSound)
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
		LoopAudioComponent = NewObject<UAudioComponent>(Owner, TEXT("InjuredLoopAudio"));
		LoopAudioComponent->bAutoActivate = false;
		LoopAudioComponent->bAutoDestroy = false;
		LoopAudioComponent->SetupAttachment(AttachComponent);
		LoopAudioComponent->RegisterComponent();
	}

	LoopAudioComponent->SetSound(InjuredLoopSound);
	LoopAudioComponent->SetVolumeMultiplier(VolumeMultiplier);
	LoopAudioComponent->SetPitchMultiplier(PitchMultiplier);
	LoopAudioComponent->Play();
	bIsPlayingLoop = true;
}

void USPInjuredSoundComponent::StopInjuredSound()
{
	PlayExitSound();

	if (LoopAudioComponent && LoopAudioComponent->IsPlaying())
	{
		LoopAudioComponent->Stop();
	}

	bIsPlayingLoop = false;
}

void USPInjuredSoundComponent::PlayEnterSound() const
{
	if (!InjuredEnterSound)
	{
		return;
	}

	USceneComponent* AttachComponent = GetAttachComponent();
	if (!AttachComponent)
	{
		return;
	}

	UGameplayStatics::SpawnSoundAttached(
		InjuredEnterSound,
		AttachComponent,
		NAME_None,
		FVector::ZeroVector,
		EAttachLocation::SnapToTarget,
		false,
		VolumeMultiplier,
		PitchMultiplier);
}

void USPInjuredSoundComponent::PlayExitSound() const
{
	if (!InjuredExitSound)
	{
		return;
	}

	USceneComponent* AttachComponent = GetAttachComponent();
	if (!AttachComponent)
	{
		return;
	}

	UGameplayStatics::SpawnSoundAttached(
		InjuredExitSound,
		AttachComponent,
		NAME_None,
		FVector::ZeroVector,
		EAttachLocation::SnapToTarget,
		false,
		VolumeMultiplier,
		PitchMultiplier);
}

ASurvivorCharacter* USPInjuredSoundComponent::GetSurvivor() const
{
	return Cast<ASurvivorCharacter>(GetOwner());
}

USceneComponent* USPInjuredSoundComponent::GetAttachComponent() const
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

bool USPInjuredSoundComponent::ShouldPlayAudio() const
{
	const UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return false;
	}

	const ASurvivorCharacter* Survivor = GetSurvivor();
	return Survivor && Survivor->IsLocallyControlled();
}

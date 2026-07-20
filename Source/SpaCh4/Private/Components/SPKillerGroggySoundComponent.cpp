#include "Components/SPKillerGroggySoundComponent.h"

#include "Characters/Killer/KillerCharacter.h"
#include "Components/AudioComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Systems/Data/KillerData.h"
#include "TimerManager.h"

USPKillerGroggySoundComponent::USPKillerGroggySoundComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USPKillerGroggySoundComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!ShouldPlayAudio())
	{
		return;
	}

	if (const AKillerCharacter* Killer = GetKiller())
	{
		if (Killer->GetKillerState() == EKillerState::Groggy)
		{
			StartGroggySound();
			LastKnownState = EKillerState::Groggy;
			bHasLastKnownState = true;
		}
	}
}

void USPKillerGroggySoundComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopGroggySound(false);
	Super::EndPlay(EndPlayReason);
}

void USPKillerGroggySoundComponent::SyncToKillerState(const EKillerState NewState)
{
	if (bHasLastKnownState && LastKnownState != NewState)
	{
		HandleKillerStateTransition(LastKnownState, NewState);
	}
	else if (!bHasLastKnownState && NewState == EKillerState::Groggy)
	{
		HandleKillerStateTransition(EKillerState::Idle, NewState);
	}

	LastKnownState = NewState;
	bHasLastKnownState = true;
}

void USPKillerGroggySoundComponent::HandleKillerStateTransition(
	const EKillerState OldState,
	const EKillerState NewState)
{
	if (!ShouldPlayAudio())
	{
		return;
	}

	const bool bWasGroggy = OldState == EKillerState::Groggy;
	const bool bIsGroggy = NewState == EKillerState::Groggy;

	if (!bWasGroggy && bIsGroggy)
	{
		StartGroggySound();
	}
	else if (bWasGroggy && !bIsGroggy)
	{
		StopGroggySound(true);
	}
}

void USPKillerGroggySoundComponent::StartGroggySound()
{
	if (bIsPlayingGroggySound)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	World->GetTimerManager().ClearTimer(GroggyStopTimerHandle);
	World->GetTimerManager().ClearTimer(GroggyFadeTimerHandle);

	PlayEnterSound();

	if (!GroggySound)
	{
		return;
	}

	AActor* Owner = GetOwner();
	USceneComponent* AttachComponent = GetAttachComponent();
	if (!Owner || !AttachComponent)
	{
		return;
	}

	if (!GroggyAudioComponent)
	{
		GroggyAudioComponent = NewObject<UAudioComponent>(Owner, TEXT("GroggyLoopAudio"));
		GroggyAudioComponent->bAutoActivate = false;
		GroggyAudioComponent->bAutoDestroy = false;
		GroggyAudioComponent->SetupAttachment(AttachComponent);
		GroggyAudioComponent->RegisterComponent();
	}

	GroggyAudioComponent->SetSound(GroggySound);
	GroggyAudioComponent->SetVolumeMultiplier(VolumeMultiplier);
	GroggyAudioComponent->SetPitchMultiplier(PitchMultiplier);
	GroggyAudioComponent->Play();
	bIsPlayingGroggySound = true;

	const float Duration = GetGroggyDuration();
	const float FadeStartDelay = FMath::Max(0.f, Duration - FadeOutDuration);

	if (FadeOutDuration > 0.f && FadeStartDelay >= 0.f)
	{
		World->GetTimerManager().SetTimer(
			GroggyFadeTimerHandle,
			[this]()
			{
				if (GroggyAudioComponent && GroggyAudioComponent->IsPlaying())
				{
					GroggyAudioComponent->FadeOut(FadeOutDuration, 0.f);
				}
			},
			FadeStartDelay,
			false);
	}

	if (Duration > 0.f)
	{
		World->GetTimerManager().SetTimer(
			GroggyStopTimerHandle,
			[this]()
			{
				StopGroggySound(false);
			},
			Duration,
			false);
	}
}

void USPKillerGroggySoundComponent::StopGroggySound(const bool bPlayExitSound)
{
	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().ClearTimer(GroggyStopTimerHandle);
		World->GetTimerManager().ClearTimer(GroggyFadeTimerHandle);
	}

	if (bPlayExitSound)
	{
		PlayExitSound();
	}

	if (GroggyAudioComponent && GroggyAudioComponent->IsPlaying())
	{
		if (FadeOutDuration > 0.f)
		{
			GroggyAudioComponent->FadeOut(FadeOutDuration, 0.f);
		}
		else
		{
			GroggyAudioComponent->Stop();
		}
	}

	bIsPlayingGroggySound = false;
}

void USPKillerGroggySoundComponent::PlayEnterSound() const
{
	if (!GroggyEnterSound)
	{
		return;
	}

	USceneComponent* AttachComponent = GetAttachComponent();
	if (!AttachComponent)
	{
		return;
	}

	UGameplayStatics::SpawnSoundAttached(
		GroggyEnterSound,
		AttachComponent,
		NAME_None,
		FVector::ZeroVector,
		EAttachLocation::SnapToTarget,
		false,
		VolumeMultiplier,
		PitchMultiplier);
}

void USPKillerGroggySoundComponent::PlayExitSound() const
{
	if (!GroggyExitSound)
	{
		return;
	}

	USceneComponent* AttachComponent = GetAttachComponent();
	if (!AttachComponent)
	{
		return;
	}

	UGameplayStatics::SpawnSoundAttached(
		GroggyExitSound,
		AttachComponent,
		NAME_None,
		FVector::ZeroVector,
		EAttachLocation::SnapToTarget,
		false,
		VolumeMultiplier,
		PitchMultiplier);
}

AKillerCharacter* USPKillerGroggySoundComponent::GetKiller() const
{
	return Cast<AKillerCharacter>(GetOwner());
}

USceneComponent* USPKillerGroggySoundComponent::GetAttachComponent() const
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

float USPKillerGroggySoundComponent::GetGroggyDuration() const
{
	if (GroggyDurationOverride > 0.f)
	{
		return GroggyDurationOverride;
	}

	if (const AKillerCharacter* Killer = GetKiller())
	{
		if (const UKillerData* Data = Killer->GetKillerData())
		{
			return Data->TaserGroggyOnMiss;
		}
	}

	return 1.f;
}

bool USPKillerGroggySoundComponent::ShouldPlayAudio() const
{
	const UWorld* World = GetWorld();
	return World && World->GetNetMode() != NM_DedicatedServer;
}

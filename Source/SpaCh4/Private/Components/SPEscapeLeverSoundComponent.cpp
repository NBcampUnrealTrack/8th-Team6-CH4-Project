#include "Components/SPEscapeLeverSoundComponent.h"

#include "Components/AudioComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Gameplay/Escape/SPEscapeGate.h"
#include "Kismet/GameplayStatics.h"

USPEscapeLeverSoundComponent::USPEscapeLeverSoundComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USPEscapeLeverSoundComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopChannelLoopSound(false);
	Super::EndPlay(EndPlayReason);
}

void USPEscapeLeverSoundComponent::NotifyChannelStarted()
{
	StartChannelLoopSound();
}

void USPEscapeLeverSoundComponent::NotifyChannelStopped(const bool bCompleted)
{
	StopChannelLoopSound(bCompleted);
}

void USPEscapeLeverSoundComponent::StartChannelLoopSound()
{
	if (bIsPlayingChannelLoop || !ShouldPlayAudio() || !ChannelLoopSound)
	{
		return;
	}

	AActor* Owner = GetOwner();
	USceneComponent* AttachComponent = GetAttachComponent();
	if (!Owner || !AttachComponent)
	{
		return;
	}

	if (!ChannelAudioComponent)
	{
		ChannelAudioComponent = NewObject<UAudioComponent>(Owner, TEXT("LeverChannelLoopAudio"));
		ChannelAudioComponent->bAutoActivate = false;
		ChannelAudioComponent->bAutoDestroy = false;
		ChannelAudioComponent->SetupAttachment(AttachComponent);
		ChannelAudioComponent->RegisterComponent();
	}

	ChannelAudioComponent->Stop();
	ChannelAudioComponent->SetSound(ChannelLoopSound);
	ChannelAudioComponent->SetVolumeMultiplier(VolumeMultiplier);
	ChannelAudioComponent->SetPitchMultiplier(PitchMultiplier);
	ChannelAudioComponent->Play();
	bIsPlayingChannelLoop = true;
}

void USPEscapeLeverSoundComponent::StopChannelLoopSound(const bool bPlayGateOpenSound)
{
	if (!bIsPlayingChannelLoop)
	{
		if (bPlayGateOpenSound)
		{
			PlayGateOpenSound();
		}
		return;
	}

	if (ChannelAudioComponent && ChannelAudioComponent->IsPlaying())
	{
		if (ChannelFadeOutDuration > 0.f)
		{
			ChannelAudioComponent->FadeOut(ChannelFadeOutDuration, 0.f);
		}
		else
		{
			ChannelAudioComponent->Stop();
		}
	}

	bIsPlayingChannelLoop = false;

	if (bPlayGateOpenSound)
	{
		PlayGateOpenSound();
	}
}

void USPEscapeLeverSoundComponent::PlayGateOpenSound() const
{
	PlayOneShotSound(GateOpenSound);
}

void USPEscapeLeverSoundComponent::PlayOneShotSound(USoundBase* Sound) const
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

ASPEscapeGate* USPEscapeLeverSoundComponent::GetEscapeGate() const
{
	return Cast<ASPEscapeGate>(GetOwner());
}

USceneComponent* USPEscapeLeverSoundComponent::GetAttachComponent() const
{
	if (const ASPEscapeGate* Gate = GetEscapeGate())
	{
		if (USceneComponent* LeverPivot = Gate->GetLeverPivot())
		{
			return LeverPivot;
		}

		if (UStaticMeshComponent* SwitchMesh = Gate->GetSwitchMesh())
		{
			return SwitchMesh;
		}

		return Gate->GetRootComponent();
	}

	return GetOwner() ? GetOwner()->GetRootComponent() : nullptr;
}

bool USPEscapeLeverSoundComponent::ShouldPlayAudio() const
{
	const UWorld* World = GetWorld();
	return World && World->GetNetMode() != NM_DedicatedServer;
}

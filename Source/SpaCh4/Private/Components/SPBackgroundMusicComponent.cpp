#include "Components/SPBackgroundMusicComponent.h"

#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Systems/MatchGameState.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace SPBackgroundMusicPaths
{
	const TCHAR* DefaultBackgroundMusic = TEXT(
		"/Game/Sound/BackGround/MUSCSong_Black_Moon_GoAg_SWSH_Eminor-52BPM-2448-Full.MUSCSong_Black_Moon_GoAg_SWSH_Eminor-52BPM-2448-Full");
}

USPBackgroundMusicComponent::USPBackgroundMusicComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	static ConstructorHelpers::FObjectFinder<USoundBase> BackgroundMusicFinder(SPBackgroundMusicPaths::DefaultBackgroundMusic);
	if (BackgroundMusicFinder.Succeeded())
	{
		BackgroundMusic = BackgroundMusicFinder.Object;
	}
}

void USPBackgroundMusicComponent::BeginPlay()
{
	Super::BeginPlay();

	ResolveBackgroundMusicAsset();

	if (AMatchGameState* MatchGameState = Cast<AMatchGameState>(GetOwner()))
	{
		MatchGameState->OnMatchPhaseChanged.AddDynamic(this, &USPBackgroundMusicComponent::HandleMatchPhaseChanged);
		SyncToMatchPhase(MatchGameState->GetMatchPhase());

		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimerForNextTick([this]()
			{
				if (const AMatchGameState* MatchGameState = Cast<AMatchGameState>(GetOwner()))
				{
					SyncToMatchPhase(MatchGameState->GetMatchPhase());
				}
			});
		}
	}
}

void USPBackgroundMusicComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearAllTimersForObject(this);
	}

	if (AMatchGameState* MatchGameState = Cast<AMatchGameState>(GetOwner()))
	{
		MatchGameState->OnMatchPhaseChanged.RemoveDynamic(this, &USPBackgroundMusicComponent::HandleMatchPhaseChanged);
	}

	StopBackgroundMusic();
	Super::EndPlay(EndPlayReason);
}

void USPBackgroundMusicComponent::NotifyChaseMusicStarted()
{
	ChaseSuppressCount = FMath::Max(0, ChaseSuppressCount) + 1;
	if (ChaseSuppressCount != 1)
	{
		return;
	}

	DuckBackgroundMusic();
}

void USPBackgroundMusicComponent::NotifyChaseMusicStopped()
{
	if (ChaseSuppressCount <= 0)
	{
		return;
	}

	ChaseSuppressCount = FMath::Max(0, ChaseSuppressCount - 1);
	if (ChaseSuppressCount > 0)
	{
		return;
	}

	RestoreBackgroundMusic();
}

void USPBackgroundMusicComponent::SyncToMatchPhase(const EMatchPhase NewMatchPhase)
{
	if (ShouldPlayForPhase(NewMatchPhase))
	{
		StartBackgroundMusic();
	}
	else
	{
		StopBackgroundMusic();
	}
}

void USPBackgroundMusicComponent::HandleMatchPhaseChanged(const EMatchPhase NewMatchPhase)
{
	SyncToMatchPhase(NewMatchPhase);
}

void USPBackgroundMusicComponent::ResolveBackgroundMusicAsset()
{
	if (BackgroundMusic)
	{
		return;
	}

	BackgroundMusic = LoadObject<USoundBase>(nullptr, SPBackgroundMusicPaths::DefaultBackgroundMusic);
	if (!BackgroundMusic)
	{
		UE_LOG(LogTemp, Error, TEXT("SPBackgroundMusic: Failed to load default music at %s"), SPBackgroundMusicPaths::DefaultBackgroundMusic);
	}
}

UObject* USPBackgroundMusicComponent::GetAudioContextObject() const
{
	if (const UWorld* World = GetWorld())
	{
		if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(World, 0))
		{
			return PlayerController;
		}

		return const_cast<UWorld*>(World);
	}

	return const_cast<UObject*>(static_cast<const UObject*>(GetOwner()));
}

bool USPBackgroundMusicComponent::ShouldPlayForPhase(const EMatchPhase MatchPhase) const
{
	return MatchPhase == EMatchPhase::Playing || MatchPhase == EMatchPhase::EscapeActive;
}

bool USPBackgroundMusicComponent::ShouldPlayAudio() const
{
	const UWorld* World = GetWorld();
	return World && World->GetNetMode() != NM_DedicatedServer;
}

void USPBackgroundMusicComponent::EnsureAudioComponent()
{
	if (BackgroundAudioComponent || !BackgroundMusic)
	{
		return;
	}

	UObject* AudioContext = GetAudioContextObject();
	if (!AudioContext)
	{
		return;
	}

	BackgroundAudioComponent = UGameplayStatics::SpawnSound2D(
		AudioContext,
		BackgroundMusic,
		0.f,
		PitchMultiplier,
		0.f,
		nullptr,
		false,
		false);

	if (BackgroundAudioComponent)
	{
		BackgroundAudioComponent->bAllowSpatialization = false;
		BackgroundAudioComponent->bAutoDestroy = false;
	}
}

void USPBackgroundMusicComponent::StartBackgroundMusic()
{
	ResolveBackgroundMusicAsset();

	if (bMatchMusicEnabled || bStartPending || !ShouldPlayAudio() || !BackgroundMusic)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	bStartPending = true;
	World->GetTimerManager().SetTimerForNextTick([this]()
	{
		bStartPending = false;
		StartBackgroundMusicInternal();
	});
}

void USPBackgroundMusicComponent::StartBackgroundMusicInternal()
{
	ResolveBackgroundMusicAsset();

	if (bMatchMusicEnabled || !ShouldPlayAudio() || !BackgroundMusic)
	{
		return;
	}

	UObject* AudioContext = GetAudioContextObject();
	if (!AudioContext)
	{
		UE_LOG(LogTemp, Warning, TEXT("SPBackgroundMusic: No audio context object"));
		return;
	}

	if (!BackgroundAudioComponent || !IsValid(BackgroundAudioComponent))
	{
		BackgroundAudioComponent = UGameplayStatics::SpawnSound2D(
			AudioContext,
			BackgroundMusic,
			0.f,
			PitchMultiplier,
			0.f,
			nullptr,
			false,
			false);

		if (BackgroundAudioComponent)
		{
			BackgroundAudioComponent->bAllowSpatialization = false;
			BackgroundAudioComponent->bAutoDestroy = false;
		}
	}

	if (!BackgroundAudioComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("SPBackgroundMusic: Failed to create audio component for %s"), *BackgroundMusic->GetName());
		return;
	}

	BackgroundAudioComponent->SetSound(BackgroundMusic);
	BackgroundAudioComponent->SetPitchMultiplier(PitchMultiplier);

	const float TargetVolume = ChaseSuppressCount > 0 ? 0.f : VolumeMultiplier;
	BackgroundAudioComponent->SetVolumeMultiplier(TargetVolume);

	if (!BackgroundAudioComponent->IsPlaying())
	{
		BackgroundAudioComponent->Play();
	}

	bMatchMusicEnabled = true;
	bIsAudible = ChaseSuppressCount <= 0;

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("SPBackgroundMusic: Started %s playing=%d vol=%.2f"),
		*BackgroundMusic->GetName(),
		BackgroundAudioComponent->IsPlaying() ? 1 : 0,
		BackgroundAudioComponent->VolumeMultiplier);
}

void USPBackgroundMusicComponent::StopBackgroundMusic()
{
	bStartPending = false;

	if (!bMatchMusicEnabled)
	{
		return;
	}

	bMatchMusicEnabled = false;
	bIsAudible = false;
	ChaseSuppressCount = 0;

	if (BackgroundAudioComponent && BackgroundAudioComponent->IsPlaying())
	{
		if (FadeOutDuration > 0.f)
		{
			BackgroundAudioComponent->FadeOut(FadeOutDuration, 0.f);
		}
		else
		{
			BackgroundAudioComponent->Stop();
		}
	}
}

void USPBackgroundMusicComponent::DuckBackgroundMusic()
{
	if (!bMatchMusicEnabled || !ShouldPlayAudio() || !BackgroundMusic)
	{
		return;
	}

	bIsAudible = false;

	if (!BackgroundAudioComponent || !IsValid(BackgroundAudioComponent))
	{
		return;
	}

	// Do not FadeOut here: an in-progress fade can keep running after Restore and mute BGM again.
	if (BackgroundAudioComponent->IsPlaying())
	{
		BackgroundAudioComponent->SetVolumeMultiplier(0.f);
	}
}

void USPBackgroundMusicComponent::RestoreBackgroundMusic()
{
	if (!bMatchMusicEnabled || !ShouldPlayAudio() || !BackgroundMusic)
	{
		return;
	}

	if (ChaseSuppressCount > 0)
	{
		return;
	}

	if (!BackgroundAudioComponent || !IsValid(BackgroundAudioComponent))
	{
		StartBackgroundMusicInternal();
		return;
	}

	if (!BackgroundAudioComponent->IsPlaying())
	{
		BackgroundAudioComponent->SetSound(BackgroundMusic);
		BackgroundAudioComponent->SetPitchMultiplier(PitchMultiplier);
		BackgroundAudioComponent->Play();
	}

	if (FadeInDuration > 0.f)
	{
		BackgroundAudioComponent->FadeIn(FadeInDuration, VolumeMultiplier);
	}
	else
	{
		BackgroundAudioComponent->SetVolumeMultiplier(VolumeMultiplier);
	}

	bIsAudible = true;

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("SPBackgroundMusic: Restored after chase playing=%d vol=%.2f"),
		BackgroundAudioComponent->IsPlaying() ? 1 : 0,
		BackgroundAudioComponent->VolumeMultiplier);
}

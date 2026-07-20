#include "Components/SPBackgroundMusicComponent.h"

#include "Components/AudioComponent.h"
#include "Systems/MatchGameState.h"
#include "UObject/ConstructorHelpers.h"

USPBackgroundMusicComponent::USPBackgroundMusicComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	static ConstructorHelpers::FObjectFinder<USoundBase> BackgroundMusicFinder(
		TEXT("/Game/Sound/BackGround/MUSCSong_Black_Moon_GoAg_SWSH_Eminor-52BPM-2448-Full.MUSCSong_Black_Moon_GoAg_SWSH_Eminor-52BPM-2448-Full"));
	if (BackgroundMusicFinder.Succeeded())
	{
		BackgroundMusic = BackgroundMusicFinder.Object;
	}
}

void USPBackgroundMusicComponent::BeginPlay()
{
	Super::BeginPlay();

	if (AMatchGameState* MatchGameState = Cast<AMatchGameState>(GetOwner()))
	{
		MatchGameState->OnMatchPhaseChanged.AddDynamic(this, &USPBackgroundMusicComponent::HandleMatchPhaseChanged);
		HandleMatchPhaseChanged(MatchGameState->GetMatchPhase());
	}
}

void USPBackgroundMusicComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
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

void USPBackgroundMusicComponent::HandleMatchPhaseChanged(const EMatchPhase NewMatchPhase)
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
	AActor* Owner = GetOwner();
	if (!Owner || BackgroundAudioComponent)
	{
		return;
	}

	BackgroundAudioComponent = NewObject<UAudioComponent>(Owner, TEXT("BackgroundMusicAudio"));
	BackgroundAudioComponent->bAutoActivate = false;
	BackgroundAudioComponent->bAutoDestroy = false;
	BackgroundAudioComponent->bAllowSpatialization = false;
	BackgroundAudioComponent->RegisterComponent();
}

void USPBackgroundMusicComponent::StartBackgroundMusic()
{
	if (bMatchMusicEnabled || !ShouldPlayAudio() || !BackgroundMusic)
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	EnsureAudioComponent();

	BackgroundAudioComponent->Stop();
	BackgroundAudioComponent->SetSound(BackgroundMusic);
	BackgroundAudioComponent->SetPitchMultiplier(PitchMultiplier);
	BackgroundAudioComponent->Play();

	bMatchMusicEnabled = true;

	if (ChaseSuppressCount > 0)
	{
		BackgroundAudioComponent->SetVolumeMultiplier(0.f);
		bIsAudible = false;
		return;
	}

	if (FadeInDuration > 0.f)
	{
		BackgroundAudioComponent->SetVolumeMultiplier(0.f);
		BackgroundAudioComponent->FadeIn(FadeInDuration, VolumeMultiplier);
	}
	else
	{
		BackgroundAudioComponent->SetVolumeMultiplier(VolumeMultiplier);
	}

	bIsAudible = true;
}

void USPBackgroundMusicComponent::StopBackgroundMusic()
{
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
	if (!bMatchMusicEnabled || !bIsAudible || !BackgroundAudioComponent || !BackgroundAudioComponent->IsPlaying())
	{
		return;
	}

	if (FadeOutDuration > 0.f)
	{
		BackgroundAudioComponent->FadeOut(FadeOutDuration, 0.f);
	}
	else
	{
		BackgroundAudioComponent->SetVolumeMultiplier(0.f);
	}

	bIsAudible = false;
}

void USPBackgroundMusicComponent::RestoreBackgroundMusic()
{
	if (!bMatchMusicEnabled || bIsAudible || !ShouldPlayAudio() || !BackgroundMusic)
	{
		return;
	}

	EnsureAudioComponent();

	if (!BackgroundAudioComponent->IsPlaying())
	{
		BackgroundAudioComponent->SetSound(BackgroundMusic);
		BackgroundAudioComponent->SetPitchMultiplier(PitchMultiplier);
		BackgroundAudioComponent->Play();
	}

	if (FadeInDuration > 0.f)
	{
		BackgroundAudioComponent->SetVolumeMultiplier(0.f);
		BackgroundAudioComponent->FadeIn(FadeInDuration, VolumeMultiplier);
	}
	else
	{
		BackgroundAudioComponent->SetVolumeMultiplier(VolumeMultiplier);
	}

	bIsAudible = true;
}

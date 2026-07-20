#include "Components/SPDownedCrawlSoundComponent.h"

#include "Characters/Survivor/SurvivorCharacter.h"
#include "Components/AudioComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"

USPDownedCrawlSoundComponent::USPDownedCrawlSoundComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void USPDownedCrawlSoundComponent::BeginPlay()
{
	Super::BeginPlay();
}

void USPDownedCrawlSoundComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopCrawlSound();
	Super::EndPlay(EndPlayReason);
}

void USPDownedCrawlSoundComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!ShouldPlayAudio())
	{
		return;
	}

	if (IsCrawling())
	{
		StartCrawlSound();
	}
	else
	{
		StopCrawlSound();
	}
}

bool USPDownedCrawlSoundComponent::IsCrawling() const
{
	const ASurvivorCharacter* Survivor = GetSurvivor();
	if (!Survivor || Survivor->GetSurvivorState() != ESurvivorState::Downed)
	{
		return false;
	}

	return Survivor->GetVelocity().Size2D() >= MoveSpeedThreshold;
}

void USPDownedCrawlSoundComponent::StartCrawlSound()
{
	if (bIsPlayingCrawlSound || !CrawlLoopSound)
	{
		return;
	}

	AActor* Owner = GetOwner();
	USceneComponent* AttachComponent = GetAttachComponent();
	if (!Owner || !AttachComponent)
	{
		return;
	}

	if (!CrawlAudioComponent)
	{
		CrawlAudioComponent = NewObject<UAudioComponent>(Owner, TEXT("DownedCrawlAudio"));
		CrawlAudioComponent->bAutoActivate = false;
		CrawlAudioComponent->bAutoDestroy = false;
		CrawlAudioComponent->SetupAttachment(AttachComponent);
		CrawlAudioComponent->RegisterComponent();
	}

	CrawlAudioComponent->SetSound(CrawlLoopSound);
	CrawlAudioComponent->SetVolumeMultiplier(VolumeMultiplier);
	CrawlAudioComponent->SetPitchMultiplier(PitchMultiplier);
	CrawlAudioComponent->Play();
	bIsPlayingCrawlSound = true;
}

void USPDownedCrawlSoundComponent::StopCrawlSound()
{
	if (!bIsPlayingCrawlSound)
	{
		return;
	}

	if (CrawlAudioComponent && CrawlAudioComponent->IsPlaying())
	{
		CrawlAudioComponent->Stop();
	}

	bIsPlayingCrawlSound = false;
}

ASurvivorCharacter* USPDownedCrawlSoundComponent::GetSurvivor() const
{
	return Cast<ASurvivorCharacter>(GetOwner());
}

USceneComponent* USPDownedCrawlSoundComponent::GetAttachComponent() const
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

bool USPDownedCrawlSoundComponent::ShouldPlayAudio() const
{
	const UWorld* World = GetWorld();
	return World && World->GetNetMode() != NM_DedicatedServer;
}

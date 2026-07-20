#include "Components/SPKillerChaseMusicComponent.h"

#include "Characters/Killer/KillerCharacter.h"
#include "Characters/Survivor/SurvivorCharacter.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SPBackgroundMusicComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Systems/Data/KillerData.h"
#include "Systems/MatchGameState.h"

USPKillerChaseMusicComponent::USPKillerChaseMusicComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USPKillerChaseMusicComponent::OnRegister()
{
	Super::OnRegister();

	if (CanManageDetectionSphere())
	{
		SetupDetectionSphere();
	}
}

void USPKillerChaseMusicComponent::BeginPlay()
{
	Super::BeginPlay();

	if (CanManageDetectionSphere())
	{
		SetupDetectionSphere();
	}

	UpdateChaseMusicState();
}

void USPKillerChaseMusicComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopChaseMusic();

	if (DetectionSphere && GetWorld() && GetWorld()->IsGameWorld())
	{
		DetectionSphere->DestroyComponent();
		DetectionSphere = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

bool USPKillerChaseMusicComponent::CanManageDetectionSphere() const
{
	if (IsTemplate())
	{
		return false;
	}

	const AActor* Owner = GetOwner();
	if (!Owner || Owner->IsTemplate())
	{
		return false;
	}

	return Owner->GetWorld() != nullptr;
}

void USPKillerChaseMusicComponent::SetupDetectionSphere()
{
	if (!CanManageDetectionSphere())
	{
		return;
	}

	AKillerCharacter* Killer = GetKiller();
	if (!Killer)
	{
		return;
	}

	if (!DetectionSphere)
	{
		DetectionSphere = NewObject<USphereComponent>(Killer, TEXT("ChaseDetectionSphere"));
		DetectionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		DetectionSphere->SetCollisionObjectType(ECC_WorldDynamic);
		DetectionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
		DetectionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
		DetectionSphere->SetGenerateOverlapEvents(true);

		if (UCapsuleComponent* Capsule = Killer->GetCapsuleComponent())
		{
			DetectionSphere->SetupAttachment(Capsule);
		}
		else
		{
			DetectionSphere->SetupAttachment(Killer->GetRootComponent());
		}

		DetectionSphere->OnComponentBeginOverlap.AddDynamic(this, &USPKillerChaseMusicComponent::OnDetectionBeginOverlap);
		DetectionSphere->OnComponentEndOverlap.AddDynamic(this, &USPKillerChaseMusicComponent::OnDetectionEndOverlap);
		DetectionSphere->RegisterComponent();
	}

	ApplyDetectionRadiusToSphere();
	UpdateDetectionSphereVisuals();
}

void USPKillerChaseMusicComponent::UpdateDetectionSphereVisuals()
{
	if (!DetectionSphere)
	{
		return;
	}

	DetectionSphere->SetHiddenInGame(!bShowDetectionSphereInGame);
	
#if WITH_EDITORONLY_DATA
	DetectionSphere->bVisualizeComponent = bShowDetectionSphereInEditor;
#endif
	
	DetectionSphere->ShapeColor = FColor(255, 165, 0);
}

float USPKillerChaseMusicComponent::ResolveDefaultDetectionRadius() const
{
	if (DetectionRadius > 0.f)
	{
		return DetectionRadius;
	}

	if (const AKillerCharacter* Killer = GetKiller())
	{
		if (const UKillerData* Data = Killer->GetKillerData())
		{
			return Data->DetectionRadius;
		}
	}

	return 1200.f;
}

float USPKillerChaseMusicComponent::GetDetectionRadius() const
{
	if (DetectionSphere)
	{
		return DetectionSphere->GetUnscaledSphereRadius();
	}

	return ResolveDefaultDetectionRadius();
}

void USPKillerChaseMusicComponent::ApplyDetectionRadiusToSphere()
{
	if (!DetectionSphere || !DetectionSphere->IsRegistered())
	{
		return;
	}

	DetectionSphere->SetSphereRadius(ResolveDefaultDetectionRadius());
}

#if WITH_EDITOR
void USPKillerChaseMusicComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (!CanManageDetectionSphere())
	{
		return;
	}

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
	if (PropertyName == GET_MEMBER_NAME_CHECKED(USPKillerChaseMusicComponent, DetectionRadius))
	{
		ApplyDetectionRadiusToSphere();
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(USPKillerChaseMusicComponent, bShowDetectionSphereInEditor)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(USPKillerChaseMusicComponent, bShowDetectionSphereInGame))
	{
		UpdateDetectionSphereVisuals();
	}
}
#endif

void USPKillerChaseMusicComponent::OnDetectionBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	(void)OverlappedComponent;
	(void)OtherComp;
	(void)OtherBodyIndex;
	(void)bFromSweep;
	(void)SweepResult;

	ASurvivorCharacter* Survivor = Cast<ASurvivorCharacter>(OtherActor);
	if (!IsValidChaseTarget(Survivor))
	{
		return;
	}

	OverlappingSurvivors.Add(Survivor);
	UpdateChaseMusicState();
}

void USPKillerChaseMusicComponent::OnDetectionEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex)
{
	(void)OverlappedComponent;
	(void)OtherComp;
	(void)OtherBodyIndex;

	ASurvivorCharacter* Survivor = Cast<ASurvivorCharacter>(OtherActor);
	if (!Survivor)
	{
		return;
	}

	OverlappingSurvivors.Remove(Survivor);
	UpdateChaseMusicState();
}

bool USPKillerChaseMusicComponent::IsValidChaseTarget(const ASurvivorCharacter* Survivor) const
{
	if (!Survivor)
	{
		return false;
	}

	switch (Survivor->GetSurvivorState())
	{
	case ESurvivorState::Dead:
	case ESurvivorState::Escaped:
	case ESurvivorState::Caged:
		return false;
	default:
		return true;
	}
}

bool USPKillerChaseMusicComponent::HasValidOverlappingSurvivor() const
{
	for (auto It = OverlappingSurvivors.CreateConstIterator(); It; ++It)
	{
		if (IsValidChaseTarget(It->Get()))
		{
			return true;
		}
	}

	return false;
}

bool USPKillerChaseMusicComponent::ShouldLocalViewerHearChaseMusic() const
{
	if (!ShouldPlayAudio() || !HasValidOverlappingSurvivor())
	{
		return false;
	}

	const AKillerCharacter* Killer = GetKiller();
	if (bPlayForKillerOwner && Killer && Killer->IsLocallyControlled())
	{
		return true;
	}

	if (bPlayForLocalSurvivor)
	{
		const UWorld* World = GetWorld();
		const APlayerController* LocalController = World ? World->GetFirstPlayerController() : nullptr;
		const ASurvivorCharacter* LocalSurvivor = LocalController
			? Cast<ASurvivorCharacter>(LocalController->GetPawn())
			: nullptr;

		if (LocalSurvivor && OverlappingSurvivors.Contains(LocalSurvivor) && IsValidChaseTarget(LocalSurvivor))
		{
			return true;
		}
	}

	return false;
}

bool USPKillerChaseMusicComponent::ShouldPlayAudio() const
{
	const UWorld* World = GetWorld();
	return World && World->GetNetMode() != NM_DedicatedServer;
}

void USPKillerChaseMusicComponent::UpdateChaseMusicState()
{
	for (auto It = OverlappingSurvivors.CreateIterator(); It; ++It)
	{
		if (!It->IsValid() || !IsValidChaseTarget(It->Get()))
		{
			It.RemoveCurrent();
		}
	}

	if (ShouldLocalViewerHearChaseMusic())
	{
		StartChaseMusic();
	}
	else
	{
		StopChaseMusic();
	}
}

void USPKillerChaseMusicComponent::StartChaseMusic()
{
	if (bIsPlayingChaseMusic || !ChaseMusic)
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	if (!ChaseAudioComponent)
	{
		ChaseAudioComponent = NewObject<UAudioComponent>(Owner, TEXT("ChaseMusicAudio"));
		ChaseAudioComponent->bAutoActivate = false;
		ChaseAudioComponent->bAutoDestroy = false;
		ChaseAudioComponent->bAllowSpatialization = false;
		ChaseAudioComponent->RegisterComponent();
	}

	PlayEnterSound();

	ChaseAudioComponent->Stop();
	ChaseAudioComponent->SetSound(ChaseMusic);
	ChaseAudioComponent->SetPitchMultiplier(PitchMultiplier);

	if (FadeInDuration > 0.f)
	{
		ChaseAudioComponent->SetVolumeMultiplier(0.f);
		ChaseAudioComponent->Play();
		ChaseAudioComponent->FadeIn(FadeInDuration, VolumeMultiplier);
	}
	else
	{
		ChaseAudioComponent->SetVolumeMultiplier(VolumeMultiplier);
		ChaseAudioComponent->Play();
	}

	NotifyBackgroundMusicChaseState(true);
	bIsPlayingChaseMusic = true;
}

void USPKillerChaseMusicComponent::StopChaseMusic()
{
	if (!bIsPlayingChaseMusic)
	{
		return;
	}

	NotifyBackgroundMusicChaseState(false);

	PlayExitSound();

	if (ChaseAudioComponent && ChaseAudioComponent->IsPlaying())
	{
		if (FadeOutDuration > 0.f)
		{
			ChaseAudioComponent->FadeOut(FadeOutDuration, 0.f);
		}
		else
		{
			ChaseAudioComponent->Stop();
		}
	}

	bIsPlayingChaseMusic = false;
}

void USPKillerChaseMusicComponent::PlayEnterSound() const
{
	if (!ChaseEnterSound)
	{
		return;
	}

	UGameplayStatics::PlaySound2D(GetWorld(), ChaseEnterSound, VolumeMultiplier, PitchMultiplier);
}

void USPKillerChaseMusicComponent::PlayExitSound() const
{
	if (!ChaseExitSound)
	{
		return;
	}

	UGameplayStatics::PlaySound2D(GetWorld(), ChaseExitSound, VolumeMultiplier, PitchMultiplier);
}

AKillerCharacter* USPKillerChaseMusicComponent::GetKiller() const
{
	return Cast<AKillerCharacter>(GetOwner());
}

void USPKillerChaseMusicComponent::NotifyBackgroundMusicChaseState(const bool bChaseActive)
{
	if (!ShouldPlayAudio())
	{
		return;
	}

	UWorld* World = GetWorld();
	AMatchGameState* MatchGameState = World ? World->GetGameState<AMatchGameState>() : nullptr;
	USPBackgroundMusicComponent* BackgroundMusicComponent = MatchGameState
		? MatchGameState->GetBackgroundMusicComponent()
		: nullptr;
	if (!BackgroundMusicComponent)
	{
		return;
	}

	if (bChaseActive)
	{
		BackgroundMusicComponent->NotifyChaseMusicStarted();
	}
	else
	{
		BackgroundMusicComponent->NotifyChaseMusicStopped();
	}
}

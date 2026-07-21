#include "Gameplay/Escape/SPEscapeGate.h"

#include "Characters/Survivor/SurvivorCharacter.h"
#include "Components/SPInteractionComponent.h"
#include "Components/SPEscapeLeverComponent.h"
#include "Components/SPEscapeLeverSoundComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "Player/LDPlayerState.h"
#include "Systems/MatchGameState.h"
#include "TimerManager.h"
#include "Type/SPGameplayTag.h"

ASPEscapeGate::ASPEscapeGate()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	
	DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>("DoorMesh");
	SetRootComponent(DoorMesh);

	LeftDoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftDoorMesh"));
	LeftDoorMesh->SetupAttachment(DoorMesh);
	LeftDoorMesh->SetMobility(EComponentMobility::Movable);
	LeftDoorMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));

	RightDoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightDoorMesh"));
	RightDoorMesh->SetupAttachment(DoorMesh);
	RightDoorMesh->SetMobility(EComponentMobility::Movable);
	RightDoorMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));

	LeverPanelMesh = CreateDefaultSubobject<UStaticMeshComponent>("LeverPanelMesh");
	LeverPanelMesh->SetupAttachment(DoorMesh);
	LeverPanelMesh->SetCollisionProfileName(TEXT("Interactable"));
	LeverPanelMesh->SetCustomDepthStencilValue(250);
	LeverPanelMesh->SetRenderCustomDepth(false);
	
	LeverPivot = CreateDefaultSubobject<USceneComponent>("LeverPivot");
	LeverPivot->SetupAttachment(LeverPanelMesh);
	
	SwitchMesh = CreateDefaultSubobject<UStaticMeshComponent>("Mesh");
	SwitchMesh->SetCollisionProfileName(TEXT("Interactable"));
	SwitchMesh->SetCustomDepthStencilValue(250);
	SwitchMesh->SetRenderCustomDepth(false);
	SwitchMesh->SetupAttachment(LeverPivot);
	
	ExitTrigger = CreateDefaultSubobject<UBoxComponent>("ExitTrigger");
	ExitTrigger->SetupAttachment(DoorMesh);
	ExitTrigger->SetCollisionProfileName(TEXT("Trigger"));

	InteractAnchor = CreateDefaultSubobject<UArrowComponent>("InteractAnchor");
	InteractAnchor->SetupAttachment(DoorMesh);

	LeverSoundComponent = CreateDefaultSubobject<USPEscapeLeverSoundComponent>(TEXT("LeverSoundComponent"));
}

void ASPEscapeGate::BeginPlay()
{
	Super::BeginPlay();

	ExitTrigger->OnComponentBeginOverlap.AddDynamic(this, &ASPEscapeGate::OnExitTriggerBeginOverlap);

	if (LeverPivot)
	{
		InitialLeverPivotRotation = LeverPivot->GetRelativeRotation();
	}
	CurrentLeverOffset = LeverInitialRotation;

	CacheDoorClosedLocations();
	DoorOpenAlpha = bIsActivated ? 1.0f : 0.0f;
	ApplyDoorMovement(DoorOpenAlpha);

	RefreshInteractionCollision();
	BindAvailabilityDelegate();
}

void ASPEscapeGate::BindAvailabilityDelegate()
{
	AMatchGameState* MatchGameState = GetWorld() ? GetWorld()->GetGameState<AMatchGameState>() : nullptr;
	if (!MatchGameState)
	{
		GetWorldTimerManager().SetTimerForNextTick(this, &ASPEscapeGate::BindAvailabilityDelegate);
		return;
	}

	MatchGameState->OnEscapeGateAvailabilityChanged.AddUniqueDynamic(this, &ASPEscapeGate::OnEscapeAvailabilityChanged);
	OnEscapeAvailabilityChanged(MatchGameState->CanActivateEscapeGates());
}

void ASPEscapeGate::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASPEscapeGate, OpenProgress);
	DOREPLIFETIME(ASPEscapeGate, bIsActivated);
}

void ASPEscapeGate::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateLeverRotation(DeltaSeconds);
	UpdateDoorMovement(DeltaSeconds);

	if (!HasAuthority() || bIsActivated || !CurrentOpener.IsValid())
	{
		return;
	}

	if (!CanBeOpened())
	{
		CancelCurrentOpening();
		return;
	}

	OpenProgress = FMath::Min(OpenProgress + DeltaSeconds, OpenDuration);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 0.f, FColor::Green, FString::Printf(TEXT("탈출구 진행률: %.2f / %.2f"), OpenProgress, OpenDuration));
	}

	if (OpenProgress >= OpenDuration)
	{
		bIsActivated = true;
		NotifyLeverChannelStopped(true);
		OnRep_IsActivated();

		if (ASurvivorCharacter* Opener = CurrentOpener.Get())
		{
			if (USPEscapeLeverComponent* LeverComponent = Opener->GetEscapeLeverComponent())
			{
				LeverComponent->EndLeverChannel(true);
			}
			Opener->EndEscapeChanneling();
		}
		CurrentOpener = nullptr;
	}
}

void ASPEscapeGate::Interact_Implementation(AActor* Interactor)
{
	if (ASurvivorCharacter* Survivor = Cast<ASurvivorCharacter>(Interactor))
	{
		Survivor->BeginEscapeOpen(this);
	}
}

void ASPEscapeGate::SetHighlight_Implementation(bool bEnabled)
{
	SwitchMesh->SetRenderCustomDepth(bEnabled);
	LeverPanelMesh->SetRenderCustomDepth(bEnabled);
}

FGameplayTag ASPEscapeGate::GetInteractableTag_Implementation() const
{
	return SPGameplayTags::Interactable::Escape::Gate;
}

bool ASPEscapeGate::IsInteractable_Implementation() const
{
	return CanBeOpened();
}

USceneComponent* ASPEscapeGate::GetInteractFocusComponent_Implementation() const
{
	return LeverPanelMesh;
}

bool ASPEscapeGate::CanBeOpened() const
{
	if (bIsActivated)
	{
		return false;
	}

#if !UE_BUILD_SHIPPING
	if (bDebugBypassDelivery)
	{
		return true;
	}
#endif

	const AMatchGameState* MatchGameState = GetWorld() ? GetWorld()->GetGameState<AMatchGameState>() : nullptr;
	return MatchGameState && MatchGameState->CanActivateEscapeGates();
}

void ASPEscapeGate::SetOpener(ASurvivorCharacter* Opener)
{
	if (!HasAuthority() || bIsActivated)
	{
		return;
	}

	CurrentOpener = Opener;
	Multicast_NotifyLeverChannelStarted();
}

void ASPEscapeGate::ClearOpener(ASurvivorCharacter* Opener)
{
	if (CurrentOpener != Opener)
	{
		return;
	}

	CurrentOpener = nullptr;

	if (HasAuthority() && !bIsActivated)
	{
		OpenProgress = SnapProgressToCheckpoint(OpenProgress);
	}

	Multicast_NotifyLeverChannelStopped(false);
}

void ASPEscapeGate::NotifyLeverChannelStopped(const bool bCompleted)
{
	if (LeverSoundComponent)
	{
		LeverSoundComponent->NotifyChannelStopped(bCompleted);
	}
}

void ASPEscapeGate::Multicast_NotifyLeverChannelStarted_Implementation()
{
	if (LeverSoundComponent)
	{
		LeverSoundComponent->NotifyChannelStarted();
	}
}

void ASPEscapeGate::Multicast_NotifyLeverChannelStopped_Implementation(const bool bCompleted)
{
	NotifyLeverChannelStopped(bCompleted);
}

float ASPEscapeGate::SnapProgressToCheckpoint(float CurrentProgress) const
{
	if (OpenDuration <= 0.f)
	{
		return 0.f;
	}

	const float Ratio = CurrentProgress / OpenDuration;
	float SnappedRatio = 0.f;
	for (const float Checkpoint : ProgressCheckpoints)
	{
		if (Ratio + KINDA_SMALL_NUMBER >= Checkpoint && Checkpoint > SnappedRatio)
		{
			SnappedRatio = Checkpoint;
		}
	}

	return SnappedRatio * OpenDuration;
}

void ASPEscapeGate::OnRep_IsActivated()
{
	RefreshInteractionCollision();

	if (bIsActivated)
	{
		NotifyLeverChannelStopped(true);
		SwitchMesh->SetRenderCustomDepth(false);
		LeverPanelMesh->SetRenderCustomDepth(false);
	}
}

void ASPEscapeGate::OnEscapeAvailabilityChanged(bool bCanActivate)
{
	RefreshInteractionCollision();

	if (!bCanActivate && !CanBeOpened())
	{
		CancelCurrentOpening();
	}
}

void ASPEscapeGate::RefreshInteractionCollision()
{
	const FName CollisionProfile = CanBeOpened() ? FName(TEXT("Interactable")) : FName(TEXT("NoCollision"));

	if (SwitchMesh)
	{
		SwitchMesh->SetCollisionProfileName(CollisionProfile);
	}
	if (LeverPanelMesh)
	{
		LeverPanelMesh->SetCollisionProfileName(CollisionProfile);
	}
}

void ASPEscapeGate::CancelCurrentOpening()
{
	if (!HasAuthority())
	{
		return;
	}

	ASurvivorCharacter* Opener = CurrentOpener.Get();
	if (!Opener)
	{
		CurrentOpener = nullptr;
		return;
	}

	if (USPInteractionComponent* InteractionComponent = Opener->GetInteractionComponent())
	{
		InteractionComponent->CancelInteract();
	}

	if (CurrentOpener == Opener)
	{
		if (USPEscapeLeverComponent* LeverComponent = Opener->GetEscapeLeverComponent())
		{
			LeverComponent->CancelLeverChannel();
		}
		ClearOpener(Opener);
	}
}

void ASPEscapeGate::NotifyLeverPullStart()
{
	bLeverVisualPulled = true;
}

void ASPEscapeGate::NotifyLeverRelease()
{
	bLeverVisualPulled = false;
}

void ASPEscapeGate::UpdateLeverRotation(float DeltaSeconds)
{
	if (!LeverPivot)
	{
		return;
	}

	FRotator TargetOffset = LeverInitialRotation;
	if (bIsActivated)
	{
		TargetOffset = LeverCompletedRotation;
	}
	else if (bLeverVisualPulled)
	{
		TargetOffset = LeverPulledRotation;
	}

	CurrentLeverOffset = FMath::RInterpTo(CurrentLeverOffset, TargetOffset, DeltaSeconds, LeverRotateInterpSpeed);
	LeverPivot->SetRelativeRotation(InitialLeverPivotRotation.Quaternion() * CurrentLeverOffset.Quaternion());
}

void ASPEscapeGate::CacheDoorClosedLocations()
{
	if (bDoorClosedLocationsCached)
	{
		return;
	}

	if (LeftDoorMesh)
	{
		LeftDoorClosedLocation = LeftDoorMesh->GetRelativeLocation();
	}
	if (RightDoorMesh)
	{
		RightDoorClosedLocation = RightDoorMesh->GetRelativeLocation();
	}

	bDoorClosedLocationsCached = true;
}

void ASPEscapeGate::UpdateDoorMovement(float DeltaSeconds)
{
	CacheDoorClosedLocations();

	const float TargetOpenAlpha = bIsActivated ? 1.0f : 0.0f;
	if (DoorMoveDuration <= KINDA_SMALL_NUMBER)
	{
		DoorOpenAlpha = TargetOpenAlpha;
	}
	else
	{
		DoorOpenAlpha = FMath::FInterpConstantTo(
			DoorOpenAlpha,
			TargetOpenAlpha,
			DeltaSeconds,
			1.0f / DoorMoveDuration);
	}

	ApplyDoorMovement(DoorOpenAlpha);
}

void ASPEscapeGate::ApplyDoorMovement(float NormalizedOpenAlpha)
{
	const float ClampedAlpha = FMath::Clamp(NormalizedOpenAlpha, 0.0f, 1.0f);
	const float SmoothedAlpha = ClampedAlpha * ClampedAlpha * (3.0f - 2.0f * ClampedAlpha);

	if (LeftDoorMesh)
	{
		LeftDoorMesh->SetRelativeLocation(
			FMath::Lerp(LeftDoorClosedLocation, LeftDoorClosedLocation + LeftDoorOpenOffset, SmoothedAlpha));
	}
	if (RightDoorMesh)
	{
		RightDoorMesh->SetRelativeLocation(
			FMath::Lerp(RightDoorClosedLocation, RightDoorClosedLocation + RightDoorOpenOffset, SmoothedAlpha));
	}
}

FTransform ASPEscapeGate::GetInteractAnchorTransform() const
{
	return InteractAnchor ? InteractAnchor->GetComponentTransform() : GetActorTransform();
}

void ASPEscapeGate::OnExitTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority() || !bIsActivated)
	{
		return;
	}

	if (ASurvivorCharacter* Survivor = Cast<ASurvivorCharacter>(OtherActor))
	{
		if (Survivor->TryEscape(ESurvivorEscapeMethod::Gate) && GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Blue, FString::Printf(TEXT("%s 탈출!"), *Survivor->GetName()));
		}
	}
}

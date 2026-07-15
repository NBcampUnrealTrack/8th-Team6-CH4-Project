#include "Gameplay/Escape/SPEscapeGate.h"

#include "Characters/Survivor/SurvivorCharacter.h"
#include "Components/SPEscapeLeverComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "Systems/MatchGameState.h"
#include "TimerManager.h"
#include "Type/SPGameplayTag.h"

ASPEscapeGate::ASPEscapeGate()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	
	DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>("DoorMesh");
	SetRootComponent(DoorMesh);

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

	MatchGameState->OnEscapeGateAvailabilityChanged.AddDynamic(this, &ASPEscapeGate::OnEscapeAvailabilityChanged);
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

	if (!HasAuthority() || bIsActivated || !CurrentOpener.IsValid())
	{
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
	if (bIsActivated)
	{
		SwitchMesh->SetRenderCustomDepth(false);
		LeverPanelMesh->SetRenderCustomDepth(false);
	}
}

void ASPEscapeGate::OnEscapeAvailabilityChanged(bool bCanActivate)
{
	SwitchMesh->SetCollisionProfileName(TEXT("Interactable"));
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

FTransform ASPEscapeGate::GetInteractAnchorTransform() const
{
	return InteractAnchor ? InteractAnchor->GetComponentTransform() : GetActorTransform();
}

void ASPEscapeGate::OnExitTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority()) return;

	if (ASurvivorCharacter* Survivor = Cast<ASurvivorCharacter>(OtherActor))
	{
		if (bIsActivated)
		{
			Survivor->SetSurvivorState(ESurvivorState::Escaped);
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Blue, FString::Printf(TEXT("%s 탈출!"), *Survivor->GetName()));
			}
		}
	}
}

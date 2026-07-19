#include "Gameplay/Escape/SPHatch.h"

#include "Characters/Survivor/SurvivorCharacter.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Curves/CurveFloat.h"
#include "GameFramework/GameStateBase.h"
#include "Gameplay/Escape/SPHatchManagerSubsystem.h"
#include "Net/UnrealNetwork.h"
#include "Player/LDPlayerState.h"
#include "Systems/Data/SurvivorData.h"
#include "Type/SPGameplayTag.h"

ASPHatch::ASPHatch()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	TrayMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TrayMesh"));
	SetRootComponent(TrayMesh);
	TrayMesh->SetCollisionProfileName(TEXT("NoCollision"));
	TrayMesh->SetCustomDepthStencilValue(250);
	TrayMesh->SetRenderCustomDepth(false);

	DoorPivot = CreateDefaultSubobject<UArrowComponent>(TEXT("DoorPivot"));
	DoorPivot->SetupAttachment(TrayMesh);
	DoorPivot->SetRelativeRotation(FRotator::ZeroRotator);
	DoorPivot->SetArrowColor(FLinearColor::Green);
	DoorPivot->ArrowSize = 1.0f;
	DoorPivot->bIsScreenSizeScaled = true;
	DoorPivot->SetHiddenInGame(true);

	DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));
	DoorMesh->SetupAttachment(DoorPivot);
	DoorMesh->SetCollisionProfileName(TEXT("NoCollision"));
	DoorMesh->SetCustomDepthStencilValue(250);
	DoorMesh->SetRenderCustomDepth(false);

	EscapeTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("EscapeTrigger"));
	EscapeTrigger->SetupAttachment(TrayMesh);
	EscapeTrigger->SetCollisionProfileName(TEXT("NoCollision"));
	EscapeTrigger->SetGenerateOverlapEvents(false);
}

void ASPHatch::EnsureDoorComponentHierarchy()
{
	if (!TrayMesh || !DoorPivot || !DoorMesh)
	{
		return;
	}

	if (DoorPivot->GetAttachParent() != TrayMesh)
	{
		DoorPivot->AttachToComponent(TrayMesh, FAttachmentTransformRules::KeepRelativeTransform);
	}

	if (DoorMesh->GetAttachParent() != DoorPivot)
	{
		DoorMesh->AttachToComponent(DoorPivot, FAttachmentTransformRules::KeepWorldTransform);
	}
}

void ASPHatch::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	EnsureDoorComponentHierarchy();
}

void ASPHatch::BeginPlay()
{
	Super::BeginPlay();

	ClosedDoorRotation = DoorPivot ? DoorPivot->GetRelativeRotation() : FRotator::ZeroRotator;
	if (EscapeTrigger)
	{
		EscapeTrigger->OnComponentBeginOverlap.AddDynamic(this, &ASPHatch::OnEscapeTriggerBeginOverlap);
	}

	ApplyHatchState();

	if (HasAuthority())
	{
		if (USPHatchManagerSubsystem* HatchManager = GetWorld()->GetSubsystem<USPHatchManagerSubsystem>())
		{
			HatchManager->RegisterHatch(this);
		}
	}
}

void ASPHatch::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority() && GetWorld())
	{
		if (USPHatchManagerSubsystem* HatchManager = GetWorld()->GetSubsystem<USPHatchManagerSubsystem>())
		{
			HatchManager->UnregisterHatch(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void ASPHatch::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASPHatch, OpenProgress);
	DOREPLIFETIME(ASPHatch, bIsActive);
	DOREPLIFETIME(ASPHatch, bIsOpened);
	DOREPLIFETIME(ASPHatch, DoorOpenStartServerTime);
}

void ASPHatch::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bIsOpened)
	{
		UpdateDoorOpeningPresentation();
	}

	if (!HasAuthority() || !bIsActive || bIsOpened || !CurrentOpener.IsValid() || ActiveOpenDuration <= 0.0f)
	{
		return;
	}

	OpenProgress = FMath::Min(OpenProgress + DeltaSeconds, ActiveOpenDuration);

	if (OpenProgress >= ActiveOpenDuration)
	{
		CompleteOpening();
	}
}

void ASPHatch::Interact_Implementation(AActor* Interactor)
{
	if (ASurvivorCharacter* Survivor = Cast<ASurvivorCharacter>(Interactor))
	{
		Survivor->BeginHatchOpen(this);
	}
}

void ASPHatch::SetHighlight_Implementation(bool bEnabled)
{
	SetHatchRenderCustomDepth(bEnabled && CanBeOpened());
}

FGameplayTag ASPHatch::GetInteractableTag_Implementation() const
{
	return SPGameplayTags::Interactable::Escape::Hatch;
}

bool ASPHatch::IsInteractable_Implementation() const
{
	return CanBeOpened();
}

bool ASPHatch::CanBeSelected() const
{
	return HasAuthority() && !bIsActive && !bIsOpened && HasRequiredVisuals();
}

bool ASPHatch::HasRequiredVisuals() const
{
	return TrayMesh && TrayMesh->GetStaticMesh() && DoorMesh && DoorMesh->GetStaticMesh();
}

bool ASPHatch::CanBeOpened() const
{
	return bIsActive && !bIsOpened && !CurrentOpener.IsValid();
}

bool ASPHatch::TryBeginOpening(ASurvivorCharacter* Opener)
{
	if (!HasAuthority() || !IsValid(Opener) || !Opener->CanInteract() || !CanBeOpened())
	{
		return false;
	}

	const USurvivorData* SurvivorData = Opener->GetSurvivorData();
	if (!IsValid(SurvivorData) || SurvivorData->HatchEscapeDuration <= 0.0f)
	{
		return false;
	}

	CurrentOpener = Opener;
	ActiveOpenDuration = SurvivorData->HatchEscapeDuration;
	OpenProgress = 0.0f;
	return true;
}

void ASPHatch::CancelOpening(ASurvivorCharacter* Opener)
{
	if (!HasAuthority() || CurrentOpener.Get() != Opener)
	{
		return;
	}

	CurrentOpener.Reset();
	ActiveOpenDuration = 0.0f;
	OpenProgress = 0.0f;
	ForceNetUpdate();
}

void ASPHatch::ActivateHatch()
{
	if (!HasAuthority() || bIsActive)
	{
		return;
	}

	bIsActive = true;
	bIsOpened = false;
	DoorOpenStartServerTime = -1.0f;
	OpenProgress = 0.0f;
	ActiveOpenDuration = 0.0f;
	CurrentOpener.Reset();

	ApplyHatchState();
	ForceNetUpdate();
}

void ASPHatch::DeactivateHatch()
{
	if (!HasAuthority())
	{
		return;
	}

	bIsActive = false;
	bIsOpened = false;
	DoorOpenStartServerTime = -1.0f;
	OpenProgress = 0.0f;
	ActiveOpenDuration = 0.0f;
	CurrentOpener.Reset();
	ApplyHatchState();
	ForceNetUpdate();
}

void ASPHatch::CompleteOpening()
{
	if (!HasAuthority() || bIsOpened)
	{
		return;
	}

	ASurvivorCharacter* Opener = CurrentOpener.Get();
	CurrentOpener.Reset();
	DoorOpenStartServerTime = GetSynchronizedWorldTimeSeconds();
	bIsOpened = true;
	OpenProgress = ActiveOpenDuration;

	ApplyHatchState();
	ForceNetUpdate();

	if (Opener)
	{
		Opener->CompleteHatchOpen();
	}
	ActiveOpenDuration = 0.0f;
}

void ASPHatch::OnRep_IsActive()
{
	ApplyHatchState();
}

void ASPHatch::OnRep_IsOpened()
{
	ApplyHatchState();
}

void ASPHatch::OnRep_DoorOpenStartServerTime()
{
	ApplyHatchState();
}

void ASPHatch::OnEscapeTriggerBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (!HasAuthority() || !bIsActive || !bIsOpened)
	{
		return;
	}

	ASurvivorCharacter* Survivor = Cast<ASurvivorCharacter>(OtherActor);
	if (!Survivor || !Survivor->TryEscape(ESurvivorEscapeMethod::Hatch))
	{
		return;
	}
}

void ASPHatch::SetDoorRotation(const FRotator& NewRotation)
{
	DoorRotation = NewRotation;
	if (bIsOpened)
	{
		UpdateDoorOpeningPresentation();
	}
	else
	{
		ApplyDoorRotation(0.0f);
	}
}

void ASPHatch::UpdateDoorOpeningPresentation()
{
	const float RotationAlpha = GetDoorRotationAlpha();
	ApplyDoorRotation(RotationAlpha);
}

void ASPHatch::ApplyDoorRotation(float NormalizedTime)
{
	if (!DoorPivot)
	{
		return;
	}

	float PresentationAlpha = FMath::Clamp(NormalizedTime, 0.0f, 1.0f);
	if (DoorRotationCurve)
	{
		PresentationAlpha = FMath::Clamp(DoorRotationCurve->GetFloatValue(PresentationAlpha), 0.0f, 1.0f);
	}

	const FQuat InterpolatedRotation = FQuat::Slerp(
		ClosedDoorRotation.Quaternion(),
		DoorRotation.Quaternion(),
		PresentationAlpha);
	DoorPivot->SetRelativeRotation(InterpolatedRotation);
}

float ASPHatch::GetDoorRotationAlpha() const
{
	if (!bIsOpened)
	{
		return 0.0f;
	}

	if (DoorRotationDuration <= 0.0f)
	{
		return 1.0f;
	}

	if (DoorOpenStartServerTime < 0.0f)
	{
		return 0.0f;
	}

	const float ElapsedTime = GetSynchronizedWorldTimeSeconds() - DoorOpenStartServerTime;
	return FMath::Clamp(ElapsedTime / DoorRotationDuration, 0.0f, 1.0f);
}

float ASPHatch::GetSynchronizedWorldTimeSeconds() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.0f;
	}

	if (const AGameStateBase* GameState = World->GetGameState())
	{
		return GameState->GetServerWorldTimeSeconds();
	}

	return World->GetTimeSeconds();
}

void ASPHatch::SetHatchVisibility(bool bVisible)
{
	if (TrayMesh)
	{
		TrayMesh->SetVisibility(bVisible);
	}

	if (DoorMesh)
	{
		DoorMesh->SetVisibility(bVisible);
	}
}

void ASPHatch::SetHatchRenderCustomDepth(bool bEnabled)
{
	if (TrayMesh)
	{
		TrayMesh->SetRenderCustomDepth(bEnabled);
	}

	if (DoorMesh)
	{
		DoorMesh->SetRenderCustomDepth(bEnabled);
	}
}

void ASPHatch::SetInteractionCollisionEnabled(bool bEnabled)
{
	const FName CollisionProfile = bEnabled ? TEXT("Interactable") : TEXT("NoCollision");

	if (TrayMesh)
	{
		TrayMesh->SetCollisionProfileName(CollisionProfile);
	}

	if (DoorMesh)
	{
		DoorMesh->SetCollisionProfileName(CollisionProfile);
	}
}

void ASPHatch::SetEscapeTriggerEnabled(bool bEnabled)
{
	if (!EscapeTrigger)
	{
		return;
	}

	EscapeTrigger->SetGenerateOverlapEvents(bEnabled);
	EscapeTrigger->SetCollisionProfileName(bEnabled ? TEXT("Trigger") : TEXT("NoCollision"));
}

void ASPHatch::ApplyHatchState()
{
	const float RotationAlpha = GetDoorRotationAlpha();

	SetHatchVisibility(bIsActive);
	SetActorHiddenInGame(false);
	SetInteractionCollisionEnabled(bIsActive && !bIsOpened);
	SetEscapeTriggerEnabled(bIsActive && bIsOpened);
	ApplyDoorRotation(RotationAlpha);

	if (!bIsActive || bIsOpened)
	{
		SetHatchRenderCustomDepth(false);
	}
}

#include "Gameplay/Escape/SPHatch.h"

#include "Characters/Survivor/SurvivorCharacter.h"
#include "Components/ArrowComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "Systems/MatchGameState.h"
#include "TimerManager.h"
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

#if WITH_EDITOR
void ASPHatch::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(ASPHatch, DoorRotation))
	{
		ApplyDoorRotation();
	}
}
#endif

void ASPHatch::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	EnsureDoorComponentHierarchy();
}

void ASPHatch::BeginPlay()
{
	Super::BeginPlay();

	if (DoorPivot)
	{
		if (DoorRotation.IsNearlyZero() && !DoorPivot->GetRelativeRotation().IsNearlyZero())
		{
			DoorRotation = DoorPivot->GetRelativeRotation();
		}
		else
		{
			ApplyDoorRotation();
		}
	}

	ApplyHatchVisibility();
	BindAvailabilityDelegate();
}

void ASPHatch::BindAvailabilityDelegate()
{
	AMatchGameState* MatchGameState = GetWorld() ? GetWorld()->GetGameState<AMatchGameState>() : nullptr;
	if (!MatchGameState)
	{
		GetWorldTimerManager().SetTimerForNextTick(this, &ASPHatch::BindAvailabilityDelegate);
		return;
	}

	MatchGameState->OnHatchAvailabilityChanged.AddDynamic(this, &ASPHatch::OnHatchAvailabilityChanged);

	if (HasAuthority() && MatchGameState->CanSpawnHatch())
	{
		OnHatchAvailabilityChanged(true);
	}
}

void ASPHatch::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASPHatch, EscapeProgress);
	DOREPLIFETIME(ASPHatch, bIsSpawned);
}

void ASPHatch::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority() || !bIsSpawned || !CurrentEscaper.IsValid())
	{
		return;
	}

	EscapeProgress = FMath::Min(EscapeProgress + DeltaSeconds, EscapeDuration);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 0.f, FColor::Cyan, FString::Printf(TEXT("개구멍 탈출 진행률: %.2f / %.2f"), EscapeProgress, EscapeDuration));
	}

	if (EscapeProgress >= EscapeDuration)
	{
		ASurvivorCharacter* Escaper = CurrentEscaper.Get();
		CurrentEscaper = nullptr;
		EscapeProgress = 0.f;

		if (Escaper)
		{
			Escaper->CompleteHatchEscape();
		}
	}
}

void ASPHatch::Interact_Implementation(AActor* Interactor)
{
	if (ASurvivorCharacter* Survivor = Cast<ASurvivorCharacter>(Interactor))
	{
		Survivor->BeginHatchEscape(this);
	}
}

void ASPHatch::SetHighlight_Implementation(bool bEnabled)
{
	SetHatchRenderCustomDepth(bEnabled);
}

FGameplayTag ASPHatch::GetInteractableTag_Implementation() const
{
	return SPGameplayTags::Interactable::Escape::Hatch;
}

bool ASPHatch::IsInteractable_Implementation() const
{
	if (!bIsSpawned)
	{
		return false;
	}

	const AMatchGameState* MatchGameState = GetWorld() ? GetWorld()->GetGameState<AMatchGameState>() : nullptr;
	return MatchGameState && MatchGameState->CanSpawnHatch();
}

void ASPHatch::SetEscaper(ASurvivorCharacter* Escaper)
{
	if (!HasAuthority() || !bIsSpawned)
	{
		return;
	}

	CurrentEscaper = Escaper;
}

void ASPHatch::ClearEscaper(ASurvivorCharacter* Escaper)
{
	if (CurrentEscaper == Escaper)
	{
		CurrentEscaper = nullptr;
		EscapeProgress = 0.f;
	}
}

void ASPHatch::OnRep_IsSpawned()
{
	ApplyHatchVisibility();
}

void ASPHatch::OnHatchAvailabilityChanged(bool bCanSpawn)
{
	// TODO(임시 진단): 해치 수신 로그 — 원인 확정 후 제거
	UE_LOG(LogTemp, Warning, TEXT("[Hatch] %s 수신: bCanSpawn=%s Auth=%s bIsSpawned=%s"),
		*GetName(), bCanSpawn ? TEXT("Y") : TEXT("N"), HasAuthority() ? TEXT("Y") : TEXT("N"), bIsSpawned ? TEXT("Y") : TEXT("N"));

	if (!HasAuthority() || !bCanSpawn || bIsSpawned)
	{
		return;
	}

	bIsSpawned = true;
	ApplyHatchVisibility();
}

void ASPHatch::SetDoorRotation(const FRotator& NewRotation)
{
	DoorRotation = NewRotation;
	ApplyDoorRotation();
}

void ASPHatch::ApplyDoorRotation()
{
	if (!DoorPivot || DoorPivot->GetRelativeRotation().Equals(DoorRotation, KINDA_SMALL_NUMBER))
	{
		return;
	}

	DoorPivot->SetRelativeRotation(DoorRotation);
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

void ASPHatch::SetHatchCollisionEnabled(bool bEnabled)
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

void ASPHatch::ApplyHatchVisibility()
{
	SetActorHiddenInGame(!bIsSpawned);
	SetHatchCollisionEnabled(bIsSpawned);
}

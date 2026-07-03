#include "Gameplay/Escape/SPEscapeGate.h"

#include "Characters/Survivor/SurvivorCharacter.h"
#include "Components/BoxComponent.h"
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

	SwitchMesh = CreateDefaultSubobject<UStaticMeshComponent>("Mesh");
	SetRootComponent(SwitchMesh);
	SwitchMesh->SetCollisionProfileName(TEXT("NoCollision"));
	SwitchMesh->SetCustomDepthStencilValue(250);
	SwitchMesh->SetRenderCustomDepth(false);
	DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>("DoorMesh");
	DoorMesh->SetupAttachment(SwitchMesh);

	ExitTrigger = CreateDefaultSubobject<UBoxComponent>("ExitTrigger");
	ExitTrigger->SetupAttachment(SwitchMesh);
	ExitTrigger->SetCollisionProfileName(TEXT("Trigger"));
}

void ASPEscapeGate::BeginPlay()
{
	Super::BeginPlay();

	ExitTrigger->OnComponentBeginOverlap.AddDynamic(this, &ASPEscapeGate::OnExitTriggerBeginOverlap);

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
}

FGameplayTag ASPEscapeGate::GetInteractableTag_Implementation() const
{
	return SPGameplayTags::Interactable::Escape::Gate;
}

bool ASPEscapeGate::IsInteractable_Implementation() const
{
	if (bIsActivated)
	{
		return false;
	}
	
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
	if (CurrentOpener == Opener)
	{
		CurrentOpener = nullptr;
	}
}

void ASPEscapeGate::OnRep_IsActivated()
{
	if (bIsActivated)
	{
		SwitchMesh->SetRenderCustomDepth(false);
	}
}

void ASPEscapeGate::OnEscapeAvailabilityChanged(bool bCanActivate)
{
	SwitchMesh->SetCollisionProfileName(TEXT("Interactable"));
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

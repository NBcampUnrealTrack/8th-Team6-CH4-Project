#include "Gameplay/Escape/SPEscapeGate.h"

#include "Characters/Survivor/SurvivorCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "Systems/MatchGameState.h"
#include "Type/SPGameplayTag.h"

ASPEscapeGate::ASPEscapeGate()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>("Mesh");
	SetRootComponent(Mesh);
	Mesh->SetCollisionProfileName(TEXT("Interactable"));
	Mesh->SetCustomDepthStencilValue(250);
	Mesh->SetRenderCustomDepth(false);

	ExitTrigger = CreateDefaultSubobject<UBoxComponent>("ExitTrigger");
	ExitTrigger->SetupAttachment(Mesh);
	ExitTrigger->SetCollisionProfileName(TEXT("Trigger"));
}

void ASPEscapeGate::BeginPlay()
{
	Super::BeginPlay();

	ExitTrigger->OnComponentBeginOverlap.AddDynamic(this, &ASPEscapeGate::OnExitTriggerBeginOverlap);

	if (AMatchGameState* MatchGameState = GetWorld() ? GetWorld()->GetGameState<AMatchGameState>() : nullptr)
	{
		MatchGameState->OnEscapeGateAvailabilityChanged.AddDynamic(this, &ASPEscapeGate::OnEscapeAvailabilityChanged);
	}
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
	Mesh->SetRenderCustomDepth(bEnabled);
}

FGameplayTag ASPEscapeGate::GetInteractableTag_Implementation() const
{
	return SPGameplayTags::Interactable::Escape::Gate;
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
		Mesh->SetRenderCustomDepth(false);

	}
}

void ASPEscapeGate::OnEscapeAvailabilityChanged(bool bCanActivate)
{
	// TODO: 게이트 Mesh 콜리전을 'Interactable' 프로파일로 전환한다.
	//  - 평소(비활성)에는 Interactable가 아니어서 상호작용 트레이스에 잡히지 않는다.
	//  - Interactable로 전환해 조준·개방이 가능해진다.
	//  - 탈출구 개방 로직과 함께 내일 이어서 구현.
}

void ASPEscapeGate::OnExitTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority()) return;

	if (ASurvivorCharacter* Survivor = Cast<ASurvivorCharacter>(OtherActor))
	{
		if (AreAllGatesActivated())
		{
			Survivor->SetSurvivorState(ESurvivorState::Escaped);
		}
	}
}

bool ASPEscapeGate::AreAllGatesActivated() const
{
	for (TActorIterator<ASPEscapeGate> It(GetWorld()); It; ++It)
	{
		if (!It->IsActivated())
		{
			return false;
		}
	}
	return true;
}

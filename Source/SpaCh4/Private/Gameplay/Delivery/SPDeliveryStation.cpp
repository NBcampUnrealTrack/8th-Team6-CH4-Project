#include "Gameplay/Delivery/SPDeliveryStation.h"

#include "Characters/Survivor/SurvivorCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Systems/MatchGameMode.h"
#include "Systems/MatchGameState.h"
#include "Type/SPGameplayTag.h"

ASPDeliveryStation::ASPDeliveryStation()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>("Mesh");
	SetRootComponent(Mesh);
	Mesh->SetCollisionProfileName(TEXT("Interactable"));
	Mesh->SetCustomDepthStencilValue(250);
	Mesh->SetRenderCustomDepth(false);
}

void ASPDeliveryStation::Interact_Implementation(AActor* Interactor)
{
	if (ASurvivorCharacter* Survivor = Cast<ASurvivorCharacter>(Interactor))
	{
		Survivor->BeginDelivery(this);
	}
}

void ASPDeliveryStation::SetHighlight_Implementation(bool bEnabled)
{
	Mesh->SetRenderCustomDepth(bEnabled);
}

FGameplayTag ASPDeliveryStation::GetInteractableTag_Implementation() const
{
	return SPGameplayTags::Interactable::Delivery;
}

void ASPDeliveryStation::SubmitValue(int32 Value) const
{
	if (!HasAuthority())
	{
		return;
	}
	
	AMatchGameMode* MatchGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AMatchGameMode>() : nullptr;
	if (!MatchGameMode)
	{
		return;
	}

	if (MatchGameMode->AddDeliveredValue(StationId, Value))
	{
		if (const AMatchGameState* MatchGameState = GetWorld()->GetGameState<AMatchGameState>())
		{
			UE_LOG(LogTemp, Warning, TEXT("납품소 %s에 %d 반납. 현재 점수 %d"), *StationId.ToString(), Value, MatchGameState->GetDeliveryStationValue(StationId));
		}
	}
}

bool ASPDeliveryStation::IsComplete() const
{
	if (const AMatchGameState* MatchGameState = GetWorld() ? GetWorld()->GetGameState<AMatchGameState>() : nullptr)
	{
		return MatchGameState->GetDeliveryStationValue(StationId) >= MatchGameState->GetDeliveryStationTargetValueByID(StationId);
	}
	return false;
}

bool ASPDeliveryStation::IsInteractable_Implementation() const
{
	// 목표치를 다 채운 납품소는 더 이상 조준·상호작용 대상이 아니다.
	return !IsComplete();
}

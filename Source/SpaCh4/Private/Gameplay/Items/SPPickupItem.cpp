#include "Gameplay/Items/SPPickupItem.h"

#include "Characters/Survivor/SurvivorCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "Type/SPGameplayTag.h"

ASPPickupItem::ASPPickupItem()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionProfileName(TEXT("Interactable"));
	Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	Mesh->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
	Mesh->SetCustomDepthStencilValue(250);
	Mesh->SetRenderCustomDepth(false);
}

void ASPPickupItem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASPPickupItem, bStored);
	DOREPLIFETIME(ASPPickupItem, ReservedBy);
}

void ASPPickupItem::Interact_Implementation(AActor* Interactor)
{
	ISPInteractable::Interact_Implementation(Interactor);

	if (ASurvivorCharacter* Survivor = Cast<ASurvivorCharacter>(Interactor))
	{
		Survivor->BeginPickup(this);
	}
}

void ASPPickupItem::SetHighlight_Implementation(const bool bEnabled)
{
	if (Mesh)
	{
		Mesh->SetRenderCustomDepth(bEnabled);
	}
}

FGameplayTag ASPPickupItem::GetInteractableTag_Implementation() const
{
	return SPGameplayTags::Interactable::Collectible;
}

bool ASPPickupItem::IsInteractable_Implementation() const
{
	return !bStored && !IsValid(ReservedBy);
}

bool ASPPickupItem::TryReserve(ASurvivorCharacter* Survivor)
{
	if (!HasAuthority() || !IsValid(Survivor) || bStored || IsValid(ReservedBy))
	{
		return false;
	}

	ReservedBy = Survivor;
	ForceNetUpdate();
	return true;
}

void ASPPickupItem::ReleaseReservation(ASurvivorCharacter* Survivor)
{
	if (!HasAuthority() || ReservedBy != Survivor)
	{
		return;
	}

	ReservedBy = nullptr;
	ForceNetUpdate();
}

void ASPPickupItem::SetStored(const bool bNewStored, const FVector& WorldLocation)
{
	if (!HasAuthority())
	{
		return;
	}

	if (!bNewStored)
	{
		SetActorLocation(WorldLocation);
	}

	bStored = bNewStored;
	ReservedBy = nullptr;
	ApplyStoredState();
	ForceNetUpdate();
}

void ASPPickupItem::OnRep_Stored()
{
	ApplyStoredState();
}

void ASPPickupItem::ApplyStoredState()
{
	SetActorHiddenInGame(bStored);
	SetActorEnableCollision(!bStored);

	if (bStored && Mesh)
	{
		Mesh->SetRenderCustomDepth(false);
	}
}

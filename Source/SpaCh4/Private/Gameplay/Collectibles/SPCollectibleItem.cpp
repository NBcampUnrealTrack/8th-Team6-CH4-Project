#include "Gameplay/Collectibles/SPCollectibleItem.h"

#include "Characters/Survivor/SurvivorCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Type/SPGameplayTag.h"

ASPCollectibleItem::ASPCollectibleItem()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>("Mesh");
	SetRootComponent(Mesh);
	Mesh->SetCollisionProfileName(TEXT("Interactable"));

	Mesh->SetCustomDepthStencilValue(250);
	Mesh->SetRenderCustomDepth(false);
}

void ASPCollectibleItem::Interact_Implementation(AActor* Interactor)
{
	ISPInteractable::Interact_Implementation(Interactor);

	if (ASurvivorCharacter* Survivor = Cast<ASurvivorCharacter>(Interactor))
	{
		Survivor->BeginPickup(this);
	}
}

void ASPCollectibleItem::SetPickupCollisionEnabled(bool bEnabled)
{
	SetActorEnableCollision(bEnabled);
}

void ASPCollectibleItem::Multicast_SetStored_Implementation(bool bStored, FVector DropLocation)
{
	if (!bStored)
	{
		SetActorLocation(DropLocation);
	}
	SetActorHiddenInGame(bStored);
	SetActorEnableCollision(!bStored);
}

void ASPCollectibleItem::SetHighlight_Implementation(bool bEnabled)
{
	Mesh->SetRenderCustomDepth(bEnabled);
}

FGameplayTag ASPCollectibleItem::GetInteractableTag_Implementation() const
{
	return SPGameplayTags::Interactable::Collectible;
}

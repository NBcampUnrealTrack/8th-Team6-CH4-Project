#include "Gameplay/Collectibles/SPCollectibleItem.h"

ASPCollectibleItem::ASPCollectibleItem()
{
}

void ASPCollectibleItem::SetPickupCollisionEnabled(const bool bEnabled)
{
	SetActorEnableCollision(bEnabled);
}

void ASPCollectibleItem::Multicast_SetStored_Implementation(const bool bNewStored, const FVector DropLocation)
{
	if (HasAuthority())
	{
		SetStored(bNewStored, DropLocation);
	}
}

#pragma once

#include "CoreMinimal.h"
#include "Gameplay/Items/SPPickupItem.h"
#include "Inventory/SPInventoryTypes.h"
#include "SPConsumableItem.generated.h"

UCLASS(Blueprintable)
class SPACH4_API ASPConsumableItem : public ASPPickupItem
{
	GENERATED_BODY()

public:
	EConsumableItemType GetConsumableType() const { return ConsumableType; }

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SP|Item|Consumable")
	EConsumableItemType ConsumableType = EConsumableItemType::Medkit;
};

UCLASS(Blueprintable)
class SPACH4_API ASPMedkitItem : public ASPConsumableItem
{
	GENERATED_BODY()

public:
	ASPMedkitItem();
};

UCLASS(Blueprintable)
class SPACH4_API ASPSpeedPotionItem : public ASPConsumableItem
{
	GENERATED_BODY()

public:
	ASPSpeedPotionItem();
};

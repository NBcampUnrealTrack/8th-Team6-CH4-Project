#pragma once

#include "CoreMinimal.h"
#include "Gameplay/Collectibles/SPCollectibleItem.h"
#include "SPInventoryTypes.generated.h"

class UTexture2D;

UENUM(BlueprintType)
enum class EInventorySlotContentType : uint8
{
	Empty,
	Collectible,
	Consumable
};

UENUM(BlueprintType)
enum class EConsumableItemType : uint8
{
	None,
	Medkit,
	SpeedBoost
};

UENUM(BlueprintType)
enum class EPerkType : uint8
{
	None,
	FieldMedic,
	LightWheels,
	SilentRoll,
	StageTwoRescue,
	ThreatSensor
};

USTRUCT(BlueprintType)
struct FInventorySlotEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	EInventorySlotContentType ContentType = EInventorySlotContentType::Empty;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	EConsumableItemType ConsumableType = EConsumableItemType::None;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	ECollectibleSize CollectibleSize = ECollectibleSize::Small;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	int32 CollectibleValue = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	TSoftObjectPtr<UTexture2D> CollectibleIcon;

	UPROPERTY()
	TObjectPtr<ASPCollectibleItem> SourceItem;

	bool IsOccupied() const { return ContentType != EInventorySlotContentType::Empty; }

	void Clear()
	{
		ContentType = EInventorySlotContentType::Empty;
		ConsumableType = EConsumableItemType::None;
		CollectibleSize = ECollectibleSize::Small;
		CollectibleValue = 0;
		CollectibleIcon = nullptr;
		SourceItem = nullptr;
	}
};

USTRUCT(BlueprintType)
struct FPerkSlotEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	EPerkType PerkType = EPerkType::None;

	bool IsEquipped() const { return PerkType != EPerkType::None; }
};

namespace SPInventoryText
{
	SPACH4_API FText GetConsumableDisplayName(EConsumableItemType Type);
	SPACH4_API FText GetCollectibleDisplayName(ECollectibleSize Size);
	SPACH4_API FText GetPerkDisplayName(EPerkType Type);
}

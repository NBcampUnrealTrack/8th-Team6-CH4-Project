#pragma once

#include "CoreMinimal.h"
#include "Inventory/SPInventoryTypes.h"
#include "Systems/Data/KillerData.h"
#include "SPPlayerLoadout.generated.h"

USTRUCT(BlueprintType)
struct SPACH4_API FSPPlayerLoadout
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout|Survivor")
	EConsumableItemType SurvivorItem = EConsumableItemType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout|Survivor")
	TArray<EPerkType> SurvivorPerks;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout|Killer")
	TArray<EKillerPerkType> KillerPerks;

	bool operator==(const FSPPlayerLoadout& Other) const
	{
		return SurvivorItem == Other.SurvivorItem
			&& SurvivorPerks == Other.SurvivorPerks
			&& KillerPerks == Other.KillerPerks;
	}

	bool operator!=(const FSPPlayerLoadout& Other) const
	{
		return !(*this == Other);
	}
};

namespace SPPlayerLoadout
{
	inline constexpr int32 SurvivorPerkCount = 2;
	inline constexpr int32 KillerPerkCount = 2;

	SPACH4_API bool IsValidSurvivorItem(EConsumableItemType ItemType);
	SPACH4_API bool IsValidSurvivorPerk(EPerkType PerkType);
	SPACH4_API bool IsValidKillerPerk(EKillerPerkType PerkType);
	
	SPACH4_API bool IsSurvivorLoadoutCompleteOnlyItem(const FSPPlayerLoadout& Loadout);
	SPACH4_API bool IsSurvivorLoadoutComplete(const FSPPlayerLoadout& Loadout);
	SPACH4_API bool IsKillerLoadoutComplete(const FSPPlayerLoadout& Loadout);
	SPACH4_API bool IsComplete(const FSPPlayerLoadout& Loadout);
}

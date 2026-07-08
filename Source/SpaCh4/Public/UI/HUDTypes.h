#pragma once

#include "CoreMinimal.h"
#include "Inventory/SPInventoryTypes.h"
#include "HUDTypes.generated.h"

class UTexture2D;

namespace SpaCh4HUD
{
	static constexpr int32 InventorySlotCount = 4;
	static constexpr int32 PerkSlotCount = 2;
	static constexpr int32 TeammateSlotCount = 3;
	static constexpr int32 DeliveryProgressSegmentCount = 10;
}

/** UI 표시용 생존자 상태 (게임플레이 enum과 분리) */
UENUM(BlueprintType)
enum class ESurvivorDisplayState : uint8
{
	Healthy,
	Injured,
	Downed,
	Carried,
	Caged,
	Dead,
	Escaped
};

USTRUCT(BlueprintType)
struct FTeammateHUDData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD")
	FText Nickname;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD")
	int32 CageStack = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD")
	int32 MaxCageStack = 2;

	/** 0~1, Downed 상태에서만 표시 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD")
	float DownedHealthPercent = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD")
	ESurvivorDisplayState DisplayState = ESurvivorDisplayState::Healthy;
};

USTRUCT(BlueprintType)
struct FDeliveryHUDData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "HUD")
	FName StationId;

	UPROPERTY(BlueprintReadOnly, Category = "HUD")
	int32 CurrentValue = 0;

	UPROPERTY(BlueprintReadOnly, Category = "HUD")
	int32 TargetValue = 200;
};

USTRUCT(BlueprintType)
struct FInventorySlotHUDData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "HUD")
	bool bIsOccupied = false;

	UPROPERTY(BlueprintReadOnly, Category = "HUD")
	EInventorySlotContentType ContentType = EInventorySlotContentType::Empty;

	UPROPERTY(BlueprintReadOnly, Category = "HUD")
	FText ItemName;

	UPROPERTY(BlueprintReadOnly, Category = "HUD")
	TSoftObjectPtr<UTexture2D> Icon;
};

USTRUCT(BlueprintType)
struct FPerkHUDData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "HUD")
	bool bIsEquipped = false;

	UPROPERTY(BlueprintReadOnly, Category = "HUD")
	FText PerkName;
};

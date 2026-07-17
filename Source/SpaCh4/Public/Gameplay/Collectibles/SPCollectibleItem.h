#pragma once

#include "CoreMinimal.h"
#include "Gameplay/Items/SPPickupItem.h"
#include "SPCollectibleItem.generated.h"

UENUM(BlueprintType)
enum class ECollectibleSize : uint8
{
	Small,
	Medium,
	Large,
	Hazardous
};

UCLASS(Blueprintable)
class SPACH4_API ASPCollectibleItem : public ASPPickupItem
{
	GENERATED_BODY()

public:
	ASPCollectibleItem();

	int32 GetValue() const { return Value; }
	ECollectibleSize GetCollectibleSize() const { return CollectibleSize; }
	void SetPickupCollisionEnabled(bool bEnabled);

	UFUNCTION(NetMulticast, Reliable, meta = (DeprecatedFunction, DeprecationMessage = "Use SetStored on the server."))
	void Multicast_SetStored(bool bNewStored, FVector DropLocation);

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SP|Collectible")
	ECollectibleSize CollectibleSize = ECollectibleSize::Small;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SP|Collectible")
	int32 Value = 10;
};

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SpCh4CollectibleItem.generated.h"

UENUM(BlueprintType)
enum class ESpCh4CollectibleSize : uint8
{
	Small,
	Medium,
	Large,
	Hazardous
};

UCLASS()
class SPACH4_API ASpCh4CollectibleItem : public AActor
{
	GENERATED_BODY()

public:
	ASpCh4CollectibleItem();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collectible")
	ESpCh4CollectibleSize CollectibleSize = ESpCh4CollectibleSize::Small;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collectible")
	int32 Value = 10;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collectible")
	bool bBlocksParkourWhileCarried = false;
};

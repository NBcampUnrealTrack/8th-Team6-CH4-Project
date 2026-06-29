#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CollectibleItem.generated.h"

UENUM(BlueprintType)
enum class ECollectibleSize : uint8
{
	Small,
	Medium,
	Large,
	Hazardous
};

UCLASS()
class SPACH4_API ACollectibleItem : public AActor
{
	GENERATED_BODY()

public:
	ACollectibleItem();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collectible")
	ECollectibleSize CollectibleSize = ECollectibleSize::Small;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collectible")
	int32 Value = 10;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collectible")
	bool bBlocksParkourWhileCarried = false;
};

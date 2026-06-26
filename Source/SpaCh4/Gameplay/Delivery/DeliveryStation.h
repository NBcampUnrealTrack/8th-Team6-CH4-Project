#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DeliveryStation.generated.h"

UCLASS()
class SPACH4_API ADeliveryStation : public AActor
{
	GENERATED_BODY()

public:
	ADeliveryStation();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Delivery")
	FName StationId = "A";

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Delivery")
	int32 TargetValue = 200;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Delivery")
	int32 CurrentValue = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Delivery")
	float DeliveryDuration = 2.5f;
};

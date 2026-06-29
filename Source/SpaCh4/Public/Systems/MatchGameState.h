#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "MatchGameState.generated.h"

UCLASS()
class SPACH4_API AMatchGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	AMatchGameState();

	int32 GetDeliveryStationAValue() const { return DeliveryStationAValue; }
	int32 GetDeliveryStationBValue() const { return DeliveryStationBValue; }
	int32 GetDeliveryStationTargetValue() const { return DeliveryStationTargetValue; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Delivery")
	int32 DeliveryStationAValue = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Delivery")
	int32 DeliveryStationBValue = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Delivery")
	int32 DeliveryStationTargetValue = 200;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Match")
	int32 EscapedSurvivorCount = 0;
};

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "SpCh4GameState.generated.h"

UCLASS()
class SPACH4_API ASpCh4GameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	ASpCh4GameState();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Delivery")
	int32 DeliveryStationAValue = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Delivery")
	int32 DeliveryStationBValue = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Match")
	int32 EscapedSurvivorCount = 0;
};

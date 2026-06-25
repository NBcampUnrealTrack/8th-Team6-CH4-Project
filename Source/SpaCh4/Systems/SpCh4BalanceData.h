#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SpCh4BalanceData.generated.h"

UCLASS(BlueprintType)
class SPACH4_API USpCh4BalanceData : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Match")
	float MatchTimeLimit = 900.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Delivery")
	int32 DeliveryStationTargetValue = 200;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Escape")
	float EscapeGateOpenDuration = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Killer")
	float KillerSpeedRatio = 1.15f;
};

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "BalanceData.generated.h"

UENUM(BlueprintType)
enum class ECollectibleType : uint8
{
	Small,
	Medium,
	Large,
	Dangerous
};

UCLASS(BlueprintType)
class SPACH4_API UBalanceData : public UDataAsset
{
	GENERATED_BODY()

public:
	UBalanceData();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Balance|Collectible")
	int32 GetCollectibleValue(ECollectibleType Type) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Balance|Delivery")
	int32 GetStationTargetValue(FName StationId) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Balance|Delivery")
	int32 GetTotalRequiredValue() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Balance|Escape")
	int32 GetHatchRequiredDeliveredValue() const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Match")
	float MatchTimeLimit = 900.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Delivery")
	int32 DeliveryStationATargetValue = 200;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Delivery")
	int32 DeliveryStationBTargetValue = 200;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Delivery")
	int32 TotalRequiredValue = 400;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Delivery")
	float DeliveryDuration = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collectible")
	int32 SmallCollectibleValue = 10;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collectible")
	int32 MediumCollectibleValue = 30;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collectible")
	int32 LargeCollectibleValue = 50;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collectible")
	int32 DangerousCollectibleValue = 60;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Escape")
	float EscapeGateOpenDuration = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Escape")
	float HatchEscapeDuration = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cage")
	float CageRescueDuration = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Survivor")
	int32 InitialSurvivorCount = 3;
};

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SpCh4Cage.generated.h"

UENUM(BlueprintType)
enum class ESpCh4CageStage : uint8
{
	Empty,
	StageOne,
	StageTwo,
	Dead
};

UCLASS()
class SPACH4_API ASpCh4Cage : public AActor
{
	GENERATED_BODY()

public:
	ASpCh4Cage();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cage")
	float StageOneDuration = 35.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cage")
	float StageTwoDuration = 35.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cage")
	float RescueDuration = 4.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cage")
	ESpCh4CageStage CageStage = ESpCh4CageStage::Empty;
};

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Cage.generated.h"

UENUM(BlueprintType)
enum class ECageStage : uint8
{
	Empty,
	StageOne,
	StageTwo,
	Dead
};

UCLASS()
class SPACH4_API ACage : public AActor
{
	GENERATED_BODY()

public:
	ACage();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cage")
	float StageOneDuration = 35.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cage")
	float StageTwoDuration = 35.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cage")
	float RescueDuration = 4.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cage")
	ECageStage CageStage = ECageStage::Empty;
};

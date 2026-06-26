#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Hatch.generated.h"

UCLASS()
class SPACH4_API AHatch : public AActor
{
	GENERATED_BODY()

public:
	AHatch();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hatch")
	float EscapeDuration = 3.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hatch")
	bool bIsSpawned = false;
};

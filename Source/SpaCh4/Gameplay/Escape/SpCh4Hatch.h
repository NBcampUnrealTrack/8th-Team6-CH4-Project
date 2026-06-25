#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SpCh4Hatch.generated.h"

UCLASS()
class SPACH4_API ASpCh4Hatch : public AActor
{
	GENERATED_BODY()

public:
	ASpCh4Hatch();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hatch")
	float EscapeDuration = 3.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hatch")
	bool bIsSpawned = false;
};

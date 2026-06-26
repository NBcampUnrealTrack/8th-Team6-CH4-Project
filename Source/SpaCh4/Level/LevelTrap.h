#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LevelTrap.generated.h"

UCLASS()
class SPACH4_API ALevelTrap : public AActor
{
	GENERATED_BODY()

public:
	ALevelTrap();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level Trap")
	FName TrapId = "NoiseFloor";

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level Trap")
	float TriggerCooldown = 3.0f;
};

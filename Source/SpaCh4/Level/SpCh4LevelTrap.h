#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SpCh4LevelTrap.generated.h"

UCLASS()
class SPACH4_API ASpCh4LevelTrap : public AActor
{
	GENERATED_BODY()

public:
	ASpCh4LevelTrap();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level Trap")
	FName TrapId = "NoiseFloor";

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level Trap")
	float TriggerCooldown = 3.0f;
};

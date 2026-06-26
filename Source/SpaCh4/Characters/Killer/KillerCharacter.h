#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "KillerCharacter.generated.h"

UCLASS()
class SPACH4_API AKillerCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AKillerCharacter();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Killer")
	float AttackCooldown = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Killer")
	float CarryMoveSpeedMultiplier = 0.9f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Killer")
	float PickupDuration = 1.25f;
};

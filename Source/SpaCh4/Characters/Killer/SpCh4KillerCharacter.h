#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "SpCh4KillerCharacter.generated.h"

UCLASS()
class SPACH4_API ASpCh4KillerCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ASpCh4KillerCharacter();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Killer")
	float AttackCooldown = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Killer")
	float CarryMoveSpeedMultiplier = 0.9f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Killer")
	float PickupDuration = 1.25f;
};

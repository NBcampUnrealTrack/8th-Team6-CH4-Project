#pragma once

#include "CoreMinimal.h"
#include "Characters/Base/CharacterBase.h"
#include "KillerCharacter.generated.h"

UCLASS()
class SPACH4_API AKillerCharacter : public ACharacterBase
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

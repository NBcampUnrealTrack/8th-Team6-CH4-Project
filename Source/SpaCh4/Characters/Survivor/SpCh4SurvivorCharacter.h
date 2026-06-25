#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "SpCh4SurvivorCharacter.generated.h"

UENUM(BlueprintType)
enum class ESpCh4SurvivorState : uint8
{
	Healthy,
	Injured,
	Downed,
	Carried,
	Caged,
	Dead,
	Escaped
};

UCLASS()
class SPACH4_API ASpCh4SurvivorCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ASpCh4SurvivorCharacter();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Survivor")
	float SprintSpeed = 650.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Survivor")
	float CrouchMoveSpeed = 220.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Survivor")
	ESpCh4SurvivorState SurvivorState = ESpCh4SurvivorState::Healthy;
};

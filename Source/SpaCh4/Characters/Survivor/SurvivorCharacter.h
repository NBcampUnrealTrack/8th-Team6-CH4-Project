#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "SurvivorCharacter.generated.h"

UENUM(BlueprintType)
enum class ESurvivorState : uint8
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
class SPACH4_API ASurvivorCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ASurvivorCharacter();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Survivor")
	float SprintSpeed = 650.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Survivor")
	float CrouchMoveSpeed = 220.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Survivor")
	ESurvivorState SurvivorState = ESurvivorState::Healthy;
};

#pragma once

#include "CoreMinimal.h"
#include "Characters/Base/CharacterBase.h"
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
class SPACH4_API ASurvivorCharacter : public ACharacterBase
{
	GENERATED_BODY()

public:
	ASurvivorCharacter();

private:
	// 추후 구현
	/*
	UPROPERTY(EditAnywhere, Category = "Survivor")
	float SprintSpeed = 650.0f;

	UPROPERTY(EditAnywhere, Category = "Survivor")
	float CrouchMoveSpeed = 220.0f;

	UPROPERTY(VisibleAnywhere, Category = "Survivor")
	ESurvivorState SurvivorState = ESurvivorState::Healthy;
	*/
};

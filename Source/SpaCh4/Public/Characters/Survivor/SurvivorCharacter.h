#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "SurvivorCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputConfigData;
struct FInputActionValue;

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
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);

private:
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputConfigData> InputConfig;
	
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USpringArmComponent> SpringArm;
	
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UCameraComponent> Camera;

	/* 
	UPROPERTY(EditAnywhere, Category = "Survivor")
	float SprintSpeed = 650.0f;

	UPROPERTY(EditAnywhere, Category = "Survivor")
	float CrouchMoveSpeed = 220.0f;

	UPROPERTY(VisibleAnywhere, Category = "Survivor")
	ESurvivorState SurvivorState = ESurvivorState::Healthy;
	*/
};

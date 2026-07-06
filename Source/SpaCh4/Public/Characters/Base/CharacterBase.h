#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "CharacterBase.generated.h"

class USpringArmComponent;
class UCameraComponent;
class USPInputConfigData;
struct FInputActionValue;


UCLASS(Abstract)
class SPACH4_API ACharacterBase : public ACharacter
{
	GENERATED_BODY()

public:
	ACharacterBase();

protected:
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void Interact();
	virtual void JumpOver();
	
	virtual void Move(const FInputActionValue& Value);

	void Look(const FInputActionValue& Value);
	void EnsureInputConfig();
	void InitializeInputMappingContexts();

	UPROPERTY(EditDefaultsOnly, Category = "SP|Input")
	TObjectPtr<USPInputConfigData> InputConfig;

	UPROPERTY(VisibleAnywhere, Category = "SP|Camera")
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere, Category = "SP|Camera")
	TObjectPtr<UCameraComponent> Camera;
	
	UPROPERTY(EditAnywhere, Category = "SP|Debug")
	bool bDrawDebug {false};
};

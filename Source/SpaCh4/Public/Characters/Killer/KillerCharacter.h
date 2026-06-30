#pragma once

#include "CoreMinimal.h"
#include "Characters/Base/CharacterBase.h"
#include "KillerCharacter.generated.h"

class UKillerData;

// ---------------------------------------------------------------
// KillerState (살인마 상태)
// ---------------------------------------------------------------
UENUM(BlueprintType)
enum class EKillerState : uint8
{
    Idle        = 0 UMETA(DisplayName = "Idle"),
    Attacking   = 1 UMETA(DisplayName = "Attacking"),
    Groggy      = 2 UMETA(DisplayName = "Groggy"),
    PickingUp   = 3 UMETA(DisplayName = "PickingUp"),
    Carrying    = 4 UMETA(DisplayName = "Carrying"),
    Interacting = 5 UMETA(DisplayName = "Interacting")
};

UCLASS()
class SPACH4_API AKillerCharacter : public ACharacterBase
{
    GENERATED_BODY()

public:
    AKillerCharacter();
    virtual void BeginPlay() override;
    virtual void PostInitializeComponents() override;

protected:
    bool bIsBusy = false;
    
    virtual void Interact() override;
    void Attack();

    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
    
    EKillerState CurrentState = EKillerState::Idle;
    void SetKillerState(EKillerState NewState);
    void UpdateMovementSpeed();

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Killer Data")
    TObjectPtr<UKillerData> KillerData;

    // 공격 및 상호작용 로직
    void PerformAttack();
    void PickupSurvivor(AActor* Target);
    void DropSurvivor();
    
    AActor* FindInteractableActor(float Radius);
    bool PerformAttackTrace();

    UPROPERTY()
    AActor* CarriedSurvivor;
};
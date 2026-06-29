#pragma once

#include "CoreMinimal.h"
#include "Characters/Base/CharacterBase.h"
#include "KillerCharacter.generated.h"

UENUM(BlueprintType)
enum class EKillerState : uint8 { Idle, Attacking, Groggy, PickingUp, Carrying, Interacting };

class UInputAction;

UCLASS()
class SPACH4_API AKillerCharacter : public ACharacterBase
{
    GENERATED_BODY()

public:
    AKillerCharacter();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void BeginPlay() override;

protected:
    // 경고 해결: protected로 수정
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

    void HandleAttack();
    void HandleInteract();

    UPROPERTY(ReplicatedUsing = OnRep_KillerState, VisibleAnywhere, Category = "Killer Character|State")
    EKillerState CurrentState = EKillerState::Idle;

    UFUNCTION()
    void OnRep_KillerState();

    void SetKillerState(EKillerState NewState);
    void UpdateMovementSpeed();

    UFUNCTION(Server, Reliable)
    void Server_Attack();

    UFUNCTION(Server, Reliable)
    void Server_Interact();

    UFUNCTION(Server, Reliable)
    void Server_PickupSurvivor(AActor* TargetSurvivor);

    bool PerformAttackTrace();
    
    UPROPERTY(Replicated)
    TObjectPtr<AActor> CarriedSurvivor;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Killer Character|Input")
    TObjectPtr<UInputAction> AttackAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Killer Character|Input")
    TObjectPtr<UInputAction> InteractAction;

    const float BaseSpeed = 748.f;
};
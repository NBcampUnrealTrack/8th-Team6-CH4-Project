#pragma once

#include "CoreMinimal.h"
#include "Characters/Base/CharacterBase.h"
#include "InputActionValue.h" // 추가 필수
#include "KillerCharacter.generated.h"

UENUM(BlueprintType)
enum class EKillerState : uint8 { Idle, Attacking, Groggy, PickingUp, Carrying, Interacting };

UCLASS()
class SPACH4_API AKillerCharacter : public ACharacterBase
{
    GENERATED_BODY()

public:
    AKillerCharacter();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void BeginPlay() override;

protected:
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

    // EnhancedInput 요구 사양에 맞게 FInputActionValue 인자 추가
    void HandleAttack(const FInputActionValue& Value);
    void HandleInteract(const FInputActionValue& Value);

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
    UFUNCTION(Server, Reliable)
    void Server_DropSurvivor();
    UFUNCTION(Server, Reliable)
    void Server_HookSurvivor();

    AActor* FindInteractableActor(float Radius);
    bool PerformAttackTrace();
    
    UPROPERTY(Replicated)
    TObjectPtr<AActor> CarriedSurvivor;

    // 블루프린트에서 직접 에셋 할당
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Killer Character|Input")
    TObjectPtr<class UInputAction> AttackAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Killer Character|Input")
    TObjectPtr<class UInputAction> InteractAction;

    const float BaseSpeed = 748.f;
};
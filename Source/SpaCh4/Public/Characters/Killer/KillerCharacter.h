#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
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
    virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void BeginPlay() override;
    virtual void PostInitializeComponents() override;
    
    UFUNCTION(BlueprintPure, Category = "Tags")
    const FGameplayTagContainer& GetCharTags() const { return charTag; }

    UFUNCTION(BlueprintCallable, Category = "Tags")
    void AddCharTag(FGameplayTag NewTag) { charTag.AddTag(NewTag); }

protected:
    // 서버와 동기화되는 상태
    UPROPERTY(ReplicatedUsing = OnRep_CurrentState)
    EKillerState CurrentState = EKillerState::Idle;

    UFUNCTION()
    void OnRep_CurrentState();

    UPROPERTY(Replicated)
    bool bIsBusy = false;

    // 입력 및 상호작용
    virtual void Interact() override;
    void Attack();

    UFUNCTION(Server, Reliable)
    void Server_Attack();
    
    UFUNCTION(Server, Reliable)
    void Server_Interact();

    void PerformAttack();
    void SetKillerState(EKillerState NewState);
    void UpdateMovementSpeed();
    
    // 로직
    bool PerformAttackTrace();
    void PickupSurvivor(AActor* Target);
    void DropSurvivor();
    void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent);
    AActor* FindInteractableActor(float Radius);

    UPROPERTY(EditDefaultsOnly, Category = "Killer Data")
    TObjectPtr<UKillerData> KillerData;

    UPROPERTY(Replicated)
    AActor* CarriedSurvivor;

    UPROPERTY(VisibleAnywhere, Category = "Tags")
    FGameplayTagContainer charTag;
};
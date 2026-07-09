#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Characters/Base/CharacterBase.h"
#include "KillerCharacter.generated.h"

class UKillerData;
class UMeshComponent;

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
    virtual void PossessedBy(AController* NewController) override;
    virtual void PostInitializeComponents() override;
    
    UFUNCTION(BlueprintPure, Category = "Tags")
    const FGameplayTagContainer& GetCharTags() const { return charTag; }

    UFUNCTION(BlueprintCallable, Category = "Tags")
    void AddCharTag(FGameplayTag NewTag) { charTag.AddTag(NewTag); }

protected:
    bool bCanPickup = true;
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
    void SetupKillerFirstPersonCamera();
    void ApplyFirstPersonArmVisibility(USkeletalMeshComponent* TargetMesh, const TArray<FName>& VisibleRootBones) const;
    
    // 로직
    bool PerformAttackTrace();
    void PickupSurvivor(AActor* Target);
    void DropSurvivor();
    void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent);
    AActor* FindInteractableActor(float Radius);

    UPROPERTY(EditDefaultsOnly, Category = "Killer Data")
    TObjectPtr<UKillerData> KillerData;

    /** Bone (or socket) on the killer mesh where the first-person camera is anchored. */
    UPROPERTY(EditDefaultsOnly, Category = "Killer|Camera")
    FName CameraAttachBoneName = TEXT("head");

    /** Eye offset from CameraAttachBoneName (local space). */
    UPROPERTY(EditDefaultsOnly, Category = "Killer|Camera")
    FVector CameraRelativeOffset = FVector(10.f, 0.f, 0.f);

    /** Hide the locally controlled killer's shadow on this client. */
    UPROPERTY(EditDefaultsOnly, Category = "Killer|Camera")
    bool bHideOwnerShadow = true;

    /** Show only arms/hands to the owner via a leader-pose mesh; body stays visible to others. */
    UPROPERTY(EditDefaultsOnly, Category = "Killer|Camera")
    bool bShowFirstPersonArmsOnly = true;

    /**
     * Root bones kept visible on the first-person mesh (children included).
     * Leave empty to use standard clavicle/arm/hand names.
     */
    UPROPERTY(EditDefaultsOnly, Category = "Killer|Camera")
    TArray<FName> OwnerVisibleArmBones;

    /** Extra meshes hidden from owner only (e.g. hood). */
    UPROPERTY(EditDefaultsOnly, Category = "Killer|Camera")
    TArray<TObjectPtr<UMeshComponent>> OwnerHiddenMeshComponents;

    UPROPERTY(VisibleAnywhere, Category = "Killer|Camera")
    TObjectPtr<class USkeletalMeshComponent> FirstPersonArmsMesh;

    UPROPERTY(Replicated)
    AActor* CarriedSurvivor;

    UPROPERTY(VisibleAnywhere, Category = "Tags")
    FGameplayTagContainer charTag;
};
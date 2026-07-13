#pragma once

#include "CoreMinimal.h"
#include "Characters/Base/CharacterBase.h"
#include "GameplayTagAssetInterface.h"
#include "GameplayTagContainer.h"
#include "Inventory/SPInventoryTypes.h"
#include "SurvivorCharacter.generated.h"

class USurvivorData;
class USPInteractionComponent;
class USPMovementComponent;
class USPParkourComponent;
class USPEscapeLeverComponent;
class USPPickupAnimComponent;
class USPHealingAnimComponent;
class ASPCollectibleItem;
class ASPDeliveryStation;
class ASPEscapeGate;
class ASPHatch;
class USPInventoryComponent;
class ACage;

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
class SPACH4_API ASurvivorCharacter : public ACharacterBase, public IGameplayTagAssetInterface
{
	GENERATED_BODY()

public:
	ASurvivorCharacter();
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category = "SP|Survivor")
	void SetSurvivorState(ESurvivorState NewState);
	ESurvivorState GetSurvivorState() const { return SurvivorState; };

	/** Remaining downed / bleedout health in [0,1]. Meaningful only while Downed. */
	UFUNCTION(BlueprintPure, Category = "SP|Survivor")
	float GetDownedHealthPercent() const { return DownedHealthPercent; }

	bool CanMove() const;
	bool CanInteract() const;
	bool CanJumpOver() const;
	bool IsParkouring() const;
	bool IsCarrying() const;
	bool IsPullingLever() const;
	bool IsPlayingPickupAnim() const;
	bool IsHealing() const;
	int GetCagedCount() const { return CagedCount; }
	int32 GetSelectedSlotIndex() const { return SelectedSlotIndex; }

	const USurvivorData* GetSurvivorData() const { return SurvivorData; }
	USPInteractionComponent* GetInteractionComponent() const { return InteractionComponent; }
	USPParkourComponent* GetParkourComponent() const { return ParkourComponent; }
	USPEscapeLeverComponent* GetEscapeLeverComponent() const { return EscapeLeverComponent; }
	USPPickupAnimComponent* GetPickupAnimComponent() const { return PickupAnimComponent; }
	USPHealingAnimComponent* GetHealingAnimComponent() const { return HealingAnimComponent; }

	void BeginPickup(ASPCollectibleItem* Item);
	void BeginDelivery(ASPDeliveryStation* Station);
	void BeginEscapeOpen(ASPEscapeGate* Gate);
	void EndEscapeChanneling();
	void BeginHatchEscape(ASPHatch* Hatch);
	void CompleteHatchEscape();

	void EnterCaged(class ACage* Cage);
	void RescueFromCage();
	void ApplyHit();

	USPInventoryComponent* GetInventoryComponent() const { return InventoryComponent; }

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "SP|Inventory")
	bool TryAcquireConsumable(EConsumableItemType ItemType);

	FGameplayTag GetInteractableTag() const;
	virtual void GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const override;

	void NotifyParkourEnded();
	void NotifyLeverChannelEnded();
	void NotifyPickupAnimEnded();
	void NotifyHealingAnimEnded();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SP|Inventory")
	TObjectPtr<USPInventoryComponent> InventoryComponent;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	virtual void Move(const FInputActionValue& Value) override;
	virtual void Interact() override;
	virtual void JumpOver() override;
	
	virtual void OnRep_Controller() override;
private:
	UFUNCTION()
	void OnRep_SurvivorState(ESurvivorState OldState);

	UFUNCTION()
	void HandleInventoryChanged();

	void SelectSlot(int32 Index);

	UFUNCTION(Server, Reliable)
	void Server_SelectSlot(int32 Index);

	void StartRun();
	void StopRun();

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_SetWantsToRun(bool bNewWantsToRun);

	void BindInventoryHudRefresh();
	void RefreshLocalInventoryHud() const;
	void ApplyStateEffects();
	void ApplyDownedMeshOffset();
	void NotifyMatchStateChange(ESurvivorState NewState);
	void ToggleCrouch();
	void DebugTestHealingAnimPressed();
	void DebugTestHealingAnimReleased();
	void RestoreDebugMovementInput();

	UFUNCTION()
	void OnCageExpired();

	UPROPERTY(VisibleAnywhere, Category = "SP|Component")
	TObjectPtr<USPInteractionComponent> InteractionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SP|Component",  meta = (AllowPrivateAccess = true))
	TObjectPtr<USPMovementComponent> MovementComponent;

	UPROPERTY(VisibleAnywhere, Category = "SP|Component")
	TObjectPtr<USPParkourComponent> ParkourComponent;

	UPROPERTY(VisibleAnywhere, Category = "SP|Component")
	TObjectPtr<USPEscapeLeverComponent> EscapeLeverComponent;

	UPROPERTY(VisibleAnywhere, Category = "SP|Component")
	TObjectPtr<USPPickupAnimComponent> PickupAnimComponent;

	UPROPERTY(VisibleAnywhere, Category = "SP|Component")
	TObjectPtr<USPHealingAnimComponent> HealingAnimComponent;

	UPROPERTY(ReplicatedUsing = "OnRep_SurvivorState")
	ESurvivorState SurvivorState = ESurvivorState::Healthy;

	/** Remaining bleedout health while Downed (1 = full, 0 = dead). */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "SP|Survivor", meta = (AllowPrivateAccess = true))
	float DownedHealthPercent = 1.f;

	UPROPERTY(EditDefaultsOnly, Category = "SP|Data")
	TObjectPtr<USurvivorData> SurvivorData;

	/** Cached at BeginPlay; restored when leaving Downed. */
	FVector StandingMeshRelativeLocation = FVector::ZeroVector;
	bool bStandingMeshLocationCached = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "SP|Data", meta = (AllowPrivateAccess = true))
	int32 CagedCount = 0;

	FTimerHandle CageTimerHandle;
	int32 SelectedSlotIndex = 0;
	
	UPROPERTY(VisibleAnywhere, Category = "SP|Tags")
	FGameplayTagContainer OwningTag;
};

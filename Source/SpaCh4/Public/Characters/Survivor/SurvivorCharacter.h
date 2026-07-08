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
class ASPCollectibleItem;
class ASPDeliveryStation;
class ASPEscapeGate;
class ASPHatch;
class USPInventoryComponent;

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

	bool CanMove() const;
	bool CanInteract() const;
	bool CanJumpOver() const;
	bool IsParkouring() const;

	const USurvivorData* GetSurvivorData() const { return SurvivorData; }
	USPInteractionComponent* GetInteractionComponent() const { return InteractionComponent; }
	USPParkourComponent* GetParkourComponent() const { return ParkourComponent; }

	void BeginPickup(ASPCollectibleItem* Item);
	void BeginDelivery(ASPDeliveryStation* Station);
	void BeginEscapeOpen(ASPEscapeGate* Gate);
	void EndEscapeChanneling();
	void BeginHatchEscape(ASPHatch* Hatch);
	void CompleteHatchEscape();
	bool IsCarrying() const;

	USPInventoryComponent* GetInventoryComponent() const { return InventoryComponent; }

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "SP|Inventory")
	bool TryAcquireConsumable(EConsumableItemType ItemType);

	FGameplayTag GetInteractableTag() const;
	virtual void GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const override;

	void NotifyParkourEnded();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SP|Inventory")
	TObjectPtr<USPInventoryComponent> InventoryComponent;

protected:
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	virtual void Move(const FInputActionValue& Value) override;
	virtual void Interact() override;
	virtual void JumpOver() override;

private:
	UFUNCTION()
	void OnRep_SurvivorState(ESurvivorState OldState);

	UFUNCTION()
	void HandleInventoryChanged();

	void BindInventoryHudRefresh();
	void RefreshLocalInventoryHud() const;
	void ApplyStateEffects();
	void NotifyMatchStateChange(ESurvivorState NewState);
	void ToggleCrouch();

	UPROPERTY(VisibleAnywhere, Category = "SP|Component")
	TObjectPtr<USPInteractionComponent> InteractionComponent;

	UPROPERTY(VisibleAnywhere, Category = "SP|Component")
	TObjectPtr<USPMovementComponent> MovementComponent;

	UPROPERTY(VisibleAnywhere, Category = "SP|Component")
	TObjectPtr<USPParkourComponent> ParkourComponent;

	UPROPERTY(ReplicatedUsing = "OnRep_SurvivorState")
	ESurvivorState SurvivorState = ESurvivorState::Healthy;

	UPROPERTY(EditDefaultsOnly, Category = "SP|Data")
	TObjectPtr<USurvivorData> SurvivorData;

	UPROPERTY(VisibleAnywhere, Category = "SP|Tags")
	FGameplayTagContainer OwningTag;
};

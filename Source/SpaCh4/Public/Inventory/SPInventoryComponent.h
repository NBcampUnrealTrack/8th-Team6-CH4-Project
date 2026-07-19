#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Inventory/SPInventoryTypes.h"
#include "UI/HUDTypes.h"
#include "SPInventoryComponent.generated.h"

class ASPCollectibleItem;
class ASPConsumableItem;
class ASPMedkitItem;
class ASPPickupItem;
class ASPSpeedPotionItem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryComponentChanged);

UCLASS(ClassGroup = (SP), meta = (BlueprintSpawnableComponent))
class SPACH4_API USPInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USPInventoryComponent();

	static constexpr int32 InventorySlotCount = 4;
	static constexpr int32 PerkSlotCount = 2;

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	const TArray<FInventorySlotEntry>& GetInventorySlots() const { return InventorySlots; }

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	const TArray<FPerkSlotEntry>& GetPerkSlots() const { return PerkSlots; }

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	bool TryAddConsumable(EConsumableItemType ItemType);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	bool RemoveConsumable(EConsumableItemType ItemType);

	bool ConsumeConsumableAtSlot(int32 Index, EConsumableItemType ExpectedType);
	bool TryStoreWorldItem(ASPPickupItem* Item);
	bool CanStoreWorldItem(const ASPPickupItem* Item) const;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool IsSlotConsumable(int32 Index, EConsumableItemType ItemType) const;

	EConsumableItemType GetConsumableTypeAtSlot(int32 Index) const;
	int32 CountConsumable(EConsumableItemType ItemType) const;
	bool HasPerk(EPerkType PerkType) const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	void SetCollectibleFromItem(ASPCollectibleItem* Item);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	void ClearCollectible();

	bool HasFreeSlot() const;
	bool HasAnyCollectible() const;
	bool IsSlotOccupied(int32 Index) const;
	bool IsSlotCollectible(int32 Index) const;
	bool DropSlot(int32 Index, ASPPickupItem*& OutSourceItem);
	bool DeliverSlot(int32 Index, int32& OutValue, ASPCollectibleItem*& OutSourceItem);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	bool EquipPerk(EPerkType PerkType);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	TArray<FInventorySlotHUDData> BuildInventoryHUDData() const;

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	TArray<FPerkHUDData> BuildPerkHUDData() const;

	void InitializeLoadout(EConsumableItemType LoadoutItem, const TArray<EPerkType>& LoadoutPerks);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void InitializeDefaultLoadout();

	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FOnInventoryComponentChanged OnInventoryChanged;

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_InventorySlots();

	UFUNCTION()
	void OnRep_PerkSlots();

	void BroadcastInventoryChanged();

	int32 FindFirstEmptySlotIndex() const;
	int32 FindConsumableSlotIndex(EConsumableItemType ItemType) const;
	int32 FindCollectibleSlotIndex() const;
	int32 FindLastCollectibleSlotIndex() const;
	ASPConsumableItem* SpawnConsumableItem(EConsumableItemType ItemType) const;
	ASPPickupItem* SpawnStackedItem(const FInventorySlotEntry& Slot) const;

	UPROPERTY(ReplicatedUsing = OnRep_InventorySlots, VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
	TArray<FInventorySlotEntry> InventorySlots;

	UPROPERTY(ReplicatedUsing = OnRep_PerkSlots, VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
	TArray<FPerkSlotEntry> PerkSlots;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Loadout")
	EConsumableItemType DefaultLoadoutItem = EConsumableItemType::Medkit;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Loadout")
	TArray<EPerkType> DefaultPerks;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Item Classes")
	TSubclassOf<ASPMedkitItem> MedkitItemClass;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Item Classes")
	TSubclassOf<ASPSpeedPotionItem> SpeedPotionItemClass;

private:
	bool bLoadoutInitialized = false;
};

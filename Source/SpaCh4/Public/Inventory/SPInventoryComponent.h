#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Inventory/SPInventoryTypes.h"
#include "UI/HUDTypes.h"
#include "SPInventoryComponent.generated.h"

class ASPCollectibleItem;
class UGameHUDWidget;

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

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	void SetCollectibleFromItem(const ASPCollectibleItem* Item);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	void ClearCollectible();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	bool EquipPerk(EPerkType PerkType);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	TArray<FInventorySlotHUDData> BuildInventoryHUDData() const;

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	TArray<FPerkHUDData> BuildPerkHUDData() const;

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
	int32 FindCollectibleSlotIndex() const;

	UPROPERTY(ReplicatedUsing = OnRep_InventorySlots, VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
	TArray<FInventorySlotEntry> InventorySlots;

	UPROPERTY(ReplicatedUsing = OnRep_PerkSlots, VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
	TArray<FPerkSlotEntry> PerkSlots;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Loadout")
	EConsumableItemType DefaultLoadoutItem = EConsumableItemType::Medkit;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory|Loadout")
	TArray<EPerkType> DefaultPerks;
};

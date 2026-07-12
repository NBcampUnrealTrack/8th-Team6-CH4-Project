#include "Inventory/SPInventoryComponent.h"

#include "Gameplay/Collectibles/SPCollectibleItem.h"
#include "Net/UnrealNetwork.h"

USPInventoryComponent::USPInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	InventorySlots.SetNum(InventorySlotCount);
	PerkSlots.SetNum(PerkSlotCount);

	DefaultPerks = {EPerkType::SilentRoll, EPerkType::ThreatSensor};
}

void USPInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(USPInventoryComponent, InventorySlots);
	DOREPLIFETIME(USPInventoryComponent, PerkSlots);
}

void USPInventoryComponent::OnRep_InventorySlots()
{
	BroadcastInventoryChanged();
}

void USPInventoryComponent::OnRep_PerkSlots()
{
	BroadcastInventoryChanged();
}

void USPInventoryComponent::BroadcastInventoryChanged()
{
	OnInventoryChanged.Broadcast();
}

int32 USPInventoryComponent::FindFirstEmptySlotIndex() const
{
	for (int32 Index = 0; Index < InventorySlots.Num(); ++Index)
	{
		if (!InventorySlots[Index].IsOccupied())
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

int32 USPInventoryComponent::FindCollectibleSlotIndex() const
{
	for (int32 Index = 0; Index < InventorySlots.Num(); ++Index)
	{
		if (InventorySlots[Index].ContentType == EInventorySlotContentType::Collectible)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

int32 USPInventoryComponent::FindLastCollectibleSlotIndex() const
{
	for (int32 Index = InventorySlots.Num() - 1; Index >= 0; --Index)
	{
		if (InventorySlots[Index].ContentType == EInventorySlotContentType::Collectible)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

bool USPInventoryComponent::HasFreeSlot() const
{
	return FindFirstEmptySlotIndex() != INDEX_NONE;
}

bool USPInventoryComponent::HasAnyCollectible() const
{
	return FindCollectibleSlotIndex() != INDEX_NONE;
}

bool USPInventoryComponent::IsSlotOccupied(int32 Index) const
{
	return InventorySlots.IsValidIndex(Index) && InventorySlots[Index].IsOccupied();
}

bool USPInventoryComponent::IsSlotCollectible(int32 Index) const
{
	return InventorySlots.IsValidIndex(Index) && InventorySlots[Index].ContentType == EInventorySlotContentType::Collectible;
}

bool USPInventoryComponent::DropSlot(int32 Index, ASPCollectibleItem*& OutSourceItem)
{
	OutSourceItem = nullptr;

	if (!GetOwner() || !GetOwner()->HasAuthority() || !InventorySlots.IsValidIndex(Index))
	{
		return false;
	}

	FInventorySlotEntry& Slot = InventorySlots[Index];
	if (!Slot.IsOccupied())
	{
		return false;
	}

	if (Slot.ContentType == EInventorySlotContentType::Collectible)
	{
		OutSourceItem = Slot.SourceItem;
	}

	Slot.Clear();
	BroadcastInventoryChanged();
	return true;
}

bool USPInventoryComponent::DeliverSlot(int32 Index, int32& OutValue, ASPCollectibleItem*& OutSourceItem)
{
	OutValue = 0;
	OutSourceItem = nullptr;

	if (!GetOwner() || !GetOwner()->HasAuthority() || !InventorySlots.IsValidIndex(Index))
	{
		return false;
	}

	FInventorySlotEntry& Slot = InventorySlots[Index];
	if (Slot.ContentType != EInventorySlotContentType::Collectible)
	{
		return false;
	}

	OutValue = Slot.CollectibleValue;
	OutSourceItem = Slot.SourceItem;
	Slot.Clear();
	BroadcastInventoryChanged();
	return true;
}

bool USPInventoryComponent::TryAddConsumable(const EConsumableItemType ItemType)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || ItemType == EConsumableItemType::None)
	{
		return false;
	}

	for (const FInventorySlotEntry& Slot : InventorySlots)
	{
		if (Slot.ContentType == EInventorySlotContentType::Consumable && Slot.ConsumableType == ItemType)
		{
			return false;
		}
	}

	const int32 EmptyIndex = FindFirstEmptySlotIndex();
	if (EmptyIndex == INDEX_NONE)
	{
		return false;
	}

	FInventorySlotEntry& Slot = InventorySlots[EmptyIndex];
	Slot.Clear();
	Slot.ContentType = EInventorySlotContentType::Consumable;
	Slot.ConsumableType = ItemType;
	BroadcastInventoryChanged();
	return true;
}

bool USPInventoryComponent::RemoveConsumable(const EConsumableItemType ItemType)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || ItemType == EConsumableItemType::None)
	{
		return false;
	}

	for (FInventorySlotEntry& Slot : InventorySlots)
	{
		if (Slot.ContentType == EInventorySlotContentType::Consumable && Slot.ConsumableType == ItemType)
		{
			Slot.Clear();
			BroadcastInventoryChanged();
			return true;
		}
	}
	return false;
}

void USPInventoryComponent::SetCollectibleFromItem(ASPCollectibleItem* Item)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !Item)
	{
		return;
	}

	const int32 EmptyIndex = FindFirstEmptySlotIndex();
	if (EmptyIndex == INDEX_NONE)
	{
		return;
	}

	FInventorySlotEntry& Slot = InventorySlots[EmptyIndex];
	Slot.Clear();
	Slot.ContentType = EInventorySlotContentType::Collectible;
	Slot.CollectibleSize = Item->GetCollectibleSize();
	Slot.CollectibleValue = Item->GetValue();
	Slot.CollectibleIcon = Item->GetIcon();
	Slot.SourceItem = Item;
	BroadcastInventoryChanged();
}

void USPInventoryComponent::ClearCollectible()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	const int32 CollectibleIndex = FindCollectibleSlotIndex();
	if (CollectibleIndex == INDEX_NONE)
	{
		return;
	}

	InventorySlots[CollectibleIndex].Clear();
	BroadcastInventoryChanged();
}

bool USPInventoryComponent::EquipPerk(const EPerkType PerkType)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || PerkType == EPerkType::None)
	{
		return false;
	}

	for (const FPerkSlotEntry& Slot : PerkSlots)
	{
		if (Slot.PerkType == PerkType)
		{
			return false;
		}
	}

	for (FPerkSlotEntry& Slot : PerkSlots)
	{
		if (!Slot.IsEquipped())
		{
			Slot.PerkType = PerkType;
			BroadcastInventoryChanged();
			return true;
		}
	}
	return false;
}

void USPInventoryComponent::InitializeDefaultLoadout()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	if (DefaultLoadoutItem != EConsumableItemType::None)
	{
		bool bHasLoadoutItem = false;
		for (const FInventorySlotEntry& Slot : InventorySlots)
		{
			if (Slot.ContentType == EInventorySlotContentType::Consumable)
			{
				bHasLoadoutItem = true;
				break;
			}
		}
		if (!bHasLoadoutItem)
		{
			TryAddConsumable(DefaultLoadoutItem);
		}
	}

	for (const EPerkType PerkType : DefaultPerks)
	{
		if (PerkType != EPerkType::None)
		{
			EquipPerk(PerkType);
		}
	}
}

TArray<FInventorySlotHUDData> USPInventoryComponent::BuildInventoryHUDData() const
{
	TArray<FInventorySlotHUDData> Result;
	Result.SetNum(InventorySlotCount);

	for (int32 Index = 0; Index < InventorySlots.Num() && Index < InventorySlotCount; ++Index)
	{
		const FInventorySlotEntry& Slot = InventorySlots[Index];
		FInventorySlotHUDData& HUDSlot = Result[Index];
		HUDSlot.bIsOccupied = Slot.IsOccupied();
		HUDSlot.ContentType = Slot.ContentType;

		switch (Slot.ContentType)
		{
		case EInventorySlotContentType::Consumable:
			HUDSlot.ItemName = SPInventoryText::GetConsumableDisplayName(Slot.ConsumableType);
			break;
		case EInventorySlotContentType::Collectible:
			HUDSlot.ItemName = SPInventoryText::GetCollectibleDisplayName(Slot.CollectibleSize);
			HUDSlot.Icon = Slot.CollectibleIcon;
			break;
		default:
			HUDSlot.ItemName = FText::GetEmpty();
			break;
		}
	}

	return Result;
}

TArray<FPerkHUDData> USPInventoryComponent::BuildPerkHUDData() const
{
	TArray<FPerkHUDData> Result;
	Result.SetNum(PerkSlotCount);

	for (int32 Index = 0; Index < PerkSlots.Num() && Index < PerkSlotCount; ++Index)
	{
		const FPerkSlotEntry& Slot = PerkSlots[Index];
		FPerkHUDData& HUDSlot = Result[Index];
		HUDSlot.bIsEquipped = Slot.IsEquipped();
		HUDSlot.PerkName = SPInventoryText::GetPerkDisplayName(Slot.PerkType);
	}

	return Result;
}

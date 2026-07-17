#include "Inventory/SPInventoryComponent.h"

#include "Engine/World.h"
#include "Gameplay/Collectibles/SPCollectibleItem.h"
#include "Gameplay/Items/SPConsumableItem.h"
#include "Gameplay/Items/SPPickupItem.h"
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

int32 USPInventoryComponent::FindConsumableSlotIndex(const EConsumableItemType ItemType) const
{
	if (ItemType == EConsumableItemType::None)
	{
		return INDEX_NONE;
	}

	for (int32 Index = 0; Index < InventorySlots.Num(); ++Index)
	{
		const FInventorySlotEntry& Slot = InventorySlots[Index];
		if (Slot.ContentType == EInventorySlotContentType::Consumable && Slot.ConsumableType == ItemType)
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

bool USPInventoryComponent::CanStoreWorldItem(const ASPPickupItem* Item) const
{
	if (!IsValid(Item))
	{
		return false;
	}

	if (const ASPConsumableItem* Consumable = Cast<ASPConsumableItem>(Item))
	{
		return Consumable->GetConsumableType() != EConsumableItemType::None
			&& (FindConsumableSlotIndex(Consumable->GetConsumableType()) != INDEX_NONE || HasFreeSlot());
	}

	return HasFreeSlot();
}

bool USPInventoryComponent::HasAnyCollectible() const
{
	return FindCollectibleSlotIndex() != INDEX_NONE;
}

bool USPInventoryComponent::IsSlotOccupied(const int32 Index) const
{
	return InventorySlots.IsValidIndex(Index) && InventorySlots[Index].IsOccupied();
}

bool USPInventoryComponent::IsSlotCollectible(const int32 Index) const
{
	return InventorySlots.IsValidIndex(Index)
		&& InventorySlots[Index].ContentType == EInventorySlotContentType::Collectible;
}

bool USPInventoryComponent::DropSlot(const int32 Index, ASPPickupItem*& OutSourceItem)
{
	OutSourceItem = nullptr;

	if (!GetOwner() || !GetOwner()->HasAuthority() || !InventorySlots.IsValidIndex(Index))
	{
		return false;
	}

	FInventorySlotEntry& Slot = InventorySlots[Index];
	if (!Slot.IsOccupied() || !IsValid(Slot.SourceItem))
	{
		return false;
	}

	if (Slot.ContentType == EInventorySlotContentType::Consumable && Slot.Quantity > 1)
	{
		OutSourceItem = SpawnStackedItem(Slot);
		if (!OutSourceItem)
		{
			return false;
		}

		--Slot.Quantity;
		OutSourceItem->SetOwner(nullptr);
		BroadcastInventoryChanged();
		return true;
	}

	OutSourceItem = Slot.SourceItem;
	OutSourceItem->SetOwner(nullptr);
	Slot.Clear();
	BroadcastInventoryChanged();
	return true;
}

bool USPInventoryComponent::DeliverSlot(const int32 Index, int32& OutValue, ASPCollectibleItem*& OutSourceItem)
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
	OutSourceItem = Cast<ASPCollectibleItem>(Slot.SourceItem);
	Slot.Clear();
	BroadcastInventoryChanged();
	return true;
}

ASPConsumableItem* USPInventoryComponent::SpawnConsumableItem(const EConsumableItemType ItemType) const
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !GetWorld())
	{
		return nullptr;
	}

	UClass* ItemClass = nullptr;
	switch (ItemType)
	{
	case EConsumableItemType::Medkit:
		ItemClass = MedkitItemClass.Get();
		break;
	case EConsumableItemType::SpeedPotion:
		ItemClass = SpeedPotionItemClass.Get();
		break;
	default:
		return nullptr;
	}

	if (!ItemClass)
	{
		UE_LOG(LogTemp, Error, TEXT("Inventory item class is not configured for consumable type %d."), static_cast<int32>(ItemType));
		return nullptr;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = GetOwner();
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return GetWorld()->SpawnActor<ASPConsumableItem>(
		ItemClass, GetOwner()->GetActorTransform(), SpawnParameters);
}

ASPPickupItem* USPInventoryComponent::SpawnStackedItem(const FInventorySlotEntry& Slot) const
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !GetWorld() || !IsValid(Slot.SourceItem))
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return GetWorld()->SpawnActor<ASPPickupItem>(
		Slot.SourceItem->GetClass(), GetOwner()->GetActorTransform(), SpawnParameters);
}

bool USPInventoryComponent::TryAddConsumable(const EConsumableItemType ItemType)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || ItemType == EConsumableItemType::None
		|| (FindConsumableSlotIndex(ItemType) == INDEX_NONE && !HasFreeSlot()))
	{
		return false;
	}

	ASPConsumableItem* Item = SpawnConsumableItem(ItemType);
	if (!Item)
	{
		return false;
	}

	if (!TryStoreWorldItem(Item))
	{
		Item->Destroy();
		return false;
	}

	return true;
}

bool USPInventoryComponent::TryStoreWorldItem(ASPPickupItem* Item)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !IsValid(Item))
	{
		return false;
	}

	if (const ASPConsumableItem* Consumable = Cast<ASPConsumableItem>(Item))
	{
		const EConsumableItemType ItemType = Consumable->GetConsumableType();
		const int32 ExistingIndex = FindConsumableSlotIndex(ItemType);
		if (ExistingIndex != INDEX_NONE)
		{
			FInventorySlotEntry& ExistingSlot = InventorySlots[ExistingIndex];
			if (ExistingSlot.SourceItem == Item)
			{
				return false;
			}

			++ExistingSlot.Quantity;
			Item->Destroy();
			BroadcastInventoryChanged();
			return true;
		}
	}

	const int32 EmptyIndex = FindFirstEmptySlotIndex();
	if (EmptyIndex == INDEX_NONE)
	{
		return false;
	}

	FInventorySlotEntry& Slot = InventorySlots[EmptyIndex];
	Slot.Clear();
	Slot.SourceItem = Item;
	Slot.Icon = Item->GetIcon();
	Slot.Quantity = 1;

	if (const ASPConsumableItem* Consumable = Cast<ASPConsumableItem>(Item))
	{
		if (Consumable->GetConsumableType() == EConsumableItemType::None)
		{
			Slot.Clear();
			return false;
		}

		Slot.ContentType = EInventorySlotContentType::Consumable;
		Slot.ConsumableType = Consumable->GetConsumableType();
	}
	else if (const ASPCollectibleItem* Collectible = Cast<ASPCollectibleItem>(Item))
	{
		Slot.ContentType = EInventorySlotContentType::Collectible;
		Slot.CollectibleSize = Collectible->GetCollectibleSize();
		Slot.CollectibleValue = Collectible->GetValue();
	}
	else
	{
		Slot.Clear();
		return false;
	}

	Item->SetOwner(GetOwner());
	Item->SetStored(true, FVector::ZeroVector);
	BroadcastInventoryChanged();
	return true;
}

bool USPInventoryComponent::ConsumeConsumableAtSlot(const int32 Index, const EConsumableItemType ExpectedType)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !IsSlotConsumable(Index, ExpectedType))
	{
		return false;
	}

	FInventorySlotEntry& Slot = InventorySlots[Index];
	if (Slot.Quantity > 1)
	{
		--Slot.Quantity;
		BroadcastInventoryChanged();
		return true;
	}

	ASPPickupItem* SourceItem = Slot.SourceItem;
	Slot.Clear();
	BroadcastInventoryChanged();

	if (IsValid(SourceItem))
	{
		SourceItem->Destroy();
	}
	return true;
}

bool USPInventoryComponent::RemoveConsumable(const EConsumableItemType ItemType)
{
	if (ItemType == EConsumableItemType::None)
	{
		return false;
	}

	for (int32 Index = 0; Index < InventorySlots.Num(); ++Index)
	{
		if (IsSlotConsumable(Index, ItemType))
		{
			return ConsumeConsumableAtSlot(Index, ItemType);
		}
	}
	return false;
}

bool USPInventoryComponent::IsSlotConsumable(const int32 Index, const EConsumableItemType ItemType) const
{
	return ItemType != EConsumableItemType::None
		&& InventorySlots.IsValidIndex(Index)
		&& InventorySlots[Index].ContentType == EInventorySlotContentType::Consumable
		&& InventorySlots[Index].ConsumableType == ItemType;
}

EConsumableItemType USPInventoryComponent::GetConsumableTypeAtSlot(const int32 Index) const
{
	return InventorySlots.IsValidIndex(Index)
		&& InventorySlots[Index].ContentType == EInventorySlotContentType::Consumable
		? InventorySlots[Index].ConsumableType
		: EConsumableItemType::None;
}

int32 USPInventoryComponent::CountConsumable(const EConsumableItemType ItemType) const
{
	int32 Count = 0;
	for (const FInventorySlotEntry& Slot : InventorySlots)
	{
		if (Slot.ContentType == EInventorySlotContentType::Consumable && Slot.ConsumableType == ItemType)
		{
			Count += Slot.Quantity;
		}
	}
	return Count;
}

bool USPInventoryComponent::HasPerk(const EPerkType PerkType) const
{
	return PerkType != EPerkType::None
		&& PerkSlots.ContainsByPredicate([PerkType](const FPerkSlotEntry& Slot)
		{
			return Slot.PerkType == PerkType;
		});
}

void USPInventoryComponent::SetCollectibleFromItem(ASPCollectibleItem* Item)
{
	TryStoreWorldItem(Item);
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
	if (!GetOwner() || !GetOwner()->HasAuthority() || PerkType == EPerkType::None || HasPerk(PerkType))
	{
		return false;
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

void USPInventoryComponent::InitializeLoadout(
	const EConsumableItemType LoadoutItem,
	const TArray<EPerkType>& LoadoutPerks)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || bLoadoutInitialized)
	{
		return;
	}

	bLoadoutInitialized = true;
	const EConsumableItemType ItemToGrant = LoadoutItem != EConsumableItemType::None
		? LoadoutItem
		: DefaultLoadoutItem;
	if (ItemToGrant != EConsumableItemType::None)
	{
		TryAddConsumable(ItemToGrant);
	}

	const TArray<EPerkType>& PerksToGrant = LoadoutPerks.IsEmpty() ? DefaultPerks : LoadoutPerks;
	for (const EPerkType PerkType : PerksToGrant)
	{
		EquipPerk(PerkType);
	}
}

void USPInventoryComponent::InitializeDefaultLoadout()
{
	InitializeLoadout(DefaultLoadoutItem, DefaultPerks);
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
		HUDSlot.Icon = Slot.Icon;

		switch (Slot.ContentType)
		{
		case EInventorySlotContentType::Consumable:
			HUDSlot.ItemName = SPInventoryText::GetConsumableDisplayName(Slot.ConsumableType);
			HUDSlot.Quantity = Slot.Quantity;
			break;
		case EInventorySlotContentType::Collectible:
			HUDSlot.ItemName = SPInventoryText::GetCollectibleDisplayName(Slot.CollectibleSize);
			HUDSlot.Quantity = 1;
			break;
		default:
			HUDSlot.ItemName = FText::GetEmpty();
			HUDSlot.Quantity = 0;
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

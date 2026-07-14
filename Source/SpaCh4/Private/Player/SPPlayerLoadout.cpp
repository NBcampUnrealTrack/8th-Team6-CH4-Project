#include "Player/SPPlayerLoadout.h"

#include "Algo/AllOf.h"

namespace
{
	template <typename TEnum>
	bool HasUniqueEntries(const TArray<TEnum>& Entries)
	{
		TSet<TEnum> UniqueEntries;
		for (const TEnum Entry : Entries)
		{
			if (UniqueEntries.Contains(Entry))
			{
				return false;
			}
			UniqueEntries.Add(Entry);
		}
		return true;
	}
}

bool SPPlayerLoadout::IsValidSurvivorItem(const EConsumableItemType ItemType)
{
	return ItemType == EConsumableItemType::Medkit || ItemType == EConsumableItemType::SpeedBoost;
}

bool SPPlayerLoadout::IsValidSurvivorPerk(const EPerkType PerkType)
{
	switch (PerkType)
	{
	case EPerkType::FieldMedic:
	case EPerkType::LightWheels:
	case EPerkType::SilentRoll:
	case EPerkType::StageTwoRescue:
	case EPerkType::ThreatSensor:
	case EPerkType::DeliveryPro:
		return true;
	default:
		return false;
	}
}

bool SPPlayerLoadout::IsValidKillerPerk(const EKillerPerkType PerkType)
{
	switch (PerkType)
	{
	case EKillerPerkType::Overvoltage:
	case EKillerPerkType::Conduction_Coil:
	case EKillerPerkType::Circuit_Trace:
	case EKillerPerkType::Blackout_Zone:
		return true;
	default:
		return false;
	}
}

bool SPPlayerLoadout::IsSurvivorLoadoutComplete(const FSPPlayerLoadout& Loadout)
{
	if (!IsValidSurvivorItem(Loadout.SurvivorItem)
		|| Loadout.SurvivorPerks.Num() != SurvivorPerkCount
		|| !HasUniqueEntries(Loadout.SurvivorPerks))
	{
		return false;
	}

	return Algo::AllOf(Loadout.SurvivorPerks, [](const EPerkType PerkType)
	{
		return IsValidSurvivorPerk(PerkType);
	});
}

bool SPPlayerLoadout::IsKillerLoadoutComplete(const FSPPlayerLoadout& Loadout)
{
	if (Loadout.KillerPerks.Num() != KillerPerkCount || !HasUniqueEntries(Loadout.KillerPerks))
	{
		return false;
	}

	return Algo::AllOf(Loadout.KillerPerks, [](const EKillerPerkType PerkType)
	{
		return IsValidKillerPerk(PerkType);
	});
}

bool SPPlayerLoadout::IsComplete(const FSPPlayerLoadout& Loadout)
{
	return IsSurvivorLoadoutComplete(Loadout) && IsKillerLoadoutComplete(Loadout);
}

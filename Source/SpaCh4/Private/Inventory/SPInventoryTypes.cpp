#include "Inventory/SPInventoryTypes.h"

namespace SPInventoryText
{
	FText GetConsumableDisplayName(const EConsumableItemType Type)
	{
		switch (Type)
		{
		case EConsumableItemType::Medkit:
			return NSLOCTEXT("SpaCh4", "Inventory_Medkit", "Medkit");
		case EConsumableItemType::SpeedPotion:
			return NSLOCTEXT("SpaCh4", "Inventory_SpeedPotion", "Speed Potion");
		default:
			return FText::GetEmpty();
		}
	}

	FText GetCollectibleDisplayName(const ECollectibleSize Size)
	{
		switch (Size)
		{
		case ECollectibleSize::Small:
			return NSLOCTEXT("SpaCh4", "Inventory_Collectible_Small", "Collectible (S)");
		case ECollectibleSize::Medium:
			return NSLOCTEXT("SpaCh4", "Inventory_Collectible_Medium", "Collectible (M)");
		case ECollectibleSize::Large:
			return NSLOCTEXT("SpaCh4", "Inventory_Collectible_Large", "Collectible (L)");
		case ECollectibleSize::Hazardous:
			return NSLOCTEXT("SpaCh4", "Inventory_Collectible_Hazardous", "Collectible (H)");
		default:
			return NSLOCTEXT("SpaCh4", "Inventory_Collectible", "Collectible");
		}
	}

	FText GetPerkDisplayName(const EPerkType Type)
	{
		switch (Type)
		{
		case EPerkType::FieldMedic:
			return NSLOCTEXT("SpaCh4", "Perk_FieldMedic", "Field Medic");
		case EPerkType::LightWheels:
			return NSLOCTEXT("SpaCh4", "Perk_LightWheels", "Light Wheels");
		case EPerkType::SilentRoll:
			return NSLOCTEXT("SpaCh4", "Perk_SilentRoll", "Silent Roll");
		case EPerkType::StageTwoRescue:
			return NSLOCTEXT("SpaCh4", "Perk_StageTwoRescue", "Stage-Two Rescue");
		case EPerkType::ThreatSensor:
			return NSLOCTEXT("SpaCh4", "Perk_ThreatSensor", "Threat Sensor");
		default:
			return FText::GetEmpty();
		}
	}
}

#include "Systems/Data/BalanceData.h"

namespace BalanceDataStationIds
{
	const FName StationA(TEXT("A"));
	const FName StationB(TEXT("B"));
}

UBalanceData::UBalanceData()
{
}

int32 UBalanceData::GetCollectibleValue(ECollectibleType Type) const
{
	switch (Type)
	{
	case ECollectibleType::Small:
		return SmallCollectibleValue;
	case ECollectibleType::Medium:
		return MediumCollectibleValue;
	case ECollectibleType::Large:
		return LargeCollectibleValue;
	case ECollectibleType::Dangerous:
		return DangerousCollectibleValue;
	default:
		return 0;
	}
}

int32 UBalanceData::GetStationTargetValue(FName StationId) const
{
	if (StationId == BalanceDataStationIds::StationA)
	{
		return DeliveryStationATargetValue;
	}

	if (StationId == BalanceDataStationIds::StationB)
	{
		return DeliveryStationBTargetValue;
	}

	return 0;
}

int32 UBalanceData::GetTotalRequiredValue() const
{
	if (TotalRequiredValue > 0)
	{
		return TotalRequiredValue;
	}

	return DeliveryStationATargetValue + DeliveryStationBTargetValue;
}

int32 UBalanceData::GetHatchRequiredDeliveredValue() const
{
	return FMath::CeilToInt(static_cast<float>(GetTotalRequiredValue()) * 0.5f);
}

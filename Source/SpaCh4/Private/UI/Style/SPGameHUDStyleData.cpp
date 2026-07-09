#include "UI/Style/SPUIStyleData.h"

#include "UI/HUDTypes.h"

UTexture2D* USPGameHUDStyleData::GetDeliveryProgressSegmentTexture(int32 FilledSegments) const
{
	const int32 Clamped = FMath::Clamp(FilledSegments, 0, SpaCh4HUD::DeliveryProgressSegmentCount);
	if (DeliveryProgressSegmentTextures.IsValidIndex(Clamped) && DeliveryProgressSegmentTextures[Clamped])
	{
		return DeliveryProgressSegmentTextures[Clamped];
	}

	if (DeliveryProgressEmptyFallback)
	{
		return DeliveryProgressEmptyFallback;
	}

	return DeliveryProgressBackground;
}

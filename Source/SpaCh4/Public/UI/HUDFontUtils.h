#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"

class UFontFace;
class UTextBlock;
class USPUIFontStyleData;

namespace SpaCh4HUD
{
	UFontFace* LoadFontFace(const USPUIFontStyleData* StyleOverride, bool bMediumWeight = false);
	void ApplyTextFont(UTextBlock* Text, UFontFace* FontFace, int32 Size, const USPUIFontStyleData* StyleOverride = nullptr);

	inline void SetBrushImageSize(FSlateBrush& Brush, const FVector2D& Size)
	{
		Brush.ImageSize = Size;
		Brush.DrawAs = ESlateBrushDrawType::Image;
	}
}

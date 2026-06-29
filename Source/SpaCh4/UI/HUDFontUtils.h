#pragma once

#include "CoreMinimal.h"

class UFontFace;
class UTextBlock;

namespace SpaCh4HUD
{
	/** Rajdhani — condensed sci-fi HUD typeface (SIL OFL) */
	inline static const TCHAR* FontSemiBoldPath =
		TEXT("/Game/UI/HUD/Fonts/Rajdhani-SemiBold.Rajdhani-SemiBold");
	inline static const TCHAR* FontMediumPath =
		TEXT("/Game/UI/HUD/Fonts/Rajdhani-Medium.Rajdhani-Medium");

	UFontFace* LoadFontFace(const TCHAR* AssetPath);
	void ApplyTextFont(UTextBlock* Text, UFontFace* FontFace, int32 Size);
}

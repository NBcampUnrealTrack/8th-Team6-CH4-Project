#include "UI/HUDFontUtils.h"

#include "Components/TextBlock.h"
#include "Engine/Font.h"
#include "Engine/FontFace.h"
#include "Fonts/SlateFontInfo.h"

UFontFace* SpaCh4HUD::LoadFontFace(const TCHAR* AssetPath)
{
	return LoadObject<UFontFace>(nullptr, AssetPath);
}

void SpaCh4HUD::ApplyTextFont(UTextBlock* Text, UFontFace* FontFace, int32 Size)
{
	if (!Text || Size <= 0)
	{
		return;
	}

	FSlateFontInfo FontInfo;
	if (FontFace)
	{
		FontInfo = FSlateFontInfo(FontFace, static_cast<float>(Size));
	}
	else if (UFont* FallbackFont = LoadObject<UFont>(nullptr, TEXT("/Engine/EngineFonts/Roboto.Roboto")))
	{
		FontInfo = FSlateFontInfo(FallbackFont, static_cast<float>(Size));
	}
	else
	{
		return;
	}

	Text->SetFont(FontInfo);
}

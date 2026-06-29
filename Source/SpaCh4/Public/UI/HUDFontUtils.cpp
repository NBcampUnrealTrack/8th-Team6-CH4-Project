#include "UI/HUDFontUtils.h"

#include "Components/TextBlock.h"
#include "Engine/FontFace.h"

UFontFace* SpaCh4HUD::LoadFontFace(const TCHAR* AssetPath)
{
	return LoadObject<UFontFace>(nullptr, AssetPath);
}

void SpaCh4HUD::ApplyTextFont(UTextBlock* Text, UFontFace* FontFace, int32 Size)
{
	if (!Text || !FontFace || Size <= 0)
	{
		return;
	}

	FSlateFontInfo FontInfo;
	FontInfo.FontObject = FontFace;
	FontInfo.Size = static_cast<float>(Size);
	Text->SetFont(FontInfo);
}

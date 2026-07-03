#include "UI/HUDFontUtils.h"

#include "Components/TextBlock.h"
#include "Engine/Font.h"
#include "Engine/FontFace.h"
#include "Fonts/CompositeFont.h"
#include "Fonts/SlateFontInfo.h"
#include "UObject/Package.h"

namespace
{
	UFont* GetOrCreateCompositeFont(UFontFace* FontFace)
	{
		if (!FontFace)
		{
			return nullptr;
		}

		static TMap<FName, TWeakObjectPtr<UFont>> CachedFonts;
		const FName CacheKey(*FontFace->GetPathName());
		if (const TWeakObjectPtr<UFont>* Existing = CachedFonts.Find(CacheKey))
		{
			if (Existing->IsValid())
			{
				return Existing->Get();
			}
		}

		UFont* CompositeFont = NewObject<UFont>(GetTransientPackage(), NAME_None, RF_Transient | RF_Public);
		CompositeFont->FontCacheType = EFontCacheType::Runtime;

		FTypefaceEntry& TypefaceEntry = CompositeFont->CompositeFont.DefaultTypeface.Fonts.AddDefaulted_GetRef();
		TypefaceEntry.Name = FName(TEXT("Default"));
		TypefaceEntry.Font = FFontData(FontFace);

		CachedFonts.Add(CacheKey, CompositeFont);
		return CompositeFont;
	}

	UFont* LoadRobotoFont()
	{
		return LoadObject<UFont>(nullptr, TEXT("/Engine/EngineFonts/Roboto.Roboto"));
	}
}

UFontFace* SpaCh4HUD::LoadFontFace(const TCHAR* AssetPath)
{
	UFontFace* FontFace = LoadObject<UFontFace>(nullptr, AssetPath);
	if (FontFace)
	{
		FontFace->ConditionalPostLoad();
	}
	return FontFace;
}

void SpaCh4HUD::ApplyTextFont(UTextBlock* Text, UFontFace* FontFace, int32 Size)
{
	if (!Text || Size <= 0)
	{
		return;
	}

	const float FontSize = static_cast<float>(Size);
	UFontFace* ResolvedFace = FontFace ? FontFace : LoadFontFace(FontSemiBoldPath);

	if (UFont* CompositeFont = GetOrCreateCompositeFont(ResolvedFace))
	{
		Text->SetFont(FSlateFontInfo(CompositeFont, FontSize));
		return;
	}

	if (UFont* RobotoFont = LoadRobotoFont())
	{
		Text->SetFont(FSlateFontInfo(RobotoFont, FontSize));
		return;
	}

	FSlateFontInfo FontInfo = Text->GetFont();
	FontInfo.Size = FontSize;
	Text->SetFont(FontInfo);
}

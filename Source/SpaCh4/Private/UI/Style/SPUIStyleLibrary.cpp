#include "UI/Style/SPUIStyleLibrary.h"

#include "Engine/FontFace.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInterface.h"
#include "UI/HUDTypes.h"
#include "UI/Style/SPUIStyleData.h"

namespace SPUIStyleLibrary
{
	namespace Paths
	{
		static const FSoftObjectPath GameHUDStyleAsset(
			TEXT("/Game/UI/Data/DA_GameHUDStyle.DA_GameHUDStyle"));
		static const FSoftObjectPath MainMenuStyleAsset(
			TEXT("/Game/UI/Data/DA_MainMenuStyle.DA_MainMenuStyle"));
		static const FSoftObjectPath FontStyleAsset(
			TEXT("/Game/UI/Data/DA_UIFontStyle.DA_UIFontStyle"));

		static const FSoftObjectPath DeliveryStationFrameA(
			TEXT("/Game/UI/HUD/Textures/Delivery/T_HUD_Delivery_Station_A.T_HUD_Delivery_Station_A"));
		static const FSoftObjectPath DeliveryStationFrameB(
			TEXT("/Game/UI/HUD/Textures/Delivery/T_HUD_Delivery_Station_B.T_HUD_Delivery_Station_B"));
		static const FSoftObjectPath DeliveryProgressFillMI(
			TEXT("/Game/UI/HUD/Materials/MI_HUD_DeliveryProgressFill.MI_HUD_DeliveryProgressFill"));
		static const FSoftObjectPath DeliveryProgressFillMaterial(
			TEXT("/Game/UI/HUD/Materials/M_HUD_HealthBarFill.M_HUD_HealthBarFill"));
		static const FSoftObjectPath DeliveryProgressFillTexture(
			TEXT("/Game/UI/HUD/Textures/Delivery/T_HUD_Bar_Fill_Delivery.T_HUD_Bar_Fill_Delivery"));
		static const FSoftObjectPath DeliveryProgressFillTextureFallback(
			TEXT("/Game/UI/HUD/Textures/Common/T_HUD_Bar_Fill.T_HUD_Bar_Fill"));
		static const FSoftObjectPath DeliveryProgressEmpty(
			TEXT("/Game/UI/HUD/Textures/Delivery/T_HUD_Delivery_Progress_00.T_HUD_Delivery_Progress_00"));
		static const FSoftObjectPath DeliveryProgressBackground(
			TEXT("/Game/UI/HUD/Textures/Delivery/T_HUD_Delivery_Progress_BG.T_HUD_Delivery_Progress_BG"));
		static const FSoftObjectPath DeliveryStackEmpty(
			TEXT("/Game/UI/HUD/Textures/Delivery/T_HUD_Delivery_Stack_Empty.T_HUD_Delivery_Stack_Empty"));
		static const FSoftObjectPath DeliveryStackFilled(
			TEXT("/Game/UI/HUD/Textures/Delivery/T_HUD_Delivery_Stack_Filled.T_HUD_Delivery_Stack_Filled"));

		static const FSoftObjectPath PortraitSlotFrame(
			TEXT("/Game/UI/HUD/Textures/Teammate/T_HUD_Portrait_Slot.T_HUD_Portrait_Slot"));
		static const FSoftObjectPath PortraitHealthy(
			TEXT("/Game/UI/HUD/Textures/Teammate/T_HUD_Portrait_Placeholder.T_HUD_Portrait_Placeholder"));
		static const FSoftObjectPath PortraitInjured(
			TEXT("/Game/UI/HUD/Textures/Teammate/T_HUD_Portrait_Injured.T_HUD_Portrait_Injured"));
		static const FSoftObjectPath PortraitDead(
			TEXT("/Game/UI/HUD/Textures/Teammate/T_HUD_Portrait_Dead.T_HUD_Portrait_Dead"));
		static const FSoftObjectPath DownedHealthBarBackground(
			TEXT("/Game/UI/HUD/Textures/Common/T_HUD_Bar_BG.T_HUD_Bar_BG"));
		static const FSoftObjectPath DownedHealthFillMI(
			TEXT("/Game/UI/HUD/Materials/MI_HUD_HealthBarFill_Downed.MI_HUD_HealthBarFill_Downed"));
		static const FSoftObjectPath DownedHealthFillMaterial(
			TEXT("/Game/UI/HUD/Materials/M_HUD_HealthBarFill.M_HUD_HealthBarFill"));
		static const FSoftObjectPath DownedHealthFillTexture(
			TEXT("/Game/UI/HUD/Textures/Teammate/T_HUD_Bar_Fill_Downed.T_HUD_Bar_Fill_Downed"));

		static const FSoftObjectPath MainMenuTitle(
			TEXT("/Game/UI/MainMenu/Textures/T_MainMenu_Title.T_MainMenu_Title"));
		static const FSoftObjectPath SurvivorButtonNormal(
			TEXT("/Game/UI/MainMenu/Textures/T_MainMenu_Btn_SURVIVOR.T_MainMenu_Btn_SURVIVOR"));
		static const FSoftObjectPath SurvivorButtonHovered(
			TEXT("/Game/UI/MainMenu/Textures/T_MainMenu_Btn_SURVIVOR_Hover.T_MainMenu_Btn_SURVIVOR_Hover"));
		static const FSoftObjectPath KillerButtonNormal(
			TEXT("/Game/UI/MainMenu/Textures/T_MainMenu_Btn_KILLER.T_MainMenu_Btn_KILLER"));
		static const FSoftObjectPath KillerButtonHovered(
			TEXT("/Game/UI/MainMenu/Textures/T_MainMenu_Btn_KILLER_Hover.T_MainMenu_Btn_KILLER_Hover"));
		static const FSoftObjectPath SettingsButtonNormal(
			TEXT("/Game/UI/MainMenu/Textures/T_MainMenu_Btn_SETTINGS.T_MainMenu_Btn_SETTINGS"));
		static const FSoftObjectPath SettingsButtonHovered(
			TEXT("/Game/UI/MainMenu/Textures/T_MainMenu_Btn_SETTINGS_Hover.T_MainMenu_Btn_SETTINGS_Hover"));
		static const FSoftObjectPath QuitButtonNormal(
			TEXT("/Game/UI/MainMenu/Textures/T_MainMenu_Btn_QUIT.T_MainMenu_Btn_QUIT"));
		static const FSoftObjectPath QuitButtonHovered(
			TEXT("/Game/UI/MainMenu/Textures/T_MainMenu_Btn_QUIT_Hover.T_MainMenu_Btn_QUIT_Hover"));

		static const FSoftObjectPath FontSemiBold(
			TEXT("/Game/UI/HUD/Fonts/Rajdhani-SemiBold.Rajdhani-SemiBold"));
		static const FSoftObjectPath FontMedium(
			TEXT("/Game/UI/HUD/Fonts/Rajdhani-Medium.Rajdhani-Medium"));

		static FSoftObjectPath DeliveryProgressSegment(int32 SegmentIndex)
		{
			return FSoftObjectPath(FString::Printf(
				TEXT("/Game/UI/HUD/Textures/Delivery/T_HUD_Delivery_Progress_%02d.T_HUD_Delivery_Progress_%02d"),
				SegmentIndex,
				SegmentIndex));
		}
	}

	template<typename TObject>
	TObject* LoadAsset(const FSoftObjectPath& Path)
	{
		if (!Path.IsValid())
		{
			return nullptr;
		}

		return Cast<TObject>(Path.TryLoad());
	}

	template<typename TData>
	TData* LoadStyleAsset(const FSoftObjectPath& Path, TData*& BuiltInFallback)
	{
		if (TData* Authored = LoadAsset<TData>(Path))
		{
			return Authored;
		}

		if (!BuiltInFallback)
		{
			BuiltInFallback = NewObject<TData>(GetTransientPackage(), NAME_None, RF_Transient | RF_Public);
		}

		return BuiltInFallback;
	}

	static void EnsureBuiltInGameHUDStyle(USPGameHUDStyleData* Style)
	{
		if (!Style)
		{
			return;
		}

		if (!Style->DeliveryStationFrameA)
		{
			Style->DeliveryStationFrameA = LoadAsset<UTexture2D>(Paths::DeliveryStationFrameA);
		}
		if (!Style->DeliveryStationFrameB)
		{
			Style->DeliveryStationFrameB = LoadAsset<UTexture2D>(Paths::DeliveryStationFrameB);
		}
		if (!Style->DeliveryProgressFillMaterialInstance)
		{
			Style->DeliveryProgressFillMaterialInstance = LoadAsset<UMaterialInterface>(Paths::DeliveryProgressFillMI);
		}
		if (!Style->DeliveryProgressFillMaterial)
		{
			Style->DeliveryProgressFillMaterial = LoadAsset<UMaterialInterface>(Paths::DeliveryProgressFillMaterial);
		}
		if (!Style->DeliveryProgressFillTexture)
		{
			Style->DeliveryProgressFillTexture = LoadAsset<UTexture2D>(Paths::DeliveryProgressFillTexture);
		}
		if (!Style->DeliveryProgressFillTextureFallback)
		{
			Style->DeliveryProgressFillTextureFallback = LoadAsset<UTexture2D>(Paths::DeliveryProgressFillTextureFallback);
		}
		if (!Style->DeliveryProgressEmptyFallback)
		{
			Style->DeliveryProgressEmptyFallback = LoadAsset<UTexture2D>(Paths::DeliveryProgressEmpty);
		}
		if (!Style->DeliveryProgressBackground)
		{
			Style->DeliveryProgressBackground = LoadAsset<UTexture2D>(Paths::DeliveryProgressBackground);
		}
		if (!Style->DeliveryStackEmpty)
		{
			Style->DeliveryStackEmpty = LoadAsset<UTexture2D>(Paths::DeliveryStackEmpty);
		}
		if (!Style->DeliveryStackFilled)
		{
			Style->DeliveryStackFilled = LoadAsset<UTexture2D>(Paths::DeliveryStackFilled);
		}
		if (Style->DeliveryProgressSegmentTextures.Num() == 0)
		{
			Style->DeliveryProgressSegmentTextures.SetNum(SpaCh4HUD::DeliveryProgressSegmentCount + 1);
			for (int32 Index = 0; Index <= SpaCh4HUD::DeliveryProgressSegmentCount; ++Index)
			{
				Style->DeliveryProgressSegmentTextures[Index] = LoadAsset<UTexture2D>(Paths::DeliveryProgressSegment(Index));
			}
		}
		if (!Style->PortraitSlotFrame)
		{
			Style->PortraitSlotFrame = LoadAsset<UTexture2D>(Paths::PortraitSlotFrame);
		}
		if (!Style->PortraitHealthy)
		{
			Style->PortraitHealthy = LoadAsset<UTexture2D>(Paths::PortraitHealthy);
		}
		if (!Style->PortraitInjured)
		{
			Style->PortraitInjured = LoadAsset<UTexture2D>(Paths::PortraitInjured);
		}
		if (!Style->PortraitDead)
		{
			Style->PortraitDead = LoadAsset<UTexture2D>(Paths::PortraitDead);
		}
		if (!Style->DownedHealthBarBackground)
		{
			Style->DownedHealthBarBackground = LoadAsset<UTexture2D>(Paths::DownedHealthBarBackground);
		}
		if (!Style->DownedHealthFillMaterialInstance)
		{
			Style->DownedHealthFillMaterialInstance = LoadAsset<UMaterialInterface>(Paths::DownedHealthFillMI);
		}
		if (!Style->DownedHealthFillMaterial)
		{
			Style->DownedHealthFillMaterial = LoadAsset<UMaterialInterface>(Paths::DownedHealthFillMaterial);
		}
		if (!Style->DownedHealthFillTexture)
		{
			Style->DownedHealthFillTexture = LoadAsset<UTexture2D>(Paths::DownedHealthFillTexture);
		}
	}

	static void EnsureBuiltInMainMenuStyle(USPMainMenuStyleData* Style)
	{
		if (!Style)
		{
			return;
		}

		if (!Style->TitleTexture)
		{
			Style->TitleTexture = LoadAsset<UTexture2D>(Paths::MainMenuTitle);
		}
		if (!Style->SurvivorButtonNormal)
		{
			Style->SurvivorButtonNormal = LoadAsset<UTexture2D>(Paths::SurvivorButtonNormal);
		}
		if (!Style->SurvivorButtonHovered)
		{
			Style->SurvivorButtonHovered = LoadAsset<UTexture2D>(Paths::SurvivorButtonHovered);
		}
		if (!Style->KillerButtonNormal)
		{
			Style->KillerButtonNormal = LoadAsset<UTexture2D>(Paths::KillerButtonNormal);
		}
		if (!Style->KillerButtonHovered)
		{
			Style->KillerButtonHovered = LoadAsset<UTexture2D>(Paths::KillerButtonHovered);
		}
		if (!Style->SettingsButtonNormal)
		{
			Style->SettingsButtonNormal = LoadAsset<UTexture2D>(Paths::SettingsButtonNormal);
		}
		if (!Style->SettingsButtonHovered)
		{
			Style->SettingsButtonHovered = LoadAsset<UTexture2D>(Paths::SettingsButtonHovered);
		}
		if (!Style->QuitButtonNormal)
		{
			Style->QuitButtonNormal = LoadAsset<UTexture2D>(Paths::QuitButtonNormal);
		}
		if (!Style->QuitButtonHovered)
		{
			Style->QuitButtonHovered = LoadAsset<UTexture2D>(Paths::QuitButtonHovered);
		}
	}

	static void EnsureBuiltInFontStyle(USPUIFontStyleData* Style)
	{
		if (!Style)
		{
			return;
		}

		if (!Style->FontSemiBold)
		{
			Style->FontSemiBold = LoadAsset<UFontFace>(Paths::FontSemiBold);
			if (Style->FontSemiBold)
			{
				Style->FontSemiBold->ConditionalPostLoad();
			}
		}
		if (!Style->FontMedium)
		{
			Style->FontMedium = LoadAsset<UFontFace>(Paths::FontMedium);
			if (Style->FontMedium)
			{
				Style->FontMedium->ConditionalPostLoad();
			}
		}
	}

	const USPGameHUDStyleData& ResolveGameHUDStyle(const USPGameHUDStyleData* Override)
	{
		static USPGameHUDStyleData* BuiltInFallback = nullptr;
		USPGameHUDStyleData* Resolved = Override
			? const_cast<USPGameHUDStyleData*>(Override)
			: LoadStyleAsset<USPGameHUDStyleData>(Paths::GameHUDStyleAsset, BuiltInFallback);
		EnsureBuiltInGameHUDStyle(Resolved);
		return *Resolved;
	}

	const USPMainMenuStyleData& ResolveMainMenuStyle(const USPMainMenuStyleData* Override)
	{
		static USPMainMenuStyleData* BuiltInFallback = nullptr;
		USPMainMenuStyleData* Resolved = Override
			? const_cast<USPMainMenuStyleData*>(Override)
			: LoadStyleAsset<USPMainMenuStyleData>(Paths::MainMenuStyleAsset, BuiltInFallback);
		EnsureBuiltInMainMenuStyle(Resolved);
		return *Resolved;
	}

	const USPUIFontStyleData& ResolveFontStyle(const USPUIFontStyleData* Override)
	{
		static USPUIFontStyleData* BuiltInFallback = nullptr;
		USPUIFontStyleData* Resolved = Override
			? const_cast<USPUIFontStyleData*>(Override)
			: LoadStyleAsset<USPUIFontStyleData>(Paths::FontStyleAsset, BuiltInFallback);
		EnsureBuiltInFontStyle(Resolved);
		return *Resolved;
	}
}

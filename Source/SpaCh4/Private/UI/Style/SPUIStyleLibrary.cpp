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
	TData* EnsureRootedBuiltInFallback(TData*& BuiltInFallback)
	{
		if (!IsValid(BuiltInFallback))
		{
			BuiltInFallback = NewObject<TData>(GetTransientPackage(), NAME_None, RF_Transient | RF_Public);
			if (IsValid(BuiltInFallback))
			{
				// Transient package objects are GC'd between PIE sessions; root so the
				// static pointer cannot become a dangling UObject* on the 2nd Play.
				BuiltInFallback->AddToRoot();
			}
		}
		return BuiltInFallback;
	}

	template<typename TData>
	TData* LoadStyleAsset(const FSoftObjectPath& Path, TData*& BuiltInFallback)
	{
		// Missing style DAs stay missing for the editor session — without this cache the
		// 0.2s HUD refresh re-attempts a disk package load (hitch + log spam) every tick.
		// (Creating the DA mid-session requires an editor restart to be picked up.)
		static TSet<FName> FailedStylePaths;
		const FName PathName(*Path.ToString());

		if (!FailedStylePaths.Contains(PathName))
		{
			if (TData* Authored = LoadAsset<TData>(Path))
			{
				return Authored;
			}
			FailedStylePaths.Add(PathName);
		}

		return EnsureRootedBuiltInFallback(BuiltInFallback);
	}

	template<typename TObject>
	void EnsureSoftObjectOnce(TSoftObjectPtr<TObject>& Field, const FSoftObjectPath& Path)
	{
		// IsNull() only — after a failed load stash the path so we do not TryLoad every 0.2s.
		if (!Field.IsNull())
		{
			return;
		}

		if (TObject* Loaded = LoadAsset<TObject>(Path))
		{
			Field = Loaded;
		}
		else if (Path.IsValid())
		{
			Field = TSoftObjectPtr<TObject>(Path);
		}
	}

	template<typename TObject>
	void EnsureHardObjectOnce(TObjectPtr<TObject>& Field, const FSoftObjectPath& Path)
	{
		if (IsValid(Field))
		{
			return;
		}

		Field = LoadAsset<TObject>(Path);
	}

	static void EnsureBuiltInGameHUDStyle(USPGameHUDStyleData* Style)
	{
		if (!IsValid(Style))
		{
			return;
		}

		const bool bTransientBuiltIn = Style->GetOutermost() == GetTransientPackage();
		static TWeakObjectPtr<USPGameHUDStyleData> HydratedTransientBuiltIn;
		if (bTransientBuiltIn && HydratedTransientBuiltIn.Get() == Style)
		{
			// Missing assets stay missing — do not TryLoad every HUD refresh tick.
			return;
		}

		EnsureHardObjectOnce(Style->DeliveryStationFrameA, Paths::DeliveryStationFrameA);
		EnsureHardObjectOnce(Style->DeliveryStationFrameB, Paths::DeliveryStationFrameB);
		EnsureHardObjectOnce(Style->DeliveryProgressFillMaterialInstance, Paths::DeliveryProgressFillMI);
		EnsureHardObjectOnce(Style->DeliveryProgressFillMaterial, Paths::DeliveryProgressFillMaterial);
		EnsureHardObjectOnce(Style->DeliveryProgressFillTexture, Paths::DeliveryProgressFillTexture);
		EnsureHardObjectOnce(Style->DeliveryProgressFillTextureFallback, Paths::DeliveryProgressFillTextureFallback);
		EnsureHardObjectOnce(Style->DeliveryProgressEmptyFallback, Paths::DeliveryProgressEmpty);
		EnsureHardObjectOnce(Style->DeliveryProgressBackground, Paths::DeliveryProgressBackground);
		EnsureHardObjectOnce(Style->DeliveryStackEmpty, Paths::DeliveryStackEmpty);
		EnsureHardObjectOnce(Style->DeliveryStackFilled, Paths::DeliveryStackFilled);

		// Only rebuild segment arrays on the transient built-in fallback.
		// Mutating TArray UPROPERTYs on authored DA assets can AV after Live Coding ABI drift.
		if (bTransientBuiltIn && Style->DeliveryProgressSegmentTextures.Num() == 0)
		{
			TArray<TObjectPtr<UTexture2D>> Segments;
			Segments.Reserve(SpaCh4HUD::DeliveryProgressSegmentCount + 1);
			for (int32 Index = 0; Index <= SpaCh4HUD::DeliveryProgressSegmentCount; ++Index)
			{
				Segments.Add(LoadAsset<UTexture2D>(Paths::DeliveryProgressSegment(Index)));
			}
			Style->DeliveryProgressSegmentTextures = MoveTemp(Segments);
		}

		EnsureHardObjectOnce(Style->PortraitSlotFrame, Paths::PortraitSlotFrame);
		EnsureHardObjectOnce(Style->PortraitHealthy, Paths::PortraitHealthy);
		EnsureHardObjectOnce(Style->PortraitInjured, Paths::PortraitInjured);
		EnsureHardObjectOnce(Style->PortraitDead, Paths::PortraitDead);
		EnsureHardObjectOnce(Style->DownedHealthBarBackground, Paths::DownedHealthBarBackground);
		EnsureHardObjectOnce(Style->DownedHealthFillMaterialInstance, Paths::DownedHealthFillMI);
		EnsureHardObjectOnce(Style->DownedHealthFillMaterial, Paths::DownedHealthFillMaterial);
		EnsureHardObjectOnce(Style->DownedHealthFillTexture, Paths::DownedHealthFillTexture);

		if (bTransientBuiltIn)
		{
			HydratedTransientBuiltIn = Style;
		}
	}

	static void EnsureBuiltInMainMenuStyle(USPMainMenuStyleData* Style)
	{
		if (!IsValid(Style))
		{
			return;
		}

		if (!IsValid(Style->TitleTexture))
		{
			Style->TitleTexture = LoadAsset<UTexture2D>(Paths::MainMenuTitle);
		}
		if (!IsValid(Style->SurvivorButtonNormal))
		{
			Style->SurvivorButtonNormal = LoadAsset<UTexture2D>(Paths::SurvivorButtonNormal);
		}
		if (!IsValid(Style->SurvivorButtonHovered))
		{
			Style->SurvivorButtonHovered = LoadAsset<UTexture2D>(Paths::SurvivorButtonHovered);
		}
		if (!IsValid(Style->KillerButtonNormal))
		{
			Style->KillerButtonNormal = LoadAsset<UTexture2D>(Paths::KillerButtonNormal);
		}
		if (!IsValid(Style->KillerButtonHovered))
		{
			Style->KillerButtonHovered = LoadAsset<UTexture2D>(Paths::KillerButtonHovered);
		}
		if (!IsValid(Style->SettingsButtonNormal))
		{
			Style->SettingsButtonNormal = LoadAsset<UTexture2D>(Paths::SettingsButtonNormal);
		}
		if (!IsValid(Style->SettingsButtonHovered))
		{
			Style->SettingsButtonHovered = LoadAsset<UTexture2D>(Paths::SettingsButtonHovered);
		}
		if (!IsValid(Style->QuitButtonNormal))
		{
			Style->QuitButtonNormal = LoadAsset<UTexture2D>(Paths::QuitButtonNormal);
		}
		if (!IsValid(Style->QuitButtonHovered))
		{
			Style->QuitButtonHovered = LoadAsset<UTexture2D>(Paths::QuitButtonHovered);
		}
	}

	static void EnsureBuiltInFontStyle(USPUIFontStyleData* Style)
	{
		if (!IsValid(Style))
		{
			return;
		}

		if (!IsValid(Style->FontSemiBold))
		{
			Style->FontSemiBold = LoadAsset<UFontFace>(Paths::FontSemiBold);
			if (IsValid(Style->FontSemiBold))
			{
				Style->FontSemiBold->ConditionalPostLoad();
			}
		}
		if (!IsValid(Style->FontMedium))
		{
			Style->FontMedium = LoadAsset<UFontFace>(Paths::FontMedium);
			if (IsValid(Style->FontMedium))
			{
				Style->FontMedium->ConditionalPostLoad();
			}
		}
	}

	const USPGameHUDStyleData& ResolveGameHUDStyle(const USPGameHUDStyleData* Override)
	{
		static USPGameHUDStyleData* BuiltInFallback = nullptr;

		USPGameHUDStyleData* Resolved = nullptr;
		if (IsValid(Override))
		{
			Resolved = const_cast<USPGameHUDStyleData*>(Override);
		}
		else
		{
			Resolved = LoadStyleAsset<USPGameHUDStyleData>(Paths::GameHUDStyleAsset, BuiltInFallback);
		}

		if (!IsValid(Resolved))
		{
			Resolved = EnsureRootedBuiltInFallback(BuiltInFallback);
		}

		EnsureBuiltInGameHUDStyle(Resolved);
		checkf(IsValid(Resolved), TEXT("ResolveGameHUDStyle produced null style"));
		return *Resolved;
	}

	const USPMainMenuStyleData& ResolveMainMenuStyle(const USPMainMenuStyleData* Override)
	{
		static USPMainMenuStyleData* BuiltInFallback = nullptr;

		USPMainMenuStyleData* Resolved = nullptr;
		if (IsValid(Override))
		{
			Resolved = const_cast<USPMainMenuStyleData*>(Override);
		}
		else
		{
			Resolved = LoadStyleAsset<USPMainMenuStyleData>(Paths::MainMenuStyleAsset, BuiltInFallback);
		}

		if (!IsValid(Resolved))
		{
			Resolved = EnsureRootedBuiltInFallback(BuiltInFallback);
		}

		EnsureBuiltInMainMenuStyle(Resolved);
		checkf(IsValid(Resolved), TEXT("ResolveMainMenuStyle produced null style"));
		return *Resolved;
	}

	const USPUIFontStyleData& ResolveFontStyle(const USPUIFontStyleData* Override)
	{
		static USPUIFontStyleData* BuiltInFallback = nullptr;

		USPUIFontStyleData* Resolved = nullptr;
		if (IsValid(Override))
		{
			Resolved = const_cast<USPUIFontStyleData*>(Override);
		}
		else
		{
			Resolved = LoadStyleAsset<USPUIFontStyleData>(Paths::FontStyleAsset, BuiltInFallback);
		}

		if (!IsValid(Resolved))
		{
			Resolved = EnsureRootedBuiltInFallback(BuiltInFallback);
		}

		EnsureBuiltInFontStyle(Resolved);
		checkf(IsValid(Resolved), TEXT("ResolveFontStyle produced null style"));
		return *Resolved;
	}
}

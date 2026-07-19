#include "UI/Style/SPUIStyleLibrary.h"

#include "Engine/FontFace.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInterface.h"
#include "UI/HUDTypes.h"
#include "UI/Style/SPUIStyleData.h"

namespace SPUIStyleLibrary
{
	/** Stale/deleted UObject* sentinels must not reach IsValid(). */
	static bool IsUsableObjectPointer(const void* Ptr)
	{
		if (!Ptr)
		{
			return false;
		}

		const intptr_t Address = reinterpret_cast<intptr_t>(Ptr);
		if (Address <= 0)
		{
			return false;
		}

		constexpr intptr_t MaxUserSpaceAddress = 0x00007FFFFFFFFFFFLL;
		return Address >= 0x10000 && Address <= MaxUserSpaceAddress;
	}

	template<typename TObject>
	TObject* GetValidObjectPtr(const TObjectPtr<TObject>& Ptr)
	{
		if (Ptr.IsNull())
		{
			return nullptr;
		}

		UObject* Raw = Ptr.Get();
		if (!IsUsableObjectPointer(Raw) || !IsValid(Raw))
		{
			return nullptr;
		}

		return Cast<TObject>(Raw);
	}

	namespace Paths
	{
		static const FSoftObjectPath GameHUDStyleAsset(
			TEXT("/Game/UI/Data/DA_GameHUDStyle.DA_GameHUDStyle"));
		static const FSoftObjectPath MainMenuStyleAsset(
			TEXT("/Game/UI/Data/DA_MainMenuStyle.DA_MainMenuStyle"));
		static const FSoftObjectPath GameResultStyleAsset(
			TEXT("/Game/UI/Data/DA_GameResultStyle.DA_GameResultStyle"));
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
#pragma region GameResult
		static const FSoftObjectPath ResultBackground(
			TEXT("/Game/UI/Result/Textures/T_Result_Background.T_Result_Background"));
		static const FSoftObjectPath ResultSurvivorCharacter(
			TEXT("/Game/UI/Result/Textures/T_Result_Survivor_Character.T_Result_Survivor_Character"));
		static const FSoftObjectPath ResultKillerCharacter(
			TEXT("/Game/UI/Result/Textures/T_Result_Killer_Character.T_Result_Killer_Character"));
		static const FSoftObjectPath ResultSurvivorPerfectWinGrade(
			TEXT("/Game/UI/Result/Textures/Icons/T_Result_Grade_SurvivorPerfectWin.T_Result_Grade_SurvivorPerfectWin"));
		static const FSoftObjectPath ResultSurvivorWinGrade(
			TEXT("/Game/UI/Result/Textures/Icons/T_Result_Grade_SurvivorWin.T_Result_Grade_SurvivorWin"));
		static const FSoftObjectPath ResultSurvivorMinorWinGrade(
			TEXT("/Game/UI/Result/Textures/Icons/T_Result_Grade_SurvivorMinorWin.T_Result_Grade_SurvivorMinorWin"));
		static const FSoftObjectPath ResultKillerWinGrade(
			TEXT("/Game/UI/Result/Textures/Icons/T_Result_Grade_KillerWin.T_Result_Grade_KillerWin"));
		static const FSoftObjectPath ResultKillerPerfectWinGrade(
			TEXT("/Game/UI/Result/Textures/Icons/T_Result_Grade_KillerPerfectWin.T_Result_Grade_KillerPerfectWin"));
		static const FSoftObjectPath ResultDeliveredValueIcon(
			TEXT("/Game/UI/Result/Textures/Icons/T_Result_Icon_DeliveredValue.T_Result_Icon_DeliveredValue"));
		static const FSoftObjectPath ResultDeliveryCountIcon(
			TEXT("/Game/UI/Result/Textures/Icons/T_Result_Icon_DeliveryCount.T_Result_Icon_DeliveryCount"));
		static const FSoftObjectPath ResultRescueIcon(
			TEXT("/Game/UI/Result/Textures/Icons/T_Result_Icon_Rescue.T_Result_Icon_Rescue"));
		static const FSoftObjectPath ResultSelfHealIcon(
			TEXT("/Game/UI/Result/Textures/Icons/T_Result_Icon_SelfHeal.T_Result_Icon_SelfHeal"));
		static const FSoftObjectPath ResultCagedIcon(
			TEXT("/Game/UI/Result/Textures/Icons/T_Result_Icon_Caged.T_Result_Icon_Caged"));
		static const FSoftObjectPath ResultKilledSurvivorsIcon(
			TEXT("/Game/UI/Result/Textures/Icons/T_Result_Icon_KilledSurvivors.T_Result_Icon_KilledSurvivors"));
		static const FSoftObjectPath ResultValidHitIcon(
			TEXT("/Game/UI/Result/Textures/Icons/T_Result_Icon_ValidHit.T_Result_Icon_ValidHit"));
		static const FSoftObjectPath ResultDownIcon(
			TEXT("/Game/UI/Result/Textures/Icons/T_Result_Icon_Down.T_Result_Icon_Down"));
		static const FSoftObjectPath ResultCageCountIcon(
			TEXT("/Game/UI/Result/Textures/Icons/T_Result_Icon_CageCount.T_Result_Icon_CageCount"));
#pragma endregion 
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
				// Transient-package objects are GC'd between PIE sessions; root so the static
				// fallback pointer stays valid on the second Play.
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
		if (!Style->DeliveryStackEmpty)
		{
			Style->DeliveryStackEmpty = Style->DeliveryProgressBackground
				? Style->DeliveryProgressBackground
				: Style->DeliveryProgressEmptyFallback;
		}
		if (!Style->DeliveryStackFilled)
		{
			Style->DeliveryStackFilled = LoadAsset<UTexture2D>(Paths::DeliveryStackFilled);
		}
		if (!Style->DeliveryStackFilled)
		{
			Style->DeliveryStackFilled = Style->DeliveryProgressFillTexture
				? Style->DeliveryProgressFillTexture
				: Style->DeliveryProgressFillTextureFallback;
		}
		if (Style->DeliveryProgressSegmentTextures.Num() == 0)
		{
			Style->DeliveryProgressSegmentTextures.SetNum(SpaCh4HUD::DeliveryProgressSegmentCount + 1);
			for (int32 Index = 0; Index <= SpaCh4HUD::DeliveryProgressSegmentCount; ++Index)
			{
				Style->DeliveryProgressSegmentTextures[Index] = LoadAsset<UTexture2D>(Paths::DeliveryProgressSegment(Index));
			}
		}
		if (!GetValidObjectPtr<UTexture2D>(Style->PortraitSlotFrame))
		{
			Style->PortraitSlotFrame = LoadAsset<UTexture2D>(Paths::PortraitSlotFrame);
		}
		if (!GetValidObjectPtr<UTexture2D>(Style->PortraitHealthy))
		{
			Style->PortraitHealthy = LoadAsset<UTexture2D>(Paths::PortraitHealthy);
		}
		if (!GetValidObjectPtr<UTexture2D>(Style->PortraitInjured))
		{
			Style->PortraitInjured = LoadAsset<UTexture2D>(Paths::PortraitInjured);
		}
		if (!GetValidObjectPtr<UTexture2D>(Style->PortraitDead))
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

	static void EnsureBuiltInGameResultStyle(USPGameResultStyleData* Style)
	{
		if (!Style)
		{
			return;
		}

		if (!Style->Background)
		{
			Style->Background = LoadAsset<UTexture2D>(Paths::ResultBackground);
		}
		if (!Style->SurvivorCharacter)
		{
			Style->SurvivorCharacter = LoadAsset<UTexture2D>(Paths::ResultSurvivorCharacter);
		}
		if (!Style->KillerCharacter)
		{
			Style->KillerCharacter = LoadAsset<UTexture2D>(Paths::ResultKillerCharacter);
		}
		if (!Style->SurvivorPerfectWinGrade)
		{
			Style->SurvivorPerfectWinGrade = LoadAsset<UTexture2D>(Paths::ResultSurvivorPerfectWinGrade);
		}
		if (!Style->SurvivorWinGrade)
		{
			Style->SurvivorWinGrade = LoadAsset<UTexture2D>(Paths::ResultSurvivorWinGrade);
		}
		if (!Style->SurvivorMinorWinGrade)
		{
			Style->SurvivorMinorWinGrade = LoadAsset<UTexture2D>(Paths::ResultSurvivorMinorWinGrade);
		}
		if (!Style->KillerWinGrade)
		{
			Style->KillerWinGrade = LoadAsset<UTexture2D>(Paths::ResultKillerWinGrade);
		}
		if (!Style->KillerPerfectWinGrade)
		{
			Style->KillerPerfectWinGrade = LoadAsset<UTexture2D>(Paths::ResultKillerPerfectWinGrade);
		}
		if (!Style->DeliveredValueIcon)
		{
			Style->DeliveredValueIcon = LoadAsset<UTexture2D>(Paths::ResultDeliveredValueIcon);
		}
		if (!Style->DeliveryCountIcon)
		{
			Style->DeliveryCountIcon = LoadAsset<UTexture2D>(Paths::ResultDeliveryCountIcon);
		}
		if (!Style->RescueIcon)
		{
			Style->RescueIcon = LoadAsset<UTexture2D>(Paths::ResultRescueIcon);
		}
		if (!Style->SelfHealIcon)
		{
			Style->SelfHealIcon = LoadAsset<UTexture2D>(Paths::ResultSelfHealIcon);
		}
		if (!Style->CagedIcon)
		{
			Style->CagedIcon = LoadAsset<UTexture2D>(Paths::ResultCagedIcon);
		}
		if (!Style->KilledSurvivorsIcon)
		{
			Style->KilledSurvivorsIcon = LoadAsset<UTexture2D>(Paths::ResultKilledSurvivorsIcon);
		}
		if (!Style->ValidHitIcon)
		{
			Style->ValidHitIcon = LoadAsset<UTexture2D>(Paths::ResultValidHitIcon);
		}
		if (!Style->DownIcon)
		{
			Style->DownIcon = LoadAsset<UTexture2D>(Paths::ResultDownIcon);
		}
		if (!Style->CageCountIcon)
		{
			Style->CageCountIcon = LoadAsset<UTexture2D>(Paths::ResultCageCountIcon);
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
	USPGameHUDStyleData* Resolved = (IsUsableObjectPointer(Override) && IsValid(Override))
		? const_cast<USPGameHUDStyleData*>(Override)
		: LoadStyleAsset<USPGameHUDStyleData>(Paths::GameHUDStyleAsset, BuiltInFallback);
	EnsureBuiltInGameHUDStyle(Resolved);
	return *Resolved;
}

	const USPGameResultStyleData& ResolveGameResultStyle(const USPGameResultStyleData* Override)
	{
		static USPGameResultStyleData* BuiltInFallback = nullptr;
		USPGameResultStyleData* Resolved = (IsUsableObjectPointer(Override) && IsValid(Override))
			? const_cast<USPGameResultStyleData*>(Override)
			: LoadStyleAsset<USPGameResultStyleData>(Paths::GameResultStyleAsset, BuiltInFallback);
		EnsureBuiltInGameResultStyle(Resolved);
		return *Resolved;
	}

	const USPMainMenuStyleData& ResolveMainMenuStyle(const USPMainMenuStyleData* Override)
	{
		static USPMainMenuStyleData* BuiltInFallback = nullptr;
		USPMainMenuStyleData* Resolved = (IsUsableObjectPointer(Override) && IsValid(Override))
			? const_cast<USPMainMenuStyleData*>(Override)
			: LoadStyleAsset<USPMainMenuStyleData>(Paths::MainMenuStyleAsset, BuiltInFallback);
		EnsureBuiltInMainMenuStyle(Resolved);
		return *Resolved;
	}

	const USPUIFontStyleData& ResolveFontStyle(const USPUIFontStyleData* Override)
	{
		static USPUIFontStyleData* BuiltInFallback = nullptr;
		USPUIFontStyleData* Resolved = (IsUsableObjectPointer(Override) && IsValid(Override))
			? const_cast<USPUIFontStyleData*>(Override)
			: LoadStyleAsset<USPUIFontStyleData>(Paths::FontStyleAsset, BuiltInFallback);
		EnsureBuiltInFontStyle(Resolved);
		return *Resolved;
	}
}

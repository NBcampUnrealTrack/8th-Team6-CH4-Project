#include "UI/TeammateEntryWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UI/HUDFontUtils.h"

namespace
{
	static const TCHAR* PortraitSlotTexturePath = TEXT("/Game/UI/HUD/Textures/Teammate/T_HUD_Portrait_Slot.T_HUD_Portrait_Slot");
	static const TCHAR* DownedHealthBarBGTexturePath = TEXT("/Game/UI/HUD/Textures/Common/T_HUD_Bar_BG.T_HUD_Bar_BG");
	static const TCHAR* DownedHealthFillMIPath =
		TEXT("/Game/UI/HUD/Materials/MI_HUD_HealthBarFill_Downed.MI_HUD_HealthBarFill_Downed");
	static const TCHAR* DownedHealthFillMaterialPath =
		TEXT("/Game/UI/HUD/Materials/M_HUD_HealthBarFill.M_HUD_HealthBarFill");
	static const TCHAR* DownedHealthFillTexturePath =
		TEXT("/Game/UI/HUD/Textures/Teammate/T_HUD_Bar_Fill_Downed.T_HUD_Bar_Fill_Downed");

	static FLinearColor FrameTintForState(ESurvivorDisplayState State)
	{
		switch (State)
		{
		case ESurvivorDisplayState::Injured:
			return FLinearColor(1.0f, 0.86f, 0.45f, 1.0f);
		case ESurvivorDisplayState::Downed:
			return FLinearColor(1.0f, 0.42f, 0.42f, 1.0f);
		case ESurvivorDisplayState::Caged:
			return FLinearColor(0.55f, 0.72f, 1.0f, 1.0f);
		case ESurvivorDisplayState::Dead:
			return FLinearColor(0.55f, 0.58f, 0.62f, 0.75f);
		case ESurvivorDisplayState::Escaped:
			return FLinearColor(0.82f, 0.86f, 0.92f, 0.85f);
		default:
			return FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
		}
	}
}

void UTeammateEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (PortraitSlotFrame)
	{
		if (UTexture2D* FrameTex = LoadObject<UTexture2D>(nullptr, PortraitSlotTexturePath))
		{
			PortraitSlotFrame->SetBrushFromTexture(FrameTex, true);
		}
		UpdatePortraitFrame(ESurvivorDisplayState::Healthy);
	}

	SetupDownedHealthBar();
}

void UTeammateEntryWidget::SetupDownedHealthBar()
{
	const FVector2D BarSize(DownedHealthBarWidth, DownedHealthBarHeight);

	if (DownedHealthBarBG)
	{
		if (UTexture2D* BgTexture = LoadObject<UTexture2D>(nullptr, DownedHealthBarBGTexturePath))
		{
			DownedHealthBarBG->SetBrushFromTexture(BgTexture, true);
			FSlateBrush BgBrush = DownedHealthBarBG->GetBrush();
			SpaCh4HUD::SetBrushImageSize(BgBrush, BarSize);
			DownedHealthBarBG->SetBrush(BgBrush);
		}
		DownedHealthBarBG->SetColorAndOpacity(FLinearColor::White);
	}

	if (!DownedHealthBarFill)
	{
		return;
	}

	DownedHealthBarFill->SetColorAndOpacity(FLinearColor::White);

	UMaterialInterface* FillParent = LoadObject<UMaterialInterface>(nullptr, DownedHealthFillMIPath);
	if (!FillParent)
	{
		FillParent = LoadObject<UMaterialInterface>(nullptr, DownedHealthFillMaterialPath);
	}

	if (FillParent)
	{
		DownedHealthBarFillMID = UMaterialInstanceDynamic::Create(FillParent, this);
		if (DownedHealthBarFillMID)
		{
			if (UTexture2D* FillTexture = LoadObject<UTexture2D>(nullptr, DownedHealthFillTexturePath))
			{
				DownedHealthBarFillMID->SetTextureParameterValue(TEXT("FillTexture"), FillTexture);
			}

			DownedHealthBarFill->SetBrushFromMaterial(DownedHealthBarFillMID);
			FSlateBrush FillBrush = DownedHealthBarFill->GetBrush();
			SpaCh4HUD::SetBrushImageSize(FillBrush, BarSize);
			DownedHealthBarFill->SetBrush(FillBrush);
			return;
		}
	}

	if (UTexture2D* FillTexture = LoadObject<UTexture2D>(nullptr, DownedHealthFillTexturePath))
	{
		DownedHealthBarFill->SetBrushFromTexture(FillTexture, true);
		FSlateBrush FillBrush = DownedHealthBarFill->GetBrush();
		SpaCh4HUD::SetBrushImageSize(FillBrush, BarSize);
		DownedHealthBarFill->SetBrush(FillBrush);
	}
}

void UTeammateEntryWidget::UpdateFromData(const FTeammateHUDData& Data)
{
	UpdateNickname(Data.Nickname);
	UpdateCageStack(Data.CageStack, Data.MaxCageStack);

	const bool bShowDownedHealth = Data.DisplayState == ESurvivorDisplayState::Downed;
	UpdateDownedHealth(Data.DownedHealthPercent, bShowDownedHealth);
	UpdatePortraitFrame(Data.DisplayState);
	OnDisplayStateChanged(Data.DisplayState);
}

void UTeammateEntryWidget::UpdateNickname(const FText& Nickname)
{
	if (NicknameText)
	{
		NicknameText->SetText(Nickname);
	}
}

void UTeammateEntryWidget::UpdateCageStack(int32 Stack, int32 MaxStack)
{
	if (CageStackText)
	{
		CageStackText->SetText(FText::Format(
			NSLOCTEXT("SpaCh4", "CageStackFormat", "{0}/{1}"),
			FText::AsNumber(Stack),
			FText::AsNumber(MaxStack)));
	}
}

void UTeammateEntryWidget::UpdateDownedHealth(float HealthPercent, bool bShowBar)
{
	const ESlateVisibility BarVisibility =
		bShowBar ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;

	if (DownedHealthBarRoot)
	{
		DownedHealthBarRoot->SetVisibility(BarVisibility);
	}
	else
	{
		if (DownedHealthBarBG)
		{
			DownedHealthBarBG->SetVisibility(BarVisibility);
		}
		if (DownedHealthBarFill)
		{
			DownedHealthBarFill->SetVisibility(BarVisibility);
		}
	}

	const float ClampedPercent = FMath::Clamp(HealthPercent, 0.0f, 1.0f);
	if (DownedHealthBarFill)
	{
		DownedHealthBarFill->SetDesiredSizeOverride(
			FVector2D(DownedHealthBarWidth * ClampedPercent, DownedHealthBarHeight));
	}
}

void UTeammateEntryWidget::UpdatePortraitFrame(ESurvivorDisplayState State)
{
	if (!PortraitSlotFrame)
	{
		return;
	}

	PortraitSlotFrame->SetColorAndOpacity(FrameTintForState(State));
}

#include "TeammateEntryWidget.h"

#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "HUDFontUtils.h"

namespace
{
	static const TCHAR* PortraitSlotTexturePath = TEXT("/Game/UI/HUD/Textures/Teammate/T_HUD_Portrait_Slot.T_HUD_Portrait_Slot");

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

	static UFontFace* SemiBold = SpaCh4HUD::LoadFontFace(SpaCh4HUD::FontSemiBoldPath);
	static UFontFace* Medium = SpaCh4HUD::LoadFontFace(SpaCh4HUD::FontMediumPath);
	SpaCh4HUD::ApplyTextFont(NicknameText, SemiBold, 22);
	SpaCh4HUD::ApplyTextFont(CageStackText, Medium, 18);

	if (PortraitSlotFrame)
	{
		if (UTexture2D* FrameTex = LoadObject<UTexture2D>(nullptr, PortraitSlotTexturePath))
		{
			PortraitSlotFrame->SetBrushFromTexture(FrameTex, true);
		}
		UpdatePortraitFrame(ESurvivorDisplayState::Healthy);
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
	if (!DownedHealthBar)
	{
		return;
	}

	DownedHealthBar->SetPercent(FMath::Clamp(HealthPercent, 0.0f, 1.0f));
	DownedHealthBar->SetVisibility(bShowBar ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void UTeammateEntryWidget::UpdatePortraitFrame(ESurvivorDisplayState State)
{
	if (!PortraitSlotFrame)
	{
		return;
	}

	PortraitSlotFrame->SetColorAndOpacity(FrameTintForState(State));
}

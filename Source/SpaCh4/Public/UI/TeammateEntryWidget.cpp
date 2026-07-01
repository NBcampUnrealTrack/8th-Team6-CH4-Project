#include "UI/TeammateEntryWidget.h"

#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UI/HUDFontUtils.h"

namespace
{
	static constexpr float DefaultPortraitFrameSize = 100.0f;
	static constexpr float PortraitIconInsetRatio = 0.68f;
	static const TCHAR* PortraitSlotTexturePath = TEXT("/Game/UI/HUD/Textures/Teammate/T_HUD_Portrait_Slot.T_HUD_Portrait_Slot");
	static const TCHAR* PortraitNormalTexturePath =
		TEXT("/Game/UI/HUD/Textures/Teammate/T_HUD_Portrait_Placeholder.T_HUD_Portrait_Placeholder");
	static const TCHAR* PortraitInjuredTexturePath =
		TEXT("/Game/UI/HUD/Textures/Teammate/T_HUD_Portrait_Injured.T_HUD_Portrait_Injured");
	static const TCHAR* PortraitDeadTexturePath =
		TEXT("/Game/UI/HUD/Textures/Teammate/T_HUD_Portrait_Dead.T_HUD_Portrait_Dead");
	static const TCHAR* DownedHealthBarBGTexturePath = TEXT("/Game/UI/HUD/Textures/Common/T_HUD_Bar_BG.T_HUD_Bar_BG");
	static const TCHAR* DownedHealthFillMIPath =
		TEXT("/Game/UI/HUD/Materials/MI_HUD_HealthBarFill_Downed.MI_HUD_HealthBarFill_Downed");
	static const TCHAR* DownedHealthFillMaterialPath =
		TEXT("/Game/UI/HUD/Materials/M_HUD_HealthBarFill.M_HUD_HealthBarFill");
	static const TCHAR* DownedHealthFillTexturePath =
		TEXT("/Game/UI/HUD/Textures/Teammate/T_HUD_Bar_Fill_Downed.T_HUD_Bar_Fill_Downed");

	static FLinearColor FrameTintForState(ESurvivorDisplayState State)
	{
		// Ornate portrait frame art carries its own color; avoid washing it with state tints.
		(void)State;
		return FLinearColor::White;
	}

	static const TCHAR* PortraitTexturePathForState(ESurvivorDisplayState State)
	{
		switch (State)
		{
		case ESurvivorDisplayState::Injured:
		case ESurvivorDisplayState::Downed:
		case ESurvivorDisplayState::Carried:
			return PortraitInjuredTexturePath;
		case ESurvivorDisplayState::Dead:
			return PortraitDeadTexturePath;
		default:
			return PortraitNormalTexturePath;
		}
	}

	static bool IsValidDisplaySize(const FVector2D& Size)
	{
		return Size.X > KINDA_SMALL_NUMBER && Size.Y > KINDA_SMALL_NUMBER;
	}

	static bool LooksLikeTextureNativeSize(const FVector2D& Size, const UTexture2D* Texture)
	{
		if (!Texture || !IsValidDisplaySize(Size))
		{
			return false;
		}

		const float TexW = static_cast<float>(Texture->GetSizeX());
		const float TexH = static_cast<float>(Texture->GetSizeY());
		return FMath::IsNearlyEqual(Size.X, TexW, 1.0f) && FMath::IsNearlyEqual(Size.Y, TexH, 1.0f);
	}

	static FVector2D ResolveImageWidgetDisplaySize(const UImage* Image, const FVector2D& DefaultSize)
	{
		if (!Image)
		{
			return DefaultSize;
		}

		const FSlateBrush& Brush = Image->GetBrush();
		const FVector2D BrushSize = Brush.ImageSize;
		const UTexture2D* CurrentTex = Cast<UTexture2D>(Brush.GetResourceObject());
		if (IsValidDisplaySize(BrushSize) && !LooksLikeTextureNativeSize(BrushSize, CurrentTex))
		{
			return BrushSize;
		}

		return DefaultSize;
	}

	static void SetImageTextureWithDisplaySize(UImage* Image, UTexture2D* Texture, const FVector2D& DisplaySize)
	{
		if (!Image || !Texture || !IsValidDisplaySize(DisplaySize))
		{
			return;
		}

		FSlateBrush Brush = Image->GetBrush();
		Brush.SetResourceObject(Texture);
		SpaCh4HUD::SetBrushImageSize(Brush, DisplaySize);
		Image->SetBrush(Brush);
		Image->SetDesiredSizeOverride(DisplaySize);
	}
}

UTexture2D* UTeammateEntryWidget::ResolvePortraitTexture(ESurvivorDisplayState State) const
{
	switch (State)
	{
	case ESurvivorDisplayState::Injured:
	case ESurvivorDisplayState::Downed:
	case ESurvivorDisplayState::Carried:
		if (PortraitIconInjured)
		{
			return PortraitIconInjured;
		}
		break;
	case ESurvivorDisplayState::Dead:
		if (PortraitIconDead)
		{
			return PortraitIconDead;
		}
		break;
	default:
		if (PortraitIconHealthy)
		{
			return PortraitIconHealthy;
		}
		break;
	}

	return LoadObject<UTexture2D>(nullptr, PortraitTexturePathForState(State));
}

UTexture2D* UTeammateEntryWidget::ResolvePortraitSlotTexture() const
{
	if (PortraitSlotTexture)
	{
		return PortraitSlotTexture;
	}

	return LoadObject<UTexture2D>(nullptr, PortraitSlotTexturePath);
}

FVector2D UTeammateEntryWidget::ResolvePortraitIconSize() const
{
	const FVector2D FrameSize = ResolvePortraitFrameSize();
	const float FrameAxis = FMath::Max(FrameSize.X, FrameSize.Y);
	const FVector2D FittedIconSize(
		FrameAxis > KINDA_SMALL_NUMBER ? FrameAxis * PortraitIconInsetRatio : DefaultPortraitFrameSize * PortraitIconInsetRatio,
		FrameAxis > KINDA_SMALL_NUMBER ? FrameAxis * PortraitIconInsetRatio : DefaultPortraitFrameSize * PortraitIconInsetRatio);

	if (IsValidDisplaySize(PortraitIconDisplaySize))
	{
		return PortraitIconDisplaySize;
	}

	const FVector2D DesignerSize = ResolveImageWidgetDisplaySize(PortraitImage, FVector2D::ZeroVector);
	if (IsValidDisplaySize(DesignerSize) && DesignerSize.X <= FittedIconSize.X * 1.05f)
	{
		return DesignerSize;
	}

	return FittedIconSize;
}

FVector2D UTeammateEntryWidget::ResolvePortraitFrameSize() const
{
	const FVector2D IconSize = CachedPortraitIconSize;
	const FVector2D DefaultSize =
		IsValidDisplaySize(IconSize) ? IconSize : FVector2D(DefaultPortraitDisplaySize, DefaultPortraitDisplaySize);

	if (IsValidDisplaySize(PortraitFrameDisplaySize))
	{
		return PortraitFrameDisplaySize;
	}

	return ResolveImageWidgetDisplaySize(PortraitSlotFrame, DefaultSize);
}

void UTeammateEntryWidget::CachePortraitLayoutSizes()
{
	CachedPortraitFrameSize = ResolvePortraitFrameSize();
	CachedPortraitIconSize = ResolvePortraitIconSize();
}

void UTeammateEntryWidget::EnsurePortraitOverlayZOrder()
{
	if (!PortraitImage || !PortraitSlotFrame)
	{
		return;
	}

	UOverlay* Overlay = Cast<UOverlay>(PortraitImage->GetParent());
	if (!Overlay)
	{
		return;
	}

	const int32 ImageIndex = Overlay->GetChildIndex(PortraitImage);
	const int32 FrameIndex = Overlay->GetChildIndex(PortraitSlotFrame);
	if (ImageIndex == INDEX_NONE || FrameIndex == INDEX_NONE)
	{
		return;
	}

	if (ImageIndex <= FrameIndex)
	{
		return;
	}

	Overlay->RemoveChild(PortraitImage);
	Overlay->RemoveChild(PortraitSlotFrame);
	Overlay->AddChild(PortraitImage);
	Overlay->AddChild(PortraitSlotFrame);
}

void UTeammateEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();

	CachePortraitLayoutSizes();

	if (PortraitSlotFrame)
	{
		if (UTexture2D* FrameTex = ResolvePortraitSlotTexture())
		{
			SetImageTextureWithDisplaySize(PortraitSlotFrame, FrameTex, CachedPortraitFrameSize);
		}
		UpdatePortraitFrame(ESurvivorDisplayState::Healthy);
	}

	UpdatePortraitImage(ESurvivorDisplayState::Healthy);

	EnsurePortraitOverlayZOrder();

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
	UpdatePortraitImage(Data.DisplayState);
	UpdatePortraitFrame(Data.DisplayState);
	OnDisplayStateChanged(Data.DisplayState);
	EnsurePortraitOverlayZOrder();
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

	// Color/tint owned by Blueprint brush defaults.
	(void)State;
}

void UTeammateEntryWidget::UpdatePortraitImage(ESurvivorDisplayState State)
{
	if (!PortraitImage)
	{
		return;
	}

	if (UTexture2D* PortraitTex = ResolvePortraitTexture(State))
	{
		if (!IsValidDisplaySize(CachedPortraitIconSize))
		{
			CachePortraitLayoutSizes();
		}

		SetImageTextureWithDisplaySize(PortraitImage, PortraitTex, CachedPortraitIconSize);
		EnsurePortraitOverlayZOrder();
	}
}

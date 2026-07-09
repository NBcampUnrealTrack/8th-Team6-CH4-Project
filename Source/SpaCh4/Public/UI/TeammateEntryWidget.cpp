#include "UI/TeammateEntryWidget.h"

#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"
#include "UI/HUDFontUtils.h"
#include "UI/Style/SPUIStyleData.h"
#include "UI/Style/SPUIStyleLibrary.h"

namespace
{
	static constexpr float DefaultPortraitFrameSize = 100.0f;
	static constexpr float PortraitIconInsetRatio = 0.68f;

	static FLinearColor FrameTintForState(ESurvivorDisplayState State)
	{
		(void)State;
		return FLinearColor::White;
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

const USPGameHUDStyleData& UTeammateEntryWidget::GetResolvedStyle() const
{
	return SPUIStyleLibrary::ResolveGameHUDStyle(VisualStyle);
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
		return GetResolvedStyle().PortraitInjured;
	case ESurvivorDisplayState::Dead:
		if (PortraitIconDead)
		{
			return PortraitIconDead;
		}
		return GetResolvedStyle().PortraitDead;
	default:
		if (PortraitIconHealthy)
		{
			return PortraitIconHealthy;
		}
		return GetResolvedStyle().PortraitHealthy;
	}
}

UTexture2D* UTeammateEntryWidget::ResolvePortraitSlotTexture() const
{
	if (PortraitSlotTexture)
	{
		return PortraitSlotTexture;
	}

	return GetResolvedStyle().PortraitSlotFrame;
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

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateUObject(this, &UTeammateEntryWidget::SetupDownedHealthBar));
	}
	else
	{
		SetupDownedHealthBar();
	}
}

void UTeammateEntryWidget::SetupDownedHealthBar()
{
	if (!DownedHealthBarBG && !DownedHealthBarFill)
	{
		return;
	}

	const USPGameHUDStyleData& Style = GetResolvedStyle();
	const FVector2D BarSize(DownedHealthBarWidth, DownedHealthBarHeight);

	if (DownedHealthBarBG)
	{
		if (UTexture2D* BgTexture = Style.DownedHealthBarBackground)
		{
			if (IsValid(BgTexture))
			{
				BgTexture->ConditionalPostLoad();
				SetImageTextureWithDisplaySize(DownedHealthBarBG, BgTexture, BarSize);
			}
		}
		DownedHealthBarBG->SetColorAndOpacity(FLinearColor::White);
	}

	if (!DownedHealthBarFill)
	{
		return;
	}

	DownedHealthBarFill->SetColorAndOpacity(FLinearColor::White);

	UMaterialInterface* FillParent = Style.DownedHealthFillMaterialInstance;
	if (!IsValid(FillParent))
	{
		FillParent = Style.DownedHealthFillMaterial;
	}

	if (IsValid(FillParent))
	{
		DownedHealthBarFillMID = UMaterialInstanceDynamic::Create(FillParent, this);
		if (DownedHealthBarFillMID)
		{
			if (UTexture2D* FillTexture = Style.DownedHealthFillTexture)
			{
				if (IsValid(FillTexture))
				{
					FillTexture->ConditionalPostLoad();
					DownedHealthBarFillMID->SetTextureParameterValue(TEXT("FillTexture"), FillTexture);
				}
			}

			DownedHealthBarFill->SetBrushFromMaterial(DownedHealthBarFillMID);
			DownedHealthBarFill->SetDesiredSizeOverride(BarSize);
			return;
		}
	}

	if (UTexture2D* FillTexture = Style.DownedHealthFillTexture)
	{
		if (IsValid(FillTexture))
		{
			FillTexture->ConditionalPostLoad();
			SetImageTextureWithDisplaySize(DownedHealthBarFill, FillTexture, BarSize);
		}
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

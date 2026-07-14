#include "UI/TeammateEntryWidget.h"

#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"
#include "UI/HUDFontUtils.h"
#include "UI/Style/SPUIStyleData.h"
#include "UI/Style/SPUIStyleLibrary.h"

namespace TeammateEntryWidgetPrivate
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

	static bool IsImageWidgetSlateReady(const UImage* Image)
	{
		if (!Image || !IsValid(Image) || !Image->IsConstructed())
		{
			return false;
		}

		return Image->GetCachedWidget().IsValid();
	}

	static FVector2D ResolveImageWidgetDisplaySize(const UImage* Image, const FVector2D& DefaultSize)
	{
		if (!IsImageWidgetSlateReady(Image))
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
		if (!Image || !IsValid(Image) || !Texture || !IsValid(Texture) || !IsValidDisplaySize(DisplaySize))
		{
			return;
		}

		Texture->ConditionalPostLoad();
		Image->SetBrushFromTexture(Texture, true);
		Image->SetDesiredSizeOverride(DisplaySize);
	}

	static bool HasDesignerBrushResourceTE(const UImage* Image)
	{
		return IsValid(Image) && Image->GetBrush().GetResourceObject() != nullptr;
	}
}

using namespace TeammateEntryWidgetPrivate;

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

	// ShiftChild keeps each child's OverlaySlot (alignment/padding) and avoids the
	// Slate release/recreate of RemoveChild+AddChild, which can AV when this runs
	// from the HUD refresh timer or a delegate broadcast mid-frame.
	Overlay->ShiftChild(FrameIndex, PortraitImage);
}

void UTeammateEntryWidget::CollapseLegacyDownedHealthFillIfNeeded()
{
	// Prefer designer Overlay (BG + Fill). Only hide Fill when ProgressBar is the sole bar.
	if (!IsValid(DownedHealthBarProgress) || !IsValid(DownedHealthBarFill))
	{
		return;
	}

	if (IsValid(DownedHealthBarRoot)
		|| TeammateEntryWidgetPrivate::HasDesignerBrushResourceTE(DownedHealthBarFill))
	{
		return;
	}

	DownedHealthBarFill->SetVisibility(ESlateVisibility::Collapsed);
}

void UTeammateEntryWidget::ResolveOptionalWidgetBindings()
{
	// WBP Is Variable off → BindWidgetOptional stays null; WidgetTree still finds by name.
	if (!WidgetTree)
	{
		return;
	}

	if (!NicknameText)
	{
		NicknameText = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("NicknameText")));
	}
	if (!CageStackText)
	{
		CageStackText = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("CageStackText")));
	}
	if (!DownedHealthBarBG)
	{
		DownedHealthBarBG = Cast<UImage>(WidgetTree->FindWidget(TEXT("DownedHealthBarBG")));
	}
	if (!DownedHealthBarFill)
	{
		DownedHealthBarFill = Cast<UImage>(WidgetTree->FindWidget(TEXT("DownedHealthBarFill")));
	}
	if (!DownedHealthBarProgress)
	{
		DownedHealthBarProgress = Cast<UProgressBar>(WidgetTree->FindWidget(TEXT("DownedHealthBarProgress")));
	}
	if (!DownedHealthBarRoot)
	{
		DownedHealthBarRoot = WidgetTree->FindWidget(TEXT("DownedHealthBarRoot"));
	}
	// Root가 변수로 안 잡혀 있어도 Fill/BG 부모 Overlay를 펼 수 있음
	if (!DownedHealthBarRoot)
	{
		if (DownedHealthBarFill)
		{
			DownedHealthBarRoot = DownedHealthBarFill->GetParent();
		}
		else if (DownedHealthBarBG)
		{
			DownedHealthBarRoot = DownedHealthBarBG->GetParent();
		}
	}
	if (!PortraitImage)
	{
		PortraitImage = Cast<UImage>(WidgetTree->FindWidget(TEXT("PortraitImage")));
	}
	if (!PortraitSlotFrame)
	{
		PortraitSlotFrame = Cast<UImage>(WidgetTree->FindWidget(TEXT("PortraitSlotFrame")));
	}
}

void UTeammateEntryWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	ResolveOptionalWidgetBindings();
	CollapseLegacyDownedHealthFillIfNeeded();
}

void UTeammateEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ResolveOptionalWidgetBindings();
	ResetDownedHealthBarRuntimeState();
	CollapseLegacyDownedHealthFillIfNeeded();

	CachePortraitLayoutSizes();
	EnsurePortraitOverlayZOrder();

	if (IsDesignTime())
	{
		if (!IsValid(DownedHealthBarProgress))
		{
			SetupDownedHealthBar();
		}
		else
		{
			ApplyPortraitVisuals();
		}
		return;
	}

	ScheduleDownedHealthBarSetup();
}

void UTeammateEntryWidget::NativeDestruct()
{
	ClearDownedHealthBarSetupTimer();
	DownedHealthBarFillMID = nullptr;
	bDownedHealthBarInitialized = false;
	Super::NativeDestruct();
}

void UTeammateEntryWidget::ResetDownedHealthBarRuntimeState()
{
	ClearDownedHealthBarSetupTimer();
	DownedHealthBarFillMID = nullptr;
	bDownedHealthBarInitialized = false;
	DownedHealthBarSetupRetries = 0;
}

bool UTeammateEntryWidget::CanRunDeferredSetup() const
{
	const UWorld* World = GetWorld();
	return IsValid(this) && World && !World->bIsTearingDown;
}

void UTeammateEntryWidget::ClearDownedHealthBarSetupTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DownedHealthBarSetupTimerHandle);
	}
}

bool UTeammateEntryWidget::AreDownedHealthBarWidgetsReady() const
{
	if (IsValid(DownedHealthBarProgress))
	{
		return DownedHealthBarProgress->IsConstructed()
			&& DownedHealthBarProgress->GetCachedWidget().IsValid();
	}

	if (!DownedHealthBarBG && !DownedHealthBarFill)
	{
		return true;
	}

	auto IsReady = [](const UImage* Image) -> bool
	{
		return Image
			&& IsValid(Image)
			&& Image->IsConstructed()
			&& Image->GetCachedWidget().IsValid();
	};

	if (DownedHealthBarBG && !IsReady(DownedHealthBarBG))
	{
		return false;
	}

	if (DownedHealthBarFill && !IsReady(DownedHealthBarFill))
	{
		return false;
	}

	return true;
}

void UTeammateEntryWidget::ScheduleDownedHealthBarSetup()
{
	if (!CanRunDeferredSetup())
	{
		return;
	}

	UWorld* World = GetWorld();
	World->GetTimerManager().ClearTimer(DownedHealthBarSetupTimerHandle);
	World->GetTimerManager().SetTimer(
		DownedHealthBarSetupTimerHandle,
		this,
		&UTeammateEntryWidget::SetupDownedHealthBar,
		0.0f,
		false);
}

void UTeammateEntryWidget::ApplyPortraitVisuals()
{
	if (IsValid(PortraitSlotFrame))
	{
		if (UTexture2D* FrameTex = ResolvePortraitSlotTexture())
		{
			if (IsValid(FrameTex))
			{
				SetImageTextureWithDisplaySize(PortraitSlotFrame, FrameTex, CachedPortraitFrameSize);
			}
		}
		UpdatePortraitFrame(ESurvivorDisplayState::Healthy);
	}

	UpdatePortraitImage(ESurvivorDisplayState::Healthy);
	EnsurePortraitOverlayZOrder();
}

static bool IsDownedHealthFillMIDValid(
	const UMaterialInstanceDynamic* FillMID,
	const UImage* FillImage)
{
	return IsValid(FillMID)
		&& IsValid(FillImage)
		&& FillMID->GetOuter() == FillImage;
}

void UTeammateEntryWidget::SetupDownedHealthBar()
{
	if (!CanRunDeferredSetup())
	{
		ClearDownedHealthBarSetupTimer();
		return;
	}

	const bool bUsesProgressBar = IsValid(DownedHealthBarProgress);
	const bool bHasBG = IsValid(DownedHealthBarBG);
	const bool bHasFill = IsValid(DownedHealthBarFill);
	if (!bUsesProgressBar && !bHasBG && !bHasFill)
	{
		ClearDownedHealthBarSetupTimer();
		return;
	}

	if (!IsDesignTime() && !AreDownedHealthBarWidgetsReady())
	{
		constexpr int32 MaxRetries = 8;
		if (DownedHealthBarSetupRetries < MaxRetries)
		{
			++DownedHealthBarSetupRetries;
			ScheduleDownedHealthBarSetup();
		}
		else
		{
			ClearDownedHealthBarSetupTimer();
		}
		return;
	}

	ClearDownedHealthBarSetupTimer();
	ApplyPortraitVisuals();

	if (bDownedHealthBarInitialized)
	{
		return;
	}

	const bool bDesignerOwnsDownedBarLayout =
		bUsesProgressBar
		|| IsValid(DownedHealthBarRoot)
		|| (bHasBG && HasDesignerBrushResourceTE(DownedHealthBarBG));

	if (bDesignerOwnsDownedBarLayout)
	{
		CollapseLegacyDownedHealthFillIfNeeded();
		bDownedHealthBarInitialized = true;
		return;
	}

	const USPGameHUDStyleData& Style = GetResolvedStyle();
	const FVector2D BarSize(DownedHealthBarWidth, DownedHealthBarHeight);

	if (bHasBG)
	{
		if (UTexture2D* BgTexture = Style.DownedHealthBarBackground)
		{
			if (IsValid(BgTexture))
			{
				SetImageTextureWithDisplaySize(DownedHealthBarBG, BgTexture, BarSize);
			}
		}
		DownedHealthBarBG->SetColorAndOpacity(FLinearColor::White);
	}

	if (!bHasFill)
	{
		bDownedHealthBarInitialized = true;
		return;
	}

	if (HasDesignerBrushResourceTE(DownedHealthBarFill))
	{
		bDownedHealthBarInitialized = true;
		return;
	}

	DownedHealthBarFill->SetColorAndOpacity(FLinearColor::White);

	if (IsDownedHealthFillMIDValid(DownedHealthBarFillMID, DownedHealthBarFill))
	{
		DownedHealthBarFill->SetBrushFromMaterial(DownedHealthBarFillMID);
		DownedHealthBarFill->SetDesiredSizeOverride(BarSize);
		bDownedHealthBarInitialized = true;
		return;
	}

	DownedHealthBarFillMID = nullptr;

	UMaterialInterface* FillParent = Style.DownedHealthFillMaterialInstance;
	if (!IsValid(FillParent))
	{
		FillParent = Style.DownedHealthFillMaterial;
	}

	if (IsValid(FillParent) && CanRunDeferredSetup())
	{
		FillParent->ConditionalPostLoad();
		DownedHealthBarFillMID = UMaterialInstanceDynamic::Create(FillParent, DownedHealthBarFill);
		if (IsValid(DownedHealthBarFillMID))
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
			bDownedHealthBarInitialized = true;
			return;
		}
	}

	if (UTexture2D* FillTexture = Style.DownedHealthFillTexture)
	{
		if (IsValid(FillTexture))
		{
			SetImageTextureWithDisplaySize(DownedHealthBarFill, FillTexture, BarSize);
		}
	}

	bDownedHealthBarInitialized = true;
}

void UTeammateEntryWidget::UpdateFromData(const FTeammateHUDData& Data)
{
	ResolveOptionalWidgetBindings();

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
	ResolveOptionalWidgetBindings();

	const ESlateVisibility BarVisibility =
		bShowBar ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
	const float ClampedPercent = FMath::Clamp(HealthPercent, 0.0f, 1.0f);

	// Parent Overlay가 Collapsed면 Fill/BG만 Visible로 바꿔도 안 보임 → Root를 반드시 펼침
	if (!DownedHealthBarRoot && DownedHealthBarFill)
	{
		DownedHealthBarRoot = DownedHealthBarFill->GetParent();
	}
	if (!DownedHealthBarRoot && DownedHealthBarBG)
	{
		DownedHealthBarRoot = DownedHealthBarBG->GetParent();
	}

	const bool bHasImageBar =
		IsValid(DownedHealthBarRoot) || IsValid(DownedHealthBarBG) || IsValid(DownedHealthBarFill);

	if (DownedHealthBarRoot)
	{
		DownedHealthBarRoot->SetVisibility(BarVisibility);
	}

	if (bHasImageBar)
	{
		if (DownedHealthBarBG)
		{
			DownedHealthBarBG->SetVisibility(BarVisibility);
		}

		if (DownedHealthBarFill)
		{
			DownedHealthBarFill->SetVisibility(BarVisibility);
			if (DownedHealthBarFillMID)
			{
				DownedHealthBarFillMID->SetScalarParameterValue(TEXT("FillAmount"), ClampedPercent);
			}
			DownedHealthBarFill->SetDesiredSizeOverride(
				FVector2D(DownedHealthBarWidth * ClampedPercent, DownedHealthBarHeight));
		}

		if (IsValid(DownedHealthBarProgress))
		{
			DownedHealthBarProgress->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	if (IsValid(DownedHealthBarProgress))
	{
		DownedHealthBarProgress->SetVisibility(BarVisibility);
		if (!IsDesignTime())
		{
			DownedHealthBarProgress->SetPercent(ClampedPercent);
		}
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

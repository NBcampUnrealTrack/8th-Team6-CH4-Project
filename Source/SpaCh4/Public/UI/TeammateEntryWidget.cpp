#include "UI/TeammateEntryWidget.h"

#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
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

	/** Stale/deleted UObject* sentinels (e.g. 0xFFFFFFFFFFFFFFF7) must not reach IsValid(). */
	static bool IsUsableObjectPointer(const void* Ptr)
	{
		if (!Ptr)
		{
			return false;
		}

		const intptr_t Address = reinterpret_cast<intptr_t>(Ptr);
		// UE garbage/stale pointers are often small negative sentinels (e.g. -9).
		if (Address <= 0)
		{
			return false;
		}

		// Reject non-canonical user-space addresses on Win64.
		constexpr intptr_t MaxUserSpaceAddress = 0x00007FFFFFFFFFFFLL;
		return Address >= 0x10000 && Address <= MaxUserSpaceAddress;
	}

	template<typename T>
	T* GetValidObjectPtr(const TObjectPtr<T>& Ptr)
	{
		if (Ptr.IsNull())
		{
			return nullptr;
		}

		T* Raw = Ptr.Get();
		return (IsUsableObjectPointer(Raw) && IsValid(Raw)) ? Raw : nullptr;
	}

	static UTexture2D* GetValidTexture2D(const TObjectPtr<UTexture2D>& TexturePtr)
	{
		return GetValidObjectPtr<UTexture2D>(TexturePtr);
	}
}

using namespace TeammateEntryWidgetPrivate;

const USPGameHUDStyleData& UTeammateEntryWidget::GetResolvedStyle() const
{
	USPGameHUDStyleData* StyleOverride = GetValidObjectPtr<USPGameHUDStyleData>(VisualStyle);
	return SPUIStyleLibrary::ResolveGameHUDStyle(StyleOverride);
}

UTexture2D* UTeammateEntryWidget::ResolvePortraitTexture(ESurvivorDisplayState State) const
{
	const USPGameHUDStyleData& Style = GetResolvedStyle();

	switch (State)
	{
	case ESurvivorDisplayState::Injured:
	case ESurvivorDisplayState::Downed:
	case ESurvivorDisplayState::Carried:
		if (UTexture2D* InjuredIcon = GetValidTexture2D(PortraitIconInjured))
		{
			return InjuredIcon;
		}
		return GetValidTexture2D(Style.PortraitInjured);
	case ESurvivorDisplayState::Dead:
		if (UTexture2D* DeadIcon = GetValidTexture2D(PortraitIconDead))
		{
			return DeadIcon;
		}
		return GetValidTexture2D(Style.PortraitDead);
	case ESurvivorDisplayState::Caged:
		if (UTexture2D* CagedIcon = GetValidTexture2D(PortraitIconCaged))
		{
			return CagedIcon;
		}
		return GetValidTexture2D(Style.PortraitCaged);
	default:
		if (UTexture2D* HealthyIcon = GetValidTexture2D(PortraitIconHealthy))
		{
			return HealthyIcon;
		}
		return GetValidTexture2D(Style.PortraitHealthy);
	}
}

UTexture2D* UTeammateEntryWidget::ResolvePortraitSlotTexture() const
{
	if (UTexture2D* OverrideSlot = GetValidTexture2D(PortraitSlotTexture))
	{
		return OverrideSlot;
	}

	return GetValidTexture2D(GetResolvedStyle().PortraitSlotFrame);
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
	if (IsValid(DownedHealthBarProgress) && IsValid(DownedHealthBarFill))
	{
		DownedHealthBarFill->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UTeammateEntryWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	CollapseLegacyDownedHealthFillIfNeeded();
}

void UTeammateEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();

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
	else if (!IsValid(DownedHealthBarProgress))
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

	if (IsValid(DownedHealthBarProgress))
	{
		CollapseLegacyDownedHealthFillIfNeeded();

		if (!IsDesignTime())
		{
			DownedHealthBarProgress->SetPercent(ClampedPercent);
		}
		return;
	}

	if (IsValid(DownedHealthBarFill))
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

	UTexture2D* PortraitTex = ResolvePortraitTexture(State);
	if (PortraitTex != nullptr)
	{
		if (!IsValidDisplaySize(CachedPortraitIconSize))
		{
			CachePortraitLayoutSizes();
		}

		SetImageTextureWithDisplaySize(PortraitImage, PortraitTex, CachedPortraitIconSize);
		EnsurePortraitOverlayZOrder();
	}
}

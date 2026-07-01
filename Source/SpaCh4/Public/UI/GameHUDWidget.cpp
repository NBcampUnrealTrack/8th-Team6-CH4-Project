#include "UI/GameHUDWidget.h"

#include "Components/Image.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Systems/MatchGameState.h"
#include "UI/HUDFontUtils.h"
#include "UI/TeammateEntryWidget.h"

void UGameHUDWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	ApplyDeliveryRowLabels();
	SetupDeliveryProgressBars();
	RefreshDeliveryPanel();
}

void UGameHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ApplyDeliveryRowLabels();
	SetupDeliveryProgressBars();
	RefreshAll();
}

namespace
{
	UTexture2D* LoadDeliveryLabelTexture(const TCHAR* AssetName)
	{
		const FString Path = FString::Printf(
			TEXT("/Game/UI/HUD/Textures/Delivery/%s.%s"),
			AssetName,
			AssetName);
		return LoadObject<UTexture2D>(nullptr, *Path);
	}

	void ApplyDeliveryRowLabel(UImage* Icon, UTextBlock* Label, UTexture2D* LabelTexture)
	{
		if (!Icon || !LabelTexture)
		{
			return;
		}

		Icon->SetBrushFromTexture(LabelTexture, true);
		FSlateBrush Brush = Icon->GetBrush();
		Brush.ImageSize = FVector2D(40.f, 40.f);
		Icon->SetBrush(Brush);
		Icon->SetOpacity(1.0f);
		Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (Label)
		{
			Label->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	using SpaCh4HUD::SetBrushImageSize;

	static const TCHAR* DeliveryProgressFrameTexturePathA =
		TEXT("/Game/UI/HUD/Textures/Delivery/T_HUD_Delivery_Station_A.T_HUD_Delivery_Station_A");
	static const TCHAR* DeliveryProgressFrameTexturePathB =
		TEXT("/Game/UI/HUD/Textures/Delivery/T_HUD_Delivery_Station_B.T_HUD_Delivery_Station_B");
	static const TCHAR* DeliveryProgressFillMIPath =
		TEXT("/Game/UI/HUD/Materials/MI_HUD_DeliveryProgressFill.MI_HUD_DeliveryProgressFill");
	static const TCHAR* DeliveryProgressFillMaterialPath =
		TEXT("/Game/UI/HUD/Materials/M_HUD_HealthBarFill.M_HUD_HealthBarFill");
	static const TCHAR* DeliveryProgressFillTexturePath =
		TEXT("/Game/UI/HUD/Textures/Delivery/T_HUD_Bar_Fill_Delivery.T_HUD_Bar_Fill_Delivery");
	static const TCHAR* DeliveryProgressFillTextureFallbackPath =
		TEXT("/Game/UI/HUD/Textures/Common/T_HUD_Bar_Fill.T_HUD_Bar_Fill");

	/** Design reference size for SourceArt-trimmed station frames (420x76). */
	static constexpr float DeliveryCompositeWidth = 420.0f;
	static constexpr float DeliveryCompositeHeight = 76.0f;
	static constexpr float DeliveryProgressFillInsetLeft = 64.0f;
	static constexpr float DeliveryProgressFillInsetRight = 100.0f;
	static constexpr float DeliveryProgressFillInsetTop = 15.0f;
	static constexpr float DeliveryProgressFillInsetBottom = 14.0f;
	static constexpr float DeliveryProgressFillMaxWidth = 256.0f;
	static constexpr float DeliveryProgressFillHeight = 47.0f;

	struct FDeliveryProgressLayout
	{
		FVector2D FrameSize = FVector2D(DeliveryCompositeWidth, DeliveryCompositeHeight);
		FVector2D FillTranslation = FVector2D::ZeroVector;
		float FillMaxWidth = DeliveryProgressFillMaxWidth;
		float FillHeight = DeliveryProgressFillHeight;
	};

	static FVector2D GetDeliveryFrameSize(UImage* FrameImage)
	{
		if (!FrameImage)
		{
			return FVector2D(DeliveryCompositeWidth, DeliveryCompositeHeight);
		}

		const FSlateBrush& Brush = FrameImage->GetBrush();
		if (Brush.ImageSize.X > KINDA_SMALL_NUMBER && Brush.ImageSize.Y > KINDA_SMALL_NUMBER)
		{
			return Brush.ImageSize;
		}

		return FVector2D(DeliveryCompositeWidth, DeliveryCompositeHeight);
	}

	static FDeliveryProgressLayout BuildDeliveryProgressLayout(UImage* FrameImage)
	{
		FDeliveryProgressLayout Layout;
		Layout.FrameSize = GetDeliveryFrameSize(FrameImage);

		const float ScaleX = Layout.FrameSize.X / DeliveryCompositeWidth;
		const float ScaleY = Layout.FrameSize.Y / DeliveryCompositeHeight;
		Layout.FillTranslation = FVector2D(
			DeliveryProgressFillInsetLeft * ScaleX,
			DeliveryProgressFillInsetTop * ScaleY);
		Layout.FillMaxWidth = DeliveryProgressFillMaxWidth * ScaleX;
		Layout.FillHeight = DeliveryProgressFillHeight * ScaleY;
		return Layout;
	}

	static void SyncDeliveryFillBrushSize(UImage* FillImage, const FDeliveryProgressLayout& Layout)
	{
		if (!FillImage)
		{
			return;
		}

		FSlateBrush FillBrush = FillImage->GetBrush();
		SetBrushImageSize(
			FillBrush,
			FVector2D(Layout.FillMaxWidth, Layout.FillHeight));
		FillImage->SetBrush(FillBrush);
	}

	static void ApplyDeliveryFillOverlaySlot(UImage* FillImage, const FDeliveryProgressLayout& Layout)
	{
		if (!FillImage)
		{
			return;
		}

		FillImage->SetRenderTransformPivot(FVector2D(0.f, 0.f));
		FillImage->SetRenderTranslation(Layout.FillTranslation);

		if (UOverlaySlot* FillSlot = Cast<UOverlaySlot>(FillImage->Slot))
		{
			FillSlot->SetHorizontalAlignment(HAlign_Left);
			FillSlot->SetVerticalAlignment(VAlign_Top);
			FillSlot->SetPadding(FMargin(0.f));
		}
	}

	void SetupDeliveryProgressFillImage(
		UImage* FillImage,
		TObjectPtr<UMaterialInstanceDynamic>& FillMID,
		UObject* Outer,
		const FDeliveryProgressLayout& Layout)
	{
		if (!FillImage)
		{
			return;
		}

		FillImage->SetColorAndOpacity(FLinearColor::White);

		const FVector2D FillBrushSize(Layout.FillMaxWidth, Layout.FillHeight);

		if (FillMID)
		{
			FillImage->SetBrushFromMaterial(FillMID);
			SyncDeliveryFillBrushSize(FillImage, Layout);
			return;
		}

		UMaterialInterface* FillParent = LoadObject<UMaterialInterface>(nullptr, DeliveryProgressFillMIPath);
		if (!FillParent)
		{
			FillParent = LoadObject<UMaterialInterface>(nullptr, DeliveryProgressFillMaterialPath);
		}

		if (FillParent)
		{
			FillMID = UMaterialInstanceDynamic::Create(FillParent, Outer);
			if (FillMID)
			{
				if (UTexture2D* FillTexture = LoadObject<UTexture2D>(nullptr, DeliveryProgressFillTexturePath))
				{
					FillMID->SetTextureParameterValue(TEXT("FillTexture"), FillTexture);
				}
				else if (UTexture2D* FallbackTexture = LoadObject<UTexture2D>(nullptr, DeliveryProgressFillTextureFallbackPath))
				{
					FillMID->SetTextureParameterValue(TEXT("FillTexture"), FallbackTexture);
				}

				FillImage->SetBrushFromMaterial(FillMID);
				FSlateBrush FillBrush = FillImage->GetBrush();
				SetBrushImageSize(FillBrush, FillBrushSize);
				FillImage->SetBrush(FillBrush);
				return;
			}
		}

		if (UTexture2D* FillTexture = LoadObject<UTexture2D>(nullptr, DeliveryProgressFillTexturePath))
		{
			FillImage->SetBrushFromTexture(FillTexture, true);
			FSlateBrush FillBrush = FillImage->GetBrush();
			SetBrushImageSize(FillBrush, FillBrushSize);
			FillImage->SetBrush(FillBrush);
		}
	}

	void UpdateDeliveryPercentLabel(UTextBlock* Label, int32 CurrentValue, int32 TargetValue)
	{
		if (!Label || TargetValue <= 0)
		{
			return;
		}

		const int32 Percent = FMath::Clamp(
			FMath::RoundToInt(static_cast<float>(CurrentValue) / static_cast<float>(TargetValue) * 100.0f),
			0,
			100);
		Label->SetText(FText::Format(
			NSLOCTEXT("SpaCh4", "DeliveryPercentFormat", "{0}%"),
			FText::AsNumber(Percent)));
		Label->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void UGameHUDWidget::ApplyDeliveryRowLabels()
{
	if (DeliveryIconA)
	{
		DeliveryIconA->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (DeliveryIconB)
	{
		DeliveryIconB->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (DeliveryLabelA)
	{
		DeliveryLabelA->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (DeliveryLabelB)
	{
		DeliveryLabelB->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UGameHUDWidget::RefreshAll()
{
	RefreshTeammateEntries();
	RefreshDeliveryPanel();
	RefreshInventoryPanel();
	RefreshPerkPanel();
	OnDeliveryDataUpdated(GatherDeliveryData());
	OnInventoryDataUpdated(GatherInventoryData());
	OnPerkDataUpdated(GatherPerkData());
}

TArray<FTeammateHUDData> UGameHUDWidget::GatherTeammateData() const
{
	if (bUsePreviewData)
	{
		const_cast<UGameHUDWidget*>(this)->EnsurePreviewDefaults();
		return PreviewTeammateData;
	}

	return BuildTeammateDataFromGameplay();
}

TArray<FTeammateHUDData> UGameHUDWidget::BuildTeammateDataFromGameplay_Implementation() const
{
	return TArray<FTeammateHUDData>();
}

TArray<FDeliveryHUDData> UGameHUDWidget::GatherDeliveryData() const
{
	TArray<FDeliveryHUDData> Result;

	const AMatchGameState* MatchGS = GetMatchGameState();
	if (!MatchGS)
	{
		if (bUsePreviewData)
		{
			FDeliveryHUDData StationA;
			StationA.StationId = FName(TEXT("A"));
			StationA.CurrentValue = 0;
			StationA.TargetValue = 200;
			Result.Add(StationA);

			FDeliveryHUDData StationB;
			StationB.StationId = FName(TEXT("B"));
			StationB.CurrentValue = 0;
			StationB.TargetValue = 200;
			Result.Add(StationB);
		}
		return Result;
	}

	FDeliveryHUDData StationA;
	StationA.StationId = FName(TEXT("A"));
	StationA.CurrentValue = MatchGS->GetDeliveryStationAValue();
	StationA.TargetValue = MatchGS->GetDeliveryStationTargetValue();
	Result.Add(StationA);

	FDeliveryHUDData StationB;
	StationB.StationId = FName(TEXT("B"));
	StationB.CurrentValue = MatchGS->GetDeliveryStationBValue();
	StationB.TargetValue = MatchGS->GetDeliveryStationTargetValue();
	Result.Add(StationB);

	return Result;
}

TArray<FInventorySlotHUDData> UGameHUDWidget::GatherInventoryData() const
{
	if (bUsePreviewData && PreviewInventoryData.Num() > 0)
	{
		return PreviewInventoryData;
	}

	TArray<FInventorySlotHUDData> Result;
	Result.SetNum(SpaCh4HUD::InventorySlotCount);
	return Result;
}

TArray<FPerkHUDData> UGameHUDWidget::GatherPerkData() const
{
	if (bUsePreviewData && PreviewPerkData.Num() > 0)
	{
		return PreviewPerkData;
	}

	TArray<FPerkHUDData> Result;
	Result.SetNum(SpaCh4HUD::PerkSlotCount);
	return Result;
}

void UGameHUDWidget::RefreshTeammateEntries()
{
	const TArray<FTeammateHUDData> TeammateData = GatherTeammateData();

	TArray<UTeammateEntryWidget*> Entries;
	Entries.Reserve(SpaCh4HUD::TeammateSlotCount);

	for (int32 Index = 0; Index < SpaCh4HUD::TeammateSlotCount; ++Index)
	{
		const FName WidgetName(*FString::Printf(TEXT("TeammateEntries_%d"), Index));
		if (UTeammateEntryWidget* Entry = Cast<UTeammateEntryWidget>(GetWidgetFromName(WidgetName)))
		{
			Entries.Add(Entry);
		}
	}

	if (Entries.Num() == 0)
	{
		for (UTeammateEntryWidget* Entry : TeammateEntries)
		{
			if (Entry)
			{
				Entries.Add(Entry);
			}
		}
	}

	for (int32 Index = 0; Index < SpaCh4HUD::TeammateSlotCount; ++Index)
	{
		UTeammateEntryWidget* Entry = Entries.IsValidIndex(Index) ? Entries[Index] : nullptr;
		if (!Entry)
		{
			continue;
		}

		if (TeammateData.IsValidIndex(Index))
		{
			Entry->SetVisibility(ESlateVisibility::HitTestInvisible);
			Entry->UpdateFromData(TeammateData[Index]);
		}
		else
		{
			Entry->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UGameHUDWidget::EnsurePreviewDefaults()
{
	static const ESurvivorDisplayState ExpectedStates[] = {
		ESurvivorDisplayState::Healthy,
		ESurvivorDisplayState::Injured,
		ESurvivorDisplayState::Dead,
	};
	static constexpr int32 ExpectedPreviewCount = UE_ARRAY_COUNT(ExpectedStates);

	bool bHasCanonicalPreview = PreviewTeammateData.Num() == ExpectedPreviewCount;
	if (bHasCanonicalPreview)
	{
		for (int32 PreviewIndex = 0; PreviewIndex < ExpectedPreviewCount; ++PreviewIndex)
		{
			if (PreviewTeammateData[PreviewIndex].DisplayState != ExpectedStates[PreviewIndex])
			{
				bHasCanonicalPreview = false;
				break;
			}
		}
	}

	if (bHasCanonicalPreview)
	{
		return;
	}

	PreviewTeammateData.Reset();

	FTeammateHUDData TeammateA;
	TeammateA.Nickname = NSLOCTEXT("SpaCh4", "PreviewTeammateA", "Player_A");
	TeammateA.CageStack = 0;
	TeammateA.DisplayState = ESurvivorDisplayState::Healthy;
	PreviewTeammateData.Add(TeammateA);

	FTeammateHUDData TeammateB;
	TeammateB.Nickname = NSLOCTEXT("SpaCh4", "PreviewTeammateB", "Player_B");
	TeammateB.CageStack = 1;
	TeammateB.DisplayState = ESurvivorDisplayState::Injured;
	PreviewTeammateData.Add(TeammateB);

	FTeammateHUDData TeammateC;
	TeammateC.Nickname = NSLOCTEXT("SpaCh4", "PreviewTeammateC", "Player_C");
	TeammateC.CageStack = 2;
	TeammateC.DisplayState = ESurvivorDisplayState::Dead;
	PreviewTeammateData.Add(TeammateC);
}

namespace
{
	UTexture2D* LoadDeliveryProgressTexture(int32 FilledSegments)
	{
		const int32 Clamped = FMath::Clamp(FilledSegments, 0, SpaCh4HUD::DeliveryProgressSegmentCount);
		const FString Path = FString::Printf(
			TEXT("/Game/UI/HUD/Textures/Delivery/T_HUD_Delivery_Progress_%02d.T_HUD_Delivery_Progress_%02d"),
			Clamped,
			Clamped);

		if (UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *Path))
		{
			return Texture;
		}

		static UTexture2D* FallbackEmpty = LoadObject<UTexture2D>(
			nullptr,
			TEXT("/Game/UI/HUD/Textures/Delivery/T_HUD_Delivery_Progress_00.T_HUD_Delivery_Progress_00"));
		if (!FallbackEmpty)
		{
			FallbackEmpty = LoadObject<UTexture2D>(
				nullptr,
				TEXT("/Game/UI/HUD/Textures/Delivery/T_HUD_Delivery_Progress_BG.T_HUD_Delivery_Progress_BG"));
		}
		return FallbackEmpty;
	}
}

void UGameHUDWidget::SetupDeliveryProgressBars()
{
	auto SetupRow = [](
		UImage* FrameImage,
		UImage* FillImage,
		const TCHAR* FrameTexturePath,
		TObjectPtr<UMaterialInstanceDynamic>& FillMID,
		UGameHUDWidget* Outer)
	{
		if (FrameImage)
		{
			FrameImage->SetColorAndOpacity(FLinearColor::White);
			if (FrameImage->GetBrush().GetResourceObject() == nullptr)
			{
				if (UTexture2D* FrameTexture = LoadObject<UTexture2D>(nullptr, FrameTexturePath))
				{
					FrameImage->SetBrushFromTexture(FrameTexture, true);
				}
			}
		}

		const FDeliveryProgressLayout Layout = BuildDeliveryProgressLayout(FrameImage);
		SetupDeliveryProgressFillImage(FillImage, FillMID, Outer, Layout);
		ApplyDeliveryFillOverlaySlot(FillImage, Layout);
	};

	// Legacy single image: use as frame only when overlay fill exists separately.
	if (!DeliveryProgressFillA && DeliveryProgressBarA && DeliveryProgressFillMIDA)
	{
		DeliveryProgressFillMIDA = nullptr;
	}
	if (!DeliveryProgressFillB && DeliveryProgressBarB && DeliveryProgressFillMIDB)
	{
		DeliveryProgressFillMIDB = nullptr;
	}

	if (!DeliveryProgressFillA && DeliveryProgressBarA)
	{
		if (UTexture2D* FrameTexture = LoadObject<UTexture2D>(nullptr, DeliveryProgressFrameTexturePathA))
		{
			DeliveryProgressBarA->SetBrushFromTexture(FrameTexture, true);
		}
	}
	if (!DeliveryProgressFillB && DeliveryProgressBarB)
	{
		if (UTexture2D* FrameTexture = LoadObject<UTexture2D>(nullptr, DeliveryProgressFrameTexturePathB))
		{
			DeliveryProgressBarB->SetBrushFromTexture(FrameTexture, true);
		}
	}

	SetupRow(
		DeliveryProgressBGA,
		DeliveryProgressFillA,
		DeliveryProgressFrameTexturePathA,
		DeliveryProgressFillMIDA,
		this);
	SetupRow(
		DeliveryProgressBGB,
		DeliveryProgressFillB,
		DeliveryProgressFrameTexturePathB,
		DeliveryProgressFillMIDB,
		this);
}

void UGameHUDWidget::UpdateDeliveryProgress(
	UWidget* Root,
	UImage* FrameImage,
	UImage* FillImage,
	UImage* LegacyBar,
	const TArray<TObjectPtr<UImage>>& StackWidgets,
	int32 CurrentValue,
	int32 TargetValue)
{
	if (TargetValue <= 0)
	{
		return;
	}

	const int32 SegmentCount = StackWidgets.Num() > 0
		? StackWidgets.Num()
		: SpaCh4HUD::DeliveryProgressSegmentCount;

	const float ProgressPercent = FMath::Clamp(
		static_cast<float>(CurrentValue) / static_cast<float>(TargetValue),
		0.0f,
		1.0f);

	const int32 FilledSegments = FMath::Clamp(
		FMath::RoundToInt(ProgressPercent * SegmentCount),
		0,
		SegmentCount);

	if (FillImage && FrameImage)
	{
		const FDeliveryProgressLayout Layout = BuildDeliveryProgressLayout(FrameImage);

		if (FillImage == DeliveryProgressFillA)
		{
			if (!DeliveryProgressFillMIDA)
			{
				SetupDeliveryProgressFillImage(FillImage, DeliveryProgressFillMIDA, this, Layout);
			}
			else
			{
				FillImage->SetBrushFromMaterial(DeliveryProgressFillMIDA);
				SyncDeliveryFillBrushSize(FillImage, Layout);
			}
		}
		else if (FillImage == DeliveryProgressFillB)
		{
			if (!DeliveryProgressFillMIDB)
			{
				SetupDeliveryProgressFillImage(FillImage, DeliveryProgressFillMIDB, this, Layout);
			}
			else
			{
				FillImage->SetBrushFromMaterial(DeliveryProgressFillMIDB);
				SyncDeliveryFillBrushSize(FillImage, Layout);
			}
		}

		if (Root)
		{
			Root->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			FrameImage->SetVisibility(ESlateVisibility::HitTestInvisible);
			FillImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		}

		if (LegacyBar)
		{
			LegacyBar->SetVisibility(ESlateVisibility::Collapsed);
		}

		ApplyDeliveryFillOverlaySlot(FillImage, Layout);

		const float FillWidth = Layout.FillMaxWidth * ProgressPercent;
		FillImage->SetDesiredSizeOverride(FVector2D(FillWidth, Layout.FillHeight));
		FrameImage->SetVisibility(ESlateVisibility::HitTestInvisible);

		for (UImage* SegmentImage : StackWidgets)
		{
			if (SegmentImage)
			{
				SegmentImage->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
		return;
	}

	if (LegacyBar)
	{
		const bool bLegacyUsesMaterial =
			(LegacyBar == DeliveryProgressBarA && DeliveryProgressFillMIDA)
			|| (LegacyBar == DeliveryProgressBarB && DeliveryProgressFillMIDB);

		if (bLegacyUsesMaterial)
		{
			const TCHAR* FrameTexturePath =
				(LegacyBar == DeliveryProgressBarA)
					? DeliveryProgressFrameTexturePathA
					: DeliveryProgressFrameTexturePathB;
			if (UTexture2D* FrameTexture = LoadObject<UTexture2D>(nullptr, FrameTexturePath))
			{
				LegacyBar->SetBrushFromTexture(FrameTexture, true);
			}
			LegacyBar->SetDesiredSizeOverride(
				FVector2D(DeliveryCompositeWidth, DeliveryCompositeHeight));
			LegacyBar->SetOpacity(1.0f);
			LegacyBar->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else if (UTexture2D* ProgressTexture = LoadDeliveryProgressTexture(FilledSegments))
		{
			LegacyBar->SetBrushFromTexture(ProgressTexture, true);
			LegacyBar->SetOpacity(1.0f);
			LegacyBar->SetVisibility(ESlateVisibility::HitTestInvisible);
		}

		for (UImage* SegmentImage : StackWidgets)
		{
			if (SegmentImage)
			{
				SegmentImage->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
		return;
	}

	if (StackWidgets.Num() == 0)
	{
		return;
	}

	static UTexture2D* EmptyTexture = LoadObject<UTexture2D>(
		nullptr,
		TEXT("/Game/UI/HUD/Textures/Delivery/T_HUD_Delivery_Stack_Empty.T_HUD_Delivery_Stack_Empty"));
	static UTexture2D* FilledTexture = LoadObject<UTexture2D>(
		nullptr,
		TEXT("/Game/UI/HUD/Textures/Delivery/T_HUD_Delivery_Stack_Filled.T_HUD_Delivery_Stack_Filled"));
	if (!EmptyTexture)
	{
		EmptyTexture = LoadObject<UTexture2D>(
			nullptr,
			TEXT("/Game/UI/HUD/Textures/Delivery/T_HUD_Delivery_Progress_BG.T_HUD_Delivery_Progress_BG"));
	}
	if (!FilledTexture)
	{
		FilledTexture = LoadObject<UTexture2D>(
			nullptr,
			TEXT("/Game/UI/HUD/Textures/Common/T_HUD_Bar_Fill.T_HUD_Bar_Fill"));
	}

	for (int32 Index = 0; Index < StackWidgets.Num(); ++Index)
	{
		UImage* SegmentImage = StackWidgets[Index];
		if (!SegmentImage)
		{
			continue;
		}

		const bool bFilled = Index < FilledSegments;
		UTexture2D* SegmentTexture = bFilled ? FilledTexture : EmptyTexture;
		if (!SegmentTexture)
		{
			SegmentImage->SetVisibility(ESlateVisibility::Collapsed);
			continue;
		}

		SegmentImage->SetBrushFromTexture(SegmentTexture, true);
		SegmentImage->SetOpacity(1.0f);
		SegmentImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void UGameHUDWidget::RefreshDeliveryPanel()
{
	ApplyDeliveryRowLabels();

	const TArray<FDeliveryHUDData> DeliveryData = GatherDeliveryData();

	if (DeliveryProgressA)
	{
		DeliveryProgressA->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (DeliveryProgressB)
	{
		DeliveryProgressB->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (DeliveryData.IsValidIndex(0))
	{
		const FDeliveryHUDData& StationA = DeliveryData[0];
		int32 CurrentA = StationA.CurrentValue;
		const int32 TargetA = StationA.TargetValue;

		if (bUsePreviewData && CurrentA == 0 && TargetA > 0)
		{
			CurrentA = TargetA / 3;
		}

		UpdateDeliveryProgress(
			DeliveryProgressRootA,
			DeliveryProgressBGA,
			DeliveryProgressFillA,
			DeliveryProgressBarA,
			DeliveryStackA,
			CurrentA,
			TargetA);
		UpdateDeliveryPercentLabel(DeliveryValueA, CurrentA, TargetA);
	}

	if (DeliveryData.IsValidIndex(1))
	{
		const FDeliveryHUDData& StationB = DeliveryData[1];
		int32 CurrentB = StationB.CurrentValue;
		const int32 TargetB = StationB.TargetValue;

		if (bUsePreviewData && CurrentB == 0 && TargetB > 0)
		{
			CurrentB = (TargetB * 2) / 3;
		}

		UpdateDeliveryProgress(
			DeliveryProgressRootB,
			DeliveryProgressBGB,
			DeliveryProgressFillB,
			DeliveryProgressBarB,
			DeliveryStackB,
			CurrentB,
			TargetB);
		UpdateDeliveryPercentLabel(DeliveryValueB, CurrentB, TargetB);
	}
}

void UGameHUDWidget::RefreshInventoryPanel()
{
	const TArray<FInventorySlotHUDData> InventoryData = GatherInventoryData();

	for (int32 Index = 0; Index < InventorySlots.Num(); ++Index)
	{
		if (!InventorySlots[Index])
		{
			continue;
		}

		const bool bOccupied = InventoryData.IsValidIndex(Index) && InventoryData[Index].bIsOccupied;
		InventorySlots[Index]->SetOpacity(bOccupied ? 1.0f : 0.45f);
	}
}

void UGameHUDWidget::RefreshPerkPanel()
{
	const TArray<FPerkHUDData> PerkData = GatherPerkData();

	for (int32 Index = 0; Index < PerkSlots.Num(); ++Index)
	{
		if (!PerkSlots[Index])
		{
			continue;
		}

		const bool bEquipped = PerkData.IsValidIndex(Index) && PerkData[Index].bIsEquipped;
		PerkSlots[Index]->SetOpacity(bEquipped ? 1.0f : 0.45f);
	}
}

AMatchGameState* UGameHUDWidget::GetMatchGameState() const
{
	return GetWorld() ? GetWorld()->GetGameState<AMatchGameState>() : nullptr;
}

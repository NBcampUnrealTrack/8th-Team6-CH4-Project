#include "UI/GameHUDWidget.h"

#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "UI/HUDFontUtils.h"
#include "Systems/MatchGameState.h"
#include "UI/TeammateEntryWidget.h"

void UGameHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ApplyHUDFonts();
	ApplyDeliveryRowLabels();
}

void UGameHUDWidget::ApplyHUDFonts()
{
	static UFontFace* SemiBold = SpaCh4HUD::LoadFontFace(SpaCh4HUD::FontSemiBoldPath);
	SpaCh4HUD::ApplyTextFont(DeliveryLabelA, SemiBold, 30);
	SpaCh4HUD::ApplyTextFont(DeliveryLabelB, SemiBold, 30);
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
}

void UGameHUDWidget::ApplyDeliveryRowLabels()
{
	UTexture2D* LabelA = LoadDeliveryLabelTexture(TEXT("T_HUD_Delivery_Label_A"));
	UTexture2D* LabelB = LoadDeliveryLabelTexture(TEXT("T_HUD_Delivery_Label_B"));

	ApplyDeliveryRowLabel(DeliveryIconA, DeliveryLabelA, LabelA);
	ApplyDeliveryRowLabel(DeliveryIconB, DeliveryLabelB, LabelB);
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

	for (int32 Index = 0; Index < TeammateEntries.Num(); ++Index)
	{
		if (!TeammateEntries[Index])
		{
			continue;
		}

		if (TeammateData.IsValidIndex(Index))
		{
			TeammateEntries[Index]->SetVisibility(ESlateVisibility::HitTestInvisible);
			TeammateEntries[Index]->UpdateFromData(TeammateData[Index]);
		}
		else
		{
			TeammateEntries[Index]->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UGameHUDWidget::EnsurePreviewDefaults()
{
	if (PreviewTeammateData.Num() > 0)
	{
		return;
	}

	FTeammateHUDData TeammateA;
	TeammateA.Nickname = NSLOCTEXT("SpaCh4", "PreviewTeammateA", "Player_A");
	TeammateA.CageStack = 0;
	TeammateA.DisplayState = ESurvivorDisplayState::Healthy;
	PreviewTeammateData.Add(TeammateA);

	FTeammateHUDData TeammateB;
	TeammateB.Nickname = NSLOCTEXT("SpaCh4", "PreviewTeammateB", "Player_B");
	TeammateB.CageStack = 1;
	TeammateB.DisplayState = ESurvivorDisplayState::Downed;
	TeammateB.DownedHealthPercent = 0.65f;
	PreviewTeammateData.Add(TeammateB);

	FTeammateHUDData TeammateC;
	TeammateC.Nickname = NSLOCTEXT("SpaCh4", "PreviewTeammateC", "Player_C");
	TeammateC.CageStack = 2;
	TeammateC.DisplayState = ESurvivorDisplayState::Caged;
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

void UGameHUDWidget::UpdateDeliveryProgress(
	UImage* ProgressBar,
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

	const int32 FilledSegments = FMath::Clamp(
		FMath::RoundToInt(static_cast<float>(CurrentValue) / static_cast<float>(TargetValue) * SegmentCount),
		0,
		SegmentCount);

	if (ProgressBar)
	{
		if (UTexture2D* ProgressTexture = LoadDeliveryProgressTexture(FilledSegments))
		{
			ProgressBar->SetBrushFromTexture(ProgressTexture, true);
			ProgressBar->SetOpacity(1.0f);
			ProgressBar->SetVisibility(ESlateVisibility::HitTestInvisible);
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

	if (DeliveryValueA)
	{
		DeliveryValueA->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (DeliveryValueB)
	{
		DeliveryValueB->SetVisibility(ESlateVisibility::Collapsed);
	}
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

		UpdateDeliveryProgress(DeliveryProgressBarA, DeliveryStackA, CurrentA, TargetA);
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

		UpdateDeliveryProgress(DeliveryProgressBarB, DeliveryStackB, CurrentB, TargetB);
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

#include "UI/GameHUDWidget.h"

#include "Characters/Survivor/SurvivorCharacter.h"
#include "Components/Image.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Systems/MatchGameState.h"
#include "Inventory/SPInventoryComponent.h"
#include "UI/HUDFontUtils.h"
#include "UI/Style/SPUIStyleData.h"
#include "UI/Style/SPUIStyleLibrary.h"
#include "UI/TeammateEntryWidget.h"
#include "TimerManager.h"
#include "Characters/Survivor/SurvivorCharacter.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Systems/MatchGameState.h"

namespace GameHUDWidgetPrivate
{
	static bool HasDesignerBrushResource(const UImage* Image)
	{
		return IsValid(Image) && Image->GetBrush().GetResourceObject() != nullptr;
	}

	static FVector2D ReadFillBrushImageSize(const UImage* Image)
	{
		if (!HasDesignerBrushResource(Image))
		{
			return FVector2D::ZeroVector;
		}

		const FVector2D BrushSize = Image->GetBrush().ImageSize;
		if (BrushSize.X > KINDA_SMALL_NUMBER && BrushSize.Y > KINDA_SMALL_NUMBER)
		{
			return BrushSize;
		}

		return FVector2D::ZeroVector;
	}

	static constexpr float DeliveryFillDefaultWidth = 256.0f;
	static constexpr float DeliveryFillDefaultHeight = 47.0f;

	static ESurvivorDisplayState ToDisplayState(ESurvivorState State)
	{
		switch (State)
		{
		case ESurvivorState::Injured:
			return ESurvivorDisplayState::Injured;
		case ESurvivorState::Downed:
			return ESurvivorDisplayState::Downed;
		case ESurvivorState::Carried:
			return ESurvivorDisplayState::Carried;
		case ESurvivorState::Caged:
			return ESurvivorDisplayState::Caged;
		case ESurvivorState::Dead:
			return ESurvivorDisplayState::Dead;
		case ESurvivorState::Escaped:
			return ESurvivorDisplayState::Escaped;
		default:
			return ESurvivorDisplayState::Healthy;
		}
	}

	static const ASurvivorCharacter* FindSurvivorByNickname(const UWorld* World, const FString& Nickname)
	{
		if (!World || Nickname.IsEmpty())
		{
			return nullptr;
		}

		for (TActorIterator<ASurvivorCharacter> It(World); It; ++It)
		{
			const ASurvivorCharacter* Survivor = *It;
			if (!IsValid(Survivor))
			{
				continue;
			}

			const APlayerState* PS = Survivor->GetPlayerState();
			if (PS && PS->GetPlayerName() == Nickname)
			{
				return Survivor;
			}
		}

		return nullptr;
	}
}

const USPGameHUDStyleData& UGameHUDWidget::GetResolvedStyle() const
{
	return SPUIStyleLibrary::ResolveGameHUDStyle(VisualStyle);
}

const USPGameHUDStyleData& UGameHUDWidget::GetResolvedStyle() const
{
	return SPUIStyleLibrary::ResolveGameHUDStyle(VisualStyle);
}

void UGameHUDWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	CacheDesignerDeliveryFillSizes();
	ApplyDeliveryRowLabels();
	if (IsDesignTime())
	{
		if (!IsValid(DeliveryProgressA) && !IsValid(DeliveryProgressB))
		{
			SetupDeliveryProgressBars();
			RefreshDeliveryPanel();
		}
	}
}

void UGameHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BindInventoryWidgets();

	ClearDeliveryProgressSetupTimer();
	DeliveryProgressFillMIDA = nullptr;
	DeliveryProgressFillMIDB = nullptr;
	CacheDesignerDeliveryFillSizes();

	ApplyDeliveryRowLabels();

	if (IsDesignTime())
	{
		RefreshAll();
		return;
	}

	if (CanRunDeferredSetup())
	{
		GetWorld()->GetTimerManager().SetTimer(
			DeliveryProgressSetupTimerHandle,
			this,
			&UGameHUDWidget::FinalizeDeliveryProgressSetup,
			0.0f,
			false);
	}
	else
	{
		FinalizeDeliveryProgressSetup();
	}
}

void UGameHUDWidget::NativeDestruct()
{
	UnbindMatchStateDelegates();
	ClearDeliveryProgressSetupTimer();
	DeliveryProgressFillMIDA = nullptr;
	DeliveryProgressFillMIDB = nullptr;
	Super::NativeDestruct();
}

void UGameHUDWidget::BindMatchStateDelegates()
{
	if (bMatchDelegatesBound)
	{
		return;
	}

	AMatchGameState* MatchGS = GetMatchGameState();
	if (!MatchGS)
	{
		return;
	}

	MatchGS->OnDeliveryProgressChanged.AddDynamic(this, &UGameHUDWidget::HandleDeliveryProgressChanged);
	MatchGS->OnMatchPlayersChanged.AddDynamic(this, &UGameHUDWidget::HandleMatchPlayersChanged);
	MatchGS->OnSurvivorStateChanged.AddDynamic(this, &UGameHUDWidget::HandleSurvivorStateChanged);
	bMatchDelegatesBound = true;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			TeammateRefreshTimerHandle,
			this,
			&UGameHUDWidget::RefreshTeammateEntries,
			0.2f,
			true);
	}
}

void UGameHUDWidget::UnbindMatchStateDelegates()
{
	if (!bMatchDelegatesBound)
	{
		return;
	}

	if (AMatchGameState* MatchGS = GetMatchGameState())
	{
		MatchGS->OnDeliveryProgressChanged.RemoveDynamic(this, &UGameHUDWidget::HandleDeliveryProgressChanged);
		MatchGS->OnMatchPlayersChanged.RemoveDynamic(this, &UGameHUDWidget::HandleMatchPlayersChanged);
		MatchGS->OnSurvivorStateChanged.RemoveDynamic(this, &UGameHUDWidget::HandleSurvivorStateChanged);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TeammateRefreshTimerHandle);
	}

	bMatchDelegatesBound = false;
}

void UGameHUDWidget::HandleDeliveryProgressChanged(
	int32 StationAValue,
	int32 StationBValue,
	int32 TotalDeliveredValue,
	float DeliveryProgress)
{
	(void)StationAValue;
	(void)StationBValue;
	(void)TotalDeliveredValue;
	(void)DeliveryProgress;
	RefreshDeliveryPanel();
}

void UGameHUDWidget::HandleMatchPlayersChanged()
{
	RefreshTeammateEntries();
}

void UGameHUDWidget::HandleSurvivorStateChanged(FName SurvivorId, ESurvivorState SurvivorState)
{
	(void)SurvivorId;
	(void)SurvivorState;
	RefreshTeammateEntries();
}

bool UGameHUDWidget::CanRunDeferredSetup() const
{
	const UWorld* World = GetWorld();
	return IsValid(this) && World && !World->bIsTearingDown;
}

void UGameHUDWidget::ClearDeliveryProgressSetupTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DeliveryProgressSetupTimerHandle);
	}
}

void UGameHUDWidget::FinalizeDeliveryProgressSetup()
{
	if (!CanRunDeferredSetup())
	{
		ClearDeliveryProgressSetupTimer();
		return;
	}

	ClearDeliveryProgressSetupTimer();
	CacheDesignerDeliveryFillSizes();
	BindMatchStateDelegates();
	SetupDeliveryProgressBars();
	RefreshAll();
}

void UGameHUDWidget::CacheDesignerDeliveryFillSizes()
{
	CachedDeliveryFillMaxSizeA = FVector2D::ZeroVector;
	CachedDeliveryFillMaxSizeB = FVector2D::ZeroVector;

	CachedDeliveryFillMaxSizeA = GameHUDWidgetPrivate::ReadFillBrushImageSize(DeliveryProgressFillA);
	CachedDeliveryFillMaxSizeB = GameHUDWidgetPrivate::ReadFillBrushImageSize(DeliveryProgressFillB);
}

FVector2D UGameHUDWidget::ResolveDeliveryFillMaxSize(
	UImage* FillImage,
	const FVector2D& FallbackMaxSize) const
{
	if (DeliveryFillMaxSizeOverride.X > KINDA_SMALL_NUMBER
		&& DeliveryFillMaxSizeOverride.Y > KINDA_SMALL_NUMBER)
	{
		return DeliveryFillMaxSizeOverride;
	}

	if (const FVector2D LiveSize = GameHUDWidgetPrivate::ReadFillBrushImageSize(FillImage);
		LiveSize.X > KINDA_SMALL_NUMBER)
	{
		return LiveSize;
	}

	if (FillImage == DeliveryProgressFillA && CachedDeliveryFillMaxSizeA.X > KINDA_SMALL_NUMBER)
	{
		return CachedDeliveryFillMaxSizeA;
	}

	if (FillImage == DeliveryProgressFillB && CachedDeliveryFillMaxSizeB.X > KINDA_SMALL_NUMBER)
	{
		return CachedDeliveryFillMaxSizeB;
	}

	// 한쪽 Fill만 브러시 크기를 읽지 못한 경우 형제 Fill과 동일 크기 사용 (A/B WBP 동일 설정)
	if (FillImage == DeliveryProgressFillA)
	{
		if (CachedDeliveryFillMaxSizeB.X > KINDA_SMALL_NUMBER)
		{
			return CachedDeliveryFillMaxSizeB;
		}

		if (const FVector2D SiblingSize = GameHUDWidgetPrivate::ReadFillBrushImageSize(DeliveryProgressFillB);
			SiblingSize.X > KINDA_SMALL_NUMBER)
		{
			return SiblingSize;
		}
	}
	else if (FillImage == DeliveryProgressFillB)
	{
		if (CachedDeliveryFillMaxSizeA.X > KINDA_SMALL_NUMBER)
		{
			return CachedDeliveryFillMaxSizeA;
		}

		if (const FVector2D SiblingSize = GameHUDWidgetPrivate::ReadFillBrushImageSize(DeliveryProgressFillA);
			SiblingSize.X > KINDA_SMALL_NUMBER)
		{
			return SiblingSize;
		}
	}

	return FallbackMaxSize;
}

namespace
{
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

	static bool HasDesignerBrushResource(const UImage* Image)
	{
		return GameHUDWidgetPrivate::HasDesignerBrushResource(Image);
	}

	static FVector2D ResolveFillDisplaySize(const UImage* FillImage, const FDeliveryProgressLayout& Layout)
	{
		if (IsValid(FillImage))
		{
			// DesiredSize는 진행률 업데이트로 매 프레임 바뀌므로, 디자이너 기준값은 Brush.ImageSize만 사용.
			const FSlateBrush& Brush = FillImage->GetBrush();
			if (Brush.ImageSize.X > KINDA_SMALL_NUMBER && Brush.ImageSize.Y > KINDA_SMALL_NUMBER)
			{
				return Brush.ImageSize;
			}
		}

		return FVector2D(Layout.FillMaxWidth, Layout.FillHeight);
	}

	static void ApplyDesignerFrameTextureIfNeeded(UImage* FrameImage, UTexture2D* FrameTexture)
	{
		if (!IsValid(FrameImage) || HasDesignerBrushResource(FrameImage))
		{
			return;
		}

		if (FrameTexture && IsValid(FrameTexture))
		{
			FrameTexture->ConditionalPostLoad();
			FrameImage->SetBrushFromTexture(FrameTexture, true);
		}
	}

	static void SyncDeliveryFillBrushSize(UImage* FillImage, const FDeliveryProgressLayout& Layout)
	{
		if (!FillImage)
		{
			return;
		}

		FillImage->SetDesiredSizeOverride(ResolveFillDisplaySize(FillImage, Layout));
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
		const FDeliveryProgressLayout& Layout,
		const USPGameHUDStyleData& Style)
	{
		if (!IsValid(FillImage))
		{
			return;
		}

		const FVector2D FillBrushSize = ResolveFillDisplaySize(FillImage, Layout);

		// WBP/디자이너에 지정된 브러시·머티리얼·Image Size가 있으면 C++가 덮어쓰지 않음.
		if (HasDesignerBrushResource(FillImage))
		{
			return;
		}

		FillImage->SetColorAndOpacity(FLinearColor::White);

		if (IsValid(FillMID))
		{
			FillImage->SetBrushFromMaterial(FillMID);
			FillImage->SetDesiredSizeOverride(FillBrushSize);
			return;
		}

		UMaterialInterface* FillParent = Style.DeliveryProgressFillMaterialInstance;
		if (!IsValid(FillParent))
		{
			FillParent = Style.DeliveryProgressFillMaterial;
		}

		if (IsValid(FillParent))
		{
			FillParent->ConditionalPostLoad();
			UObject* MIDOuter = FillImage;
			if (!IsValid(MIDOuter))
			{
				MIDOuter = Outer;
			}

			FillMID = UMaterialInstanceDynamic::Create(FillParent, MIDOuter);
			if (IsValid(FillMID))
			{
				if (UTexture2D* FillTexture = Style.DeliveryProgressFillTexture)
				{
					if (IsValid(FillTexture))
					{
						FillTexture->ConditionalPostLoad();
						FillMID->SetTextureParameterValue(TEXT("FillTexture"), FillTexture);
					}
				}
				else if (UTexture2D* FallbackTexture = Style.DeliveryProgressFillTextureFallback)
				{
					if (IsValid(FallbackTexture))
					{
						FallbackTexture->ConditionalPostLoad();
						FillMID->SetTextureParameterValue(TEXT("FillTexture"), FallbackTexture);
					}
				}

				FillImage->SetBrushFromMaterial(FillMID);
				FillImage->SetDesiredSizeOverride(FillBrushSize);
				return;
			}
		}

		UTexture2D* FillTexture = Style.DeliveryProgressFillTexture;
		if (!IsValid(FillTexture))
		{
			FillTexture = Style.DeliveryProgressFillTextureFallback;
		}

		if (IsValid(FillTexture))
		{
			FillTexture->ConditionalPostLoad();
			FillImage->SetBrushFromTexture(FillTexture, true);
			FillImage->SetDesiredSizeOverride(FillBrushSize);
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
	RefreshInventoryAndPerkPanels();
	OnDeliveryDataUpdated(GatherDeliveryData());
}

void UGameHUDWidget::RefreshInventoryAndPerkPanels()
{
	RefreshInventoryPanel();
	RefreshPerkPanel();
	OnInventoryDataUpdated(GatherInventoryData());
	OnPerkDataUpdated(GatherPerkData());
}

TArray<FTeammateHUDData> UGameHUDWidget::GatherTeammateData() const
{
	if (GetMatchGameState())
	{
		return BuildTeammateDataFromGameplay();
	}

	if (bUsePreviewData)
	{
		const_cast<UGameHUDWidget*>(this)->EnsurePreviewDefaults();
		return PreviewTeammateData;
	}

	return TArray<FTeammateHUDData>();
}

TArray<FTeammateHUDData> UGameHUDWidget::BuildTeammateDataFromGameplay_Implementation() const
{
	TArray<FTeammateHUDData> Result;

	const AMatchGameState* MatchGS = GetMatchGameState();
	const UWorld* World = GetWorld();
	if (!MatchGS || !World)
	{
		return Result;
	}

	for (const FMatchPlayerState& MatchPlayer : MatchGS->GetMatchPlayers())
	{
		if (MatchPlayer.PlayerRole != ELobbyPlayerRole::Survivor)
		{
			continue;
		}

		FTeammateHUDData Entry;
		Entry.Nickname = FText::FromString(MatchPlayer.Nickname);
		Entry.DisplayState = GameHUDWidgetPrivate::ToDisplayState(MatchPlayer.SurvivorState);
		Entry.CageStack = MatchPlayer.SurvivorState == ESurvivorState::Caged ? 1 : 0;
		Entry.MaxCageStack = 2;
		Entry.DownedHealthPercent = 1.0f;

		if (const ASurvivorCharacter* Survivor = GameHUDWidgetPrivate::FindSurvivorByNickname(World, MatchPlayer.Nickname))
		{
			Entry.DisplayState = GameHUDWidgetPrivate::ToDisplayState(Survivor->GetSurvivorState());
			// <---------------------------- 빌드 안돼서 주석 처리 ---------------------------------->
			//Entry.DownedHealthPercent = Survivor->GetDownedHealthPercent();
			if (Survivor->GetSurvivorState() == ESurvivorState::Caged)
			{
				Entry.CageStack = FMath::Max(Entry.CageStack, 1);
			}
		}
		else if (MatchPlayer.SurvivorState == ESurvivorState::Downed)
		{
			Entry.DownedHealthPercent = 1.0f;
		}

		Result.Add(Entry);

		if (Result.Num() >= SpaCh4HUD::TeammateSlotCount)
		{
			break;
		}
	}

	return Result;
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
	StationA.TargetValue = MatchGS->GetDeliveryStationTargetValueByID(StationA.StationId);
	Result.Add(StationA);

	FDeliveryHUDData StationB;
	StationB.StationId = FName(TEXT("B"));
	StationB.CurrentValue = MatchGS->GetDeliveryStationBValue();
	StationB.TargetValue = MatchGS->GetDeliveryStationTargetValueByID(StationB.StationId);
	Result.Add(StationB);

	return Result;
}

TArray<FInventorySlotHUDData> UGameHUDWidget::GatherInventoryData() const
{
	if (const APlayerController* PlayerController = GetOwningPlayer())
	{
		if (const ASurvivorCharacter* Survivor = Cast<ASurvivorCharacter>(PlayerController->GetPawn()))
		{
			if (const USPInventoryComponent* Inventory = Survivor->GetInventoryComponent())
			{
				return Inventory->BuildInventoryHUDData();
			}
		}
	}

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
	if (const APlayerController* PlayerController = GetOwningPlayer())
	{
		if (const ASurvivorCharacter* Survivor = Cast<ASurvivorCharacter>(PlayerController->GetPawn()))
		{
			if (const USPInventoryComponent* Inventory = Survivor->GetInventoryComponent())
			{
				return Inventory->BuildPerkHUDData();
			}
		}
	}

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

void UGameHUDWidget::SetupDeliveryProgressBars()
{
	// ProgressBar가 있으면 WBP(사이즈·스타일·레이아웃) 설정을 C++가 건드리지 않음.
	if (IsValid(DeliveryProgressA) && IsValid(DeliveryProgressB))
	{
		return;
	}

	const USPGameHUDStyleData& Style = GetResolvedStyle();

	auto SetupRow = [&Style](
		UImage* FrameImage,
		UImage* FillImage,
		UTexture2D* FrameTexture,
		TObjectPtr<UMaterialInstanceDynamic>& FillMID,
		UGameHUDWidget* Outer)
	{
		if (IsValid(FrameImage))
		{
			if (!HasDesignerBrushResource(FrameImage))
			{
				FrameImage->SetColorAndOpacity(FLinearColor::White);
			}
			ApplyDesignerFrameTextureIfNeeded(FrameImage, FrameTexture);
		}

		const FDeliveryProgressLayout Layout = BuildDeliveryProgressLayout(FrameImage);
		const bool bDesignerFill = HasDesignerBrushResource(FillImage);
		SetupDeliveryProgressFillImage(FillImage, FillMID, Outer, Layout, Style);
		if (!bDesignerFill)
		{
			ApplyDeliveryFillOverlaySlot(FillImage, Layout);
		}
	};

	if (!IsValid(DeliveryProgressA))
	{
		if (!DeliveryProgressFillA && DeliveryProgressBarA && DeliveryProgressFillMIDA)
		{
			DeliveryProgressFillMIDA = nullptr;
		}

		if (!DeliveryProgressFillA && DeliveryProgressBarA)
		{
			ApplyDesignerFrameTextureIfNeeded(DeliveryProgressBarA, Style.DeliveryStationFrameA);
		}

		SetupRow(
			DeliveryProgressBGA,
			DeliveryProgressFillA,
			Style.DeliveryStationFrameA,
			DeliveryProgressFillMIDA,
			this);
	}

	if (!IsValid(DeliveryProgressB))
	{
		if (!DeliveryProgressFillB && DeliveryProgressBarB && DeliveryProgressFillMIDB)
		{
			DeliveryProgressFillMIDB = nullptr;
		}

		if (!DeliveryProgressFillB && DeliveryProgressBarB)
		{
			ApplyDesignerFrameTextureIfNeeded(DeliveryProgressBarB, Style.DeliveryStationFrameB);
		}

		SetupRow(
			DeliveryProgressBGB,
			DeliveryProgressFillB,
			Style.DeliveryStationFrameB,
			DeliveryProgressFillMIDB,
			this);
	}
}

void UGameHUDWidget::UpdateDeliveryProgressBar(
	UProgressBar* ProgressBar,
	UImage* FillImage,
	int32 CurrentValue,
	int32 TargetValue)
{
	if (!IsValid(ProgressBar))
	{
		return;
	}

	// WBP 디자이너: 사이즈·스타일·가시성·슬롯은 엔진 설정 유지. 런타임에는 Percent만 갱신.
	if (!IsDesignTime())
	{
		const float ProgressPercent = (TargetValue > 0)
			? FMath::Clamp(static_cast<float>(CurrentValue) / static_cast<float>(TargetValue), 0.0f, 1.0f)
			: 0.0f;

		ProgressBar->SetPercent(ProgressPercent);
	}

	if (IsValid(FillImage))
	{
		FillImage->SetVisibility(ESlateVisibility::Collapsed);
	}
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
		if (FillImage)
		{
			FillImage->SetDesiredSizeOverride(FVector2D::ZeroVector);
		}
		return;
	}

	const USPGameHUDStyleData& Style = GetResolvedStyle();

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
		const bool bDesignerFill = HasDesignerBrushResource(FillImage);

		if (!bDesignerFill)
		{
			if (FillImage == DeliveryProgressFillA)
			{
				if (!DeliveryProgressFillMIDA)
				{
					SetupDeliveryProgressFillImage(FillImage, DeliveryProgressFillMIDA, this, Layout, Style);
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
					SetupDeliveryProgressFillImage(FillImage, DeliveryProgressFillMIDB, this, Layout, Style);
				}
				else
				{
					FillImage->SetBrushFromMaterial(DeliveryProgressFillMIDB);
					SyncDeliveryFillBrushSize(FillImage, Layout);
				}
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

		if (!bDesignerFill)
		{
			ApplyDeliveryFillOverlaySlot(FillImage, Layout);
		}

		const FVector2D DesignerFallback(
			GameHUDWidgetPrivate::DeliveryFillDefaultWidth,
			GameHUDWidgetPrivate::DeliveryFillDefaultHeight);
		const FVector2D FallbackMaxSize = bDesignerFill
			? DesignerFallback
			: FVector2D(Layout.FillMaxWidth, Layout.FillHeight);
		const FVector2D FillMaxSize = ResolveDeliveryFillMaxSize(FillImage, FallbackMaxSize);
		const float FillWidth = FillMaxSize.X * ProgressPercent;
		FillImage->SetDesiredSizeOverride(FVector2D(FillWidth, FillMaxSize.Y));
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
			UTexture2D* FrameTexture = (LegacyBar == DeliveryProgressBarA)
				? Style.DeliveryStationFrameA
				: Style.DeliveryStationFrameB;
			ApplyDesignerFrameTextureIfNeeded(LegacyBar, FrameTexture);
			LegacyBar->SetDesiredSizeOverride(
				FVector2D(DeliveryCompositeWidth, DeliveryCompositeHeight));
			LegacyBar->SetOpacity(1.0f);
			LegacyBar->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else if (!HasDesignerBrushResource(LegacyBar))
		{
			if (UTexture2D* ProgressTexture = Style.GetDeliveryProgressSegmentTexture(FilledSegments))
			{
				LegacyBar->SetBrushFromTexture(ProgressTexture, true);
				LegacyBar->SetOpacity(1.0f);
				LegacyBar->SetVisibility(ESlateVisibility::HitTestInvisible);
			}
		}
		else
		{
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

	UTexture2D* EmptyTexture = Style.DeliveryStackEmpty ? Style.DeliveryStackEmpty : Style.DeliveryProgressBackground;
	UTexture2D* FilledTexture = Style.DeliveryStackFilled ? Style.DeliveryStackFilled : Style.DeliveryProgressFillTextureFallback;

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

	const bool bUsesProgressBars = IsValid(DeliveryProgressA) || IsValid(DeliveryProgressB);
	if (!bUsesProgressBars)
	{
		CacheDesignerDeliveryFillSizes();
	}

	const TArray<FDeliveryHUDData> DeliveryData = GatherDeliveryData();

	auto RefreshStation = [this](
		const FDeliveryHUDData* StationData,
		UProgressBar* ProgressBar,
		UWidget* Root,
		UImage* FrameImage,
		UImage* FillImage,
		UImage* LegacyBar,
		const TArray<TObjectPtr<UImage>>& StackWidgets,
		UTextBlock* ValueLabel)
	{
		const int32 CurrentValue = StationData ? StationData->CurrentValue : 0;
		const int32 TargetValue = StationData ? StationData->TargetValue : 0;

		if (IsValid(ProgressBar))
		{
			UpdateDeliveryProgressBar(ProgressBar, FillImage, CurrentValue, TargetValue);
		}
		else
		{
			UpdateDeliveryProgress(
				Root,
				FrameImage,
				FillImage,
				LegacyBar,
				StackWidgets,
				CurrentValue,
				TargetValue);
		}

		UpdateDeliveryPercentLabel(ValueLabel, CurrentValue, TargetValue);
	};

	RefreshStation(
		DeliveryData.IsValidIndex(0) ? &DeliveryData[0] : nullptr,
		DeliveryProgressA,
		DeliveryProgressRootA,
		DeliveryProgressBGA,
		DeliveryProgressFillA,
		DeliveryProgressBarA,
		DeliveryStackA,
		DeliveryValueA);

	RefreshStation(
		DeliveryData.IsValidIndex(1) ? &DeliveryData[1] : nullptr,
		DeliveryProgressB,
		DeliveryProgressRootB,
		DeliveryProgressBGB,
		DeliveryProgressFillB,
		DeliveryProgressBarB,
		DeliveryStackB,
		DeliveryValueB);
}

void UGameHUDWidget::BindInventoryWidgets()
{
	if (InventorySlots.Num() == 0)
	{
		for (int32 Index = 0; Index < SpaCh4HUD::InventorySlotCount; ++Index)
		{
			const FName WidgetName = *FString::Printf(TEXT("InventorySlots_%d"), Index);
			if (UImage* SlotImage = Cast<UImage>(GetWidgetFromName(WidgetName)))
			{
				InventorySlots.Add(SlotImage);
			}
		}
	}

	if (InventoryIcons.Num() == 0)
	{
		for (int32 Index = 0; Index < SpaCh4HUD::InventorySlotCount; ++Index)
		{
			const FName WidgetName = *FString::Printf(TEXT("InventoryIcons_%d"), Index);
			if (UImage* IconImage = Cast<UImage>(GetWidgetFromName(WidgetName)))
			{
				InventoryIcons.Add(IconImage);
			}
		}
	}

	if (PerkSlots.Num() == 0)
	{
		for (int32 Index = 0; Index < SpaCh4HUD::PerkSlotCount; ++Index)
		{
			const FName WidgetName = *FString::Printf(TEXT("PerkSlots_%d"), Index);
			if (UImage* PerkImage = Cast<UImage>(GetWidgetFromName(WidgetName)))
			{
				PerkSlots.Add(PerkImage);
			}
		}
	}
}

int32 UGameHUDWidget::GetSelectedInventorySlot() const
{
	if (const APlayerController* PlayerController = GetOwningPlayer())
	{
		if (const ASurvivorCharacter* Survivor = Cast<ASurvivorCharacter>(PlayerController->GetPawn()))
		{
			return Survivor->GetSelectedSlotIndex();
		}
	}
	return 0;
}

void UGameHUDWidget::RefreshInventoryPanel()
{
	const TArray<FInventorySlotHUDData> InventoryData = GatherInventoryData();
	const int32 SelectedIndex = GetSelectedInventorySlot();

	for (int32 Index = 0; Index < InventorySlots.Num(); ++Index)
	{
		UImage* SlotFrame = InventorySlots[Index];
		if (!SlotFrame)
		{
			continue;
		}

		const bool bValidData = InventoryData.IsValidIndex(Index);
		const bool bOccupied = bValidData && InventoryData[Index].bIsOccupied;
		SlotFrame->SetOpacity(Index == SelectedIndex ? 1.0f : 0.6f);

		UImage* IconImage = InventoryIcons.IsValidIndex(Index) ? InventoryIcons[Index] : nullptr;
		if (!IconImage)
		{
			continue;
		}

		const bool bShowIcon = bOccupied && bValidData
			&& InventoryData[Index].ContentType == EInventorySlotContentType::Collectible;

		UTexture2D* Icon = bShowIcon ? InventoryData[Index].Icon.LoadSynchronous() : nullptr;
		IconImage->SetBrushFromTexture(Icon, false);
		IconImage->SetOpacity(Icon ? 1.0f : 0.0f);
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

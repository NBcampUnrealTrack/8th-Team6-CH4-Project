#include "UI/GameHUDWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Characters/Survivor/SurvivorCharacter.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/PanelWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInterface.h"
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

	static bool ProgressBarHasFillResource(const UProgressBar* ProgressBar)
	{
		return IsValid(ProgressBar)
			&& ProgressBar->GetWidgetStyle().FillImage.GetResourceObject() != nullptr;
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

	static const ASurvivorCharacter* FindSurvivorForMatchPlayer(const UWorld* World, const FMatchPlayerState& MatchPlayer)
	{
		if (!World)
		{
			return nullptr;
		}

		if (const ASurvivorCharacter* ByName = FindSurvivorByNickname(World, MatchPlayer.Nickname))
		{
			return ByName;
		}

		if (MatchPlayer.PlayerId == INDEX_NONE)
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
			if (PS && PS->GetPlayerId() == MatchPlayer.PlayerId)
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

	UE_LOG(LogTemp, Warning, TEXT("HUD NativeConstruct world=%s design=%d"),
		*GetNameSafe(GetWorld()),
		IsDesignTime() ? 1 : 0);

	BindInventoryWidgets();

	ClearDeliveryProgressSetupTimer();
	DeliveryProgressFillMIDA = nullptr;
	DeliveryProgressFillMIDB = nullptr;
	bDeliveryProgressBarARaised = false;
	bDeliveryProgressBarBRaised = false;
	CacheDesignerDeliveryFillSizes();

	ApplyDeliveryRowLabels();

	if (IsDesignTime())
	{
		RefreshAll();
		return;
	}

	// Immediate setup — a 0s deferred timer was skipping bind in some PIE timings.
	FinalizeDeliveryProgressSetup();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			TeammateRefreshTimerHandle,
			this,
			&UGameHUDWidget::RefreshMatchHudPanels,
			0.2f,
			true);
	}
}

void UGameHUDWidget::NativeDestruct()
{
	// Clear refresh timers even when MatchGS bind never succeeded (early Unbind return).
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TeammateRefreshTimerHandle);
		World->GetTimerManager().ClearTimer(MatchDelegateBindRetryTimerHandle);
	}
	TeammateRefreshTimerHandle.Invalidate();
	MatchDelegateBindRetryTimerHandle.Invalidate();

	UnbindMatchStateDelegates();
	ClearDeliveryProgressSetupTimer();
	DeliveryProgressFillMIDA = nullptr;
	DeliveryProgressFillMIDB = nullptr;
	bDeliveryProgressBarARaised = false;
	bDeliveryProgressBarBRaised = false;
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
		UE_LOG(LogTemp, Warning, TEXT("HUD BindMatchStateDelegates: MatchGameState not ready, retry"));
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				MatchDelegateBindRetryTimerHandle,
				this,
				&UGameHUDWidget::BindMatchStateDelegates,
				0.25f,
				false);
		}
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MatchDelegateBindRetryTimerHandle);
	}

	MatchGS->OnDeliveryProgressChanged.AddDynamic(this, &UGameHUDWidget::HandleDeliveryProgressChanged);
	MatchGS->OnMatchPlayersChanged.AddDynamic(this, &UGameHUDWidget::HandleMatchPlayersChanged);
	MatchGS->OnSurvivorStateChanged.AddDynamic(this, &UGameHUDWidget::HandleSurvivorStateChanged);
	bMatchDelegatesBound = true;

	UE_LOG(LogTemp, Warning, TEXT("HUD Bound MatchGS delegates (Delivery/Players/Survivor) gs=%s"),
		*GetNameSafe(MatchGS));

	// Sync current values in case broadcasts fired before we bound.
	RefreshDeliveryPanel();
	RefreshTeammateEntries();
}

void UGameHUDWidget::UnbindMatchStateDelegates()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MatchDelegateBindRetryTimerHandle);
		World->GetTimerManager().ClearTimer(TeammateRefreshTimerHandle);
	}

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

	bMatchDelegatesBound = false;
}

void UGameHUDWidget::HandleDeliveryProgressChanged(
	int32 StationAValue,
	int32 StationBValue,
	int32 TotalDeliveredValue,
	float DeliveryProgress)
{
	(void)TotalDeliveredValue;
	(void)DeliveryProgress;

	ResolveDeliveryWidgetBindings();
	EnsureDeliveryFillWidgets();
	ApplyDeliveryRowLabels();

	const AMatchGameState* MatchGS = GetMatchGameState();
	const int32 TargetA = MatchGS
		? MatchGS->GetDeliveryStationTargetValueByID(FName(TEXT("A")))
		: 200;
	const int32 TargetB = MatchGS
		? MatchGS->GetDeliveryStationTargetValueByID(FName(TEXT("B")))
		: 200;

	UE_LOG(LogTemp, Warning,
		TEXT("HUD DeliveryVisual A=%d/%d ProgressA=%s PreferPB=%d FillA=%s | B=%d/%d ProgressB=%s PreferPB=%d FillB=%s"),
		StationAValue, TargetA,
		*GetNameSafe(DeliveryProgressA),
		ShouldPreferDesignerProgressBar(DeliveryProgressA, DeliveryProgressFillA) ? 1 : 0,
		*GetNameSafe(DeliveryProgressFillA),
		StationBValue, TargetB,
		*GetNameSafe(DeliveryProgressB),
		ShouldPreferDesignerProgressBar(DeliveryProgressB, DeliveryProgressFillB) ? 1 : 0,
		*GetNameSafe(DeliveryProgressFillB));

	ApplyDeliveryStationVisuals(
		StationAValue,
		TargetA,
		DeliveryProgressA,
		DeliveryProgressRootA,
		DeliveryProgressBGA,
		DeliveryProgressFillA,
		DeliveryProgressBarA,
		DeliveryStackA,
		DeliveryValueA);

	ApplyDeliveryStationVisuals(
		StationBValue,
		TargetB,
		DeliveryProgressB,
		DeliveryProgressRootB,
		DeliveryProgressBGB,
		DeliveryProgressFillB,
		DeliveryProgressBarB,
		DeliveryStackB,
		DeliveryValueB);

	OnDeliveryDataUpdated(GatherDeliveryData());
}

void UGameHUDWidget::HandleMatchPlayersChanged()
{
	RefreshTeammateEntries();
}

void UGameHUDWidget::HandleSurvivorStateChanged(const int32 SurvivorPlayerId, const FString SurvivorNickname, const ESurvivorState SurvivorState)
{
	(void)SurvivorPlayerId;
	(void)SurvivorState;
	(void)SurvivorNickname;
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
	ResolveDeliveryWidgetBindings();
	EnsureDeliveryFillWidgets();
	CacheDesignerDeliveryFillSizes();
	BindMatchStateDelegates();
	SetupDeliveryProgressBars();
	RefreshAll();
}

void UGameHUDWidget::ResolveDeliveryWidgetBindings()
{
	if (!WidgetTree)
	{
		return;
	}

	auto ResolveImage = [this](TObjectPtr<UImage>& Field, const TCHAR* Name)
	{
		if (!IsValid(Field))
		{
			Field = Cast<UImage>(WidgetTree->FindWidget(Name));
		}
	};
	auto ResolveText = [this](TObjectPtr<UTextBlock>& Field, const TCHAR* Name)
	{
		if (!IsValid(Field))
		{
			Field = Cast<UTextBlock>(WidgetTree->FindWidget(Name));
		}
	};
	auto ResolveWidget = [this](TObjectPtr<UWidget>& Field, const TCHAR* Name)
	{
		if (!IsValid(Field))
		{
			Field = WidgetTree->FindWidget(Name);
		}
	};
	auto ResolveProgress = [this](TObjectPtr<UProgressBar>& Field, const TCHAR* Name)
	{
		if (!IsValid(Field))
		{
			Field = Cast<UProgressBar>(WidgetTree->FindWidget(Name));
		}
	};

	ResolveImage(DeliveryProgressFillA, TEXT("DeliveryProgressFillA"));
	ResolveImage(DeliveryProgressFillB, TEXT("DeliveryProgressFillB"));
	ResolveImage(DeliveryProgressBGA, TEXT("DeliveryProgressBGA"));
	ResolveImage(DeliveryProgressBGB, TEXT("DeliveryProgressBGB"));
	ResolveImage(DeliveryProgressBarA, TEXT("DeliveryProgressBarA"));
	ResolveImage(DeliveryProgressBarB, TEXT("DeliveryProgressBarB"));
	ResolveImage(DeliveryIconA, TEXT("DeliveryIconA"));
	ResolveImage(DeliveryIconB, TEXT("DeliveryIconB"));
	ResolveText(DeliveryValueA, TEXT("DeliveryValueA"));
	ResolveText(DeliveryValueB, TEXT("DeliveryValueB"));
	ResolveText(DeliveryLabelA, TEXT("DeliveryLabelA"));
	ResolveText(DeliveryLabelB, TEXT("DeliveryLabelB"));
	ResolveWidget(DeliveryProgressRootA, TEXT("DeliveryProgressRootA"));
	ResolveWidget(DeliveryProgressRootB, TEXT("DeliveryProgressRootB"));
	ResolveProgress(DeliveryProgressA, TEXT("DeliveryProgressA"));
	ResolveProgress(DeliveryProgressB, TEXT("DeliveryProgressB"));

	if (!IsValid(DeliveryProgressFillA))
	{
		ResolveImage(DeliveryProgressFillA, TEXT("ProgressFillA"));
	}
	if (!IsValid(DeliveryProgressFillB))
	{
		ResolveImage(DeliveryProgressFillB, TEXT("ProgressFillB"));
	}
	if (!IsValid(DeliveryProgressBGA))
	{
		ResolveImage(DeliveryProgressBGA, TEXT("ProgressBGA"));
	}
	if (!IsValid(DeliveryProgressBGB))
	{
		ResolveImage(DeliveryProgressBGB, TEXT("ProgressBGB"));
	}

	// WBP uses DeliveryProgressStack* container names (not DeliveryStack* array).
	if (DeliveryStackA.Num() == 0)
	{
		if (UWidget* StackRoot = WidgetTree->FindWidget(TEXT("DeliveryProgressStackA")))
		{
			if (UPanelWidget* Panel = Cast<UPanelWidget>(StackRoot))
			{
				const int32 ChildCount = Panel->GetChildrenCount();
				for (int32 Index = 0; Index < ChildCount; ++Index)
				{
					if (UImage* Segment = Cast<UImage>(Panel->GetChildAt(Index)))
					{
						DeliveryStackA.Add(Segment);
					}
				}
			}
		}
	}
	if (DeliveryStackB.Num() == 0)
	{
		if (UWidget* StackRoot = WidgetTree->FindWidget(TEXT("DeliveryProgressStackB")))
		{
			if (UPanelWidget* Panel = Cast<UPanelWidget>(StackRoot))
			{
				const int32 ChildCount = Panel->GetChildrenCount();
				for (int32 Index = 0; Index < ChildCount; ++Index)
				{
					if (UImage* Segment = Cast<UImage>(Panel->GetChildAt(Index)))
					{
						DeliveryStackB.Add(Segment);
					}
				}
			}
		}
	}
}

void UGameHUDWidget::EnsureDeliveryFillWidgets()
{
	if (!WidgetTree)
	{
		return;
	}

	auto EnsureFill = [this](TObjectPtr<UImage>& FillField, UWidget* Root, UImage* Frame, FName WidgetName)
	{
		if (IsValid(FillField))
		{
			return;
		}

		FillField = Cast<UImage>(WidgetTree->FindWidget(WidgetName));
		if (IsValid(FillField))
		{
			return;
		}

		UPanelWidget* Parent = nullptr;
		if (Frame)
		{
			Parent = Cast<UPanelWidget>(Frame->GetParent());
		}
		if (!Parent)
		{
			Parent = Cast<UOverlay>(Root);
		}
		if (!Parent)
		{
			Parent = Cast<UPanelWidget>(Root);
		}
		if (!Parent)
		{
			return;
		}

		UImage* NewFill = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), WidgetName);
		if (!NewFill)
		{
			return;
		}

		NewFill->SetVisibility(ESlateVisibility::HitTestInvisible);
		NewFill->SetColorAndOpacity(FLinearColor::White);

		if (UOverlay* Overlay = Cast<UOverlay>(Parent))
		{
			if (UOverlaySlot* Slot = Overlay->AddChildToOverlay(NewFill))
			{
				Slot->SetHorizontalAlignment(HAlign_Left);
				Slot->SetVerticalAlignment(VAlign_Top);
				Slot->SetPadding(FMargin(0.f));
			}
		}
		else
		{
			Parent->AddChild(NewFill);
		}

		FillField = NewFill;
	};

	// Designer ProgressBar fill takes priority — do not fabricate Image fills that steal updates.
	if (!ShouldPreferDesignerProgressBar(DeliveryProgressA, DeliveryProgressFillA))
	{
		EnsureFill(DeliveryProgressFillA, DeliveryProgressRootA, DeliveryProgressBGA, TEXT("DeliveryProgressFillA"));
	}
	if (!ShouldPreferDesignerProgressBar(DeliveryProgressB, DeliveryProgressFillB))
	{
		EnsureFill(DeliveryProgressFillB, DeliveryProgressRootB, DeliveryProgressBGB, TEXT("DeliveryProgressFillB"));
	}
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

	bool HasDesignerBrushResource(const UImage* Image)
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
		// Lobby 미지정(None)도 UI 테스트에서 생존자로 취급. Killer만 제외.
		if (MatchPlayer.PlayerRole == ELobbyPlayerRole::Killer)
		{
			continue;
		}

		FTeammateHUDData Entry;
		Entry.Nickname = FText::FromString(MatchPlayer.Nickname);
		Entry.DisplayState = GameHUDWidgetPrivate::ToDisplayState(MatchPlayer.SurvivorState);
		Entry.CageStack = MatchPlayer.SurvivorState == ESurvivorState::Caged ? 1 : 0;
		Entry.MaxCageStack = 2;
		Entry.DownedHealthPercent = 1.0f;

		if (const ASurvivorCharacter* Survivor = GameHUDWidgetPrivate::FindSurvivorForMatchPlayer(World, MatchPlayer))
		{
			Entry.DisplayState = GameHUDWidgetPrivate::ToDisplayState(Survivor->GetSurvivorState());
			Entry.DownedHealthPercent = 1.0f;
			if (Survivor->GetSurvivorState() == ESurvivorState::Caged)
			{
				Entry.CageStack = FMath::Max(Entry.CageStack, MatchPlayer.CagedCount > 0 ? MatchPlayer.CagedCount : 1);
			}
			else
			{
				Entry.CageStack = MatchPlayer.CagedCount;
			}
		}
		else if (MatchPlayer.SurvivorState == ESurvivorState::Downed)
		{
			Entry.DownedHealthPercent = 1.0f;
		}
		else
		{
			Entry.CageStack = MatchPlayer.CagedCount;
		}

		Result.Add(Entry);

		if (Result.Num() >= SpaCh4HUD::TeammateSlotCount)
		{
			break;
		}
	}

	// MatchPlayers가 비었거나 역할 미등록이면 월드 생존자로 폴백.
	if (Result.Num() == 0)
	{
		for (TActorIterator<ASurvivorCharacter> It(World); It; ++It)
		{
			const ASurvivorCharacter* Survivor = *It;
			if (!IsValid(Survivor))
			{
				continue;
			}

			FTeammateHUDData Entry;
			const APlayerState* PS = Survivor->GetPlayerState();
			Entry.Nickname = FText::FromString(PS ? PS->GetPlayerName() : TEXT("Player"));
			Entry.DisplayState = GameHUDWidgetPrivate::ToDisplayState(Survivor->GetSurvivorState());
			Entry.DownedHealthPercent = 1.0f;
			Entry.CageStack = Survivor->GetCagedCount();
			Entry.MaxCageStack = 2;
			Result.Add(Entry);

			if (Result.Num() >= SpaCh4HUD::TeammateSlotCount)
			{
				break;
			}
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

void UGameHUDWidget::RefreshMatchHudPanels()
{
	if (!IsValid(this) || !GetWorld() || !IsInGameThread())
	{
		return;
	}

	if (!bMatchDelegatesBound)
	{
		BindMatchStateDelegates();
	}

	RefreshDeliveryPanel();
	RefreshTeammateEntries();
	RefreshInventoryAndPerkPanels();
}

void UGameHUDWidget::RefreshTeammateEntries()
{
	const TArray<FTeammateHUDData> TeammateData = GatherTeammateData();

	TArray<UTeammateEntryWidget*> Entries;
	Entries.Reserve(SpaCh4HUD::TeammateSlotCount);

	for (int32 Index = 0; Index < SpaCh4HUD::TeammateSlotCount; ++Index)
	{
		const FName WidgetName(*FString::Printf(TEXT("TeammateEntries_%d"), Index));
		UTeammateEntryWidget* Entry = Cast<UTeammateEntryWidget>(GetWidgetFromName(WidgetName));
		if (!Entry && WidgetTree)
		{
			Entry = Cast<UTeammateEntryWidget>(WidgetTree->FindWidget(WidgetName));
		}
		if (Entry)
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
		ESurvivorDisplayState::Downed,
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
	TeammateB.CageStack = 0;
	TeammateB.DisplayState = ESurvivorDisplayState::Downed;
	TeammateB.DownedHealthPercent = 0.65f;
	PreviewTeammateData.Add(TeammateB);

	FTeammateHUDData TeammateC;
	TeammateC.Nickname = NSLOCTEXT("SpaCh4", "PreviewTeammateC", "Player_C");
	TeammateC.CageStack = 2;
	TeammateC.DisplayState = ESurvivorDisplayState::Dead;
	PreviewTeammateData.Add(TeammateC);
}

void UGameHUDWidget::SetupDeliveryProgressBars()
{
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

		if (!IsValid(FillImage))
		{
			return;
		}

		const FDeliveryProgressLayout Layout = BuildDeliveryProgressLayout(FrameImage);
		SetupDeliveryProgressFillImage(FillImage, FillMID, Outer, Layout, Style);
		// Always left-align so width/scale progress reads L→R even with designer Stretch slots.
		ApplyDeliveryFillOverlaySlot(FillImage, Layout);
	};

	if (ShouldPreferDesignerProgressBar(DeliveryProgressA, DeliveryProgressFillA))
	{
		EnsureProgressBarDesignerFillMID(DeliveryProgressA, DeliveryProgressFillMIDA);
		if (IsValid(DeliveryProgressFillA))
		{
			DeliveryProgressFillA->SetVisibility(ESlateVisibility::Collapsed);
		}
		if (IsValid(DeliveryProgressBGA))
		{
			ApplyDesignerFrameTextureIfNeeded(DeliveryProgressBGA, Style.DeliveryStationFrameA);
		}
	}
	else
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

	if (ShouldPreferDesignerProgressBar(DeliveryProgressB, DeliveryProgressFillB))
	{
		EnsureProgressBarDesignerFillMID(DeliveryProgressB, DeliveryProgressFillMIDB);
		if (IsValid(DeliveryProgressFillB))
		{
			DeliveryProgressFillB->SetVisibility(ESlateVisibility::Collapsed);
		}
		if (IsValid(DeliveryProgressBGB))
		{
			ApplyDesignerFrameTextureIfNeeded(DeliveryProgressBGB, Style.DeliveryStationFrameB);
		}
	}
	else
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

void UGameHUDWidget::EnsureProgressBarHasFillBrush(UProgressBar* ProgressBar)
{
	if (!IsValid(ProgressBar))
	{
		return;
	}

	FProgressBarStyle BarStyle = ProgressBar->GetWidgetStyle();
	const bool bHasFill = BarStyle.FillImage.GetResourceObject() != nullptr;
	if (bHasFill)
	{
		BarStyle.BackgroundImage.TintColor = FSlateColor(FLinearColor(0.f, 0.f, 0.f, 0.f));
		ProgressBar->SetWidgetStyle(BarStyle);
		return;
	}

	// Prefer already-resolved style fields; fall back to direct soft loads so we never
	// depend on mutating DeliveryProgressSegmentTextures during HUD construct.
	UTexture2D* FillTexture = nullptr;
	if (IsValid(VisualStyle))
	{
		FillTexture = VisualStyle->DeliveryProgressFillTexture;
		if (!IsValid(FillTexture))
		{
			FillTexture = VisualStyle->DeliveryProgressFillTextureFallback;
		}
	}

	if (!IsValid(FillTexture))
	{
		const USPGameHUDStyleData& Style = GetResolvedStyle();
		FillTexture = Style.DeliveryProgressFillTexture;
		if (!IsValid(FillTexture))
		{
			FillTexture = Style.DeliveryProgressFillTextureFallback;
		}
	}

	if (!IsValid(FillTexture))
	{
		FillTexture = LoadObject<UTexture2D>(
			nullptr,
			TEXT("/Game/UI/HUD/Textures/Delivery/T_HUD_Bar_Fill_Delivery.T_HUD_Bar_Fill_Delivery"));
	}
	if (!IsValid(FillTexture))
	{
		FillTexture = LoadObject<UTexture2D>(
			nullptr,
			TEXT("/Game/UI/HUD/Textures/Common/T_HUD_Bar_Fill.T_HUD_Bar_Fill"));
	}

	if (IsValid(FillTexture))
	{
		FillTexture->ConditionalPostLoad();
		BarStyle.FillImage.SetResourceObject(FillTexture);
		BarStyle.FillImage.ImageSize = FVector2D(
			GameHUDWidgetPrivate::DeliveryFillDefaultWidth,
			GameHUDWidgetPrivate::DeliveryFillDefaultHeight);
		BarStyle.FillImage.DrawAs = ESlateBrushDrawType::Image;
		BarStyle.FillImage.TintColor = FSlateColor(FLinearColor::White);
		BarStyle.FillImage.Tiling = ESlateBrushTileType::NoTile;
	}

	BarStyle.BackgroundImage.TintColor = FSlateColor(FLinearColor(0.f, 0.f, 0.f, 0.f));
	ProgressBar->SetWidgetStyle(BarStyle);
}

void UGameHUDWidget::EnsureProgressBarDesignerFillMID(
	UProgressBar* ProgressBar,
	TObjectPtr<UMaterialInstanceDynamic>& FillMID)
{
	if (!IsValid(ProgressBar))
	{
		FillMID = nullptr;
		return;
	}

	FProgressBarStyle BarStyle = ProgressBar->GetWidgetStyle();
	UObject* Resource = BarStyle.FillImage.GetResourceObject();

	if (UMaterialInstanceDynamic* ExistingMID = Cast<UMaterialInstanceDynamic>(Resource))
	{
		FillMID = ExistingMID;
		return;
	}

	if (UMaterialInterface* Material = Cast<UMaterialInterface>(Resource))
	{
		Material->ConditionalPostLoad();
		FillMID = UMaterialInstanceDynamic::Create(Material, ProgressBar);
		if (IsValid(FillMID))
		{
			// Preserve designer Image Size / Draw As / Margin; only swap resource to MID.
			BarStyle.FillImage.SetResourceObject(FillMID);
			BarStyle.BackgroundImage.TintColor = FSlateColor(FLinearColor(0.f, 0.f, 0.f, 0.f));
			ProgressBar->SetWidgetStyle(BarStyle);
		}
		return;
	}

	FillMID = nullptr;
	EnsureProgressBarHasFillBrush(ProgressBar);
}

bool UGameHUDWidget::ShouldPreferDesignerProgressBar(UProgressBar* ProgressBar, UImage* FillImage) const
{
	if (!IsValid(ProgressBar))
	{
		return false;
	}

	// WBP ProgressBar Fill Image (e.g. MI_HUD_DeliveryProgressFill) wins over Image fills.
	if (GameHUDWidgetPrivate::ProgressBarHasFillResource(ProgressBar))
	{
		return true;
	}

	// Empty ProgressBar fill: only fall back to Image path when a designer Image fill exists.
	if (IsValid(FillImage) && GameHUDWidgetPrivate::HasDesignerBrushResource(FillImage))
	{
		return false;
	}

	return true;
}

void UGameHUDWidget::UpdateDeliveryProgressBar(
	UProgressBar* ProgressBar,
	UImage* FillImage,
	int32 CurrentValue,
	int32 TargetValue,
	TObjectPtr<UMaterialInstanceDynamic>& ProgressBarFillMID)
{
	if (!IsValid(ProgressBar))
	{
		return;
	}

	const float ProgressPercent = (TargetValue > 0)
		? FMath::Clamp(static_cast<float>(CurrentValue) / static_cast<float>(TargetValue), 0.0f, 1.0f)
		: 0.0f;

	if (!IsDesignTime())
	{
		EnsureProgressBarDesignerFillMID(ProgressBar, ProgressBarFillMID);

		bool bDrivenByFillAmount = false;
		if (IsValid(ProgressBarFillMID))
		{
			TArray<FMaterialParameterInfo> ScalarInfos;
			TArray<FGuid> ScalarIds;
			ProgressBarFillMID->GetAllScalarParameterInfo(ScalarInfos, ScalarIds);
			for (const FMaterialParameterInfo& Info : ScalarInfos)
			{
				if (Info.Name == TEXT("FillAmount"))
				{
					bDrivenByFillAmount = true;
					break;
				}
			}

			if (bDrivenByFillAmount)
			{
				ProgressBarFillMID->SetScalarParameterValue(TEXT("FillAmount"), ProgressPercent);
			}
		}

		// FillAmount materials paint the full quad; keep Percent at 1 to avoid double-clip.
		// Texture / non-FillAmount materials rely on ProgressBar Percent clipping.
		ProgressBar->SetPercent(bDrivenByFillAmount ? 1.0f : ProgressPercent);
		ProgressBar->SetFillColorAndOpacity(FLinearColor::White);
		ProgressBar->SetVisibility(ESlateVisibility::HitTestInvisible);

		// Raise ProgressBar once — reparenting every tick risks Slate AV on PIE restart.
		bool* bRaisedFlag = nullptr;
		if (ProgressBar == DeliveryProgressA)
		{
			bRaisedFlag = &bDeliveryProgressBarARaised;
		}
		else if (ProgressBar == DeliveryProgressB)
		{
			bRaisedFlag = &bDeliveryProgressBarBRaised;
		}

		if (bRaisedFlag && !*bRaisedFlag)
		{
			if (UWidget* BarParent = ProgressBar->GetParent())
			{
				if (UOverlay* Overlay = Cast<UOverlay>(BarParent->GetParent()))
				{
					const int32 ChildCount = Overlay->GetChildrenCount();
					if (ChildCount > 0 && Overlay->GetChildAt(ChildCount - 1) != BarParent)
					{
						// ShiftChild reorders in place, keeping the designer OverlaySlot and
						// skipping the Slate release/recreate of RemoveFromParent+AddChild.
						Overlay->ShiftChild(ChildCount - 1, BarParent);
					}
				}
			}
			*bRaisedFlag = true;
		}

		if (CurrentValue > 0)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("HUD ProgressBarUpdate %s value=%d/%d percent=%.3f fillAmount=%d mid=%s"),
				*GetNameSafe(ProgressBar),
				CurrentValue,
				TargetValue,
				ProgressPercent,
				bDrivenByFillAmount ? 1 : 0,
				*GetNameSafe(ProgressBarFillMID));
		}
	}
	// Designer ProgressBar owns the fill — hide competing Image widgets.
	if (IsValid(FillImage))
	{
		FillImage->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UGameHUDWidget::ApplyDeliveryFillProgressVisual(UImage* FillImage, UImage* FrameImage, float ProgressPercent)
{
	if (!IsValid(FillImage))
	{
		return;
	}

	const float Clamped = FMath::Clamp(ProgressPercent, 0.0f, 1.0f);
	const FDeliveryProgressLayout Layout = BuildDeliveryProgressLayout(FrameImage);
	ApplyDeliveryFillOverlaySlot(FillImage, Layout);

	const FVector2D DesignerFallback(
		GameHUDWidgetPrivate::DeliveryFillDefaultWidth,
		GameHUDWidgetPrivate::DeliveryFillDefaultHeight);
	const bool bDesignerFill = HasDesignerBrushResource(FillImage);
	const FVector2D FallbackMaxSize = bDesignerFill
		? DesignerFallback
		: FVector2D(Layout.FillMaxWidth, Layout.FillHeight);
	const FVector2D FillMaxSize = ResolveDeliveryFillMaxSize(FillImage, FallbackMaxSize);

	// Keep full brush size, then scale on X from the left edge.
	// DesiredSize alone is ignored when the designer slot uses Stretch/Fill.
	FillImage->SetDesiredSizeOverride(FillMaxSize);
	FillImage->SetRenderTransformPivot(FVector2D(0.f, 0.5f));
	FillImage->SetRenderScale(FVector2D(FMath::Max(Clamped, 0.0001f), 1.f));
	FillImage->SetOpacity(Clamped > KINDA_SMALL_NUMBER ? 1.f : 0.f);
	FillImage->SetVisibility(ESlateVisibility::HitTestInvisible);

	if (FillImage == DeliveryProgressFillA && DeliveryProgressFillMIDA)
	{
		DeliveryProgressFillMIDA->SetScalarParameterValue(TEXT("FillAmount"), Clamped);
	}
	else if (FillImage == DeliveryProgressFillB && DeliveryProgressFillMIDB)
	{
		DeliveryProgressFillMIDB->SetScalarParameterValue(TEXT("FillAmount"), Clamped);
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
		if (IsValid(FillImage))
		{
			FillImage->SetRenderScale(FVector2D(0.0001f, 1.f));
			FillImage->SetOpacity(0.f);
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

	if (IsValid(FillImage))
	{
		const bool bDesignerFill = HasDesignerBrushResource(FillImage);
		if (!bDesignerFill)
		{
			const FDeliveryProgressLayout Layout = BuildDeliveryProgressLayout(FrameImage);
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
		else if (IsValid(FrameImage))
		{
			FrameImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		}

		if (LegacyBar)
		{
			LegacyBar->SetVisibility(ESlateVisibility::Collapsed);
		}

		ApplyDeliveryFillProgressVisual(FillImage, FrameImage, ProgressPercent);

		if (IsValid(FrameImage))
		{
			FrameImage->SetVisibility(ESlateVisibility::HitTestInvisible);
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

	UTexture2D* EmptyTexture = IsValid(Style.DeliveryStackEmpty)
		? Style.DeliveryStackEmpty
		: Style.DeliveryProgressBackground;
	UTexture2D* FilledTexture = IsValid(Style.DeliveryStackFilled)
		? Style.DeliveryStackFilled
		: Style.DeliveryProgressFillTextureFallback;

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
	ResolveDeliveryWidgetBindings();
	EnsureDeliveryFillWidgets();
	ApplyDeliveryRowLabels();

	CacheDesignerDeliveryFillSizes();

	const TArray<FDeliveryHUDData> DeliveryData = GatherDeliveryData();

	ApplyDeliveryStationVisuals(
		DeliveryData.IsValidIndex(0) ? DeliveryData[0].CurrentValue : 0,
		DeliveryData.IsValidIndex(0) ? DeliveryData[0].TargetValue : 0,
		DeliveryProgressA,
		DeliveryProgressRootA,
		DeliveryProgressBGA,
		DeliveryProgressFillA,
		DeliveryProgressBarA,
		DeliveryStackA,
		DeliveryValueA);

	ApplyDeliveryStationVisuals(
		DeliveryData.IsValidIndex(1) ? DeliveryData[1].CurrentValue : 0,
		DeliveryData.IsValidIndex(1) ? DeliveryData[1].TargetValue : 0,
		DeliveryProgressB,
		DeliveryProgressRootB,
		DeliveryProgressBGB,
		DeliveryProgressFillB,
		DeliveryProgressBarB,
		DeliveryStackB,
		DeliveryValueB);
}

void UGameHUDWidget::ApplyDeliveryStationVisuals(
	int32 CurrentValue,
	int32 TargetValue,
	UProgressBar* ProgressBar,
	UWidget* Root,
	UImage* FrameImage,
	UImage* FillImage,
	UImage* LegacyBar,
	const TArray<TObjectPtr<UImage>>& StackWidgets,
	UTextBlock* ValueLabel)
{
	if (ShouldPreferDesignerProgressBar(ProgressBar, FillImage))
	{
		TObjectPtr<UMaterialInstanceDynamic>& ProgressBarFillMID =
			(ProgressBar == DeliveryProgressA) ? DeliveryProgressFillMIDA : DeliveryProgressFillMIDB;

		UpdateDeliveryProgressBar(
			ProgressBar,
			FillImage,
			CurrentValue,
			TargetValue,
			ProgressBarFillMID);

		if (IsValid(LegacyBar))
		{
			LegacyBar->SetVisibility(ESlateVisibility::Collapsed);
		}
		for (UImage* SegmentImage : StackWidgets)
		{
			if (SegmentImage)
			{
				SegmentImage->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
		if (IsValid(Root))
		{
			Root->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		if (IsValid(FrameImage))
		{
			FrameImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		}

		UpdateDeliveryPercentLabel(ValueLabel, CurrentValue, TargetValue);
		return;
	}

	if (IsValid(ProgressBar))
	{
		ProgressBar->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (IsValid(FillImage) || IsValid(LegacyBar) || StackWidgets.Num() > 0)
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
}

void UGameHUDWidget::BindInventoryWidgets()
{
	if (InventorySlots.Num() == 0)
	{
		for (int32 Index = 0; Index < SpaCh4HUD::InventorySlotCount; ++Index)
		{
			const FName WidgetName = *FString::Printf(TEXT("InventorySlots_%d"), Index);
			UImage* SlotImage = Cast<UImage>(GetWidgetFromName(WidgetName));
			if (!SlotImage && WidgetTree)
			{
				SlotImage = Cast<UImage>(WidgetTree->FindWidget(WidgetName));
			}
			if (SlotImage)
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
			UImage* IconImage = Cast<UImage>(GetWidgetFromName(WidgetName));
			if (!IconImage && WidgetTree)
			{
				IconImage = Cast<UImage>(WidgetTree->FindWidget(WidgetName));
			}
			if (IconImage)
			{
				InventoryIcons.Add(IconImage);
			}
		}
	}

	if (InventoryCounts.Num() != SpaCh4HUD::InventorySlotCount)
	{
		InventoryCounts.SetNum(SpaCh4HUD::InventorySlotCount);
		for (int32 Index = 0; Index < SpaCh4HUD::InventorySlotCount; ++Index)
		{
			const FName WidgetName = *FString::Printf(TEXT("InventoryCounts_%d"), Index);
			UTextBlock* CountText = Cast<UTextBlock>(GetWidgetFromName(WidgetName));
			if (!CountText && WidgetTree)
			{
				CountText = Cast<UTextBlock>(WidgetTree->FindWidget(WidgetName));
			}

			InventoryCounts[Index] = CountText;
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

		const bool bShowIcon = bOccupied && bValidData;

		UTexture2D* Icon = bShowIcon ? InventoryData[Index].Icon.LoadSynchronous() : nullptr;
		IconImage->SetBrushFromTexture(Icon, false);
		IconImage->SetOpacity(Icon ? 1.0f : 0.0f);

		if (UTextBlock* CountText = InventoryCounts.IsValidIndex(Index) ? InventoryCounts[Index] : nullptr)
		{
			const int32 Quantity = bValidData ? InventoryData[Index].Quantity : 0;
			CountText->SetText(Quantity > 1 ? FText::AsNumber(Quantity) : FText::GetEmpty());
			CountText->SetVisibility(Quantity > 1 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
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

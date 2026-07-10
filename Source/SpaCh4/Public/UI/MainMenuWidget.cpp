#include "UI/MainMenuWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Player/MainMenuPlayerController.h"
#include "Systems/LobbyGameState.h"
#include "Systems/SPEOSSessionSubsystem.h"
#include "Styling/SlateTypes.h"
#include "UI/HUDFontUtils.h"
#include "UI/Style/SPUIStyleData.h"
#include "UI/Style/SPUIStyleLibrary.h"

const USPMainMenuStyleData& UMainMenuWidget::GetResolvedStyle() const
{
	return SPUIStyleLibrary::ResolveMainMenuStyle(VisualStyle);
}

void UMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (!TitleText)
	{
		TitleText = Cast<UTextBlock>(GetWidgetFromName(TEXT("TitleText")));
	}
	if (!SurvivorButton)
	{
		SurvivorButton = Cast<UButton>(GetWidgetFromName(TEXT("SurvivorButton")));
	}
	if (!KillerButton)
	{
		KillerButton = Cast<UButton>(GetWidgetFromName(TEXT("KillerButton")));
	}
	if (!SettingsButton)
	{
		SettingsButton = Cast<UButton>(GetWidgetFromName(TEXT("SettingsButton")));
	}
	if (!QuitButton)
	{
		QuitButton = Cast<UButton>(GetWidgetFromName(TEXT("QuitButton")));
	}
	if (!CancelMatchmakingButton)
	{
		CancelMatchmakingButton = Cast<UButton>(GetWidgetFromName(TEXT("CancelMatchmakingButton")));
	}
	if (!MatchmakingStatusText)
	{
		MatchmakingStatusText = Cast<UTextBlock>(GetWidgetFromName(TEXT("MatchmakingStatusText")));
	}
	if (!TitleImage)
	{
		TitleImage = Cast<UImage>(GetWidgetFromName(TEXT("TitleImage")));
	}

	BindMenuButtons();
	BindOnlineEvents();
	BindLobbyStateEvents();
	ApplyMenuLabels();
	EnsureTitleOnTop();
	ApplyMenuTitleImage();
	ApplyMenuButtonStyles();

	UGameInstance* GameInstance = GetGameInstance();
	const USPEOSSessionSubsystem* SessionSubsystem = GameInstance ? GameInstance->GetSubsystem<USPEOSSessionSubsystem>() : nullptr;
	const bool bIsMatchmaking = SessionSubsystem && SessionSubsystem->IsMatchmakingActive();
	RefreshMatchmakingControls(bIsMatchmaking);
	if (bIsMatchmaking)
	{
		UpdateMatchmakingStatus(TEXT("현재 생존자 0명, 살인마 0명 대기중"), false);
	}

	LoginToEOS();
}

void UMainMenuWidget::BindMenuButtons()
{
	if (SurvivorButton)
	{
		SurvivorButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleSurvivorClicked);
	}

	if (KillerButton)
	{
		KillerButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleKillerClicked);
	}

	if (SettingsButton)
	{
		SettingsButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleSettingsClicked);
	}

	if (QuitButton)
	{
		QuitButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleQuitClicked);
	}

	if (CancelMatchmakingButton)
	{
		CancelMatchmakingButton->OnClicked.AddDynamic(this, &UMainMenuWidget::HandleCancelMatchmakingClicked);
	}
}

void UMainMenuWidget::BindOnlineEvents()
{
	UGameInstance* GameInstance = GetGameInstance();
	USPEOSSessionSubsystem* SessionSubsystem = GameInstance ? GameInstance->GetSubsystem<USPEOSSessionSubsystem>() : nullptr;
	if (!SessionSubsystem)
	{
		return;
	}

	SessionSubsystem->OnLoginCompleted.RemoveDynamic(this, &UMainMenuWidget::HandleOnlineLoginCompleted);
	SessionSubsystem->OnLoginCompleted.AddDynamic(this, &UMainMenuWidget::HandleOnlineLoginCompleted);
	SessionSubsystem->OnMatchmakingStatusChanged.RemoveDynamic(this, &UMainMenuWidget::HandleMatchmakingStatusChanged);
	SessionSubsystem->OnMatchmakingStatusChanged.AddDynamic(this, &UMainMenuWidget::HandleMatchmakingStatusChanged);
}

void UMainMenuWidget::BindLobbyStateEvents()
{
	ALobbyGameState* LobbyGameState = GetWorld() ? GetWorld()->GetGameState<ALobbyGameState>() : nullptr;
	if (!LobbyGameState)
	{
		return;
	}

	LobbyGameState->OnLobbyRoleCountChanged.RemoveDynamic(this, &UMainMenuWidget::HandleLobbyRoleCountChanged);
	LobbyGameState->OnLobbyRoleCountChanged.AddDynamic(this, &UMainMenuWidget::HandleLobbyRoleCountChanged);
	LobbyGameState->OnLobbyPhaseChanged.RemoveDynamic(this, &UMainMenuWidget::HandleLobbyPhaseChanged);
	LobbyGameState->OnLobbyPhaseChanged.AddDynamic(this, &UMainMenuWidget::HandleLobbyPhaseChanged);
	LobbyGameState->OnLobbyCountdownChanged.RemoveDynamic(this, &UMainMenuWidget::HandleLobbyCountdownChanged);
	LobbyGameState->OnLobbyCountdownChanged.AddDynamic(this, &UMainMenuWidget::HandleLobbyCountdownChanged);

	HandleLobbyRoleCountChanged(LobbyGameState->GetSurvivorCount(), LobbyGameState->GetKillerCount());
	HandleLobbyPhaseChanged(LobbyGameState->GetLobbyPhase());
}

void UMainMenuWidget::ApplyMenuLabels()
{
	if (TitleText)
	{
		TitleText->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UMainMenuWidget::EnsureTitleOnTop()
{
	UVerticalBox* MenuVBox = Cast<UVerticalBox>(GetWidgetFromName(TEXT("MenuVBox")));
	if (!MenuVBox || !TitleImage || TitleImage->GetParent() != MenuVBox)
	{
		return;
	}

	if (MenuVBox->GetChildIndex(TitleImage) == 0)
	{
		return;
	}

	TArray<UWidget*> OrderedChildren;
	OrderedChildren.Reserve(MenuVBox->GetChildrenCount());
	OrderedChildren.Add(TitleImage);

	for (int32 Index = 0; Index < MenuVBox->GetChildrenCount(); ++Index)
	{
		if (UWidget* Child = MenuVBox->GetChildAt(Index))
		{
			if (Child != TitleImage)
			{
				OrderedChildren.Add(Child);
			}
		}
	}

	while (MenuVBox->GetChildrenCount() > 0)
	{
		MenuVBox->RemoveChild(MenuVBox->GetChildAt(0));
	}

	for (UWidget* Child : OrderedChildren)
	{
		MenuVBox->AddChild(Child);
	}
}

void UMainMenuWidget::ApplyMenuTitleImage()
{
	if (!TitleImage)
	{
		return;
	}

	const FSlateBrush& ExistingBrush = TitleImage->GetBrush();
	const FVector2D DesignerImageSize = ExistingBrush.ImageSize;
	const bool bHasDesignerTexture = ExistingBrush.GetResourceObject() != nullptr;

	TitleImage->SetVisibility(ESlateVisibility::HitTestInvisible);

	if (bHasDesignerTexture)
	{
		FSlateBrush Brush = ExistingBrush;
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.Tiling = ESlateBrushTileType::NoTile;
		TitleImage->SetBrush(Brush);
		TitleImage->SetColorAndOpacity(FLinearColor::White);
		return;
	}

	const USPMainMenuStyleData& Style = GetResolvedStyle();
	UTexture2D* Texture = Style.TitleTexture;
	if (!Texture)
	{
		return;
	}

	TitleImage->SetBrushFromTexture(Texture, true);

	FSlateBrush Brush = TitleImage->GetBrush();
	if (!DesignerImageSize.IsNearlyZero())
	{
		Brush.ImageSize = DesignerImageSize;
	}
	Brush.DrawAs = ESlateBrushDrawType::Image;
	Brush.Tiling = ESlateBrushTileType::NoTile;
	TitleImage->SetBrush(Brush);
	TitleImage->SetColorAndOpacity(FLinearColor::White);
}

void UMainMenuWidget::ConfigureButtonStyle(
	UButton* Button,
	UTexture2D* NormalTexture,
	UTexture2D* HoveredTexture,
	float DefaultDisplayHeight,
	float HorizontalScale) const
{
	if (!Button || !NormalTexture)
	{
		return;
	}

	UTexture2D* const HoverTexture = HoveredTexture ? HoveredTexture : NormalTexture;

	FButtonStyle Style = Button->GetStyle();
	FVector2D ImageSize = Style.Normal.ImageSize;

	if (ImageSize.IsNearlyZero())
	{
		const float TextureWidth = static_cast<float>(NormalTexture->GetSizeX());
		const float TextureHeight = static_cast<float>(NormalTexture->GetSizeY());
		const float DisplayWidth = (TextureHeight > 0.0f ? DefaultDisplayHeight * (TextureWidth / TextureHeight) : DefaultDisplayHeight)
			* HorizontalScale;
		ImageSize = FVector2D(DisplayWidth, DefaultDisplayHeight);
	}

	auto ApplyBrush = [](FSlateBrush& Brush, UTexture2D* Texture, const FVector2D& Size)
	{
		Brush.SetResourceObject(Texture);
		Brush.ImageSize = Size;
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.Tiling = ESlateBrushTileType::NoTile;
		Brush.TintColor = FSlateColor(FLinearColor::White);
	};

	ApplyBrush(Style.Normal, NormalTexture, ImageSize);
	ApplyBrush(Style.Hovered, HoverTexture, ImageSize);
	ApplyBrush(Style.Pressed, NormalTexture, ImageSize);
	ApplyBrush(Style.Disabled, NormalTexture, ImageSize);
	Style.Pressed.TintColor = FSlateColor(FLinearColor(0.9f, 0.9f, 0.9f, 1.0f));

	Button->SetStyle(Style);
	Button->SetColorAndOpacity(FLinearColor::White);
}

void UMainMenuWidget::ApplyMenuButtonStyles()
{
	const USPMainMenuStyleData& Style = GetResolvedStyle();

	ConfigureButtonStyle(SurvivorButton, Style.SurvivorButtonNormal, Style.SurvivorButtonHovered, 52.0f, 1.12f);
	ConfigureButtonStyle(KillerButton, Style.KillerButtonNormal, Style.KillerButtonHovered, 52.0f, 1.12f);
	ConfigureButtonStyle(SettingsButton, Style.SettingsButtonNormal, Style.SettingsButtonHovered, 44.0f, 1.12f);
	ConfigureButtonStyle(QuitButton, Style.QuitButtonNormal, Style.QuitButtonHovered, 44.0f, 1.12f);

	if (UTextBlock* LegacyText = Cast<UTextBlock>(GetWidgetFromName(TEXT("SurvivorButtonText"))))
	{
		LegacyText->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (UTextBlock* LegacyText = Cast<UTextBlock>(GetWidgetFromName(TEXT("KillerButtonText"))))
	{
		LegacyText->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (UTextBlock* LegacyText = Cast<UTextBlock>(GetWidgetFromName(TEXT("SettingsButtonText"))))
	{
		LegacyText->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (UTextBlock* LegacyText = Cast<UTextBlock>(GetWidgetFromName(TEXT("QuitButtonText"))))
	{
		LegacyText->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UMainMenuWidget::HandleSurvivorClicked()
{
	SelectedMatchmakingRole = ELobbyPlayerRole::Survivor;
	OpenSurvivorLobby();
}

void UMainMenuWidget::HandleKillerClicked()
{
	SelectedMatchmakingRole = ELobbyPlayerRole::Killer;
	OpenKillerLobby();
}

void UMainMenuWidget::HandleSettingsClicked()
{
	OpenSettings();
}

void UMainMenuWidget::HandleQuitClicked()
{
	QuitGame();
}

void UMainMenuWidget::HandleCancelMatchmakingClicked()
{
	CancelOnlineMatchmaking();
}

void UMainMenuWidget::OpenSurvivorLobby()
{
	StartOnlineMatchmakingAsRole(ELobbyPlayerRole::Survivor);
}

void UMainMenuWidget::OpenKillerLobby()
{
	StartOnlineMatchmakingAsRole(ELobbyPlayerRole::Killer);
}

void UMainMenuWidget::LoginToEOS()
{
	UGameInstance* GameInstance = GetGameInstance();
	USPEOSSessionSubsystem* SessionSubsystem = GameInstance ? GameInstance->GetSubsystem<USPEOSSessionSubsystem>() : nullptr;
	if (SessionSubsystem)
	{
		SessionSubsystem->Login();
	}
}

void UMainMenuWidget::StartOnlineMatchmaking()
{
	StartOnlineMatchmakingAsRole(SelectedMatchmakingRole);
}

void UMainMenuWidget::StartOnlineMatchmakingAsRole(ELobbyPlayerRole SelectedRole)
{
	UGameInstance* GameInstance = GetGameInstance();
	USPEOSSessionSubsystem* SessionSubsystem = GameInstance ? GameInstance->GetSubsystem<USPEOSSessionSubsystem>() : nullptr;
	if (!SessionSubsystem)
	{
		return;
	}

	SelectedMatchmakingRole = SelectedRole;
	RefreshMatchmakingControls(true);
	UpdateMatchmakingStatus(TEXT("세션 찾는중"), false);
	SessionSubsystem->StartMatchmaking(SelectedRole, MainMenuLevelPath);
}

void UMainMenuWidget::CancelOnlineMatchmaking()
{
	UGameInstance* GameInstance = GetGameInstance();
	USPEOSSessionSubsystem* SessionSubsystem = GameInstance ? GameInstance->GetSubsystem<USPEOSSessionSubsystem>() : nullptr;
	if (SessionSubsystem)
	{
		SessionSubsystem->CancelMatchmaking();
	}

	SelectedMatchmakingRole = ELobbyPlayerRole::None;
	RefreshMatchmakingControls(false);
}

void UMainMenuWidget::UpdateMatchmakingStatus(const FString& StatusMessage, bool bIsError)
{
	if (MatchmakingStatusText)
	{
		MatchmakingStatusText->SetText(FText::FromString(StatusMessage));
		MatchmakingStatusText->SetVisibility(StatusMessage.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		MatchmakingStatusText->SetColorAndOpacity(bIsError ? FSlateColor(FLinearColor::Red) : FSlateColor(FLinearColor::White));
	}

	OnMainMenuMatchmakingStatusChanged.Broadcast(!bIsError, StatusMessage);
}

void UMainMenuWidget::HandleOnlineLoginCompleted(bool bWasSuccessful, const FString& StatusMessage)
{
	UE_LOG(LogTemp, Log, TEXT("MainMenu EOS login: %s (%s)"), bWasSuccessful ? TEXT("Success") : TEXT("Failed"), *StatusMessage);
}

void UMainMenuWidget::HandleMatchmakingStatusChanged(bool bWasSuccessful, const FString& StatusMessage)
{
	UE_LOG(LogTemp, Log, TEXT("MainMenu matchmaking: %s (%s)"), bWasSuccessful ? TEXT("Progress") : TEXT("Failed"), *StatusMessage);
	UpdateMatchmakingStatus(StatusMessage, !bWasSuccessful);
	if (!bWasSuccessful)
	{
		RefreshMatchmakingControls(false);
	}
}

void UMainMenuWidget::HandleLobbyRoleCountChanged(int32 SurvivorCount, int32 KillerCount)
{
	UpdateRoleCountStatus(SurvivorCount, KillerCount);
	OnMainMenuMatchmakingRoleCountChanged.Broadcast(SurvivorCount, KillerCount);
}

void UMainMenuWidget::HandleLobbyPhaseChanged(ELobbyPhase NewPhase)
{
	if (NewPhase == ELobbyPhase::Traveling)
	{
		UpdateMatchmakingStatus(TEXT("게임 입장중"), false);
		return;
	}

	if (NewPhase == ELobbyPhase::Countdown)
	{
		UpdateMatchmakingStatus(TEXT("역할 정원 충족. 게임 입장 준비중"), false);
	}
}

void UMainMenuWidget::HandleLobbyCountdownChanged(int32 CountdownRemainingTime)
{
	if (CountdownRemainingTime > 0)
	{
		UpdateMatchmakingStatus(FString::Printf(TEXT("게임 입장까지 %d초"), CountdownRemainingTime), false);
	}
}

void UMainMenuWidget::OpenSettings()
{
	UE_LOG(LogTemp, Log, TEXT("MainMenu: Settings selected (hook up settings screen when ready)."));
}

void UMainMenuWidget::OpenLevelAtPath(const FString& LevelPath)
{
	if (LevelPath.IsEmpty())
	{
		return;
	}

	UGameplayStatics::OpenLevel(this, FName(*LevelPath));
}

void UMainMenuWidget::RefreshMatchmakingControls(bool bIsMatchmaking)
{
	if (SurvivorButton)
	{
		SurvivorButton->SetIsEnabled(!bIsMatchmaking);
	}
	if (KillerButton)
	{
		KillerButton->SetIsEnabled(!bIsMatchmaking);
	}
	if (CancelMatchmakingButton)
	{
		CancelMatchmakingButton->SetVisibility(bIsMatchmaking ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UMainMenuWidget::UpdateRoleCountStatus(int32 SurvivorCount, int32 KillerCount)
{
	const ALobbyGameState* LobbyGameState = GetWorld() ? GetWorld()->GetGameState<ALobbyGameState>() : nullptr;
	if (!LobbyGameState || !MatchmakingStatusText)
	{
		return;
	}

	UGameInstance* GameInstance = GetGameInstance();
	const USPEOSSessionSubsystem* SessionSubsystem = GameInstance ? GameInstance->GetSubsystem<USPEOSSessionSubsystem>() : nullptr;
	if ((!SessionSubsystem || !SessionSubsystem->IsMatchmakingActive()) && SelectedMatchmakingRole == ELobbyPlayerRole::None)
	{
		return;
	}

	if (LobbyGameState->GetLobbyPhase() == ELobbyPhase::WaitingForPlayers)
	{
		MatchmakingStatusText->SetText(FText::FromString(FString::Printf(
			TEXT("현재 생존자 %d/%d명, 살인마 %d/%d명 대기중"),
			SurvivorCount,
			LobbyGameState->GetSurvivorLimit(),
			KillerCount,
			LobbyGameState->GetKillerLimit())));
		MatchmakingStatusText->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void UMainMenuWidget::QuitGame()
{
	APlayerController* PlayerController = GetOwningPlayer();
	if (PlayerController)
	{
		UKismetSystemLibrary::QuitGame(this, PlayerController, EQuitPreference::Quit, false);
	}
}

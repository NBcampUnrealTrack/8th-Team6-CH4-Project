#include "UI/MainMenuWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Systems/SPEOSSessionSubsystem.h"
#include "Styling/SlateTypes.h"
#include "UI/HUDFontUtils.h"

namespace
{
	static const TCHAR* SurvivorButtonTexturePath =
		TEXT("/Game/UI/MainMenu/Textures/T_MainMenu_Btn_SURVIVOR.T_MainMenu_Btn_SURVIVOR");
	static const TCHAR* KillerButtonTexturePath =
		TEXT("/Game/UI/MainMenu/Textures/T_MainMenu_Btn_KILLER.T_MainMenu_Btn_KILLER");
	static const TCHAR* SettingsButtonTexturePath =
		TEXT("/Game/UI/MainMenu/Textures/T_MainMenu_Btn_SETTINGS.T_MainMenu_Btn_SETTINGS");
	static const TCHAR* QuitButtonTexturePath =
		TEXT("/Game/UI/MainMenu/Textures/T_MainMenu_Btn_QUIT.T_MainMenu_Btn_QUIT");
	static const TCHAR* SurvivorButtonHoverTexturePath =
		TEXT("/Game/UI/MainMenu/Textures/T_MainMenu_Btn_SURVIVOR_Hover.T_MainMenu_Btn_SURVIVOR_Hover");
	static const TCHAR* KillerButtonHoverTexturePath =
		TEXT("/Game/UI/MainMenu/Textures/T_MainMenu_Btn_KILLER_Hover.T_MainMenu_Btn_KILLER_Hover");
	static const TCHAR* SettingsButtonHoverTexturePath =
		TEXT("/Game/UI/MainMenu/Textures/T_MainMenu_Btn_SETTINGS_Hover.T_MainMenu_Btn_SETTINGS_Hover");
	static const TCHAR* QuitButtonHoverTexturePath =
		TEXT("/Game/UI/MainMenu/Textures/T_MainMenu_Btn_QUIT_Hover.T_MainMenu_Btn_QUIT_Hover");
	static const TCHAR* TitleTexturePath =
		TEXT("/Game/UI/MainMenu/Textures/T_MainMenu_Title.T_MainMenu_Title");

	UTexture2D* LoadMenuTexture(const TCHAR* AssetPath)
	{
		return Cast<UTexture2D>(StaticLoadObject(UTexture2D::StaticClass(), nullptr, AssetPath));
	}
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
	if (!TitleImage)
	{
		TitleImage = Cast<UImage>(GetWidgetFromName(TEXT("TitleImage")));
	}

	BindMenuButtons();
	BindOnlineEvents();
	ApplyMenuLabels();
	EnsureTitleOnTop();
	ApplyMenuTitleImage();
	ApplyMenuButtonStyles();
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

	// Designer / WBP brush settings win. Only apply project fallback when nothing is assigned.
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

	UTexture2D* Texture = LoadMenuTexture(TitleTexturePath);
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
	ConfigureButtonStyle(
		SurvivorButton,
		LoadMenuTexture(SurvivorButtonTexturePath),
		LoadMenuTexture(SurvivorButtonHoverTexturePath),
		52.0f,
		1.12f);
	ConfigureButtonStyle(
		KillerButton,
		LoadMenuTexture(KillerButtonTexturePath),
		LoadMenuTexture(KillerButtonHoverTexturePath),
		52.0f,
		1.12f);
	ConfigureButtonStyle(
		SettingsButton,
		LoadMenuTexture(SettingsButtonTexturePath),
		LoadMenuTexture(SettingsButtonHoverTexturePath),
		44.0f,
		1.12f);
	ConfigureButtonStyle(
		QuitButton,
		LoadMenuTexture(QuitButtonTexturePath),
		LoadMenuTexture(QuitButtonHoverTexturePath),
		44.0f,
		1.12f);

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
	OpenSurvivorLobby();
}

void UMainMenuWidget::HandleKillerClicked()
{
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

void UMainMenuWidget::OpenSurvivorLobby()
{
	StartOnlineMatchmaking();
}

void UMainMenuWidget::OpenKillerLobby()
{
	StartOnlineMatchmaking();
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
	UGameInstance* GameInstance = GetGameInstance();
	USPEOSSessionSubsystem* SessionSubsystem = GameInstance ? GameInstance->GetSubsystem<USPEOSSessionSubsystem>() : nullptr;
	if (!SessionSubsystem)
	{
		return;
	}

	SessionSubsystem->StartMatchmaking(SurvivorLobbyLevelPath);
}

void UMainMenuWidget::HandleOnlineLoginCompleted(bool bWasSuccessful, const FString& StatusMessage)
{
	UE_LOG(LogTemp, Log, TEXT("MainMenu EOS login: %s (%s)"), bWasSuccessful ? TEXT("Success") : TEXT("Failed"), *StatusMessage);
}

void UMainMenuWidget::HandleMatchmakingStatusChanged(bool bWasSuccessful, const FString& StatusMessage)
{
	UE_LOG(LogTemp, Log, TEXT("MainMenu matchmaking: %s (%s)"), bWasSuccessful ? TEXT("Progress") : TEXT("Failed"), *StatusMessage);
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

void UMainMenuWidget::QuitGame()
{
	APlayerController* PlayerController = GetOwningPlayer();
	if (PlayerController)
	{
		UKismetSystemLibrary::QuitGame(this, PlayerController, EQuitPreference::Quit, false);
	}
}

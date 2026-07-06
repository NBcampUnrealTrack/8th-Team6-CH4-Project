#include "UI/GameHUD.h"

#include "Blueprint/UserWidget.h"
#include "UI/GameHUDWidget.h"
#include "UObject/ConstructorHelpers.h"

AGameHUD::AGameHUD()
{
	static ConstructorHelpers::FClassFinder<UGameHUDWidget> WidgetClassFinder(
		TEXT("/Game/Blueprints/UI/WBP_GameHUD.WBP_GameHUD_C"));
	if (WidgetClassFinder.Succeeded())
	{
		GameHUDWidgetClass = WidgetClassFinder.Class;
	}
}

void AGameHUD::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* PlayerController = GetOwningPlayerController();
	if (!PlayerController || !PlayerController->IsLocalController())
	{
		return;
	}

	if (!GameHUDWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("AGameHUD: GameHUDWidgetClass is not set."));
		return;
	}

	GameHUDWidget = CreateWidget<UGameHUDWidget>(PlayerController, GameHUDWidgetClass);
	if (!GameHUDWidget)
	{
		UE_LOG(LogTemp, Warning, TEXT("AGameHUD: Failed to create HUD widget from %s."),
			*GetNameSafe(GameHUDWidgetClass));
		return;
	}

	GameHUDWidget->AddToViewport();
	GameHUDWidget->RefreshAll();
}

void AGameHUD::RefreshInventoryPanels()
{
	if (GameHUDWidget)
	{
		GameHUDWidget->RefreshInventoryAndPerkPanels();
	}
}

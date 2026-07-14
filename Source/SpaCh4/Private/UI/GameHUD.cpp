#include "UI/GameHUD.h"

#include "Blueprint/UserWidget.h"
#include "UI/GameHUDWidget.h"

AGameHUD::AGameHUD()
{
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
		UE_LOG(LogTemp, Warning, TEXT("AGameHUD: GameHUDWidgetClass is not set. Assign WBP_GameHUD on BP_GameHUD."));
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
	UE_LOG(LogTemp, Warning, TEXT("AGameHUD: Created HUD widget %s"), *GetNameSafe(GameHUDWidget));
}

void AGameHUD::RefreshInventoryPanels()
{
	if (GameHUDWidget)
	{
		GameHUDWidget->RefreshInventoryAndPerkPanels();
	}
}

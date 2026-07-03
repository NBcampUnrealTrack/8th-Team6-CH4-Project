#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MainMenuWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;

UCLASS(Abstract, Blueprintable)
class SPACH4_API UMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "MainMenu")
	void OpenSurvivorLobby();

	UFUNCTION(BlueprintCallable, Category = "MainMenu")
	void OpenKillerLobby();

	UFUNCTION(BlueprintCallable, Category = "MainMenu")
	void OpenSettings();

	UFUNCTION(BlueprintCallable, Category = "MainMenu")
	void QuitGame();

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> TitleImage;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> SurvivorButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> KillerButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> SettingsButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> QuitButton;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MainMenu|Travel")
	FString SurvivorLobbyLevelPath = TEXT("/Game/Developers/qkrwl/Collections/Maps/TestLobbyLevel");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MainMenu|Travel")
	FString KillerLobbyLevelPath = TEXT("/Game/Developers/qkrwl/Collections/Maps/TestLobbyLevel");

private:
	UFUNCTION()
	void HandleSurvivorClicked();

	UFUNCTION()
	void HandleKillerClicked();

	UFUNCTION()
	void HandleSettingsClicked();

	UFUNCTION()
	void HandleQuitClicked();

	void BindMenuButtons();
	void ApplyMenuLabels();
	void ApplyMenuTitleImage();
	void ApplyMenuButtonStyles();
	void EnsureTitleOnTop();
	void OpenLevelAtPath(const FString& LevelPath);

	void ConfigureButtonStyle(
		UButton* Button,
		UTexture2D* NormalTexture,
		UTexture2D* HoveredTexture,
		float DefaultDisplayHeight,
		float HorizontalScale = 1.0f) const;
};

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Systems/LobbyGameState.h"
#include "MainMenuWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;
class USPMainMenuStyleData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMainMenuMatchmakingStatusSignature, bool, bWasSuccessful, const FString&, StatusMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMainMenuMatchmakingRoleCountSignature, int32, SurvivorCount, int32, KillerCount);

UCLASS(Abstract, Blueprintable)
class SPACH4_API UMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "MainMenu")
	void OpenSurvivorLobby();

	UFUNCTION(BlueprintCallable, Category = "MainMenu")
	void OpenKillerLobby();

	UFUNCTION(BlueprintCallable, Category = "MainMenu|Online")
	void LoginToEOS();

	UFUNCTION(BlueprintCallable, Category = "MainMenu|Online")
	void StartOnlineMatchmaking();

	UFUNCTION(BlueprintCallable, Category = "MainMenu|Online")
	void CancelOnlineMatchmaking();

	UFUNCTION(BlueprintCallable, Category = "MainMenu|Online")
	void UpdateMatchmakingStatus(const FString& StatusMessage, bool bIsError);

	UFUNCTION(BlueprintCallable, Category = "MainMenu")
	void OpenSettings();

	UFUNCTION(BlueprintCallable, Category = "MainMenu")
	void QuitGame();

	UPROPERTY(BlueprintAssignable, Category = "MainMenu|Online")
	FMainMenuMatchmakingStatusSignature OnMainMenuMatchmakingStatusChanged;

	UPROPERTY(BlueprintAssignable, Category = "MainMenu|Online")
	FMainMenuMatchmakingRoleCountSignature OnMainMenuMatchmakingRoleCountChanged;

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

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> CancelMatchmakingButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MatchmakingStatusText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MainMenu|Travel")
	FString MainMenuLevelPath;

	/** 미지정 시 /Game/UI/Data/DA_MainMenuStyle 또는 SPUIStyleLibrary 기본값 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MainMenu|Style")
	TObjectPtr<USPMainMenuStyleData> VisualStyle;

private:
	const USPMainMenuStyleData& GetResolvedStyle() const;

	UFUNCTION()
	void HandleSurvivorClicked();

	UFUNCTION()
	void HandleKillerClicked();

	UFUNCTION()
	void HandleSettingsClicked();

	UFUNCTION()
	void HandleQuitClicked();

	UFUNCTION()
	void HandleCancelMatchmakingClicked();

	UFUNCTION()
	void HandleOnlineLoginCompleted(bool bWasSuccessful, const FString& StatusMessage);

	UFUNCTION()
	void HandleMatchmakingStatusChanged(bool bWasSuccessful, const FString& StatusMessage);

	UFUNCTION()
	void HandleLobbyRoleCountChanged(int32 SurvivorCount, int32 KillerCount);

	UFUNCTION()
	void HandleLobbyPhaseChanged(ELobbyPhase NewPhase);

	UFUNCTION()
	void HandleLobbyCountdownChanged(int32 CountdownRemainingTime);

	void BindMenuButtons();
	void BindOnlineEvents();
	void BindLobbyStateEvents();
	void ApplyMenuLabels();
	void ApplyMenuTitleImage();
	void ApplyMenuButtonStyles();
	void EnsureTitleOnTop();
	void OpenLevelAtPath(const FString& LevelPath);
	void StartOnlineMatchmakingAsRole(ELobbyPlayerRole SelectedRole);
	void RefreshMatchmakingControls(bool bIsMatchmaking);
	void UpdateRoleCountStatus(int32 SurvivorCount, int32 KillerCount);

	void ConfigureButtonStyle(
		UButton* Button,
		UTexture2D* NormalTexture,
		UTexture2D* HoveredTexture,
		float DefaultDisplayHeight,
		float HorizontalScale = 1.0f) const;

	ELobbyPlayerRole SelectedMatchmakingRole = ELobbyPlayerRole::None;
};

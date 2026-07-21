// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Player/LDPlayerState.h"
#include "Systems/MatchGameState.h"
#include "GameResultWidget.generated.h"

class UTextBlock;
class UButton;
class UImage;
class UOverlay;
class UTexture2D;
class UWidget;
class UWidgetSwitcher;
class USPGameResultStyleData;
class USPResultPrimaryStatWidget;
class USPResultStatRowWidget;
/**
 * 
 */
UCLASS()
class SPACH4_API UGameResultWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UFUNCTION()
	void SetResultText(EMatchResult MatchResult);
	
	UFUNCTION()
	void SetInformationText(FString InfoText);

	UFUNCTION(BlueprintCallable, Category = "GameResult|Stats")
	void SetSurvivorStats(const FSurvivorMatchStats& SurvivorStats);

	UFUNCTION(BlueprintCallable, Category = "GameResult|Stats")
	void SetKillerStats(const FKillerMatchStats& KillerStats);

	UFUNCTION(BlueprintCallable, Category = "GameResult|Animation")
	void PlayResultIntroAnimation();
	
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ResultTitleText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ScoreText;
	
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> InformationText;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> ReturnToMainMenuButton;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "GameResult|Animation")
	TObjectPtr<UOverlay> Overlay_Main;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "GameResult|Player")
	TObjectPtr<UTextBlock> Text_PlayerNickname;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "GameResult|Player")
	TObjectPtr<UTextBlock> Text_PlayerRole;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "GameResult|Result")
	TObjectPtr<UTextBlock> Text_ResultTitle;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "GameResult|Result")
	TObjectPtr<UTextBlock> Text_PersonalOutcome;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "GameResult|Visual")
	TObjectPtr<UImage> Image_ResultBackground;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "GameResult|Visual")
	TObjectPtr<UImage> Image_RoleBackground;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "GameResult|Visual")
	TObjectPtr<UImage> Image_RoleCharacter;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "GameResult|Visual")
	TObjectPtr<UImage> Image_ResultEmblem;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "GameResult|Stats")
	TObjectPtr<UTextBlock> Text_StatHeader;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "GameResult|Stats")
	TObjectPtr<UWidgetSwitcher> WidgetSwitcher_Stats;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "GameResult|Stats|Survivor")
	TObjectPtr<USPResultPrimaryStatWidget> WBP_ResultStat_Survivor;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "GameResult|Stats|Killer")
	TObjectPtr<USPResultPrimaryStatWidget> WBP_ResultStat_Killer;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "GameResult|Stats|Survivor")
	TObjectPtr<USPResultStatRowWidget> WBP_ResultStatRow_DeliveryCount;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "GameResult|Stats|Survivor")
	TObjectPtr<USPResultStatRowWidget> WBP_ResultStatRow_Rescue;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "GameResult|Stats|Survivor")
	TObjectPtr<USPResultStatRowWidget> WBP_ResultStatRow_SelfHeal;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "GameResult|Stats|Survivor")
	TObjectPtr<USPResultStatRowWidget> WBP_ResultStatRow_Caged;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "GameResult|Stats|Killer")
	TObjectPtr<USPResultStatRowWidget> WBP_ResultStatRow_ValidHitCount;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "GameResult|Stats|Killer")
	TObjectPtr<USPResultStatRowWidget> WBP_ResultStatRow_DownCount;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "GameResult|Stats|Killer")
	TObjectPtr<USPResultStatRowWidget> WBP_ResultStatRow_CageCount;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "GameResult|Stats|Killer")
	TObjectPtr<USPResultStatRowWidget> WBP_ResultStatRow_Temp;

	/** 미지정 시 /Game/UI/Data/DA_GameResultStyle 또는 기본 결과 텍스처를 사용합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GameResult|Style")
	TObjectPtr<USPGameResultStyleData> VisualStyle;

private:
	const USPGameResultStyleData& GetResolvedStyle() const;
	void ApplyBackground();
	void ApplyRoleVisual(ELobbyPlayerRole PlayerRole);
	void ApplyResultEmblem(EMatchResult MatchResult);
	void UpdateResultIntroAnimation();
	void CompleteResultIntroAnimation();
	void SetStatsRenderOpacity(float SurvivorPrimaryOpacity, float KillerPrimaryOpacity, const TArray<float>& StageOpacities);
	void RefreshPlayerIdentity();
	void SetPrimaryStat(
		USPResultPrimaryStatWidget* PrimaryStatWidget,
		const FText& Label,
		const FText& Description,
		int32 Value,
		UTexture2D* IconTexture);
	bool HasStructuredStatsWidgets() const;
	static float GetIntroAnimationAlpha(float ElapsedTime, float StartTime, float Duration);
	static void SetWidgetRenderOpacity(UWidget* Widget, float Opacity);
	static void SetImageTexture(UImage* Image, UTexture2D* Texture);

	EMatchResult CurrentMatchResult = EMatchResult::None;
	FVector2D ResultTitleBaseTranslation = FVector2D::ZeroVector;
	float ResultIntroElapsedTime = 0.0f;
	bool bIsResultIntroPlaying = false;
	bool bHasPlayedResultIntro = false;
};

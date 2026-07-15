#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/HUDTypes.h"
#include "GameHUDWidget.generated.h"

class UImage;
class UProgressBar;
class UTextBlock;
class UTeammateEntryWidget;
class UWidget;
class UMaterialInstanceDynamic;
class AMatchGameState;
class USPGameHUDStyleData;

UCLASS(Abstract, Blueprintable)
class SPACH4_API UGameHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "HUD")
	void RefreshAll();

	UFUNCTION(BlueprintCallable, Category = "HUD")
	void RefreshInventoryAndPerkPanels();

	UFUNCTION(BlueprintCallable, Category = "HUD")
	TArray<FTeammateHUDData> GatherTeammateData() const;

	UFUNCTION(BlueprintCallable, Category = "HUD")
	TArray<FDeliveryHUDData> GatherDeliveryData() const;

	UFUNCTION(BlueprintCallable, Category = "HUD")
	TArray<FInventorySlotHUDData> GatherInventoryData() const;

	UFUNCTION(BlueprintCallable, Category = "HUD")
	TArray<FPerkHUDData> GatherPerkData() const;

protected:
	/** true면 MatchGameState 없을 때만 Preview 데이터 사용 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|Preview")
	bool bUsePreviewData = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|Preview")
	TArray<FTeammateHUDData> PreviewTeammateData;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|Preview")
	TArray<FInventorySlotHUDData> PreviewInventoryData;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|Preview")
	TArray<FPerkHUDData> PreviewPerkData;

	/** 미지정 시 /Game/UI/Data/DA_GameHUDStyle 또는 SPUIStyleLibrary 기본값 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|Style")
	TObjectPtr<USPGameHUDStyleData> VisualStyle;

	/** 0이면 DeliveryProgressFill A/B 각 Image의 Brush Image Size 사용. A/B 동일 크기 강제 시 여기 지정 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|Delivery", meta = (ClampMin = "0"))
	FVector2D DeliveryFillMaxSizeOverride = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "HUD")
	TArray<TObjectPtr<UTeammateEntryWidget>> TeammateEntries;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "HUD")
	TObjectPtr<UTextBlock> DeliveryLabelA;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "HUD")
	TObjectPtr<UTextBlock> DeliveryLabelB;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "HUD")
	TObjectPtr<UImage> DeliveryIconA;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "HUD")
	TObjectPtr<UImage> DeliveryIconB;

	/** Single composite progress bar (legacy — used when overlay fill is absent) */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "HUD")
	TObjectPtr<UImage> DeliveryProgressBarA;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "HUD")
	TObjectPtr<UImage> DeliveryProgressBarB;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "HUD")
	TObjectPtr<UWidget> DeliveryProgressRootA;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "HUD")
	TObjectPtr<UWidget> DeliveryProgressRootB;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "HUD")
	TObjectPtr<UImage> DeliveryProgressBGA;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "HUD")
	TObjectPtr<UImage> DeliveryProgressBGB;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "HUD")
	TObjectPtr<UImage> DeliveryProgressFillA;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "HUD")
	TObjectPtr<UImage> DeliveryProgressFillB;

	/** Legacy 10-segment stack — used when progress bar widget is absent */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "HUD")
	TArray<TObjectPtr<UImage>> DeliveryStackA;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "HUD")
	TArray<TObjectPtr<UImage>> DeliveryStackB;

	/** Designer ProgressBar — preferred when Fill Image is set in WBP */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "HUD")
	TObjectPtr<UProgressBar> DeliveryProgressA;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "HUD")
	TObjectPtr<UProgressBar> DeliveryProgressB;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "HUD")
	TObjectPtr<UTextBlock> DeliveryValueA;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "HUD")
	TObjectPtr<UTextBlock> DeliveryValueB;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "HUD")
	TArray<TObjectPtr<UImage>> InventorySlots;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "HUD")
	TArray<TObjectPtr<UImage>> InventoryIcons;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "HUD")
	TArray<TObjectPtr<UImage>> PerkSlots;

	UFUNCTION(BlueprintImplementableEvent, Category = "HUD")
	void OnDeliveryDataUpdated(const TArray<FDeliveryHUDData>& DeliveryData);

	UFUNCTION(BlueprintImplementableEvent, Category = "HUD")
	void OnInventoryDataUpdated(const TArray<FInventorySlotHUDData>& InventoryData);

	UFUNCTION(BlueprintImplementableEvent, Category = "HUD")
	void OnPerkDataUpdated(const TArray<FPerkHUDData>& PerkData);

	/** 게임플레이 연동 시 BP/C++에서 오버라이드 */
	UFUNCTION(BlueprintNativeEvent, Category = "HUD")
	TArray<FTeammateHUDData> BuildTeammateDataFromGameplay() const;

	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION()
	void HandleDeliveryProgressChanged(int32 StationAValue, int32 StationBValue, int32 TotalDeliveredValue, float DeliveryProgress);

	UFUNCTION()
	void HandleMatchPlayersChanged();

	UFUNCTION()
	void HandleSurvivorStateChanged(FName SurvivorId, ESurvivorState SurvivorState);

private:
	void BindInventoryWidgets();
	void ApplyDeliveryRowLabels();
	void RefreshTeammateEntries();
	void RefreshDeliveryPanel();
	void RefreshMatchHudPanels();
	void ApplyDeliveryStationVisuals(
		int32 CurrentValue,
		int32 TargetValue,
		UProgressBar* ProgressBar,
		UWidget* Root,
		UImage* FrameImage,
		UImage* FillImage,
		UImage* LegacyBar,
		const TArray<TObjectPtr<UImage>>& StackWidgets,
		UTextBlock* ValueLabel);
	void RefreshInventoryPanel();
	void RefreshPerkPanel();
	void EnsurePreviewDefaults();
	void SetupDeliveryProgressBars();
	void FinalizeDeliveryProgressSetup();
	void ClearDeliveryProgressSetupTimer();
	void CacheDesignerDeliveryFillSizes();
	void ResolveDeliveryWidgetBindings();
	void EnsureDeliveryFillWidgets();
	void BindMatchStateDelegates();
	void UnbindMatchStateDelegates();
	bool CanRunDeferredSetup() const;
	FVector2D ResolveDeliveryFillMaxSize(UImage* FillImage, const FVector2D& FallbackMaxSize) const;
	const USPGameHUDStyleData& GetResolvedStyle() const;
	void UpdateDeliveryProgress(
		UWidget* Root,
		UImage* FrameImage,
		UImage* FillImage,
		UImage* LegacyBar,
		const TArray<TObjectPtr<UImage>>& StackWidgets,
		int32 CurrentValue,
		int32 TargetValue);

	void UpdateDeliveryProgressBar(
		UProgressBar* ProgressBar,
		UImage* FillImage,
		int32 CurrentValue,
		int32 TargetValue,
		TObjectPtr<UMaterialInstanceDynamic>& ProgressBarFillMID);

	bool ShouldPreferDesignerProgressBar(UProgressBar* ProgressBar, UImage* FillImage) const;
	void ApplyDeliveryFillProgressVisual(UImage* FillImage, UImage* FrameImage, float ProgressPercent);
	void EnsureProgressBarHasFillBrush(UProgressBar* ProgressBar);
	void EnsureProgressBarDesignerFillMID(
		UProgressBar* ProgressBar,
		TObjectPtr<UMaterialInstanceDynamic>& FillMID);
	AMatchGameState* GetMatchGameState() const;
	int32 GetSelectedInventorySlot() const;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DeliveryProgressFillMIDA;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DeliveryProgressFillMIDB;

	UPROPERTY(Transient)
	FVector2D CachedDeliveryFillMaxSizeA = FVector2D::ZeroVector;

	UPROPERTY(Transient)
	FVector2D CachedDeliveryFillMaxSizeB = FVector2D::ZeroVector;

	FTimerHandle DeliveryProgressSetupTimerHandle;
	FTimerHandle TeammateRefreshTimerHandle;
	FTimerHandle MatchDelegateBindRetryTimerHandle;

	bool bMatchDelegatesBound = false;
	bool bDeliveryProgressBarARaised = false;
	bool bDeliveryProgressBarBRaised = false;
};

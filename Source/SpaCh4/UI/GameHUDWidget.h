#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HUDTypes.h"
#include "GameHUDWidget.generated.h"

class UImage;
class UProgressBar;
class UTextBlock;
class UTeammateEntryWidget;
class AMatchGameState;

UCLASS(Abstract, Blueprintable)
class SPACH4_API UGameHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "HUD")
	void RefreshAll();

	UFUNCTION(BlueprintCallable, Category = "HUD")
	TArray<FTeammateHUDData> GatherTeammateData() const;

	UFUNCTION(BlueprintCallable, Category = "HUD")
	TArray<FDeliveryHUDData> GatherDeliveryData() const;

	UFUNCTION(BlueprintCallable, Category = "HUD")
	TArray<FInventorySlotHUDData> GatherInventoryData() const;

	UFUNCTION(BlueprintCallable, Category = "HUD")
	TArray<FPerkHUDData> GatherPerkData() const;

protected:
	/** L_UI_Test 등 레이아웃 작업용. 게임플레이 연동 전까지 true 유지 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|Preview")
	bool bUsePreviewData = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|Preview")
	TArray<FTeammateHUDData> PreviewTeammateData;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|Preview")
	TArray<FInventorySlotHUDData> PreviewInventoryData;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|Preview")
	TArray<FPerkHUDData> PreviewPerkData;

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

	/** Single composite progress bar (preferred — no segment stretch) */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "HUD")
	TObjectPtr<UImage> DeliveryProgressBarA;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "HUD")
	TObjectPtr<UImage> DeliveryProgressBarB;

	/** Legacy 10-segment stack — used when progress bar widget is absent */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "HUD")
	TArray<TObjectPtr<UImage>> DeliveryStackA;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "HUD")
	TArray<TObjectPtr<UImage>> DeliveryStackB;

	/** Legacy widgets — hidden after DBD-style delivery layout */
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

	virtual void NativeConstruct() override;

private:
	void ApplyHUDFonts();
	void ApplyDeliveryRowLabels();
	void RefreshTeammateEntries();
	void RefreshDeliveryPanel();
	void RefreshInventoryPanel();
	void RefreshPerkPanel();
	void EnsurePreviewDefaults();
	void UpdateDeliveryProgress(UImage* ProgressBar, const TArray<TObjectPtr<UImage>>& StackWidgets, int32 CurrentValue, int32 TargetValue);
	AMatchGameState* GetMatchGameState() const;
};

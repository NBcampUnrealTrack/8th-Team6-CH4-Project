#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/HUDTypes.h"
#include "TeammateEntryWidget.generated.h"

class UImage;
class UMaterialInstanceDynamic;
class UTextBlock;
class UWidget;

UCLASS(Abstract, Blueprintable)
class SPACH4_API UTeammateEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "HUD")
	void UpdateFromData(const FTeammateHUDData& Data);

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> PortraitImage;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> PortraitSlotFrame;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NicknameText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CageStackText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> DownedHealthBarRoot;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> DownedHealthBarBG;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> DownedHealthBarFill;

	UFUNCTION(BlueprintImplementableEvent, Category = "HUD")
	void OnDisplayStateChanged(ESurvivorDisplayState NewState);

private:
	static constexpr float DownedHealthBarWidth = 248.0f;
	static constexpr float DownedHealthBarHeight = 27.0f;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DownedHealthBarFillMID;

	void SetupDownedHealthBar();
	void UpdateNickname(const FText& Nickname);
	void UpdateCageStack(int32 Stack, int32 MaxStack);
	void UpdateDownedHealth(float HealthPercent, bool bShowBar);
	void UpdatePortraitFrame(ESurvivorDisplayState State);
};

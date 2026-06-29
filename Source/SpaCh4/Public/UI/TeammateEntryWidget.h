#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/HUDTypes.h"
#include "TeammateEntryWidget.generated.h"

class UImage;
class UProgressBar;
class UTextBlock;

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
	TObjectPtr<UProgressBar> DownedHealthBar;

	UFUNCTION(BlueprintImplementableEvent, Category = "HUD")
	void OnDisplayStateChanged(ESurvivorDisplayState NewState);

private:
	void UpdateNickname(const FText& Nickname);
	void UpdateCageStack(int32 Stack, int32 MaxStack);
	void UpdateDownedHealth(float HealthPercent, bool bShowBar);
	void UpdatePortraitFrame(ESurvivorDisplayState State);
};

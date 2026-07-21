#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SPResultStatRowWidget.generated.h"

class UImage;
class UTextBlock;
class UTexture2D;

UCLASS(Abstract, Blueprintable)
class SPACH4_API USPResultStatRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "GameResult|Stats")
	void SetStat(const FText& Label, int32 Value);

	UFUNCTION(BlueprintCallable, Category = "GameResult|Stats")
	void SetStatText(const FText& Label, const FText& Value);

	UFUNCTION(BlueprintCallable, Category = "GameResult|Stats")
	void SetStatIcon(UTexture2D* IconTexture);

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "GameResult|Stats")
	TObjectPtr<UImage> Image_Icon;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "GameResult|Stats")
	TObjectPtr<UTextBlock> Text_StatLabel;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "GameResult|Stats")
	TObjectPtr<UTextBlock> Text_Value;
};

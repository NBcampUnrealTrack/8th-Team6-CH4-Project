#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "GameHUD.generated.h"

class UGameHUDWidget;

UCLASS()
class SPACH4_API AGameHUD : public AHUD
{
	GENERATED_BODY()

public:
	AGameHUD();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD")
	TSubclassOf<UGameHUDWidget> GameHUDWidgetClass;

	UPROPERTY(BlueprintReadOnly, Category = "HUD")
	TObjectPtr<UGameHUDWidget> GameHUDWidget;
};

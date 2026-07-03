#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "MainMenuPlayerController.generated.h"

UCLASS()
class SPACH4_API AMainMenuPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AMainMenuPlayerController();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MainMenu|Camera")
	FName MainMenuCameraTag = TEXT("MainMenuCamera");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MainMenu|Camera")
	float LockedExposureBias = -1.05f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MainMenu|Camera")
	float LockedExposureBrightness = 0.16f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MainMenu|UI")
	TSubclassOf<class UMainMenuWidget> MainMenuWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<class UMainMenuWidget> MainMenuWidgetInstance;

	void FocusMainMenuCamera();
	void InstallExposureLockModifier();
	void ShowMainMenuWidget();
};

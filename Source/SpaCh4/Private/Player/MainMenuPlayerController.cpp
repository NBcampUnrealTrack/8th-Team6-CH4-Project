#include "Player/MainMenuPlayerController.h"

#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Player/MainMenuExposureCameraModifier.h"
#include "UI/MainMenuWidget.h"

AMainMenuPlayerController::AMainMenuPlayerController()
{
	bShowMouseCursor = true;
}

void AMainMenuPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController())
	{
		return;
	}

	if (GEngine)
	{
		GEngine->Exec(GetWorld(), TEXT("r.EyeAdaptationQuality 0"));
	}

	FocusMainMenuCamera();
	InstallExposureLockModifier();
	ShowMainMenuWidget();
}

void AMainMenuPlayerController::FocusMainMenuCamera()
{
	ACameraActor* MenuCamera = nullptr;

	for (TActorIterator<ACameraActor> It(GetWorld()); It; ++It)
	{
		if (It->ActorHasTag(MainMenuCameraTag))
		{
			MenuCamera = *It;
			break;
		}
	}

	if (!MenuCamera)
	{
		UE_LOG(LogTemp, Warning, TEXT("MainMenuPlayerController: No camera tagged '%s' found."), *MainMenuCameraTag.ToString());
		return;
	}

	SetViewTargetWithBlend(MenuCamera, 0.0f);
}

void AMainMenuPlayerController::InstallExposureLockModifier()
{
	APlayerCameraManager* CameraManager = PlayerCameraManager;
	if (!CameraManager)
	{
		return;
	}

	if (CameraManager->FindCameraModifierByClass(UMainMenuExposureCameraModifier::StaticClass()))
	{
		return;
	}

	UCameraModifier* NewModifier = CameraManager->AddNewCameraModifier(UMainMenuExposureCameraModifier::StaticClass());
	UMainMenuExposureCameraModifier* ExposureModifier = Cast<UMainMenuExposureCameraModifier>(NewModifier);
	if (!ExposureModifier)
	{
		return;
	}

	ExposureModifier->LockedExposureBias = LockedExposureBias;
	ExposureModifier->LockedExposureBrightness = LockedExposureBrightness;
	ExposureModifier->EnableModifier();
}

void AMainMenuPlayerController::ShowMainMenuWidget()
{
	if (!MainMenuWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("AMainMenuPlayerController: MainMenuWidgetClass is not set. Assign WBP_MainMenu on the controller Blueprint."));
		return;
	}

	if (MainMenuWidgetClass->HasAnyClassFlags(CLASS_Abstract))
	{
		UE_LOG(LogTemp, Error, TEXT("AMainMenuPlayerController: MainMenuWidgetClass is abstract. Assign WBP_MainMenu."));
		return;
	}

	MainMenuWidgetInstance = CreateWidget<UMainMenuWidget>(this, MainMenuWidgetClass);
	if (!MainMenuWidgetInstance)
	{
		return;
	}

	MainMenuWidgetInstance->AddToViewport(0);

	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(MainMenuWidgetInstance->TakeWidget());
	SetInputMode(InputMode);
	bShowMouseCursor = true;
}

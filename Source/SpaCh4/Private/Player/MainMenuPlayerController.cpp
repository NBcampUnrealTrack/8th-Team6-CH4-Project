#include "Player/MainMenuPlayerController.h"

#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Player/MainMenuExposureCameraModifier.h"
#include "Systems/MainMenuGameMode.h"
#include "Systems/SPEOSSessionSubsystem.h"
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
	SubmitPendingMatchmakingRole();
}

void AMainMenuPlayerController::SubmitPendingMatchmakingRole()
{
	UGameInstance* GameInstance = GetGameInstance();
	USPEOSSessionSubsystem* SessionSubsystem = GameInstance ? GameInstance->GetSubsystem<USPEOSSessionSubsystem>() : nullptr;
	if (!SessionSubsystem || !SessionSubsystem->IsMatchmakingActive())
	{
		return;
	}

	const ELobbyPlayerRole PendingRole = SessionSubsystem->GetPendingMatchmakingRole();
	if (PendingRole == ELobbyPlayerRole::None)
	{
		return;
	}

	SubmitMatchmakingRole(PendingRole, SessionSubsystem->GetCachedNickname());
}

void AMainMenuPlayerController::SubmitMatchmakingRole(ELobbyPlayerRole SelectedRole, const FString& Nickname)
{
	if (SelectedRole == ELobbyPlayerRole::None)
	{
		ClientReceiveMainMenuMatchmakingStatus(false, TEXT("역할이 선택되지 않았습니다."));
		return;
	}

	ServerSubmitMainMenuMatchmakingRole(SelectedRole, Nickname);
}

void AMainMenuPlayerController::CancelMainMenuMatchmaking()
{
	UGameInstance* GameInstance = GetGameInstance();
	USPEOSSessionSubsystem* SessionSubsystem = GameInstance ? GameInstance->GetSubsystem<USPEOSSessionSubsystem>() : nullptr;
	if (SessionSubsystem)
	{
		SessionSubsystem->CancelMatchmaking();
	}
}

void AMainMenuPlayerController::ServerSubmitMainMenuMatchmakingRole_Implementation(ELobbyPlayerRole SelectedRole, const FString& Nickname)
{
	AMainMenuGameMode* MainMenuGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AMainMenuGameMode>() : nullptr;
	if (!MainMenuGameMode)
	{
		ClientReceiveMainMenuMatchmakingStatus(false, TEXT("메인 메뉴 매치메이킹 서버가 없습니다."));
		return;
	}

	FString StatusMessage;
	const bool bWasSubmitted = MainMenuGameMode->SubmitMatchmakingRole(this, Nickname, SelectedRole, StatusMessage);
	ClientReceiveMainMenuMatchmakingStatus(bWasSubmitted, StatusMessage);
}

void AMainMenuPlayerController::ClientReceiveMainMenuMatchmakingStatus_Implementation(bool bWasSuccessful, const FString& StatusMessage)
{
	if (MainMenuWidgetInstance)
	{
		MainMenuWidgetInstance->UpdateMatchmakingStatus(StatusMessage, !bWasSuccessful);
	}
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

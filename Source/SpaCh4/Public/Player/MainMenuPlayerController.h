#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Player/SPPlayerLoadout.h"
#include "Systems/LobbyGameState.h"
#include "MainMenuPlayerController.generated.h"

UCLASS()
class SPACH4_API AMainMenuPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AMainMenuPlayerController();

	UFUNCTION(BlueprintCallable, Category = "MainMenu|Matchmaking")
	void SubmitPendingMatchmakingRole();

	UFUNCTION(BlueprintCallable, Category = "MainMenu|Matchmaking")
	void SubmitMatchmakingRole(ELobbyPlayerRole SelectedRole, const FString& Nickname);

	UFUNCTION(BlueprintCallable, Category = "MainMenu|Matchmaking")
	void CancelMainMenuMatchmaking();

	UFUNCTION(BlueprintCallable, Category = "MainMenu|Loadout")
	void SavePlayerLoadout(const FSPPlayerLoadout& NewLoadout);

	UFUNCTION(Server, Reliable)
	void ServerSubmitMainMenuMatchmakingRole(ELobbyPlayerRole SelectedRole, const FString& Nickname);

	UFUNCTION(Server, Reliable)
	void ServerSavePlayerLoadout(const FSPPlayerLoadout& NewLoadout);

	UFUNCTION(Client, Reliable)
	void ClientReceiveMainMenuMatchmakingStatus(bool bWasSuccessful, const FString& StatusMessage);

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
	void SubmitCachedPlayerLoadout();
};

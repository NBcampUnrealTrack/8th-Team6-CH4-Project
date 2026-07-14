#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Systems/MatchGameState.h"
#include "SPPlayerController.generated.h"

class USPInputConfigData;
class UInputMappingContext;
class UEnhancedInputLocalPlayerSubsystem;

UCLASS()
class SPACH4_API ASPPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Online|Session")
	void ReturnToMainMenu();

	UFUNCTION(BlueprintPure, Category = "Online|Session")
	bool IsReturnToMainMenuPending() const;

	UFUNCTION(BlueprintCallable, Category = "Input")
	void AddInputMappingContext(UInputMappingContext* MappingContext, int32 Priority = 0);

	UFUNCTION(BlueprintCallable, Category = "Input")
	void RemoveInputMappingContext(UInputMappingContext* MappingContext);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GameResult|UI")
	TSubclassOf<class UGameResultWidget> GameResultWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<class UGameResultWidget> GameResultWidgetInstance;

private:
	UEnhancedInputLocalPlayerSubsystem* GetInputSubsystem() const;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<USPInputConfigData> InputConfig;

	// 추후 게임 결과창에서 로비로 돌아갈 때 사용
	UPROPERTY(EditDefaultsOnly, Category = "Online|Session")
	FString MainMenuLevelPath = TEXT("/Game/Maps/L_MainMenu");

	UFUNCTION()
	void OnMatchEnded(EMatchResult Result);

	void CompleteReturnToMainMenu();
	void TryCompletePendingHostReturn();
	bool HasConnectedRemotePlayers() const;

	bool bIsHostReturnToMainMenuPending = false;
	FTimerHandle HostReturnToMainMenuTimerHandle;

	static constexpr float HostReturnCheckInterval = 0.5f;
};

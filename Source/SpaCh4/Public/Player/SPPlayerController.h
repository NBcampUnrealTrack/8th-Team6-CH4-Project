#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
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

	UFUNCTION(BlueprintCallable, Category = "Input")
	void AddInputMappingContext(UInputMappingContext* MappingContext, int32 Priority = 0);

	UFUNCTION(BlueprintCallable, Category = "Input")
	void RemoveInputMappingContext(UInputMappingContext* MappingContext);

protected:
	virtual void BeginPlay() override;

private:
	UEnhancedInputLocalPlayerSubsystem* GetInputSubsystem() const;
	
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<USPInputConfigData> InputConfig;

	// 추후 게임 결과창에서 로비로 돌아갈 때 사용
	UPROPERTY(EditDefaultsOnly, Category = "Online|Session")
	FString MainMenuLevelPath = TEXT("/Game/Maps/L_MainMenu");
};

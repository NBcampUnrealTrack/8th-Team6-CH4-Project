#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SPPlayerController.generated.h"

class UInputConfigData;
class UInputMappingContext;
class UEnhancedInputLocalPlayerSubsystem;

UCLASS()
class SPACH4_API ASPPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Input")
	void AddInputMappingContext(UInputMappingContext* MappingContext, int32 Priority = 0);

	UFUNCTION(BlueprintCallable, Category = "Input")
	void RemoveInputMappingContext(UInputMappingContext* MappingContext);

protected:
	virtual void BeginPlay() override;

private:
	UEnhancedInputLocalPlayerSubsystem* GetInputSubsystem() const;
	
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputConfigData> InputConfig;
};

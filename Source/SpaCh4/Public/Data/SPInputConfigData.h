#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SPInputConfigData.generated.h"

class UInputMappingContext;
class UInputAction;

USTRUCT(BlueprintType)
struct FInputMappingContextEntry
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> MappingContext = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	int32 Priority = 0;
};

UCLASS(BlueprintType)
class SPACH4_API USPInputConfigData : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Context")
	TArray<FInputMappingContextEntry> MappingContexts;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Action")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Action")
	TObjectPtr<UInputAction> LookAction;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Action")
	TObjectPtr<UInputAction> InteractAction;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Action")
	TObjectPtr<UInputAction> JumpOverAction;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Action")
    TObjectPtr<UInputAction> AttackAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Action")
	TObjectPtr<UInputAction> CrouchAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Action")
	TObjectPtr<UInputAction> RunAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Action")
	TArray<TObjectPtr<UInputAction>> SelectSlotActions;
};

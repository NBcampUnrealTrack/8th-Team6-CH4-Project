// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Interface.h"
#include "SPInteractable.generated.h"

// This class does not need to be modified.
UINTERFACE()
class USPInteractable : public UInterface
{
	GENERATED_BODY()
};

class SPACH4_API ISPInteractable
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, Category = "Interaction")
	void Interact(AActor* Instigator);

	UFUNCTION(BlueprintNativeEvent, Category = "Interaction")
	void SetHighlight(bool bEnabled);
	
	UFUNCTION(BlueprintNativeEvent, Category = "Interaction")
	FGameplayTag GetInteractableTag() const;
};

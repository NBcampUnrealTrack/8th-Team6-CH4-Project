// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Interface.h"
#include "SPInteractable.generated.h"

class USceneComponent;

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

	// 현재 상호작용 가능한 상태인지 여부. 기본값 true.
	// 완료된 납품소, 아직 개방 불가하거나 이미 열린 탈출구 등은 false를 반환해
	// 조준 하이라이트와 상호작용 대상에서 제외된다.
	UFUNCTION(BlueprintNativeEvent, Category = "Interaction")
	bool IsInteractable() const;
	virtual bool IsInteractable_Implementation() const { return true; }

	UFUNCTION(BlueprintNativeEvent, Category = "Interaction")
	USceneComponent* GetInteractFocusComponent() const;
	virtual USceneComponent* GetInteractFocusComponent_Implementation() const { return nullptr; }
};

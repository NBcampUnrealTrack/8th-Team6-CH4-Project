// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Player/LDPlayerState.h"
#include "Systems/MatchGameState.h"
#include "GameResultWidget.generated.h"

class UTextBlock;
class UButton;
/**
 * 
 */
UCLASS()
class SPACH4_API UGameResultWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UFUNCTION()
	void SetResultText(EMatchResult MatchResult);
	
	UFUNCTION()
	void SetInformationText(FString InfoText);

	UFUNCTION(BlueprintCallable, Category = "GameResult|Stats")
	void SetSurvivorStats(const FSurvivorMatchStats& SurvivorStats);

	UFUNCTION(BlueprintCallable, Category = "GameResult|Stats")
	void SetKillerStats(const FKillerMatchStats& KillerStats);
	
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ResultTitleText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ScoreText;
	
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> InformationText;
	
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> ReturnToMainMenuButton;
	
	
	
};

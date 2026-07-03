// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SurvivorData.generated.h"

/**
 * 
 */
UCLASS()
class SPACH4_API USurvivorData : public UDataAsset
{
	GENERATED_BODY()
public:
	USurvivorData();
	
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
	float SurvivorSprintSpeed = 650;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
	float SurvivorCrouchSpeed = 220;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
	float SurvivorInjuredSpeedMultiplier = 0.92;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
	float CarrySlowSmall = 0.95;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
	float CarrySlowMedium = 0.90;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
	float CarrySlowLarge = 0.85;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
	float CarrySlowHazardous = 0.90;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	float PickupRange = 150;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	float PickupDuration = 0.80;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	float DropDuration = 0.50;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	float DeliveryDuration = 2.50;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	float DeliveryMovePenalty = 0.70;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	float RescueRange = 200;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	float RescueDurationStage1 = 4.00;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	float RescueDurationStage2 = 4.00;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	float CageRescueDuration = 4.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Escape")
	float EscapeGateOpenDuration = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Escape")
	float HatchEscapeDuration = 3.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noise")
	float SprintNoiseRadius = 1500;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noise")
	float CrouchNoiseRadius = 800;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Noise")
	float ChannelNoiseRadius = 1000;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Items")
	float MedkitDuration = 3.00;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Items")
	float MedkitMovePenalty = 0.60;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Items")
	float SpeedBoostMultiplier = 1.20;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Items")
	float SpeedBoostDuration = 4.00;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Items")
	float SpeedFatigueMultiplier = 0.90;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Items")
	float SpeedFatigueDuration = 3.00;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Perks")
	float FieldMedicTimeReduction = 1.00;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Perks")
	float DeliveryProPenaltyReduction = 0.50;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Perks")
	float SilentRollNoiseReduction = 0.30;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Perks")
	float StageTwoRescueReduction = 1.00;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Perks")
	float ThreatSensorRadius = 1500;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Perks")
	float ThreatSensorCooldown = 5.00;
};

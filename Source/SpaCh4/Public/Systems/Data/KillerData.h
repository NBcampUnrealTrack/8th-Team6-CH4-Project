// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "KillerData.generated.h"

class UAnimMontage;

UENUM(BlueprintType)
enum class EKillerPerkType : uint8
{
	None UMETA(DisplayName = "미선택"),
	Overvoltage UMETA(DisplayName = "과전압"),
	Conduction_Coil UMETA(DisplayName = "전도 코일"),
	Circuit_Trace UMETA(DisplayName = "회로 추적"),
	Blackout_Zone UMETA(DisplayName = "전력 차단")
};

/**
 * 
 */
UCLASS(BlueprintType)
class SPACH4_API UKillerData : public UDataAsset
{
	GENERATED_BODY()
public:
	UKillerData();

	/** Full-body montage used while hoisting a downed survivor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual|Carry")
	TSoftObjectPtr<UAnimMontage> PickupMontage;

	/** Upper-body looping montage used while carrying. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual|Carry")
	TSoftObjectPtr<UAnimMontage> CarryingMontage;

	/** Upper-body roots blended over the locomotion pose. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual|Carry")
	TArray<FName> CarryBlendRootBones;

	/** Normalized pickup montage time at which the survivor is attached. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual|Carry", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PickupAttachNormalizedTime = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ToolTip="기본값 575(생존자650 * 1.15)"));
	float KillerBaseSpeed = 575.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ToolTip="기본값 748(생존자650 * 1.15)"));
	float KillerSprintSpeed = 748.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ToolTip="기본값 403(생존자350 * 1.15)"));
	float KillerWalkSpeed = 403.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
	float KillerCarrySpeedMultiplier = 0.9f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
	float KillerGroggySpeedMultiplier = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	float TaserRange = 250.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	float TaserHitboxRadius = 40.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	float TaserWindup = 0.35f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	float TaserActive = 0.40f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	float TaserRecoveryHit = 0.30f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	float TaserCooldown = 1.20f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	float TaserHitstun = 0.30f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	float TaserGroggyOnMiss = 1.00f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ChainShock")
	float ChainShockCooldown = 24.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ChainShock")
	float ChainShockBuffDuration = 6.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ChainShock")
	float ChainShockRadius = 350.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ChainShock")
	float ChainShockHitstun = 0.50f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	float PickupRange = 150.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	float PickupDuration = 1.25f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	//float CageDepositRange = 200.f;
	float CageDepositRange = 400.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	float CageDepositDuration = 1.00f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Detection")
	float DetectionRadius = 1200.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Perks")
	float OvervoltageWindupReduction = 0.10f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Perks")
	float ConductionCoilRadiusBonus = 100.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Perks")
	float CircuitTraceDuration = 6.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Perks")
	float BlakoutZoneRadius = 800.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Perks")
	float BlackoutZoneDepositPenalty = 0.80f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traps")
	float TrapRetriggerCooldown = 3.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traps")
	float NoiseFloorRevealRadius = 1500.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traps")
	float NoiseFloorRevealDuration = 3.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traps")
	float NoiseFloorGroggyBonus = 1.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traps")
	float ShockPlateStunSurvivor = 0.50f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traps")
	float ShockPlateInjuredBonus = 0.30f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traps")
	float ShockPlateGroggyKiller = 0.50f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traps")
	float SteamVentDuration = 2.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traps")
	float SteamVentSlowMultiplier = 0.90f;
};

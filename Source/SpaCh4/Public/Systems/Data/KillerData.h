#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "KillerData.generated.h"

UENUM(BlueprintType)
enum class EKillerPerkType : uint8
{
	None UMETA(DisplayName = "미선택"),
	Overvoltage UMETA(DisplayName = "과전압"),
	Conduction_Coil UMETA(DisplayName = "전도 코일"),
	Circuit_Trace UMETA(DisplayName = "회로 추적"),
	Blackout_Zone UMETA(DisplayName = "전력 차단")
};

class UAnimInstance;
class UAnimMontage;
class UAnimSequence;
class USkeletalMesh;

UCLASS(BlueprintType)
class SPACH4_API UKillerData : public UDataAsset
{
	GENERATED_BODY()

public:
	UKillerData();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	TSoftObjectPtr<USkeletalMesh> BodyMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	TSoftClassPtr<UAnimInstance> AnimInstanceClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	TSoftObjectPtr<UAnimSequence> ArmAttackSequence;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	TSoftObjectPtr<UAnimMontage> AttackGroggyMontage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	TArray<FName> ArmAttackBlendRootBones;

	/** Full-body montage: bend down and hoist a downed survivor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual|Carry")
	TSoftObjectPtr<UAnimMontage> PickupMontage;

	/** Upper-body loop while carrying; legs stay on motion matching. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual|Carry")
	TSoftObjectPtr<UAnimMontage> CarryingMontage;

	/** Bones weighted for carrying montage (spine/arms). Legs remain on MM. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual|Carry")
	TArray<FName> CarryBlendRootBones;

	/** Mesh bone/socket the carried survivor attaches to (spine for stable shoulder carry). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual|Carry")
	FName CarryAttachSocketName = TEXT("spine_03");

	/** Relative location of carried survivor root vs CarryAttachSocketName. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual|Carry")
	FVector CarryAttachRelativeLocation = FVector(-20.f, 45.f, 10.f);

	/** Relative rotation of carried survivor vs CarryAttachSocketName (fireman / shoulder drape). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual|Carry")
	FRotator CarryAttachRelativeRotation = FRotator(-85.f, 0.f, 90.f);

	/** Normalized time [0,1] in PickupMontage when the survivor snaps to the hand. */
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

	/** Minimum input magnitude (control-yaw space) before locomotion velocity is aligned for MM. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LocomotionStrafeInputDeadzone = 0.15f;

	/** Align horizontal velocity to movement input so MM picks F/FL/FR vs B/BL/BR correctly. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
	bool bAlignLocomotionVelocityToInput = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0.0"))
	float LocomotionVelocityAlignMinSpeed = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	float TaserRange = 250.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	float TaserHitboxRadius = 40.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	float TaserHitboxHalfHeight = 90.f;
	
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
	float CageDepositRange = 200.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	float CageDepositDuration = 1.00f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	float CarryAttachForwardOffset = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	float DropForwardOffset = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	float PickupCooldown = 2.0f;

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

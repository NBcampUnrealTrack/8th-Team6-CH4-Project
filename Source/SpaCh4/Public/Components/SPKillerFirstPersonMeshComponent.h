#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SPKillerFirstPersonMeshComponent.generated.h"

class UAnimInstance;
class UMeshComponent;
class USkeletalMesh;
class UKillerData;

UCLASS(ClassGroup = (SP), meta = (BlueprintSpawnableComponent))
class SPACH4_API USPKillerFirstPersonMeshComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USPKillerFirstPersonMeshComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	void ScheduleSetup();
	void RefreshFirstPersonView();
	void EnsureAnimationSetup();

protected:
	ACharacter* GetOwnerCharacter() const;
	const UKillerData* ResolveKillerData() const;
	USkeletalMesh* ResolveBodyMeshAsset() const;
	TSubclassOf<UAnimInstance> ResolveAnimInstanceClass() const;

	/** Optional override when the owner does not expose KillerData. */
	UPROPERTY(EditDefaultsOnly, Category = "SP|Killer|FirstPerson|Data")
	TObjectPtr<UKillerData> KillerDataOverride;

	UPROPERTY(EditDefaultsOnly, Category = "SP|Killer|FirstPerson|Data")
	TSoftObjectPtr<USkeletalMesh> FallbackBodyMesh;

	UPROPERTY(EditDefaultsOnly, Category = "SP|Killer|FirstPerson|Data")
	TSoftClassPtr<UAnimInstance> FallbackAnimInstanceClass;

	UPROPERTY(EditDefaultsOnly, Category = "SP|Killer|FirstPerson")
	FName CameraAttachBoneName = TEXT("head");

	UPROPERTY(EditDefaultsOnly, Category = "SP|Killer|FirstPerson")
	TArray<FName> CameraAttachBoneFallbacks = {
		TEXT("head"), TEXT("Head"), TEXT("neck_01"), TEXT("Neck"),
	};

	UPROPERTY(EditDefaultsOnly, Category = "SP|Killer|FirstPerson")
	FVector CameraRelativeOffset = FVector(10.f, 0.f, 0.f);

	UPROPERTY(EditDefaultsOnly, Category = "SP|Killer|FirstPerson|Camera")
	float SpringArmLength = 0.f;

	UPROPERTY(EditDefaultsOnly, Category = "SP|Killer|FirstPerson|Camera")
	bool bSpringArmCollisionTest = false;

	UPROPERTY(EditDefaultsOnly, Category = "SP|Killer|FirstPerson|Camera")
	bool bSpringArmUsePawnControlRotation = true;

	UPROPERTY(EditDefaultsOnly, Category = "SP|Killer|FirstPerson|Camera")
	bool bSpringArmEnableCameraLag = false;

	UPROPERTY(EditDefaultsOnly, Category = "SP|Killer|FirstPerson|Camera")
	bool bSpringArmInheritRoll = false;

	UPROPERTY(EditDefaultsOnly, Category = "SP|Killer|FirstPerson|Camera")
	bool bUsePerspectiveProjection = true;

	UPROPERTY(EditDefaultsOnly, Category = "SP|Killer|FirstPerson")
	bool bHideOwnerShadow = true;

	UPROPERTY(EditDefaultsOnly, Category = "SP|Killer|FirstPerson")
	TArray<TObjectPtr<UMeshComponent>> OwnerHiddenMeshComponents;

	UPROPERTY(EditDefaultsOnly, Category = "SP|Killer|FirstPerson|Pitch")
	bool bEnableUpperBodyPitchFollow = true;

	UPROPERTY(EditDefaultsOnly, Category = "SP|Killer|FirstPerson|Pitch", meta = (ClampMin = "0", ClampMax = "1"))
	float UpperBodyPitchFollowStrength = 0.75f;

	UPROPERTY(EditDefaultsOnly, Category = "SP|Killer|FirstPerson|Pitch")
	float UpperBodyPitchMin = -75.f;

	UPROPERTY(EditDefaultsOnly, Category = "SP|Killer|FirstPerson|Pitch")
	float UpperBodyPitchMax = 60.f;

	UPROPERTY(EditDefaultsOnly, Category = "SP|Killer|FirstPerson|Pitch")
	float UpperBodyPitchInterpSpeed = 18.f;

	UPROPERTY(EditDefaultsOnly, Category = "SP|Killer|FirstPerson")
	float SetupRetryInterval = 0.05f;

private:
	UFUNCTION()
	void HandleOwnerControllerChanged(APawn* Pawn, AController* OldController, AController* NewController);

	void ApplySpringArmDefaults();
	void ApplyCameraDefaults();
	void TrySetupFirstPersonView();
	void UpdateUpperBodyPitchFollow();

	float SmoothedUpperBodyPitch = 0.f;

	FTimerHandle FirstPersonSetupTimer;
};

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Animation/AnimEnums.h"
#include "SPParkourComponent.generated.h"

class UAnimMontage;
class ASurvivorCharacter;

UCLASS(ClassGroup = (SP), meta = (BlueprintSpawnableComponent))
class SPACH4_API USPParkourComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USPParkourComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	bool IsParkouring() const { return bIsParkour; }
	bool CanJumpOver() const;

	void RequestJumpOver();
	void EnsureMontagesLoaded();

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_JumpOver();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayParkourMontage(
		UAnimMontage* Montage,
		AActor* ObstacleActor,
		const FRotator& FacingRotation,
		float ObstacleHeight,
		FVector_NetQuantize ObstacleImpactPoint);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_SnapParkourLanding(FVector_NetQuantize WorldLocation, FRotator WorldRotation);

	UFUNCTION()
	void OnParkourMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	ASurvivorCharacter* GetSurvivor() const;

	bool TraceParkourObstacle(FHitResult& OutObstacleHit, float& OutObstacleHeight, FString* OutFailReason = nullptr) const;
	void GetParkourFacing(FVector& OutForward, FVector& OutRight) const;
	void PlayParkourMontage(
		UAnimMontage* Montage,
		AActor* ObstacleActor,
		const FRotator& FacingRotation,
		float ObstacleHeight,
		const FVector& ObstacleImpactPoint);
	void EndParkour(bool bInterrupted);
	void UpdateParkourRootMotion();
	void InitParkourVaultTargets(AActor* ObstacleActor, const FVector& ObstacleImpactPoint);
	FVector BuildParkourVaultLocation(float MontageAlpha) const;
	FVector BuildParkourLocationFromRootMotion(const FVector& RootMotionTranslation, float MontageTime, float MontageLength) const;
	bool ExtractMontageRootMotionAtTime(const UAnimMontage* Montage, float MontageTime, FVector& OutTranslation, FQuat& OutRotation) const;
	bool ComputeParkourLandingFromRootMotion(FVector& OutLocation, FRotator& OutRotation) const;
	bool ResolveParkourLanding(FVector& OutLocation, FRotator& OutRotation) const;
	void SnapParkourLocationToGround(FVector& InOutLocation, bool bIgnoreParkourObstacle = true) const;
	void ApplyParkourLanding(const FVector& WorldLocation, const FRotator& WorldRotation);
	void SetParkourObstacleCollisionIgnored(bool bIgnore);
	void OnParkourEndTimer();
	void LogParkourDebug(const FString& Message, FColor Color = FColor::Yellow) const;

	UPROPERTY(EditDefaultsOnly, Category = "SP|Parkour")
	TObjectPtr<UAnimMontage> ParkourMontage;

	UPROPERTY(EditDefaultsOnly, Category = "SP|Parkour")
	float ParkourTraceDistance{150.f};

	UPROPERTY(EditDefaultsOnly, Category = "SP|Parkour")
	float ParkourMinStartDistance{15.f};

	UPROPERTY(EditDefaultsOnly, Category = "SP|Parkour")
	float ParkourMaxStartDistance{70.f};

	UPROPERTY(EditDefaultsOnly, Category = "SP|Parkour")
	float ParkourTraceRadius{25.f};

	UPROPERTY(EditDefaultsOnly, Category = "SP|Parkour")
	float MinObstacleHeight{50.f};

	UPROPERTY(EditDefaultsOnly, Category = "SP|Parkour")
	float MaxObstacleHeight{130.f};

	UPROPERTY(EditDefaultsOnly, Category = "SP|Parkour")
	float ParkourReferenceObstacleHeight{90.f};

	UPROPERTY(EditDefaultsOnly, Category = "SP|Parkour")
	float ParkourClearanceOverObstacle{15.f};

	UPROPERTY(EditDefaultsOnly, Category = "SP|Parkour")
	float ParkourLandingForwardOffset{55.f};

	UPROPERTY(EditDefaultsOnly, Category = "SP|Parkour", meta = (ClampMin = "0.5", ClampMax = "0.9"))
	float ParkourLandByMontageAlpha{0.67f};

	UPROPERTY(EditDefaultsOnly, Category = "SP|Parkour")
	bool bDrawDebug{false};

	UPROPERTY(Replicated)
	bool bIsParkour{false};

	TWeakObjectPtr<AActor> CurrentParkourObstacle;
	FOnMontageEnded ParkourMontageEndedDelegate;
	TObjectPtr<UAnimMontage> ActiveParkourMontage;
	FTransform ParkourStartTransform = FTransform::Identity;
	float ParkourObstacleHeight = 0.f;
	float ParkourStartFeetZ = 0.f;
	FVector ParkourVaultEndLocation = FVector::ZeroVector;
	float ParkourVaultPeakCenterZ = 0.f;
	float ParkourWallClearForward = 0.f;
	float ParkourWallClearMontageAlpha = 0.f;
	ERootMotionMode::Type CachedRootMotionMode = ERootMotionMode::NoRootMotionExtraction;
	TEnumAsByte<EMovementMode> CachedMovementMode = MOVE_Walking;
	float CachedGravityScale = 1.f;
	FTimerHandle ParkourEndTimerHandle;
};

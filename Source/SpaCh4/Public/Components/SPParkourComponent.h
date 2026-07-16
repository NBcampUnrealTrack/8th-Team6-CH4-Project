#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Animation/AnimEnums.h"
#include "SPParkourComponent.generated.h"

class ACharacter;
class UCapsuleComponent;
class UAnimMontage;
class ASPParkourZone;
struct FHitResult;
struct FCollisionQueryParams;

struct FParkourWallVaultRecord
{
	TWeakObjectPtr<AActor> Obstacle;
	TArray<double> VaultTimestamps;
	double BlockedUntilTime = 0.0;
};

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
	void OnParkourLandNotify();
	void RegisterParkourZoneOverlap(ASPParkourZone* Zone, bool bIsOverlapping);

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

	ACharacter* GetParkourCharacter() const;
	bool CanOwnerPerformParkour() const;
	float GetParkourFeetZ(const ACharacter* Character, const UCapsuleComponent* Capsule) const;

	bool TraceParkourObstacle(FHitResult& OutObstacleHit, float& OutObstacleHeight, FString* OutFailReason = nullptr) const;
	void GetParkourFacing(FVector& OutForward, FVector& OutRight) const;
	void PlayParkourMontage(
		UAnimMontage* Montage,
		AActor* ObstacleActor,
		const FRotator& FacingRotation,
		float ObstacleHeight,
		const FVector& ObstacleImpactPoint);
	void EndParkour(bool bInterrupted);
	void UpdateParkourRootMotion(float DeltaTime);
	void InitParkourVaultTargets(AActor* ObstacleActor, const FVector& ObstacleImpactPoint);
	void CacheParkourLandNotifyTiming(UAnimMontage* Montage);
	bool ResolveParkourVaultTargets(
		const FTransform& StartTransform,
		float StartFeetZ,
		float ObstacleHeight,
		AActor* ObstacleActor,
		const FVector& ObstacleImpactPoint,
		FVector& OutEndLocation,
		float& OutPeakCenterZ,
		float& OutWallClearForward,
		float& OutWallClearMontageAlpha,
		FString* OutFailReason = nullptr) const;
	bool SweepParkourCapsulePath(
		const FVector& Start,
		const FVector& End,
		const FVector& Forward,
		FHitResult& OutHit,
		AActor* IgnoredObstacle = nullptr,
		float SweepCenterZOverride = -1.f) const;
	bool ValidateParkourLandingLocation(
		const FTransform& StartTransform,
		float StartFeetZ,
		const FVector& ProposedLocation,
		FString* OutFailReason = nullptr) const;
	bool PreviewParkourVault(
		const FRotator& FacingRotation,
		float ObstacleHeight,
		AActor* ObstacleActor,
		const FVector& ObstacleImpactPoint,
		FString* OutFailReason = nullptr) const;
	bool SampleParkourGroundCenterZ(
		FVector& InOutCenterLocation,
		bool bIgnoreParkourObstacle = true,
		AActor* IgnoredObstacleOverride = nullptr,
		float PeakCenterZOverride = -1.f) const;
	FVector BuildParkourVaultLocation(float MontageAlpha) const;
	FVector BuildParkourLocationFromRootMotion(const FVector& RootMotionTranslation, float MontageTime, float MontageLength) const;
	bool ExtractMontageRootMotionAtTime(const UAnimMontage* Montage, float MontageTime, FVector& OutTranslation, FQuat& OutRotation) const;
	bool ComputeParkourLandingFromRootMotion(FVector& OutLocation, FRotator& OutRotation) const;
	bool ResolveParkourLanding(FVector& OutLocation, FRotator& OutRotation) const;
	bool SnapParkourLocationToGround(FVector& InOutLocation, bool bIgnoreParkourObstacle = true) const;
	void ApplyParkourLanding(const FVector& WorldLocation, const FRotator& WorldRotation);
	void SetParkourObstacleCollisionIgnored(bool bIgnore);
	void SetParkourCollisionSuppressed(bool bSuppressed);
	void OnParkourEndTimer();
	float GetParkourArcFactor(float MontageAlpha) const;
	bool HasSufficientVaultForwardProgress(const FVector& Location) const;
	float GetParkourMontageAlpha() const;
	bool IsInsideParkourZone() const;
	bool IsObstacleVaultBlocked(AActor* ObstacleActor, FString* OutFailReason = nullptr) const;
	bool TryRegisterWallVault(AActor* ObstacleActor, FString* OutFailReason = nullptr);
	FParkourWallVaultRecord* FindWallVaultRecord(AActor* ObstacleActor);
	const FParkourWallVaultRecord* FindWallVaultRecord(AActor* ObstacleActor) const;
	void PruneWallVaultRecord(FParkourWallVaultRecord& Record, double Now) const;
	void LogParkourDebug(const FString& Message, FColor Color = FColor::Yellow) const;

	UPROPERTY(EditDefaultsOnly, Category = "SP|Parkour")
	TObjectPtr<UAnimMontage> ParkourMontage;

	UPROPERTY(EditDefaultsOnly, Category = "SP|Parkour")
	float ParkourTraceDistance{200.f};

	UPROPERTY(EditDefaultsOnly, Category = "SP|Parkour")
	float ParkourMinStartDistance{5.f};

	UPROPERTY(EditDefaultsOnly, Category = "SP|Parkour")
	float ParkourMaxStartDistance{150.f};

	UPROPERTY(EditDefaultsOnly, Category = "SP|Parkour")
	float ParkourTraceRadius{45.f};

	UPROPERTY(EditDefaultsOnly, Category = "SP|Parkour")
	float MinObstacleHeight{20.f};

	UPROPERTY(EditDefaultsOnly, Category = "SP|Parkour")
	float MaxObstacleHeight{350.f};

	UPROPERTY(EditDefaultsOnly, Category = "SP|Parkour")
	float ParkourReferenceObstacleHeight{90.f};

	UPROPERTY(EditDefaultsOnly, Category = "SP|Parkour")
	float ParkourClearanceOverObstacle{55.f};

	UPROPERTY(EditDefaultsOnly, Category = "SP|Parkour")
	float ParkourLandingForwardOffset{75.f};

	/** Minimum center travel required to clear the obstacle's far side (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "SP|Parkour", meta = (ClampMin = "0"))
	float ParkourMinLandingPastWallOffset{12.f};

	/** Max actor-center travel from parkour start along facing (cm). Prevents overshooting past thin walls. */
	UPROPERTY(EditDefaultsOnly, Category = "SP|Parkour", meta = (ClampMin = "80"))
	float ParkourMaxVaultTravel{280.f};

	/** Max vertical drop from start feet to landing feet (cm). Rejects void/out-of-bounds snaps. */
	UPROPERTY(EditDefaultsOnly, Category = "SP|Parkour", meta = (ClampMin = "0"))
	float ParkourMaxLandingDrop{350.f};

	/** Montage alpha when the vault reaches maximum height (start of descent). */
	UPROPERTY(EditDefaultsOnly, Category = "SP|Parkour", meta = (ClampMin = "0.2", ClampMax = "0.65"))
	float ParkourVaultPeakMontageAlpha{0.35f};

	/** Montage alpha when forward travel to the landing point should be complete. */
	UPROPERTY(EditDefaultsOnly, Category = "SP|Parkour", meta = (ClampMin = "0.75", ClampMax = "0.99"))
	float ParkourLandByMontageAlpha{0.96f};

	UPROPERTY(EditDefaultsOnly, Category = "SP|Parkour")
	bool bDrawDebug{false};

	/** Parkour input/trace is only allowed while overlapping a parkour zone volume. */
	UPROPERTY(EditDefaultsOnly, Category = "SP|Parkour|Zone")
	bool bRequireParkourZone{true};

	/** Skip per-wall vault count/cooldown (e.g. killer has no wall limit). */
	UPROPERTY(EditDefaultsOnly, Category = "SP|Parkour|WallLimit")
	bool bExemptFromWallVaultLimit{false};

	/** Max successful vaults on the same wall within the tracking window before cooldown. */
	UPROPERTY(EditDefaultsOnly, Category = "SP|Parkour|WallLimit", meta = (ClampMin = "1"))
	int32 MaxVaultsPerWallInWindow{3};

	/** Sliding window for counting repeated vaults on the same wall (seconds). */
	UPROPERTY(EditDefaultsOnly, Category = "SP|Parkour|WallLimit", meta = (ClampMin = "1"))
	float WallVaultWindowSeconds{180.f};

	/** Cooldown applied after exceeding the vault limit on one wall (seconds). */
	UPROPERTY(EditDefaultsOnly, Category = "SP|Parkour|WallLimit", meta = (ClampMin = "1"))
	float WallVaultCooldownSeconds{180.f};

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
	bool bParkourVaultEndValid = false;
	bool bParkourFeetOnGround = false;
	float ParkourLandNotifyMontageAlpha = -1.f;
	float ParkourMontageElapsed = 0.f;
	float ParkourMontageDuration = 0.f;
	TArray<TWeakObjectPtr<ASPParkourZone>> OverlappingParkourZones;
	TArray<FParkourWallVaultRecord> WallVaultRecords;
	ERootMotionMode::Type CachedRootMotionMode = ERootMotionMode::NoRootMotionExtraction;
	TEnumAsByte<EMovementMode> CachedMovementMode = MOVE_Walking;
	float CachedGravityScale = 1.f;
	uint8 CachedCapsuleCollision = 3;
	uint8 CachedMeshCollision = 0;
	bool bParkourCollisionSuppressed = false;
	FTimerHandle ParkourEndTimerHandle;
};

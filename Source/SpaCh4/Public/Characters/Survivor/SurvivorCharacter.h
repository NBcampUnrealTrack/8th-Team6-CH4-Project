#pragma once

#include "CoreMinimal.h"
#include "Characters/Base/CharacterBase.h"
#include "GameplayTagAssetInterface.h"
#include "GameplayTagContainer.h"
#include "Inventory/SPInventoryTypes.h"
#include "Animation/AnimEnums.h"
#include "SurvivorCharacter.generated.h"

class UAnimMontage;
class USurvivorData;
class USPInteractionComponent;
class USPMovementComponent;
class ASPCollectibleItem;
class ASPDeliveryStation;
class ASPEscapeGate;
class ASPHatch;
class USPInventoryComponent;

UENUM(BlueprintType)
enum class ESurvivorState : uint8
{
	Healthy,
	Injured,
	Downed,
	Carried,
	Caged,
	Dead,
	Escaped
};

UCLASS()
class SPACH4_API ASurvivorCharacter : public ACharacterBase, public IGameplayTagAssetInterface
{
	GENERATED_BODY()

public:
	ASurvivorCharacter();
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;

	void SetSurvivorState(ESurvivorState NewState);
	ESurvivorState GetSurvivorState() const { return SurvivorState; };

	bool CanMove() const;
	bool CanInteract() const;
	bool CanJumpOver() const;
	bool IsParkouring() const { return bIsParkour; }

	const USurvivorData* GetSurvivorData() const { return SurvivorData; }
	USPInteractionComponent* GetInteractionComponent() const { return InteractionComponent; }

	void BeginPickup(ASPCollectibleItem* Item);
	void BeginDelivery(ASPDeliveryStation* Station);
	void BeginEscapeOpen(ASPEscapeGate* Gate);
	void EndEscapeChanneling();
	void BeginHatchEscape(ASPHatch* Hatch);
	void CompleteHatchEscape();
	bool IsCarrying() const;

	USPInventoryComponent* GetInventoryComponent() const { return InventoryComponent; }

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "SP|Inventory")
	bool TryAcquireConsumable(EConsumableItemType ItemType);

	FGameplayTag GetInteractableTag() const;
	virtual void GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SP|Inventory")
	TObjectPtr<USPInventoryComponent> InventoryComponent;

protected:
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	virtual void Move(const FInputActionValue& Value) override;
	virtual void Interact() override;
	virtual void JumpOver() override;

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

private:
	UFUNCTION()
	void OnRep_SurvivorState(ESurvivorState OldState);

	UFUNCTION()
	void HandleInventoryChanged();

	UFUNCTION()
	void OnParkourMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	void BindInventoryHudRefresh();
	void RefreshLocalInventoryHud() const;
	void ApplyStateEffects();
	void NotifyMatchStateChange(ESurvivorState NewState);

	bool TraceParkourObstacle(FHitResult& OutObstacleHit, float& OutObstacleHeight, FString* OutFailReason = nullptr) const;
	void GetParkourFacing(FVector& OutForward, FVector& OutRight) const;
	void EnsureParkourMontagesLoaded();
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

	UPROPERTY(VisibleAnywhere, Category = "SP|Component")
	TObjectPtr<USPInteractionComponent> InteractionComponent;

	UPROPERTY(VisibleAnywhere, Category = "SP|Component")
	TObjectPtr<USPMovementComponent> MovementComponent;

	UPROPERTY(ReplicatedUsing = "OnRep_SurvivorState")
	ESurvivorState SurvivorState = ESurvivorState::Healthy;

	UPROPERTY(EditDefaultsOnly, Category = "SP|Data")
	TObjectPtr<USurvivorData> SurvivorData;

	UPROPERTY(VisibleAnywhere, Category = "SP|Tags")
	FGameplayTagContainer OwningTag;

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

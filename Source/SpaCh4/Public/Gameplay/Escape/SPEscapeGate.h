#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interface/SPInteractable.h"
#include "SPEscapeGate.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class UBoxComponent;
class UPrimitiveComponent;
class UArrowComponent;
class ASurvivorCharacter;
class USPEscapeLeverSoundComponent;

UCLASS()
class SPACH4_API ASPEscapeGate : public AActor, public ISPInteractable
{
	GENERATED_BODY()

public:
	ASPEscapeGate();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual void SetHighlight_Implementation(bool bEnabled) override;
	virtual FGameplayTag GetInteractableTag_Implementation() const override;
	virtual bool IsInteractable_Implementation() const override;
	virtual USceneComponent* GetInteractFocusComponent_Implementation() const override;

	bool IsActivated() const { return bIsActivated; }
	float GetOpenProgress() const { return OpenProgress; }
	float GetOpenDuration() const { return OpenDuration; }

	bool CanBeOpened() const;

	FTransform GetInteractAnchorTransform() const;

	void NotifyLeverPullStart();
	void NotifyLeverRelease();

	void SetOpener(ASurvivorCharacter* Opener);
	void ClearOpener(ASurvivorCharacter* Opener);

	USceneComponent* GetLeverPivot() const { return LeverPivot; }
	UStaticMeshComponent* GetSwitchMesh() const { return SwitchMesh; }
	UStaticMeshComponent* GetLeftDoorMesh() const { return LeftDoorMesh; }
	UStaticMeshComponent* GetRightDoorMesh() const { return RightDoorMesh; }
	USPEscapeLeverSoundComponent* GetLeverSoundComponent() const { return LeverSoundComponent; }

protected:
	virtual void BeginPlay() override;
	
	UFUNCTION()
	void OnExitTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnRep_IsActivated();
	
	UFUNCTION()
	void OnEscapeAvailabilityChanged(bool bCanActivate);
	
	void BindAvailabilityDelegate();
	void RefreshInteractionCollision();
	void CancelCurrentOpening();
	void UpdateLeverRotation(float DeltaSeconds);
	void CacheDoorClosedLocations();
	void UpdateDoorMovement(float DeltaSeconds);
	void ApplyDoorMovement(float NormalizedOpenAlpha);
	void NotifyLeverChannelStopped(bool bCompleted);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_NotifyLeverChannelStarted();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_NotifyLeverChannelStopped(bool bCompleted);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SP|Escape|Sound")
	TObjectPtr<USPEscapeLeverSoundComponent> LeverSoundComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SP|Escape")
	TObjectPtr<UStaticMeshComponent> SwitchMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SP|Escape")
	TObjectPtr<USceneComponent> LeverPivot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SP|Escape")
	TObjectPtr<UStaticMeshComponent> LeverPanelMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SP|Escape")
	TObjectPtr<UStaticMeshComponent> DoorMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SP|Escape|Door")
	TObjectPtr<UStaticMeshComponent> LeftDoorMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SP|Escape|Door")
	TObjectPtr<UStaticMeshComponent> RightDoorMesh;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SP|Escape")
	TObjectPtr<UBoxComponent> ExitTrigger;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SP|Escape")
	TObjectPtr<UArrowComponent> InteractAnchor;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SP|Escape")
	float OpenDuration = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SP|Escape|Debug")
	bool bDebugBypassDelivery = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SP|Escape")
	TArray<float> ProgressCheckpoints = {0.33f, 0.66f};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SP|Escape|Door", meta = (Units = "cm"))
	FVector LeftDoorOpenOffset = FVector(-100.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SP|Escape|Door", meta = (Units = "cm"))
	FVector RightDoorOpenOffset = FVector(100.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SP|Escape|Door", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float DoorMoveDuration = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SP|Escape|Lever")
	FRotator LeverInitialRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SP|Escape|Lever")
	FRotator LeverPulledRotation = FRotator(0.0f, 0.0f, -30.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SP|Escape|Lever")
	FRotator LeverCompletedRotation = FRotator(0.0f, 0.0f, -60.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SP|Escape|Lever")
	float LeverRotateInterpSpeed = 8.0f;

	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "SP|Escape")
	float OpenProgress = 0.0f;

	UPROPERTY(ReplicatedUsing = OnRep_IsActivated, VisibleAnywhere, BlueprintReadOnly, Category = "SP|Escape")
	bool bIsActivated = false;

private:
	float SnapProgressToCheckpoint(float CurrentProgress) const;

	TWeakObjectPtr<ASurvivorCharacter> CurrentOpener;

	bool bLeverVisualPulled = false;

	FRotator CurrentLeverOffset = FRotator::ZeroRotator;

	FRotator InitialLeverPivotRotation = FRotator::ZeroRotator;

	FVector LeftDoorClosedLocation = FVector::ZeroVector;

	FVector RightDoorClosedLocation = FVector::ZeroVector;

	float DoorOpenAlpha = 0.0f;

	bool bDoorClosedLocationsCached = false;
};

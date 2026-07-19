#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interface/SPInteractable.h"
#include "SPHatch.generated.h"

class UArrowComponent;
class UBoxComponent;
class UCurveFloat;
class UPrimitiveComponent;
class UStaticMeshComponent;
class ASurvivorCharacter;

UCLASS()
class SPACH4_API ASPHatch : public AActor, public ISPInteractable
{
	GENERATED_BODY()

public:
	ASPHatch();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual void SetHighlight_Implementation(bool bEnabled) override;
	virtual FGameplayTag GetInteractableTag_Implementation() const override;
	virtual bool IsInteractable_Implementation() const override;

	bool IsActive() const { return bIsActive; }
	bool IsOpened() const { return bIsOpened; }
	bool CanBeSelected() const;
	bool CanBeOpened() const;

	float GetOpenProgress() const { return OpenProgress; }
	float GetOpenDuration() const { return ActiveOpenDuration; }

	bool TryBeginOpening(ASurvivorCharacter* Opener);
	void CancelOpening(ASurvivorCharacter* Opener);

	void ActivateHatch();
	void DeactivateHatch();

	UFUNCTION(BlueprintCallable, Category = "SP|Hatch|Door")
	void SetDoorRotation(const FRotator& NewRotation);

	UFUNCTION(BlueprintCallable, Category = "SP|Hatch|Door")
	FRotator GetDoorRotation() const { return DoorRotation; }

protected:
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void OnRep_IsActive();

	UFUNCTION()
	void OnRep_IsOpened();

	UFUNCTION()
	void OnRep_DoorOpenStartServerTime();

	UFUNCTION()
	void OnEscapeTriggerBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	void CompleteOpening();
	void ApplyHatchState();
	void UpdateDoorOpeningPresentation();
	void ApplyDoorRotation(float NormalizedTime);
	float GetDoorRotationAlpha() const;
	float GetSynchronizedWorldTimeSeconds() const;
	void SetHatchVisibility(bool bVisible);
	void SetHatchRenderCustomDepth(bool bEnabled);
	void SetInteractionCollisionEnabled(bool bEnabled);
	void SetEscapeTriggerEnabled(bool bEnabled);
	void EnsureDoorComponentHierarchy();
	bool HasRequiredVisuals() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SP|Hatch")
	TObjectPtr<UStaticMeshComponent> TrayMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SP|Hatch|Door")
	TObjectPtr<UArrowComponent> DoorPivot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SP|Hatch|Door")
	TObjectPtr<UStaticMeshComponent> DoorMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SP|Hatch|Escape")
	TObjectPtr<UBoxComponent> EscapeTrigger;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SP|Hatch|Door")
	FRotator DoorRotation = FRotator::ZeroRotator;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SP|Hatch|Door", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float DoorRotationDuration = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SP|Hatch|Door")
	TObjectPtr<UCurveFloat> DoorRotationCurve;

	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "SP|Hatch")
	float OpenProgress = 0.0f;

	UPROPERTY(ReplicatedUsing = OnRep_IsActive, VisibleAnywhere, BlueprintReadOnly, Category = "SP|Hatch")
	bool bIsActive = false;

	UPROPERTY(ReplicatedUsing = OnRep_IsOpened, VisibleAnywhere, BlueprintReadOnly, Category = "SP|Hatch")
	bool bIsOpened = false;

	UPROPERTY(ReplicatedUsing = OnRep_DoorOpenStartServerTime, VisibleAnywhere, BlueprintReadOnly, Category = "SP|Hatch|Door")
	float DoorOpenStartServerTime = -1.0f;

private:
	TWeakObjectPtr<ASurvivorCharacter> CurrentOpener;
	float ActiveOpenDuration = 0.0f;
	FRotator ClosedDoorRotation = FRotator::ZeroRotator;
};

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interface/SPInteractable.h"
#include "Cage.generated.h"

class ASurvivorCharacter;
class UArrowComponent;
class USceneComponent;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class ECageStatus : uint8
{
	Empty       = 0 UMETA(DisplayName = "Empty"),
	Occupied    = 1 UMETA(DisplayName = "Occupied"),
	Dead        = 2 UMETA(DisplayName = "Dead")
};

UCLASS()
class SPACH4_API ACage : public AActor, public ISPInteractable
{
	GENERATED_BODY()

public:
	ACage();

	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;

	void SetCageStatus(ECageStatus NewStatus);
	void SetOccupied(ASurvivorCharacter* Survivor);

	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual void SetHighlight_Implementation(bool bEnabled) override;
	virtual FGameplayTag GetInteractableTag_Implementation() const override;
	virtual bool IsInteractable_Implementation() const override;
	virtual USceneComponent* GetInteractFocusComponent_Implementation() const override;

	ASurvivorCharacter* GetTrappedSurvivor() const { return TrappedSurvivor; }
	FTransform GetPrisonerAnchorTransform() const;

	UFUNCTION(BlueprintPure, Category = "Cage")
	float GetRescueDuration() const { return RescueDuration; }

	UFUNCTION(BlueprintPure, Category = "Cage")
	ECageStatus GetCageStatus() const { return CurrentStatus; }

	UFUNCTION(BlueprintCallable, Category = "Cage|Door")
	void SetDoorRotation(const FRotator& NewRotation);

	UFUNCTION(BlueprintCallable, Category = "Cage|Door")
	FRotator GetDoorRotation() const { return DoorRotation; }

	UFUNCTION(BlueprintPure, Category = "Cage")
	float GetStageOneDuration() const { return StageOneDuration; }

	UFUNCTION(BlueprintPure, Category = "Cage")
	float GetStageTwoDuration() const { return StageTwoDuration; }
	
	UFUNCTION(BlueprintPure, Category = "Cage")
	UStaticMeshComponent* GetCageMesh() const { return CageMesh; }
	
	UFUNCTION(BlueprintPure, Category = "Cage")
	FTransform GetCageMeshTransform() const;
	
	UFUNCTION()
	void HandleSurvivorDeath(ASurvivorCharacter* DeadSurvivor);
	
protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	
	// 하강 애니메이션 타이머
	FTimerHandle MoveTimerHandle;
    
	// 삭제 처리 타이머
	FTimerHandle DeleteTimerHandle;
	
	int32 MoveSteps;
	
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_DestroyCageMesh();
	
	UPROPERTY(Replicated)
	FVector TargetLocation;
	
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_UpdateCageLocation(FVector NewLocation);

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	void EnsureDoorComponentHierarchy();
	void ApplyDoorMeshAttachment();
	void ApplyDoorRotation();
	void ApplySupportMeshScale();
	void SyncSupportMeshScaleFromEditorComponent();
	void ApplyAssemblyRotation();
	void SyncAssemblyRotationFromEditorComponent();
	void ConfigureCollisionChannels();

#if WITH_EDITOR
	bool bApplyingSupportMeshScaleFromProperty = false;
#endif

	/** Actor root for level placement. Use SceneRoot rotation to orient the whole cage. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cage")
	TObjectPtr<USceneComponent> PlacementRoot;

	/** Rotate this component to orient the support mesh, cage mesh, and door together. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cage")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cage")
	TObjectPtr<USceneComponent> CageRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cage|Support")
	TObjectPtr<USceneComponent> SupportMeshScaleRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cage|Support")
	TObjectPtr<UStaticMeshComponent> SupportMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cage")
	TObjectPtr<UStaticMeshComponent> CageMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cage|Door")
	TObjectPtr<UArrowComponent> DoorPivot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cage|Door")
	TObjectPtr<UStaticMeshComponent> DoorMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cage|Prisoner")
	TObjectPtr<UArrowComponent> PrisonerAnchor;

	/** Rotates support mesh, cage mesh, and door together. Synced with SceneRoot when PlacementRoot is the actor root. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cage")
	FRotator AssemblyRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cage|Door")
	FRotator DoorRotation = FRotator::ZeroRotator;

	/** If true, DoorMesh is parented to DoorPivot and rotates with it. If false, DoorMesh stays on CageMesh. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cage|Door")
	bool bDoorMeshFollowsPivot = true;

	/** Scale applied to SupportMeshScaleRoot. CageMesh is unaffected. Also synced when scaling SupportMeshScaleRoot in the editor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cage|Support", meta = (ClampMin = "0.01"))
	FVector SupportMeshScale = FVector(1.f);

	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "Cage")
	ECageStatus CurrentStatus = ECageStatus::Empty;

	UPROPERTY(Replicated)
	TObjectPtr<ASurvivorCharacter> TrappedSurvivor;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cage")
	float StageOneDuration = 35.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cage")
	float StageTwoDuration = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cage")
	float RescueDuration = 4.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cage")
	ECageStatus CageStage = ECageStatus::Empty;
};

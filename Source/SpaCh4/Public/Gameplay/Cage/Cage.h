#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
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
class SPACH4_API ACage : public AActor
{
	GENERATED_BODY()

public:
	ACage();

	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;

	void SetCageStatus(ECageStatus NewStatus);

	UFUNCTION(BlueprintPure, Category = "Cage")
	ECageStatus GetCageStatus() const { return CurrentStatus; }

	UFUNCTION(BlueprintCallable, Category = "Cage|Door")
	void SetDoorRotation(const FRotator& NewRotation);

	UFUNCTION(BlueprintCallable, Category = "Cage|Door")
	FRotator GetDoorRotation() const { return DoorRotation; }

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cage")
	ECageStatus CurrentStatus = ECageStatus::Empty;

	UPROPERTY(Replicated)
	TObjectPtr<ASurvivorCharacter> TrappedSurvivor;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cage")
	float StageOneDuration = 35.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cage")
	float StageTwoDuration = 35.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cage")
	float RescueDuration = 4.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cage")
	ECageStatus CageStage = ECageStatus::Empty;
};

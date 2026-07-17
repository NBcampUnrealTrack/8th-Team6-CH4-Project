#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interface/SPInteractable.h"
#include "SPHatch.generated.h"

class UArrowComponent;
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

	bool IsSpawned() const { return bIsSpawned; }

	float GetEscapeProgress() const { return EscapeProgress; }
	float GetEscapeDuration() const { return EscapeDuration; }

	void SetEscaper(ASurvivorCharacter* Escaper);
	void ClearEscaper(ASurvivorCharacter* Escaper);

	UFUNCTION(BlueprintCallable, Category = "SP|Hatch|Door")
	void SetDoorRotation(const FRotator& NewRotation);

	UFUNCTION(BlueprintCallable, Category = "SP|Hatch|Door")
	FRotator GetDoorRotation() const { return DoorRotation; }

protected:
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UFUNCTION()
	void OnRep_IsSpawned();
	
	UFUNCTION()
	void OnHatchAvailabilityChanged(bool bCanSpawn);
	void BindAvailabilityDelegate();
	
	void ApplyHatchVisibility();
	void SetHatchRenderCustomDepth(bool bEnabled);
	void SetHatchCollisionEnabled(bool bEnabled);
	void EnsureDoorComponentHierarchy();
	void ApplyDoorRotation();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SP|Hatch")
	TObjectPtr<UStaticMeshComponent> TrayMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SP|Hatch|Door")
	TObjectPtr<UArrowComponent> DoorPivot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SP|Hatch|Door")
	TObjectPtr<UStaticMeshComponent> DoorMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SP|Hatch|Door")
	FRotator DoorRotation = FRotator::ZeroRotator;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SP|Hatch")
	float EscapeDuration = 3.0f;

	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "SP|Hatch")
	float EscapeProgress = 0.0f;
	
	UPROPERTY(ReplicatedUsing = OnRep_IsSpawned, VisibleAnywhere, BlueprintReadOnly, Category = "SP|Hatch")
	bool bIsSpawned = false;

private:
	TWeakObjectPtr<ASurvivorCharacter> CurrentEscaper;
};

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interface/SPInteractable.h"
#include "SPHatch.generated.h"

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
	
	void SetEscaper(ASurvivorCharacter* Escaper);
	void ClearEscaper(ASurvivorCharacter* Escaper);

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnRep_IsSpawned();
	
	UFUNCTION()
	void OnHatchAvailabilityChanged(bool bCanSpawn);
	void BindAvailabilityDelegate();
	
	void ApplyHatchVisibility();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SP|Hatch")
	TObjectPtr<UStaticMeshComponent> HatchMesh;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SP|Hatch")
	float EscapeDuration = 3.0f;

	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "SP|Hatch")
	float EscapeProgress = 0.0f;
	
	UPROPERTY(ReplicatedUsing = OnRep_IsSpawned, VisibleAnywhere, BlueprintReadOnly, Category = "SP|Hatch")
	bool bIsSpawned = false;

private:
	TWeakObjectPtr<ASurvivorCharacter> CurrentEscaper;
};

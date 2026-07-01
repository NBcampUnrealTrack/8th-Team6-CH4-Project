#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interface/SPInteractable.h"
#include "SPEscapeGate.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class UPrimitiveComponent;
class ASurvivorCharacter;

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

	bool IsActivated() const { return bIsActivated; }

	void SetOpener(ASurvivorCharacter* Opener);
	void ClearOpener(ASurvivorCharacter* Opener);

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnRep_IsActivated();
	
	UFUNCTION()
	void OnEscapeAvailabilityChanged(bool bCanActivate);

	UFUNCTION()
	void OnExitTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SP|Escape")
	TObjectPtr<UStaticMeshComponent> Mesh;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SP|Escape")
	TObjectPtr<UBoxComponent> ExitTrigger;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SP|Escape")
	float OpenDuration = 8.0f;
	
	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "SP|Escape")
	float OpenProgress = 0.0f;

	UPROPERTY(ReplicatedUsing = OnRep_IsActivated, VisibleAnywhere, BlueprintReadOnly, Category = "SP|Escape")
	bool bIsActivated = false;

private:
	bool AreAllGatesActivated() const;
	
	TWeakObjectPtr<ASurvivorCharacter> CurrentOpener;
};

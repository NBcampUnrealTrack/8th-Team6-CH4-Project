#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interface/SPInteractable.h"
#include "SPPickupItem.generated.h"

class ASurvivorCharacter;
class UStaticMeshComponent;
class UTexture2D;

UCLASS(Abstract, Blueprintable)
class SPACH4_API ASPPickupItem : public AActor, public ISPInteractable
{
	GENERATED_BODY()

public:
	ASPPickupItem();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual void SetHighlight_Implementation(bool bEnabled) override;
	virtual FGameplayTag GetInteractableTag_Implementation() const override;
	virtual bool IsInteractable_Implementation() const override;

	TSoftObjectPtr<UTexture2D> GetIcon() const { return Icon; }
	UStaticMeshComponent* GetItemMesh() const { return Mesh; }
	bool IsStored() const { return bStored; }

	bool TryReserve(ASurvivorCharacter* Survivor);
	void ReleaseReservation(ASurvivorCharacter* Survivor);
	void SetStored(bool bNewStored, const FVector& WorldLocation);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SP|Item")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SP|Item")
	TSoftObjectPtr<UTexture2D> Icon;

private:
	UFUNCTION()
	void OnRep_Stored();

	void ApplyStoredState();

	UPROPERTY(ReplicatedUsing = OnRep_Stored)
	bool bStored = false;

	UPROPERTY(Replicated)
	TObjectPtr<ASurvivorCharacter> ReservedBy;
};

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interface/SPInteractable.h"
#include "SPDeliveryStation.generated.h"

class UStaticMeshComponent;
class ASurvivorCharacter;

UCLASS()
class SPACH4_API ASPDeliveryStation : public AActor, public ISPInteractable
{
	GENERATED_BODY()

public:
	ASPDeliveryStation();

	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual void SetHighlight_Implementation(bool bEnabled) override;
	virtual FGameplayTag GetInteractableTag_Implementation() const override;
	virtual bool IsInteractable_Implementation() const override;
	
	void SubmitValue(int32 Value) const;
	bool IsComplete() const;
	float GetDeliveryDuration() const { return DeliveryDuration; }
	FName GetStationId() const { return StationId; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SP|Delivery")
	TObjectPtr<UStaticMeshComponent> Mesh;

	// A or B만 지정
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SP|Delivery")
	FName StationId = "A";
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SP|Delivery")
	float DeliveryDuration = 2.5f;
};

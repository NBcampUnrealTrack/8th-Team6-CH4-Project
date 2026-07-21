#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interface/SPInteractable.h"
#include "SPInteractableActor.generated.h"

class UStaticMeshComponent;


UCLASS()
class SPACH4_API ASPInteractableActor : public AActor, public ISPInteractable
{
	GENERATED_BODY()

public:
	ASPInteractableActor();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void Interact_Implementation(AActor* Interactor) override;

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "SP|Interaction")
	void OnInteracted(AActor* Interactor);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SP|Interaction")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category = "SP|Interaction")
	int32 InteractCount = 0;

	UPROPERTY(EditAnywhere, Category = "SP|Debug")
	bool bDebugLog = true;
	
};

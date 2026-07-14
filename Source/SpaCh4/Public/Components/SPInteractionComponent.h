#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "SPInteractionComponent.generated.h"

class ASurvivorCharacter;
class USurvivorData;
class ASPCollectibleItem;
class ASPDeliveryStation;
class ASPEscapeGate;
class ASPHatch;
class UAnimMontage;


UCLASS(ClassGroup = (SP), meta = (BlueprintSpawnableComponent))
class SPACH4_API USPInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USPInteractionComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	
	void RequestInteract();   
	void NotifyMoveInput();   
	void BeginPickup(ASPCollectibleItem* Item);
	void BeginDelivery(ASPDeliveryStation* Station);
	void BeginEscapeOpen(ASPEscapeGate* Gate);
	void EndEscapeChanneling();
	void BeginHatchEscape(ASPHatch* Hatch);
	void CompleteHatchEscape();
	void CancelInteract();

	bool IsCarrying() const;
	bool IsInteracting() const { return bIsInteract; }
	bool ShouldCancelOnMove() const { return bCancelInteractOnMove; }
	FGameplayTag GetInteractableTag() const { return InteractableTag; }

protected:
	virtual void BeginPlay() override;

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_Interact();

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_CancelInteract();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_FaceInteractTarget(float TargetYaw);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayInteractMontage(UAnimMontage* Montage);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_StopInteractMontage();

private:
	ASurvivorCharacter* GetSurvivor() const;
	const USurvivorData* GetSurvivorData() const;

	void UpdateInteract();
	bool TraceInteractable(FHitResult& OutHit) const;

	void CompletePickup();
	void BeginDrop();
	void CompleteDrop();
	FVector ResolveGroundedDropLocation(const ASurvivorCharacter* Survivor, ASPCollectibleItem* Item) const;
	void CompleteDelivery();
	void FaceInteractTarget(const AActor* Target);
	void PlayInteractMontage(UAnimMontage* Montage);
	void StopInteractMontage();

	UPROPERTY(EditDefaultsOnly, Category = "SP|Interact")
	float InteractReach{200.f};

	UPROPERTY(EditDefaultsOnly, Category = "SP|Interact")
	float InteractRadius{20.f};

	UPROPERTY(EditDefaultsOnly, Category = "SP|Interact")
	bool bCancelInteractOnMove{true};

	UPROPERTY(EditDefaultsOnly, Category = "SP|Interact")
	bool bDrawDebug{false};
	
	UPROPERTY(EditDefaultsOnly, Category = "SP|Carry")
	bool bInstantPickup{false};

	UPROPERTY(EditDefaultsOnly, Category = "SP|Drop")
	float DropForwardOffset{70.f};

	UPROPERTY(EditDefaultsOnly, Category = "SP|Drop")
	float DropTraceDistance{300.f};

	UPROPERTY(EditDefaultsOnly, Category = "SP|Interact|Montage")
	TSoftObjectPtr<UAnimMontage> PickupMontage;

	UPROPERTY(EditDefaultsOnly, Category = "SP|Interact|Montage")
	TSoftObjectPtr<UAnimMontage> DropMontage;

	UPROPERTY(EditDefaultsOnly, Category = "SP|Interact|Montage")
	TSoftObjectPtr<UAnimMontage> DeliveryMontage;

	UPROPERTY(EditDefaultsOnly, Category = "SP|Interact|Montage")
	TSoftObjectPtr<UAnimMontage> HatchEscapeMontage;
	
	UPROPERTY(Replicated)
	bool bIsInteract{false};


	FTimerHandle PickupDropTimer;
	TWeakObjectPtr<ASPCollectibleItem> CurrentPickupItem;
	TWeakObjectPtr<ASPDeliveryStation> CurrentDeliveryStation;
	TWeakObjectPtr<ASPEscapeGate> CurrentEscapeGate;
	TWeakObjectPtr<ASPHatch> CurrentHatch;
	TWeakObjectPtr<AActor> LastActor;
	FGameplayTag InteractableTag;
};

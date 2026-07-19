#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "SPInteractionComponent.generated.h"

class ASurvivorCharacter;
class USurvivorData;
class ASPPickupItem;
class ASPDeliveryStation;
class ASPEscapeGate;
class ASPHatch;
class UAnimMontage;
class ACage;

UCLASS(ClassGroup = (SP), meta = (BlueprintSpawnableComponent))
class SPACH4_API USPInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USPInteractionComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void RequestInteract();
	void RequestDrop();
	void RequestCancelInteract();
	void NotifyMoveInput();
	void BeginPickup(ASPPickupItem* Item);
	void BeginDelivery(ASPDeliveryStation* Station);
	void BeginEscapeOpen(ASPEscapeGate* Gate);
	void EndEscapeChanneling();
	void BeginHatchOpen(ASPHatch* Hatch);
	void CompleteHatchOpen();
	void BeginRescue(ACage* Cage);
	void CancelInteract();
	void DropAllItems();

	bool IsCarrying() const;

	UFUNCTION(BlueprintPure, Category = "SP|Interact")
	bool IsInteracting() const { return bIsInteract; }

	UFUNCTION(BlueprintPure, Category = "SP|Interact")
	bool CanSelfHeal() const;

	bool ShouldCancelOnMove() const { return bCancelInteractOnMove; }

	UFUNCTION(BlueprintPure, Category = "SP|Interact")
	FGameplayTag GetInteractableTag() const { return InteractableTag; }

	UFUNCTION(BlueprintPure, Category = "SP|Interact")
	float GetInteractProgress() const { return InteractProgress; }

protected:
	virtual void BeginPlay() override;

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_Interact();

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_Drop();

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
	float ComputeInteractProgress() const;

	void CompletePickup();
	void BeginDrop();
	void CompleteDrop();
	FVector ResolveGroundedDropLocation(const ASurvivorCharacter* Survivor, ASPPickupItem* Item) const;
	void CompleteDelivery();
	void CompleteRescue();
	bool TryUseSelectedConsumable();
	bool TryBeginSelfHeal();
	bool TryBeginSpeedPotionUse();
	bool IsSelectedSlotMedkit() const;
	void CompleteHeal();
	void CancelHealChannel();
	void CompleteSpeedPotionUse();
	void CancelSpeedPotionChannel();
	void FaceInteractTarget(AActor* Target);
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

	UPROPERTY(EditDefaultsOnly, Category = "SP|Interact|Montage")
	TSoftObjectPtr<UAnimMontage> RescueMontage;

	UPROPERTY(EditDefaultsOnly, Category = "SP|Interact|Montage")
	TSoftObjectPtr<UAnimMontage> SpeedPotionUseMontage;

	UPROPERTY(Replicated)
	bool bIsInteract{false};

	UPROPERTY(Replicated)
	float InteractProgress{0.f};

	bool bIsSelfHealing{false};
	bool bIsUsingSpeedPotion{false};
	int32 ActiveConsumableSlotIndex = INDEX_NONE;

	FTimerHandle PickupDropTimer;
	FTimerHandle HealTimer;
	FTimerHandle SpeedPotionUseTimer;
	FTimerHandle RescueTimer;
	TWeakObjectPtr<ASPPickupItem> CurrentPickupItem;
	TWeakObjectPtr<ASPDeliveryStation> CurrentDeliveryStation;
	TWeakObjectPtr<ASPEscapeGate> CurrentEscapeGate;
	TWeakObjectPtr<ASPHatch> CurrentHatch;
	TWeakObjectPtr<ACage> CurrentCage;
	TWeakObjectPtr<AActor> LastActor;
	FGameplayTag InteractableTag;
};

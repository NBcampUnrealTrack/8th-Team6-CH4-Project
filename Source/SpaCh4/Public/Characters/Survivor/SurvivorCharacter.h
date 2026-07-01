#pragma once

#include "CoreMinimal.h"
#include "Characters/Base/CharacterBase.h"
#include "GameplayTagAssetInterface.h"
#include "GameplayTagContainer.h"
#include "SurvivorCharacter.generated.h"

class USurvivorData;
class ASPCollectibleItem;
class ASPDeliveryStation;
class ASPEscapeGate;

UENUM(BlueprintType)
enum class ESurvivorState : uint8
{
	Healthy,
	Injured,
	Downed,
	Carried,
	Caged,
	Dead,
	Escaped
};

UCLASS()
class SPACH4_API ASurvivorCharacter : public ACharacterBase, public IGameplayTagAssetInterface
{
	GENERATED_BODY()

public:
	ASurvivorCharacter();
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	
	void SetSurvivorState(ESurvivorState NewState);
	ESurvivorState GetSurvivorState() const { return SurvivorState; };
	
	bool CanMove() const;
	bool CanInteract() const;
	bool CanJumpOver() const;
	
	void BeginPickup(ASPCollectibleItem* Item);
	void BeginDelivery(ASPDeliveryStation* Station);
	void BeginEscapeOpen(ASPEscapeGate* Gate);
	void EndEscapeChanneling();
	bool IsCarrying() const { return CarriedItem != nullptr; }
	
	FGameplayTag GetInteractableTag() const { return InteractableTag; }
	virtual void GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const override;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_Interact();
	
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_CancelInteract();

	virtual void Move(const FInputActionValue& Value) override;
	virtual void Interact() override;
	virtual void JumpOver() override;
private:
	UFUNCTION()
	void OnRep_SurvivorState();
	
	void ApplyStateEffects();
	void CompletePickup();
	void BeginDrop();
	void CompleteDrop();
	void CompleteDelivery();
	void CancelInteract();
	
	void UpdateInteract();
	bool TraceInteractable(FHitResult& OutHit) const;
	
	/* 생존자의 State & Interact 관리 */
	UPROPERTY(ReplicatedUsing="OnRep_SurvivorState")
	ESurvivorState SurvivorState = ESurvivorState::Healthy;
	
	UPROPERTY(EditDefaultsOnly, Category = "SP|Interact")
	float InteractReach{200.f};
	
	UPROPERTY(EditDefaultsOnly, Category = "SP|Interact")
	float InteractRadius{20.f};

	// 상호작용 중 이동 시 상호작용 취소 여부
	UPROPERTY(EditDefaultsOnly, Category = "SP|Interact")
	bool bCancelInteractOnMove{true};

	UPROPERTY(EditDefaultsOnly, Category = "SP|Data")
	TObjectPtr<USurvivorData> SurvivorData;

	// TODO: SurvivorData로 이관
	UPROPERTY(EditDefaultsOnly, Category = "SP|Movement")
	float DownedWalkSpeed{150.f};

	UPROPERTY(Replicated)
	TObjectPtr<ASPCollectibleItem> CarriedItem;

	// 상호작용 중인지 판단
	UPROPERTY(Replicated)
	bool bIsInteract{false};
	
	UPROPERTY(EditDefaultsOnly, Category = "SP|Carry")
	FName CarrySocketName{"CarrySocket"};

	// 즉시 획득 여부
	UPROPERTY(EditDefaultsOnly, Category = "SP|Carry")
	bool bInstantPickup{false};
	
	UPROPERTY(VisibleAnywhere, Category = "SP|Tags")
	FGameplayTagContainer OwningTag;
	
	
	
	FTimerHandle PickupDropTimer;
	TWeakObjectPtr<ASPCollectibleItem> CurrentPickupItem;
	TWeakObjectPtr<ASPDeliveryStation> CurrentDeliveryStation;
	TWeakObjectPtr<ASPEscapeGate> CurrentEscapeGate;
	TWeakObjectPtr<AActor> LastActor;
	FGameplayTag InteractableTag;
	
};

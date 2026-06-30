#pragma once

#include "CoreMinimal.h"
#include "Characters/Base/CharacterBase.h"
#include "SurvivorCharacter.generated.h"

class USurvivorData;
class ASPCollectibleItem;

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
class SPACH4_API ASurvivorCharacter : public ACharacterBase
{
	GENERATED_BODY()

public:
	ASurvivorCharacter();
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	
	void SetSurvivorState(ESurvivorState NewState);
	bool CanMove() const;
	bool CanInteract() const;
	bool CanJumpOver() const;
	
	void BeginPickup(ASPCollectibleItem* Item);
	bool IsCarrying() const { return CarriedItem != nullptr; }

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_Interact();
	virtual void Interact() override;	
	virtual void JumpOver() override;
private:
	UFUNCTION()
	void OnRep_SurvivorState();
	
	void ApplyStateEffects();
	void CompletePickup();
	void BeginDrop();
	void CompleteDrop();
	void CancelInteract();
	
	void UpdateInteractFocus();
	bool TraceInteractable(FHitResult& OutHit) const;
	
	/* 생존자의 State & Interact 관리 */
	UPROPERTY(ReplicatedUsing="OnRep_SurvivorState")
	ESurvivorState SurvivorState = ESurvivorState::Healthy;
	
	UPROPERTY(EditDefaultsOnly, Category = "SP|Interact")
	float InteractReach{200.f};
	
	UPROPERTY(EditDefaultsOnly, Category = "SP|Interact")
	float InteractRadius{20.f};

	UPROPERTY(EditDefaultsOnly, Category = "SP|Data")
	TObjectPtr<USurvivorData> SurvivorData;

	// TODO: SurvivorData로 이관
	UPROPERTY(EditDefaultsOnly, Category = "SP|Movement")
	float DownedWalkSpeed{150.f};

	UPROPERTY(Replicated)
	TObjectPtr<ASPCollectibleItem> CarriedItem;

	UPROPERTY(EditDefaultsOnly, Category = "SP|Carry")
	FName CarrySocketName{"CarrySocket"};

	// 즉시 획득 여부
	UPROPERTY(EditDefaultsOnly, Category = "SP|Carry")
	bool bInstantPickup{false};
	
	
	bool bIsInteract{false};           
	FTimerHandle PickupDropTimer;
	TWeakObjectPtr<ASPCollectibleItem> CurrentPickupItem;
	TWeakObjectPtr<AActor> LastActor;
};

#pragma once

#include "CoreMinimal.h"
#include "Characters/Base/CharacterBase.h"
#include "GameplayTagAssetInterface.h"
#include "GameplayTagContainer.h"
#include "SurvivorCharacter.generated.h"

class USurvivorData;
class USPInteractionComponent;
class USPMovementComponent;
class ASPCollectibleItem;
class ASPDeliveryStation;
class ASPEscapeGate;
class ASPHatch;

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

	const USurvivorData* GetSurvivorData() const { return SurvivorData; }
	USPInteractionComponent* GetInteractionComponent() const { return InteractionComponent; }

	/* 상호작용 파사드 — 대상 액터/게이트/해치가 호출하면 컴포넌트로 위임 */
	void BeginPickup(ASPCollectibleItem* Item);
	void BeginDelivery(ASPDeliveryStation* Station);
	void BeginEscapeOpen(ASPEscapeGate* Gate);
	void EndEscapeChanneling();
	void BeginHatchEscape(ASPHatch* Hatch);
	void CompleteHatchEscape();
	bool IsCarrying() const;

	FGameplayTag GetInteractableTag() const;
	virtual void GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const override;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	virtual void Move(const FInputActionValue& Value) override;
	virtual void Interact() override;
	virtual void JumpOver() override;

private:
	UFUNCTION()
	void OnRep_SurvivorState(ESurvivorState OldState);

	/* 상태 처리 */
	void ApplyStateEffects();
	void NotifyMatchStateChange(ESurvivorState NewState);

	/* 컴포넌트 (상호작용 · 이동 정책) */
	UPROPERTY(VisibleAnywhere, Category = "SP|Component")
	TObjectPtr<USPInteractionComponent> InteractionComponent;

	UPROPERTY(VisibleAnywhere, Category = "SP|Component")
	TObjectPtr<USPMovementComponent> MovementComponent;

	/* 생존자 상태 */
	UPROPERTY(ReplicatedUsing = "OnRep_SurvivorState")
	ESurvivorState SurvivorState = ESurvivorState::Healthy;

	UPROPERTY(EditDefaultsOnly, Category = "SP|Data")
	TObjectPtr<USurvivorData> SurvivorData;

	UPROPERTY(VisibleAnywhere, Category = "SP|Tags")
	FGameplayTagContainer OwningTag;
};

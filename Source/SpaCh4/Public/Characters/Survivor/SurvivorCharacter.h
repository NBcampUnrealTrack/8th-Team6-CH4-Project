#pragma once

#include "CoreMinimal.h"
#include "Characters/Base/CharacterBase.h"
#include "GameplayTagAssetInterface.h"
#include "GameplayTagContainer.h"
#include "Inventory/SPInventoryTypes.h"
#include "SurvivorCharacter.generated.h"

class USurvivorData;
class USPInteractionComponent;
class USPMovementComponent;
class USPParkourComponent;
class USPScratchMarkComponent;
class USPOilDripComponent;
class USPInjuredSoundComponent;
class USPDownedCrawlSoundComponent;
class USPHealingSoundComponent;
class USPEscapeLeverComponent;
class USPPickupAnimComponent;
class USPHealingAnimComponent;
class ACage;
class ASPPickupItem;
class ASPDeliveryStation;
class ASPEscapeGate;
class ASPHatch;
class USPInventoryComponent;
class ALDPlayerState;
enum class ESurvivorEscapeMethod : uint8;

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
	ASurvivorCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category = "SP|Survivor")
	void SetSurvivorState(ESurvivorState NewState);
	ESurvivorState GetSurvivorState() const { return SurvivorState; };

	bool CanMove() const;
	bool CanInteract() const;
	bool CanJumpOver() const;
	bool IsParkouring() const;
	bool IsCarrying() const;
	bool IsPullingLever() const;
	bool IsChannelingLever() const;
	bool IsPlayingPickupAnim() const;
	bool IsHealing() const;
	bool IsChannelingHealing() const;
	int GetCagedCount() const { return CagedCount; }
	int32 GetSelectedSlotIndex() const { return SelectedSlotIndex; }

	const USurvivorData* GetSurvivorData() const { return SurvivorData; }
	USPInteractionComponent* GetInteractionComponent() const { return InteractionComponent; }
	USPMovementComponent* GetSPMovementComponent() const { return MovementComponent; }
	USPParkourComponent* GetParkourComponent() const { return ParkourComponent; }
	USPEscapeLeverComponent* GetEscapeLeverComponent() const { return EscapeLeverComponent; }
	USPPickupAnimComponent* GetPickupAnimComponent() const { return PickupAnimComponent; }
	USPHealingAnimComponent* GetHealingAnimComponent() const { return HealingAnimComponent; }
	USPInjuredSoundComponent* GetInjuredSoundComponent() const { return InjuredSoundComponent; }
	USPDownedCrawlSoundComponent* GetDownedCrawlSoundComponent() const { return DownedCrawlSoundComponent; }
	USPHealingSoundComponent* GetHealingSoundComponent() const { return HealingSoundComponent; }

	void BeginPickup(ASPPickupItem* Item);
	void BeginDelivery(ASPDeliveryStation* Station);
	void BeginEscapeOpen(ASPEscapeGate* Gate);
	void EndEscapeChanneling();
	void BeginHatchOpen(ASPHatch* Hatch);
	void CompleteHatchOpen();
	bool TryEscape(ESurvivorEscapeMethod EscapeMethod);

	void EnterCaged(ACage* Cage);
	void ApplyHit();
	void RecoverOneStep();
	void RescueFromCage(ASurvivorCharacter* Rescuer);
	void BeginRescue(ACage* Cage);

	USPInventoryComponent* GetInventoryComponent() const { return InventoryComponent; }

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "SP|Inventory")
	bool TryAcquireConsumable(EConsumableItemType ItemType);

	FGameplayTag GetInteractableTag() const;
	virtual void GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const override;

	void NotifyParkourEnded();
	void NotifyLeverChannelEnded();
	void NotifyPickupAnimEnded();
	void NotifyHealingAnimEnded();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SP|Inventory")
	TObjectPtr<USPInventoryComponent> InventoryComponent;
	
	ACage* GetCurrentCage() const { return CurrentCage; }
	
	void BeginHealingOther(ASurvivorCharacter* TargetSurvivor);
	void ReceiveHealing(ASurvivorCharacter* Healer);
	
	ASurvivorCharacter* FindHealableSurvivor(float Radius);

protected:
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;

	virtual void Move(const FInputActionValue& Value) override;
	virtual void Interact() override;
	virtual void JumpOver() override;
	
	virtual void OnRep_Controller() override;
	
	UPROPERTY()
	ACage* CurrentCage;
private:
	UFUNCTION()
	void OnRep_SurvivorState(ESurvivorState OldState);

	UFUNCTION()
	void HandleInventoryChanged();

	void SelectSlot(int32 Index);
	void DropSelectedItem();
	void StopInteract();

	UFUNCTION(Server, Reliable)
	void Server_SelectSlot(int32 Index);

	void StartRun();
	void StopRun();

	UFUNCTION()
	void OnCageExpired();
	
	UFUNCTION(BlueprintCallable, Category = "Survivor|Cage")
	void ExitCaged();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_ApplyCagedPose(FVector Location, FRotator Rotation);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_RestoreOrientRotation();

	// *** Debug
	UFUNCTION(Exec)
	void DebugCageSelf();

	UFUNCTION(Server, Reliable)
	void Server_DebugCageSelf();
	// *** Debug

	void BindInventoryHudRefresh();
	void RefreshLocalInventoryHud() const;
	void ApplyDeathRagdoll();
	void ApplyStateEffects();
	void RestoreAfterConfinement(ESurvivorState OldState, ESurvivorState NewState);
	bool TryPlaceAfterRescue(const ASurvivorCharacter* Rescuer);
	void NotifyMatchStateChange(ESurvivorState NewState, ALDPlayerState* SurvivorPlayerState);
	void ToggleCrouch();

	UPROPERTY(VisibleAnywhere, Category = "SP|Component")
	TObjectPtr<USPInteractionComponent> InteractionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SP|Component",  meta = (AllowPrivateAccess = true))
	TObjectPtr<USPMovementComponent> MovementComponent;

	UPROPERTY(VisibleAnywhere, Category = "SP|Component")
	TObjectPtr<USPParkourComponent> ParkourComponent;

	UPROPERTY(VisibleAnywhere, Category = "SP|Component")
	TObjectPtr<USPScratchMarkComponent> ScratchMarkComponent;

	UPROPERTY(VisibleAnywhere, Category = "SP|Component")
	TObjectPtr<USPOilDripComponent> OilDripComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SP|Component", meta = (AllowPrivateAccess = true))
	TObjectPtr<USPInjuredSoundComponent> InjuredSoundComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SP|Component", meta = (AllowPrivateAccess = true))
	TObjectPtr<USPDownedCrawlSoundComponent> DownedCrawlSoundComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SP|Component", meta = (AllowPrivateAccess = true))
	TObjectPtr<USPHealingSoundComponent> HealingSoundComponent;

	UPROPERTY(VisibleAnywhere, Category = "SP|Component")
	TObjectPtr<USPEscapeLeverComponent> EscapeLeverComponent;

	UPROPERTY(VisibleAnywhere, Category = "SP|Component")
	TObjectPtr<USPPickupAnimComponent> PickupAnimComponent;

	UPROPERTY(VisibleAnywhere, Category = "SP|Component")
	TObjectPtr<USPHealingAnimComponent> HealingAnimComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing = "OnRep_SurvivorState",  meta = (AllowPrivateAccess = true))
	ESurvivorState SurvivorState = ESurvivorState::Healthy;

	UPROPERTY(EditDefaultsOnly, Category = "SP|Data")
	TObjectPtr<USurvivorData> SurvivorData;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category = "SP|Data", meta = (AllowPrivateAccess = true))
	int32 CagedCount = 0;

	int32 SelectedSlotIndex = 0;
	
	UPROPERTY(VisibleAnywhere, Category = "SP|Tags")
	FGameplayTagContainer OwningTag;

	UPROPERTY(EditDefaultsOnly, Category = "SP|Cage")
	float RescueDropOffset = 100.f;

	UPROPERTY(EditDefaultsOnly, Category = "SP|Death")
	float DeathCamDuration = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "SP|Death")
	float CorpseLifetime = 6.0f;

	bool bDeathRagdollApplied = false;

	FTimerHandle CageTimerHandle;
};

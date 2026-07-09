#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SPPickupAnimComponent.generated.h"

class UAnimMontage;
class ASurvivorCharacter;

UCLASS(ClassGroup = (SP), meta = (BlueprintSpawnableComponent))
class SPACH4_API USPPickupAnimComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USPPickupAnimComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	bool IsPlayingPickupAnim() const { return bIsPlayingPickupAnim; }

	void BeginPickupAnim();
	void EndPickupAnim();
	void CancelPickupAnim();
	void ForceEndPickupAnim();

protected:
	void EnsureMontageLoaded();

private:
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_BeginPickupAnim();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_EndPickupAnim();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_CancelPickupAnim();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_ForceEndPickupAnim();

	UFUNCTION()
	void OnPickupMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	ASurvivorCharacter* GetSurvivor() const;
	void PlayPickupMontage();
	void StopPickupMontage();
	void EnterPickupState();
	void ExitPickupState();

	UPROPERTY(EditDefaultsOnly, Category = "SP|Pickup")
	TObjectPtr<UAnimMontage> PickupMontage;

	UPROPERTY(EditDefaultsOnly, Category = "SP|Pickup")
	float PickupMontageBlendOut = 0.2f;

	UPROPERTY(Replicated)
	bool bIsPlayingPickupAnim = false;

	TObjectPtr<UAnimMontage> ActivePickupMontage;
	FOnMontageEnded PickupMontageEndedDelegate;
};

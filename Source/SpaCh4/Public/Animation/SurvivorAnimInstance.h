#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "SurvivorAnimInstance.generated.h"

class UAnimSequence;

/** Native survivor anim instance — exposes Downed / prone crawl params to AnimBP. */
UCLASS()
class SPACH4_API USurvivorAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	UPROPERTY(BlueprintReadOnly, Category = "SP|Survivor|Locomotion")
	bool bIsDowned = false;

	UPROPERTY(BlueprintReadOnly, Category = "SP|Survivor|Locomotion")
	bool bIsCarried = false;

	/** Forward crawl speed for prone BlendSpace (0 = idle .. crawl max). Backward uses idle. */
	UPROPERTY(BlueprintReadOnly, Category = "SP|Survivor|Locomotion")
	float ProneSpeed = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "SP|Survivor|Locomotion")
	float GroundSpeed = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "SP|Survivor|Locomotion")
	bool bShouldMove = false;

	/** AnimGraph Call Function — avoids constant pin overwrite on TwoWayBlend/BlendList. */
	UFUNCTION(BlueprintPure, Category = "SP|Survivor|Locomotion")
	bool AnimIsDowned() const { return bIsDowned; }

	UFUNCTION(BlueprintPure, Category = "SP|Survivor|Locomotion")
	float AnimProneSpeed() const { return ProneSpeed; }

protected:
	/** Soft refs so ABP defaults are optional; falls back to Content paths. */
	UPROPERTY(EditDefaultsOnly, Category = "SP|Survivor|Prone")
	TSoftObjectPtr<UAnimSequence> ProneIdleAnim;

	UPROPERTY(EditDefaultsOnly, Category = "SP|Survivor|Prone")
	TSoftObjectPtr<UAnimSequence> ProneFwdAnim;

	UPROPERTY(EditDefaultsOnly, Category = "SP|Survivor|Carry")
	TSoftObjectPtr<UAnimSequence> CarriedPoseAnim;

	UPROPERTY(EditDefaultsOnly, Category = "SP|Survivor|Prone")
	FName ProneSlotName = TEXT("DefaultSlot");

	UPROPERTY(EditDefaultsOnly, Category = "SP|Survivor|Prone")
	float ProneSlotBlendIn = 0.2f;

	UPROPERTY(EditDefaultsOnly, Category = "SP|Survivor|Prone")
	float ProneSlotBlendOut = 0.25f;

	UPROPERTY(EditDefaultsOnly, Category = "SP|Survivor|Prone")
	float ProneMoveSpeedThreshold = 10.f;

private:
	void UpdateDownedAnimGraphNodes();
	void UpdateProneSlotPlayback();
	void StopProneSlotPlayback();
	void EnsureProneAnimsLoaded();
	UAnimSequence* ResolveProneSequence() const;
	UAnimSequence* LoadProneAnim(const TSoftObjectPtr<UAnimSequence>& Soft, const TCHAR* FallbackPath) const;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> CachedProneIdle;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> CachedProneFwd;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> CachedCarriedPose;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> ActiveProneSlotSequence;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveProneSlotMontage;

	bool bProneSlotPlaying = false;
};

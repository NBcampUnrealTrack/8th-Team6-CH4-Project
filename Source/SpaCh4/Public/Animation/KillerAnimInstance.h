#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "KillerAnimInstance.generated.h"

/** Native anim instance for killer; exposes first-person view pitch and carry flags to AnimBP. */
UCLASS()
class SPACH4_API UKillerAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	/** Camera pitch (degrees) applied to upper-body bones in AnimBP. */
	UPROPERTY(BlueprintReadOnly, Category = "SP|Killer|FirstPerson")
	float FPViewPitch = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "SP|Killer|Carry")
	bool bIsPickingUp = false;

	UPROPERTY(BlueprintReadOnly, Category = "SP|Killer|Carry")
	bool bIsCarrying = false;

	void SetFPViewPitch(float InPitch);
};

#include "Animation/KillerAnimInstance.h"

#include "Characters/Killer/KillerCharacter.h"

void UKillerAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	const AKillerCharacter* Killer = Cast<AKillerCharacter>(TryGetPawnOwner());
	if (!Killer)
	{
		bIsPickingUp = false;
		bIsCarrying = false;
		return;
	}

	const EKillerState State = Killer->GetKillerState();
	bIsPickingUp = State == EKillerState::PickingUp;
	bIsCarrying = State == EKillerState::Carrying;
}

void UKillerAnimInstance::SetFPViewPitch(float InPitch)
{
	FPViewPitch = InPitch;
}

#include "Components/SPMovementComponent.h"

#include "Characters/Survivor/SurvivorCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Systems/Data/SurvivorData.h"

USPMovementComponent::USPMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void USPMovementComponent::BeginPlay()
{
	Super::BeginPlay();

	SnapToTargetSpeed();
}

void USPMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bHitEscapeSprintActive)
	{
		HitEscapeSprintRemaining -= DeltaTime;
		if (HitEscapeSprintRemaining <= 0.f)
		{
			bHitEscapeSprintActive = false;
			HitEscapeSprintRemaining = 0.f;
		}
	}

	const ASurvivorCharacter* Survivor = GetSurvivor();
	UCharacterMovementComponent* MoveComp = Survivor ? Survivor->GetCharacterMovement() : nullptr;
	if (!MoveComp)
	{
		return;
	}

	const float Target = ComputeTargetMoveSpeed();
	MoveComp->MaxWalkSpeed = FMath::FInterpTo(MoveComp->MaxWalkSpeed, Target, DeltaTime, MoveSpeedInterpSpeed);
}

void USPMovementComponent::SnapToTargetSpeed()
{
	const ASurvivorCharacter* Survivor = GetSurvivor();
	if (UCharacterMovementComponent* MoveComp = Survivor ? Survivor->GetCharacterMovement() : nullptr)
	{
		MoveComp->MaxWalkSpeed = ComputeTargetMoveSpeed();
	}
}

ASurvivorCharacter* USPMovementComponent::GetSurvivor() const
{
	return Cast<ASurvivorCharacter>(GetOwner());
}

const USurvivorData* USPMovementComponent::GetSurvivorData() const
{
	const ASurvivorCharacter* Survivor = GetSurvivor();
	return Survivor ? Survivor->GetSurvivorData() : nullptr;
}

float USPMovementComponent::ComputeTargetMoveSpeed() const
{
	if (bHitEscapeSprintActive)
	{
		return HitEscapeSprintSpeed;
	}
	return GetBaseWalkSpeed();
}

float USPMovementComponent::GetBaseWalkSpeed() const
{
	const ASurvivorCharacter* Survivor = GetSurvivor();
	const USurvivorData* Data = GetSurvivorData();
	if (!Survivor || !Data)
	{
		return 0.f;
	}

	switch (Survivor->GetSurvivorState())
	{
	case ESurvivorState::Healthy:
		return Data->SurvivorWalkSpeed;

	case ESurvivorState::Injured:
		return Data->SurvivorWalkSpeed * Data->SurvivorInjuredSpeedMultiplier;

	default:
		// Downed/Carried/Caged/Dead/Escaped: 기본 이동 불가
		return 0.f;
	}
}

float USPMovementComponent::GetSprintSpeedForState(ESurvivorState State) const
{
	const USurvivorData* Data = GetSurvivorData();
	if (!Data)
	{
		return 0.f;
	}

	if (State == ESurvivorState::Injured)
	{
		return Data->SurvivorSprintSpeed * Data->SurvivorInjuredSpeedMultiplier;
	}
	return Data->SurvivorSprintSpeed;
}

void USPMovementComponent::HandleStateTransition(ESurvivorState OldState, ESurvivorState NewState)
{
	const bool bHitDowngrade =
		(OldState == ESurvivorState::Healthy && NewState == ESurvivorState::Injured) ||
		(OldState == ESurvivorState::Injured && NewState == ESurvivorState::Downed);

	if (bHitDowngrade)
	{
		StartHitEscapeSprint(OldState);
	}
}

void USPMovementComponent::StartHitEscapeSprint(ESurvivorState PreviousState)
{
	const USurvivorData* Data = GetSurvivorData();
	if (!Data)
	{
		return;
	}

	HitEscapeSprintSpeed = GetSprintSpeedForState(PreviousState);
	HitEscapeSprintRemaining = Data->HitEscapeSprintDuration;
	bHitEscapeSprintActive = true;
}

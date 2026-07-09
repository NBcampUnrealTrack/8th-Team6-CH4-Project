#include "Components/SPMovementComponent.h"

#include "Characters/Survivor/SurvivorCharacter.h"
#include "Inventory/SPInventoryComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Gameplay/Collectibles/SPCollectibleItem.h"
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
	if (MoveComp->IsCrouching())
	{
		MoveComp->MaxWalkSpeedCrouched = FMath::FInterpTo(MoveComp->MaxWalkSpeedCrouched, Target, DeltaTime, MoveSpeedInterpSpeed);
	}
	else
	{
		MoveComp->MaxWalkSpeed = FMath::FInterpTo(MoveComp->MaxWalkSpeed, Target, DeltaTime, MoveSpeedInterpSpeed);
	}
}

void USPMovementComponent::SnapToTargetSpeed()
{
	const ASurvivorCharacter* Survivor = GetSurvivor();
	if (UCharacterMovementComponent* MoveComp = Survivor ? Survivor->GetCharacterMovement() : nullptr)
	{
		const float Target = ComputeTargetMoveSpeed();
		if (MoveComp->IsCrouching())
		{
			MoveComp->MaxWalkSpeedCrouched = Target;
		}
		else
		{
			MoveComp->MaxWalkSpeed = Target;
		}
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
	const float BaseSpeed = bHitEscapeSprintActive ? HitEscapeSprintSpeed : GetBaseWalkSpeed();
	return BaseSpeed * GetCarryMoveSpeedMultiplier();
}

float USPMovementComponent::GetBaseWalkSpeed() const
{
	const ASurvivorCharacter* Survivor = GetSurvivor();
	const USurvivorData* Data = GetSurvivorData();
	if (!Survivor || !Data)
	{
		return 0.f;
	}

	const float WalkSpeed = Survivor->bIsCrouched ? Data->SurvivorCrouchSpeed : Data->SurvivorWalkSpeed;

	switch (Survivor->GetSurvivorState())
	{
	case ESurvivorState::Healthy:
		return WalkSpeed;

	case ESurvivorState::Injured:
		return WalkSpeed * Data->SurvivorInjuredSpeedMultiplier;

	default:
		return 0.f;
	}
}

float USPMovementComponent::GetCarryMoveSpeedMultiplier() const
{
	const ASurvivorCharacter* Survivor = GetSurvivor();
	const USurvivorData* Data = GetSurvivorData();
	if (!Survivor || !Data)
	{
		return 1.f;
	}

	const USPInventoryComponent* Inventory = Survivor->GetInventoryComponent();
	if (!Inventory)
	{
		return 1.f;
	}

	float TotalPenalty = 0.f;
	for (const FInventorySlotEntry& Slot : Inventory->GetInventorySlots())
	{
		if (Slot.ContentType != EInventorySlotContentType::Collectible)
		{
			continue;
		}

		switch (Slot.CollectibleSize)
		{
		case ECollectibleSize::Small:     TotalPenalty += 1.f - Data->CarrySlowSmall;     break;
		case ECollectibleSize::Medium:    TotalPenalty += 1.f - Data->CarrySlowMedium;    break;
		case ECollectibleSize::Large:     TotalPenalty += 1.f - Data->CarrySlowLarge;     break;
		case ECollectibleSize::Hazardous: TotalPenalty += 1.f - Data->CarrySlowHazardous; break;
		default: break;
		}
	}

	return FMath::Max(Data->MinCarrySpeedMultiplier, 1.f - TotalPenalty);
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

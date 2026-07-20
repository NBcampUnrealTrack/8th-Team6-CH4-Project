#include "Components/SPMovementComponent.h"

#include "Characters/Survivor/SurvivorCharacter.h"
#include "Inventory/SPInventoryComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Gameplay/Collectibles/SPCollectibleItem.h"
#include "Net/UnrealNetwork.h"
#include "Systems/Data/SurvivorData.h"
#include "TimerManager.h"

USPMovementComponent::USPMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void USPMovementComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(USPMovementComponent, bWantsToRun);
	DOREPLIFETIME(USPMovementComponent, SpeedPotionPhase);
	DOREPLIFETIME(USPMovementComponent, bDeliveryMovePenaltyActive);
}

void USPMovementComponent::BeginPlay()
{
	Super::BeginPlay();

	SnapToTargetSpeed();
}

bool USPMovementComponent::IsRunning() const
{
	return bWantsToRun || bHitEscapeSprintActive;
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

void USPMovementComponent::SetWantsToRun(bool bNewWantsToRun)
{
	bWantsToRun = bNewWantsToRun;
}

void USPMovementComponent::SetDeliveryMovePenaltyActive(const bool bNewActive)
{
	ASurvivorCharacter* Survivor = GetSurvivor();
	if (!Survivor || !Survivor->HasAuthority() || bDeliveryMovePenaltyActive == bNewActive)
	{
		return;
	}

	bDeliveryMovePenaltyActive = bNewActive;
	SnapToTargetSpeed();
	Survivor->ForceNetUpdate();
}

void USPMovementComponent::OnRep_DeliveryMovePenaltyActive()
{
	SnapToTargetSpeed();
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

float USPMovementComponent::GetTargetMoveSpeed() const
{
	return ComputeTargetMoveSpeed();
}

float USPMovementComponent::ComputeTargetMoveSpeed() const
{
	const float BaseSpeed = bHitEscapeSprintActive ? HitEscapeSprintSpeed : GetBaseWalkSpeed();
	return BaseSpeed * GetCarryMoveSpeedMultiplier() * GetDeliveryMoveSpeedMultiplier() * GetSpeedPotionMultiplier();
}

float USPMovementComponent::GetBaseWalkSpeed() const
{
	const ASurvivorCharacter* Survivor = GetSurvivor();
	const USurvivorData* Data = GetSurvivorData();
	if (!Survivor || !Data)
	{
		return 0.f;
	}

	float BaseSpeed;
	if (Survivor->bIsCrouched)
	{
		BaseSpeed = Data->SurvivorCrouchSpeed;
	}
	else
	{
		BaseSpeed = bWantsToRun ? Data->SurvivorRunSpeed : Data->SurvivorWalkSpeed;
	}

	switch (Survivor->GetSurvivorState())
	{
	case ESurvivorState::Healthy:
		return BaseSpeed;

	case ESurvivorState::Injured:
		return BaseSpeed * Data->SurvivorInjuredSpeedMultiplier;

	case ESurvivorState::Downed:
		return Data->SurvivorDownedCrawlSpeed;

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

float USPMovementComponent::GetDeliveryMoveSpeedMultiplier() const
{
	if (!bDeliveryMovePenaltyActive)
	{
		return 1.f;
	}

	const USurvivorData* Data = GetSurvivorData();
	return Data ? FMath::Clamp(Data->DeliveryMovePenalty, 0.f, 1.f) : 1.f;
}

float USPMovementComponent::GetSpeedPotionMultiplier() const
{
	const ASurvivorCharacter* Survivor = GetSurvivor();
	const USurvivorData* Data = GetSurvivorData();
	if (!Survivor || !Data)
	{
		return 1.f;
	}

	const ESurvivorState State = Survivor->GetSurvivorState();
	if (State != ESurvivorState::Healthy && State != ESurvivorState::Injured)
	{
		return 1.f;
	}

	switch (SpeedPotionPhase)
	{
	case ESpeedPotionPhase::Boost:
		return Data->SpeedBoostMultiplier;
	case ESpeedPotionPhase::Fatigue:
		return Data->SpeedFatigueMultiplier;
	default:
		return 1.f;
	}
}

bool USPMovementComponent::CanActivateSpeedPotion() const
{
	const ASurvivorCharacter* Survivor = GetSurvivor();
	const USurvivorData* Data = GetSurvivorData();
	if (!Survivor || !Data || SpeedPotionPhase != ESpeedPotionPhase::None)
	{
		return false;
	}

	const ESurvivorState State = Survivor->GetSurvivorState();
	return State == ESurvivorState::Healthy || State == ESurvivorState::Injured;
}

bool USPMovementComponent::TryActivateSpeedPotion(const bool bSkipFatigue)
{
	ASurvivorCharacter* Survivor = GetSurvivor();
	const USurvivorData* Data = GetSurvivorData();
	if (!Survivor || !Survivor->HasAuthority() || !Data || !CanActivateSpeedPotion())
	{
		return false;
	}

	bSkipSpeedPotionFatigue = bSkipFatigue;
	SpeedPotionPhase = ESpeedPotionPhase::Boost;
	Survivor->ForceNetUpdate();
	GetWorld()->GetTimerManager().SetTimer(
		SpeedPotionTimer, this, &USPMovementComponent::FinishSpeedPotionBoost, Data->SpeedBoostDuration, false);
	return true;
}

void USPMovementComponent::FinishSpeedPotionBoost()
{
	ASurvivorCharacter* Survivor = GetSurvivor();
	const USurvivorData* Data = GetSurvivorData();
	if (!Survivor || !Survivor->HasAuthority() || !Data)
	{
		return;
	}

	if (bSkipSpeedPotionFatigue)
	{
		SpeedPotionPhase = ESpeedPotionPhase::None;
		bSkipSpeedPotionFatigue = false;
		Survivor->ForceNetUpdate();
		return;
	}

	SpeedPotionPhase = ESpeedPotionPhase::Fatigue;
	Survivor->ForceNetUpdate();
	GetWorld()->GetTimerManager().SetTimer(
		SpeedPotionTimer, this, &USPMovementComponent::FinishSpeedPotionFatigue, Data->SpeedFatigueDuration, false);
}

void USPMovementComponent::FinishSpeedPotionFatigue()
{
	if (ASurvivorCharacter* Survivor = GetSurvivor(); Survivor && Survivor->HasAuthority())
	{
		SpeedPotionPhase = ESpeedPotionPhase::None;
		bSkipSpeedPotionFatigue = false;
		Survivor->ForceNetUpdate();
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

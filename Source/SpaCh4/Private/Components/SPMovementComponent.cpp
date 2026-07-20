#include "Components/SPMovementComponent.h"

#include "Characters/Survivor/SurvivorCharacter.h"
#include "Inventory/SPInventoryComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Gameplay/Collectibles/SPCollectibleItem.h"
#include "Net/UnrealNetwork.h"
#include "Systems/Data/SurvivorData.h"
#include "TimerManager.h"

namespace SPMovementPrediction
{
	class FSavedMove_Survivor final : public FSavedMove_Character
	{
	public:
		typedef FSavedMove_Character Super;

		virtual void Clear() override
		{
			Super::Clear();
			bSavedWantsToRun = false;
		}

		virtual uint8 GetCompressedFlags() const override
		{
			uint8 Flags = Super::GetCompressedFlags();
			if (bSavedWantsToRun)
			{
				Flags |= FLAG_Custom_0;
			}
			return Flags;
		}

		virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const override
		{
			const FSavedMove_Survivor* NewSurvivorMove = static_cast<const FSavedMove_Survivor*>(NewMove.Get());
			if (!NewSurvivorMove || bSavedWantsToRun != NewSurvivorMove->bSavedWantsToRun)
			{
				return false;
			}
			return Super::CanCombineWith(NewMove, InCharacter, MaxDelta);
		}

		virtual void SetMoveFor(
			ACharacter* Character,
			float InDeltaTime,
			const FVector& NewAcceleration,
			FNetworkPredictionData_Client_Character& ClientData) override
		{
			Super::SetMoveFor(Character, InDeltaTime, NewAcceleration, ClientData);
			if (const USPSurvivorCharacterMovementComponent* MoveComp =
				Cast<USPSurvivorCharacterMovementComponent>(Character->GetCharacterMovement()))
			{
				bSavedWantsToRun = MoveComp->WantsToRun();
			}
		}

		virtual void PrepMoveFor(ACharacter* Character) override
		{
			Super::PrepMoveFor(Character);
			if (USPSurvivorCharacterMovementComponent* MoveComp =
				Cast<USPSurvivorCharacterMovementComponent>(Character->GetCharacterMovement()))
			{
				MoveComp->SetWantsToRun(bSavedWantsToRun);
			}
		}

	private:
		uint8 bSavedWantsToRun : 1 = false;
	};

	class FNetworkPredictionData_Client_Survivor final : public FNetworkPredictionData_Client_Character
	{
	public:
		using Super = FNetworkPredictionData_Client_Character;

		explicit FNetworkPredictionData_Client_Survivor(const UCharacterMovementComponent& ClientMovement)
			: Super(ClientMovement)
		{
		}

		virtual FSavedMovePtr AllocateNewMove() override
		{
			return MakeShared<FSavedMove_Survivor>();
		}
	};
}

void USPSurvivorCharacterMovementComponent::SetWantsToRun(bool bNewWantsToRun)
{
	if (bWantsToRun == bNewWantsToRun)
	{
		return;
	}

	bWantsToRun = bNewWantsToRun;
	ApplyWantsToRun();
}

void USPSurvivorCharacterMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);
	bWantsToRun = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
	ApplyWantsToRun();
}

FNetworkPredictionData_Client* USPSurvivorCharacterMovementComponent::GetPredictionData_Client() const
{
	if (!ClientPredictionData)
	{
		USPSurvivorCharacterMovementComponent* MutableThis =
			const_cast<USPSurvivorCharacterMovementComponent*>(this);
		MutableThis->ClientPredictionData =
			new SPMovementPrediction::FNetworkPredictionData_Client_Survivor(*this);
	}
	return ClientPredictionData;
}

void USPSurvivorCharacterMovementComponent::ApplyWantsToRun()
{
	ASurvivorCharacter* Survivor = Cast<ASurvivorCharacter>(GetOwner());
	USPMovementComponent* SurvivorMovementState = Survivor ? Survivor->GetSPMovementComponent() : nullptr;
	if (SurvivorMovementState)
	{
		SurvivorMovementState->SetWantsToRun(bWantsToRun && Survivor->CanMove());
	}
}

USPMovementComponent::USPMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void USPMovementComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(USPMovementComponent, bWantsToRun, COND_SkipOwner);
	DOREPLIFETIME(USPMovementComponent, SpeedPotionPhase);
	DOREPLIFETIME(USPMovementComponent, bDeliveryMovePenaltyActive);
	DOREPLIFETIME(USPMovementComponent, bHitEscapeSprintActive);
	DOREPLIFETIME(USPMovementComponent, HitEscapeSprintSpeed);
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
	if (bWantsToRun == bNewWantsToRun)
	{
		return;
	}

	bWantsToRun = bNewWantsToRun;
	SnapToTargetSpeed();

	if (ASurvivorCharacter* Survivor = GetSurvivor(); Survivor && Survivor->HasAuthority())
	{
		Survivor->ForceNetUpdate();
	}
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

void USPMovementComponent::OnRep_WantsToRun()
{
	SnapToTargetSpeed();
}

void USPMovementComponent::OnRep_SpeedPotionPhase()
{
	SnapToTargetSpeed();
}

void USPMovementComponent::OnRep_HitEscapeSprintState()
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
	const ASurvivorCharacter* Survivor = GetSurvivor();
	if (!Survivor)
	{
		return 0.f;
	}

	const ESurvivorState State = Survivor->GetSurvivorState();
	if (State != ESurvivorState::Healthy && State != ESurvivorState::Injured && State != ESurvivorState::Downed)
	{
		return 0.f;
	}

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
	SnapToTargetSpeed();
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
		SnapToTargetSpeed();
		Survivor->ForceNetUpdate();
		return;
	}

	SpeedPotionPhase = ESpeedPotionPhase::Fatigue;
	SnapToTargetSpeed();
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
		SnapToTargetSpeed();
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

	ASurvivorCharacter* Survivor = GetSurvivor();
	if (bHitDowngrade && Survivor && Survivor->HasAuthority())
	{
		StartHitEscapeSprint(OldState);
	}

	SnapToTargetSpeed();
}

void USPMovementComponent::StartHitEscapeSprint(ESurvivorState PreviousState)
{
	ASurvivorCharacter* Survivor = GetSurvivor();
	const USurvivorData* Data = GetSurvivorData();
	UWorld* World = GetWorld();
	if (!Survivor || !Survivor->HasAuthority() || !Data || !World)
	{
		return;
	}

	HitEscapeSprintSpeed = GetSprintSpeedForState(PreviousState);
	bHitEscapeSprintActive = true;
	SnapToTargetSpeed();
	Survivor->ForceNetUpdate();
	if (Data->HitEscapeSprintDuration <= 0.f)
	{
		FinishHitEscapeSprint();
		return;
	}

	World->GetTimerManager().SetTimer(
		HitEscapeSprintTimer, this, &USPMovementComponent::FinishHitEscapeSprint,
		Data->HitEscapeSprintDuration, false);
}

void USPMovementComponent::FinishHitEscapeSprint()
{
	ASurvivorCharacter* Survivor = GetSurvivor();
	if (!Survivor || !Survivor->HasAuthority())
	{
		return;
	}

	bHitEscapeSprintActive = false;
	HitEscapeSprintSpeed = 0.f;
	SnapToTargetSpeed();
	Survivor->ForceNetUpdate();
}

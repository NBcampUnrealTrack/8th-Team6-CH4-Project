#include "Characters/Survivor/SurvivorCharacter.h"

#include "Components/SPInteractionComponent.h"
#include "Components/SPMovementComponent.h"
#include "Components/SPParkourComponent.h"
#include "Data/SPInputConfigData.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "Gameplay/Collectibles/SPCollectibleItem.h"
#include "Gameplay/Delivery/SPDeliveryStation.h"
#include "Gameplay/Escape/SPEscapeGate.h"
#include "Gameplay/Escape/SPHatch.h"
#include "InputAction.h"
#include "Inventory/SPInventoryComponent.h"
#include "Net/UnrealNetwork.h"
#include "Systems/MatchGameMode.h"
#include "Type/SPGameplayTag.h"
#include "UI/GameHUD.h"

ASurvivorCharacter::ASurvivorCharacter()
{
	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;

	InteractionComponent = CreateDefaultSubobject<USPInteractionComponent>("InteractionComponent");
	MovementComponent = CreateDefaultSubobject<USPMovementComponent>("MovementComponent");
	ParkourComponent = CreateDefaultSubobject<USPParkourComponent>(TEXT("ParkourComponent"));
	InventoryComponent = CreateDefaultSubobject<USPInventoryComponent>(TEXT("InventoryComponent"));

	OwningTag.AddTag(SPGameplayTags::Character::Survivor);

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->bOrientRotationToMovement = true;
		MoveComp->RotationRate = FRotator(0.f, 540.f, 0.f);
		MoveComp->GetNavAgentPropertiesRef().bCanCrouch = true;
	}
}

void ASurvivorCharacter::GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const
{
	TagContainer.AppendTags(OwningTag);
}

void ASurvivorCharacter::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASurvivorCharacter, SurvivorState);
}

void ASurvivorCharacter::Move(const FInputActionValue& Value)
{
	if (IsParkouring())
	{
		return;
	}

	if (InteractionComponent)
	{
		InteractionComponent->NotifyMoveInput();
	}

	Super::Move(Value);
}

void ASurvivorCharacter::Interact()
{
	Super::Interact();

	if (CanInteract() && InteractionComponent)
	{
		InteractionComponent->RequestInteract();
	}
}

void ASurvivorCharacter::JumpOver()
{
	Super::JumpOver();

	if (ParkourComponent)
	{
		ParkourComponent->RequestJumpOver();
	}
}

void ASurvivorCharacter::ToggleCrouch()
{
	if (bIsCrouched)
	{
		UnCrouch();
		return;
	}

	const bool bStateAllowsCrouch = SurvivorState == ESurvivorState::Healthy || SurvivorState == ESurvivorState::Injured;
	if (bStateAllowsCrouch && CanCrouch())
	{
		Crouch();
	}
}

void ASurvivorCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority() && InventoryComponent)
	{
		InventoryComponent->InitializeDefaultLoadout();
	}

	BindInventoryHudRefresh();
	ApplyStateEffects();
}

void ASurvivorCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	EnsureInputConfig();

	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		UInputAction* JumpOverAction = InputConfig ? InputConfig->JumpOverAction.Get() : nullptr;
		if (!JumpOverAction)
		{
			JumpOverAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/InputAction/IA_JumpOver.IA_JumpOver"));
		}

		if (JumpOverAction)
		{
			EnhancedInput->BindAction(JumpOverAction, ETriggerEvent::Started, this, &ASurvivorCharacter::JumpOver);
		}

		UInputAction* CrouchAction = InputConfig ? InputConfig->CrouchAction.Get() : nullptr;
		if (!CrouchAction)
		{
			CrouchAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/InputAction/IA_Crouch.IA_Crouch"));
		}

		if (CrouchAction)
		{
			EnhancedInput->BindAction(CrouchAction, ETriggerEvent::Started, this, &ASurvivorCharacter::ToggleCrouch);
		}
	}
}

void ASurvivorCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	BindInventoryHudRefresh();
	RefreshLocalInventoryHud();
}

void ASurvivorCharacter::BindInventoryHudRefresh()
{
	if (!IsLocallyControlled() || !InventoryComponent)
	{
		return;
	}

	InventoryComponent->OnInventoryChanged.RemoveDynamic(this, &ASurvivorCharacter::HandleInventoryChanged);
	InventoryComponent->OnInventoryChanged.AddDynamic(this, &ASurvivorCharacter::HandleInventoryChanged);
}

void ASurvivorCharacter::HandleInventoryChanged()
{
	RefreshLocalInventoryHud();
}

void ASurvivorCharacter::RefreshLocalInventoryHud() const
{
	if (!IsLocallyControlled())
	{
		return;
	}

	const APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (!PlayerController)
	{
		return;
	}

	if (AGameHUD* GameHUD = Cast<AGameHUD>(PlayerController->GetHUD()))
	{
		GameHUD->RefreshInventoryPanels();
	}
}

bool ASurvivorCharacter::TryAcquireConsumable(const EConsumableItemType ItemType)
{
	return InventoryComponent && InventoryComponent->TryAddConsumable(ItemType);
}

void ASurvivorCharacter::SetSurvivorState(ESurvivorState NewState)
{
	if (!HasAuthority() || SurvivorState == NewState)
	{
		return;
	}

	const ESurvivorState OldState = SurvivorState;
	SurvivorState = NewState;

	if (MovementComponent)
	{
		MovementComponent->HandleStateTransition(OldState, NewState);
	}

	if (InteractionComponent)
	{
		InteractionComponent->CancelInteract();
	}

	ApplyStateEffects();
	NotifyMatchStateChange(NewState);
}

bool ASurvivorCharacter::CanMove() const
{
	if (IsParkouring())
	{
		return false;
	}

	return SurvivorState == ESurvivorState::Healthy || SurvivorState == ESurvivorState::Injured || SurvivorState == ESurvivorState::Downed;
}

bool ASurvivorCharacter::CanInteract() const
{
	return SurvivorState == ESurvivorState::Healthy || SurvivorState == ESurvivorState::Injured;
}

bool ASurvivorCharacter::CanJumpOver() const
{
	return ParkourComponent && ParkourComponent->CanJumpOver();
}

bool ASurvivorCharacter::IsParkouring() const
{
	return ParkourComponent && ParkourComponent->IsParkouring();
}

void ASurvivorCharacter::NotifyParkourEnded()
{
	ApplyStateEffects();
}

void ASurvivorCharacter::OnRep_SurvivorState(ESurvivorState OldState)
{
	if (MovementComponent)
	{
		MovementComponent->HandleStateTransition(OldState, SurvivorState);
	}
	ApplyStateEffects();
}

void ASurvivorCharacter::ApplyStateEffects()
{
	if (AController* Ctrl = GetController())
	{
		Ctrl->ResetIgnoreMoveInput();
		if (!CanMove())
		{
			Ctrl->SetIgnoreMoveInput(true);
		}
	}
}

void ASurvivorCharacter::BeginPickup(ASPCollectibleItem* Item)
{
	if (InteractionComponent)
	{
		InteractionComponent->BeginPickup(Item);
	}
}

void ASurvivorCharacter::BeginDelivery(ASPDeliveryStation* Station)
{
	if (InteractionComponent)
	{
		InteractionComponent->BeginDelivery(Station);
	}
}

void ASurvivorCharacter::BeginEscapeOpen(ASPEscapeGate* Gate)
{
	if (InteractionComponent)
	{
		InteractionComponent->BeginEscapeOpen(Gate);
	}
}

void ASurvivorCharacter::EndEscapeChanneling()
{
	if (InteractionComponent)
	{
		InteractionComponent->EndEscapeChanneling();
	}
}

void ASurvivorCharacter::BeginHatchEscape(ASPHatch* Hatch)
{
	if (InteractionComponent)
	{
		InteractionComponent->BeginHatchEscape(Hatch);
	}
}

void ASurvivorCharacter::CompleteHatchEscape()
{
	if (InteractionComponent)
	{
		InteractionComponent->CompleteHatchEscape();
	}
}

bool ASurvivorCharacter::IsCarrying() const
{
	return InteractionComponent && InteractionComponent->IsCarrying();
}

FGameplayTag ASurvivorCharacter::GetInteractableTag() const
{
	return InteractionComponent ? InteractionComponent->GetInteractableTag() : FGameplayTag();
}

void ASurvivorCharacter::NotifyMatchStateChange(ESurvivorState NewState)
{
	AMatchGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AMatchGameMode>() : nullptr;
	if (!GameMode)
	{
		return;
	}

	if (NewState == ESurvivorState::Escaped)
	{
		// 추후 통합 때 제대로 된 ID를 넘기는 방식으로 구현
		const AController* SurvivorController = GetController();
		const APlayerState* SurvivorPlayerState = SurvivorController ? SurvivorController->PlayerState : nullptr;
		const FString SurvivorName = SurvivorPlayerState ? SurvivorPlayerState->GetPlayerName() : FString();
		GameMode->RegisterSurvivorEscaped(SurvivorName.IsEmpty() ? NAME_None : FName(*SurvivorName));
	}
}

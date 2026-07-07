#include "Characters/Survivor/SurvivorCharacter.h"

#include "Components/SPInteractionComponent.h"
#include "Components/SPMovementComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Inventory/SPInventoryComponent.h"
#include "Net/UnrealNetwork.h"
#include "Systems/MatchGameMode.h"
#include "Type/SPGameplayTag.h"
#include "UI/GameHUD.h"

ASurvivorCharacter::ASurvivorCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;

	InteractionComponent = CreateDefaultSubobject<USPInteractionComponent>("InteractionComponent");
	MovementComponent = CreateDefaultSubobject<USPMovementComponent>("MovementComponent");
	InventoryComponent = CreateDefaultSubobject<USPInventoryComponent>(TEXT("InventoryComponent"));

	OwningTag.AddTag(SPGameplayTags::Character::Survivor);
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

void ASurvivorCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
}

void ASurvivorCharacter::Move(const FInputActionValue& Value)
{
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

	if (!CanJumpOver()) return;

	// TODO: JumpOver 행동 처리
	// 전방으로 'JumpOverTrace' 전용 콜리전 채널 트레이스 -> 창틀/난간과 같이 JumpOver 표면만 식별해야 함
	// 서버에서 목표 지점 구하기 -> 보간 이동 -> Multicast 동기화
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
	if (!HasAuthority() || SurvivorState == NewState) return;

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
	return SurvivorState == ESurvivorState::Healthy || SurvivorState == ESurvivorState::Injured || SurvivorState == ESurvivorState::Downed;
}

bool ASurvivorCharacter::CanInteract() const
{
	return SurvivorState == ESurvivorState::Healthy || SurvivorState == ESurvivorState::Injured;
}

bool ASurvivorCharacter::CanJumpOver() const
{
	// 공중에서는 바로 넘지 못하도록
	if (const UCharacterMovementComponent* CMC = GetCharacterMovement())
	{
		if (CMC->IsFalling())
		{
			return false;
		}
	}

	// TODO: 운반 중일 때도 넘어갈 수 있을지?
	return SurvivorState == ESurvivorState::Healthy || SurvivorState == ESurvivorState::Injured;
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

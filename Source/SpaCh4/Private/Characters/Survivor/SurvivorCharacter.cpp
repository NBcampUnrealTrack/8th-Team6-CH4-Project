#include "Characters/Survivor/SurvivorCharacter.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "Gameplay/Collectibles/SPCollectibleItem.h"
#include "Gameplay/Delivery/SPDeliveryStation.h"
#include "Gameplay/Escape/SPEscapeGate.h"
#include "Interface/SPInteractable.h"
#include "Inventory/SPInventoryComponent.h"
#include "Net/UnrealNetwork.h"
#include "Systems/Data/SurvivorData.h"
#include "Systems/MatchGameState.h"
#include "TimerManager.h"
#include "Type/SPGameplayTag.h"
#include "UI/GameHUD.h"

ASurvivorCharacter::ASurvivorCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

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
	DOREPLIFETIME(ASurvivorCharacter, CarriedItem);
	DOREPLIFETIME(ASurvivorCharacter, bIsInteract);
}

void ASurvivorCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateInteract();
}

void ASurvivorCharacter::Move(const FInputActionValue& Value)
{
	if (bIsInteract && bCancelInteractOnMove)
	{
		Server_CancelInteract();
	}
	
	Super::Move(Value);
}

bool ASurvivorCharacter::Server_CancelInteract_Validate()
{
	return true;
}

void ASurvivorCharacter::Server_CancelInteract_Implementation()
{
	CancelInteract();
}

void ASurvivorCharacter::UpdateInteract()
{
	if (!IsLocallyControlled())
	{
		return;
	}

	AActor* ThisActor = nullptr;
	FGameplayTag Tag;
	
	if (CanInteract())
	{
		FHitResult Hit;
		if (TraceInteractable(Hit) && Hit.GetActor() && Hit.GetActor()->Implements<USPInteractable>()
			&& ISPInteractable::Execute_IsInteractable(Hit.GetActor()))
		{
			ThisActor = Hit.GetActor();
			Tag = ISPInteractable::Execute_GetInteractableTag(ThisActor);
		}
	}

	if (ThisActor != LastActor && !bIsInteract)
	{
		if (LastActor.IsValid())
		{
			ISPInteractable::Execute_SetHighlight(LastActor.Get(), false);
		}
		if (ThisActor)
		{
			ISPInteractable::Execute_SetHighlight(ThisActor, true);
		}
		LastActor = ThisActor;
		InteractableTag = Tag;
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
	
	SurvivorState = NewState;
	CancelInteract();
	ApplyStateEffects();
}

void ASurvivorCharacter::CancelInteract()
{
	if (!bIsInteract)
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(PickupDropTimer);
	bIsInteract = false;
	CurrentPickupItem = nullptr;
	CurrentDeliveryStation = nullptr;

	if (ASPEscapeGate* Gate = CurrentEscapeGate.Get())
	{
		Gate->ClearOpener(this);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("탈출구 상호작용 취소"));
		}
	}
	CurrentEscapeGate = nullptr;

	ApplyStateEffects();
}

bool ASurvivorCharacter::CanMove() const
{
	return SurvivorState == ESurvivorState::Healthy || SurvivorState == ESurvivorState::Injured || SurvivorState == ESurvivorState::Downed;
}

bool ASurvivorCharacter::CanInteract() const
{
	return SurvivorState == ESurvivorState::Healthy || SurvivorState == ESurvivorState::Injured;
}

bool ASurvivorCharacter::TraceInteractable(FHitResult& OutHit) const
{
	if (!GetController())
	{
		return false;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	GetController()->GetPlayerViewPoint(ViewLocation, ViewRotation);

	const FVector Start = ViewLocation;
	const FVector End = Start + ViewRotation.Vector() * InteractReach;

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	if (CarriedItem)
	{
		Params.AddIgnoredActor(CarriedItem);
	}

	const bool bHit = GetWorld()->SweepSingleByChannel(
		OutHit, Start, End, FQuat::Identity, ECC_GameTraceChannel1,
		FCollisionShape::MakeSphere(InteractRadius), Params);

	if (bDrawDebug)
	{
		DrawDebugSphere(GetWorld(), End, InteractRadius, 12, bHit ? FColor::Green : FColor::Red, false, 0.f);
	}

	return bHit;
}

bool ASurvivorCharacter::Server_Interact_Validate()
{
	// 일단 모든 요청이 신뢰가 있다고 가정
	return true;
}

void ASurvivorCharacter::Server_Interact_Implementation()
{
	if (!CanInteract() || bIsInteract)
	{
		return;
	}
	
	FHitResult Hit;
	if (TraceInteractable(Hit) && Hit.GetActor() && Hit.GetActor()->Implements<USPInteractable>()
		&& ISPInteractable::Execute_IsInteractable(Hit.GetActor()))
	{
		ISPInteractable::Execute_Interact(Hit.GetActor(), this);
		return;
	}
	
	if (IsCarrying())
	{
		BeginDrop();
	}
}

void ASurvivorCharacter::Interact()
{
	Super::Interact();
	
	if (CanInteract())
	{
		Server_Interact();
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

void ASurvivorCharacter::OnRep_SurvivorState()
{
	ApplyStateEffects();
}

void ASurvivorCharacter::ApplyStateEffects()
{
	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	if (!MoveComp || !SurvivorData)
	{
		return;
	}

	switch (SurvivorState)
	{
	case ESurvivorState::Healthy:
		MoveComp->MaxWalkSpeed = SurvivorData->SurvivorSprintSpeed;
		break;

	case ESurvivorState::Injured:
		MoveComp->MaxWalkSpeed = SurvivorData->SurvivorSprintSpeed * SurvivorData->SurvivorInjuredSpeedMultiplier;
		break;

	case ESurvivorState::Downed:
		MoveComp->MaxWalkSpeed = DownedWalkSpeed;
		break;

	default:
		// Carried/Caged/Dead/Escaped: 이동 입력 차단
		break;
	}
	
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
	if (!HasAuthority() || bIsInteract || IsCarrying() || !Item || !SurvivorData)
	{
		return;
	}
	
	CurrentPickupItem = Item;
	
	if (bInstantPickup)
	{
		CompletePickup();
		return;
	}

	bIsInteract = true;
	GetWorldTimerManager().SetTimer(
		PickupDropTimer, this, &ASurvivorCharacter::CompletePickup, SurvivorData->PickupDuration, false);
}

void ASurvivorCharacter::CompletePickup()
{
	bIsInteract = false;
	if (!CurrentPickupItem.IsValid())
	{
		return;
	}

	CarriedItem = CurrentPickupItem.Get();
	CurrentPickupItem = nullptr;

	CarriedItem->SetPickupCollisionEnabled(false);
	CarriedItem->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, CarrySocketName);

	if (InventoryComponent)
	{
		InventoryComponent->SetCollectibleFromItem(CarriedItem);
	}
}

void ASurvivorCharacter::BeginDrop()
{
	if (!HasAuthority() || bIsInteract || !IsCarrying() || !SurvivorData)
	{
		return;
	}

	bIsInteract = true;
	GetWorldTimerManager().SetTimer(
		PickupDropTimer, this, &ASurvivorCharacter::CompleteDrop, SurvivorData->DropDuration, false);
}

void ASurvivorCharacter::CompleteDrop()
{
	bIsInteract = false;
	if (!CarriedItem)
	{
		return;
	}

	CarriedItem->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	CarriedItem->SetActorLocation(GetActorLocation());
	CarriedItem->SetPickupCollisionEnabled(true);
	CarriedItem = nullptr;

	if (InventoryComponent)
	{
		InventoryComponent->ClearCollectible();
	}
}

void ASurvivorCharacter::BeginDelivery(ASPDeliveryStation* Station)
{
	if (!HasAuthority() || bIsInteract || !IsCarrying() || !SurvivorData)
	{
		return;
	}
	if (!Station || Station->IsComplete())
	{
		return;
	}

	CurrentDeliveryStation = Station;
	bIsInteract = true;
	
	if (!bCancelInteractOnMove)
	{
		if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
		{
			MoveComp->MaxWalkSpeed *= SurvivorData->DeliveryMovePenalty;
		}
	}

	GetWorldTimerManager().SetTimer(
		PickupDropTimer, this, &ASurvivorCharacter::CompleteDelivery, Station->GetDeliveryDuration(), false);
}

void ASurvivorCharacter::CompleteDelivery()
{
	bIsInteract = false;
	ApplyStateEffects();

	ASPDeliveryStation* Station = CurrentDeliveryStation.Get();
	CurrentDeliveryStation = nullptr;

	if (!Station || !CarriedItem)
	{
		return;
	}

	Station->SubmitValue(CarriedItem->GetValue());

	CarriedItem->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	CarriedItem->Destroy();
	CarriedItem = nullptr;

	if (InventoryComponent)
	{
		InventoryComponent->ClearCollectible();
	}
}

void ASurvivorCharacter::BeginEscapeOpen(ASPEscapeGate* Gate)
{
	if (!HasAuthority() || bIsInteract || !Gate || Gate->IsActivated())
	{
		return;
	}
	
	const AMatchGameState* MatchGameState = GetWorld() ? GetWorld()->GetGameState<AMatchGameState>() : nullptr;
	if (!MatchGameState || !MatchGameState->CanActivateEscapeGates())
	{
		return;
	}

	CurrentEscapeGate = Gate;
	bIsInteract = true;
	Gate->SetOpener(this);
}

void ASurvivorCharacter::EndEscapeChanneling()
{
	bIsInteract = false;
	CurrentEscapeGate = nullptr;
	ApplyStateEffects();
}

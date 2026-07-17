#include "Characters/Survivor/SurvivorCharacter.h"

#include "Components/SPInteractionComponent.h"
#include "Components/SPEscapeLeverComponent.h"
#include "Components/SPPickupAnimComponent.h"
#include "Components/SPHealingAnimComponent.h"
#include "Components/SPMovementComponent.h"
#include "Components/SPParkourComponent.h"
#include "Components/SPScratchMarkComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Data/SPInputConfigData.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "EngineUtils.h"
#include "Gameplay/Cage/Cage.h"
#include "Gameplay/Collectibles/SPCollectibleItem.h"
#include "Gameplay/Items/SPPickupItem.h"
#include "Gameplay/Delivery/SPDeliveryStation.h"
#include "Gameplay/Escape/SPEscapeGate.h"
#include "Gameplay/Escape/SPHatch.h"
#include "InputAction.h"
#include "Inventory/SPInventoryComponent.h"
#include "Net/UnrealNetwork.h"
#include "Player/SPPlayerController.h"
#include "Player/LDPlayerState.h"
#include "Systems/MatchGameMode.h"
#include "Type/SPCollisionChannels.h"
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
	EscapeLeverComponent = CreateDefaultSubobject<USPEscapeLeverComponent>(TEXT("EscapeLeverComponent"));
	PickupAnimComponent = CreateDefaultSubobject<USPPickupAnimComponent>(TEXT("PickupAnimComponent"));
	HealingAnimComponent = CreateDefaultSubobject<USPHealingAnimComponent>(TEXT("HealingAnimComponent"));
	InventoryComponent = CreateDefaultSubobject<USPInventoryComponent>(TEXT("InventoryComponent"));
	ScratchMarkComponent = CreateDefaultSubobject<USPScratchMarkComponent>(TEXT("ScratchMarkComponent"));

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

	if (InteractionComponent && InteractionComponent->IsInteracting())
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

void ASurvivorCharacter::OnRep_Controller()
{
	Super::OnRep_Controller();
	
	BindInventoryHudRefresh();
	RefreshLocalInventoryHud();
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

void ASurvivorCharacter::StartRun()
{
	if (MovementComponent)
	{
		MovementComponent->SetWantsToRun(true);
	}
	Server_SetWantsToRun(true);
}

void ASurvivorCharacter::StopRun()
{
	if (MovementComponent)
	{
		MovementComponent->SetWantsToRun(false);
	}
	Server_SetWantsToRun(false);
}

bool ASurvivorCharacter::Server_SetWantsToRun_Validate(bool bNewWantsToRun)
{
	return true;
}

void ASurvivorCharacter::Server_SetWantsToRun_Implementation(bool bNewWantsToRun)
{
	if (MovementComponent)
	{
		MovementComponent->SetWantsToRun(bNewWantsToRun);
	}
}

void ASurvivorCharacter::EnterCaged(ACage* Cage)
{
	if (!HasAuthority() || !Cage) return;

	CurrentCage = Cage;
	++CagedCount;
	
	UE_LOG(LogTemp, Warning, TEXT("[Cage Log] 생존자 %s 갇힘! 현재 누적: %d"), *GetName(), CagedCount);
	// 케이지 당한 횟수 기록
	if (ALDPlayerState* LDPlayerState = GetController() ? GetController()->GetPlayerState<ALDPlayerState>() : nullptr)
	{
		LDPlayerState->RecordCaged();
	}

	UE_LOG(LogTemp, Warning, TEXT("[Cage Log] 생존자 %s가 케이지에 갇혔습니다. 누적 횟수: %d"), *GetName(), CagedCount);

	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	const FTransform Anchor = Cage->GetPrisonerAnchorTransform();
	Multicast_ApplyCagedPose(Anchor.GetLocation(), Anchor.Rotator());
	
	if (CagedCount >= 3)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cage Log] 3회 누적되어 즉시 사망 처리합니다."));
		SetSurvivorState(ESurvivorState::Dead);
		return;
	}

	Cage->SetOccupied(this);
	SetSurvivorState(ESurvivorState::Caged);

	const float Time = (CagedCount == 1) ? Cage->GetStageOneDuration() : Cage->GetStageTwoDuration();
	GetWorldTimerManager().SetTimer(
	CageTimerHandle, this, &ASurvivorCharacter::OnCageExpired, Time, true);
}

void ASurvivorCharacter::OnCageExpired()
{
	++CagedCount;
	UE_LOG(LogTemp, Warning, TEXT("[Cage Log] 시간 경과! 누적 카운트 증가: %d"), CagedCount);
	
	if (CagedCount >= 3)
	{
		GetWorldTimerManager().ClearTimer(CageTimerHandle);
		SetSurvivorState(ESurvivorState::Dead);
	}
}

void ASurvivorCharacter::ApplyDeathRagdoll()
{
	if (bDeathRagdollApplied)
	{
		return;
	}

	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp)
	{
		UE_LOG(LogTemp, Fatal, TEXT("%s cannot enter death ragdoll because it has no SkeletalMeshComponent."), *GetName());
		return;
	}

	if (!MeshComp->GetPhysicsAsset())
	{
		UE_LOG(LogTemp, Fatal, TEXT("%s cannot enter death ragdoll because its SkeletalMesh has no PhysicsAsset."), *GetName());
		return;
	}

	bDeathRagdollApplied = true;
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	SetActorEnableCollision(true);

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->StopMovementImmediately();
		MoveComp->DisableMovement();
	}

	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	MeshComp->SetCollisionProfileName(TEXT("Ragdoll"));
	MeshComp->SetCollisionObjectType(ECC_PhysicsBody);
	MeshComp->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
	MeshComp->SetCollisionResponseToAllChannels(ECR_Ignore);
	MeshComp->SetCollisionResponseToChannel(SPCollisionChannels::Cage, ECR_Block);
	MeshComp->SetEnableGravity(true);
	MeshComp->SetSimulatePhysics(true);
	MeshComp->SetAllBodiesSimulatePhysics(true);
	MeshComp->SetAllBodiesPhysicsBlendWeight(1.0f, false);
	MeshComp->WakeAllRigidBodies();

	if (GetNetMode() != NM_DedicatedServer && !MeshComp->IsAnySimulatingPhysics())
	{
		UE_LOG(
			LogTemp,
			Fatal,
			TEXT("%s failed to enable death ragdoll physics on its SkeletalMesh bodies."),
			*GetName());
	}
}

void ASurvivorCharacter::Multicast_ApplyCagedPose_Implementation(FVector Location, FRotator Rotation)
{
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->bOrientRotationToMovement = false;
		MoveComp->NetworkSmoothingMode = ENetworkSmoothingMode::Exponential;
	}
	SetActorLocationAndRotation(Location, Rotation);
}

void ASurvivorCharacter::Multicast_RestoreOrientRotation_Implementation()
{
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->bOrientRotationToMovement = true;
	}
}

void ASurvivorCharacter::RescueFromCage(ASurvivorCharacter* Rescuer)
{
	if (!HasAuthority()) return;
	GetWorldTimerManager().ClearTimer(CageTimerHandle);

	Multicast_RestoreOrientRotation();

	if (Rescuer)
	{
		const FVector DropLocation = Rescuer->GetActorLocation() + Rescuer->GetActorRightVector() * RescueDropOffset;
		SetActorLocation(DropLocation);
	}

	SetSurvivorState(ESurvivorState::Injured);
}

void ASurvivorCharacter::BeginRescue(ACage* Cage)
{
	if (InteractionComponent)
	{
		InteractionComponent->BeginRescue(Cage);
	}
}

void ASurvivorCharacter::DebugCageSelf()
{
	Server_DebugCageSelf();
}

void ASurvivorCharacter::Server_DebugCageSelf_Implementation()
{
	if (SurvivorState == ESurvivorState::Caged || SurvivorState == ESurvivorState::Dead)
	{
		return;
	}

	ACage* Nearest = nullptr;
	float NearestDistSq = TNumericLimits<float>::Max();
	const FVector MyLocation = GetActorLocation();
	for (TActorIterator<ACage> It(GetWorld()); It; ++It)
	{
		const float DistSq = FVector::DistSquared(MyLocation, It->GetActorLocation());
		if (DistSq < NearestDistSq)
		{
			NearestDistSq = DistSq;
			Nearest = *It;
		}
	}

	if (Nearest)
	{
		EnterCaged(Nearest);
	}
}

void ASurvivorCharacter::ApplyHit()
{
	if (!HasAuthority()) return;
	switch (SurvivorState)
	{
	case ESurvivorState::Healthy:
		SetSurvivorState(ESurvivorState::Injured);
		break;
	case ESurvivorState::Injured:
		SetSurvivorState(ESurvivorState::Downed);
		break;
	default:
		break;
	}
}

void ASurvivorCharacter::RecoverOneStep()
{
	if (!HasAuthority())
	{
		return;
	}

	switch (SurvivorState)
	{
	case ESurvivorState::Downed:
		SetSurvivorState(ESurvivorState::Injured);
		break;
	case ESurvivorState::Injured:
		SetSurvivorState(ESurvivorState::Healthy);
		break;
	default:
		break;
	}
}

void ASurvivorCharacter::BeginPlay()
{
	Super::BeginPlay();

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
		
		EnhancedInput->BindAction(InputConfig->RunAction, ETriggerEvent::Started, this, &ASurvivorCharacter::StartRun);
		EnhancedInput->BindAction(InputConfig->RunAction, ETriggerEvent::Completed, this, &ASurvivorCharacter::StopRun);

		if (InputConfig)
		{
			if (UInputAction* InteractAction = InputConfig->InteractAction.Get())
			{
				EnhancedInput->BindAction(InteractAction, ETriggerEvent::Completed, this, &ASurvivorCharacter::StopInteract);
				EnhancedInput->BindAction(InteractAction, ETriggerEvent::Canceled, this, &ASurvivorCharacter::StopInteract);
			}

			if (UInputAction* DropAction = InputConfig->DropAction.Get())
			{
				EnhancedInput->BindAction(DropAction, ETriggerEvent::Started, this, &ASurvivorCharacter::DropSelectedItem);
			}

			for (int32 SlotIndex = 0; SlotIndex < InputConfig->SelectSlotActions.Num(); ++SlotIndex)
			{
				if (UInputAction* SlotAction = InputConfig->SelectSlotActions[SlotIndex].Get())
				{
					EnhancedInput->BindAction(SlotAction, ETriggerEvent::Started, this, &ASurvivorCharacter::SelectSlot, SlotIndex);
				}
			}
		}
	}
}

void ASurvivorCharacter::StopInteract()
{
	if (InteractionComponent)
	{
		InteractionComponent->RequestCancelInteract();
	}
}

void ASurvivorCharacter::SelectSlot(int32 Index)
{
	SelectedSlotIndex = Index;
	Server_SelectSlot(Index);
	RefreshLocalInventoryHud();
}

void ASurvivorCharacter::Server_SelectSlot_Implementation(int32 Index)
{
	SelectedSlotIndex = Index;
}

void ASurvivorCharacter::DropSelectedItem()
{
	if (CanInteract() && InteractionComponent)
	{
		InteractionComponent->RequestDrop();
	}
}

void ASurvivorCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (HasAuthority() && InventoryComponent)
	{
		if (const ALDPlayerState* LDPlayerState = GetPlayerState<ALDPlayerState>())
		{
			const FSPPlayerLoadout& Loadout = LDPlayerState->GetPlayerLoadout();
			InventoryComponent->InitializeLoadout(Loadout.SurvivorItem, Loadout.SurvivorPerks);
		}
		else
		{
			InventoryComponent->InitializeDefaultLoadout();
		}
	}

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
	ForceNetUpdate();

	if (SurvivorState == ESurvivorState::Dead)
	{
		ApplyDeathRagdoll();

		if (ASPPlayerController* SPPC = Cast<ASPPlayerController>(GetController()))
		{
			SPPC->EnterSpectatorMode(DeathCamDuration);
		}

		SetLifeSpan(CorpseLifetime);

		if (CurrentCage)
		{
			CurrentCage->HandleSurvivorDeath(this);
		}
	}
	else if (SurvivorState == ESurvivorState::Escaped)
	{
		if (ASPPlayerController* SPPC = Cast<ASPPlayerController>(GetController()))
		{
			SPPC->EnterSpectatorMode(0.f);
		}
	}

	if (MovementComponent)
	{
		MovementComponent->HandleStateTransition(OldState, NewState);
	}

	if (InteractionComponent)
	{
		InteractionComponent->CancelInteract();
		if (NewState == ESurvivorState::Downed)
		{
			InteractionComponent->DropAllItems();
		}
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
	if (IsPullingLever() || IsPlayingPickupAnim() || IsHealing())
	{
		return false;
	}

	return ParkourComponent && ParkourComponent->CanJumpOver();
}

bool ASurvivorCharacter::IsParkouring() const
{
	return ParkourComponent && ParkourComponent->IsParkouring();
}

bool ASurvivorCharacter::IsPullingLever() const
{
	return EscapeLeverComponent && EscapeLeverComponent->IsPullingLever();
}

bool ASurvivorCharacter::IsChannelingLever() const
{
	return EscapeLeverComponent && EscapeLeverComponent->IsChannelingLever();
}

bool ASurvivorCharacter::IsPlayingPickupAnim() const
{
	return PickupAnimComponent && PickupAnimComponent->IsPlayingPickupAnim();
}

void ASurvivorCharacter::NotifyParkourEnded()
{
	ApplyStateEffects();
}

void ASurvivorCharacter::NotifyLeverChannelEnded()
{
	ApplyStateEffects();
}

void ASurvivorCharacter::NotifyPickupAnimEnded()
{
	ApplyStateEffects();
}

bool ASurvivorCharacter::IsHealing() const
{
	return HealingAnimComponent && HealingAnimComponent->IsHealing();
}

bool ASurvivorCharacter::IsChannelingHealing() const
{
	return HealingAnimComponent && HealingAnimComponent->IsChannelingHealing();
}

void ASurvivorCharacter::NotifyHealingAnimEnded()
{
	ApplyStateEffects();
}

void ASurvivorCharacter::OnRep_SurvivorState(ESurvivorState OldState)
{
	if (SurvivorState == ESurvivorState::Dead)
	{
		ApplyDeathRagdoll();
	}

	if (MovementComponent)
	{
		MovementComponent->HandleStateTransition(OldState, SurvivorState);
	}
	ApplyStateEffects();
}

void ASurvivorCharacter::ApplyStateEffects()
{
	if (SurvivorState == ESurvivorState::Carried)
	{
		return; 
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

void ASurvivorCharacter::BeginPickup(ASPPickupItem* Item)
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
		GameMode->RegisterSurvivorEscaped(GetController());
		return;
	}
	if (NewState == ESurvivorState::Dead)
	{
		GameMode->RegisterSurvivorKilled(GetController());
		return;
	}
	// 타 플레이어hud연동을 위해 모든 StateChange를 등록
	GameMode->RegisterSurvivorStateChanged(GetController(), NewState);

}

#include "Components/SPInteractionComponent.h"

#include "Animation/AnimInstance.h"
#include "Characters/Survivor/SurvivorCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/SPPickupAnimComponent.h"
#include "Components/SPEscapeLeverComponent.h"
#include "Components/SPHealingAnimComponent.h"
#include "Components/SPMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "Gameplay/Cage/Cage.h"
#include "Gameplay/Collectibles/SPCollectibleItem.h"
#include "Gameplay/Items/SPPickupItem.h"
#include "Gameplay/Delivery/SPDeliveryStation.h"
#include "Gameplay/Escape/SPEscapeGate.h"
#include "Gameplay/Escape/SPHatch.h"
#include "Interface/SPInteractable.h"
#include "Inventory/SPInventoryComponent.h"
#include "Net/UnrealNetwork.h"
#include "Player/LDPlayerState.h"
#include "Systems/Data/SurvivorData.h"
#include "TimerManager.h"

USPInteractionComponent::USPInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void USPInteractionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(USPInteractionComponent, bIsInteract);
	DOREPLIFETIME_CONDITION(USPInteractionComponent, InteractProgress, COND_OwnerOnly);
}

void USPInteractionComponent::BeginPlay()
{
	Super::BeginPlay();
}

void USPInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdateInteract();

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		InteractProgress = ComputeInteractProgress();
	}
}

ASurvivorCharacter* USPInteractionComponent::GetSurvivor() const
{
	return Cast<ASurvivorCharacter>(GetOwner());
}

const USurvivorData* USPInteractionComponent::GetSurvivorData() const
{
	ASurvivorCharacter* Survivor = GetSurvivor();
	return Survivor ? Survivor->GetSurvivorData() : nullptr;
}

bool USPInteractionComponent::IsCarrying() const
{
	const ASurvivorCharacter* Survivor = GetSurvivor();
	const USPInventoryComponent* Inventory = Survivor ? Survivor->GetInventoryComponent() : nullptr;
	return Inventory && Inventory->HasAnyCollectible();
}

void USPInteractionComponent::RequestInteract()
{
	if (bIsInteract || bInteractionRequestPending || bInteractionCancelPending)
	{
		return;
	}

	bInteractionRequestPending = true;
	bInteractionCancelPending = false;
	Server_Interact();
}

void USPInteractionComponent::RequestDrop()
{
	if (bIsInteract || bInteractionRequestPending || bInteractionCancelPending)
	{
		return;
	}

	bInteractionRequestPending = true;
	bInteractionCancelPending = false;
	Server_Drop();
}

void USPInteractionComponent::RequestCancelInteract()
{
	if (!bIsInteract && !bInteractionRequestPending)
	{
		return;
	}

	bInteractionRequestPending = false;
	bInteractionCancelPending = true;
	Server_CancelInteract();
}

void USPInteractionComponent::NotifyMoveInput()
{
	if (IsInteracting() && bCancelInteractOnMove)
	{
		RequestCancelInteract();
	}
}

bool USPInteractionComponent::Server_CancelInteract_Validate()
{
	return true;
}

void USPInteractionComponent::Server_CancelInteract_Implementation()
{
	CancelInteract();
	Client_ResolveInteractionRequest(false);
}

void USPInteractionComponent::Client_ResolveInteractionRequest_Implementation(bool bServerInteracting)
{
	bInteractionRequestPending = false;
	if (!bServerInteracting)
	{
		bInteractionCancelPending = false;
	}
}

void USPInteractionComponent::OnRep_IsInteracting()
{
	bInteractionRequestPending = false;
	if (!bIsInteract)
	{
		bInteractionCancelPending = false;
	}
}

void USPInteractionComponent::UpdateInteract()
{
	ASurvivorCharacter* Survivor = GetSurvivor();
	if (!Survivor || !Survivor->IsLocallyControlled())
	{
		return;
	}

	AActor* ThisActor = nullptr;
	FGameplayTag Tag;

	if (Survivor->CanInteract())
	{
		FHitResult Hit;
		if (TraceInteractable(Hit) && Hit.GetActor() && Hit.GetActor()->Implements<USPInteractable>()
			&& ISPInteractable::Execute_IsInteractable(Hit.GetActor()))
		{
			ThisActor = Hit.GetActor();
			Tag = ISPInteractable::Execute_GetInteractableTag(ThisActor);
		}
	}

	if (ThisActor != LastActor && !IsInteracting())
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

bool USPInteractionComponent::TraceInteractable(FHitResult& OutHit) const
{
	const ASurvivorCharacter* Survivor = GetSurvivor();
	if (!Survivor || !Survivor->GetController())
	{
		return false;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	Survivor->GetController()->GetPlayerViewPoint(ViewLocation, ViewRotation);

	const FVector Start = Survivor->GetPawnViewLocation();;
	const FVector End = Start + ViewRotation.Vector() * InteractReach;

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Survivor);

	const bool bHit = GetWorld()->SweepSingleByChannel(
		OutHit, Start, End, FQuat::Identity, ECC_GameTraceChannel1,
		FCollisionShape::MakeSphere(InteractRadius), Params);

	if (bDrawDebug)
	{
		DrawDebugSphere(GetWorld(), End, InteractRadius, 12, bHit ? FColor::Green : FColor::Red, false, 0.f);
	}

	return bHit;
}

float USPInteractionComponent::ComputeInteractProgress() const
{
	if (!bIsInteract)
	{
		return 0.f;
	}

	if (const ASPEscapeGate* Gate = CurrentEscapeGate.Get())
	{
		const float Duration = Gate->GetOpenDuration();
		return Duration > 0.f ? FMath::Clamp(Gate->GetOpenProgress() / Duration, 0.f, 1.f) : 0.f;
	}

	if (const ASPHatch* Hatch = CurrentHatch.Get())
	{
		const float Duration = Hatch->GetOpenDuration();
		return Duration > 0.f ? FMath::Clamp(Hatch->GetOpenProgress() / Duration, 0.f, 1.f) : 0.f;
	}

	if (CurrentCage.IsValid())
	{
		FTimerManager& TimerManager = GetWorld()->GetTimerManager();
		const float Rate = TimerManager.GetTimerRate(RescueTimer);
		return Rate > 0.f ? FMath::Clamp(TimerManager.GetTimerElapsed(RescueTimer) / Rate, 0.f, 1.f) : 0.f;
	}

	if (UWorld* World = GetWorld())
	{
		FTimerManager& TimerManager = World->GetTimerManager();
		if (TimerManager.IsTimerActive(HealTimer))
		{
			const float Rate = TimerManager.GetTimerRate(HealTimer);
			if (Rate > 0.f)
			{
				return FMath::Clamp(TimerManager.GetTimerElapsed(HealTimer) / Rate, 0.f, 1.f);
			}
		}

		if (TimerManager.IsTimerActive(SpeedPotionUseTimer))
		{
			const float Rate = TimerManager.GetTimerRate(SpeedPotionUseTimer);
			if (Rate > 0.f)
			{
				return FMath::Clamp(TimerManager.GetTimerElapsed(SpeedPotionUseTimer) / Rate, 0.f, 1.f);
			}
		}

		if (TimerManager.IsTimerActive(PickupDropTimer))
		{
			const float Rate = TimerManager.GetTimerRate(PickupDropTimer);
			if (Rate > 0.f)
			{
				return FMath::Clamp(TimerManager.GetTimerElapsed(PickupDropTimer) / Rate, 0.f, 1.f);
			}
		}
	}

	return 0.f;
}

bool USPInteractionComponent::Server_Interact_Validate()
{
	return true;
}

void USPInteractionComponent::Server_Interact_Implementation()
{
	ASurvivorCharacter* Survivor = GetSurvivor();
	if (!Survivor || !Survivor->CanInteract() || bIsInteract)
	{
		Client_ResolveInteractionRequest(false);
		return;
	}

	FHitResult Hit;
	if (TraceInteractable(Hit) && Hit.GetActor() && Hit.GetActor()->Implements<USPInteractable>()
		&& ISPInteractable::Execute_IsInteractable(Hit.GetActor()))
	{
		FaceInteractTarget(Hit.GetActor());
		ISPInteractable::Execute_Interact(Hit.GetActor(), Survivor);
		Client_ResolveInteractionRequest(bIsInteract);
		return;
	}

	TryUseSelectedConsumable();
	Client_ResolveInteractionRequest(bIsInteract);
}

bool USPInteractionComponent::Server_Drop_Validate()
{
	return true;
}

void USPInteractionComponent::Server_Drop_Implementation()
{
	ASurvivorCharacter* Survivor = GetSurvivor();
	if (!Survivor || !Survivor->CanInteract() || bIsInteract)
	{
		Client_ResolveInteractionRequest(false);
		return;
	}

	BeginDrop();
	Client_ResolveInteractionRequest(bIsInteract);
}

bool USPInteractionComponent::TryUseSelectedConsumable()
{
	ASurvivorCharacter* Survivor = GetSurvivor();
	USPInventoryComponent* Inventory = Survivor ? Survivor->GetInventoryComponent() : nullptr;
	if (!Survivor || !Inventory)
	{
		return false;
	}

	const int32 SlotIndex = Survivor->GetSelectedSlotIndex();
	switch (Inventory->GetConsumableTypeAtSlot(SlotIndex))
	{
	case EConsumableItemType::Medkit:
		return TryBeginSelfHeal();

	case EConsumableItemType::SpeedPotion:
		return TryBeginSpeedPotionUse();

	default:
		return false;
	}
}

bool USPInteractionComponent::TryBeginSelfHeal()
{
	ASurvivorCharacter* Survivor = GetSurvivor();
	const USurvivorData* Data = GetSurvivorData();
	UWorld* World = GetWorld();
	if (!Survivor || !Survivor->HasAuthority() || bIsInteract || !Data || !World || Data->MedkitDuration <= 0.f)
	{
		return false;
	}

	if (Survivor->GetSurvivorState() != ESurvivorState::Injured || !IsSelectedSlotMedkit())
	{
		return false;
	}

	bIsSelfHealing = true;
	bIsInteract = true;
	ActiveConsumableSlotIndex = Survivor->GetSelectedSlotIndex();

	if (USPHealingAnimComponent* HealAnim = Survivor->GetHealingAnimComponent())
	{
		HealAnim->SetAutoCompleteLoop(false);
		HealAnim->BeginHealingChannel();
	}

	World->GetTimerManager().SetTimer(
		HealTimer, this, &USPInteractionComponent::CompleteHeal, Data->MedkitDuration, false);
	return true;
}

bool USPInteractionComponent::TryBeginSpeedPotionUse()
{
	ASurvivorCharacter* Survivor = GetSurvivor();
	USPInventoryComponent* Inventory = Survivor ? Survivor->GetInventoryComponent() : nullptr;
	USPMovementComponent* Movement = Survivor ? Survivor->GetSPMovementComponent() : nullptr;
	const USurvivorData* Data = GetSurvivorData();
	UWorld* World = GetWorld();
	const int32 SlotIndex = Survivor ? Survivor->GetSelectedSlotIndex() : INDEX_NONE;
	if (!Survivor || !Survivor->HasAuthority() || bIsInteract || !Inventory || !Movement || !Data || !World
		|| Data->SpeedPotionUseDuration <= 0.f || !Movement->CanActivateSpeedPotion()
		|| !Inventory->IsSlotConsumable(SlotIndex, EConsumableItemType::SpeedPotion))
	{
		return false;
	}

	bIsUsingSpeedPotion = true;
	bIsInteract = true;
	ActiveConsumableSlotIndex = SlotIndex;
	PlayInteractMontage(SpeedPotionUseMontage.LoadSynchronous());
	World->GetTimerManager().SetTimer(
		SpeedPotionUseTimer, this, &USPInteractionComponent::CompleteSpeedPotionUse,
		Data->SpeedPotionUseDuration, false);
	return true;
}

bool USPInteractionComponent::IsSelectedSlotMedkit() const
{
	const ASurvivorCharacter* Survivor = GetSurvivor();
	const USPInventoryComponent* Inventory = Survivor ? Survivor->GetInventoryComponent() : nullptr;
	return Inventory && Inventory->IsSlotConsumable(Survivor->GetSelectedSlotIndex(), EConsumableItemType::Medkit);
}

bool USPInteractionComponent::CanSelfHeal() const
{
	const ASurvivorCharacter* Survivor = GetSurvivor();
	if (!Survivor || IsInteracting() || LastActor.IsValid())
	{
		return false;
	}

	return Survivor->GetSurvivorState() == ESurvivorState::Injured && IsSelectedSlotMedkit();
}

void USPInteractionComponent::CompleteHeal()
{
	ASurvivorCharacter* Survivor = GetSurvivor();

	bIsInteract = false;
	bIsSelfHealing = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HealTimer);
	}

	if (Survivor)
	{
		USPInventoryComponent* Inventory = Survivor->GetInventoryComponent();
		const bool bConsumed = Inventory
			&& Inventory->ConsumeConsumableAtSlot(ActiveConsumableSlotIndex, EConsumableItemType::Medkit);

		if (USPHealingAnimComponent* HealAnim = Survivor->GetHealingAnimComponent())
		{
			HealAnim->EndHealingChannel(true);
			HealAnim->SetAutoCompleteLoop(true);
		}

		if (bConsumed)
		{
			Survivor->RecoverOneStep();
			if (ALDPlayerState* PlayerState = Survivor->GetController() ? Survivor->GetController()->GetPlayerState<ALDPlayerState>() : nullptr)
			{
				PlayerState->RecordSuccessfulSelfHeal();
			}
		}
	}
	ActiveConsumableSlotIndex = INDEX_NONE;
}

void USPInteractionComponent::CancelHealChannel()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HealTimer);
	}

	if (ASurvivorCharacter* Survivor = GetSurvivor())
	{
		if (USPHealingAnimComponent* HealAnim = Survivor->GetHealingAnimComponent())
		{
			HealAnim->CancelHealingChannel();
			HealAnim->SetAutoCompleteLoop(true);
		}
	}

	bIsSelfHealing = false;
	ActiveConsumableSlotIndex = INDEX_NONE;
}

void USPInteractionComponent::CompleteSpeedPotionUse()
{
	ASurvivorCharacter* Survivor = GetSurvivor();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SpeedPotionUseTimer);
	}

	bIsInteract = false;
	bIsUsingSpeedPotion = false;
	StopInteractMontage();

	USPInventoryComponent* Inventory = Survivor ? Survivor->GetInventoryComponent() : nullptr;
	USPMovementComponent* Movement = Survivor ? Survivor->GetSPMovementComponent() : nullptr;
	if (Survivor && Survivor->HasAuthority() && Inventory && Movement
		&& Inventory->IsSlotConsumable(ActiveConsumableSlotIndex, EConsumableItemType::SpeedPotion)
		&& Movement->CanActivateSpeedPotion())
	{
		const bool bSkipFatigue = Inventory->HasPerk(EPerkType::LightWheels);
		if (Inventory->ConsumeConsumableAtSlot(ActiveConsumableSlotIndex, EConsumableItemType::SpeedPotion))
		{
			Movement->TryActivateSpeedPotion(bSkipFatigue);
		}
	}

	ActiveConsumableSlotIndex = INDEX_NONE;
}

void USPInteractionComponent::CancelSpeedPotionChannel()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SpeedPotionUseTimer);
	}

	bIsUsingSpeedPotion = false;
	ActiveConsumableSlotIndex = INDEX_NONE;
}

void USPInteractionComponent::FaceInteractTarget(AActor* Target)
{
	ASurvivorCharacter* Survivor = GetSurvivor();
	if (!Survivor || !Target)
	{
		return;
	}

	FVector FocusLocation = Target->GetActorLocation();
	if (Target->Implements<USPInteractable>())
	{
		if (const USceneComponent* FocusComponent = ISPInteractable::Execute_GetInteractFocusComponent(Target))
		{
			FocusLocation = FocusComponent->GetComponentLocation();
		}
	}

	const FVector ToTarget = FocusLocation - Survivor->GetActorLocation();
	if (ToTarget.SizeSquared2D() < KINDA_SMALL_NUMBER)
	{
		return;
	}
	Multicast_FaceInteractTarget(ToTarget.Rotation().Yaw);
}

void USPInteractionComponent::Multicast_FaceInteractTarget_Implementation(float TargetYaw)
{
	if (ASurvivorCharacter* Survivor = GetSurvivor())
	{
		Survivor->SetActorRotation(FRotator(0.f, TargetYaw, 0.f));
	}
}

void USPInteractionComponent::PlayInteractMontage(UAnimMontage* Montage)
{
	if (!Montage)
	{
		return;
	}
	Multicast_PlayInteractMontage(Montage);
}

void USPInteractionComponent::StopInteractMontage()
{
	Multicast_StopInteractMontage();
}

void USPInteractionComponent::Multicast_PlayInteractMontage_Implementation(UAnimMontage* Montage)
{
	const ASurvivorCharacter* Survivor = GetSurvivor();
	USkeletalMeshComponent* Mesh = Survivor ? Survivor->GetMesh() : nullptr;
	if (UAnimInstance* AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr)
	{
		AnimInstance->Montage_Play(Montage);
	}
}

void USPInteractionComponent::Multicast_StopInteractMontage_Implementation()
{
	const ASurvivorCharacter* Survivor = GetSurvivor();
	USkeletalMeshComponent* Mesh = Survivor ? Survivor->GetMesh() : nullptr;
	if (UAnimInstance* AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr)
	{
		AnimInstance->Montage_Stop(0.2f);
	}
}

void USPInteractionComponent::CancelInteract()
{
	if (!bIsInteract)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PickupDropTimer);
		World->GetTimerManager().ClearTimer(RescueTimer);
	}

	ASurvivorCharacter* Survivor = GetSurvivor();
	const bool bWasPickup = CurrentPickupItem.IsValid();
	const bool bWasDelivery = CurrentDeliveryStation.IsValid();
	if (ASPPickupItem* PickupItem = CurrentPickupItem.Get())
	{
		PickupItem->ReleaseReservation(Survivor);
	}

	bIsInteract = false;
	CurrentPickupItem = nullptr;
	CurrentDeliveryStation = nullptr;
	StopInteractMontage();

	if (bWasDelivery && Survivor)
	{
		if (USPMovementComponent* Movement = Survivor->GetSPMovementComponent())
		{
			Movement->SetDeliveryMovePenaltyActive(false);
		}
	}

	if (bIsSelfHealing)
	{
		CancelHealChannel();
	}

	if (bIsUsingSpeedPotion)
	{
		CancelSpeedPotionChannel();
	}

	if (bWasPickup && Survivor && Survivor->GetPickupAnimComponent())
	{
		Survivor->GetPickupAnimComponent()->CancelPickupAnim();
	}

	if (ASPEscapeGate* Gate = CurrentEscapeGate.Get())
	{
		if (Survivor && Survivor->GetEscapeLeverComponent())
		{
			Survivor->GetEscapeLeverComponent()->CancelLeverChannel();
		}
		Gate->ClearOpener(Survivor);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("탈출구 상호작용 취소"));
		}
	}
	CurrentEscapeGate = nullptr;

	if (ASPHatch* Hatch = CurrentHatch.Get())
	{
		Hatch->CancelOpening(Survivor);
	}
	CurrentHatch = nullptr;

	CurrentCage = nullptr;
}

void USPInteractionComponent::BeginPickup(ASPPickupItem* Item)
{
	ASurvivorCharacter* Survivor = GetSurvivor();
	const USurvivorData* Data = GetSurvivorData();
	if (!Survivor || !Survivor->HasAuthority() || bIsInteract || !Item || !Data)
	{
		return;
	}

	const USPInventoryComponent* Inventory = Survivor->GetInventoryComponent();
	if (!Inventory || !Inventory->CanStoreWorldItem(Item))
	{
		return;
	}
	if (!Item->TryReserve(Survivor))
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

	if (USPPickupAnimComponent* PickupAnim = Survivor->GetPickupAnimComponent())
	{
		PickupAnim->BeginPickupAnim();
	}
	else
	{
		PlayInteractMontage(PickupMontage.LoadSynchronous());
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			PickupDropTimer, this, &USPInteractionComponent::CompletePickup, Data->PickupDuration, false);
	}
}

void USPInteractionComponent::CompletePickup()
{
	if (ASurvivorCharacter* Survivor = GetSurvivor())
	{
		if (USPPickupAnimComponent* PickupAnim = Survivor->GetPickupAnimComponent())
		{
			PickupAnim->EndPickupAnim();
		}
	}

	bIsInteract = false;
	StopInteractMontage();
	if (!CurrentPickupItem.IsValid())
	{
		return;
	}

	ASPPickupItem* Item = CurrentPickupItem.Get();
	CurrentPickupItem = nullptr;

	ASurvivorCharacter* Survivor = GetSurvivor();
	USPInventoryComponent* Inv = Survivor ? Survivor->GetInventoryComponent() : nullptr;
	if (!Inv || !Inv->CanStoreWorldItem(Item))
	{
		Item->ReleaseReservation(Survivor);
		return;
	}

	if (!Inv->TryStoreWorldItem(Item))
	{
		Item->ReleaseReservation(Survivor);
	}
}

void USPInteractionComponent::BeginDrop()
{
	ASurvivorCharacter* Survivor = GetSurvivor();
	const USurvivorData* Data = GetSurvivorData();
	if (!Survivor || !Survivor->HasAuthority() || bIsInteract || !Data)
	{
		return;
	}

	USPInventoryComponent* Inv = Survivor->GetInventoryComponent();
	if (!Inv || !Inv->IsSlotOccupied(Survivor->GetSelectedSlotIndex()))
	{
		return;
	}

	bIsInteract = true;
	PlayInteractMontage(DropMontage.LoadSynchronous());
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			PickupDropTimer, this, &USPInteractionComponent::CompleteDrop, Data->DropDuration, false);
	}
}

void USPInteractionComponent::CompleteDrop()
{
	bIsInteract = false;
	StopInteractMontage();

	const ASurvivorCharacter* Survivor = GetSurvivor();
	USPInventoryComponent* Inv = Survivor ? Survivor->GetInventoryComponent() : nullptr;
	if (!Inv)
	{
		return;
	}

	ASPPickupItem* Item = nullptr;
	if (!Inv->DropSlot(Survivor->GetSelectedSlotIndex(), Item))
	{
		return;
	}

	if (Item)
	{
		Item->SetStored(false, ResolveGroundedDropLocation(Survivor, Item));
	}
}

void USPInteractionComponent::DropAllItems()
{
	ASurvivorCharacter* Survivor = GetSurvivor();
	USPInventoryComponent* Inventory = Survivor ? Survivor->GetInventoryComponent() : nullptr;
	if (!Survivor || !Survivor->HasAuthority() || !Inventory)
	{
		return;
	}

	int32 DroppedItemCount = 0;
	float LateralOffset = 0.f;
	float PreviousHalfWidth = 0.f;
	for (int32 SlotIndex = 0; SlotIndex < USPInventoryComponent::InventorySlotCount; ++SlotIndex)
	{
		while (Inventory->IsSlotOccupied(SlotIndex))
		{
			ASPPickupItem* Item = nullptr;
			if (!Inventory->DropSlot(SlotIndex, Item) || !Item)
			{
				break;
			}

			FVector DropLocation = ResolveGroundedDropLocation(Survivor, Item);
			FVector BoundsOrigin;
			FVector BoxExtent;
			Item->GetActorBounds(false, BoundsOrigin, BoxExtent);
			if (DroppedItemCount > 0)
			{
				LateralOffset += PreviousHalfWidth + BoxExtent.Y;
			}
			DropLocation += Survivor->GetActorRightVector() * LateralOffset;
			Item->SetStored(false, DropLocation);
			PreviousHalfWidth = BoxExtent.Y;
			++DroppedItemCount;
		}
	}
}

FVector USPInteractionComponent::ResolveGroundedDropLocation(const ASurvivorCharacter* Survivor, ASPPickupItem* Item) const
{
	const FVector Origin = Survivor->GetActorLocation();
	const FVector TraceStart = Origin + Survivor->GetActorForwardVector() * DropForwardOffset;
	const FVector TraceEnd = TraceStart - FVector(0.f, 0.f, DropTraceDistance);

	float GroundZ = Origin.Z;
	if (const UCapsuleComponent* Capsule = Survivor->GetCapsuleComponent())
	{
		GroundZ = Origin.Z - Capsule->GetScaledCapsuleHalfHeight();
	}

	if (const UWorld* World = GetWorld())
	{
		FCollisionQueryParams Params(SCENE_QUERY_STAT(CollectibleDrop), false);
		Params.AddIgnoredActor(Survivor);
		Params.AddIgnoredActor(Item);

		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, Params))
		{
			GroundZ = Hit.ImpactPoint.Z;
		}
	}

	FVector BoundsOrigin;
	FVector BoxExtent;
	Item->GetActorBounds(false, BoundsOrigin, BoxExtent);
	const float PivotToBottom = Item->GetActorLocation().Z - (BoundsOrigin.Z - BoxExtent.Z);

	return FVector(TraceStart.X, TraceStart.Y, GroundZ + PivotToBottom);
}

void USPInteractionComponent::BeginDelivery(ASPDeliveryStation* Station)
{
	ASurvivorCharacter* Survivor = GetSurvivor();
	const USurvivorData* Data = GetSurvivorData();
	if (!Survivor || !Survivor->HasAuthority() || bIsInteract || !Data)
	{
		return;
	}

	USPInventoryComponent* Inv = Survivor->GetInventoryComponent();
	if (!Inv || !Inv->IsSlotCollectible(Survivor->GetSelectedSlotIndex()))
	{
		return;
	}
	if (!Station || Station->IsComplete())
	{
		return;
	}

	CurrentDeliveryStation = Station;
	bIsInteract = true;
	PlayInteractMontage(DeliveryMontage.LoadSynchronous());

	if (!bCancelInteractOnMove)
	{
		if (USPMovementComponent* Movement = Survivor->GetSPMovementComponent())
		{
			Movement->SetDeliveryMovePenaltyActive(true);
		}
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			PickupDropTimer, this, &USPInteractionComponent::CompleteDelivery, Station->GetDeliveryDuration(), false);
	}
}

void USPInteractionComponent::CompleteDelivery()
{
	bIsInteract = false;
	StopInteractMontage();

	ASPDeliveryStation* Station = CurrentDeliveryStation.Get();
	CurrentDeliveryStation = nullptr;

	ASurvivorCharacter* Survivor = GetSurvivor();
	if (Survivor)
	{
		if (USPMovementComponent* Movement = Survivor->GetSPMovementComponent())
		{
			Movement->SetDeliveryMovePenaltyActive(false);
		}
	}

	USPInventoryComponent* Inv = Survivor ? Survivor->GetInventoryComponent() : nullptr;
	if (!Station || !Inv)
	{
		return;
	}

	int32 Value = 0;
	ASPCollectibleItem* Item = nullptr;
	if (!Inv->DeliverSlot(Survivor->GetSelectedSlotIndex(), Value, Item))
	{
		return;
	}

	if (Value > 0)
	{
		const int32 AcceptedValue = Station->SubmitValue(Value);
		if (AcceptedValue > 0)
		{
			if (ALDPlayerState* PlayerState = Survivor->GetController() ? Survivor->GetController()->GetPlayerState<ALDPlayerState>() : nullptr)
			{
				PlayerState->RecordDelivery(AcceptedValue);
			}
		}
	}

	if (Item)
	{
		Item->Destroy();
	}
}

void USPInteractionComponent::BeginEscapeOpen(ASPEscapeGate* Gate)
{
	ASurvivorCharacter* Survivor = GetSurvivor();
	if (!Survivor || !Survivor->HasAuthority() || bIsInteract || !Gate || Gate->IsActivated())
	{
		return;
	}

	if (!Gate->CanBeOpened())
	{
		return;
	}

	CurrentEscapeGate = Gate;
	bIsInteract = true;
	Gate->SetOpener(Survivor);

	if (USPEscapeLeverComponent* LeverComponent = Survivor->GetEscapeLeverComponent())
	{
		LeverComponent->BeginLeverChannel(Gate);
	}
}

void USPInteractionComponent::EndEscapeChanneling()
{
	if (ASurvivorCharacter* Survivor = GetSurvivor())
	{
		if (USPEscapeLeverComponent* LeverComponent = Survivor->GetEscapeLeverComponent())
		{
			LeverComponent->EndLeverChannel(true);
		}
	}

	bIsInteract = false;
	CurrentEscapeGate = nullptr;
	StopInteractMontage();
}

void USPInteractionComponent::BeginHatchOpen(ASPHatch* Hatch)
{
	ASurvivorCharacter* Survivor = GetSurvivor();
	if (!Survivor || !Survivor->HasAuthority() || bIsInteract || !Survivor->CanInteract() || !Hatch)
	{
		return;
	}
	if (!Hatch->IsInteractable_Implementation())
	{
		return;
	}
	if (!Hatch->TryBeginOpening(Survivor))
	{
		return;
	}

	CurrentHatch = Hatch;
	bIsInteract = true;
	PlayInteractMontage(HatchEscapeMontage.LoadSynchronous());
}

void USPInteractionComponent::CompleteHatchOpen()
{
	bIsInteract = false;
	CurrentHatch = nullptr;
	StopInteractMontage();
}

void USPInteractionComponent::BeginRescue(ACage* Cage)
{
	ASurvivorCharacter* Survivor = GetSurvivor();
	if (!Survivor || !Survivor->HasAuthority() || bIsInteract || !Survivor->CanInteract() || !Cage)
	{
		return;
	}

	ASurvivorCharacter* Victim = Cage->GetTrappedSurvivor();
	if (!Victim || Victim->GetSurvivorState() != ESurvivorState::Caged)
	{
		return;
	}

	CurrentCage = Cage;
	bIsInteract = true;
	PlayInteractMontage(RescueMontage.LoadSynchronous());

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			RescueTimer, this, &USPInteractionComponent::CompleteRescue, Cage->GetRescueDuration(), false);
	}
}

void USPInteractionComponent::CompleteRescue()
{
	bIsInteract = false;
	StopInteractMontage();

	ACage* Cage = CurrentCage.Get();
	CurrentCage = nullptr;
	if (!Cage)
	{
		return;
	}

	ASurvivorCharacter* Rescuer = GetSurvivor();
	ASurvivorCharacter* Victim = Cage->GetTrappedSurvivor();
	if (IsValid(Rescuer) && IsValid(Victim) && Victim->GetSurvivorState() == ESurvivorState::Caged)
	{
		Victim->RescueFromCage(Rescuer);
		if (ALDPlayerState* PlayerState = Rescuer->GetController()
			? Rescuer->GetController()->GetPlayerState<ALDPlayerState>()
			: nullptr)
		{
			PlayerState->RecordCageRescue();
		}
	}
	Cage->SetCageStatus(ECageStatus::Empty);
}

#include "Components/SPInteractionComponent.h"

#include "Animation/AnimInstance.h"
#include "Characters/Survivor/SurvivorCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/SPPickupAnimComponent.h"
#include "Components/SPEscapeLeverComponent.h"
#include "Components/SPHealingAnimComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Gameplay/Collectibles/SPCollectibleItem.h"
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
	const ASurvivorCharacter* Survivor = GetSurvivor();
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
	Server_Interact();
}

void USPInteractionComponent::NotifyMoveInput()
{
	if (bIsInteract && bCancelInteractOnMove)
	{
		Server_CancelInteract();
	}
}

bool USPInteractionComponent::Server_CancelInteract_Validate()
{
	return true;
}

void USPInteractionComponent::Server_CancelInteract_Implementation()
{
	CancelInteract();
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
		const float Duration = Hatch->GetEscapeDuration();
		return Duration > 0.f ? FMath::Clamp(Hatch->GetEscapeProgress() / Duration, 0.f, 1.f) : 0.f;
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
		return;
	}

	FHitResult Hit;
	if (TraceInteractable(Hit) && Hit.GetActor() && Hit.GetActor()->Implements<USPInteractable>()
		&& ISPInteractable::Execute_IsInteractable(Hit.GetActor()))
	{
		FaceInteractTarget(Hit.GetActor());
		ISPInteractable::Execute_Interact(Hit.GetActor(), Survivor);
		return;
	}

	TryBeginSelfHeal();
}

void USPInteractionComponent::TryBeginSelfHeal()
{
	ASurvivorCharacter* Survivor = GetSurvivor();
	if (!Survivor || !Survivor->HasAuthority() || bIsInteract)
	{
		return;
	}

	if (Survivor->GetSurvivorState() != ESurvivorState::Injured || !IsSelectedSlotMedkit())
	{
		return;
	}

	bIsSelfHealing = true;
	bIsInteract = true;

	if (USPHealingAnimComponent* HealAnim = Survivor->GetHealingAnimComponent())
	{
		HealAnim->SetAutoCompleteLoop(false);
		HealAnim->BeginHealingChannel();
	}

	const USurvivorData* Data = GetSurvivorData();
	const float Duration = Data ? Data->MedkitDuration : 3.f;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			HealTimer, this, &USPInteractionComponent::CompleteHeal, Duration, false);
	}
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
	if (!Survivor || bIsInteract || LastActor.IsValid())
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
		if (USPInventoryComponent* Inventory = Survivor->GetInventoryComponent())
		{
			Inventory->RemoveConsumable(EConsumableItemType::Medkit);
		}

		if (USPHealingAnimComponent* HealAnim = Survivor->GetHealingAnimComponent())
		{
			HealAnim->EndHealingChannel(true);
			HealAnim->SetAutoCompleteLoop(true);
		}

		Survivor->RecoverOneStep();
		if (ALDPlayerState* PlayerState = Survivor->GetController() ? Survivor->GetController()->GetPlayerState<ALDPlayerState>() : nullptr)
		{
			PlayerState->RecordSuccessfulSelfHeal();
		}
	}
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
	}

	ASurvivorCharacter* Survivor = GetSurvivor();
	const bool bWasPickup = CurrentPickupItem.IsValid();

	bIsInteract = false;
	CurrentPickupItem = nullptr;
	CurrentDeliveryStation = nullptr;
	StopInteractMontage();

	if (bIsSelfHealing)
	{
		CancelHealChannel();
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
		Hatch->ClearEscaper(Survivor);
	}
	CurrentHatch = nullptr;
}

void USPInteractionComponent::BeginPickup(ASPCollectibleItem* Item)
{
	ASurvivorCharacter* Survivor = GetSurvivor();
	const USurvivorData* Data = GetSurvivorData();
	if (!Survivor || !Survivor->HasAuthority() || bIsInteract || !Item || !Data)
	{
		return;
	}

	const USPInventoryComponent* Inventory = Survivor->GetInventoryComponent();
	if (!Inventory || !Inventory->HasFreeSlot())
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

	ASPCollectibleItem* Item = CurrentPickupItem.Get();
	CurrentPickupItem = nullptr;

	const ASurvivorCharacter* Survivor = GetSurvivor();
	USPInventoryComponent* Inv = Survivor ? Survivor->GetInventoryComponent() : nullptr;
	if (!Inv || !Inv->HasFreeSlot())
	{
		return;
	}

	Inv->SetCollectibleFromItem(Item);
	Item->Multicast_SetStored(true, FVector::ZeroVector);
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

	ASPCollectibleItem* Item = nullptr;
	if (!Inv->DropSlot(Survivor->GetSelectedSlotIndex(), Item))
	{
		return;
	}

	if (Item)
	{
		Item->Multicast_SetStored(false, ResolveGroundedDropLocation(Survivor, Item));
	}
}

FVector USPInteractionComponent::ResolveGroundedDropLocation(const ASurvivorCharacter* Survivor, ASPCollectibleItem* Item) const
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
		if (UCharacterMovementComponent* MoveComp = Survivor->GetCharacterMovement())
		{
			MoveComp->MaxWalkSpeed *= Data->DeliveryMovePenalty;
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

	const ASurvivorCharacter* Survivor = GetSurvivor();
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

void USPInteractionComponent::BeginHatchEscape(ASPHatch* Hatch)
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

	CurrentHatch = Hatch;
	bIsInteract = true;
	PlayInteractMontage(HatchEscapeMontage.LoadSynchronous());
	Hatch->SetEscaper(Survivor);
}

void USPInteractionComponent::CompleteHatchEscape()
{
	bIsInteract = false;
	CurrentHatch = nullptr;
	StopInteractMontage();
	if (ASurvivorCharacter* Survivor = GetSurvivor())
	{
		if (ALDPlayerState* PlayerState = Survivor->GetController() ? Survivor->GetController()->GetPlayerState<ALDPlayerState>() : nullptr)
		{
			PlayerState->RecordEscaped(ESurvivorEscapeMethod::Hatch);
		}
		Survivor->SetSurvivorState(ESurvivorState::Escaped);
	}
}

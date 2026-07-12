#include "Components/SPInteractionComponent.h"

#include "Animation/AnimInstance.h"
#include "Characters/Survivor/SurvivorCharacter.h"
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
#include "Systems/Data/SurvivorData.h"
#include "Systems/MatchGameState.h"
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
}

void USPInteractionComponent::BeginPlay()
{
	Super::BeginPlay();
}

void USPInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdateInteract();
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

	BeginDrop();
}

void USPInteractionComponent::FaceInteractTarget(const AActor* Target)
{
	ASurvivorCharacter* Survivor = GetSurvivor();
	if (!Survivor || !Target)
	{
		return;
	}

	const FVector ToTarget = Target->GetActorLocation() - Survivor->GetActorLocation();
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
	bIsInteract = false;
	CurrentPickupItem = nullptr;
	CurrentDeliveryStation = nullptr;
	StopInteractMontage();

	ASurvivorCharacter* Survivor = GetSurvivor();
	if (ASPEscapeGate* Gate = CurrentEscapeGate.Get())
	{
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
	PlayInteractMontage(Data->PickupMontage);
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			PickupDropTimer, this, &USPInteractionComponent::CompletePickup, Data->PickupDuration, false);
	}
}

void USPInteractionComponent::CompletePickup()
{
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
	PlayInteractMontage(Data->DropMontage);
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
		Item->Multicast_SetStored(false, Survivor->GetActorLocation());
	}
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
	PlayInteractMontage(Data->DeliveryMontage);

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
		Station->SubmitValue(Value);
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

	const AMatchGameState* MatchGameState = GetWorld() ? GetWorld()->GetGameState<AMatchGameState>() : nullptr;
	if (!MatchGameState || !MatchGameState->CanActivateEscapeGates())
	{
		return;
	}

	CurrentEscapeGate = Gate;
	bIsInteract = true;
	if (const USurvivorData* Data = GetSurvivorData())
	{
		PlayInteractMontage(Data->EscapeLeverMontage);
	}
	Gate->SetOpener(Survivor);
}

void USPInteractionComponent::EndEscapeChanneling()
{
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
	if (const USurvivorData* Data = GetSurvivorData())
	{
		PlayInteractMontage(Data->HatchEscapeMontage);
	}
	Hatch->SetEscaper(Survivor);
}

void USPInteractionComponent::CompleteHatchEscape()
{
	bIsInteract = false;
	CurrentHatch = nullptr;
	StopInteractMontage();
	if (ASurvivorCharacter* Survivor = GetSurvivor())
	{
		Survivor->SetSurvivorState(ESurvivorState::Escaped);
	}
}

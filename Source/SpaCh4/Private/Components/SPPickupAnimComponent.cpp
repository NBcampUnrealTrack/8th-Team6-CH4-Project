#include "Components/SPPickupAnimComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Characters/Survivor/SurvivorCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

namespace SPPickupAnim
{
	static constexpr TCHAR PickupMontagePath[] =
		TEXT("/Game/Assets/Survivor_Locomotion/Pickup/A_ItemPickup_fromIdleCrouch_90_LH_25cm_Montage.A_ItemPickup_fromIdleCrouch_90_LH_25cm_Montage");
}

USPPickupAnimComponent::USPPickupAnimComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	static ConstructorHelpers::FObjectFinder<UAnimMontage> PickupFinder(SPPickupAnim::PickupMontagePath);
	if (PickupFinder.Succeeded())
	{
		PickupMontage = PickupFinder.Object;
	}
}

void USPPickupAnimComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(USPPickupAnimComponent, bIsPlayingPickupAnim);
}

void USPPickupAnimComponent::EnsureMontageLoaded()
{
	if (!PickupMontage)
	{
		PickupMontage = LoadObject<UAnimMontage>(nullptr, SPPickupAnim::PickupMontagePath);
	}
}

ASurvivorCharacter* USPPickupAnimComponent::GetSurvivor() const
{
	return Cast<ASurvivorCharacter>(GetOwner());
}

void USPPickupAnimComponent::BeginPickupAnim()
{
	if (bIsPlayingPickupAnim)
	{
		return;
	}

	if (GetOwner()->HasAuthority())
	{
		Multicast_BeginPickupAnim();
	}
}

void USPPickupAnimComponent::EndPickupAnim()
{
	ForceEndPickupAnim();
}

void USPPickupAnimComponent::CancelPickupAnim()
{
	ForceEndPickupAnim();
}

void USPPickupAnimComponent::ForceEndPickupAnim()
{
	if (!bIsPlayingPickupAnim)
	{
		return;
	}

	if (GetOwner()->HasAuthority())
	{
		Multicast_ForceEndPickupAnim();
	}
}

void USPPickupAnimComponent::Multicast_BeginPickupAnim_Implementation()
{
	if (bIsPlayingPickupAnim)
	{
		return;
	}

	EnsureMontageLoaded();
	bIsPlayingPickupAnim = true;
	EnterPickupState();
	PlayPickupMontage();
}

void USPPickupAnimComponent::Multicast_EndPickupAnim_Implementation()
{
	ExitPickupState();
}

void USPPickupAnimComponent::Multicast_CancelPickupAnim_Implementation()
{
	ExitPickupState();
}

void USPPickupAnimComponent::Multicast_ForceEndPickupAnim_Implementation()
{
	ExitPickupState();
}

void USPPickupAnimComponent::EnterPickupState()
{
	ASurvivorCharacter* Survivor = GetSurvivor();
	if (!Survivor)
	{
		return;
	}

}

void USPPickupAnimComponent::ExitPickupState()
{
	StopPickupMontage();

	const bool bWasPlaying = bIsPlayingPickupAnim;
	bIsPlayingPickupAnim = false;
	ActivePickupMontage = nullptr;

	if (!bWasPlaying)
	{
		return;
	}

	if (ASurvivorCharacter* Survivor = GetSurvivor())
	{
		Survivor->NotifyPickupAnimEnded();
	}
}

void USPPickupAnimComponent::PlayPickupMontage()
{
	if (!PickupMontage)
	{
		ExitPickupState();
		return;
	}

	ASurvivorCharacter* Survivor = GetSurvivor();
	USkeletalMeshComponent* MeshComp = Survivor ? Survivor->GetMesh() : nullptr;
	UAnimInstance* AnimInstance = MeshComp ? MeshComp->GetAnimInstance() : nullptr;
	if (!AnimInstance)
	{
		ExitPickupState();
		return;
	}

	StopPickupMontage();

	ActivePickupMontage = PickupMontage;
	PickupMontageEndedDelegate.BindUObject(this, &USPPickupAnimComponent::OnPickupMontageEnded);
	AnimInstance->Montage_SetEndDelegate(PickupMontageEndedDelegate, PickupMontage);

	const float Duration = AnimInstance->Montage_Play(PickupMontage, 1.f);
	if (Duration <= 0.f)
	{
		ActivePickupMontage = nullptr;
		ExitPickupState();
	}
}

void USPPickupAnimComponent::StopPickupMontage()
{
	ASurvivorCharacter* Survivor = GetSurvivor();
	USkeletalMeshComponent* MeshComp = Survivor ? Survivor->GetMesh() : nullptr;
	UAnimInstance* AnimInstance = MeshComp ? MeshComp->GetAnimInstance() : nullptr;
	if (AnimInstance && ActivePickupMontage && AnimInstance->Montage_IsPlaying(ActivePickupMontage))
	{
		AnimInstance->Montage_Stop(PickupMontageBlendOut, ActivePickupMontage);
	}

	ActivePickupMontage = nullptr;
}

void USPPickupAnimComponent::OnPickupMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (Montage != ActivePickupMontage || !bIsPlayingPickupAnim)
	{
		return;
	}

	ActivePickupMontage = nullptr;

	if (!bInterrupted)
	{
		ExitPickupState();
	}
}

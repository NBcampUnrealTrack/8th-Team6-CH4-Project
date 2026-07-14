#include "Components/SPKillerAttackAnimComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendProfile.h"
#include "Characters/Killer/KillerCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Character.h"
#include "Systems/Data/KillerData.h"

namespace
{
	bool IsBoneInArmHierarchy(const FReferenceSkeleton& RefSkeleton, int32 BoneIndex, const TArray<FName>& ArmRootBones)
	{
		int32 Current = BoneIndex;
		while (Current != INDEX_NONE)
		{
			const FName BoneName = RefSkeleton.GetBoneName(Current);
			for (const FName& ArmRoot : ArmRootBones)
			{
				if (BoneName == ArmRoot)
				{
					return true;
				}
			}

			Current = RefSkeleton.GetParentIndex(Current);
		}

		return false;
	}
}

USPKillerAttackAnimComponent::USPKillerAttackAnimComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	FallbackArmAttackSequence = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(
		TEXT("/Game/Assets/Killer_Locomotion/Attack/SK_Mannequin_Arms_Skeleton_Attack.SK_Mannequin_Arms_Skeleton_Attack")));
	FallbackGroggyMontage = TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(
		TEXT("/Game/Assets/Killer_Locomotion/Groggy/AS_Zombie_Idle_Montage.AS_Zombie_Idle_Montage")));
}

ACharacter* USPKillerAttackAnimComponent::GetOwnerCharacter() const
{
	return Cast<ACharacter>(GetOwner());
}

const UKillerData* USPKillerAttackAnimComponent::ResolveKillerData() const
{
	if (KillerDataOverride)
	{
		return KillerDataOverride;
	}

	if (const AKillerCharacter* Killer = Cast<AKillerCharacter>(GetOwner()))
	{
		return Killer->GetKillerData();
	}

	return nullptr;
}

UAnimSequence* USPKillerAttackAnimComponent::ResolveArmAttackSequence() const
{
	if (ArmAttackSequence)
	{
		return ArmAttackSequence;
	}

	if (const UKillerData* Data = ResolveKillerData())
	{
		if (UAnimSequence* Sequence = Data->ArmAttackSequence.LoadSynchronous())
		{
			return Sequence;
		}
	}

	return FallbackArmAttackSequence.LoadSynchronous();
}

UAnimMontage* USPKillerAttackAnimComponent::ResolveGroggyMontage() const
{
	if (GroggyMontage)
	{
		return GroggyMontage;
	}

	if (const UKillerData* Data = ResolveKillerData())
	{
		if (UAnimMontage* Montage = Data->AttackGroggyMontage.LoadSynchronous())
		{
			return Montage;
		}
	}

	return FallbackGroggyMontage.LoadSynchronous();
}

float USPKillerAttackAnimComponent::GetArmAttackMontageLength() const
{
	if (const UAnimMontage* Montage = CachedArmAttackMontage.Get())
	{
		return Montage->GetPlayLength();
	}

	if (const UAnimSequence* Sequence = ResolveArmAttackSequence())
	{
		return Sequence->GetPlayLength() + ArmAttackBlendIn + ArmAttackBlendOut;
	}

	return 0.f;
}

const TArray<FName>& USPKillerAttackAnimComponent::ResolveArmBlendRootBones() const
{
	if (const UKillerData* Data = ResolveKillerData())
	{
		if (!Data->ArmAttackBlendRootBones.IsEmpty())
		{
			return Data->ArmAttackBlendRootBones;
		}
	}

	return ArmBlendRootBones;
}

UAnimInstance* USPKillerAttackAnimComponent::GetAnimInstance() const
{
	const ACharacter* OwnerCharacter = GetOwnerCharacter();
	const USkeletalMeshComponent* MeshComp = OwnerCharacter ? OwnerCharacter->GetMesh() : nullptr;
	return MeshComp ? MeshComp->GetAnimInstance() : nullptr;
}

void USPKillerAttackAnimComponent::EnsureAttackSequenceLoaded()
{
	if (!ArmAttackSequence)
	{
		ArmAttackSequence = ResolveArmAttackSequence();
	}

	if (!CachedArmAttackMontage && ArmAttackSequence)
	{
		CachedArmAttackMontage = UAnimMontage::CreateSlotAnimationAsDynamicMontage(
			ArmAttackSequence,
			AttackMontageSlotName,
			ArmAttackBlendIn,
			ArmAttackBlendOut);
	}
}

UBlendProfile* USPKillerAttackAnimComponent::GetOrCreateArmBlendProfile()
{
	if (ArmBlendProfile)
	{
		return ArmBlendProfile;
	}

	if (RuntimeArmBlendProfile)
	{
		return RuntimeArmBlendProfile;
	}

	const ACharacter* OwnerCharacter = GetOwnerCharacter();
	const USkeletalMeshComponent* MeshComp = OwnerCharacter ? OwnerCharacter->GetMesh() : nullptr;
	const USkeletalMesh* SkeletalMeshAsset = MeshComp ? MeshComp->GetSkeletalMeshAsset() : nullptr;
	const USkeleton* Skeleton = SkeletalMeshAsset ? SkeletalMeshAsset->GetSkeleton() : nullptr;
	if (!Skeleton)
	{
		return nullptr;
	}

	RuntimeArmBlendProfile = NewObject<UBlendProfile>(const_cast<USkeleton*>(Skeleton), NAME_None, RF_Transient);
	if (!RuntimeArmBlendProfile)
	{
		return nullptr;
	}

	RuntimeArmBlendProfile->Mode = EBlendProfileMode::WeightFactor;

	const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();
	const TArray<FName>& BlendRootBones = ResolveArmBlendRootBones();
	bool bHasArmRootBone = false;
	for (const FName& BoneName : BlendRootBones)
	{
		if (RefSkeleton.FindBoneIndex(BoneName) != INDEX_NONE)
		{
			bHasArmRootBone = true;
			break;
		}
	}

	if (!bHasArmRootBone)
	{
		RuntimeArmBlendProfile = nullptr;
		return nullptr;
	}

	for (int32 BoneIndex = 0; BoneIndex < RefSkeleton.GetNum(); ++BoneIndex)
	{
		const float Scale = IsBoneInArmHierarchy(RefSkeleton, BoneIndex, BlendRootBones) ? 1.f : 0.f;
		RuntimeArmBlendProfile->SetBoneBlendScale(BoneIndex, Scale, false, true);
	}

	return RuntimeArmBlendProfile;
}

void USPKillerAttackAnimComponent::BeginArmAttackAnim()
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		Multicast_BeginArmAttackAnim();
	}
}

void USPKillerAttackAnimComponent::EndArmAttackAnim()
{
	if (!bIsPlayingArmAttackAnim)
	{
		return;
	}

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		Multicast_EndArmAttackAnim();
	}
}

void USPKillerAttackAnimComponent::CancelArmAttackAnim()
{
	EndArmAttackAnim();
}

float USPKillerAttackAnimComponent::BeginGroggyAnim()
{
	float MontageDuration = 0.f;
	if (UAnimMontage* Montage = ResolveGroggyMontage())
	{
		MontageDuration = Montage->GetPlayLength();
	}

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		Multicast_BeginGroggyAnim();
	}

	return MontageDuration;
}

void USPKillerAttackAnimComponent::EndGroggyAnim()
{
	if (!bIsPlayingGroggyAnim)
	{
		return;
	}

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		Multicast_EndGroggyAnim();
	}
}

void USPKillerAttackAnimComponent::Multicast_BeginGroggyAnim_Implementation()
{
	bIsPlayingGroggyAnim = true;
	StopArmAttackMontage();
	ClearArmAttackMontageState();
	PlayGroggyMontage();
}

void USPKillerAttackAnimComponent::Multicast_EndGroggyAnim_Implementation()
{
	StopGroggyMontage();
	ClearGroggyMontageState();
}

void USPKillerAttackAnimComponent::ClearGroggyMontageState()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(GroggyResetTimer);
	}

	ActiveGroggyMontage = nullptr;
	bIsPlayingGroggyAnim = false;
}

void USPKillerAttackAnimComponent::ScheduleGroggyResetTimer(float MontageDuration)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(GroggyResetTimer);
		const float ResetDelay = FMath::Max(MontageDuration + GroggyBlendOut + 0.05f, 0.1f);
		World->GetTimerManager().SetTimer(
			GroggyResetTimer,
			this,
			&USPKillerAttackAnimComponent::OnGroggyMontageTimeout,
			ResetDelay,
			false);
	}
}

void USPKillerAttackAnimComponent::OnGroggyMontageTimeout()
{
	if (!bIsPlayingGroggyAnim)
	{
		return;
	}

	UAnimInstance* AnimInstance = GetAnimInstance();
	if (AnimInstance && ActiveGroggyMontage && AnimInstance->Montage_IsPlaying(ActiveGroggyMontage))
	{
		return;
	}

	ClearGroggyMontageState();
}

float USPKillerAttackAnimComponent::PlayGroggyMontage()
{
	UAnimMontage* MontageToPlay = ResolveGroggyMontage();
	UAnimInstance* AnimInstance = GetAnimInstance();
	if (!AnimInstance || !IsValid(AnimInstance) || !MontageToPlay)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("SPKillerAttackAnim: cannot play groggy (AnimInstance=%s Montage=%s)"),
			AnimInstance ? *AnimInstance->GetName() : TEXT("null"),
			*GetNameSafe(MontageToPlay));
		ClearGroggyMontageState();
		return 0.f;
	}

	StopGroggyMontage();

	ActiveGroggyMontage = MontageToPlay;
	GroggyMontageEndedDelegate.BindUObject(this, &USPKillerAttackAnimComponent::OnGroggyMontageEnded);
	AnimInstance->Montage_SetEndDelegate(GroggyMontageEndedDelegate, MontageToPlay);

	const float Duration = AnimInstance->Montage_Play(
		MontageToPlay,
		1.f,
		EMontagePlayReturnType::MontageLength,
		0.f,
		true);

	if (Duration <= 0.f)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("SPKillerAttackAnim: Groggy Montage_Play returned 0 for %s. Check ABP_Killer Slot (DefaultSlot)."),
			*GetNameSafe(MontageToPlay));
		ClearGroggyMontageState();
		return 0.f;
	}

	bIsPlayingGroggyAnim = true;
	ScheduleGroggyResetTimer(Duration);
	UE_LOG(LogTemp, Log, TEXT("SPKillerAttackAnim: playing groggy for %.2fs"), Duration);
	return Duration;
}

void USPKillerAttackAnimComponent::StopGroggyMontage()
{
	UAnimInstance* AnimInstance = GetAnimInstance();
	UAnimMontage* MontageToStop = ActiveGroggyMontage ? ActiveGroggyMontage.Get() : ResolveGroggyMontage();
	if (!AnimInstance || !MontageToStop)
	{
		return;
	}

	GroggyMontageEndedDelegate.Unbind();
	AnimInstance->Montage_SetEndDelegate(GroggyMontageEndedDelegate, MontageToStop);
	if (AnimInstance->Montage_IsPlaying(MontageToStop))
	{
		AnimInstance->Montage_Stop(GroggyBlendOut, MontageToStop);
	}
}

void USPKillerAttackAnimComponent::OnGroggyMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (!bIsPlayingGroggyAnim)
	{
		return;
	}

	if (Montage != ActiveGroggyMontage)
	{
		return;
	}

	ClearGroggyMontageState();
}

void USPKillerAttackAnimComponent::Multicast_BeginArmAttackAnim_Implementation()
{
	bIsPlayingArmAttackAnim = true;
	PlayArmAttackMontage();
}

void USPKillerAttackAnimComponent::Multicast_EndArmAttackAnim_Implementation()
{
	StopArmAttackMontage();
	ClearArmAttackMontageState();
}

void USPKillerAttackAnimComponent::ClearArmAttackMontageState()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ArmAttackResetTimer);
	}

	ActiveArmAttackMontage = nullptr;
	bIsPlayingArmAttackAnim = false;
}

void USPKillerAttackAnimComponent::ScheduleArmAttackResetTimer(float MontageDuration)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ArmAttackResetTimer);
		const float ResetDelay = FMath::Max(MontageDuration + ArmAttackBlendOut + 0.05f, 0.1f);
		World->GetTimerManager().SetTimer(
			ArmAttackResetTimer,
			this,
			&USPKillerAttackAnimComponent::OnArmAttackMontageTimeout,
			ResetDelay,
			false);
	}
}

void USPKillerAttackAnimComponent::OnArmAttackMontageTimeout()
{
	if (!bIsPlayingArmAttackAnim)
	{
		return;
	}

	UAnimInstance* AnimInstance = GetAnimInstance();
	if (AnimInstance && ActiveArmAttackMontage && AnimInstance->Montage_IsPlaying(ActiveArmAttackMontage))
	{
		return;
	}

	const bool bShouldBroadcast = bIsPlayingArmAttackAnim;
	ClearArmAttackMontageState();
	if (bShouldBroadcast)
	{
		OnArmAttackMontageFinished.Broadcast();
	}
}

void USPKillerAttackAnimComponent::PlayArmAttackMontage()
{
	EnsureAttackSequenceLoaded();

	UAnimInstance* AnimInstance = GetAnimInstance();
	if (!AnimInstance || !IsValid(AnimInstance) || !CachedArmAttackMontage)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("SPKillerAttackAnim: cannot play arm attack (AnimInstance=%s CachedMontage=%s Sequence=%s)"),
			AnimInstance ? *AnimInstance->GetName() : TEXT("null"),
			*GetNameSafe(CachedArmAttackMontage.Get()),
			*GetNameSafe(ArmAttackSequence.Get()));
		ClearArmAttackMontageState();
		return;
	}

	const ACharacter* OwnerCharacter = GetOwnerCharacter();
	const USkeletalMeshComponent* MeshComp = OwnerCharacter ? OwnerCharacter->GetMesh() : nullptr;
	const USkeletalMesh* SkeletalMeshAsset = MeshComp ? MeshComp->GetSkeletalMeshAsset() : nullptr;
	const USkeleton* MeshSkeleton = SkeletalMeshAsset ? SkeletalMeshAsset->GetSkeleton() : nullptr;
	const USkeleton* SequenceSkeleton = ArmAttackSequence ? ArmAttackSequence->GetSkeleton() : nullptr;
	if (MeshSkeleton && SequenceSkeleton && MeshSkeleton != SequenceSkeleton)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("SPKillerAttackAnim: attack sequence skeleton (%s) does not match mesh skeleton (%s)."),
			*SequenceSkeleton->GetPathName(),
			*MeshSkeleton->GetPathName());
		ClearArmAttackMontageState();
		return;
	}

	StopArmAttackMontage();

	UAnimMontage* MontageToPlay = CachedArmAttackMontage;
	ActiveArmAttackMontage = MontageToPlay;
	ArmAttackMontageEndedDelegate.BindUObject(this, &USPKillerAttackAnimComponent::OnArmAttackMontageEnded);
	AnimInstance->Montage_SetEndDelegate(ArmAttackMontageEndedDelegate, MontageToPlay);

	float Duration = 0.f;
	if (UBlendProfile* BlendProfile = GetOrCreateArmBlendProfile())
	{
		FMontageBlendSettings BlendSettings;
		BlendSettings.BlendProfile = BlendProfile;
		BlendSettings.Blend.BlendTime = ArmAttackBlendIn;
		Duration = AnimInstance->Montage_PlayWithBlendSettings(
			MontageToPlay,
			BlendSettings,
			1.f,
			EMontagePlayReturnType::MontageLength,
			0.f,
			true);
	}
	else
	{
		Duration = AnimInstance->Montage_Play(
			MontageToPlay,
			1.f,
			EMontagePlayReturnType::MontageLength,
			0.f,
			true);
	}

	if (Duration <= 0.f)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("SPKillerAttackAnim: Montage_Play returned 0 for %s (slot=%s). Check ABP_Killer has a Slot node named DefaultSlot."),
			*GetNameSafe(MontageToPlay),
			*AttackMontageSlotName.ToString());
		ClearArmAttackMontageState();
		return;
	}

	bIsPlayingArmAttackAnim = true;
	ScheduleArmAttackResetTimer(Duration);
	UE_LOG(LogTemp, Log, TEXT("SPKillerAttackAnim: playing arm attack for %.2fs"), Duration);
}

void USPKillerAttackAnimComponent::StopArmAttackMontage()
{
	UAnimInstance* AnimInstance = GetAnimInstance();
	UAnimMontage* MontageToStop = ActiveArmAttackMontage ? ActiveArmAttackMontage.Get() : CachedArmAttackMontage.Get();
	if (!AnimInstance || !MontageToStop)
	{
		return;
	}

	ArmAttackMontageEndedDelegate.Unbind();
	AnimInstance->Montage_SetEndDelegate(ArmAttackMontageEndedDelegate, MontageToStop);
	if (AnimInstance->Montage_IsPlaying(MontageToStop))
	{
		AnimInstance->Montage_Stop(ArmAttackBlendOut, MontageToStop);
	}
}

void USPKillerAttackAnimComponent::OnArmAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (!bIsPlayingArmAttackAnim)
	{
		return;
	}

	if (Montage != ActiveArmAttackMontage && Montage != CachedArmAttackMontage)
	{
		return;
	}

	const bool bShouldBroadcast = !bInterrupted;
	ClearArmAttackMontageState();
	if (bShouldBroadcast)
	{
		OnArmAttackMontageFinished.Broadcast();
	}
}

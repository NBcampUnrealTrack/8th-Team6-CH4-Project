#include "Components/SPKillerCarryAnimComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendProfile.h"
#include "Characters/Killer/KillerCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Character.h"
#include "Systems/Data/KillerData.h"
#include "TimerManager.h"

namespace SPKillerCarryAnimPrivate
{
	bool IsBoneInHierarchy(const FReferenceSkeleton& RefSkeleton, int32 BoneIndex, const TArray<FName>& RootBones)
	{
		int32 Current = BoneIndex;
		while (Current != INDEX_NONE)
		{
			const FName BoneName = RefSkeleton.GetBoneName(Current);
			for (const FName& RootBone : RootBones)
			{
				if (BoneName == RootBone)
				{
					return true;
				}
			}
			Current = RefSkeleton.GetParentIndex(Current);
		}
		return false;
	}
}

USPKillerCarryAnimComponent::USPKillerCarryAnimComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetIsReplicatedByDefault(true);

	FallbackPickupMontage = TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(
		TEXT("/Game/Assets/Killer_Locomotion/Pickup/AS_PickUP_Montage.AS_PickUP_Montage")));
	FallbackCarryingMontage = TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(
		TEXT("/Game/Assets/Killer_Locomotion/Pickup/AS_Carrying_Montage.AS_Carrying_Montage")));
	FallbackPickupSequence = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(
		TEXT("/Game/AS_PickUP.AS_PickUP")));
	FallbackCarryingSequence = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(
		TEXT("/Game/AS_Carrying.AS_Carrying")));
}

void USPKillerCarryAnimComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bWantsCarryingAnim)
	{
		EnsureCarryingMontagePlaying();
	}
}

ACharacter* USPKillerCarryAnimComponent::GetOwnerCharacter() const
{
	return Cast<ACharacter>(GetOwner());
}

UAnimInstance* USPKillerCarryAnimComponent::GetAnimInstance() const
{
	const ACharacter* OwnerCharacter = GetOwnerCharacter();
	const USkeletalMeshComponent* MeshComp = OwnerCharacter ? OwnerCharacter->GetMesh() : nullptr;
	return MeshComp ? MeshComp->GetAnimInstance() : nullptr;
}

const UKillerData* USPKillerCarryAnimComponent::ResolveKillerData() const
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

UAnimMontage* USPKillerCarryAnimComponent::ResolvePickupMontage() const
{
	if (const UKillerData* Data = ResolveKillerData())
	{
		if (UAnimMontage* Montage = Data->PickupMontage.LoadSynchronous())
		{
			return Montage;
		}
	}

	if (UAnimMontage* Montage = FallbackPickupMontage.LoadSynchronous())
	{
		return Montage;
	}

	if (UAnimSequence* Sequence = FallbackPickupSequence.LoadSynchronous())
	{
		return UAnimMontage::CreateSlotAnimationAsDynamicMontage(
			Sequence,
			PickupMontageSlotName,
			PickupBlendIn,
			PickupBlendOut,
			1.f,
			1);
	}

	return nullptr;
}

UAnimMontage* USPKillerCarryAnimComponent::ResolveCarryingMontage() const
{
	if (const UKillerData* Data = ResolveKillerData())
	{
		if (UAnimMontage* Montage = Data->CarryingMontage.LoadSynchronous())
		{
			return Montage;
		}
	}

	if (UAnimMontage* Montage = FallbackCarryingMontage.LoadSynchronous())
	{
		return Montage;
	}

	if (UAnimSequence* Sequence = FallbackCarryingSequence.LoadSynchronous())
	{
		return UAnimMontage::CreateSlotAnimationAsDynamicMontage(
			Sequence,
			MontageSlotName,
			CarryingBlendIn,
			CarryingBlendOut,
			1.f,
			9999);
	}

	return nullptr;
}

UAnimMontage* USPKillerCarryAnimComponent::PrepareMontageForSlot(UAnimMontage* SourceMontage, FName SlotName)
{
	if (!SourceMontage || SlotName.IsNone())
	{
		return SourceMontage;
	}

	// Always rebuild from AS_Carrying, even when the authored montage already uses this
	// slot. The authored montage can reach its end and restart through the end delegate,
	// briefly exposing the source pose. A looping dynamic montage keeps the upper-body
	// carry pose continuous while the AnimGraph supplies motion-matched legs.
	for (const FSlotAnimationTrack& Track : SourceMontage->SlotAnimTracks)
	{
		for (const FAnimSegment& Segment : Track.AnimTrack.AnimSegments)
		{
			if (UAnimSequence* Sequence = Cast<UAnimSequence>(Segment.GetAnimReference()))
			{
				if (UAnimMontage* DynamicMontage = UAnimMontage::CreateSlotAnimationAsDynamicMontage(
					Sequence,
					SlotName,
					CarryingBlendIn,
					CarryingBlendOut,
					1.f,
					9999))
				{
					return DynamicMontage;
				}
			}
		}
	}

	UAnimMontage* SlotMontage = DuplicateObject<UAnimMontage>(SourceMontage, GetTransientPackage());
	if (!SlotMontage)
	{
		return SourceMontage;
	}

	SlotMontage->SetFlags(RF_Transient);
	if (SlotMontage->SlotAnimTracks.Num() == 0)
	{
		FSlotAnimationTrack& NewTrack = SlotMontage->SlotAnimTracks.AddDefaulted_GetRef();
		NewTrack.SlotName = SlotName;
	}
	else
	{
		for (FSlotAnimationTrack& Track : SlotMontage->SlotAnimTracks)
		{
			Track.SlotName = SlotName;
		}
	}

	return SlotMontage;
}

const TArray<FName>& USPKillerCarryAnimComponent::ResolveCarryBlendRootBones() const
{
	if (const UKillerData* Data = ResolveKillerData())
	{
		if (!Data->CarryBlendRootBones.IsEmpty())
		{
			return Data->CarryBlendRootBones;
		}
	}

	return CarryBlendRootBones;
}

float USPKillerCarryAnimComponent::ResolveAttachNormalizedTime() const
{
	if (const UKillerData* Data = ResolveKillerData())
	{
		return FMath::Clamp(Data->PickupAttachNormalizedTime, 0.f, 1.f);
	}

	return 0.55f;
}

float USPKillerCarryAnimComponent::GetPickupMontagePlayLength() const
{
	if (UAnimMontage* Montage = ResolvePickupMontage())
	{
		const float PlayLength = Montage->GetPlayLength();
		if (PlayLength > KINDA_SMALL_NUMBER)
		{
			return PlayLength;
		}
	}

	if (const UKillerData* Data = ResolveKillerData())
	{
		return Data->PickupDuration;
	}

	return 1.25f;
}

UBlendProfile* USPKillerCarryAnimComponent::GetOrCreateCarryBlendProfile()
{
	if (RuntimeCarryBlendProfile)
	{
		return RuntimeCarryBlendProfile;
	}

	const ACharacter* OwnerCharacter = GetOwnerCharacter();
	const USkeletalMeshComponent* MeshComp = OwnerCharacter ? OwnerCharacter->GetMesh() : nullptr;
	const USkeletalMesh* SkeletalMeshAsset = MeshComp ? MeshComp->GetSkeletalMeshAsset() : nullptr;
	const USkeleton* Skeleton = SkeletalMeshAsset ? SkeletalMeshAsset->GetSkeleton() : nullptr;
	if (!Skeleton)
	{
		return nullptr;
	}

	RuntimeCarryBlendProfile = NewObject<UBlendProfile>(const_cast<USkeleton*>(Skeleton), NAME_None, RF_Transient);
	if (!RuntimeCarryBlendProfile)
	{
		return nullptr;
	}

	RuntimeCarryBlendProfile->Mode = EBlendProfileMode::WeightFactor;

	const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();
	const TArray<FName>& BlendRootBones = ResolveCarryBlendRootBones();
	bool bHasRoot = false;
	for (const FName& BoneName : BlendRootBones)
	{
		if (RefSkeleton.FindBoneIndex(BoneName) != INDEX_NONE)
		{
			bHasRoot = true;
			break;
		}
	}

	if (!bHasRoot)
	{
		RuntimeCarryBlendProfile = nullptr;
		return nullptr;
	}

	for (int32 BoneIndex = 0; BoneIndex < RefSkeleton.GetNum(); ++BoneIndex)
	{
		const float Scale = SPKillerCarryAnimPrivate::IsBoneInHierarchy(RefSkeleton, BoneIndex, BlendRootBones)
			? 1.f
			: 0.f;
		RuntimeCarryBlendProfile->SetBoneBlendScale(BoneIndex, Scale, false, true);
	}

	return RuntimeCarryBlendProfile;
}

bool USPKillerCarryAnimComponent::ShouldKeepCarryingAnim() const
{
	if (!bWantsCarryingAnim)
	{
		return false;
	}

	if (const AKillerCharacter* Killer = Cast<AKillerCharacter>(GetOwner()))
	{
		return Killer->GetKillerState() == EKillerState::Carrying;
	}

	return bWantsCarryingAnim;
}

void USPKillerCarryAnimComponent::BeginPickupAnim()
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		Multicast_BeginPickupAnim();
	}
}

void USPKillerCarryAnimComponent::BeginCarryingAnim()
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		Multicast_BeginCarryingAnim();
	}
	else
	{
		// Client-side recovery when replication arrives without the multicast.
		Multicast_BeginCarryingAnim_Implementation();
	}
}

void USPKillerCarryAnimComponent::EndCarryAnims()
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		Multicast_EndCarryAnims();
	}
	else
	{
		Multicast_EndCarryAnims_Implementation();
	}
}

void USPKillerCarryAnimComponent::Multicast_BeginPickupAnim_Implementation()
{
	StopActiveMontages(PickupBlendOut);
	bWantsCarryingAnim = false;
	SetComponentTickEnabled(false);
	ClearCarryingState();
	ClearPickupState();

	bIsPlayingPickupAnim = true;
	bAttachRequested = false;
	PlayPickupMontage();
	OnPickupBegan.Broadcast();
}

void USPKillerCarryAnimComponent::Multicast_BeginCarryingAnim_Implementation()
{
	StopActiveMontages(CarryingBlendIn);
	ClearPickupState();

	bWantsCarryingAnim = true;
	bIsPlayingCarryingAnim = true;
	SetComponentTickEnabled(true);
	PlayCarryingMontage();
}

void USPKillerCarryAnimComponent::Multicast_EndCarryAnims_Implementation()
{
	bWantsCarryingAnim = false;
	SetComponentTickEnabled(false);
	StopActiveMontages(CarryingBlendOut);
	ClearPickupState();
	ClearCarryingState();
	OnCarryEnded.Broadcast();
}

void USPKillerCarryAnimComponent::ClearPickupState()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PickupAttachTimer);
		World->GetTimerManager().ClearTimer(PickupResetTimer);
	}

	ActivePickupMontage = nullptr;
	bIsPlayingPickupAnim = false;
}

void USPKillerCarryAnimComponent::ClearCarryingState()
{
	ActiveCarryingMontage = nullptr;
	bIsPlayingCarryingAnim = false;
}

void USPKillerCarryAnimComponent::ConfigureCarryingMontageLoop(UAnimInstance* AnimInstance, UAnimMontage* Montage) const
{
	if (!AnimInstance || !Montage)
	{
		return;
	}

	const int32 NumSections = Montage->GetNumSections();
	for (int32 Index = 0; Index < NumSections; ++Index)
	{
		const FName SectionName = Montage->GetSectionName(Index);
		if (!SectionName.IsNone())
		{
			AnimInstance->Montage_SetNextSection(SectionName, SectionName, Montage);
		}
	}
}

void USPKillerCarryAnimComponent::PlayPickupMontage()
{
	UAnimMontage* MontageToPlay = ResolvePickupMontage();
	UAnimInstance* AnimInstance = GetAnimInstance();
	if (!AnimInstance || !IsValid(AnimInstance) || !MontageToPlay)
	{
		ClearPickupState();
		OnPickupFinished.Broadcast();
		return;
	}

	ActivePickupMontage = MontageToPlay;
	PickupMontageEndedDelegate.BindUObject(this, &USPKillerCarryAnimComponent::OnPickupMontageEnded);
	AnimInstance->Montage_SetEndDelegate(PickupMontageEndedDelegate, MontageToPlay);

	const float Duration = AnimInstance->Montage_Play(
		MontageToPlay,
		1.f,
		EMontagePlayReturnType::MontageLength,
		0.f,
		true);

	if (Duration <= 0.f)
	{
		ClearPickupState();
		OnPickupFinished.Broadcast();
		return;
	}

	if (UWorld* World = GetWorld())
	{
		const float AttachDelay = Duration * ResolveAttachNormalizedTime();
		World->GetTimerManager().ClearTimer(PickupAttachTimer);
		World->GetTimerManager().SetTimer(
			PickupAttachTimer,
			this,
			&USPKillerCarryAnimComponent::OnPickupAttachTimeout,
			FMath::Max(AttachDelay, 0.01f),
			false);

		World->GetTimerManager().ClearTimer(PickupResetTimer);
		World->GetTimerManager().SetTimer(
			PickupResetTimer,
			this,
			&USPKillerCarryAnimComponent::OnPickupMontageTimeout,
			FMath::Max(Duration + PickupBlendOut + 0.05f, 0.1f),
			false);
	}
}

void USPKillerCarryAnimComponent::PlayCarryingMontage()
{
	UAnimMontage* SourceMontage = ResolveCarryingMontage();
	UAnimInstance* AnimInstance = GetAnimInstance();
	if (!AnimInstance || !IsValid(AnimInstance) || !SourceMontage)
	{
		ClearCarryingState();
		return;
	}

	// UpperBody slot + ABP layered blend keeps legs on motion matching.
	UAnimMontage* MontageToPlay = PrepareMontageForSlot(SourceMontage, MontageSlotName);
	ActiveCarryingMontage = MontageToPlay;
	bIsPlayingCarryingAnim = true;
	CarryingMontageEndedDelegate.BindUObject(this, &USPKillerCarryAnimComponent::OnCarryingMontageEnded);
	AnimInstance->Montage_SetEndDelegate(CarryingMontageEndedDelegate, MontageToPlay);

	const float Duration = AnimInstance->Montage_Play(
		MontageToPlay,
		1.f,
		EMontagePlayReturnType::MontageLength,
		0.f,
		true);

	if (Duration <= 0.f)
	{
		UE_LOG(LogTemp, Warning, TEXT("SPKillerCarryAnim: failed to play carrying montage %s on slot %s"),
			*GetNameSafe(MontageToPlay),
			*MontageSlotName.ToString());
		ClearCarryingState();
		return;
	}

	ConfigureCarryingMontageLoop(AnimInstance, MontageToPlay);
	UE_LOG(LogTemp, Log, TEXT("SPKillerCarryAnim: playing %s on slot %s for %.2fs"),
		*GetNameSafe(MontageToPlay),
		*MontageSlotName.ToString(),
		Duration);
}

void USPKillerCarryAnimComponent::EnsureCarryingMontagePlaying()
{
	if (!ShouldKeepCarryingAnim())
	{
		return;
	}

	UAnimInstance* AnimInstance = GetAnimInstance();
	if (!AnimInstance)
	{
		return;
	}

	const bool bPlaying = ActiveCarryingMontage && AnimInstance->Montage_IsPlaying(ActiveCarryingMontage);
	if (!bPlaying)
	{
		PlayCarryingMontage();
	}
}

void USPKillerCarryAnimComponent::StopActiveMontages(float BlendOut)
{
	UAnimInstance* AnimInstance = GetAnimInstance();
	if (!AnimInstance)
	{
		return;
	}

	PickupMontageEndedDelegate.Unbind();
	CarryingMontageEndedDelegate.Unbind();

	if (ActivePickupMontage && AnimInstance->Montage_IsPlaying(ActivePickupMontage))
	{
		AnimInstance->Montage_Stop(BlendOut, ActivePickupMontage);
	}

	if (ActiveCarryingMontage && AnimInstance->Montage_IsPlaying(ActiveCarryingMontage))
	{
		AnimInstance->Montage_Stop(BlendOut, ActiveCarryingMontage);
	}
}

void USPKillerCarryAnimComponent::OnPickupAttachTimeout()
{
	if (!bIsPlayingPickupAnim || bAttachRequested)
	{
		return;
	}

	bAttachRequested = true;
	OnPickupAttachRequested.Broadcast();
}

void USPKillerCarryAnimComponent::OnPickupMontageTimeout()
{
	if (!bIsPlayingPickupAnim)
	{
		return;
	}

	UAnimInstance* AnimInstance = GetAnimInstance();
	if (AnimInstance && ActivePickupMontage && AnimInstance->Montage_IsPlaying(ActivePickupMontage))
	{
		return;
	}

	if (!bAttachRequested)
	{
		bAttachRequested = true;
		OnPickupAttachRequested.Broadcast();
	}

	ClearPickupState();
	OnPickupFinished.Broadcast();
}

void USPKillerCarryAnimComponent::OnPickupMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (!bIsPlayingPickupAnim || Montage != ActivePickupMontage)
	{
		return;
	}

	(void)bInterrupted;

	if (!bAttachRequested)
	{
		bAttachRequested = true;
		OnPickupAttachRequested.Broadcast();
	}

	ClearPickupState();
	// Enter carry even if the pickup montage was interrupted (e.g. MM / slot conflict).
	OnPickupFinished.Broadcast();
}

void USPKillerCarryAnimComponent::OnCarryingMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	(void)bInterrupted;

	if (Montage != ActiveCarryingMontage && Montage != ResolveCarryingMontage())
	{
		return;
	}

	if (ShouldKeepCarryingAnim())
	{
		PlayCarryingMontage();
		return;
	}

	ClearCarryingState();
}

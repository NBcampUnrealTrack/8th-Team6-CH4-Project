#include "Components/SPHealingAnimComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "AlphaBlend.h"
#include "Characters/Survivor/SurvivorCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

namespace SPHealingAnim
{
	constexpr float HealingReturnNotPlayingTimeout = 0.2f;
}

USPHealingAnimComponent::USPHealingAnimComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetIsReplicatedByDefault(true);
}

void USPHealingAnimComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(USPHealingAnimComponent, bIsHealing);
}

void USPHealingAnimComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (CurrentPhase == EHealingAnimPhase::Return)
	{
		UAnimInstance* AnimInstance = GetAnimInstance();
		if (!AnimInstance || !HealingReturnMontage || !AnimInstance->Montage_IsPlaying(HealingReturnMontage))
		{
			ReturnMontageNotPlayingTime += DeltaTime;
			if (ReturnMontageNotPlayingTime >= SPHealingAnim::HealingReturnNotPlayingTimeout)
			{
				FinishHealingChannel();
			}
		}
		else
		{
			ReturnMontageNotPlayingTime = 0.f;

			const float ReturnLength = HealingReturnMontage->GetPlayLength();
			if (ReturnLength > KINDA_SMALL_NUMBER)
			{
				const float Position = AnimInstance->Montage_GetPosition(HealingReturnMontage);
				if (Position >= ReturnLength - 0.05f)
				{
					FinishHealingChannel();
				}
			}
		}
		return;
	}

	if (!bIsHealing)
	{
		return;
	}

	if (GetOwner() && GetOwner()->HasAuthority() && ShouldEndLoopChannel())
	{
		EndHealingChannel(true);
		return;
	}

	if (CurrentPhase == EHealingAnimPhase::Knee && !bLoopTransitionTriggered)
	{
		UAnimInstance* AnimInstance = GetAnimInstance();
		if (!AnimInstance || !HealingKneeMontage || !HealingLoopMontage)
		{
			return;
		}

		if (!AnimInstance->Montage_IsPlaying(HealingKneeMontage))
		{
			TransitionKneeToLoop();
			return;
		}

		const float MontageLength = HealingKneeMontage->GetPlayLength();
		if (MontageLength <= KINDA_SMALL_NUMBER)
		{
			TransitionKneeToLoop();
			return;
		}

		const float MontagePosition = AnimInstance->Montage_GetPosition(HealingKneeMontage);
		const float BlendIn = FMath::Clamp(HealingKneeToLoopBlendIn, 0.05f, MontageLength * 0.5f);
		if (MontagePosition >= MontageLength - BlendIn)
		{
			TransitionKneeToLoop();
		}
		return;
	}

	if (CurrentPhase == EHealingAnimPhase::Loop && !bIsEndingHealing)
	{
		UAnimInstance* AnimInstance = GetAnimInstance();
		if (!AnimInstance || !HealingLoopMontage)
		{
			return;
		}

		if (!AnimInstance->Montage_IsPlaying(HealingLoopMontage))
		{
			bLoopCrossfadeQueued = false;
			RestartLoopMontage();
			return;
		}

		const float LoopLength = HealingLoopMontage->GetPlayLength();
		if (LoopLength <= KINDA_SMALL_NUMBER)
		{
			return;
		}

		const float Position = AnimInstance->Montage_GetPosition(HealingLoopMontage);
		const float BlendIn = FMath::Clamp(HealingLoopRestartBlendIn, 0.05f, LoopLength * 0.45f);

		// Allow another wrap once we are back near the start after a crossfade restart.
		if (Position < BlendIn)
		{
			bLoopCrossfadeQueued = false;
		}

		if (!bLoopCrossfadeQueued && Position >= LoopLength - BlendIn)
		{
			bLoopCrossfadeQueued = true;
			RestartLoopMontage();
		}
	}
}

void USPHealingAnimComponent::BeginHealingChannel()
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || bIsHealing)
	{
		return;
	}

	Multicast_BeginHealingChannel();
}

void USPHealingAnimComponent::EndHealingChannel(const bool bCompleted)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !IsHealingChannelActive())
	{
		return;
	}

	Multicast_EndHealingChannel(bCompleted);
}

void USPHealingAnimComponent::CancelHealingChannel()
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !IsHealingChannelActive())
	{
		return;
	}

	Multicast_CancelHealingChannel();
}

void USPHealingAnimComponent::BeginHealingChannelDebug()
{
	BeginHealingChannel();
}

void USPHealingAnimComponent::CancelHealingChannelDebug()
{
	CancelHealingChannel();
}

void USPHealingAnimComponent::Multicast_BeginHealingChannel_Implementation()
{
	StartHealingChannelInternal();
}

void USPHealingAnimComponent::Multicast_EndHealingChannel_Implementation(const bool bCompleted)
{
	if (!IsHealingChannelActive())
	{
		return;
	}

	ExitHealingState(bCompleted);
}

void USPHealingAnimComponent::Multicast_CancelHealingChannel_Implementation()
{
	if (!IsHealingChannelActive())
	{
		return;
	}

	ExitHealingState(false);
}

void USPHealingAnimComponent::OnHealingMontageEnded(UAnimMontage* Montage, const bool bInterrupted)
{
	if (Montage == HealingReturnMontage && CurrentPhase != EHealingAnimPhase::Return)
	{
		return;
	}

	if (Montage == HealingKneeMontage && CurrentPhase != EHealingAnimPhase::Knee)
	{
		return;
	}

	if (Montage == HealingLoopMontage && CurrentPhase != EHealingAnimPhase::Loop)
	{
		return;
	}

	if (CurrentPhase == EHealingAnimPhase::Return)
	{
		if (Montage == HealingReturnMontage)
		{
			ActiveHealingMontage = nullptr;
			FinishHealingChannel();
		}
		return;
	}

	if (!bIsHealing)
	{
		return;
	}

	if (bLoopTransitionTriggered && Montage == HealingKneeMontage)
	{
		return;
	}

	if (CurrentPhase == EHealingAnimPhase::Knee && Montage == HealingKneeMontage)
	{
		TransitionKneeToLoop();
		return;
	}

	if (CurrentPhase == EHealingAnimPhase::Loop && Montage == HealingLoopMontage)
	{
		if (bIsEndingHealing)
		{
			return;
		}

		RestartLoopMontage();
	}
}

ASurvivorCharacter* USPHealingAnimComponent::GetSurvivor() const
{
	return Cast<ASurvivorCharacter>(GetOwner());
}

UAnimInstance* USPHealingAnimComponent::GetAnimInstance() const
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	return Character && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
}

bool USPHealingAnimComponent::IsHealingChannelActive() const
{
	return bIsHealing
		|| CurrentPhase == EHealingAnimPhase::Knee
		|| CurrentPhase == EHealingAnimPhase::Loop;
}

void USPHealingAnimComponent::StartHealingChannelInternal()
{
	if (bIsHealing || CurrentPhase == EHealingAnimPhase::Return)
	{
		return;
	}

	EnsureMontagesLoaded();
	if (!HealingKneeMontage || !HealingLoopMontage || !HealingReturnMontage)
	{
		return;
	}

	bIsHealing = true;
	bIsEndingHealing = false;
	bLoopTransitionTriggered = false;
	bLoopCrossfadeQueued = false;
	LoopEndWorldTime = 0.f;
	ReturnMontageNotPlayingTime = 0.f;
	ClearHealingTimers();
	EnterHealingState();
	SetHealingTickEnabled(true);
	PlayHealingMontage(HealingKneeMontage, EHealingAnimPhase::Knee);
}

void USPHealingAnimComponent::EnterHealingState()
{
	ASurvivorCharacter* Survivor = GetSurvivor();
	if (!Survivor)
	{
		return;
	}

	if (USkeletalMeshComponent* MeshComp = Survivor->GetMesh())
	{
		if (UAnimInstance* AnimInstance = MeshComp->GetAnimInstance())
		{
			AnimInstance->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
		}
	}

}

void USPHealingAnimComponent::ExitHealingState(const bool bPlayReturnAnim)
{
	if (CurrentPhase == EHealingAnimPhase::Return)
	{
		return;
	}

	OnHealingChannelEnded.Broadcast(bPlayReturnAnim);

	bIsEndingHealing = true;
	bLoopTransitionTriggered = true;
	bIsHealing = false;

	if (bPlayReturnAnim && HealingReturnMontage)
	{
		PlayReturnMontage(HealingToReturnBlendIn);
		return;
	}

	FinishHealingChannel();
}

void USPHealingAnimComponent::FinishHealingChannel()
{
	if (CurrentPhase == EHealingAnimPhase::None && !bIsHealing && !bIsEndingHealing)
	{
		RestoreMovementAfterHealing();
		return;
	}

	SetHealingTickEnabled(false);
	ClearHealingTimers();
	ReturnMontageNotPlayingTime = 0.f;
	StopAllHealingMontages(0.f);

	bIsHealing = false;
	bIsEndingHealing = false;
	bLoopTransitionTriggered = false;
	bLoopCrossfadeQueued = false;
	LoopEndWorldTime = 0.f;
	CurrentPhase = EHealingAnimPhase::None;
	ActiveHealingMontage = nullptr;

	RestoreMovementAfterHealing();

	if (ASurvivorCharacter* Survivor = GetSurvivor())
	{
		Survivor->NotifyHealingAnimEnded();
	}
}

void USPHealingAnimComponent::TransitionKneeToLoop()
{
	if (!bIsHealing || CurrentPhase != EHealingAnimPhase::Knee || !HealingLoopMontage || bLoopTransitionTriggered)
	{
		return;
	}

	UAnimInstance* AnimInstance = GetAnimInstance();
	if (!AnimInstance)
	{
		return;
	}

	bLoopTransitionTriggered = true;
	SetHealingTickEnabled(true);
	UpdateLoopEndTime();
	PlayLoopMontage(HealingKneeToLoopBlendIn, true);

	OnHealingLoopBegan.Broadcast();
}

void USPHealingAnimComponent::PlayLoopMontage(const float BlendInTime, const bool bCrossfadeFromPrevious)
{
	if (!bIsHealing || !HealingLoopMontage)
	{
		return;
	}

	UAnimInstance* AnimInstance = GetAnimInstance();
	if (!AnimInstance)
	{
		return;
	}

	CurrentPhase = EHealingAnimPhase::Loop;
	ActiveHealingMontage = HealingLoopMontage;

	const float BlendIn = BlendInTime > 0.f
		? FMath::Clamp(BlendInTime, 0.05f, 0.25f)
		: 0.f;

	if (bCrossfadeFromPrevious)
	{
		if (HealingKneeMontage && AnimInstance->Montage_IsPlaying(HealingKneeMontage))
		{
			AnimInstance->Montage_Stop(BlendIn > 0.f ? BlendIn : HealingMontageBlendOut, HealingKneeMontage);
		}
	}
	else
	{
		StopAllHealingMontages(0.f);
	}

	HealingMontageEndedDelegate.BindUObject(this, &USPHealingAnimComponent::OnHealingMontageEnded);
	AnimInstance->Montage_SetEndDelegate(HealingMontageEndedDelegate, HealingLoopMontage);

	float Duration = 0.f;
	if (BlendIn > 0.f)
	{
		const FAlphaBlendArgs BlendInArgs(BlendIn);
		// bStopAllMontages=false so Knee can crossfade into Loop.
		Duration = AnimInstance->Montage_PlayWithBlendIn(
			HealingLoopMontage,
			BlendInArgs,
			1.f,
			EMontagePlayReturnType::MontageLength,
			0.f,
			false);
	}
	else
	{
		Duration = AnimInstance->Montage_Play(HealingLoopMontage, 1.f);
	}

	if (Duration <= 0.f)
	{
		Duration = AnimInstance->Montage_Play(HealingLoopMontage, 1.f);
	}

	if (Duration <= 0.f)
	{
		ActiveHealingMontage = nullptr;
		return;
	}

	ConfigureLoopMontage(AnimInstance);
	SetHealingTickEnabled(true);
}

void USPHealingAnimComponent::PlayReturnMontage(const float BlendInTime)
{
	if (!HealingReturnMontage)
	{
		FinishHealingChannel();
		return;
	}

	UAnimInstance* AnimInstance = GetAnimInstance();
	if (!AnimInstance)
	{
		FinishHealingChannel();
		return;
	}

	CurrentPhase = EHealingAnimPhase::Return;
	ActiveHealingMontage = HealingReturnMontage;

	const float BlendIn = FMath::Clamp(BlendInTime > 0.f ? BlendInTime : HealingToReturnBlendIn, 0.05f, 0.3f);
	BlendOutLoopForReturn(AnimInstance, BlendIn);

	HealingMontageEndedDelegate.BindUObject(this, &USPHealingAnimComponent::OnHealingMontageEnded);
	AnimInstance->Montage_SetEndDelegate(HealingMontageEndedDelegate, HealingReturnMontage);

	const FAlphaBlendArgs BlendInArgs(BlendIn);
	const float Duration = AnimInstance->Montage_PlayWithBlendIn(
		HealingReturnMontage,
		BlendInArgs,
		1.f,
		EMontagePlayReturnType::MontageLength,
		0.f,
		false);

	if (Duration <= 0.f)
	{
		ActiveHealingMontage = nullptr;
		FinishHealingChannel();
		return;
	}

	ReturnMontageNotPlayingTime = 0.f;
	SetHealingTickEnabled(true);
	ClearHealingTimers();
	if (UWorld* World = GetWorld())
	{
		const float FallbackDelay = Duration + BlendIn + 0.2f;
		World->GetTimerManager().SetTimer(
			ReturnMontageFallbackTimer,
			this,
			&USPHealingAnimComponent::HandleReturnMontageFallback,
			FallbackDelay,
			false);
	}
}

void USPHealingAnimComponent::ConfigureLoopMontage(UAnimInstance* AnimInstance)
{
	// Do not SetNextSection(self) — that hard-cuts. Loop wrap uses RestartLoopMontage crossfade.
	ClearLoopMontage(AnimInstance);
}

void USPHealingAnimComponent::ClearLoopMontage(UAnimInstance* AnimInstance)
{
	if (!AnimInstance || !HealingLoopMontage || HealingLoopMontage->CompositeSections.Num() == 0)
	{
		return;
	}

	const FName SectionName = HealingLoopMontage->GetSectionName(0);
	AnimInstance->Montage_SetNextSection(SectionName, NAME_None, HealingLoopMontage);
}

void USPHealingAnimComponent::RestartLoopMontage()
{
	if (bIsEndingHealing || !bIsHealing || CurrentPhase != EHealingAnimPhase::Loop || !HealingLoopMontage)
	{
		return;
	}

	UAnimInstance* AnimInstance = GetAnimInstance();
	if (!AnimInstance)
	{
		return;
	}

	const float LoopLength = HealingLoopMontage->GetPlayLength();
	const float BlendIn = FMath::Clamp(
		HealingLoopRestartBlendIn,
		0.05f,
		LoopLength > KINDA_SMALL_NUMBER ? LoopLength * 0.45f : 0.25f);

	ClearLoopMontage(AnimInstance);

	HealingMontageEndedDelegate.BindUObject(this, &USPHealingAnimComponent::OnHealingMontageEnded);
	AnimInstance->Montage_SetEndDelegate(HealingMontageEndedDelegate, HealingLoopMontage);

	const FAlphaBlendArgs BlendInArgs(BlendIn);
	float Duration = AnimInstance->Montage_PlayWithBlendIn(
		HealingLoopMontage,
		BlendInArgs,
		1.f,
		EMontagePlayReturnType::MontageLength,
		0.f,
		true);

	if (Duration <= 0.f)
	{
		Duration = AnimInstance->Montage_Play(HealingLoopMontage, 1.f);
	}

	if (Duration <= 0.f)
	{
		return;
	}

	ActiveHealingMontage = HealingLoopMontage;
	CurrentPhase = EHealingAnimPhase::Loop;
	ConfigureLoopMontage(AnimInstance);
}

void USPHealingAnimComponent::BlendOutLoopForReturn(UAnimInstance* AnimInstance, const float BlendOutTime)
{
	if (!AnimInstance)
	{
		return;
	}

	if (HealingLoopMontage)
	{
		ClearLoopMontage(AnimInstance);
		if (AnimInstance->Montage_IsPlaying(HealingLoopMontage))
		{
			AnimInstance->Montage_Stop(BlendOutTime, HealingLoopMontage);
		}
	}

	if (HealingKneeMontage && AnimInstance->Montage_IsPlaying(HealingKneeMontage))
	{
		AnimInstance->Montage_Stop(BlendOutTime, HealingKneeMontage);
	}
}

void USPHealingAnimComponent::PlayHealingMontage(UAnimMontage* Montage, const EHealingAnimPhase Phase, const float BlendInTime, const bool bStopPreviousMontage)
{
	if (!Montage)
	{
		return;
	}

	UAnimInstance* AnimInstance = GetAnimInstance();
	if (!AnimInstance)
	{
		return;
	}

	CurrentPhase = Phase;

	if (bStopPreviousMontage)
	{
		StopAllHealingMontages(0.f);
	}

	ActiveHealingMontage = Montage;

	HealingMontageEndedDelegate.BindUObject(this, &USPHealingAnimComponent::OnHealingMontageEnded);
	AnimInstance->Montage_SetEndDelegate(HealingMontageEndedDelegate, Montage);

	float Duration = 0.f;
	const float BlendIn = BlendInTime > 0.f ? BlendInTime : HealingMontageBlendOut;
	if (BlendInTime > 0.f)
	{
		const FAlphaBlendArgs BlendInArgs(BlendIn);
		Duration = AnimInstance->Montage_PlayWithBlendIn(
			Montage,
			BlendInArgs,
			1.f,
			EMontagePlayReturnType::MontageLength,
			0.f,
			bStopPreviousMontage);
	}
	else
	{
		Duration = AnimInstance->Montage_Play(Montage, 1.f);
	}

	if (Duration <= 0.f)
	{
		ActiveHealingMontage = nullptr;
		CurrentPhase = EHealingAnimPhase::None;

		if (Phase == EHealingAnimPhase::Knee && HealingLoopMontage && bIsHealing)
		{
			TransitionKneeToLoop();
		}
		else if (Phase == EHealingAnimPhase::Return)
		{
			FinishHealingChannel();
		}
	}
}

void USPHealingAnimComponent::StopActiveHealingMontage(const float BlendOutTime)
{
	UAnimInstance* AnimInstance = GetAnimInstance();
	const float BlendOut = BlendOutTime >= 0.f ? BlendOutTime : HealingMontageBlendOut;
	if (AnimInstance && ActiveHealingMontage && AnimInstance->Montage_IsPlaying(ActiveHealingMontage))
	{
		AnimInstance->Montage_Stop(BlendOut, ActiveHealingMontage);
	}

	ActiveHealingMontage = nullptr;
}

void USPHealingAnimComponent::StopAllHealingMontages(const float BlendOutTime)
{
	UAnimInstance* AnimInstance = GetAnimInstance();
	if (!AnimInstance)
	{
		ActiveHealingMontage = nullptr;
		return;
	}

	const float BlendOut = BlendOutTime >= 0.f ? BlendOutTime : HealingMontageBlendOut;

	auto StopIfPlaying = [AnimInstance, BlendOut](UAnimMontage* Montage)
	{
		if (Montage && AnimInstance->Montage_IsPlaying(Montage))
		{
			AnimInstance->Montage_Stop(BlendOut, Montage);
		}
	};

	StopIfPlaying(HealingKneeMontage);
	StopIfPlaying(HealingLoopMontage);
	StopIfPlaying(HealingReturnMontage);
	ActiveHealingMontage = nullptr;
}

void USPHealingAnimComponent::RestoreMovementAfterHealing()
{
	ASurvivorCharacter* Survivor = GetSurvivor();
	if (!Survivor)
	{
		return;
	}

	if (USkeletalMeshComponent* MeshComp = Survivor->GetMesh())
	{
		if (UAnimInstance* AnimInstance = MeshComp->GetAnimInstance())
		{
			AnimInstance->SetRootMotionMode(ERootMotionMode::RootMotionFromMontagesOnly);
		}
	}

	bHasCachedMovementMode = false;
}

void USPHealingAnimComponent::SetHealingTickEnabled(const bool bEnabled)
{
	SetComponentTickEnabled(bEnabled);
}

void USPHealingAnimComponent::ClearHealingTimers()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReturnMontageFallbackTimer);
	}
}

void USPHealingAnimComponent::HandleReturnMontageFallback()
{
	if (CurrentPhase == EHealingAnimPhase::Return)
	{
		FinishHealingChannel();
	}
}

void USPHealingAnimComponent::UpdateLoopEndTime()
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		LoopEndWorldTime = 0.f;
		return;
	}

	LoopEndWorldTime = World->GetTimeSeconds() + FMath::Max(0.1f, HealingLoopDuration);
}

bool USPHealingAnimComponent::ShouldEndLoopChannel() const
{
	if (!bAutoCompleteLoop || !bIsHealing || CurrentPhase != EHealingAnimPhase::Loop || bIsEndingHealing)
	{
		return false;
	}

	const UWorld* World = GetWorld();
	return World && LoopEndWorldTime > 0.f && World->GetTimeSeconds() >= LoopEndWorldTime;
}

void USPHealingAnimComponent::EnsureMontagesLoaded()
{
	if (!HealingKneeMontage && !HealingKneeMontageAsset.IsNull())
	{
		HealingKneeMontage = HealingKneeMontageAsset.LoadSynchronous();
	}
	if (!HealingLoopMontage && !HealingLoopMontageAsset.IsNull())
	{
		HealingLoopMontage = HealingLoopMontageAsset.LoadSynchronous();
	}
	if (!HealingReturnMontage && !HealingReturnMontageAsset.IsNull())
	{
		HealingReturnMontage = HealingReturnMontageAsset.LoadSynchronous();
	}
}

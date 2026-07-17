#include "Components/SPEscapeLeverComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "AlphaBlend.h"
#include "Characters/Survivor/SurvivorCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Gameplay/Escape/SPEscapeGate.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace SPEscapeLever
{
	static constexpr TCHAR PulldownMontagePath[] =
		TEXT("/Game/Assets/Survivor_Locomotion/Lever/AS_Lever_Pulldown_Montage.AS_Lever_Pulldown_Montage");
	static constexpr TCHAR WatchMontagePath[] =
		TEXT("/Game/Assets/Survivor_Locomotion/Lever/AS_Lever_Watch_Montage.AS_Lever_Watch_Montage");
	static constexpr TCHAR ReturnMontagePath[] =
		TEXT("/Game/Assets/Survivor_Locomotion/Lever/AS_Lever_Return_Montage.AS_Lever_Return_Montage");
}

USPEscapeLeverComponent::USPEscapeLeverComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetIsReplicatedByDefault(true);

	/*
	static ConstructorHelpers::FObjectFinder<UAnimMontage> PulldownFinder(SPEscapeLever::PulldownMontagePath);
	if (PulldownFinder.Succeeded())
	{
		LeverPulldownMontage = PulldownFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UAnimMontage> WatchFinder(SPEscapeLever::WatchMontagePath);
	if (WatchFinder.Succeeded())
	{
		LeverWatchMontage = WatchFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UAnimMontage> ReturnFinder(SPEscapeLever::ReturnMontagePath);
	if (ReturnFinder.Succeeded())
	{
		LeverReturnMontage = ReturnFinder.Object;
	}
	*/
}

void USPEscapeLeverComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(USPEscapeLeverComponent, bIsPullingLever);
}

void USPEscapeLeverComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (CurrentPhase == ELeverAnimPhase::Return)
	{
		UAnimInstance* AnimInstance = GetAnimInstance();
		if (!AnimInstance || !LeverReturnMontage || !AnimInstance->Montage_IsPlaying(LeverReturnMontage))
		{
			ReturnMontageNotPlayingTime += DeltaTime;
			if (ReturnMontageNotPlayingTime >= 0.35f)
			{
				FinishLeverChannel();
			}
		}
		else
		{
			ReturnMontageNotPlayingTime = 0.f;
		}
		return;
	}

	if (GetOwner()->HasAuthority() && ShouldEndWatchChannel())
	{
		EndLeverChannel(true);
		return;
	}

	if (!bIsPullingLever || CurrentPhase != ELeverAnimPhase::Pulldown || bWatchTransitionTriggered || !LeverPulldownMontage)
	{
		return;
	}

	UAnimInstance* AnimInstance = GetAnimInstance();
	if (!AnimInstance || !AnimInstance->Montage_IsPlaying(LeverPulldownMontage))
	{
		return;
	}

	const float MontageLength = LeverPulldownMontage->GetPlayLength();
	const float MontagePosition = AnimInstance->Montage_GetPosition(LeverPulldownMontage);
	const float Crossfade = FMath::Max(FMath::Max(LeverPulldownToWatchBlendIn, LeverPulldownMontage->BlendOut.GetBlendTime()), 0.2f);
	const float BlendIn = FMath::Clamp(Crossfade, 0.05f, MontageLength * 0.5f);

	if (MontagePosition >= MontageLength - BlendIn)
	{
		TransitionPulldownToWatch();
	}
}

void USPEscapeLeverComponent::EnsureMontagesLoaded()
{
	if (!LeverPulldownMontage)
	{
		LeverPulldownMontage = LeverPulldownMontageAsset.IsNull()
			? LoadObject<UAnimMontage>(nullptr, SPEscapeLever::PulldownMontagePath)
			: LeverPulldownMontageAsset.LoadSynchronous();
	}
	if (!LeverWatchMontage)
	{
		LeverWatchMontage = LeverWatchMontageAsset.IsNull()
			? LoadObject<UAnimMontage>(nullptr, SPEscapeLever::WatchMontagePath)
			: LeverWatchMontageAsset.LoadSynchronous();
	}
	if (!LeverReturnMontage)
	{
		LeverReturnMontage = LeverReturnMontageAsset.IsNull()
			? LoadObject<UAnimMontage>(nullptr, SPEscapeLever::ReturnMontagePath)
			: LeverReturnMontageAsset.LoadSynchronous();
	}
}

ASurvivorCharacter* USPEscapeLeverComponent::GetSurvivor() const
{
	return Cast<ASurvivorCharacter>(GetOwner());
}

UAnimInstance* USPEscapeLeverComponent::GetAnimInstance() const
{
	const ASurvivorCharacter* Survivor = GetSurvivor();
	const USkeletalMeshComponent* MeshComp = Survivor ? Survivor->GetMesh() : nullptr;
	return MeshComp ? MeshComp->GetAnimInstance() : nullptr;
}

void USPEscapeLeverComponent::SetLeverTickEnabled(bool bEnabled)
{
	SetComponentTickEnabled(bEnabled);
}

void USPEscapeLeverComponent::BeginLeverChannel(ASPEscapeGate* Gate)
{
	if (!Gate || bIsPullingLever || CurrentPhase == ELeverAnimPhase::Return)
	{
		return;
	}

	if (GetOwner()->HasAuthority())
	{
		Multicast_BeginLeverChannel(Gate);
	}
}

void USPEscapeLeverComponent::EndLeverChannel(bool bCompleted)
{
	if (!IsLeverChannelActive())
	{
		return;
	}

	if (GetOwner()->HasAuthority())
	{
		Multicast_EndLeverChannel(bCompleted);
	}
}

void USPEscapeLeverComponent::CancelLeverChannel()
{
	if (!IsLeverChannelActive())
	{
		return;
	}

	if (GetOwner()->HasAuthority())
	{
		Multicast_CancelLeverChannel();
	}
}

void USPEscapeLeverComponent::OnLeverPullNotify()
{
	if (ASPEscapeGate* Gate = CurrentGate.Get())
	{
		Gate->NotifyLeverPullStart();
	}
}

void USPEscapeLeverComponent::BeginLeverChannelDebug()
{
	if (bIsPullingLever)
	{
		return;
	}

	if (GetOwner()->HasAuthority())
	{
		Multicast_BeginLeverChannelDebug();
	}
}

void USPEscapeLeverComponent::EndLeverChannelDebug()
{
	CancelLeverChannelDebug();
}

void USPEscapeLeverComponent::CancelLeverChannelDebug()
{
	CancelLeverChannel();
}

void USPEscapeLeverComponent::Multicast_BeginLeverChannel_Implementation(ASPEscapeGate* Gate)
{
	if (!Gate || bIsPullingLever || CurrentPhase == ELeverAnimPhase::Return)
	{
		return;
	}

	StartLeverChannelInternal(Gate);
}

void USPEscapeLeverComponent::Multicast_BeginLeverChannelDebug_Implementation()
{
	if (bIsPullingLever)
	{
		return;
	}

	if (CurrentPhase == ELeverAnimPhase::Return)
	{
		AbortReturnMontage();
	}

	StartLeverChannelInternal(nullptr);
}

void USPEscapeLeverComponent::StartLeverChannelInternal(ASPEscapeGate* Gate)
{
	EnsureMontagesLoaded();

	UE_LOG(LogTemp, Warning, TEXT("[LEVER] Start: Pulldown=%d Watch=%d Return=%d"), LeverPulldownMontage != nullptr, LeverWatchMontage != nullptr, LeverReturnMontage != nullptr);

	CurrentGate = Gate;
	bIsPullingLever = true;
	bWatchTransitionTriggered = false;
	bIsEndingChannel = false;
	CurrentPhase = ELeverAnimPhase::None;
	UpdateChannelEndTime();

	EnterLeverState();
	StopAllLeverMontages(0.f);

	if (LeverPulldownMontage)
	{
		PlayLeverMontage(LeverPulldownMontage, ELeverAnimPhase::Pulldown);
		SetLeverTickEnabled(true);

		if (UWorld* World = GetWorld())
		{
			const float PulldownLength = LeverPulldownMontage->GetPlayLength();
			const float Crossfade = FMath::Max(FMath::Max(LeverPulldownToWatchBlendIn, LeverPulldownMontage->BlendOut.GetBlendTime()), 0.2f);
			const float TransitionDelay = FMath::Max(0.05f, PulldownLength - Crossfade);
			FTimerHandle WatchTransitionTimer;
			World->GetTimerManager().SetTimer(WatchTransitionTimer, this, &USPEscapeLeverComponent::TransitionPulldownToWatch, TransitionDelay, false);
			UE_LOG(LogTemp, Warning, TEXT("[LEVER] Pulldown length=%f, scheduled Watch in %f"), PulldownLength, TransitionDelay);
		}
	}
	else if (LeverWatchMontage)
	{
		PlayWatchMontage();
	}
}

void USPEscapeLeverComponent::Multicast_EndLeverChannel_Implementation(bool bCompleted)
{
	if (!IsLeverChannelActive())
	{
		return;
	}

	ExitLeverState(true);
}

void USPEscapeLeverComponent::Multicast_CancelLeverChannel_Implementation()
{
	if (!IsLeverChannelActive())
	{
		return;
	}

	ExitLeverState(false);
}

bool USPEscapeLeverComponent::IsLeverChannelActive() const
{
	return bIsPullingLever
		|| CurrentPhase == ELeverAnimPhase::Pulldown
		|| CurrentPhase == ELeverAnimPhase::Watch;
}

void USPEscapeLeverComponent::EnterLeverState()
{
	ASurvivorCharacter* Survivor = GetSurvivor();
	if (!Survivor)
	{
		return;
	}

	if (const ASPEscapeGate* Gate = CurrentGate.Get())
	{
		const FTransform Anchor = Gate->GetInteractAnchorTransform();
		FVector SnapLocation = Anchor.GetLocation();
		SnapLocation.Z = Survivor->GetActorLocation().Z;
		const FRotator SnapRotation(0.f, Anchor.Rotator().Yaw, 0.f);
		Survivor->SetActorLocationAndRotation(SnapLocation, SnapRotation, false, nullptr, ETeleportType::TeleportPhysics);
	}

	if (USkeletalMeshComponent* MeshComp = Survivor->GetMesh())
	{
		if (UAnimInstance* AnimInstance = MeshComp->GetAnimInstance())
		{
			AnimInstance->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
		}
	}

}

void USPEscapeLeverComponent::RestoreMovementAfterLever()
{
	ASurvivorCharacter* Survivor = GetSurvivor();
	if (!Survivor)
	{
		return;
	}

	bHasCachedMovementMode = false;
}

void USPEscapeLeverComponent::ClearLeverTimers()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReturnMontageFallbackTimer);
	}
}

void USPEscapeLeverComponent::HandleReturnMontageFallback()
{
	if (CurrentPhase == ELeverAnimPhase::Return)
	{
		FinishLeverChannel();
	}
}

void USPEscapeLeverComponent::UpdateChannelEndTime()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (const ASPEscapeGate* Gate = CurrentGate.Get())
	{
		const float Remaining = FMath::Max(0.f, Gate->GetOpenDuration() - Gate->GetOpenProgress());
		ChannelEndWorldTime = World->GetTimeSeconds() + Remaining;
		return;
	}

	ChannelEndWorldTime = World->GetTimeSeconds() + LeverDebugChannelDuration;
}

bool USPEscapeLeverComponent::ShouldEndWatchChannel() const
{
	if (!IsLeverChannelActive() || CurrentPhase != ELeverAnimPhase::Watch || bIsEndingChannel)
	{
		return false;
	}

	const UWorld* World = GetWorld();
	if (World && World->GetTimeSeconds() >= ChannelEndWorldTime)
	{
		return true;
	}

	if (const ASPEscapeGate* Gate = CurrentGate.Get())
	{
		return Gate->IsActivated() || Gate->GetOpenProgress() >= Gate->GetOpenDuration();
	}

	return false;
}

void USPEscapeLeverComponent::BlendOutWatchForReturn(UAnimInstance* AnimInstance, float BlendOutTime)
{
	if (!AnimInstance || !LeverWatchMontage)
	{
		return;
	}

	ClearWatchMontageLoop(AnimInstance);

	if (AnimInstance->Montage_IsPlaying(LeverWatchMontage))
	{
		AnimInstance->Montage_Stop(BlendOutTime, LeverWatchMontage);
	}
}

void USPEscapeLeverComponent::TransitionPulldownToWatch()
{
	UE_LOG(LogTemp, Warning, TEXT("[LEVER] Transition->Watch called: pulling=%d phase=%d watchValid=%d triggered=%d"), bIsPullingLever, (int32)CurrentPhase, LeverWatchMontage != nullptr, bWatchTransitionTriggered);
	if (!bIsPullingLever || CurrentPhase != ELeverAnimPhase::Pulldown || !LeverWatchMontage || bWatchTransitionTriggered)
	{
		return;
	}

	UAnimInstance* AnimInstance = GetAnimInstance();
	if (!AnimInstance)
	{
		return;
	}

	bWatchTransitionTriggered = true;
	SetLeverTickEnabled(true);
	UpdateChannelEndTime();

	const float Crossfade = FMath::Max(FMath::Max(LeverPulldownToWatchBlendIn, LeverPulldownMontage ? LeverPulldownMontage->BlendOut.GetBlendTime() : 0.f), 0.2f);
	PlayWatchMontage(Crossfade, true);
}

void USPEscapeLeverComponent::PlayWatchMontage(float BlendInTime, bool bCrossfadeFromPrevious)
{
	if (!bIsPullingLever || !LeverWatchMontage)
	{
		return;
	}

	UAnimInstance* AnimInstance = GetAnimInstance();
	if (!AnimInstance)
	{
		return;
	}

	CurrentPhase = ELeverAnimPhase::Watch;
	ActiveLeverMontage = LeverWatchMontage;

	if (!bCrossfadeFromPrevious)
	{
		StopAllLeverMontages(0.f);
	}

	LeverMontageEndedDelegate.BindUObject(this, &USPEscapeLeverComponent::OnLeverMontageEnded);
	AnimInstance->Montage_SetEndDelegate(LeverMontageEndedDelegate, LeverWatchMontage);

	float Duration = 0.f;
	if (BlendInTime > 0.f)
	{
		const float BlendIn = FMath::Clamp(BlendInTime, 0.05f, 0.8f);
		const FAlphaBlendArgs BlendInArgs(BlendIn);
		Duration = AnimInstance->Montage_PlayWithBlendIn(LeverWatchMontage, BlendInArgs, 1.f, EMontagePlayReturnType::MontageLength, 0.f, bCrossfadeFromPrevious);
	}
	else
	{
		Duration = AnimInstance->Montage_Play(LeverWatchMontage, 1.f);
	}
	UE_LOG(LogTemp, Warning, TEXT("[LEVER] Watch play Duration=%f blendPath=%d"), Duration, BlendInTime > 0.f ? 1 : 0);

	if (Duration <= 0.f)
	{
		ActiveLeverMontage = nullptr;
		CurrentPhase = ELeverAnimPhase::None;
		return;
	}

	ConfigureWatchMontageLoop(AnimInstance);
	SetLeverTickEnabled(true);
}

void USPEscapeLeverComponent::ConfigureWatchMontageLoop(UAnimInstance* AnimInstance)
{
	if (!AnimInstance || !LeverWatchMontage || LeverWatchMontage->CompositeSections.Num() == 0)
	{
		return;
	}

	const FName SectionName = LeverWatchMontage->GetSectionName(0);
	AnimInstance->Montage_SetNextSection(SectionName, SectionName, LeverWatchMontage);
}

void USPEscapeLeverComponent::ClearWatchMontageLoop(UAnimInstance* AnimInstance)
{
	if (!AnimInstance || !LeverWatchMontage || LeverWatchMontage->CompositeSections.Num() == 0)
	{
		return;
	}

	const FName SectionName = LeverWatchMontage->GetSectionName(0);
	AnimInstance->Montage_SetNextSection(SectionName, NAME_None, LeverWatchMontage);
}

void USPEscapeLeverComponent::RestartWatchMontageLoop()
{
	if (bIsEndingChannel || !bIsPullingLever || CurrentPhase != ELeverAnimPhase::Watch || !LeverWatchMontage)
	{
		return;
	}

	UAnimInstance* AnimInstance = GetAnimInstance();
	if (!AnimInstance)
	{
		return;
	}

	if (AnimInstance->Montage_IsPlaying(LeverWatchMontage))
	{
		const FName SectionName = LeverWatchMontage->GetSectionName(0);
		AnimInstance->Montage_JumpToSection(SectionName, LeverWatchMontage);
		ConfigureWatchMontageLoop(AnimInstance);
		ActiveLeverMontage = LeverWatchMontage;
		return;
	}

	PlayWatchMontage(0.f, false);
}

void USPEscapeLeverComponent::AbortReturnMontage()
{
	SetLeverTickEnabled(false);
	ClearLeverTimers();
	ReturnMontageNotPlayingTime = 0.f;
	StopAllLeverMontages(0.f);
	CurrentPhase = ELeverAnimPhase::None;
	bWatchTransitionTriggered = false;
	bIsEndingChannel = false;
}

void USPEscapeLeverComponent::ExitLeverState(bool bPlayReturnAnim)
{
	if (CurrentPhase == ELeverAnimPhase::Return)
	{
		return;
	}

	bIsEndingChannel = true;
	SetLeverTickEnabled(false);
	bWatchTransitionTriggered = true;
	bIsPullingLever = false;

	if (!bPlayReturnAnim)
	{
		if (ASPEscapeGate* Gate = CurrentGate.Get())
		{
			Gate->NotifyLeverRelease();
		}
	}

	CurrentGate = nullptr;

	if (bPlayReturnAnim && LeverReturnMontage)
	{
		PlayReturnMontage(LeverToReturnBlendIn);
		return;
	}

	FinishLeverChannel();
}

void USPEscapeLeverComponent::PlayReturnMontage(float BlendInTime)
{
	if (!LeverReturnMontage)
	{
		FinishLeverChannel();
		return;
	}

	UAnimInstance* AnimInstance = GetAnimInstance();
	if (!AnimInstance)
	{
		FinishLeverChannel();
		return;
	}

	CurrentPhase = ELeverAnimPhase::Return;
	ActiveLeverMontage = LeverReturnMontage;

	const float BlendIn = FMath::Clamp(BlendInTime > 0.f ? BlendInTime : LeverToReturnBlendIn, 0.05f, 0.3f);
	BlendOutWatchForReturn(AnimInstance, BlendIn);

	LeverMontageEndedDelegate.BindUObject(this, &USPEscapeLeverComponent::OnLeverMontageEnded);
	AnimInstance->Montage_SetEndDelegate(LeverMontageEndedDelegate, LeverReturnMontage);

	const FAlphaBlendArgs BlendInArgs(BlendIn);
	const float Duration = AnimInstance->Montage_PlayWithBlendIn(LeverReturnMontage, BlendInArgs, 1.f, EMontagePlayReturnType::MontageLength, 0.f, false);
	UE_LOG(LogTemp, Warning, TEXT("[LEVER] Return play Duration=%f"), Duration);

	if (Duration <= 0.f)
	{
		ActiveLeverMontage = nullptr;
		FinishLeverChannel();
		return;
	}

	ReturnMontageNotPlayingTime = 0.f;
	SetLeverTickEnabled(true);
	ClearLeverTimers();
	if (UWorld* World = GetWorld())
	{
		const float FallbackDelay = Duration + BlendIn + 0.2f;
		World->GetTimerManager().SetTimer(
			ReturnMontageFallbackTimer,
			this,
			&USPEscapeLeverComponent::HandleReturnMontageFallback,
			FallbackDelay,
			false);
	}
}

void USPEscapeLeverComponent::FinishLeverChannel()
{
	SetLeverTickEnabled(false);
	ClearLeverTimers();
	ReturnMontageNotPlayingTime = 0.f;
	StopAllLeverMontages(0.f);

	bIsPullingLever = false;
	bWatchTransitionTriggered = false;
	bIsEndingChannel = false;
	CurrentGate = nullptr;
	CurrentPhase = ELeverAnimPhase::None;
	ChannelEndWorldTime = 0.f;

	if (ASurvivorCharacter* Survivor = GetSurvivor())
	{
		RestoreMovementAfterLever();

		Survivor->NotifyLeverChannelEnded();
	}
}

void USPEscapeLeverComponent::PlayLeverMontage(UAnimMontage* Montage, ELeverAnimPhase Phase, float BlendInTime, bool bStopPreviousMontage)
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
		StopAllLeverMontages(0.f);
	}

	ActiveLeverMontage = Montage;

	LeverMontageEndedDelegate.BindUObject(this, &USPEscapeLeverComponent::OnLeverMontageEnded);
	AnimInstance->Montage_SetEndDelegate(LeverMontageEndedDelegate, Montage);

	float Duration = 0.f;
	const float BlendIn = BlendInTime > 0.f ? BlendInTime : LeverMontageBlendOut;
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
		ActiveLeverMontage = nullptr;
		CurrentPhase = ELeverAnimPhase::None;

		if (Phase == ELeverAnimPhase::Pulldown && LeverWatchMontage && bIsPullingLever)
		{
			TransitionPulldownToWatch();
		}
		else if (Phase == ELeverAnimPhase::Return)
		{
			FinishLeverChannel();
		}
	}
}

void USPEscapeLeverComponent::StopActiveLeverMontage(float BlendOutTime)
{
	UAnimInstance* AnimInstance = GetAnimInstance();
	const float BlendOut = BlendOutTime >= 0.f ? BlendOutTime : LeverMontageBlendOut;
	if (AnimInstance && ActiveLeverMontage && AnimInstance->Montage_IsPlaying(ActiveLeverMontage))
	{
		AnimInstance->Montage_Stop(BlendOut, ActiveLeverMontage);
	}

	ActiveLeverMontage = nullptr;
}

void USPEscapeLeverComponent::StopAllLeverMontages(float BlendOutTime)
{
	UAnimInstance* AnimInstance = GetAnimInstance();
	if (!AnimInstance)
	{
		ActiveLeverMontage = nullptr;
		return;
	}

	const float BlendOut = BlendOutTime >= 0.f ? BlendOutTime : LeverMontageBlendOut;

	auto StopIfPlaying = [AnimInstance, BlendOut](UAnimMontage* Montage)
	{
		if (Montage && AnimInstance->Montage_IsPlaying(Montage))
		{
			AnimInstance->Montage_Stop(BlendOut, Montage);
		}
	};

	StopIfPlaying(LeverPulldownMontage);
	StopIfPlaying(LeverWatchMontage);
	StopIfPlaying(LeverReturnMontage);
	ActiveLeverMontage = nullptr;
}

void USPEscapeLeverComponent::OnLeverMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	UE_LOG(LogTemp, Warning, TEXT("[LEVER] MontageEnded: isPulldown=%d isWatch=%d isReturn=%d phase=%d interrupted=%d"), Montage == LeverPulldownMontage, Montage == LeverWatchMontage, Montage == LeverReturnMontage, (int32)CurrentPhase, bInterrupted);
	if (Montage == LeverReturnMontage && CurrentPhase != ELeverAnimPhase::Return)
	{
		return;
	}

	if (Montage == LeverPulldownMontage && CurrentPhase != ELeverAnimPhase::Pulldown)
	{
		return;
	}

	if (Montage == LeverWatchMontage && CurrentPhase != ELeverAnimPhase::Watch)
	{
		return;
	}

	if (CurrentPhase == ELeverAnimPhase::Return)
	{
		if (Montage == LeverReturnMontage)
		{
			ActiveLeverMontage = nullptr;
			FinishLeverChannel();
		}
		return;
	}

	if (!bIsPullingLever)
	{
		return;
	}

	if (bWatchTransitionTriggered && Montage == LeverPulldownMontage)
	{
		return;
	}

	if (CurrentPhase == ELeverAnimPhase::Pulldown && Montage == LeverPulldownMontage)
	{
		TransitionPulldownToWatch();
		return;
	}

	if (CurrentPhase == ELeverAnimPhase::Watch && Montage == LeverWatchMontage)
	{
		if (bIsEndingChannel)
		{
			return;
		}

		RestartWatchMontageLoop();
	}
}

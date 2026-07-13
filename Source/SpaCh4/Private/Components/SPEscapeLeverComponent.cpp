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
	const float BlendIn = FMath::Clamp(LeverPulldownToWatchBlendIn, 0.05f, MontageLength * 0.5f);

	if (MontagePosition >= MontageLength - BlendIn)
	{
		TransitionPulldownToWatch();
	}
}

void USPEscapeLeverComponent::EnsureMontagesLoaded()
{
	if (!LeverPulldownMontage)
	{
		LeverPulldownMontage = LoadObject<UAnimMontage>(nullptr, SPEscapeLever::PulldownMontagePath);
	}
	if (!LeverWatchMontage)
	{
		LeverWatchMontage = LoadObject<UAnimMontage>(nullptr, SPEscapeLever::WatchMontagePath);
	}
	if (!LeverReturnMontage)
	{
		LeverReturnMontage = LoadObject<UAnimMontage>(nullptr, SPEscapeLever::ReturnMontagePath);
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

	ExitLeverState(true);
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

	if (UCharacterMovementComponent* MoveComp = Survivor->GetCharacterMovement())
	{
		if (MoveComp->MovementMode != MOVE_None)
		{
			CachedMovementMode = MoveComp->MovementMode;
			CachedGravityScale = MoveComp->GravityScale;
			bHasCachedMovementMode = true;
		}
		else if (!bHasCachedMovementMode)
		{
			CachedMovementMode = MOVE_Walking;
			CachedGravityScale = MoveComp->GravityScale;
			bHasCachedMovementMode = true;
		}

		MoveComp->StopMovementImmediately();
		MoveComp->Velocity = FVector::ZeroVector;
		MoveComp->SetMovementMode(MOVE_None);
	}

	if (USkeletalMeshComponent* MeshComp = Survivor->GetMesh())
	{
		if (UAnimInstance* AnimInstance = MeshComp->GetAnimInstance())
		{
			AnimInstance->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
		}
	}

	if (AController* Controller = Survivor->GetController())
	{
		Controller->SetIgnoreMoveInput(true);
	}
}

void USPEscapeLeverComponent::RestoreMovementAfterLever()
{
	ASurvivorCharacter* Survivor = GetSurvivor();
	if (!Survivor)
	{
		return;
	}

	if (UCharacterMovementComponent* MoveComp = Survivor->GetCharacterMovement())
	{
		if (bHasCachedMovementMode)
		{
			MoveComp->SetMovementMode(CachedMovementMode);
			MoveComp->GravityScale = CachedGravityScale;
			bHasCachedMovementMode = false;
		}
		else if (MoveComp->MovementMode == MOVE_None)
		{
			MoveComp->SetMovementMode(MOVE_Walking);
		}
	}
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

	PlayWatchMontage(LeverPulldownToWatchBlendIn, true);
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
		const float BlendIn = FMath::Clamp(BlendInTime, 0.05f, 0.25f);
		const FAlphaBlendArgs BlendInArgs(BlendIn);
		Duration = AnimInstance->Montage_PlayWithBlendIn(
			LeverWatchMontage,
			BlendInArgs,
			1.f,
			EMontagePlayReturnType::MontageLength,
			0.f,
			bCrossfadeFromPrevious);
	}
	else
	{
		Duration = AnimInstance->Montage_Play(LeverWatchMontage, 1.f);
	}

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
	const float Duration = AnimInstance->Montage_PlayWithBlendIn(
		LeverReturnMontage,
		BlendInArgs,
		1.f,
		EMontagePlayReturnType::MontageLength,
		0.f,
		false);

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

		if (AController* Controller = Survivor->GetController())
		{
			Controller->ResetIgnoreMoveInput();
		}

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

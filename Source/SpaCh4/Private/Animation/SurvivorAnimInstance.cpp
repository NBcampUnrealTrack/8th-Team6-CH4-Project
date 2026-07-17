#include "Animation/SurvivorAnimInstance.h"

#include "Animation/AnimClassInterface.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSubsystem_Tag.h"
#include "Characters/Survivor/SurvivorCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/UnrealType.h"

namespace SurvivorAnimTags
{
	static const FName DownedBlend(TEXT("DownedBlend"));
	static const FName ProneBS(TEXT("ProneBS"));
}

namespace SurvivorPronePaths
{
	static const TCHAR* Idle = TEXT("/Game/Assets/Survivor_Locomotion/Prone/New/anim_Prone_Idle.anim_Prone_Idle");
	static const TCHAR* Fwd = TEXT("/Game/Assets/Survivor_Locomotion/Prone/New/anim_Prone_Fwd_Loop_R.anim_Prone_Fwd_Loop_R");
	static const TCHAR* Carried = TEXT("/Game/Assets/Survivor_Locomotion/Prone/SK_Survivor_V2_Skeleton_Sequence.SK_Survivor_V2_Skeleton_Sequence");
}

namespace SurvivorAnimNodeUtil
{
	void* FindTaggedNodeMemory(UAnimInstance* Instance, FName Tag, UScriptStruct*& OutStruct)
	{
		OutStruct = nullptr;
		if (!Instance)
		{
			return nullptr;
		}

		IAnimClassInterface* AnimClass = IAnimClassInterface::GetFromClass(Instance->GetClass());
		if (!AnimClass)
		{
			return nullptr;
		}

		const FAnimSubsystem_Tag* TagSubsystem = AnimClass->FindSubsystem<FAnimSubsystem_Tag>();
		if (!TagSubsystem)
		{
			return nullptr;
		}

		const int32 NodeIndex = TagSubsystem->FindNodeIndexByTag(Tag);
		if (NodeIndex == INDEX_NONE)
		{
			return nullptr;
		}

		const TArray<FStructProperty*>& NodeProperties = AnimClass->GetAnimNodeProperties();
		if (!NodeProperties.IsValidIndex(NodeIndex) || !NodeProperties[NodeIndex])
		{
			return nullptr;
		}

		FStructProperty* NodeProp = NodeProperties[NodeIndex];
		OutStruct = NodeProp->Struct;
		return NodeProp->ContainerPtrToValuePtr<void>(Instance);
	}

	bool SetBool(void* Node, UScriptStruct* Struct, FName Name, bool bValue)
	{
		if (!Node || !Struct)
		{
			return false;
		}

		FBoolProperty* BoolProp = CastField<FBoolProperty>(Struct->FindPropertyByName(Name));
		if (!BoolProp)
		{
			return false;
		}

		BoolProp->SetPropertyValue_InContainer(Node, bValue);
		return true;
	}

	bool SetFloat(void* Node, UScriptStruct* Struct, FName Name, float Value)
	{
		if (!Node || !Struct)
		{
			return false;
		}

		FFloatProperty* FloatProp = CastField<FFloatProperty>(Struct->FindPropertyByName(Name));
		if (!FloatProp)
		{
			return false;
		}

		FloatProp->SetPropertyValue_InContainer(Node, Value);
		return true;
	}
}

void USurvivorAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	EnsureProneAnimsLoaded();
}

void USurvivorAnimInstance::EnsureProneAnimsLoaded()
{
	if (!CachedProneIdle)
	{
		CachedProneIdle = LoadProneAnim(ProneIdleAnim, SurvivorPronePaths::Idle);
	}
	if (!CachedProneFwd)
	{
		CachedProneFwd = LoadProneAnim(ProneFwdAnim, SurvivorPronePaths::Fwd);
	}
	if (!CachedCarriedPose)
	{
		CachedCarriedPose = LoadProneAnim(CarriedPoseAnim, SurvivorPronePaths::Carried);
	}
}

void USurvivorAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	APawn* Pawn = TryGetPawnOwner();
	const ASurvivorCharacter* Survivor = Cast<ASurvivorCharacter>(Pawn);
	if (!Survivor)
	{
		bIsDowned = false;
		bIsCarried = false;
		ProneSpeed = 0.f;
		GroundSpeed = 0.f;
		bShouldMove = false;
		StopProneSlotPlayback();
		UpdateDownedAnimGraphNodes();
		return;
	}

	const ESurvivorState State = Survivor->GetSurvivorState();
	bIsCarried = State == ESurvivorState::Carried;
	bIsDowned = State == ESurvivorState::Downed || bIsCarried;

	const UCharacterMovementComponent* MoveComp = Survivor->GetCharacterMovement();
	const FVector Velocity = MoveComp ? MoveComp->Velocity : FVector::ZeroVector;
	GroundSpeed = Velocity.Size2D();
	bShouldMove = GroundSpeed > 3.f;

	if (bIsDowned)
	{
		// Carried uses a dedicated fixed pose. Downed crawl uses planar speed.
		ProneSpeed = bIsCarried ? 0.f : GroundSpeed;
		UpdateProneSlotPlayback();
	}
	else
	{
		ProneSpeed = 0.f;
		StopProneSlotPlayback();
	}

	UpdateDownedAnimGraphNodes();
}

UAnimSequence* USurvivorAnimInstance::LoadProneAnim(const TSoftObjectPtr<UAnimSequence>& Soft, const TCHAR* FallbackPath) const
{
	if (UAnimSequence* Loaded = Soft.LoadSynchronous())
	{
		return Loaded;
	}

	return LoadObject<UAnimSequence>(nullptr, FallbackPath);
}

UAnimSequence* USurvivorAnimInstance::ResolveProneSequence() const
{
	const_cast<USurvivorAnimInstance*>(this)->EnsureProneAnimsLoaded();

	if (bIsCarried)
	{
		return CachedCarriedPose ? CachedCarriedPose.Get() : CachedProneIdle.Get();
	}

	if (ProneSpeed > ProneMoveSpeedThreshold)
	{
		return CachedProneFwd ? CachedProneFwd.Get() : CachedProneIdle.Get();
	}

	return CachedProneIdle.Get();
}

void USurvivorAnimInstance::UpdateProneSlotPlayback()
{
	EnsureProneAnimsLoaded();

	UAnimSequence* Desired = ResolveProneSequence();
	if (!Desired || ProneSlotName.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("SurvivorAnim: sequence missing (idle=%s fwd=%s carried=%s)"),
			*GetNameSafe(CachedProneIdle),
			*GetNameSafe(CachedProneFwd),
			*GetNameSafe(CachedCarriedPose));
		return;
	}

	if (bProneSlotPlaying && ActiveProneSlotSequence == Desired && ActiveProneSlotMontage
		&& Montage_IsPlaying(ActiveProneSlotMontage))
	{
		return;
	}

	if (ActiveProneSlotMontage && Montage_IsPlaying(ActiveProneSlotMontage))
	{
		Montage_Stop(ProneSlotBlendOut, ActiveProneSlotMontage);
	}

	// NOTE: LoopCount 0 makes FAnimSegment length 0 and PlaySlot fails. Use a large count.
	UAnimMontage* Montage = PlaySlotAnimationAsDynamicMontage(
		Desired,
		ProneSlotName,
		ProneSlotBlendIn,
		ProneSlotBlendOut,
		1.f,
		100000);

	if (!Montage)
	{
		UE_LOG(LogTemp, Warning, TEXT("SurvivorAnim: failed to play prone slot anim %s on slot %s"),
			*GetNameSafe(Desired), *ProneSlotName.ToString());
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("SurvivorAnim: playing %s on %s (%s)"),
			*GetNameSafe(Desired),
			*ProneSlotName.ToString(),
			bIsCarried ? TEXT("Carried") : TEXT("Downed"));
	}

	ActiveProneSlotMontage = Montage;
	ActiveProneSlotSequence = Desired;
	bProneSlotPlaying = Montage != nullptr;
}

void USurvivorAnimInstance::StopProneSlotPlayback()
{
	if (!bProneSlotPlaying && !ActiveProneSlotMontage)
	{
		return;
	}

	if (ActiveProneSlotMontage)
	{
		Montage_Stop(ProneSlotBlendOut, ActiveProneSlotMontage);
	}
	else
	{
		StopSlotAnimation(ProneSlotBlendOut, ProneSlotName);
	}

	ActiveProneSlotMontage = nullptr;
	ActiveProneSlotSequence = nullptr;
	bProneSlotPlaying = false;
}

void USurvivorAnimInstance::UpdateDownedAnimGraphNodes()
{
	using namespace SurvivorAnimNodeUtil;

	// BlendListByBool (tag DownedBlend): true => Prone BS, false => Slot/MM.
	// Pin defaults may overwrite; Slot montage is the primary Downed visual.
	UScriptStruct* BlendStruct = nullptr;
	if (void* BlendNode = FindTaggedNodeMemory(this, SurvivorAnimTags::DownedBlend, BlendStruct))
	{
		SetBool(BlendNode, BlendStruct, TEXT("bActiveValue"), bIsDowned);
	}

	UScriptStruct* ProneStruct = nullptr;
	if (void* ProneNode = FindTaggedNodeMemory(this, SurvivorAnimTags::ProneBS, ProneStruct))
	{
		if (!SetFloat(ProneNode, ProneStruct, TEXT("X"), ProneSpeed))
		{
			SetFloat(ProneNode, ProneStruct, TEXT("XAxis"), ProneSpeed);
		}
	}
}

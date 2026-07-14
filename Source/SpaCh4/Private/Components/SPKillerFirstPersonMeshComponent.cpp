#include "Components/SPKillerFirstPersonMeshComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/KillerAnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Characters/Base/CharacterBase.h"
#include "Characters/Killer/KillerCharacter.h"
#include "Components/MeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Character.h"
#include "GameFramework/SpringArmComponent.h"
#include "Systems/Data/KillerData.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"

USPKillerFirstPersonMeshComponent::USPKillerFirstPersonMeshComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;

	FallbackBodyMesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(
		TEXT("/Game/Assets/Killer/New/Meshes/SKM_Rogue_Main.SKM_Rogue_Main")));
	FallbackAnimInstanceClass = TSoftClassPtr<UAnimInstance>(FSoftClassPath(
		TEXT("/Game/Assets/Killer_Locomotion/BP/ABP_Killer.ABP_Killer_C")));
}

void USPKillerFirstPersonMeshComponent::BeginPlay()
{
	Super::BeginPlay();

	if (APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		OwnerPawn->ReceiveControllerChangedDelegate.AddUniqueDynamic(
			this, &USPKillerFirstPersonMeshComponent::HandleOwnerControllerChanged);
	}

	ApplySpringArmDefaults();
	ApplyCameraDefaults();
	ScheduleSetup();
}

void USPKillerFirstPersonMeshComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		OwnerPawn->ReceiveControllerChangedDelegate.RemoveDynamic(
			this, &USPKillerFirstPersonMeshComponent::HandleOwnerControllerChanged);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FirstPersonSetupTimer);
	}

	Super::EndPlay(EndPlayReason);
}

void USPKillerFirstPersonMeshComponent::HandleOwnerControllerChanged(
	APawn* Pawn,
	AController* OldController,
	AController* NewController)
{
	if (Pawn == GetOwner())
	{
		ScheduleSetup();
	}
}

ACharacter* USPKillerFirstPersonMeshComponent::GetOwnerCharacter() const
{
	return Cast<ACharacter>(GetOwner());
}

const UKillerData* USPKillerFirstPersonMeshComponent::ResolveKillerData() const
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

USkeletalMesh* USPKillerFirstPersonMeshComponent::ResolveBodyMeshAsset() const
{
	if (const UKillerData* Data = ResolveKillerData())
	{
		if (USkeletalMesh* Mesh = Data->BodyMesh.LoadSynchronous())
		{
			return Mesh;
		}
	}

	if (USkeletalMesh* Mesh = FallbackBodyMesh.LoadSynchronous())
	{
		return Mesh;
	}

	return nullptr;
}

TSubclassOf<UAnimInstance> USPKillerFirstPersonMeshComponent::ResolveAnimInstanceClass() const
{
	if (const UKillerData* Data = ResolveKillerData())
	{
		if (UClass* AnimClass = Data->AnimInstanceClass.LoadSynchronous())
		{
			return AnimClass;
		}
	}

	if (UClass* AnimClass = FallbackAnimInstanceClass.LoadSynchronous())
	{
		return AnimClass;
	}

	return nullptr;
}

void USPKillerFirstPersonMeshComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdateUpperBodyPitchFollow();
}

static float NormalizePitchDegrees(float Pitch)
{
	const float Normalized = FMath::UnwindDegrees(Pitch);
	return FMath::Clamp(Normalized, -89.f, 89.f);
}

void USPKillerFirstPersonMeshComponent::UpdateUpperBodyPitchFollow()
{
	if (!bEnableUpperBodyPitchFollow)
	{
		return;
	}

	ACharacter* OwnerCharacter = GetOwnerCharacter();
	if (!OwnerCharacter || !OwnerCharacter->IsLocallyControlled())
	{
		return;
	}

	USkeletalMeshComponent* BodyMesh = OwnerCharacter->GetMesh();
	AController* Controller = OwnerCharacter->GetController();
	if (!BodyMesh || !Controller)
	{
		return;
	}

	const float TargetPitch = FMath::Clamp(
		NormalizePitchDegrees(Controller->GetControlRotation().Pitch),
		UpperBodyPitchMin,
		UpperBodyPitchMax) * UpperBodyPitchFollowStrength;

	const float DeltaTime = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.f;
	SmoothedUpperBodyPitch = (DeltaTime > 0.f)
		? FMath::FInterpTo(SmoothedUpperBodyPitch, TargetPitch, DeltaTime, UpperBodyPitchInterpSpeed)
		: TargetPitch;

	if (UKillerAnimInstance* KillerAnim = Cast<UKillerAnimInstance>(BodyMesh->GetAnimInstance()))
	{
		KillerAnim->SetFPViewPitch(SmoothedUpperBodyPitch);
	}
	else if (UAnimInstance* AnimInstance = BodyMesh->GetAnimInstance())
	{
		if (FFloatProperty* PitchProperty = FindFProperty<FFloatProperty>(
			AnimInstance->GetClass(), TEXT("FPViewPitch")))
		{
			PitchProperty->SetPropertyValue_InContainer(AnimInstance, SmoothedUpperBodyPitch);
		}
	}
}

void USPKillerFirstPersonMeshComponent::ApplySpringArmDefaults()
{
	ACharacterBase* OwnerCharacter = Cast<ACharacterBase>(GetOuter());
	if (!OwnerCharacter)
	{
		return;
	}

	if (USpringArmComponent* SpringArm = OwnerCharacter->GetSpringArmComponent())
	{
		SpringArm->TargetArmLength = SpringArmLength;
		SpringArm->bDoCollisionTest = bSpringArmCollisionTest;
		SpringArm->bUsePawnControlRotation = bSpringArmUsePawnControlRotation;
		SpringArm->bEnableCameraLag = bSpringArmEnableCameraLag;
		SpringArm->bInheritRoll = bSpringArmInheritRoll;
	}
}

void USPKillerFirstPersonMeshComponent::ApplyCameraDefaults()
{
	ACharacterBase* OwnerCharacter = Cast<ACharacterBase>(GetOuter());
	if (!OwnerCharacter || !bUsePerspectiveProjection)
	{
		return;
	}

	if (UCameraComponent* Camera = OwnerCharacter->GetCameraComponent())
	{
		Camera->SetProjectionMode(ECameraProjectionMode::Perspective);
	}
}

void USPKillerFirstPersonMeshComponent::EnsureAnimationSetup()
{
	ACharacter* OwnerCharacter = GetOwnerCharacter();
	USkeletalMeshComponent* SkelMesh = OwnerCharacter ? OwnerCharacter->GetMesh() : nullptr;
	if (!SkelMesh)
	{
		return;
	}

	if (!SkelMesh->GetSkeletalMeshAsset())
	{
		if (USkeletalMesh* BodyMesh = ResolveBodyMeshAsset())
		{
			SkelMesh->SetSkeletalMesh(BodyMesh);
		}
	}

	if (!SkelMesh->GetAnimClass())
	{
		if (TSubclassOf<UAnimInstance> AnimClass = ResolveAnimInstanceClass())
		{
			SkelMesh->SetAnimInstanceClass(AnimClass);
		}
	}
}

void USPKillerFirstPersonMeshComponent::RefreshFirstPersonView()
{
	EnsureAnimationSetup();

	ACharacter* OwnerCharacter = GetOwnerCharacter();
	ACharacterBase* OwnerBase = Cast<ACharacterBase>(OwnerCharacter);
	USkeletalMeshComponent* SkelMesh = OwnerCharacter ? OwnerCharacter->GetMesh() : nullptr;
	USpringArmComponent* SpringArm = OwnerBase ? OwnerBase->GetSpringArmComponent() : nullptr;
	if (!SkelMesh || !SpringArm)
	{
		return;
	}

	FName AttachName = CameraAttachBoneName;
	if (AttachName.IsNone() || !SkelMesh->DoesSocketExist(AttachName))
	{
		for (const FName& Candidate : CameraAttachBoneFallbacks)
		{
			if (SkelMesh->DoesSocketExist(Candidate))
			{
				AttachName = Candidate;
				break;
			}
		}
	}

	if (!AttachName.IsNone() && SkelMesh->DoesSocketExist(AttachName))
	{
		SpringArm->AttachToComponent(
			SkelMesh,
			FAttachmentTransformRules::SnapToTargetNotIncludingScale,
			AttachName);
		SpringArm->SetRelativeLocation(CameraRelativeOffset);
	}

	SkelMesh->SetVisibility(true, true);
	SkelMesh->SetOnlyOwnerSee(false);
	SkelMesh->SetOwnerNoSee(false);

	if (OwnerCharacter->IsLocallyControlled() && bHideOwnerShadow)
	{
		SkelMesh->SetCastShadow(false);
	}

	for (UMeshComponent* MeshComp : OwnerHiddenMeshComponents)
	{
		if (MeshComp)
		{
			MeshComp->SetVisibility(true, true);
			if (OwnerCharacter->IsLocallyControlled() && bHideOwnerShadow)
			{
				MeshComp->SetCastShadow(false);
			}
		}
	}
}

void USPKillerFirstPersonMeshComponent::TrySetupFirstPersonView()
{
	RefreshFirstPersonView();

	ACharacter* OwnerCharacter = GetOwnerCharacter();
	USkeletalMeshComponent* SkelMesh = OwnerCharacter ? OwnerCharacter->GetMesh() : nullptr;
	if (!SkelMesh)
	{
		return;
	}

	if (SkelMesh->GetSkeletalMeshAsset() || !OwnerCharacter->IsLocallyControlled())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(FirstPersonSetupTimer);
		}
	}
}

void USPKillerFirstPersonMeshComponent::ScheduleSetup()
{
	UWorld* World = GetWorld();
	ACharacter* OwnerCharacter = GetOwnerCharacter();
	USkeletalMeshComponent* SkelMesh = OwnerCharacter ? OwnerCharacter->GetMesh() : nullptr;
	if (!World || !SkelMesh)
	{
		return;
	}

	TrySetupFirstPersonView();

	if (!FirstPersonSetupTimer.IsValid())
	{
		World->GetTimerManager().SetTimer(
			FirstPersonSetupTimer,
			FTimerDelegate::CreateUObject(this, &USPKillerFirstPersonMeshComponent::TrySetupFirstPersonView),
			SetupRetryInterval,
			true);
	}
}

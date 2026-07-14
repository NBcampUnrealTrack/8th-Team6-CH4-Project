#include "Characters/Killer/KillerCharacter.h"
#include "Systems/Data/KillerData.h"
#include "Animation/AnimInstance.h"
#include "Animation/KillerAnimInstance.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "EnhancedInputComponent.h"
#include "Data/SPInputConfigData.h"
#include "EnhancedInputSubsystems.h"
#include "Camera/CameraComponent.h"
#include "Characters/Survivor/SurvivorCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SPKillerCarryAnimComponent.h"
#include "Components/SPKillerAttackAnimComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Gameplay/Cage/Cage.h"
#include "Type/SPGameplayTag.h"
#include "Engine/SkeletalMesh.h"
#include "ReferenceSkeleton.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "UObject/UnrealType.h"

AKillerCharacter::AKillerCharacter()
{
    PrimaryActorTick.bCanEverTick = true;

    bUseControllerRotationYaw = true;         
    if (GetCharacterMovement())
    {
        GetCharacterMovement()->bOrientRotationToMovement = false;
    }
    
    charTag.AddTag(SPGameplayTags::Character::Killer);
    CarryAnimComponent = CreateDefaultSubobject<USPKillerCarryAnimComponent>(TEXT("CarryAnimComponent"));
    AttackAnimComp = CreateDefaultSubobject<USPKillerAttackAnimComponent>(TEXT("AttackAnim"));
}

void AKillerCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    UpdateUpperBodyPitchFollow(DeltaSeconds);
}

void AKillerCharacter::UpdateUpperBodyPitchFollow(float DeltaSeconds)
{
    if (!bEnableUpperBodyPitchFollow || !IsLocallyControlled() || !Controller || !GetMesh())
    {
        return;
    }

    const float ViewPitch = FMath::Clamp(
        FMath::UnwindDegrees(Controller->GetControlRotation().Pitch),
        UpperBodyPitchMin,
        UpperBodyPitchMax);
    const float TargetPitch = ViewPitch * UpperBodyPitchFollowStrength;
    SmoothedUpperBodyPitch = FMath::FInterpTo(
        SmoothedUpperBodyPitch,
        TargetPitch,
        DeltaSeconds,
        UpperBodyPitchInterpSpeed);

    UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
    if (UKillerAnimInstance* KillerAnimInstance = Cast<UKillerAnimInstance>(AnimInstance))
    {
        KillerAnimInstance->SetFPViewPitch(SmoothedUpperBodyPitch);
        return;
    }

    // ABP_Killer currently derives directly from UAnimInstance, so keep the
    // Blueprint variable path working without forcing an AnimBP reparent.
    if (AnimInstance)
    {
        if (FFloatProperty* PitchProperty = FindFProperty<FFloatProperty>(
            AnimInstance->GetClass(), TEXT("FPViewPitch")))
        {
            PitchProperty->SetPropertyValue_InContainer(AnimInstance, SmoothedUpperBodyPitch);
        }
    }
}

void AKillerCharacter::NotifyControllerChanged()
{
    Super::NotifyControllerChanged();
}

void AKillerCharacter::PostInitializeComponents()
{
    Super::PostInitializeComponents();
    if (GetCharacterMovement())
    {
        GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    }
}

void AKillerCharacter::ApplyFirstPersonArmVisibility(
    USkeletalMeshComponent* TargetMesh,
    const TArray<FName>& VisibleRootBones) const
{
    const USkeletalMesh* SkeletalMeshAsset = TargetMesh ? TargetMesh->GetSkeletalMeshAsset() : nullptr;
    if (!SkeletalMeshAsset)
    {
        return;
    }

    const FReferenceSkeleton& RefSkel = SkeletalMeshAsset->GetRefSkeleton();
    TSet<FName> VisibleRoots;
    for (const FName& BoneName : VisibleRootBones)
    {
        if (!BoneName.IsNone())
        {
            VisibleRoots.Add(BoneName);
        }
    }

    if (VisibleRoots.IsEmpty())
    {
        static const FName DefaultRoots[] = {
            TEXT("clavicle_l"), TEXT("upperarm_l"), TEXT("lowerarm_l"), TEXT("hand_l"),
            TEXT("clavicle_r"), TEXT("upperarm_r"), TEXT("lowerarm_r"), TEXT("hand_r"),
        };
        for (const FName& BoneName : DefaultRoots)
        {
            VisibleRoots.Add(BoneName);
        }
    }

    // A visible arm still needs its root/pelvis/spine ancestors to carry transforms.
    // Hiding those ancestors at zero scale also hides every arm below them.
    TSet<FName> RequiredAncestorBones;
    for (const FName& VisibleRoot : VisibleRoots)
    {
        int32 BoneIndex = RefSkel.FindBoneIndex(VisibleRoot);
        while (BoneIndex != INDEX_NONE)
        {
            RequiredAncestorBones.Add(RefSkel.GetBoneName(BoneIndex));
            BoneIndex = RefSkel.GetParentIndex(BoneIndex);
        }
    }

    auto IsBoneVisible = [&](int32 BoneIndex) -> bool
    {
        if (RequiredAncestorBones.Contains(RefSkel.GetBoneName(BoneIndex)))
        {
            return true;
        }

        int32 Current = BoneIndex;
        while (Current != INDEX_NONE)
        {
            if (VisibleRoots.Contains(RefSkel.GetBoneName(Current)))
            {
                return true;
            }
            Current = RefSkel.GetParentIndex(Current);
        }
        return false;
    };

    for (int32 BoneIndex = 0; BoneIndex < RefSkel.GetNum(); ++BoneIndex)
    {
        const FName BoneName = RefSkel.GetBoneName(BoneIndex);
        if (!IsBoneVisible(BoneIndex))
        {
            TargetMesh->HideBoneByName(BoneName, EPhysBodyOp::PBO_None);
        }
        else
        {
            TargetMesh->UnHideBoneByName(BoneName);
        }
    }
}

void AKillerCharacter::SetupKillerFirstPersonCamera()
{
    USkeletalMeshComponent* SkelMesh = GetMesh();
    if (!SkelMesh || !SpringArm)
    {
        return;
    }

    // Preserve the BP-authored camera distance/angle. Disabling lag is enough to keep
    // that camera rig rigidly fixed to the animated head bone while moving.
    SpringArm->bEnableCameraLag = false;
    SpringArm->bEnableCameraRotationLag = false;

    FName AttachName = CameraAttachBoneName;
    if (AttachName.IsNone() || !SkelMesh->DoesSocketExist(AttachName))
    {
        static const FName FallbackBones[] = {
            TEXT("head"), TEXT("Head"), TEXT("neck_01"), TEXT("Neck"),
        };
        for (const FName& Candidate : FallbackBones)
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

    if (!IsLocallyControlled())
    {
        if (FirstPersonArmsMesh)
        {
            FirstPersonArmsMesh->SetVisibility(false, true);
        }
        return;
    }

    // Only hide the body from the owner when a FP arms mesh is available to replace it.
    // Otherwise attack/groggy montages play on an invisible mesh and look like they "don't work".
    const bool bUseFpArmsOnly = bShowFirstPersonArmsOnly && FirstPersonArmsMesh != nullptr;
    SkelMesh->SetOwnerNoSee(bUseFpArmsOnly);
    if (bHideOwnerShadow && bUseFpArmsOnly)
    {
        SkelMesh->SetCastShadow(false);
    }

    for (UMeshComponent* MeshComp : OwnerHiddenMeshComponents)
    {
        if (MeshComp)
        {
            MeshComp->SetOwnerNoSee(bUseFpArmsOnly);
            if (bHideOwnerShadow && bUseFpArmsOnly)
            {
                MeshComp->SetCastShadow(false);
            }
        }
    }

    if (bUseFpArmsOnly)
    {
        if (USkeletalMesh* BodyMeshAsset = SkelMesh->GetSkeletalMeshAsset())
        {
            FirstPersonArmsMesh->SetSkeletalMesh(BodyMeshAsset);
            FirstPersonArmsMesh->SetLeaderPoseComponent(SkelMesh);
            FirstPersonArmsMesh->SetCastShadow(false);
            FirstPersonArmsMesh->SetVisibility(true, true);
            ApplyFirstPersonArmVisibility(FirstPersonArmsMesh, OwnerVisibleArmBones);
        }
    }
    else if (FirstPersonArmsMesh)
    {
        FirstPersonArmsMesh->SetVisibility(false, true);
    }
}

void AKillerCharacter::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);

    if (IsLocallyControlled())
    {
        SetupKillerFirstPersonCamera();
    }
}

void AKillerCharacter::BeginPlay()
{
    Super::BeginPlay();

    if (CarryAnimComponent)
    {
        CarryAnimComponent->OnPickupAttachRequested.AddUObject(this, &AKillerCharacter::HandlePickupAttachWindow);
        CarryAnimComponent->OnPickupFinished.AddUObject(this, &AKillerCharacter::HandlePickupMontageFinished);
    }

    if (AttackAnimComp)
    {
        AttackAnimComp->OnArmAttackMontageFinished.AddUObject(this, &AKillerCharacter::OnArmAttackMontageFinished);
    }

    if (UCameraComponent* FollowCamera = GetCameraComponent())
    {
        FollowCamera->SetProjectionMode(ECameraProjectionMode::Perspective);
    }
    
    if (GetMesh() && SpringArm)
    {
        SpringArm->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, FName("head"));
        SpringArm->SetRelativeLocation(FVector(0, 0, 0)); // 오프셋 조정 필요
    }

    // FP arms mesh is optional. Hiding the body without one made attack/groggy montages invisible.
    if (IsLocallyControlled())
    {
        SetupKillerFirstPersonCamera();
    }

    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
        {
            if (InputConfig)
            {
                for (const FInputMappingContextEntry& Entry : InputConfig->MappingContexts)
                {
                    if (Entry.MappingContext) Subsystem->AddMappingContext(Entry.MappingContext, Entry.Priority);
                }
            }
        }
    }
    
    if (GetCharacterMovement()) GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    UpdateMovementSpeed();
    
    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        // 최소 Pitch와 최대 Pitch 설정
        // -45도(위) ~ +45도(아래) 범위로 제한하려면 아래와 같이 설정
        PC->PlayerCameraManager->ViewPitchMin = -45.0f;
        PC->PlayerCameraManager->ViewPitchMax = 45.0f;
        UE_LOG(LogTemp, Warning, TEXT("Pitch 제한 설정 완료: Min=%f, Max=%f"), PC->PlayerCameraManager->ViewPitchMin, PC->PlayerCameraManager->ViewPitchMax);
    }
}

void AKillerCharacter::OnArmAttackMontageFinished()
{
    if (!bAwaitingPostAttackGroggy)
    {
        return;
    }

    bArmAttackMontageComplete = true;
    TryEnterPostAttackGroggy();
}

void AKillerCharacter::ClearPostAttackGroggyTimers()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(PostAttackGroggyFallbackTimer);
        World->GetTimerManager().ClearTimer(PostAttackGroggyEndTimer);
    }
}

void AKillerCharacter::SchedulePostAttackGroggyFallback()
{
    if (!KillerData)
    {
        return;
    }

    float GroggyStartDelay = KillerData->TaserWindup + KillerData->TaserActive;
    if (AttackAnimComp)
    {
        GroggyStartDelay = FMath::Max(GroggyStartDelay, AttackAnimComp->GetArmAttackMontageLength());
    }

    GetWorldTimerManager().SetTimer(
        PostAttackGroggyFallbackTimer,
        this,
        &AKillerCharacter::OnPostAttackGroggyFallback,
        GroggyStartDelay,
        false);
}

void AKillerCharacter::OnPostAttackGroggyFallback()
{
    bArmAttackMontageComplete = true;
    TryEnterPostAttackGroggy();
}

void AKillerCharacter::TryEnterPostAttackGroggy()
{
    if (!HasAuthority() || !KillerData || !bAwaitingPostAttackGroggy)
    {
        return;
    }

    if (!bAttackWindupComplete || !bArmAttackMontageComplete)
    {
        return;
    }

    bAwaitingPostAttackGroggy = false;
    bAttackWindupComplete = false;
    bArmAttackMontageComplete = false;
    GetWorldTimerManager().ClearTimer(PostAttackGroggyFallbackTimer);

    SetKillerState(EKillerState::Groggy);

    float GroggyDuration = KillerData->TaserGroggyOnMiss;
    if (AttackAnimComp)
    {
        GroggyDuration = FMath::Max(GroggyDuration, AttackAnimComp->BeginGroggyAnim());
    }

    GetWorldTimerManager().SetTimer(
        PostAttackGroggyEndTimer,
        FTimerDelegate::CreateWeakLambda(this, [this]()
        {
            if (AttackAnimComp)
            {
                AttackAnimComp->EndGroggyAnim();
            }
            if (GetCharacterMovement())
            {
                GetCharacterMovement()->SetMovementMode(MOVE_Walking);
            }
            SetKillerState(EKillerState::Idle);
        }),
        GroggyDuration,
        false);
}

void AKillerCharacter::PerformAttack()
{
    if (!KillerData) return;

    ClearPostAttackGroggyTimers();
    bAwaitingPostAttackGroggy = false;
    bAttackWindupComplete = false;
    bArmAttackMontageComplete = false;

    SetKillerState(EKillerState::Attacking);

    if (AttackAnimComp)
    {
        AttackAnimComp->BeginArmAttackAnim();
    }
    
    // 공격 시작 시 이동 차단
    if (GetCharacterMovement())
    {
        GetCharacterMovement()->DisableMovement();
    }
    
    FTimerHandle WindupTimer;
    GetWorldTimerManager().SetTimer(WindupTimer, [this]() {
        if (!KillerData)
        {
            return;
        }

        FVector BoxCenter = GetActorLocation() + (GetActorForwardVector() * (KillerData->TaserRange / 2.f));
        FVector BoxExtent = FVector(KillerData->TaserRange / 2.f, KillerData->TaserHitboxRadius, KillerData->TaserHitboxHalfHeight);
        DrawDebugBox(GetWorld(), BoxCenter, BoxExtent, GetActorRotation().Quaternion(), FColor::Red, false, 2.0f, 0, 2.0f);

        const bool bHit = PerformAttackTrace();
        UE_LOG(LogTemp, Warning, TEXT("공격 결과: %s"), bHit ? TEXT("적중!") : TEXT("허공"));

        // Both hit and miss transition into the same post-attack groggy animation.
        // Previously the hit branch skipped groggy and returned directly to Idle.
        bAwaitingPostAttackGroggy = true;
        bAttackWindupComplete = true;
        SchedulePostAttackGroggyFallback();
        TryEnterPostAttackGroggy();
    }, KillerData->TaserWindup, false);
}

void AKillerCharacter::AttachCarriedSurvivor(AActor* Target)
{
    if (!Target || !KillerData)
    {
        return;
    }

    ASurvivorCharacter* Survivor = Cast<ASurvivorCharacter>(Target);
    if (!Survivor || Survivor->GetSurvivorState() != ESurvivorState::Downed)
    {
        return;
    }

    CarriedSurvivor = Target;

    GetCapsuleComponent()->IgnoreActorWhenMoving(CarriedSurvivor, true);

    if (UCharacterMovementComponent* MoveComp = CarriedSurvivor->FindComponentByClass<UCharacterMovementComponent>())
    {
        MoveComp->DisableMovement();
        MoveComp->StopMovementImmediately();
    }

    CarriedSurvivor->SetActorEnableCollision(false);
    TArray<UPrimitiveComponent*> Primitives;
    CarriedSurvivor->GetComponents<UPrimitiveComponent>(Primitives);
    for (UPrimitiveComponent* Comp : Primitives)
    {
        Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Comp->SetCollisionResponseToAllChannels(ECR_Ignore);
    }

    ApplyCarryAttachmentTransform(CarriedSurvivor);
}

namespace
{
    // Component-space transform of a bone in the reference pose — i.e. the pose shown in
    // the BP viewport where CarryAttachPoint gets tuned against the capsule.
    FTransform GetRefPoseComponentSpaceTransform(const USkeletalMeshComponent* Mesh, FName BoneName)
    {
        const USkeletalMesh* Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
        if (!Asset)
        {
            return FTransform::Identity;
        }

        const FReferenceSkeleton& RefSkel = Asset->GetRefSkeleton();
        int32 BoneIndex = RefSkel.FindBoneIndex(BoneName);
        FTransform Result = FTransform::Identity;
        while (BoneIndex != INDEX_NONE)
        {
            Result = Result * RefSkel.GetRefBonePose()[BoneIndex];
            BoneIndex = RefSkel.GetParentIndex(BoneIndex);
        }
        return Result;
    }
}

void AKillerCharacter::ApplyCarryAttachmentTransform(AActor* Target)
{
    if (!Target || !KillerData)
    {
        return;
    }

    // The head-bone camera boom lags behind the pawn while moving (CharacterBase enables
    // camera lag), so a carried survivor fixed to the body slides into the killer's view.
    // Kill the lag for the local killer while carrying; restored on drop from the archetype.
    if (IsLocallyControlled() && SpringArm)
    {
        SpringArm->bEnableCameraLag = false;
        SpringArm->bEnableCameraRotationLag = false;
    }

    // Carried survivors move via attachment, not CMC. On clients the simulated-proxy
    // network smoothing keeps lerping the mesh toward stale replicated positions, which
    // makes the survivor visibly trail the killer. Runs on server and on OnRep clients.
    if (ACharacter* TargetChar = Cast<ACharacter>(Target))
    {
        if (UCharacterMovementComponent* TargetMove = TargetChar->GetCharacterMovement())
        {
            TargetMove->NetworkSmoothingMode = ENetworkSmoothingMode::Disabled;
        }
    }

    // Designer-tunable attach point: BP_KillerCharacter's CarryAttachPoint SceneComponent,
    // tuned in the BP viewport relative to the capsule. The killer camera rides the head
    // bone, so the survivor must ride the animated mesh too (spine_03) or it drifts into
    // view while moving. Convert the tuned capsule-relative pose into a spine_03-relative
    // pose using the ref pose (the pose shown in the BP viewport), then attach to the bone.
    for (UActorComponent* Comp : GetComponents())
    {
        if (Comp && Comp->GetFName() == TEXT("CarryAttachPoint"))
        {
            if (USceneComponent* CarryAttachPoint = Cast<USceneComponent>(Comp))
            {
                USkeletalMeshComponent* CarryMesh = GetMesh();
                const FName CarryBone = TEXT("spine_03");
                if (CarryMesh
                    && (CarryMesh->DoesSocketExist(CarryBone) || CarryMesh->GetBoneIndex(CarryBone) != INDEX_NONE))
                {
                    // Actor-space bone transform in ref pose = bone(component space) * mesh relative transform.
                    const FTransform BoneActorTM =
                        GetRefPoseComponentSpaceTransform(CarryMesh, CarryBone) * CarryMesh->GetRelativeTransform();
                    // CarryAttachPoint sits under the capsule (actor root), so world->actor is exact.
                    const FTransform PointActorTM =
                        CarryAttachPoint->GetComponentTransform().GetRelativeTransform(GetActorTransform());
                    const FTransform RelToBone = PointActorTM.GetRelativeTransform(BoneActorTM);

                    Target->AttachToComponent(
                        CarryMesh,
                        FAttachmentTransformRules::SnapToTargetNotIncludingScale,
                        CarryBone);
                    Target->SetActorRelativeLocation(RelToBone.GetLocation());
                    Target->SetActorRelativeRotation(RelToBone.Rotator());

                    UE_LOG(LogTemp, Warning,
                        TEXT("KillerCarry: attached %s to bone %s via CarryAttachPoint (rel loc=%s rot=%s)"),
                        *GetNameSafe(Target),
                        *CarryBone.ToString(),
                        *RelToBone.GetLocation().ToCompactString(),
                        *RelToBone.Rotator().ToCompactString());
                    return;
                }

                Target->AttachToComponent(
                    CarryAttachPoint,
                    FAttachmentTransformRules::SnapToTargetNotIncludingScale);
                Target->SetActorRelativeLocation(FVector::ZeroVector);
                Target->SetActorRelativeRotation(FRotator::ZeroRotator);

                UE_LOG(LogTemp, Warning,
                    TEXT("KillerCarry: attached %s to CarryAttachPoint component"),
                    *GetNameSafe(Target));
                return;
            }
        }
    }

    USkeletalMeshComponent* KillerMesh = GetMesh();
    auto DoesAttachPointExist = [KillerMesh](FName Name) -> bool
    {
        return KillerMesh
            && !Name.IsNone()
            && (KillerMesh->DoesSocketExist(Name) || KillerMesh->GetBoneIndex(Name) != INDEX_NONE);
    };

    // Prefer configured socket, then stable shoulder/spine bones (hand follows anim and leaves
    // the survivor floating in front of the torso during the carrying pose).
    static const FName FallbackSockets[] = {
        TEXT("spine_03"),
        TEXT("spine_02"),
        TEXT("clavicle_r"),
        TEXT("upperarm_r"),
        TEXT("hand_r"),
    };

    FName AttachSocket = KillerData->CarryAttachSocketName;
    const FVector RelLocation = KillerData->CarryAttachRelativeLocation;
    const FRotator RelRotation = KillerData->CarryAttachRelativeRotation;

    // hand_* follows the carry anim in front of the chest — prefer spine for shoulder sit.
    // Location/Rotation always come from DA_KillerData so you can tune them in the editor.
    const FString SocketStr = AttachSocket.ToString();
    if (AttachSocket.IsNone() || SocketStr.Contains(TEXT("hand"), ESearchCase::IgnoreCase))
    {
        AttachSocket = TEXT("spine_03");
    }

    if (!DoesAttachPointExist(AttachSocket))
    {
        AttachSocket = NAME_None;
        for (const FName Candidate : FallbackSockets)
        {
            if (DoesAttachPointExist(Candidate))
            {
                AttachSocket = Candidate;
                break;
            }
        }
    }

    if (KillerMesh && !AttachSocket.IsNone())
    {
        Target->AttachToComponent(
            KillerMesh,
            FAttachmentTransformRules::SnapToTargetNotIncludingScale,
            AttachSocket);
        Target->SetActorRelativeLocation(RelLocation);
        Target->SetActorRelativeRotation(RelRotation);

        UE_LOG(LogTemp, Warning,
            TEXT("KillerCarry: attached %s to socket %s loc=%s rot=%s"),
            *GetNameSafe(Target),
            *AttachSocket.ToString(),
            *RelLocation.ToCompactString(),
            *RelRotation.ToCompactString());
    }
    else
    {
        Target->AttachToActor(this, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
        Target->SetActorRelativeLocation(FVector(
            KillerData->CarryAttachForwardOffset,
            40.f,
            40.f));
        Target->SetActorRelativeRotation(RelRotation);

        UE_LOG(LogTemp, Warning,
            TEXT("KillerCarry: socket missing, attached %s to actor with fallback offset"),
            *GetNameSafe(Target));
    }
}

void AKillerCharacter::PickupSurvivor(AActor* Target)
{
    ASurvivorCharacter* Survivor = Cast<ASurvivorCharacter>(Target);
    if (!Survivor || Survivor->GetSurvivorState() != ESurvivorState::Downed)
    {
        PendingPickupTarget = nullptr;
        bIsBusy = false;
        SetKillerState(EKillerState::Idle);
        if (GetCharacterMovement())
        {
            GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        }
        if (CarryAnimComponent)
        {
            CarryAnimComponent->EndCarryAnims();
        }
        return;
    }

    if (CarriedSurvivor != Target)
    {
        AttachCarriedSurvivor(Target);
    }
    else
    {
        ApplyCarryAttachmentTransform(Target);
    }

    // Start carry anim before state rep so OnRep Ensure sees bWantsCarryingAnim when possible.
    if (CarryAnimComponent)
    {
        CarryAnimComponent->BeginCarryingAnim();
    }
    SetKillerState(EKillerState::Carrying);

    if (GetCharacterMovement())
    {
        GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    }
    bIsBusy = false;
    PendingPickupTarget = nullptr;
}

void AKillerCharacter::HandlePickupAttachWindow()
{
    if (!HasAuthority())
    {
        return;
    }

    AActor* Target = PendingPickupTarget.Get();
    ASurvivorCharacter* Survivor = Cast<ASurvivorCharacter>(Target);
    if (!Survivor
        || Survivor->GetSurvivorState() != ESurvivorState::Downed
        || CarriedSurvivor)
    {
        return;
    }

    AttachCarriedSurvivor(Target);
}

void AKillerCharacter::HandlePickupMontageFinished()
{
    if (!HasAuthority())
    {
        return;
    }

    AActor* Target = PendingPickupTarget.Get();
    if (!Target && CarriedSurvivor)
    {
        Target = CarriedSurvivor;
    }

    if (!Target)
    {
        SetKillerState(EKillerState::Idle);
        bIsBusy = false;
        if (GetCharacterMovement())
        {
            GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        }
        return;
    }

    PickupSurvivor(Target);
}

namespace
{
    // Keep the BP-authored camera distance/angle, but never restore positional lag:
    // the camera rig must remain rigidly attached to the animated head bone.
    void RestoreCarryCameraLag(AKillerCharacter* Killer)
    {
        USpringArmComponent* Arm = Killer ? Killer->GetSpringArmComponent() : nullptr;
        if (!Arm || !Killer->IsLocallyControlled())
        {
            return;
        }

        Arm->bEnableCameraLag = false;
        Arm->bEnableCameraRotationLag = false;
    }
}

void AKillerCharacter::OnRep_CarriedSurvivor()
{
    if (!CarriedSurvivor)
    {
        RestoreCarryCameraLag(this);

        // Dropped on the server — restore network smoothing on any survivor we disabled
        // it for while carrying (the old value is not passed to this OnRep).
        for (TActorIterator<ASurvivorCharacter> It(GetWorld()); It; ++It)
        {
            ASurvivorCharacter* Survivor = *It;
            if (!IsValid(Survivor) || Survivor->GetRootComponent()->GetAttachParent())
            {
                continue;
            }

            if (UCharacterMovementComponent* MoveComp = Survivor->GetCharacterMovement())
            {
                if (MoveComp->NetworkSmoothingMode == ENetworkSmoothingMode::Disabled)
                {
                    MoveComp->NetworkSmoothingMode = ENetworkSmoothingMode::Exponential;
                }
            }
        }
        return;
    }

    ApplyCarryAttachmentTransform(CarriedSurvivor);
}

void AKillerCharacter::DropSurvivor()
{
    if (!CarriedSurvivor) return;

    if (CarryAnimComponent)
    {
        CarryAnimComponent->EndCarryAnims();
    }

    if (ASurvivorCharacter* Survivor = Cast<ASurvivorCharacter>(CarriedSurvivor))
    {
        Survivor->SetSurvivorState(ESurvivorState::Downed);
    }

    // 운반 중 꺼뒀던 시뮬레이티드 프록시 스무딩 복구 (서버 측; 클라는 OnRep에서 복구)
    if (ACharacter* DroppedChar = Cast<ACharacter>(CarriedSurvivor))
    {
        if (UCharacterMovementComponent* DroppedMove = DroppedChar->GetCharacterMovement())
        {
            DroppedMove->NetworkSmoothingMode = ENetworkSmoothingMode::Exponential;
        }
    }

    RestoreCarryCameraLag(this);

    // 1. 살인마와 생존자 간 충돌 무시 해제
    GetCapsuleComponent()->IgnoreActorWhenMoving(CarriedSurvivor, false);
    if (UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(CarriedSurvivor->GetRootComponent()))
    {
        Root->IgnoreActorWhenMoving(this, false);
    }

    // 2. 부착 해제
    CarriedSurvivor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

    // 3. 살인마의 전방으로 생존자를 '순간 이동'시켜서 캡슐 겹침을 방지
    const float DropOffset = KillerData ? KillerData->DropForwardOffset : 150.f;
    FVector SafeLocation = GetActorLocation() + (GetActorForwardVector() * DropOffset);
    SafeLocation.Z = GetActorLocation().Z; // 바닥 높이 맞춤
    CarriedSurvivor->SetActorLocation(SafeLocation, false, nullptr, ETeleportType::TeleportPhysics);

    // 4. 즉시 콜리전 복구 (이제는 안 겹치니까 튕기지 않음!)
    CarriedSurvivor->SetActorEnableCollision(true);
    
    // 생존자 컴포넌트들 복구
    TArray<UPrimitiveComponent*> Primitives;
    CarriedSurvivor->GetComponents<UPrimitiveComponent>(Primitives);
    for (auto* Comp : Primitives)
    {
        Comp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        Comp->SetCollisionResponseToAllChannels(ECR_Block); // 기본 상태 복구
    }

    // 5. 살인마 자신의 움직임 복구
    if (GetCharacterMovement())
    {
        GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        GetCharacterMovement()->StopMovementImmediately(); // 관성 제거
    }

    CarriedSurvivor = nullptr;
    
    // 상태를 Idle로 변경
    SetKillerState(EKillerState::Idle);
    
    // bIsBusy 해제 (중요)
    bIsBusy = false;
}

void AKillerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
    if (EIC && InputConfig)
    {
        if (InputConfig->AttackAction) EIC->BindAction(InputConfig->AttackAction, ETriggerEvent::Started, this, &AKillerCharacter::Attack);
        if (InputConfig->InteractAction) EIC->BindAction(InputConfig->InteractAction, ETriggerEvent::Started, this, &AKillerCharacter::Interact);
    }
}

void AKillerCharacter::UpdateMovementSpeed()
{
    if (!KillerData) return;

    float Multiplier = 1.0f;

    if (CurrentState == EKillerState::Carrying) 
        Multiplier = KillerData->KillerCarrySpeedMultiplier;
    else if (CurrentState == EKillerState::Groggy) 
        Multiplier = KillerData->KillerGroggySpeedMultiplier;

    float FinalSpeed = KillerData->KillerBaseSpeed * Multiplier;
    GetCharacterMovement()->MaxWalkSpeed = FinalSpeed;
    
    UE_LOG(LogTemp, Warning, TEXT("속도 업데이트됨: 상태(%d), Multiplier(%.2f), 최종속도(%.2f)"), 
        (int32)CurrentState, Multiplier, FinalSpeed);
}

AActor* AKillerCharacter::FindInteractableActor(float Radius)
{
    TArray<FOverlapResult> Overlaps;
    FCollisionShape Sphere = FCollisionShape::MakeSphere(Radius);
    FCollisionQueryParams Params(NAME_None, false, this);
    
    if (GetWorld()->OverlapMultiByChannel(Overlaps, GetActorLocation(), FQuat::Identity, ECC_Pawn, Sphere, Params))
    {
        // Carrying 중에는 케이지만, Idle 픽업 시에는 생존자만
        const bool bLookingForCage = (CurrentState == EKillerState::Carrying);

        for (const auto& Result : Overlaps)
        {
            if (bLookingForCage)
            {
                if (ACage* Cage = Cast<ACage>(Result.GetActor()))
                {
                    if (Cage->GetCageStatus() == ECageStatus::Empty)
                    {
                        return Cage;
                    }
                }
                continue;
            }

            if (ASurvivorCharacter* Survivor = Cast<ASurvivorCharacter>(Result.GetActor()))
            {
                if (Survivor->GetSurvivorState() == ESurvivorState::Downed)
                {
                    return Survivor;
                }
            }
        }
    }
    return nullptr;
}

bool AKillerCharacter::PerformAttackTrace()
{
    FVector BoxCenter = GetActorLocation() + (GetActorForwardVector() * (KillerData->TaserRange / 2.f));
    FVector BoxExtent = FVector(KillerData->TaserRange / 2.f, KillerData->TaserHitboxRadius, 90.f);
    
    TArray<FOverlapResult> Overlaps;
    FCollisionShape BoxShape = FCollisionShape::MakeBox(BoxExtent);
    FCollisionQueryParams Params(NAME_None, false, this);

    bool bAnyHit = GetWorld()->OverlapMultiByChannel(Overlaps, BoxCenter, GetActorRotation().Quaternion(), ECC_Pawn, BoxShape, Params);

    if (bAnyHit)
    {
        for (const FOverlapResult& Overlap : Overlaps)
        {
            if (ASurvivorCharacter* HitSurvivor = Cast<ASurvivorCharacter>(Overlap.GetActor()))
            {
                // [수정] Healthy 또는 Injured 상태일 때만 공격 적중 처리
                ESurvivorState SState = HitSurvivor->GetSurvivorState();
                if (SState == ESurvivorState::Healthy || SState == ESurvivorState::Injured)
                {
                    UE_LOG(LogTemp, Warning, TEXT("공격 적중: %s"), *HitSurvivor->GetName());
                    HitSurvivor->ApplyHit(); 
                    return true;
                }
            }
        }
    }
    return false;
}

void AKillerCharacter::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AKillerCharacter, CurrentState);
    DOREPLIFETIME(AKillerCharacter, bIsBusy);
    DOREPLIFETIME(AKillerCharacter, CarriedSurvivor);
}

// 상태 변경 시 모든 클라이언트가 속도 업데이트
// 상태 변경 시 모든 클라이언트가 속도·캐리 애니 동기화
void AKillerCharacter::OnRep_CurrentState()
{
    UpdateMovementSpeed();

    if (!CarryAnimComponent)
    {
        return;
    }

    if (CurrentState == EKillerState::Carrying)
    {
        if (!CarryAnimComponent->WantsCarryingAnim())
        {
            CarryAnimComponent->BeginCarryingAnim();
        }
        else
        {
            CarryAnimComponent->EnsureCarryingMontagePlaying();
        }
    }
    else if (CurrentState != EKillerState::PickingUp
        && CurrentState != EKillerState::Interacting
        && CarryAnimComponent->WantsCarryingAnim())
    {
        CarryAnimComponent->EndCarryAnims();
    }
}

// 서버 공격 RPC
void AKillerCharacter::Attack() { Server_Attack(); }

void AKillerCharacter::Server_Attack_Implementation()
{
    if (CurrentState == EKillerState::Idle) PerformAttack();
}

// 서버 상호작용 RPC
void AKillerCharacter::Interact() { Server_Interact(); }

void AKillerCharacter::Server_Interact_Implementation()
{
    if (!KillerData || bIsBusy) return;
    
    // 1. Idle 상태일 때: 생존자 픽업 (bCanPickup 쿨타임 체크 추가)
    switch (CurrentState)
    {
    case EKillerState::Idle:
        HandlePickupInteraction();
        break;
        
    case EKillerState::Carrying:
        HandleCarryingInteraction();
        break;
        
    default:
        break;
    }
}

// 상태 변경 함수 수정 (서버에서만 실행)
void AKillerCharacter::SetKillerState(EKillerState NewState)
{
    if (HasAuthority())
    {
        if (CurrentState != NewState)
        {
            UE_LOG(LogTemp, Log, TEXT("Killer State 변경: %d -> %d"), (int32)CurrentState, (int32)NewState);
            CurrentState = NewState;
            UpdateMovementSpeed();
            OnRep_CurrentState();
        }
    }
}

// 상태별 처리 함수 분리
void AKillerCharacter::HandlePickupInteraction()
{
    if (!bCanPickup || !CarryAnimComponent)
    {
        return;
    }

    AActor* Target = FindInteractableActor(KillerData->PickupRange);
    if (!Target)
    {
        return;
    }

    PendingPickupTarget = Target;
    bIsBusy = true;
    SetKillerState(EKillerState::PickingUp);
    GetCharacterMovement()->DisableMovement();
    CarryAnimComponent->BeginPickupAnim();
}

void AKillerCharacter::HandleCarryingInteraction()
{
    AActor* InteractTarget = FindInteractableActor(KillerData->CageDepositRange);
    ACage* TargetCage = Cast<ACage>(InteractTarget);

    if (TargetCage)
    {
        ProcessCageDeposit(TargetCage);
    }
    else
    {
        ProcessNormalDrop();
    }
}

void AKillerCharacter::ProcessCageDeposit(ACage* TargetCage)
{
    bIsBusy = true;
    SetKillerState(EKillerState::Interacting);
    GetCharacterMovement()->DisableMovement();
    if (CarryAnimComponent)
    {
        CarryAnimComponent->EndCarryAnims();
    }

    FTimerHandle TimerHandle;
    GetWorldTimerManager().SetTimer(TimerHandle, [this, TargetCage]() {
        if (ASurvivorCharacter* Survivor = Cast<ASurvivorCharacter>(CarriedSurvivor))
        {
            Survivor->SetSurvivorState(ESurvivorState::Caged);
            Survivor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
            Survivor->SetActorLocation(TargetCage->GetActorLocation());
            if (UCharacterMovementComponent* SurvivorMove = Survivor->GetCharacterMovement())
            {
                SurvivorMove->NetworkSmoothingMode = ENetworkSmoothingMode::Exponential;
            }
        }

        RestoreCarryCameraLag(this);
        
        TargetCage->SetCageStatus(ECageStatus::Occupied);
        CarriedSurvivor = nullptr;
        
        SetKillerState(EKillerState::Idle);
        bIsBusy = false;
        if (GetCharacterMovement()) GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        
        // 쿨타임 시작 로직...
    }, KillerData->CageDepositDuration, false);
}

void AKillerCharacter::ProcessNormalDrop()
{
    bIsBusy = true;
    SetKillerState(EKillerState::Interacting);
    GetCharacterMovement()->DisableMovement();

    // 1초 대기 후 Drop 실행
    FTimerHandle DropTimer;
    GetWorldTimerManager().SetTimer(DropTimer, [this]() {
        DropSurvivor();
        
        bIsBusy = false;
        SetKillerState(EKillerState::Idle);
        if (GetCharacterMovement()) GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    }, 1.0f, false);
}
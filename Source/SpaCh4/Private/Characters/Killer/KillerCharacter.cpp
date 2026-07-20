#include "Characters/Killer/KillerCharacter.h"
#include "Systems/Data/KillerData.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "InputActionValue.h"
#include "InputAction.h"
#include "EnhancedInputComponent.h"
#include "Data/SPInputConfigData.h"
#include "EnhancedInputSubsystems.h"
#include "Camera/CameraComponent.h"
#include "Characters/Survivor/SurvivorCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SPKillerCarryAnimComponent.h"
#include "Components/SPKillerCarrySoundComponent.h"
#include "Components/SPKillerChaseMusicComponent.h"
#include "Components/SPKillerGroggySoundComponent.h"
#include "Components/SPParkourComponent.h"
#include "Components/SPTaserVFXComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "UObject/ConstructorHelpers.h"
/*<--------- SPKillerFirstPersonMeshComponent 부재에 의한 주석 처리 ----------------------------->
#include "Components/SPKillerFirstPersonMeshComponent.h"
*/
#include "GameFramework/SpringArmComponent.h"
#include "Gameplay/Cage/Cage.h"
#include "Type/SPGameplayTag.h"
#include "Engine/SkeletalMesh.h"
#include "ReferenceSkeleton.h"
#include "Net/UnrealNetwork.h"
#include "Player/LDPlayerState.h"

namespace
{
    FTransform GetRefPoseComponentSpaceTransform(const USkeletalMeshComponent* Mesh, const FName BoneName)
    {
        const USkeletalMesh* Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
        if (!Asset)
        {
            return FTransform::Identity;
        }

        const FReferenceSkeleton& RefSkeleton = Asset->GetRefSkeleton();
        int32 BoneIndex = RefSkeleton.FindBoneIndex(BoneName);
        if (BoneIndex == INDEX_NONE)
        {
            return FTransform::Identity;
        }

        FTransform Result = RefSkeleton.GetRefBonePose()[BoneIndex];
        while ((BoneIndex = RefSkeleton.GetParentIndex(BoneIndex)) != INDEX_NONE)
        {
            Result *= RefSkeleton.GetRefBonePose()[BoneIndex];
        }
        return Result;
    }
}

AKillerCharacter::AKillerCharacter()
{
    PrimaryActorTick.bCanEverTick = false;

    bUseControllerRotationYaw = true;         
    if (GetCharacterMovement())
    {
        GetCharacterMovement()->bOrientRotationToMovement = false;
    }
    
    charTag.AddTag(SPGameplayTags::Character::Killer);
    CarryAnimComponent = CreateDefaultSubobject<USPKillerCarryAnimComponent>(TEXT("CarryAnimComponent"));
    CarrySoundComponent = CreateDefaultSubobject<USPKillerCarrySoundComponent>(TEXT("CarrySoundComponent"));
    GroggySoundComponent = CreateDefaultSubobject<USPKillerGroggySoundComponent>(TEXT("GroggySoundComponent"));
    ChaseMusicComponent = CreateDefaultSubobject<USPKillerChaseMusicComponent>(TEXT("ChaseMusicComponent"));
    ParkourComponent = CreateDefaultSubobject<USPParkourComponent>(TEXT("ParkourComponent"));
    TaserVFXComponent = CreateDefaultSubobject<USPTaserVFXComponent>(TEXT("TaserVFXComponent"));

    static ConstructorHelpers::FObjectFinder<UAnimMontage> AttackMontageFinder(
        TEXT("/Game/Assets/Killer_Locomotion/Attack/AM_Attack_Montage.AM_Attack_Montage"));
    if (AttackMontageFinder.Succeeded())
    {
        AttackMontage = AttackMontageFinder.Object;
    }

    static ConstructorHelpers::FObjectFinder<UAnimMontage> GroggyMontageFinder(
        TEXT("/Game/Assets/Killer_Locomotion/Groggy/AS_Zombie_Idle_Montage.AS_Zombie_Idle_Montage"));
    if (GroggyMontageFinder.Succeeded())
    {
        GroggyMontage = GroggyMontageFinder.Object;
    }

    FallbackGroggySequence = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(
        TEXT("/Game/Assets/Killer_Locomotion/Groggy/AS_Zombie_Idle1.AS_Zombie_Idle1")));
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

    auto IsBoneVisible = [&](int32 BoneIndex) -> bool
    {
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

    // Keep the camera rigidly fixed to the animated head while moving.
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

    // Keep the killer body visible unless a dedicated first-person arms mesh
    // actually exists to replace it.
    const bool bUseFirstPersonArmsOnly = bShowFirstPersonArmsOnly && FirstPersonArmsMesh != nullptr;
    SkelMesh->SetOwnerNoSee(bUseFirstPersonArmsOnly);
    if (bHideOwnerShadow && bUseFirstPersonArmsOnly)
    {
        SkelMesh->SetCastShadow(false);
    }

    for (UMeshComponent* MeshComp : OwnerHiddenMeshComponents)
    {
        if (MeshComp)
        {
            MeshComp->SetOwnerNoSee(bUseFirstPersonArmsOnly);
            if (bHideOwnerShadow && bUseFirstPersonArmsOnly)
            {
                MeshComp->SetCastShadow(false);
            }
        }
    }

    if (bUseFirstPersonArmsOnly)
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
    
    SetupKillerFirstPersonCamera(); // 카메라 초기화 분리
    InitializeInputSubsystem();
 
    if (GetCharacterMovement()) GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    UpdateMovementSpeed();
    
    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        PC->PlayerCameraManager->ViewPitchMin = MinPitch;
        PC->PlayerCameraManager->ViewPitchMax = MaxPitch;
    }
}

void AKillerCharacter::InitializeInputSubsystem()
{
    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
        {
            if (InputConfig)
            {
                for (const FInputMappingContextEntry& Entry : InputConfig->MappingContexts)
                {
                    if (Entry.MappingContext) 
                    {
                        Subsystem->AddMappingContext(Entry.MappingContext, Entry.Priority);
                    }
                }
            }
        }
    }
}


void AKillerCharacter::PerformAttack()
{
    if (!KillerData) return;

    SetKillerState(EKillerState::Attacking);

    if (HasAuthority() && AttackMontage)
    {
        Multicast_PlayAttackMontage(AttackMontage);
    }
    
    // 공격 시작 시 이동 차단
    if (GetCharacterMovement())
    {
        GetCharacterMovement()->DisableMovement();
    }
    
    // 1. Windup (0.35s)
    FTimerHandle WindupTimer;
    GetWorldTimerManager().SetTimer(WindupTimer, [this]() {
        
        // Active 판정 및 Trace는 서버에서만 수행
        FVector HitLocation = GetActorLocation() + GetActorForwardVector() * (KillerData ? KillerData->TaserRange : 250.f);
        bool bHit = PerformAttackTrace(HitLocation);
        UE_LOG(LogTemp, Warning, TEXT("공격 결과: %s"), bHit ? TEXT("적중!") : TEXT("허공"));

        if (HasAuthority())
        {
            Multicast_PlayTaserVFX(ETaserVFXPhase::Discharge, HitLocation, bHit);
        }

        auto RestoreMovement = [this]() {
            if (GetCharacterMovement())
            {
                GetCharacterMovement()->SetMovementMode(MOVE_Walking);
            }
            SetKillerState(EKillerState::Idle);
        };

        if (bHit) 
        {
            FTimerHandle RecoveryTimer;
            GetWorldTimerManager().SetTimer(RecoveryTimer, RestoreMovement, KillerData->TaserRecoveryHit, false);
        } 
        else 
        {
            SetKillerState(EKillerState::Groggy);
            FTimerHandle GroggyTimer;
            GetWorldTimerManager().SetTimer(GroggyTimer, RestoreMovement, KillerData->TaserGroggyOnMiss, false);
        }
    }, KillerData->TaserWindup, false);
}

void AKillerCharacter::Multicast_PlayTaserVFX_Implementation(ETaserVFXPhase Phase, FVector EndLocation, bool bHit)
{
    USPTaserVFXComponent* VFXComponent = TaserVFXComponent;
    if (!VFXComponent)
    {
        VFXComponent = FindComponentByClass<USPTaserVFXComponent>();
    }

    if (!VFXComponent)
    {
        UE_LOG(LogTemp, Warning, TEXT("Killer TaserVFX: component missing on %s"), *GetName());
        return;
    }

    switch (Phase)
    {
    case ETaserVFXPhase::Discharge:
        VFXComponent->PlayDischarge(EndLocation, bHit);
        break;
    default:
        break;
    }
}

void AKillerCharacter::Server_Attack_Implementation()
{
    if (CurrentState == EKillerState::Idle)
    {
        PerformAttack();
    }
}

void AKillerCharacter::Multicast_PlayAttackMontage_Implementation(UAnimMontage* MontageToPlay)
{
    if (!MontageToPlay)
    {
        return;
    }

    USkeletalMeshComponent* SkelMesh = GetMesh();
    UAnimInstance* AnimInstance = SkelMesh ? SkelMesh->GetAnimInstance() : nullptr;
    if (!AnimInstance)
    {
        return;
    }

    AnimInstance->Montage_Play(MontageToPlay);
}

UAnimMontage* AKillerCharacter::ResolveGroggyMontage()
{
    if (UAnimSequence* Sequence = FallbackGroggySequence.LoadSynchronous())
    {
        RuntimeGroggyMontage = UAnimMontage::CreateSlotAnimationAsDynamicMontage(
            Sequence,
            TEXT("DefaultSlot"),
            0.15f,
            0.15f,
            1.f,
            1);
        if (RuntimeGroggyMontage && RuntimeGroggyMontage->GetPlayLength() > KINDA_SMALL_NUMBER)
        {
            return RuntimeGroggyMontage;
        }
    }

    if (GroggyMontage && GroggyMontage->GetPlayLength() > KINDA_SMALL_NUMBER)
    {
        return GroggyMontage;
    }

    return nullptr;
}

void AKillerCharacter::StopGroggyMontageLocal(float BlendOut)
{
    USkeletalMeshComponent* SkelMesh = GetMesh();
    UAnimInstance* AnimInstance = SkelMesh ? SkelMesh->GetAnimInstance() : nullptr;
    if (!AnimInstance)
    {
        return;
    }

    if (ActiveGroggyMontage && AnimInstance->Montage_IsPlaying(ActiveGroggyMontage))
    {
        AnimInstance->Montage_Stop(BlendOut, ActiveGroggyMontage);
    }
    else if (UAnimMontage* ResolvedMontage = ResolveGroggyMontage())
    {
        if (AnimInstance->Montage_IsPlaying(ResolvedMontage))
        {
            AnimInstance->Montage_Stop(BlendOut, ResolvedMontage);
        }
    }

    ActiveGroggyMontage = nullptr;
}

void AKillerCharacter::Multicast_StopGroggyMontage_Implementation()
{
    StopGroggyMontageLocal();
}

void AKillerCharacter::Multicast_PlayGroggyMontage_Implementation()
{
    UAnimMontage* MontageToPlay = ResolveGroggyMontage();
    if (!MontageToPlay)
    {
        UE_LOG(LogTemp, Warning, TEXT("Groggy montage missing - check AS_Zombie_Idle_Montage / AS_Zombie_Idle1"));
        return;
    }

    USkeletalMeshComponent* SkelMesh = GetMesh();
    UAnimInstance* AnimInstance = SkelMesh ? SkelMesh->GetAnimInstance() : nullptr;
    if (!AnimInstance)
    {
        return;
    }

    if (AttackMontage && AnimInstance->Montage_IsPlaying(AttackMontage))
    {
        AnimInstance->Montage_Stop(0.12f, AttackMontage);
    }

    ActiveGroggyMontage = MontageToPlay;
    const float PlayRate = 1.f;
    AnimInstance->Montage_Play(MontageToPlay, PlayRate);
}


void AKillerCharacter::AttachCarriedSurvivor(AActor* Target)
{
    ASurvivorCharacter* Survivor = Cast<ASurvivorCharacter>(Target);
    if (!Survivor || Survivor->GetSurvivorState() != ESurvivorState::Downed) return;
    
    CarriedSurvivor = Target;
    Survivor->SetSurvivorState(ESurvivorState::Carried);

    // 1. 살인마와 생존자 서로 간의 충돌 무시
    GetCapsuleComponent()->IgnoreActorWhenMoving(CarriedSurvivor, true);

    // 2. 생존자의 무브먼트 및 물리 완전 정지
    if (UCharacterMovementComponent* MoveComp = CarriedSurvivor->FindComponentByClass<UCharacterMovementComponent>())
    {
        MoveComp->DisableMovement();
        MoveComp->StopMovementImmediately();
    }

    // 3. 콜리전 제거
    CarriedSurvivor->SetActorEnableCollision(false);
    TArray<UPrimitiveComponent*> Primitives;
    CarriedSurvivor->GetComponents<UPrimitiveComponent>(Primitives);
    for (auto* Comp : Primitives)
    {
        Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Comp->SetCollisionResponseToAllChannels(ECR_Ignore); // 모든 채널 무시
    }

    ApplyCarryAttachmentTransform(CarriedSurvivor);
}

void AKillerCharacter::ApplyCarryAttachmentTransform(AActor* Target)
{
    if (!Target)
    {
        return;
    }

    if (ACharacter* TargetCharacter = Cast<ACharacter>(Target))
    {
        if (UCharacterMovementComponent* TargetMovement = TargetCharacter->GetCharacterMovement())
        {
            TargetMovement->NetworkSmoothingMode = ENetworkSmoothingMode::Disabled;
        }
    }

    for (UActorComponent* Component : GetComponents())
    {
        if (!Component || Component->GetFName() != TEXT("CarryAttachPoint"))
        {
            continue;
        }

        USceneComponent* CarryAttachPoint = Cast<USceneComponent>(Component);
        if (!CarryAttachPoint)
        {
            continue;
        }

        USkeletalMeshComponent* CarryMesh = GetMesh();
        const FName CarryBone(TEXT("spine_03"));
        if (CarryMesh
            && (CarryMesh->DoesSocketExist(CarryBone) || CarryMesh->GetBoneIndex(CarryBone) != INDEX_NONE))
        {
            // CarryAttachPoint is tuned in the BP viewport relative to the capsule.
            // Convert that authored pose to spine_03 space so it follows the animated body.
            const FTransform BoneActorTransform =
                GetRefPoseComponentSpaceTransform(CarryMesh, CarryBone) * CarryMesh->GetRelativeTransform();
            const FTransform PointActorTransform =
                CarryAttachPoint->GetComponentTransform().GetRelativeTransform(GetActorTransform());
            const FTransform RelativeToBone =
                PointActorTransform.GetRelativeTransform(BoneActorTransform);

            Target->AttachToComponent(
                CarryMesh,
                FAttachmentTransformRules::SnapToTargetNotIncludingScale,
                CarryBone);

            Target->SetActorRelativeLocation(RelativeToBone.GetLocation() + FVector(10.f, 0.f, 10.f));
            //Target->SetActorRelativeLocation(RelativeToBone.GetLocation());
            Target->SetActorRelativeRotation(RelativeToBone.Rotator());
            return;
        }

        Target->AttachToComponent(
            CarryAttachPoint,
            FAttachmentTransformRules::SnapToTargetNotIncludingScale);
        Target->SetActorRelativeLocation(FVector::ZeroVector);
        Target->SetActorRelativeRotation(FRotator::ZeroRotator);
        return;
    }

    // Safety fallback for non-BP killer classes.
    Target->AttachToActor(this, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
    Target->SetActorRelativeLocation(FVector(150.f, 0.f, 0.f));
}

void AKillerCharacter::PickupSurvivor(AActor* Target)
{
    if (!CarriedSurvivor)
    {
        AttachCarriedSurvivor(Target);
    }

    if (!CarriedSurvivor)
    {
        PendingPickupTarget = nullptr;
        bIsBusy = false;
        SetKillerState(EKillerState::Idle);
        if (GetCharacterMovement())
        {
            GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        }
        return;
    }

    // The survivor becomes Carried only after the pickup montage has fully
    // completed. The carried-pose animation switches on this state change.
    if (ASurvivorCharacter* Survivor = Cast<ASurvivorCharacter>(CarriedSurvivor))
    {
        Survivor->SetSurvivorState(ESurvivorState::Carried);
    }

    if (CarryAnimComponent)
    {
        CarryAnimComponent->BeginCarryingAnim();
    }
    SetKillerState(EKillerState::Carrying);
    if (GetCharacterMovement())
    {
        GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    }
    PendingPickupTarget = nullptr;
    bIsBusy = false;
}

void AKillerCharacter::HandlePickupAttachWindow()
{
    if (HasAuthority() && !CarriedSurvivor)
    {
        AttachCarriedSurvivor(PendingPickupTarget.Get());
    }
}

void AKillerCharacter::HandlePickupMontageFinished()
{
    if (HasAuthority())
    {
        PickupSurvivor(PendingPickupTarget.IsValid() ? PendingPickupTarget.Get() : CarriedSurvivor);
    }
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
        
    if (ACharacter* DroppedCharacter = Cast<ACharacter>(CarriedSurvivor))
        {
        if (UCharacterMovementComponent* DroppedMovement = DroppedCharacter->GetCharacterMovement())
        {
            DroppedMovement->NetworkSmoothingMode = ENetworkSmoothingMode::Exponential;
        }
    }
    
    // 1. 살인마와 생존자 간 충돌 무시 해제
    GetCapsuleComponent()->IgnoreActorWhenMoving(CarriedSurvivor, false);
    if (UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(CarriedSurvivor->GetRootComponent()))
    {
        Root->IgnoreActorWhenMoving(this, false);
    }

    // 2. 부착 해제
    CarriedSurvivor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

    // 3. 살인마의 전방으로 생존자를 '순간 이동'시켜서 캡슐 겹침을 방지
    // 150cm 앞에 배치하여 캡슐이 닿지 않게 함
    FVector SafeLocation = GetActorLocation() + (GetActorForwardVector() * 150.f);
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

        UInputAction* JumpOverAction = InputConfig->JumpOverAction.Get();
        if (!JumpOverAction)
        {
            JumpOverAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/InputAction/IA_JumpOver.IA_JumpOver"));
        }
        if (JumpOverAction)
        {
            EIC->BindAction(JumpOverAction, ETriggerEvent::Started, this, &AKillerCharacter::JumpOver);
        }
    }
}

void AKillerCharacter::JumpOver()
{
    Super::JumpOver();

    if (ParkourComponent)
    {
        ParkourComponent->RequestJumpOver();
    }
}

bool AKillerCharacter::IsParkouring() const
{
    return ParkourComponent && ParkourComponent->IsParkouring();
}

void AKillerCharacter::NotifyParkourEnded()
{
    UpdateMovementSpeed();
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
        for (const auto& Result : Overlaps)
        {
            // 1. 생존자 감지 (Downed 상태만)
            if (ASurvivorCharacter* Survivor = Cast<ASurvivorCharacter>(Result.GetActor()))
            {
                if (Survivor->GetSurvivorState() == ESurvivorState::Downed) return Survivor;
            }
            // 2. Cage 감지 (Empty 상태만)
            if (ACage* Cage = Cast<ACage>(Result.GetActor()))
            {
                if (Cage->GetCageStatus() == ECageStatus::Empty) return Cage;
            }
        }
    }
    return nullptr;
}

bool AKillerCharacter::PerformAttackTrace(FVector& OutHitLocation)
{
    OutHitLocation = GetActorLocation() + (GetActorForwardVector() * KillerData->TaserRange);

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
            AActor* HitActor = Overlap.GetActor();
            ASurvivorCharacter* HitSurvivor = Cast<ASurvivorCharacter>(HitActor);
            
            // 1. Cast 실패 여부 확인
            if (!HitSurvivor) 
            {
                UE_LOG(LogTemp, Warning, TEXT("충돌체 감지됨, 하지만 ASurvivorCharacter가 아님: %s"), HitActor ? *HitActor->GetName() : TEXT("Null"));
                continue;
            }

            // 2. 상태 체크 로직
            ESurvivorState SState = HitSurvivor->GetSurvivorState();
            UE_LOG(LogTemp, Warning, TEXT("생존자 감지됨: %s, 상태: %d"), *HitSurvivor->GetName(), (int32)SState);

            if (SState == ESurvivorState::Healthy || SState == ESurvivorState::Injured)
            {
                HitSurvivor->ApplyHit();
                OutHitLocation = HitSurvivor->GetActorLocation();
                // 공격 성공 및 다운 시킨 횟수 기록
                if (ALDPlayerState* LDPlayerState = GetController() ? GetController()->GetPlayerState<ALDPlayerState>() : nullptr)
                {
                    LDPlayerState->RecordKillerHit(SState == ESurvivorState::Injured);
                }
                return true;
            }
        }
    }
    return false;
}

void AKillerCharacter::OnRep_CarriedSurvivor(AActor* PreviousCarriedSurvivor)
{
    if (ACharacter* PreviousCharacter = Cast<ACharacter>(PreviousCarriedSurvivor))
    {
        if (UCharacterMovementComponent* PreviousMovement = PreviousCharacter->GetCharacterMovement())
        {
            PreviousMovement->NetworkSmoothingMode = ENetworkSmoothingMode::Exponential;
        }
    }

    if (CarriedSurvivor)
    {
        ApplyCarryAttachmentTransform(CarriedSurvivor);
        if (CarryAnimComponent)
        {
            CarryAnimComponent->EnsureCarryingMontagePlaying();
        }
    }
}

void AKillerCharacter::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AKillerCharacter, CurrentState);
    DOREPLIFETIME(AKillerCharacter, bIsBusy);
    DOREPLIFETIME(AKillerCharacter, CarriedSurvivor);
}

// 상태 변경 시 모든 클라이언트가 속도·캐리 애니메이션 업데이트
void AKillerCharacter::OnRep_CurrentState()
{
    UpdateMovementSpeed();

    if (CurrentState == EKillerState::Groggy)
    {
        if (UAnimMontage* MontageToPlay = ResolveGroggyMontage())
        {
            USkeletalMeshComponent* SkelMesh = GetMesh();
            UAnimInstance* AnimInstance = SkelMesh ? SkelMesh->GetAnimInstance() : nullptr;
            if (AnimInstance && !AnimInstance->Montage_IsPlaying(MontageToPlay))
            {
                Multicast_PlayGroggyMontage_Implementation();
            }
        }
    }
    else
    {
        StopGroggyMontageLocal();
    }

    if (GroggySoundComponent)
    {
        GroggySoundComponent->SyncToKillerState(CurrentState);
    }

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

// 서버 상호작용 RPC
void AKillerCharacter::Interact() { Server_Interact(); }

void AKillerCharacter::Server_Interact_Implementation()
{
    if (!KillerData || bIsBusy || IsParkouring()) return;
    
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
            const EKillerState PreviousState = CurrentState;
            UE_LOG(LogTemp, Log, TEXT("Killer State 변경: %d -> %d"), (int32)PreviousState, (int32)NewState);
            CurrentState = NewState;
            UpdateMovementSpeed();
            OnRep_CurrentState();

            if (NewState == EKillerState::Groggy)
            {
                Multicast_PlayGroggyMontage();
            }
            else if (PreviousState == EKillerState::Groggy)
            {
                Multicast_StopGroggyMontage();
            }
        }
    }
}

// 상태별 처리 함수 분리
void AKillerCharacter::HandlePickupInteraction()
{
    if (!bCanPickup || !CarryAnimComponent) return;

    AActor* Target = FindInteractableActor(KillerData->PickupRange);
    if (!Target) return;

    ASurvivorCharacter* Survivor = Cast<ASurvivorCharacter>(Target);
    if (!Survivor || Survivor->GetSurvivorState() != ESurvivorState::Downed) return;

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
    if (!TargetCage) return;

    bIsBusy = true;
    SetKillerState(EKillerState::Interacting);
    if (GetCharacterMovement())
    {
        GetCharacterMovement()->DisableMovement();
    }

    if (CarryAnimComponent)
    {
        CarryAnimComponent->EndCarryAnims();
    }

    FTimerHandle TimerHandle;
    GetWorldTimerManager().SetTimer(TimerHandle, [this, TargetCage]() {
        if (ASurvivorCharacter* Survivor = Cast<ASurvivorCharacter>(CarriedSurvivor))
        {
            // 케이지 횟수 기록
            if (ALDPlayerState* LDPlayerState = GetController() ? GetController()->GetPlayerState<ALDPlayerState>() : nullptr)
            {
                LDPlayerState->RecordKillerCage();
            }
            Survivor->EnterCaged(TargetCage);
        }
        TargetCage->SetCageStatus(ECageStatus::Occupied);
        CarriedSurvivor = nullptr;
        
        SetKillerState(EKillerState::Idle);
        bIsBusy = false;
        if (GetCharacterMovement()) GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        
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

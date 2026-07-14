#include "Characters/Killer/KillerCharacter.h"
#include "Systems/Data/KillerData.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "EnhancedInputComponent.h"
#include "Data/SPInputConfigData.h"
#include "EnhancedInputSubsystems.h"
#include "Camera/CameraComponent.h"
#include "Characters/Survivor/SurvivorCharacter.h"
#include "Components/CapsuleComponent.h"
/*<--------- SPKillerFirstPersonMeshComponent 부재에 의한 주석 처리 ----------------------------->
#include "Components/SPKillerFirstPersonMeshComponent.h"
*/
#include "GameFramework/SpringArmComponent.h"
#include "Gameplay/Cage/Cage.h"
#include "Type/SPGameplayTag.h"
#include "Net/UnrealNetwork.h"

AKillerCharacter::AKillerCharacter()
{
    PrimaryActorTick.bCanEverTick = false;

    bUseControllerRotationYaw = true;         
    if (GetCharacterMovement())
    {
        GetCharacterMovement()->bOrientRotationToMovement = false;
    }
    
    charTag.AddTag(SPGameplayTags::Character::Killer);
    /*<--------- SPKillerFirstPersonMeshComponent 부재에 의한 주석 처리 ----------------------------->
    FirstPersonMeshComp = CreateDefaultSubobject<USPKillerFirstPersonMeshComponent>(TEXT("FirstPersonMesh"));
    */
}

void AKillerCharacter::NotifyControllerChanged()
{
    Super::NotifyControllerChanged();
    /*<--------- SPKillerFirstPersonMeshComponent 부재에 의한 주석 처리 ----------------------------->
    if (FirstPersonMeshComp)
    {
        FirstPersonMeshComp->ScheduleSetup();
    }
    */
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

    SkelMesh->SetOwnerNoSee(bShowFirstPersonArmsOnly);
    if (bHideOwnerShadow)
    {
        SkelMesh->SetCastShadow(false);
    }

    for (UMeshComponent* MeshComp : OwnerHiddenMeshComponents)
    {
        if (MeshComp)
        {
            MeshComp->SetOwnerNoSee(true);
            if (bHideOwnerShadow)
            {
                MeshComp->SetCastShadow(false);
            }
        }
    }

    if (bShowFirstPersonArmsOnly && FirstPersonArmsMesh)
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

    if (UCameraComponent* FollowCamera = GetCameraComponent())
    {
        FollowCamera->SetProjectionMode(ECameraProjectionMode::Perspective);
    }

    /*<--------- SPKillerFirstPersonMeshComponent 부재에 의한 주석 처리 ----------------------------->
    if (FirstPersonMeshComp)
    {
        FirstPersonMeshComp->ScheduleSetup();
        FirstPersonMeshComp->EnsureAnimationSetup();
    }*/
    
    if (GetMesh() && SpringArm)
    {
        SpringArm->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, FName("head"));
        SpringArm->SetRelativeLocation(FVector(0, 0, 0)); // 오프셋 조정 필요
    }

    // 2. 1인칭 메쉬 설정
    if (GetMesh())
    {
        GetMesh()->SetOwnerNoSee(true); // 내 몸은 안 보이게
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

void AKillerCharacter::PerformAttack()
{
    if (!KillerData) return;

    SetKillerState(EKillerState::Attacking);
    
    // 공격 시작 시 이동 차단
    if (GetCharacterMovement())
    {
        GetCharacterMovement()->DisableMovement();
    }
    
    // 1. Windup (0.35s)
    FTimerHandle WindupTimer;
    GetWorldTimerManager().SetTimer(WindupTimer, [this]() {
        
        // 2. Active 판정 (0.4s)
        FVector BoxCenter = GetActorLocation() + (GetActorForwardVector() * (KillerData->TaserRange / 2.f));
        FVector BoxExtent = FVector(KillerData->TaserRange / 2.f, KillerData->TaserHitboxRadius, 90.f);
        DrawDebugBox(GetWorld(), BoxCenter, BoxExtent, GetActorRotation().Quaternion(), FColor::Red, false, 2.0f, 0, 2.0f);

        bool bHit = PerformAttackTrace();
        UE_LOG(LogTemp, Warning, TEXT("공격 결과: %s"), bHit ? TEXT("적중!") : TEXT("허공"));

        // 3. 결과에 따른 상태 전이 및 복귀 로직
        auto RestoreMovement = [this]() {
            if (GetCharacterMovement())
            {
                GetCharacterMovement()->SetMovementMode(MOVE_Walking);
            }
            SetKillerState(EKillerState::Idle);
        };

        if (bHit) 
        {
            // 적중: 후딜 시간 뒤 이동 복구
            FTimerHandle RecoveryTimer;
            GetWorldTimerManager().SetTimer(RecoveryTimer, RestoreMovement, KillerData->TaserRecoveryHit, false);
            
        } 
        else 
        {
            // 빗나감: 그로기 진입 후 이동 복구
            SetKillerState(EKillerState::Groggy);
            FTimerHandle GroggyTimer;
            GetWorldTimerManager().SetTimer(GroggyTimer, RestoreMovement, KillerData->TaserGroggyOnMiss, false);
        }
    }, KillerData->TaserWindup, false);
}

void AKillerCharacter::PickupSurvivor(AActor* Target)
{
    if (!Target) return;
    
    if (ASurvivorCharacter* Survivor = Cast<ASurvivorCharacter>(Target))
    {
        Survivor->SetSurvivorState(ESurvivorState::Carried);
    }
    
    CarriedSurvivor = Target;

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

    // 4. [핵심] 캡슐 반지름보다 더 먼 위치(예: 150cm)로 강제 배치
    CarriedSurvivor->AttachToActor(this, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
    CarriedSurvivor->SetActorRelativeLocation(FVector(150.f, 0.f, 0.f)); 

    SetKillerState(EKillerState::Carrying);
}

void AKillerCharacter::DropSurvivor()
{
    if (!CarriedSurvivor) return;

    if (ASurvivorCharacter* Survivor = Cast<ASurvivorCharacter>(CarriedSurvivor))
    {
        Survivor->SetSurvivorState(ESurvivorState::Downed);
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
}

// 상태 변경 시 모든 클라이언트가 속도 업데이트
void AKillerCharacter::OnRep_CurrentState() { UpdateMovementSpeed(); }

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
    if (!bCanPickup) return;

    AActor* Target = FindInteractableActor(KillerData->PickupRange);
    if (!Target) return;

    bIsBusy = true;
    SetKillerState(EKillerState::PickingUp);
    GetCharacterMovement()->DisableMovement();

    FTimerHandle TimerHandle;
    GetWorldTimerManager().SetTimer(TimerHandle, [this, Target]() {
        PickupSurvivor(Target);
        if (GetCharacterMovement()) GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        bIsBusy = false;
    }, KillerData->PickupDuration, false);
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

    FTimerHandle TimerHandle;
    GetWorldTimerManager().SetTimer(TimerHandle, [this, TargetCage]() {
        if (ASurvivorCharacter* Survivor = Cast<ASurvivorCharacter>(CarriedSurvivor))
        {
            Survivor->SetSurvivorState(ESurvivorState::Caged);
            Survivor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
            Survivor->SetActorLocation(TargetCage->GetActorLocation());
        }
        
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
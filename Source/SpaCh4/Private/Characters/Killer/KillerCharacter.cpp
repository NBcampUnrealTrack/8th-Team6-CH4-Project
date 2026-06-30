#include "Characters/Killer/KillerCharacter.h"
#include "Systems/Data/KillerData.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "EnhancedInputComponent.h"
#include "Data/SPInputConfigData.h"
#include "EnhancedInputSubsystems.h"
#include "Characters/Survivor/SurvivorCharacter.h"

AKillerCharacter::AKillerCharacter() { PrimaryActorTick.bCanEverTick = false; }

void AKillerCharacter::PostInitializeComponents()
{
    Super::PostInitializeComponents();
    if (GetCharacterMovement()) GetCharacterMovement()->SetMovementMode(MOVE_Walking);
}

void AKillerCharacter::BeginPlay()
{
    Super::BeginPlay();
    
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
}

void AKillerCharacter::Interact()
{
    if (!KillerData || bIsBusy) return;

    if (CurrentState == EKillerState::Idle)
    {
        if (AActor* Target = FindInteractableActor(KillerData->PickupRange)) 
        {
            bIsBusy = true;
            SetKillerState(EKillerState::PickingUp);
            GetCharacterMovement()->DisableMovement();

            FTimerHandle TimerHandle;
            GetWorldTimerManager().SetTimer(TimerHandle, [this, Target]() {
                PickupSurvivor(Target);
                GetCharacterMovement()->SetMovementMode(MOVE_Walking);
                bIsBusy = false;
            }, KillerData->PickupDuration, false);
        }
    }
    else if (CurrentState == EKillerState::Carrying)
    {
        bIsBusy = true;
        SetKillerState(EKillerState::Interacting);
        GetCharacterMovement()->DisableMovement();

        FTimerHandle TimerHandle;
        GetWorldTimerManager().SetTimer(TimerHandle, [this]() {
            DropSurvivor();
            GetCharacterMovement()->SetMovementMode(MOVE_Walking);
            bIsBusy = false;
        }, KillerData->CageDepositDuration, false);
    }
}

void AKillerCharacter::Attack()
{
    if (CurrentState == EKillerState::Idle) PerformAttack();
}

void AKillerCharacter::PerformAttack()
{
    if (!KillerData) return;

    SetKillerState(EKillerState::Attacking);
    
    // 1. Windup (0.35s)
    FTimerHandle WindupTimer;
    GetWorldTimerManager().SetTimer(WindupTimer, [this]() {
        
        // 2. Active 판정 (0.4s)
        FVector BoxCenter = GetActorLocation() + (GetActorForwardVector() * (KillerData->TaserRange / 2.f));
        FVector BoxExtent = FVector(KillerData->TaserRange / 2.f, KillerData->TaserHitboxRadius, 90.f);
        DrawDebugBox(GetWorld(), BoxCenter, BoxExtent, GetActorRotation().Quaternion(), FColor::Red, false, KillerData->TaserActive);

        bool bHit = PerformAttackTrace();
        UE_LOG(LogTemp, Warning, TEXT("공격 결과: %s"), bHit ? TEXT("적중!") : TEXT("허공"));

        // 3. 결과에 따른 상태 전이 및 유지 시간 적용
        if (bHit) 
        {
            // 적중: 총 1.05s 시점에 Idle 복귀 (후딜 0.3s 반영)
            // Attacking 상태를 유지하다가 후딜 끝날 때 Idle로
            FTimerHandle RecoveryTimer;
            GetWorldTimerManager().SetTimer(RecoveryTimer, [this]() { 
                SetKillerState(EKillerState::Idle); 
            }, KillerData->TaserRecoveryHit, false);
        } 
        else 
        {
            // 빗나감: 그로기 진입 (1.75s 동안 유지)
            SetKillerState(EKillerState::Groggy);
            
            FTimerHandle GroggyTimer;
            GetWorldTimerManager().SetTimer(GroggyTimer, [this]() { 
                SetKillerState(EKillerState::Idle); 
            }, KillerData->TaserGroggyOnMiss, false);
        }
    }, KillerData->TaserWindup, false);
}

bool AKillerCharacter::PerformAttackTrace()
{
    // 공격 박스 설정
    FVector BoxCenter = GetActorLocation() + (GetActorForwardVector() * (KillerData->TaserRange / 2.f));
    FVector BoxExtent = FVector(KillerData->TaserRange / 2.f, KillerData->TaserHitboxRadius, 90.f);
    DrawDebugBox(GetWorld(), BoxCenter, BoxExtent, GetActorRotation().Quaternion(), FColor::Red, false, KillerData->TaserActive, 0, 2.0f);

    TArray<FOverlapResult> Overlaps;
    FCollisionShape BoxShape = FCollisionShape::MakeBox(BoxExtent);
    FCollisionQueryParams Params(NAME_None, false, this); // 자신 제외

    bool bAnyHit = GetWorld()->OverlapMultiByChannel(Overlaps, BoxCenter, GetActorRotation().Quaternion(), ECC_Pawn, BoxShape, Params);

    bool bHitSurvivor = false;
    if (bAnyHit)
    {
        for (const FOverlapResult& Overlap : Overlaps)
        {
            if (ASurvivorCharacter* HitSurvivor = Cast<ASurvivorCharacter>(Overlap.GetActor()))
            {
                UE_LOG(LogTemp, Warning, TEXT("공격 적중: %s"), *HitSurvivor->GetName());
                bHitSurvivor = true;
                // [참고: 여기서 생존자에게 데미지 함수 호출]
            }
        }
    }
    return bHitSurvivor;
}

void AKillerCharacter::PickupSurvivor(AActor* Target)
{
    CarriedSurvivor = Target;
    Cast<UPrimitiveComponent>(CarriedSurvivor->GetRootComponent())->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    CarriedSurvivor->AttachToActor(this, FAttachmentTransformRules::SnapToTargetIncludingScale);
    SetKillerState(EKillerState::Carrying);
}

void AKillerCharacter::DropSurvivor()
{
    if (CarriedSurvivor)
    {
        Cast<UPrimitiveComponent>(CarriedSurvivor->GetRootComponent())->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        CarriedSurvivor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
        CarriedSurvivor = nullptr;
    }
    SetKillerState(EKillerState::Idle);
}

void AKillerCharacter::SetKillerState(EKillerState NewState)
{
    if (CurrentState != NewState)
    {
        UE_LOG(LogTemp, Log, TEXT("Killer State 변경: %d -> %d"), (int32)CurrentState, (int32)NewState);
    }
    
    CurrentState = NewState;
    
    UpdateMovementSpeed();
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

    if (CurrentState == EKillerState::Attacking) // 공격 중 50% 속도
        Multiplier = KillerData->TaserAttackSpeedMultiplier; 
    else if (CurrentState == EKillerState::Carrying) 
        Multiplier = KillerData->KillerCarrySpeedMultiplier;
    else if (CurrentState == EKillerState::Groggy) 
        Multiplier = KillerData->KillerGroggySpeedMultiplier;

    GetCharacterMovement()->MaxWalkSpeed = KillerData->KillerBaseSpeed * Multiplier;
}

AActor* AKillerCharacter::FindInteractableActor(float Radius)
{
    TArray<FOverlapResult> Overlaps;
    FCollisionShape Sphere = FCollisionShape::MakeSphere(Radius);
    FCollisionQueryParams Params(NAME_None, false, this);
    
    DrawDebugSphere(GetWorld(), GetActorLocation(), Radius, 12, FColor::Yellow, false, 2.0f);
    bool bResult = GetWorld()->OverlapMultiByChannel(Overlaps, GetActorLocation(), FQuat::Identity, ECC_Pawn, Sphere, Params);
    
    if (bResult)
    {
        for (const auto& Result : Overlaps)
        {
            if (ASurvivorCharacter* Survivor = Cast<ASurvivorCharacter>(Result.GetActor()))
            {
                UE_LOG(LogTemp, Warning, TEXT("생존자 감지됨: %s"), *Survivor->GetName());
                return Survivor;
            }
        }
    }
    return nullptr;
}

/*bool AKillerCharacter::PerformAttackTrace()
{
    FHitResult Hit;
    FVector Start = GetActorLocation();
    FVector End = Start + (GetActorForwardVector() * KillerData->TaserRange);
    FCollisionShape Box = FCollisionShape::MakeBox(FVector(KillerData->TaserRange / 2.f, KillerData->TaserHitboxRadius, 90.f));

    bool bHit = GetWorld()->SweepSingleByChannel(Hit, Start, End, GetActorRotation().Quaternion(), ECC_Pawn, Box);
    
    if (bHit && Hit.GetActor())
    {
        ASurvivorCharacter* HitSurvivor = Cast<ASurvivorCharacter>(Hit.GetActor());
        
        if (HitSurvivor)
        {
            // ESurvivorState::Healthy, ESurvivorState::Injured 값을 직접 비교
            
            ESurvivorState SurvivorState = HitSurvivor->GetSurvivorState(); 
            
            if (SurvivorState == ESurvivorState::Healthy || SurvivorState == ESurvivorState::Injured)
            {
                return true; // 공격 성공!
            }
        }
    }
    
    return false;
}*/
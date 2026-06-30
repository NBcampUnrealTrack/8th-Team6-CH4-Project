#include "Characters/Killer/KillerCharacter.h"
#include "Systems/Data/KillerData.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "EnhancedInputComponent.h"
#include "Data/InputConfigData.h"
#include "EnhancedInputSubsystems.h"

AKillerCharacter::AKillerCharacter() { PrimaryActorTick.bCanEverTick = false; }

void AKillerCharacter::PostInitializeComponents()
{
    Super::PostInitializeComponents();
    if (GetCharacterMovement())
    {
        GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    }
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
                    if (Entry.MappingContext)
                    {
                        Subsystem->AddMappingContext(Entry.MappingContext, Entry.Priority);
                    }
                }
            }
        }
    }
    
    // 3. 이동 모드 강제 설정
    if (GetCharacterMovement())
    {
        GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    }
}

void AKillerCharacter::Interact()
{
    if (!KillerData) 
    {
        UE_LOG(LogTemp, Error, TEXT("Interact 실패: KillerData가 없습니다!"));
        return;
    }
    
    if (bIsBusy)
    {
        UE_LOG(LogTemp, Warning, TEXT("Interact 실패: 현재 bIsBusy 상태입니다."));
        return;
    }

    // 1. Idle 상태일 때: 생존자 들기 (PickingUp)
    if (CurrentState == EKillerState::Idle)
    {
        AActor* Target = FindInteractableActor(KillerData->PickupRange);
        if (Target)
        {
            bIsBusy = true;
            SetKillerState(EKillerState::PickingUp);
            UE_LOG(LogTemp, Warning, TEXT("PickingUp 상태 진입, 시전 시간: %f초"), KillerData->PickupDuration);
            
            GetCharacterMovement()->DisableMovement();

            FTimerHandle TimerHandle;
            GetWorldTimerManager().SetTimer(TimerHandle, [this, Target]() {
                PickupSurvivor(Target);
                GetCharacterMovement()->SetMovementMode(MOVE_Walking);
                bIsBusy = false;
                UE_LOG(LogTemp, Warning, TEXT("Pickup 완료: 상태 Carrying으로 변경"));
            }, KillerData->PickupDuration, false);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("Interact 실패: 주변에 들 수 있는 생존자가 없습니다."));
        }
    }
    // 2. Carrying 상태일 때: 내려놓기/수감 (Interacting)
    else if (CurrentState == EKillerState::Carrying)
    {
        bIsBusy = true;
        SetKillerState(EKillerState::Interacting);
        UE_LOG(LogTemp, Warning, TEXT("Interacting 상태 진입, 시전 시간: %f초"), KillerData->CageDepositDuration);
        
        GetCharacterMovement()->DisableMovement();

        FTimerHandle TimerHandle;
        GetWorldTimerManager().SetTimer(TimerHandle, [this]() {
            DropSurvivor();
            GetCharacterMovement()->SetMovementMode(MOVE_Walking);
            bIsBusy = false;
            UE_LOG(LogTemp, Warning, TEXT("Drop 완료: 상태 Idle로 복귀"));
        }, KillerData->CageDepositDuration, false);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Interact 실패: 현재 상태가 Idle이나 Carrying이 아닙니다. (상태: %d)"), (int32)CurrentState);
    }
}

void AKillerCharacter::Attack()
{
    UE_LOG(LogTemp, Warning, TEXT("공격 키 입력됨! 현재 상태: %d"), (int32)CurrentState);
    
    if (CurrentState == EKillerState::Idle) 
    {
        PerformAttack();
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("공격 실패: Idle 상태가 아님!"));
    }
}

void AKillerCharacter::PerformAttack()
{
    if (!KillerData) return;

    SetKillerState(EKillerState::Attacking);
    
    // 1. 박스 위치를 확실하게 캐릭터 바로 앞 50cm 지점으로 고정
    FVector Start = GetActorLocation(); 
    FVector Forward = GetActorForwardVector();
    FVector BoxCenter = Start + (Forward * 100.0f); // 캐릭터 앞 100cm에 생성
    
    FVector BoxExtent = FVector(100.f, 50.f, 50.f);
    FQuat Rotation = GetActorRotation().Quaternion();

    // 2. 디버그 박스 색상을 녹색(성공)으로 하고, 5초 동안 유지되게 해서 확실히 확인
    DrawDebugBox(GetWorld(), BoxCenter, BoxExtent, Rotation, FColor::Green, false, 5.0f, 0, 5.0f);

    FTimerHandle TimerHandle;
    GetWorldTimerManager().SetTimer(TimerHandle, [this, BoxCenter, BoxExtent, Rotation]() {
        FHitResult Hit;
        // Sweep으로 실제 충돌 검사
        bool bHit = GetWorld()->SweepSingleByChannel(Hit, GetActorLocation(), GetActorLocation() + (GetActorForwardVector() * 200.f), Rotation, ECC_Pawn, FCollisionShape::MakeBox(BoxExtent));
        
        UE_LOG(LogTemp, Warning, TEXT("공격 결과: %s"), bHit ? TEXT("적중!") : TEXT("허공"));

        SetKillerState(bHit ? EKillerState::Idle : EKillerState::Groggy);
    }, KillerData->TaserWindup, false);
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
    CurrentState = NewState;
    
    // 상태가 변할 때마다 모드가 풀리지 않도록 강제 설정
    if (GetCharacterMovement())
    {
        GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    }
    
    UpdateMovementSpeed();
}

void AKillerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    // 1. 부모의 기본 바인딩 (Move, Look 등) 처리
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    
    UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
    if (EIC && InputConfig)
    {
        // 2. 공격 바인딩
        if (InputConfig->AttackAction)
        {
            EIC->BindAction(InputConfig->AttackAction, ETriggerEvent::Started, this, &AKillerCharacter::Attack);
        }

        // 3. 상호작용 바인딩 (R키와 E키가 모두 IA_Interact에 할당되어 있다면 하나만 바인딩하면 됩니다)
        if (InputConfig->InteractAction)
        {
            EIC->BindAction(InputConfig->InteractAction, ETriggerEvent::Started, this, &AKillerCharacter::Interact);
            UE_LOG(LogTemp, Warning, TEXT("상호작용 키(R/E) 바인딩 완료!"));
        }
    }
}

void AKillerCharacter::UpdateMovementSpeed()
{
    if (!KillerData) 
    {
        GetCharacterMovement()->MaxWalkSpeed = 748.f; // 안전장치
        return;
    }
    
    float Multiplier = (CurrentState == EKillerState::Carrying) ? KillerData->KillerCarrySpeedMultiplier : 
                       (CurrentState == EKillerState::Groggy ? KillerData->KillerGroggySpeedMultiplier : 1.0f);
    GetCharacterMovement()->MaxWalkSpeed = KillerData->KillerBaseSpeed * Multiplier;
}

AActor* AKillerCharacter::FindInteractableActor(float Radius)
{
    TArray<FOverlapResult> Overlaps;
    FCollisionShape Sphere = FCollisionShape::MakeSphere(Radius);
    
    // 범위 시각화 (디버그 용)
    DrawDebugSphere(GetWorld(), GetActorLocation(), Radius, 12, FColor::Yellow, false, 2.0f);

    bool bResult = GetWorld()->OverlapMultiByChannel(Overlaps, GetActorLocation(), FQuat::Identity, ECC_Pawn, Sphere, FCollisionQueryParams(NAME_None, false, this));
    
    if (!bResult)
    {
        UE_LOG(LogTemp, Warning, TEXT("감지된 물체가 하나도 없습니다. 범위: %f"), Radius);
        return nullptr;
    }

    for (const auto& Result : Overlaps)
    {
        AActor* HitActor = Result.GetActor();
        if (HitActor)
        {
            // 여기서 태그 확인 로그 출력
            bool bHasTag = HitActor->ActorHasTag("DownedSurvivor");
            UE_LOG(LogTemp, Warning, TEXT("감지된 액터: %s, 태그 포함 여부: %d"), *HitActor->GetName(), bHasTag);
            
            if (bHasTag) return HitActor;
        }
    }
    return nullptr;
}

bool AKillerCharacter::PerformAttackTrace()
{
    if (!KillerData) return false;
    FHitResult Hit;
    return GetWorld()->SweepSingleByChannel(Hit, GetActorLocation(), GetActorLocation() + (GetActorForwardVector() * KillerData->TaserRange), FQuat::Identity, ECC_Pawn, FCollisionShape::MakeBox(FVector(KillerData->TaserRange / 2, KillerData->TaserHitboxRadius, 75.f)));
}
#include "Characters/Killer/KillerCharacter.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Data/InputConfigData.h"

AKillerCharacter::AKillerCharacter() { bReplicates = true; }

void AKillerCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AKillerCharacter, CurrentState);
    DOREPLIFETIME(AKillerCharacter, CarriedSurvivor);
}

void AKillerCharacter::BeginPlay()
{
    Super::BeginPlay();
    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        if (auto* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
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
}

void AKillerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        if (AttackAction) EIC->BindAction(AttackAction, ETriggerEvent::Started, this, &AKillerCharacter::HandleAttack);
        if (InteractAction) EIC->BindAction(InteractAction, ETriggerEvent::Started, this, &AKillerCharacter::HandleInteract);
    }
}

void AKillerCharacter::HandleAttack() { if (CurrentState == EKillerState::Idle) Server_Attack(); }

void AKillerCharacter::Server_Attack_Implementation()
{
    SetKillerState(EKillerState::Attacking);
    FTimerHandle TimerHandle;
    GetWorldTimerManager().SetTimer(TimerHandle, [this]() {
        bool bHit = PerformAttackTrace();
        SetKillerState(bHit ? EKillerState::Idle : EKillerState::Groggy);
        if (!bHit) {
            FTimerHandle GroggyTimer;
            GetWorldTimerManager().SetTimer(GroggyTimer, [this]() { SetKillerState(EKillerState::Idle); }, 1.0f, false);
        }
    }, 0.35f, false);
}

void AKillerCharacter::HandleInteract() { Server_Interact(); }

void AKillerCharacter::Server_Interact_Implementation()
{
    // 생존자를 들거나 케이지에 거는 로직 시작점
    if (CurrentState == EKillerState::Idle) SetKillerState(EKillerState::PickingUp);
    else if (CurrentState == EKillerState::Carrying) SetKillerState(EKillerState::Interacting);
}

void AKillerCharacter::Server_PickupSurvivor_Implementation(AActor* TargetSurvivor)
{
    if (TargetSurvivor)
    {
        CarriedSurvivor = TargetSurvivor;
        SetKillerState(EKillerState::Carrying);
    }
}

bool AKillerCharacter::PerformAttackTrace()
{
    // 1. 설정
    FVector Forward = GetActorForwardVector();
    float TraceRange = 250.f; // 사거리
    FVector Start = GetActorLocation() + (Forward * 20.f) + FVector(0, 0, 80.f);
    FVector End = Start + (Forward * TraceRange);
    
    // 공격 판정 크기 (기획에 맞게 조절)
    FVector BoxExtent = FVector(TraceRange / 2.0f, 100.f, 75.f); 
    FCollisionShape Box = FCollisionShape::MakeBox(BoxExtent);
    FQuat Rotation = GetActorRotation().Quaternion();

    // 2. 공격 범위 시각화 (캐릭터 앞부터 사거리 끝까지의 박스)
    // 박스의 중심점은 Start와 End의 중간 위치입니다.
    FVector Center = (Start + End) / 2.0f;
    
    // DrawDebugBox(World, Center, Extent, Rotation, Color, Persistent, LifeTime, DepthPriority, Thickness)
    DrawDebugBox(GetWorld(), Center, BoxExtent, Rotation, FColor::Red, false, 1.0f, 0, 5.0f);

    FHitResult Hit;
    return GetWorld()->SweepSingleByChannel(Hit, Start, End, Rotation, ECC_Pawn, Box);
}

void AKillerCharacter::SetKillerState(EKillerState NewState)
{
    if (HasAuthority())
    {
        CurrentState = NewState;
        UpdateMovementSpeed();
        OnRep_KillerState();
    }
}

void AKillerCharacter::UpdateMovementSpeed()
{
    float Multiplier = (CurrentState == EKillerState::Carrying) ? 0.9f : ((CurrentState == EKillerState::Groggy) ? 0.7f : 1.0f);
    GetCharacterMovement()->MaxWalkSpeed = BaseSpeed * Multiplier;
}

void AKillerCharacter::OnRep_KillerState() { /* 애니메이션 몽타주 재생 로직 작성 위치 */ }
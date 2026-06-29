#include "Characters/Killer/KillerCharacter.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnhancedInputComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"

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
    if (GetCharacterMovement()) GetCharacterMovement()->SetMovementMode(MOVE_Walking);
}

void AKillerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    // 부모 클래스의 Move/Look 바인딩 수행
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    
    if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        if (AttackAction) EIC->BindAction(AttackAction, ETriggerEvent::Started, this, &AKillerCharacter::HandleAttack);
        if (InteractAction) EIC->BindAction(InteractAction, ETriggerEvent::Started, this, &AKillerCharacter::HandleInteract);
        
        UE_LOG(LogTemp, Warning, TEXT("[DEBUG] 총 바인드된 액션 수: %d"), EIC->GetNumActionBindings());
    }
}

void AKillerCharacter::UpdateMovementSpeed()
{
    float Multiplier = (CurrentState == EKillerState::Carrying) ? 0.9f : (CurrentState == EKillerState::Groggy ? 0.7f : 1.0f);
    if (GetCharacterMovement()) 
    {
        GetCharacterMovement()->MaxWalkSpeed = BaseSpeed * Multiplier;
        if (GetCharacterMovement()->MovementMode == MOVE_None) GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    }
}

void AKillerCharacter::OnRep_KillerState() { UpdateMovementSpeed(); }

AActor* AKillerCharacter::FindInteractableActor(float Radius)
{
    TArray<FOverlapResult> Overlaps;
    FCollisionShape Sphere = FCollisionShape::MakeSphere(Radius);
    if (GetWorld()->OverlapMultiByChannel(Overlaps, GetActorLocation(), FQuat::Identity, ECC_WorldDynamic, Sphere, FCollisionQueryParams(NAME_None, false, this)))
    {
        for (const auto& Result : Overlaps)
            if (Result.GetActor()->ActorHasTag("DownedSurvivor") || Result.GetActor()->ActorHasTag("Cage"))
                return Result.GetActor();
    }
    return nullptr;
}

void AKillerCharacter::HandleAttack(const FInputActionValue& Value) { if (CurrentState == EKillerState::Idle) Server_Attack(); }
void AKillerCharacter::HandleInteract(const FInputActionValue& Value) { Server_Interact(); }

void AKillerCharacter::Server_Interact_Implementation()
{
    if (CurrentState == EKillerState::Idle)
    {
        if (AActor* Target = FindInteractableActor(200.f))
            if (Target->ActorHasTag("DownedSurvivor")) Server_PickupSurvivor(Target);
    }
    else if (CurrentState == EKillerState::Carrying)
    {
        AActor* Target = FindInteractableActor(200.f);
        if (Target && Target->ActorHasTag("Cage")) Server_HookSurvivor();
        else Server_DropSurvivor();
    }
}

void AKillerCharacter::Server_PickupSurvivor_Implementation(AActor* TargetSurvivor)
{
    if (TargetSurvivor)
    {
        CarriedSurvivor = TargetSurvivor;
        Cast<UPrimitiveComponent>(CarriedSurvivor->GetRootComponent())->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        CarriedSurvivor->AttachToActor(this, FAttachmentTransformRules::SnapToTargetIncludingScale);
        SetKillerState(EKillerState::Carrying);
    }
}

void AKillerCharacter::Server_DropSurvivor_Implementation()
{
    if (CarriedSurvivor)
    {
        Cast<UPrimitiveComponent>(CarriedSurvivor->GetRootComponent())->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        CarriedSurvivor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
        CarriedSurvivor = nullptr;
    }
    SetKillerState(EKillerState::Idle);
}

void AKillerCharacter::Server_HookSurvivor_Implementation() { Server_DropSurvivor(); }

void AKillerCharacter::Server_Attack_Implementation()
{
    SetKillerState(EKillerState::Attacking);
    FTimerHandle TimerHandle;
    GetWorldTimerManager().SetTimer(TimerHandle, [this]() {
        bool bHit = PerformAttackTrace();
        SetKillerState(bHit ? EKillerState::Idle : EKillerState::Groggy);
    }, 0.35f, false);
}

bool AKillerCharacter::PerformAttackTrace()
{
    FHitResult Hit;
    FVector Start = GetActorLocation() + (GetActorForwardVector() * 50.f);
    FVector End = Start + (GetActorForwardVector() * 200.f);
    return GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeBox(FVector(100.f, 50.f, 50.f)));
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
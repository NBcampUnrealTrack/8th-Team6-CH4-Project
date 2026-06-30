#include "Characters/Survivor/SurvivorCharacter.h"

#include "GameFramework/CharacterMovementComponent.h"
#include "Interface/Interactable.h"
#include "Net/UnrealNetwork.h"

ASurvivorCharacter::ASurvivorCharacter()
{
	
}

void ASurvivorCharacter::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(ASurvivorCharacter, SurvivorState);
}

void ASurvivorCharacter::SetSurvivorState(ESurvivorState NewState)
{
	if (!HasAuthority() || SurvivorState == NewState) return;
	
	SurvivorState = NewState;
	ApplyStateEffects();
}

bool ASurvivorCharacter::CanMove() const
{
	return SurvivorState == ESurvivorState::Healthy || SurvivorState == ESurvivorState::Injured || SurvivorState == ESurvivorState::Downed;
}

bool ASurvivorCharacter::CanInteract() const
{
	return SurvivorState == ESurvivorState::Healthy || SurvivorState == ESurvivorState::Injured;
}

bool ASurvivorCharacter::Server_Interact_Validate()
{
	// 일단 모든 요청이 신뢰가 있다고 가정
	return true;
}

void ASurvivorCharacter::Server_Interact_Implementation()
{
	UE_LOG(LogTemp, Warning, TEXT("Before Action Activate"));
	if (!CanInteract()) return;
	UE_LOG(LogTemp, Warning, TEXT("After Action Activate"));
	
	FVector CameraLocation; 
	FRotator CameraRotation;
	GetController()->GetPlayerViewPoint(CameraLocation, CameraRotation);
	
	const FVector Start = CameraLocation;
	const FVector End   = Start + CameraRotation.Vector() * InteractReach;
	
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	
	FHitResult Hit;
	const bool bHit = GetWorld()->SweepSingleByChannel(
		Hit, Start, End, FQuat::Identity,
		ECC_GameTraceChannel1, // 커스텀 채널(InteractTrace)                               
		FCollisionShape::MakeSphere(InteractRadius), Params);
	
	if (bDrawDebug)
	{
		DrawDebugSphere(GetWorld(), End, InteractRadius, 12, FColor::Green, false, 5.f);
	}
	
	if (bHit && Hit.GetActor() && Hit.GetActor()->Implements<UInteractable>())
	{
		IInteractable::Execute_Interact(Hit.GetActor(), this);
	}
}

void ASurvivorCharacter::Interact()
{
	Super::Interact();
	
	Server_Interact();
}

void ASurvivorCharacter::JumpOver()
{
	Super::JumpOver();

	if (!CanJumpOver())
	{
		return;
	}

	// TODO: JumpOver 행동 처리
	// 전방으로 'JumpOverTrace' 전용 콜리전 채널 트레이스 → 창틀/난간 등. JumpOver 표면만 식별.
	// 서버에서 목표 지점 구하기 → 보간 이동 → Multicast 동기화
}

bool ASurvivorCharacter::CanJumpOver() const
{
	// 공중에서는 바로 넘지 못하도록
	if (const UCharacterMovementComponent* CMC = GetCharacterMovement())
	{
		if (CMC->IsFalling())
		{
			return false;
		}
	}

	// TODO: 운반 중일 때도 넘어갈 수 있을지? 
	return SurvivorState == ESurvivorState::Healthy || SurvivorState == ESurvivorState::Injured;
}

void ASurvivorCharacter::OnRep_SurvivorState()
{
	ApplyStateEffects();
}

void ASurvivorCharacter::ApplyStateEffects()
{
	/*switch (SurvivorState)
	{
		// 각 State에 따른 분기를 처리해야 함.
		// Healthy/Injured: 이동 가능
		// Downed 이동 속도 금격히 감소, 상호작용 불가
		// Dead/Caged/Carried/Escaped: 이동 불가, 상호작용 불가
	}*/
}

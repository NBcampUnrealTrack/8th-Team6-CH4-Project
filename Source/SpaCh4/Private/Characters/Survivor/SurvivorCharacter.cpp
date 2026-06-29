#include "Characters/Survivor/SurvivorCharacter.h"

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

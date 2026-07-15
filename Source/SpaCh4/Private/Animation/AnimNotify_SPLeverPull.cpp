#include "Animation/AnimNotify_SPLeverPull.h"

#include "Characters/Survivor/SurvivorCharacter.h"
#include "Components/SPEscapeLeverComponent.h"
#include "Components/SkeletalMeshComponent.h"

void UAnimNotify_SPLeverPull::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp)
	{
		return;
	}

	if (ASurvivorCharacter* Survivor = Cast<ASurvivorCharacter>(MeshComp->GetOwner()))
	{
		if (USPEscapeLeverComponent* LeverComponent = Survivor->GetEscapeLeverComponent())
		{
			LeverComponent->OnLeverPullNotify();
		}
	}
}

FString UAnimNotify_SPLeverPull::GetNotifyName_Implementation() const
{
	return TEXT("SP Lever Pull");
}

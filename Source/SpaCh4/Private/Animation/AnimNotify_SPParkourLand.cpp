#include "Animation/AnimNotify_SPParkourLand.h"

#include "Characters/Survivor/SurvivorCharacter.h"
#include "Components/SPParkourComponent.h"
#include "Components/SkeletalMeshComponent.h"

void UAnimNotify_SPParkourLand::Notify(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp)
	{
		return;
	}

	if (ASurvivorCharacter* Survivor = Cast<ASurvivorCharacter>(MeshComp->GetOwner()))
	{
		if (USPParkourComponent* ParkourComponent = Survivor->GetParkourComponent())
		{
			ParkourComponent->OnParkourLandNotify();
		}
	}
}

FString UAnimNotify_SPParkourLand::GetNotifyName_Implementation() const
{
	return TEXT("SP Parkour Land");
}

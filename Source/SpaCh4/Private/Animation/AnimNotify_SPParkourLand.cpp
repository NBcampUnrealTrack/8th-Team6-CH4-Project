#include "Animation/AnimNotify_SPParkourLand.h"

#include "Components/SPParkourComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"

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

	if (ACharacter* Character = Cast<ACharacter>(MeshComp->GetOwner()))
	{
		if (USPParkourComponent* ParkourComponent = Character->FindComponentByClass<USPParkourComponent>())
		{
			ParkourComponent->OnParkourLandNotify();
		}
	}
}

FString UAnimNotify_SPParkourLand::GetNotifyName_Implementation() const
{
	return TEXT("SP Parkour Land");
}

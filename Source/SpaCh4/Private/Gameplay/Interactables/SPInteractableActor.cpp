#include "Gameplay/Interactables/SPInteractableActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

ASPInteractableActor::ASPInteractableActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Mesh->SetCollisionObjectType(ECC_WorldDynamic);
	Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	Mesh->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
}

void ASPInteractableActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASPInteractableActor, InteractCount);
}

void ASPInteractableActor::Interact_Implementation(AActor* Interactor)
{
	if (HasAuthority())
	{
		++InteractCount;
	}
	
	OnInteracted(Interactor);

	if (bDebugLog && GEngine)
	{
		FString TryInteractMessage = FString::Printf(TEXT("Try Interact: %d"), InteractCount);
		GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Cyan, TryInteractMessage);
	}
}

#include "Gameplay/Cage/Cage.h"

#include "Characters/Survivor/SurvivorCharacter.h"
#include "Components/ArrowComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"

namespace
{
	bool IsUnderSupportHierarchy(const USceneComponent* Component, const USceneComponent* SupportRoot, const UStaticMeshComponent* SupportMesh)
	{
		if (!Component)
		{
			return false;
		}

		for (const USceneComponent* Parent = Component->GetAttachParent(); Parent; Parent = Parent->GetAttachParent())
		{
			if (Parent == SupportRoot || Parent == SupportMesh)
			{
				return true;
			}
		}

		return false;
	}
}

ACage::ACage()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	PlacementRoot = CreateDefaultSubobject<USceneComponent>(TEXT("PlacementRoot"));
	SetRootComponent(PlacementRoot);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SceneRoot->SetupAttachment(PlacementRoot);

	CageRoot = CreateDefaultSubobject<USceneComponent>(TEXT("CageRoot"));
	CageRoot->SetupAttachment(SceneRoot);

	SupportMeshScaleRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SupportMeshScaleRoot"));
	SupportMeshScaleRoot->SetupAttachment(CageRoot);

	SupportMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SupportMesh"));
	SupportMesh->SetupAttachment(SupportMeshScaleRoot);
	SupportMesh->SetRelativeScale3D(FVector::OneVector);
	SupportMesh->SetCollisionProfileName(TEXT("BlockAll"));

	CageMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CageMesh"));
	CageMesh->SetupAttachment(CageRoot);
	CageMesh->SetCollisionProfileName(TEXT("BlockAll"));

	DoorPivot = CreateDefaultSubobject<UArrowComponent>(TEXT("DoorPivot"));
	DoorPivot->SetupAttachment(CageMesh);
	DoorPivot->SetRelativeRotation(FRotator::ZeroRotator);
	DoorPivot->SetArrowColor(FLinearColor::Green);
	DoorPivot->ArrowSize = 1.0f;
	DoorPivot->bIsScreenSizeScaled = true;
	DoorPivot->SetHiddenInGame(true);

	DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));
	DoorMesh->SetupAttachment(DoorPivot);
	DoorMesh->SetCollisionProfileName(TEXT("NoCollision"));

	ApplySupportMeshScale();
}

void ACage::EnsureDoorComponentHierarchy()
{
	if (!PlacementRoot || !SceneRoot || !CageRoot || !SupportMeshScaleRoot || !SupportMesh || !CageMesh || !DoorPivot || !DoorMesh)
	{
		return;
	}

	USceneComponent* CurrentRoot = Cast<USceneComponent>(GetRootComponent());

	// Migrate older layouts that used CageRoot or SceneRoot as the actor root.
	if (CurrentRoot == CageRoot)
	{
		PlacementRoot->SetWorldTransform(CageRoot->GetComponentTransform());
		SceneRoot->AttachToComponent(PlacementRoot, FAttachmentTransformRules::KeepWorldTransform);
		CageRoot->AttachToComponent(SceneRoot, FAttachmentTransformRules::KeepWorldTransform);
		SetRootComponent(PlacementRoot);
	}
	else if (CurrentRoot == SceneRoot)
	{
		PlacementRoot->SetWorldTransform(SceneRoot->GetComponentTransform());
		SceneRoot->AttachToComponent(PlacementRoot, FAttachmentTransformRules::KeepWorldTransform);
		SetRootComponent(PlacementRoot);
	}
	else if (CurrentRoot != PlacementRoot)
	{
		PlacementRoot->AttachToComponent(CurrentRoot, FAttachmentTransformRules::KeepWorldTransform);
		SetRootComponent(PlacementRoot);
	}

	if (SceneRoot->GetAttachParent() != PlacementRoot)
	{
		SceneRoot->AttachToComponent(PlacementRoot, FAttachmentTransformRules::KeepWorldTransform);
	}

	if (CageRoot->GetAttachParent() != SceneRoot)
	{
		CageRoot->AttachToComponent(SceneRoot, FAttachmentTransformRules::KeepWorldTransform);
	}

	if (SupportMeshScaleRoot->GetAttachParent() != CageRoot)
	{
		SupportMeshScaleRoot->AttachToComponent(CageRoot, FAttachmentTransformRules::KeepRelativeTransform);
	}

	if (SupportMesh->GetAttachParent() != SupportMeshScaleRoot)
	{
		SupportMesh->AttachToComponent(SupportMeshScaleRoot, FAttachmentTransformRules::KeepRelativeTransform);
	}

	if (CageMesh->GetAttachParent() != CageRoot || IsUnderSupportHierarchy(CageMesh, SupportMeshScaleRoot, SupportMesh))
	{
		CageMesh->AttachToComponent(CageRoot, FAttachmentTransformRules::KeepWorldTransform);
	}

	if (DoorPivot->GetAttachParent() != CageMesh)
	{
		DoorPivot->AttachToComponent(CageMesh, FAttachmentTransformRules::KeepRelativeTransform);
	}
}

void ACage::ApplySupportMeshScale()
{
	if (!SupportMeshScaleRoot || !SupportMesh)
	{
		return;
	}

	if (!SupportMesh->GetRelativeScale3D().Equals(FVector::OneVector, KINDA_SMALL_NUMBER))
	{
		SupportMesh->SetRelativeScale3D(FVector::OneVector);
	}

	const FVector EffectiveScale(
		FMath::Max(0.01f, SupportMeshScale.X),
		FMath::Max(0.01f, SupportMeshScale.Y),
		FMath::Max(0.01f, SupportMeshScale.Z));

	if (!SupportMeshScaleRoot->GetRelativeScale3D().Equals(EffectiveScale, KINDA_SMALL_NUMBER))
	{
		SupportMeshScaleRoot->SetRelativeScale3D(EffectiveScale);
	}
}

void ACage::SyncSupportMeshScaleFromEditorComponent()
{
#if WITH_EDITOR
	if (bApplyingSupportMeshScaleFromProperty || !SupportMeshScaleRoot)
	{
		return;
	}

	const FVector RootScale = SupportMeshScaleRoot->GetRelativeScale3D();
	if (!RootScale.Equals(SupportMeshScale, KINDA_SMALL_NUMBER))
	{
		SupportMeshScale = RootScale;
	}
#endif
}

void ACage::ApplyAssemblyRotation()
{
	if (!SceneRoot || SceneRoot->GetRelativeRotation().Equals(AssemblyRotation, KINDA_SMALL_NUMBER))
	{
		return;
	}

	SceneRoot->SetRelativeRotation(AssemblyRotation);
}

void ACage::SyncAssemblyRotationFromEditorComponent()
{
#if WITH_EDITOR
	if (!SceneRoot)
	{
		return;
	}

	const FRotator RootRotation = SceneRoot->GetRelativeRotation();
	if (!RootRotation.Equals(AssemblyRotation, KINDA_SMALL_NUMBER))
	{
		AssemblyRotation = RootRotation;
	}
#endif
}

void ACage::ApplyDoorMeshAttachment()
{
	if (!CageMesh || !DoorPivot || !DoorMesh)
	{
		return;
	}

	USceneComponent* DesiredParent = bDoorMeshFollowsPivot
		? static_cast<USceneComponent*>(DoorPivot)
		: static_cast<USceneComponent*>(CageMesh);

	if (DoorMesh->GetAttachParent() != DesiredParent)
	{
		DoorMesh->AttachToComponent(DesiredParent, FAttachmentTransformRules::KeepWorldTransform);
	}
}

#if WITH_EDITOR
void ACage::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
	const FName MemberPropertyName = PropertyChangedEvent.MemberProperty
		? PropertyChangedEvent.MemberProperty->GetFName()
		: NAME_None;

	if (
		PropertyName == GET_MEMBER_NAME_CHECKED(ACage, AssemblyRotation)
		|| MemberPropertyName == GET_MEMBER_NAME_CHECKED(ACage, AssemblyRotation))
	{
		EnsureDoorComponentHierarchy();
		ApplyAssemblyRotation();
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(ACage, DoorRotation))
	{
		ApplyDoorRotation();
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(ACage, bDoorMeshFollowsPivot))
	{
		EnsureDoorComponentHierarchy();
		ApplyDoorMeshAttachment();
	}
	else if (
		PropertyName == GET_MEMBER_NAME_CHECKED(ACage, SupportMeshScale)
		|| MemberPropertyName == GET_MEMBER_NAME_CHECKED(ACage, SupportMeshScale))
	{
		bApplyingSupportMeshScaleFromProperty = true;
		EnsureDoorComponentHierarchy();
		ApplySupportMeshScale();
		bApplyingSupportMeshScaleFromProperty = false;
	}
}
#endif

void ACage::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	EnsureDoorComponentHierarchy();
	SyncAssemblyRotationFromEditorComponent();
	ApplyAssemblyRotation();
	SyncSupportMeshScaleFromEditorComponent();
	ApplySupportMeshScale();
	ApplyDoorMeshAttachment();
}

void ACage::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	EnsureDoorComponentHierarchy();
	ApplyAssemblyRotation();
	ApplySupportMeshScale();
	ApplyDoorMeshAttachment();
}

void ACage::BeginPlay()
{
	Super::BeginPlay();

	EnsureDoorComponentHierarchy();
	ApplyAssemblyRotation();
	ApplySupportMeshScale();
	ApplyDoorMeshAttachment();

	if (DoorPivot)
	{
		if (DoorRotation.IsNearlyZero() && !DoorPivot->GetRelativeRotation().IsNearlyZero())
		{
			DoorRotation = DoorPivot->GetRelativeRotation();
		}
		else
		{
			ApplyDoorRotation();
		}
	}
}

void ACage::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ACage, TrappedSurvivor);
}

void ACage::SetCageStatus(ECageStatus NewStatus)
{
	if (CurrentStatus != NewStatus)
	{
		CurrentStatus = NewStatus;

		switch (CurrentStatus)
		{
		case ECageStatus::Empty:
			TrappedSurvivor = nullptr;
			break;
		case ECageStatus::Dead:
			break;
		default:
			break;
		}
	}
}

void ACage::SetDoorRotation(const FRotator& NewRotation)
{
	DoorRotation = NewRotation;
	ApplyDoorRotation();
}

void ACage::ApplyDoorRotation()
{
	if (!DoorPivot || DoorPivot->GetRelativeRotation().Equals(DoorRotation, KINDA_SMALL_NUMBER))
	{
		return;
	}

	DoorPivot->SetRelativeRotation(DoorRotation);
}

FTransform ACage::GetCageMeshTransform() const
{
	if (CageMesh)
	{
		return CageMesh->GetComponentTransform();
	}
	return GetActorTransform();
}

void ACage::HandleSurvivorDeath(ASurvivorCharacter* DeadSurvivor)
{
	if (!CageMesh) return;
	if (DeadSurvivor) DeadSurvivor->Destroy();

	const float UpdateInterval = 0.03f; 
	const float DropAmount = -0.5f;     
	const int32 TotalSteps = 200;     
    
	MoveSteps = 0; 

	GetWorldTimerManager().SetTimer(MoveTimerHandle, [this, DropAmount, TotalSteps]() {
		if (CageMesh && MoveSteps < TotalSteps)
		{
			CageMesh->AddRelativeLocation(FVector(0, 0, DropAmount));
			MoveSteps++;
		}
		else
		{
			GetWorldTimerManager().ClearTimer(MoveTimerHandle);
            
			if (CageMesh) {
				CageMesh->DestroyComponent();
			}
		}
	}, UpdateInterval, true);
}
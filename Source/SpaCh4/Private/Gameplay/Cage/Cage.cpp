#include "Gameplay/Cage/Cage.h"

#include "Characters/Survivor/SurvivorCharacter.h"
#include "Components/ArrowComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "Type/SPCollisionChannels.h"
#include "Type/SPGameplayTag.h"

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
	SupportMesh->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);

	CageMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CageMesh"));
	CageMesh->SetupAttachment(CageRoot);
	CageMesh->SetIsReplicated(true);
	CageMesh->SetCollisionProfileName(TEXT("BlockAll"));
	CageMesh->SetCollisionObjectType(SPCollisionChannels::Cage);
	CageMesh->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
	CageMesh->SetCustomDepthStencilValue(250);
	CageMesh->SetRenderCustomDepth(false);

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
	DoorMesh->SetCustomDepthStencilValue(250);
	DoorMesh->SetRenderCustomDepth(false);

	PrisonerAnchor = CreateDefaultSubobject<UArrowComponent>(TEXT("PrisonerAnchor"));
	PrisonerAnchor->SetupAttachment(CageRoot);
	PrisonerAnchor->SetArrowColor(FLinearColor::Yellow);
	PrisonerAnchor->bIsScreenSizeScaled = true;
	PrisonerAnchor->SetHiddenInGame(true);

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

	if (CageMesh)
	{
		// Existing Blueprint instances may have serialized Component Replicates off.
		CageMesh->SetIsReplicated(true);
	}

	ConfigureCollisionChannels();
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
	DOREPLIFETIME(ACage, CurrentStatus);
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

		ForceNetUpdate();
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

void ACage::ConfigureCollisionChannels()
{
	if (CageMesh)
	{
		CageMesh->SetCollisionObjectType(SPCollisionChannels::Cage);
		CageMesh->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
	}
}

void ACage::SetOccupied(ASurvivorCharacter* Survivor)
{
	TrappedSurvivor = Survivor;
	SetCageStatus(ECageStatus::Occupied);
}

FTransform ACage::GetPrisonerAnchorTransform() const
{
	return PrisonerAnchor ? PrisonerAnchor->GetComponentTransform() : GetActorTransform();
}

void ACage::Interact_Implementation(AActor* Interactor)
{
	if (ASurvivorCharacter* Survivor = Cast<ASurvivorCharacter>(Interactor))
	{
		Survivor->BeginRescue(this);
	}
}

void ACage::SetHighlight_Implementation(bool bEnabled)
{
	if (CageMesh)
	{
		CageMesh->SetRenderCustomDepth(bEnabled);
	}
	if (DoorMesh)
	{
		DoorMesh->SetRenderCustomDepth(bEnabled);
	}
}

FGameplayTag ACage::GetInteractableTag_Implementation() const
{
	return SPGameplayTags::Interactable::Cage;
}

bool ACage::IsInteractable_Implementation() const
{
	return CurrentStatus == ECageStatus::Occupied;
}

USceneComponent* ACage::GetInteractFocusComponent_Implementation() const
{
	return DoorMesh;
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
	// 서버에서 상태 처리 및 생존자 파괴
	if (HasAuthority())
	{
		SetCageStatus(ECageStatus::Dead);
		if (IsValid(DeadSurvivor)) DeadSurvivor->Destroy();
	}

	if (!HasAuthority() || !CageMesh) return;

	TWeakObjectPtr<ACage> WeakThis(this);
	MoveSteps = 0;
	const int32 TotalSteps = 200;

	GetWorldTimerManager().SetTimer(MoveTimerHandle, [WeakThis, TotalSteps]() {
		if (!WeakThis.IsValid()) return;
        
		ACage* Cage = WeakThis.Get();
        
		// 1. 이동 로직: SetRelativeLocation 사용
		if (Cage->MoveSteps < TotalSteps)
		{
			// 부드러운 하강을 위해 현재 위치에서 계속 0.5씩 감소
			FVector NewLoc = Cage->CageMesh->GetRelativeLocation() + FVector(0, 0, -0.5f);
			Cage->Multicast_UpdateCageLocation(NewLoc); // 모든 클라이언트에게 위치 전송
			Cage->MoveSteps++;

			// 2. 디버그 로그 추가 (10스텝마다 한 번씩만 출력하여 로그 도배 방지)
			if (Cage->MoveSteps % 10 == 0)
			{
				UE_LOG(LogTemp, Warning, TEXT("케이지 하강 중... [%d/%d], 현재 위치: %s"), 
					Cage->MoveSteps, TotalSteps, *NewLoc.ToString());
			}
		}
		else
		{
			// 3. 종료 로직
			UE_LOG(LogTemp, Warning, TEXT("케이지 하강 완료. 삭제 처리 시작."));
			Cage->GetWorldTimerManager().ClearTimer(Cage->MoveTimerHandle);
			Cage->Multicast_DestroyCageMesh();
		}
	}, 0.03f, true);
}

void ACage::Multicast_DestroyCageMesh_Implementation()
{
	if (!IsValid(CageMesh)) return;

	// 1. 모든 자식 컴포넌트(DoorMesh 포함)를 찾아서 비활성화
	TArray<USceneComponent*> ChildComponents; // 변수명 변경 (에러 해결)
	CageMesh->GetChildrenComponents(true, ChildComponents);

	for (USceneComponent* Child : ChildComponents)
	{
		if (IsValid(Child))
		{
			Child->SetVisibility(false, true);
			// 충돌 설정 안전 캐스팅
			if (UPrimitiveComponent* PrimitiveChild = Cast<UPrimitiveComponent>(Child))
			{
				PrimitiveChild->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			}
		}
	}

	// 2. 메인 메시와 문을 포함한 모든 자식 지연 삭제
	FTimerHandle DelayHandle;
	GetWorldTimerManager().SetTimer(DelayHandle, [this, ChildComponents]() { // 변경된 변수명 사용
	   // 자식부터 삭제
	   for (USceneComponent* Child : ChildComponents)
	   {
		  if (IsValid(Child)) Child->DestroyComponent();
	   }
        
	   // 마지막으로 메인 메시 삭제
	   if (IsValid(CageMesh))
	   {
		  CageMesh->DestroyComponent();
		  CageMesh = nullptr;
	   }
	}, 0.2f, false);
}

void ACage::Multicast_UpdateCageLocation_Implementation(FVector NewLocation)
{
	if (CageMesh) CageMesh->SetRelativeLocation(NewLocation);
}

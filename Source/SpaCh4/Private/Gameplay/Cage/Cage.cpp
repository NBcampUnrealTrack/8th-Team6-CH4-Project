#include "Gameplay/Cage/Cage.h"
#include "Net/UnrealNetwork.h"

ACage::ACage()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	// 메시 컴포넌트 생성 및 설정
	CageMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CageMesh"));
	RootComponent = CageMesh; // 갈고리 자체의 위치가 메시 위치가 됨
    
	// 필요 시 콜리전 설정
	CageMesh->SetCollisionProfileName(TEXT("BlockAll"));
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
        
		// 상태별 로직 처리
		switch (CurrentStatus)
		{
		case ECageStatus::Empty:
			TrappedSurvivor = nullptr;
			break;
		case ECageStatus::Dead:
			break;
		}
	}
}
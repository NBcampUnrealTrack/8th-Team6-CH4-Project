#include "Gameplay/Parkour/SPParkourZone.h"

#include "Components/BoxComponent.h"
#include "Components/BillboardComponent.h"
#include "Components/SPParkourComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"

ASPParkourZone::ASPParkourZone()
{
	PrimaryActorTick.bCanEverTick = false;

	ZoneBox = CreateDefaultSubobject<UBoxComponent>(TEXT("ZoneBox"));
	SetRootComponent(ZoneBox);
	ZoneBox->SetBoxExtent(FVector(500.f, 500.f, 200.f));
	ZoneBox->SetCollisionProfileName(TEXT("Trigger"));
	ZoneBox->SetGenerateOverlapEvents(true);

#if WITH_EDITORONLY_DATA
	EditorSprite = CreateDefaultSubobject<UBillboardComponent>(TEXT("EditorSprite"));
	if (EditorSprite)
	{
		EditorSprite->SetupAttachment(ZoneBox);
		EditorSprite->SetHiddenInGame(true);
	}
#endif
}

void ASPParkourZone::BeginPlay()
{
	Super::BeginPlay();

	if (ZoneBox)
	{
		ZoneBox->OnComponentBeginOverlap.AddDynamic(this, &ASPParkourZone::OnZoneBeginOverlap);
		ZoneBox->OnComponentEndOverlap.AddDynamic(this, &ASPParkourZone::OnZoneEndOverlap);
	}
}

bool ASPParkourZone::ContainsPoint(const FVector& WorldPoint) const
{
	if (!ZoneBox)
	{
		return false;
	}

	const FTransform BoxTransform = ZoneBox->GetComponentTransform();
	const FVector LocalPoint = BoxTransform.InverseTransformPosition(WorldPoint);
	const FVector Extent = ZoneBox->GetUnscaledBoxExtent();

	return FMath::Abs(LocalPoint.X) <= Extent.X
		&& FMath::Abs(LocalPoint.Y) <= Extent.Y
		&& FMath::Abs(LocalPoint.Z) <= Extent.Z;
}

void ASPParkourZone::OnZoneBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	(void)OverlappedComponent;
	(void)OtherComp;
	(void)OtherBodyIndex;
	(void)bFromSweep;
	(void)SweepResult;

	if (ACharacter* Character = Cast<ACharacter>(OtherActor))
	{
		HandleCharacterOverlap(Character, true);
	}
}

void ASPParkourZone::OnZoneEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex)
{
	(void)OverlappedComponent;
	(void)OtherComp;
	(void)OtherBodyIndex;

	if (ACharacter* Character = Cast<ACharacter>(OtherActor))
	{
		HandleCharacterOverlap(Character, false);
	}
}

void ASPParkourZone::HandleCharacterOverlap(ACharacter* Character, bool bEntered)
{
	if (!Character)
	{
		return;
	}

	if (USPParkourComponent* ParkourComponent = Character->FindComponentByClass<USPParkourComponent>())
	{
		ParkourComponent->RegisterParkourZoneOverlap(this, bEntered);
	}

	if (bDrawDebugBounds && GetWorld())
	{
		const FColor DebugColor = bEntered ? FColor::Green : FColor::Red;
		const FVector Origin = ZoneBox->GetComponentLocation();
		const FVector Extent = ZoneBox->GetScaledBoxExtent();
		DrawDebugBox(GetWorld(), Origin, Extent, ZoneBox->GetComponentQuat(), DebugColor, false, 2.f, 0, 2.f);
	}
}

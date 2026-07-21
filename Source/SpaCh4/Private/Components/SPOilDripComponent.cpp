#include "Components/SPOilDripComponent.h"

#include "Characters/Survivor/SurvivorCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/DecalComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	const FVector2D OilVariantOffsets[] = {
		FVector2D(0.f, 0.f),
		FVector2D(0.5f, 0.f),
		FVector2D(0.f, 0.5f),
		FVector2D(0.5f, 0.5f)
	};

	const FName OilVariantOffsetParam(TEXT("VariantOffset"));
	const FName OilHueOffsetParam(TEXT("HueOffset"));
}

USPOilDripComponent::USPOilDripComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void USPOilDripComponent::BeginPlay()
{
	Super::BeginPlay();

	NextInterval = PickNextInterval();
}

void USPOilDripComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const ASurvivorCharacter* Survivor = GetSurvivor();
	if (!Survivor || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (!IsLeavingDrips())
	{
		TimeSinceLastDrip = 0.f;
		return;
	}

	TimeSinceLastDrip += DeltaTime;
	if (TimeSinceLastDrip < NextInterval)
	{
		return;
	}

	TimeSinceLastDrip = 0.f;
	NextInterval = PickNextInterval();

	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FVector FootLocation = Survivor->GetActorLocation();
	if (const UCapsuleComponent* Capsule = Survivor->GetCapsuleComponent())
	{
		FootLocation.Z -= Capsule->GetScaledCapsuleHalfHeight();
	}

	const FVector TraceStart = FootLocation + FVector(0.f, 0.f, SurfaceTraceUpOffset);
	const FVector TraceEnd = FootLocation - FVector(0.f, 0.f, SurfaceTraceDownDistance);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(OilDrip), false);
	Params.AddIgnoredActor(Survivor);

	FHitResult Hit;
	if (!World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, Params))
	{
		return;
	}

	SpawnOilDrip(Hit.ImpactPoint, Hit.ImpactNormal);
}

void USPOilDripComponent::SpawnOilDrip(FVector Location, FVector Normal) const
{
	if (!OilDecalMaterial)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector Lateral = FVector::CrossProduct(Normal, FVector::ForwardVector).GetSafeNormal();
	const FVector FinalLocation = Location + Lateral * FMath::FRandRange(-LateralJitter, LateralJitter);

	FRotator DecalRotation = FRotationMatrix::MakeFromX(-Normal).Rotator();
	DecalRotation.Roll += FMath::FRandRange(0.f, 360.f);

	const float SizeScale = 1.f + FMath::FRandRange(-DecalSizeJitter, DecalSizeJitter);

	UDecalComponent* Decal = UGameplayStatics::SpawnDecalAtLocation(World, OilDecalMaterial, DecalSize * SizeScale, FinalLocation, DecalRotation, DecalLifetime);
	if (!Decal)
	{
		return;
	}

	if (UMaterialInstanceDynamic* DecalMaterial = Decal->CreateDynamicMaterialInstance())
	{
		const FVector2D Variant = OilVariantOffsets[FMath::RandRange(0, UE_ARRAY_COUNT(OilVariantOffsets) - 1)];
		DecalMaterial->SetVectorParameterValue(OilVariantOffsetParam, FLinearColor(Variant.X, Variant.Y, 0.f, 0.f));
		DecalMaterial->SetScalarParameterValue(OilHueOffsetParam, FMath::FRand());
	}

	Decal->SetSortOrder(DecalSortOrder);
	Decal->SetFadeOut(FMath::Max(0.f, DecalLifetime - DecalFadeDuration), DecalFadeDuration, false);
}

ASurvivorCharacter* USPOilDripComponent::GetSurvivor() const
{
	return Cast<ASurvivorCharacter>(GetOwner());
}

bool USPOilDripComponent::IsLeavingDrips() const
{
	const ASurvivorCharacter* Survivor = GetSurvivor();
	return Survivor && Survivor->GetSurvivorState() == ESurvivorState::Injured;
}

float USPOilDripComponent::PickNextInterval() const
{
	const float Jittered = DripInterval + FMath::FRandRange(-DripIntervalJitter, DripIntervalJitter);
	return FMath::Max(Jittered, DripInterval * 0.25f);
}

#include "Components/SPScratchMarkComponent.h"

#include "Characters/Killer/KillerCharacter.h"
#include "Characters/Survivor/SurvivorCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/DecalComponent.h"
#include "Components/SPMovementComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Type/SPGameplayTag.h"

USPScratchMarkComponent::USPScratchMarkComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void USPScratchMarkComponent::BeginPlay()
{
	Super::BeginPlay();

	NextInterval = PickNextInterval();

	if (const ASurvivorCharacter* Survivor = GetSurvivor())
	{
		CachedMovement = Survivor->FindComponentByClass<USPMovementComponent>();
	}
}

void USPScratchMarkComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const ASurvivorCharacter* Survivor = GetSurvivor();
	// Scratch marks are cosmetic, so derive them from replicated movement on the viewing killer.
	if (!Survivor || !ShouldLocalViewerSeeMarks())
	{
		return;
	}

	const FVector CurrentLocation = Survivor->GetActorLocation();

	if (!IsLeavingMarks())
	{
		LastSampleLocation = CurrentLocation;
		bHasLastSample = true;
		DistanceSinceLastMark = 0.f;
		return;
	}

	if (!bHasLastSample)
	{
		LastSampleLocation = CurrentLocation;
		bHasLastSample = true;
		return;
	}

	DistanceSinceLastMark += FVector::Dist2D(CurrentLocation, LastSampleLocation);
	LastSampleLocation = CurrentLocation;

	if (DistanceSinceLastMark < NextInterval)
	{
		return;
	}

	DistanceSinceLastMark = 0.f;
	NextInterval = PickNextInterval();

	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FVector FootLocation = CurrentLocation;
	if (const UCapsuleComponent* Capsule = Survivor->GetCapsuleComponent())
	{
		FootLocation.Z -= Capsule->GetScaledCapsuleHalfHeight();
	}

	const FVector TraceStart = FootLocation + FVector(0.f, 0.f, SurfaceTraceUpOffset);
	const FVector TraceEnd = FootLocation - FVector(0.f, 0.f, SurfaceTraceDownDistance);

	FVector MarkLocation = FootLocation;
	FVector MarkNormal = FVector::UpVector;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ScratchMark), false);
	Params.AddIgnoredActor(Survivor);

	FHitResult Hit;
	if (World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, Params))
	{
		MarkLocation = Hit.ImpactPoint;
		MarkNormal = Hit.ImpactNormal;
	}

	const float BaseYaw = Survivor->GetVelocity().Rotation().Yaw;
	SpawnScratchMark(MarkLocation, MarkNormal, BaseYaw);
}

void USPScratchMarkComponent::SpawnScratchMark(FVector Location, FVector Normal, float BaseYaw) const
{
	if (!ScratchDecalMaterial)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector Forward = FRotator(0.f, BaseYaw, 0.f).Vector();
	const FVector Right = FVector::CrossProduct(Normal, Forward).GetSafeNormal();
	const FVector FinalLocation = Location + Right * FMath::FRandRange(-LateralJitter, LateralJitter);

	FRotator DecalRotation = FRotationMatrix::MakeFromX(-Normal).Rotator();
	DecalRotation.Roll += BaseYaw + FMath::FRandRange(-YawJitter, YawJitter);

	const float SizeScale = 1.f + FMath::FRandRange(-DecalSizeJitter, DecalSizeJitter);

	UDecalComponent* Decal = UGameplayStatics::SpawnDecalAtLocation(World, ScratchDecalMaterial, DecalSize * SizeScale, FinalLocation, DecalRotation, DecalLifetime);
	if (Decal)
	{
		Decal->SetFadeOut(FMath::Max(0.f, DecalLifetime - DecalFadeDuration), DecalFadeDuration, false);
	}
}

ASurvivorCharacter* USPScratchMarkComponent::GetSurvivor() const
{
	return Cast<ASurvivorCharacter>(GetOwner());
}

bool USPScratchMarkComponent::IsLeavingMarks() const
{
	const ASurvivorCharacter* Survivor = GetSurvivor();
	const USPMovementComponent* Movement = CachedMovement.Get();
	if (!Survivor || !Movement || !Survivor->CanMove())
	{
		return false;
	}

	if (Survivor->bIsCrouched)
	{
		return false;
	}

	const UCharacterMovementComponent* CharacterMovement = Survivor->GetCharacterMovement();
	if (!CharacterMovement || !CharacterMovement->IsMovingOnGround())
	{
		return false;
	}

	if (Survivor->GetVelocity().SizeSquared2D() < FMath::Square(MinSpeedToLeaveMarks))
	{
		return false;
	}

	return Movement->IsRunning();
}

bool USPScratchMarkComponent::ShouldLocalViewerSeeMarks() const
{
	if (!bKillerOnly)
	{
		return true;
	}

	const UWorld* World = GetWorld();
	const APlayerController* LocalController = World ? World->GetFirstPlayerController() : nullptr;
	if (!LocalController || !LocalController->IsLocalController())
	{
		return false;
	}

	const AKillerCharacter* LocalKiller = Cast<AKillerCharacter>(LocalController->GetPawn());
	return LocalKiller && LocalKiller->GetCharTags().HasTagExact(SPGameplayTags::Character::Killer);
}

float USPScratchMarkComponent::PickNextInterval() const
{
	const float Jittered = SpawnDistanceInterval + FMath::FRandRange(-SpawnDistanceJitter, SpawnDistanceJitter);
	return FMath::Max(Jittered, SpawnDistanceInterval * 0.25f);
}

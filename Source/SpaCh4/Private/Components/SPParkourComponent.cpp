#include "Components/SPParkourComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotify_SPParkourLand.h"
#include "Characters/Killer/KillerCharacter.h"
#include "Characters/Survivor/SurvivorCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/EngineTypes.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Gameplay/Parkour/SPParkourZone.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace SPParkour
{
	static constexpr TCHAR HurdleMontagePath[] =
		TEXT("/Game/Assets/Survivor_Locomotion/Miscellaneous/M_Neutral_Traversal_Hurdle_1_0_stand_F_V2_Rfoot1_Montage.M_Neutral_Traversal_Hurdle_1_0_stand_F_V2_Rfoot1_Montage");

	static constexpr TCHAR KillerParkourMontagePath[] =
		TEXT("/Game/Assets/Killer_Locomotion/Parkour/SK_Rogue_Main_Sequence_Montage.SK_Rogue_Main_Sequence_Montage");

	static FCollisionObjectQueryParams MakeObstacleObjectQuery()
	{
		FCollisionObjectQueryParams Params;
		Params.AddObjectTypesToQuery(ECC_WorldStatic);
		Params.AddObjectTypesToQuery(ECC_WorldDynamic);
		return Params;
	}

	static bool IsLandscapeLikeObstacle(const AActor* Actor)
	{
		return Actor && Actor->GetClass()->GetName().Contains(TEXT("Landscape"));
	}

	static float ComputeObstacleDepth(const AActor* ObstacleActor, const FVector& Forward)
	{
		float ObstacleDepth = 35.f;
		if (!ObstacleActor)
		{
			return ObstacleDepth;
		}

		if (IsLandscapeLikeObstacle(ObstacleActor))
		{
			return 45.f;
		}

		FVector Origin;
		FVector Extent;
		ObstacleActor->GetActorBounds(true, Origin, Extent);
		ObstacleDepth = FMath::Abs(Forward.X) * Extent.X * 2.f + FMath::Abs(Forward.Y) * Extent.Y * 2.f;
		return FMath::Clamp(ObstacleDepth, 25.f, 120.f);
	}

	static float ComputeVaultPeakCenterZ(
		const float ObstacleTopWorldZ,
		const float HalfHeight,
		const float Clearance)
	{
		// Capsule bottom sits on the measured wall top (+ optional tiny clearance).
		return ObstacleTopWorldZ + HalfHeight + Clearance;
	}
}

USPParkourComponent::USPParkourComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);

	static ConstructorHelpers::FObjectFinder<UAnimMontage> HurdleMontageFinder(SPParkour::HurdleMontagePath);
	if (HurdleMontageFinder.Succeeded())
	{
		ParkourMontage = HurdleMontageFinder.Object;
	}
}

void USPParkourComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(USPParkourComponent, bIsParkour);
}

void USPParkourComponent::BeginPlay()
{
	Super::BeginPlay();

	if (Cast<AKillerCharacter>(GetOwner()))
	{
		bExemptFromWallVaultLimit = true;
		ParkourMontage = LoadObject<UAnimMontage>(nullptr, SPParkour::KillerParkourMontagePath);

		ParkourTraceDistance = FMath::Max(ParkourTraceDistance, 240.f);
		ParkourMaxStartDistance = FMath::Max(ParkourMaxStartDistance, 190.f);
		ParkourTraceRadius = FMath::Max(ParkourTraceRadius, 55.f);
		MaxObstacleHeight = FMath::Max(MaxObstacleHeight, 400.f);
		ParkourMaxVaultTravel = FMath::Max(ParkourMaxVaultTravel, 320.f);
		ParkourLandingForwardOffset = FMath::Max(ParkourLandingForwardOffset, 65.f);

		if (const ACharacter* KillerCharacter = GetParkourCharacter())
		{
			if (const UCapsuleComponent* Capsule = KillerCharacter->GetCapsuleComponent())
			{
				const float RadiusScale = FMath::Max(1.f, Capsule->GetScaledCapsuleRadius() / 34.f);
				ParkourMaxStartDistance *= RadiusScale;
				ParkourTraceDistance *= RadiusScale;
				ParkourMaxVaultTravel *= RadiusScale;
				ParkourTraceRadius = FMath::Max(ParkourTraceRadius, Capsule->GetScaledCapsuleRadius() * 1.1f);
				ParkourLandingForwardOffset *= RadiusScale;
				ParkourMinLandingPastWallOffset *= RadiusScale;
			}
		}

		ParkourLandByMontageAlpha = FMath::Min(ParkourLandByMontageAlpha, 0.82f);
		ParkourVaultPeakMontageAlpha = FMath::Clamp(ParkourVaultPeakMontageAlpha, 0.38f, 0.48f);
		ParkourReferenceObstacleHeight = FMath::Max(ParkourReferenceObstacleHeight, 90.f);
		ParkourClearanceOverObstacle = FMath::Max(ParkourClearanceOverObstacle, 55.f);
	}
	else
	{
		MaxObstacleHeight = FMath::Min(MaxObstacleHeight, 130.f);
		ParkourReferenceObstacleHeight = FMath::Clamp(ParkourReferenceObstacleHeight, 55.f, 100.f);
		ParkourClearanceOverObstacle = FMath::Clamp(ParkourClearanceOverObstacle, 0.f, 15.f);
	}

	ParkourMinStartDistance = FMath::Min(ParkourMinStartDistance, 12.f);
	ParkourMaxStartDistance = FMath::Max(ParkourMaxStartDistance, 100.f);
	ParkourMaxVaultTravel = FMath::Max(ParkourMaxVaultTravel, 220.f);
	ParkourLandingForwardOffset = FMath::Max(ParkourLandingForwardOffset, 75.f);
	ParkourMinLandingPastWallOffset = FMath::Max(ParkourMinLandingPastWallOffset, 35.f);

	EnsureMontagesLoaded();

	if (ACharacter* Character = GetParkourCharacter())
	{
		if (USkeletalMeshComponent* MeshComp = Character->GetMesh())
		{
			if (UAnimInstance* AnimInstance = MeshComp->GetAnimInstance())
			{
				AnimInstance->SetRootMotionMode(ERootMotionMode::RootMotionFromMontagesOnly);
				CachedRootMotionMode = ERootMotionMode::RootMotionFromMontagesOnly;
			}
		}
	}
}

void USPParkourComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bIsParkour || !ActiveParkourMontage)
	{
		return;
	}

	ParkourMontageElapsed += DeltaTime;

	ACharacter* Character = GetParkourCharacter();
	const ENetRole LocalRole = Character ? Character->GetLocalRole() : ROLE_None;
	const bool bCanDriveParkourMotion = LocalRole == ROLE_Authority || LocalRole == ROLE_AutonomousProxy;
	if (bCanDriveParkourMotion)
	{
		UpdateParkourRootMotion(DeltaTime);
	}

	UAnimInstance* AnimInstance = Character && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
	if (!AnimInstance || !AnimInstance->Montage_IsPlaying(ActiveParkourMontage))
	{
		if (bCanDriveParkourMotion)
		{
			EndParkour(false);
		}
	}
}

ACharacter* USPParkourComponent::GetParkourCharacter() const
{
	return Cast<ACharacter>(GetOwner());
}

bool USPParkourComponent::CanOwnerPerformParkour() const
{
	const ACharacter* Character = GetParkourCharacter();
	if (!Character)
	{
		return false;
	}

	if (const ASurvivorCharacter* Survivor = Cast<ASurvivorCharacter>(Character))
	{
		const ESurvivorState State = Survivor->GetSurvivorState();
		return State == ESurvivorState::Healthy || State == ESurvivorState::Injured;
	}

	if (const AKillerCharacter* Killer = Cast<AKillerCharacter>(Character))
	{
		return Killer->GetKillerState() == EKillerState::Idle;
	}

	return false;
}

float USPParkourComponent::GetParkourFeetZ(const ACharacter* Character, const UCapsuleComponent* Capsule) const
{
	if (!Character || !Capsule)
	{
		return 0.f;
	}

	const float CapsuleFeetZ = Character->GetActorLocation().Z - Capsule->GetScaledCapsuleHalfHeight();

	if (const UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
	{
		if (MoveComp->CurrentFloor.IsWalkableFloor())
		{
			return MoveComp->CurrentFloor.HitResult.ImpactPoint.Z;
		}
	}

	if (!GetWorld())
	{
		return CapsuleFeetZ;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ParkourFeet), false, Character);
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const FVector CapsuleBottom = Character->GetActorLocation() - FVector(0.f, 0.f, HalfHeight);
	const FVector TraceStart = CapsuleBottom + FVector(0.f, 0.f, 2.f);
	const FVector TraceEnd = CapsuleBottom - FVector(0.f, 0.f, 60.f);

	FHitResult GroundHit;
	const FCollisionObjectQueryParams ObjQuery = SPParkour::MakeObstacleObjectQuery();
	if (GetWorld()->LineTraceSingleByObjectType(GroundHit, TraceStart, TraceEnd, ObjQuery, Params)
		&& GroundHit.bBlockingHit)
	{
		return GroundHit.ImpactPoint.Z;
	}

	return CapsuleFeetZ;
}

bool USPParkourComponent::CanJumpOver() const
{
	if (bIsParkour)
	{
		return false;
	}

	const ACharacter* Character = GetParkourCharacter();
	if (!Character)
	{
		return false;
	}

	if (const UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
	{
		if (MoveComp->IsFalling())
		{
			return false;
		}
	}

	if (!CanOwnerPerformParkour())
	{
		return false;
	}

	if (bRequireParkourZone && !IsInsideParkourZone())
	{
		return false;
	}

	return true;
}

void USPParkourComponent::RequestJumpOver()
{
	LogParkourDebug(TEXT("JumpOver input"), FColor::Cyan);

	if (!CanJumpOver())
	{
		LogParkourDebug(TEXT("CanJumpOver = false"), FColor::Red);
		return;
	}

	if (GetOwner()->HasAuthority())
	{
		Server_JumpOver_Implementation();
	}
	else
	{
		Server_JumpOver();
	}
}

void USPParkourComponent::EnsureMontagesLoaded()
{
	if (ParkourMontage)
	{
		return;
	}

	const TCHAR* MontagePath = bExemptFromWallVaultLimit
		? SPParkour::KillerParkourMontagePath
		: SPParkour::HurdleMontagePath;
	ParkourMontage = LoadObject<UAnimMontage>(nullptr, MontagePath);
}

bool USPParkourComponent::Server_JumpOver_Validate()
{
	return true;
}

void USPParkourComponent::Server_JumpOver_Implementation()
{
	EnsureMontagesLoaded();

	if (!CanJumpOver() || bIsParkour)
	{
		LogParkourDebug(TEXT("Server: blocked (state/parkour)"), FColor::Red);
		return;
	}

	FHitResult ObstacleHit;
	float ObstacleHeight = 0.f;
	FString FailReason;
	if (!TraceParkourObstacle(ObstacleHit, ObstacleHeight, &FailReason))
	{
		LogParkourDebug(FailReason.IsEmpty() ? TEXT("Trace failed") : FailReason, FColor::Orange);
		return;
	}

	UAnimMontage* Montage = ParkourMontage;
	if (!Montage)
	{
		LogParkourDebug(TEXT("ParkourMontage null - montage path check"), FColor::Red);
		return;
	}

	ACharacter* Character = GetParkourCharacter();
	if (!Character)
	{
		return;
	}

	const FRotator FacingRotation = ResolveParkourFacingRotation(ObstacleHit);

	FString VaultFailReason;
	if (!PreviewParkourVault(FacingRotation, ObstacleHeight, ObstacleHit.GetActor(), ObstacleHit.ImpactPoint, &VaultFailReason))
	{
		LogParkourDebug(
			VaultFailReason.IsEmpty() ? TEXT("Vault preview skipped (lenient)") : FString::Printf(TEXT("Vault preview warn: %s"), *VaultFailReason),
			FColor::Yellow);
	}

	FString WallLimitReason;
	if (!bExemptFromWallVaultLimit)
	{
		if (IsObstacleVaultBlocked(ObstacleHit.GetActor(), &WallLimitReason))
		{
			LogParkourDebug(
				WallLimitReason.IsEmpty() ? TEXT("Wall vault on cooldown") : WallLimitReason,
				FColor::Orange);
			return;
		}

		if (!TryRegisterWallVault(ObstacleHit.GetActor(), &WallLimitReason))
		{
			LogParkourDebug(
				WallLimitReason.IsEmpty() ? TEXT("Wall vault registration failed") : WallLimitReason,
				FColor::Orange);
			return;
		}
	}

	LogParkourDebug(
		FString::Printf(TEXT("Start H:%.0f D:%.0f -> %s"), ObstacleHeight, FVector::Dist2D(Character->GetActorLocation(), ObstacleHit.ImpactPoint), *Montage->GetName()),
		FColor::Green);

	Multicast_PlayParkourMontage(Montage, ObstacleHit.GetActor(), FacingRotation, ObstacleHeight, ObstacleHit.ImpactPoint);
}

void USPParkourComponent::Multicast_PlayParkourMontage_Implementation(
	UAnimMontage* Montage,
	AActor* ObstacleActor,
	const FRotator& FacingRotation,
	float ObstacleHeight,
	FVector_NetQuantize ObstacleImpactPoint)
{
	if (!Montage || bIsParkour)
	{
		return;
	}

	PlayParkourMontage(Montage, ObstacleActor, FacingRotation, ObstacleHeight, ObstacleImpactPoint);
}

void USPParkourComponent::GetParkourFacing(FVector& OutForward, FVector& OutRight) const
{
	if (bIsParkour)
	{
		OutForward = ParkourStartTransform.GetRotation().GetForwardVector().GetSafeNormal2D();
		OutRight = ParkourStartTransform.GetRotation().GetRightVector().GetSafeNormal2D();
		return;
	}

	const ACharacter* Character = GetParkourCharacter();
	if (!Character)
	{
		OutForward = FVector::ForwardVector;
		OutRight = FVector::RightVector;
		return;
	}

	OutForward = Character->GetActorForwardVector().GetSafeNormal2D();
	OutRight = Character->GetActorRightVector().GetSafeNormal2D();
}

FRotator USPParkourComponent::ResolveParkourFacingRotation(const FHitResult& ObstacleHit) const
{
	const ACharacter* Character = GetParkourCharacter();
	if (!Character)
	{
		return FRotator::ZeroRotator;
	}

	FVector SurfaceNormal2D(ObstacleHit.ImpactNormal.X, ObstacleHit.ImpactNormal.Y, 0.f);
	if (SurfaceNormal2D.SizeSquared() > FMath::Square(0.01f))
	{
		// Vault over the wall: travel opposite the wall normal, not toward the impact XY offset.
		const FVector VaultForward = -SurfaceNormal2D.GetSafeNormal();
		return FRotator(0.f, FRotationMatrix::MakeFromX(VaultForward).Rotator().Yaw, 0.f);
	}

	return FRotator(0.f, Character->GetActorRotation().Yaw, 0.f);
}

bool USPParkourComponent::TraceParkourObstacle(FHitResult& OutObstacleHit, float& OutObstacleHeight, FString* OutFailReason) const
{
	auto SetFail = [OutFailReason](const FString& Reason)
	{
		if (OutFailReason)
		{
			*OutFailReason = Reason;
		}
	};

	OutObstacleHeight = 0.f;

	const ACharacter* Character = GetParkourCharacter();
	const UCapsuleComponent* Capsule = Character ? Character->GetCapsuleComponent() : nullptr;
	if (!Capsule || !GetWorld())
	{
		SetFail(TEXT("Capsule/World missing"));
		return false;
	}

	if (bRequireParkourZone && !IsInsideParkourZone())
	{
		SetFail(TEXT("Outside parkour zone"));
		return false;
	}

	FVector Forward;
	FVector Right;
	GetParkourFacing(Forward, Right);
	(void)Right;

	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const float CapsuleRadius = Capsule->GetScaledCapsuleRadius();
	const float FeetZ = GetParkourFeetZ(Character, Capsule);
	const FVector FeetLocation(Character->GetActorLocation().X, Character->GetActorLocation().Y, FeetZ);
	const FVector TraceStart = FeetLocation + FVector(0.f, 0.f, HalfHeight * 0.6f);
	const FVector TraceEnd = TraceStart + Forward * ParkourTraceDistance;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ParkourTrace), false, Character);

	const FCollisionObjectQueryParams ObjQuery = SPParkour::MakeObstacleObjectQuery();
	const bool bForwardHit = GetWorld()->SweepSingleByObjectType(
		OutObstacleHit,
		TraceStart,
		TraceEnd,
		FQuat::Identity,
		ObjQuery,
		FCollisionShape::MakeSphere(ParkourTraceRadius),
		Params);

	if (bDrawDebug)
	{
		DrawDebugLine(GetWorld(), TraceStart, TraceEnd, bForwardHit ? FColor::Green : FColor::Red, false, 1.5f, 0, 2.f);
		if (bForwardHit)
		{
			DrawDebugSphere(GetWorld(), OutObstacleHit.ImpactPoint, 12.f, 8, FColor::Yellow, false, 1.5f);
		}
	}

	if (!bForwardHit || !OutObstacleHit.bBlockingHit)
	{
		SetFail(TEXT("No forward obstacle"));
		return false;
	}

	const FVector ActorCenter = Character->GetActorLocation();
	const FVector ActorForward = Character->GetActorForwardVector().GetSafeNormal2D();
	const FVector ToObstacle2D(
		OutObstacleHit.ImpactPoint.X - ActorCenter.X,
		OutObstacleHit.ImpactPoint.Y - ActorCenter.Y,
		0.f);
	const float ToObstacleDistance2D = ToObstacle2D.Size();

	FVector SurfaceNormal2D = FVector(OutObstacleHit.ImpactNormal.X, OutObstacleHit.ImpactNormal.Y, 0.f);
	if (SurfaceNormal2D.SizeSquared() < FMath::Square(0.01f))
	{
		// Flush/penetrating hits often lack a usable XY normal; assume wall faces the actor.
		SurfaceNormal2D = -ActorForward;
	}
	else
	{
		SurfaceNormal2D.Normalize();
	}

	float ObstacleAheadDot = 1.f;
	float LateralOffset = 0.f;
	if (ToObstacleDistance2D > 1.f)
	{
		const FVector ToObstacleDir = ToObstacle2D / ToObstacleDistance2D;
		ObstacleAheadDot = FVector::DotProduct(ToObstacleDir, ActorForward);
		LateralOffset = FMath::Abs(FVector::DotProduct(ToObstacle2D, Right));
	}
	else
	{
		// Impact shares XY with actor center when pressed against the wall.
		ObstacleAheadDot = FVector::DotProduct(ActorForward, -SurfaceNormal2D);
	}

	const float MaxLateralOffset = CapsuleRadius + ParkourTraceRadius * 0.35f;

	if (ObstacleAheadDot < ParkourMinFacingDot)
	{
		SetFail(FString::Printf(TEXT("Obstacle not in front (%.2f < %.2f)"), ObstacleAheadDot, ParkourMinFacingDot));
		return false;
	}

	if (ToObstacleDistance2D > 1.f && LateralOffset > MaxLateralOffset)
	{
		SetFail(TEXT("Obstacle too far sideways"));
		return false;
	}

	const float ForwardDist = FVector::DotProduct(ToObstacle2D, Forward);
	const float HitDistance = ToObstacleDistance2D > 1.f
		? FMath::Max(0.f, ForwardDist - CapsuleRadius)
		: 0.f;

	if (OutObstacleHit.bStartPenetrating && OutObstacleHit.PenetrationDepth > CapsuleRadius * 0.92f)
	{
		SetFail(TEXT("Inside obstacle"));
		return false;
	}

	if (HitDistance > ParkourMaxStartDistance + 15.f)
	{
		SetFail(FString::Printf(TEXT("Too far (%.0f > %.0f)"), HitDistance, ParkourMaxStartDistance));
		return false;
	}

	if (!SurfaceNormal2D.IsNearlyZero())
	{
		const float FaceDot = FVector::DotProduct(ActorForward, -SurfaceNormal2D);
		if (FaceDot < ParkourMinFacingDot)
		{
			SetFail(FString::Printf(TEXT("Must face obstacle (%.2f < %.2f)"), FaceDot, ParkourMinFacingDot));
			return false;
		}
	}

	const FVector ImpactNormal = OutObstacleHit.ImpactNormal.GetSafeNormal();
	if (ImpactNormal.IsNearlyZero())
	{
		SetFail(TEXT("Invalid obstacle hit"));
		return false;
	}
	const FVector WallSamplePoint = OutObstacleHit.ImpactPoint - ImpactNormal * 10.f;

	const float TopTraceStartZ = FMath::Max(OutObstacleHit.ImpactPoint.Z, FeetLocation.Z) + MaxObstacleHeight + 15.f;
	const FVector TopTraceStart(
		WallSamplePoint.X,
		WallSamplePoint.Y,
		TopTraceStartZ);
	const FVector TopTraceEnd(
		WallSamplePoint.X,
		WallSamplePoint.Y,
		FeetLocation.Z - 5.f);

	AActor* ForwardHitActor = OutObstacleHit.GetActor();

	FHitResult TopHit;
	const bool bTopHit = GetWorld()->LineTraceSingleByObjectType(
		TopHit,
		TopTraceStart,
		TopTraceEnd,
		ObjQuery,
		Params);

	float ObstacleTopZ = -MAX_FLT;
	if (bTopHit && TopHit.bBlockingHit)
	{
		const bool bSameObstacle = !ForwardHitActor || TopHit.GetActor() == ForwardHitActor;
		if (bSameObstacle)
		{
			ObstacleTopZ = TopHit.ImpactPoint.Z;
		}
	}

	if (ObstacleTopZ <= -MAX_FLT + 1.f)
	{
		OutObstacleHeight = ParkourReferenceObstacleHeight;
	}
	else
	{
		OutObstacleHeight = ObstacleTopZ - FeetLocation.Z;
	}

	OutObstacleHeight = FMath::Clamp(OutObstacleHeight, MinObstacleHeight, MaxObstacleHeight);

	if (bDrawDebug)
	{
		DrawDebugLine(GetWorld(), TopTraceStart, TopTraceEnd, FColor::Cyan, false, 1.5f, 0, 1.5f);
		if (ObstacleTopZ > -MAX_FLT + 1.f)
		{
			const FVector WallTopPoint(OutObstacleHit.ImpactPoint.X, OutObstacleHit.ImpactPoint.Y, ObstacleTopZ);
			DrawDebugLine(GetWorld(), WallTopPoint - FVector(30.f, 0.f, 0.f), WallTopPoint + FVector(30.f, 0.f, 0.f), FColor::Yellow, false, 1.5f, 0, 3.f);
			DrawDebugCapsule(
				GetWorld(),
				FVector(WallTopPoint.X, WallTopPoint.Y, ObstacleTopZ + HalfHeight + ParkourClearanceOverObstacle),
				HalfHeight,
				CapsuleRadius,
				FQuat::Identity,
				FColor::Cyan,
				false,
				1.5f,
				0,
				1.5f);
		}

		DrawDebugString(
			GetWorld(),
			FVector(OutObstacleHit.ImpactPoint.X, OutObstacleHit.ImpactPoint.Y, ObstacleTopZ),
			FString::Printf(TEXT("H:%.0f D:%.0f Top:%.0f"), OutObstacleHeight, HitDistance, ObstacleTopZ),
			nullptr,
			FColor::White,
			1.5f);
	}

	return true;
}

void USPParkourComponent::PlayParkourMontage(
	UAnimMontage* Montage,
	AActor* ObstacleActor,
	const FRotator& FacingRotation,
	float ObstacleHeight,
	const FVector& ObstacleImpactPoint)
{
	ACharacter* Character = GetParkourCharacter();
	if (!Montage || !Character)
	{
		return;
	}

	USkeletalMeshComponent* MeshComp = Character->GetMesh();
	UAnimInstance* AnimInstance = MeshComp ? MeshComp->GetAnimInstance() : nullptr;
	if (!AnimInstance)
	{
		return;
	}

	UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement();
	if (MoveComp)
	{
		CachedMovementMode = MoveComp->MovementMode;
		CachedGravityScale = MoveComp->GravityScale;
		CachedNetworkSmoothingMode = MoveComp->NetworkSmoothingMode;
		MoveComp->NetworkSmoothingMode = ENetworkSmoothingMode::Disabled;
		MoveComp->StopMovementImmediately();
		MoveComp->Velocity = FVector::ZeroVector;
		MoveComp->GravityScale = 0.f;
		MoveComp->SetMovementMode(MOVE_None);
		MoveComp->bAllowPhysicsRotationDuringAnimRootMotion = true;
	}

	SetParkourCollisionSuppressed(true);

	CachedRootMotionMode = AnimInstance->RootMotionMode;
	AnimInstance->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);

	ParkourStartTransform = Character->GetActorTransform();
	Character->SetActorRotation(FRotator(0.f, FacingRotation.Yaw, 0.f));
	ParkourStartTransform.SetRotation(Character->GetActorRotation().Quaternion());

	ParkourObstacleHeight = FMath::Clamp(ObstacleHeight, MinObstacleHeight, MaxObstacleHeight);
	if (const UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
	{
		ParkourStartFeetZ = GetParkourFeetZ(Character, Capsule);
	}
	ParkourObstacleTopZ = ParkourStartFeetZ + ParkourObstacleHeight;
	ParkourObstacleImpactPoint = ObstacleImpactPoint;

	LogParkourDebug(
		FString::Printf(
			TEXT("Vault H:%.0f topZ:%.0f peakZ:%.0f"),
			ParkourObstacleHeight,
			ParkourObstacleTopZ,
			ParkourObstacleTopZ + (Character->GetCapsuleComponent() ? Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0.f) + ParkourClearanceOverObstacle),
		FColor::Silver);

	InitParkourVaultTargets(ObstacleActor, ObstacleImpactPoint);

	if (ObstacleActor)
	{
		CurrentParkourObstacle = ObstacleActor;
		SetParkourWallCollisionDisabled(true, ObstacleActor);
	}

	ActiveParkourMontage = Montage;
	bIsParkour = true;
	bParkourFeetOnGround = false;
	ParkourMontageElapsed = 0.f;

	if (MoveComp)
	{
		Character->PrimaryActorTick.AddPrerequisite(MoveComp, MoveComp->PrimaryComponentTick);
	}

	if (AController* Ctrl = Character->GetController())
	{
		Ctrl->SetIgnoreMoveInput(true);
	}

	ParkourMontageEndedDelegate.BindUObject(this, &USPParkourComponent::OnParkourMontageEnded);
	AnimInstance->Montage_SetEndDelegate(ParkourMontageEndedDelegate, Montage);

	const float Duration = AnimInstance->Montage_Play(Montage, 1.f);
	if (Duration <= 0.f)
	{
		LogParkourDebug(
			TEXT("Montage_Play failed - check ABP slot and AnimClass"),
			FColor::Red);
		EndParkour(true);
		return;
	}

	ParkourMontageDuration = Duration;
	CacheParkourLandNotifyTiming(Montage);

	if (bExemptFromWallVaultLimit)
	{
		const float TouchdownAlpha = ParkourLandNotifyMontageAlpha > 0.f
			? ParkourLandNotifyMontageAlpha
			: ParkourLandByMontageAlpha;
		const float ClearAlpha = FMath::Clamp(ParkourWallClearMontageAlpha, 0.30f, TouchdownAlpha - 0.05f);
		ParkourKillerDescentStartAlpha = FMath::Clamp(ClearAlpha + 0.10f, 0.45f, TouchdownAlpha - 0.04f);
		if (ParkourLandNotifyMontageAlpha <= 0.f)
		{
			ParkourLandNotifyMontageAlpha = TouchdownAlpha;
		}

		LogParkourDebug(
			FString::Printf(
				TEXT("Killer land notify @%.2f, descent %.2f -> %.2f"),
				ParkourLandNotifyMontageAlpha,
				ParkourKillerDescentStartAlpha,
				ParkourLandNotifyMontageAlpha),
			FColor::Silver);
	}
	else if (ParkourLandNotifyMontageAlpha > 0.f)
	{
		const float MinLandAlpha = FMath::Clamp(FMath::Max(ParkourWallClearMontageAlpha, 0.78f), 0.55f, 0.92f);
		if (ParkourLandNotifyMontageAlpha < MinLandAlpha)
		{
			LogParkourDebug(
				FString::Printf(
					TEXT("Land notify clamped %.2f -> %.2f"),
					ParkourLandNotifyMontageAlpha,
					MinLandAlpha),
				FColor::Yellow);
			ParkourLandNotifyMontageAlpha = MinLandAlpha;
		}
	}

	GetWorld()->GetTimerManager().ClearTimer(ParkourEndTimerHandle);
	GetWorld()->GetTimerManager().SetTimer(
		ParkourEndTimerHandle,
		this,
		&USPParkourComponent::OnParkourEndTimer,
		Duration + 0.1f,
		false);

	FVector PeakTranslation = FVector::ZeroVector;
	FQuat PeakRotation = FQuat::Identity;
	if (ExtractMontageRootMotionAtTime(Montage, Duration, PeakTranslation, PeakRotation))
	{
		LogParkourDebug(
			FString::Printf(TEXT("Montage RM peak %s (Z=%.1f)"), *PeakTranslation.ToCompactString(), PeakTranslation.Z),
			FColor::Silver);
	}

	LogParkourDebug(FString::Printf(TEXT("Playing %s (%.2fs)"), *Montage->GetName(), Duration), FColor::Green);
}

void USPParkourComponent::InitParkourVaultTargets(AActor* ObstacleActor, const FVector& ObstacleImpactPoint)
{
	bParkourVaultEndValid = ResolveParkourVaultTargets(
		ParkourStartTransform,
		ParkourStartFeetZ,
		ParkourObstacleHeight,
		ObstacleActor,
		ObstacleImpactPoint,
		ParkourVaultEndLocation,
		ParkourVaultPeakCenterZ,
		ParkourWallClearForward,
		ParkourWallClearMontageAlpha,
		nullptr);

	if (!bParkourVaultEndValid)
	{
		const ACharacter* Character = GetParkourCharacter();
		const UCapsuleComponent* Capsule = Character ? Character->GetCapsuleComponent() : nullptr;
		if (Capsule)
		{
			const FVector Forward = ParkourStartTransform.GetRotation().GetForwardVector().GetSafeNormal2D();
			const FVector StartCenter = ParkourStartTransform.GetLocation();
			const float Radius = Capsule->GetScaledCapsuleRadius();
			const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();

			const float ObstacleDepth = SPParkour::ComputeObstacleDepth(ObstacleActor, Forward);

			const float DistToWallFace = FMath::Max(
				FVector::DotProduct(ObstacleImpactPoint - StartCenter, Forward),
				0.f);
			const float FallbackTravel = FMath::Min(
				DistToWallFace + ObstacleDepth + Radius + ParkourMinLandingPastWallOffset,
				ParkourMaxVaultTravel);

			ParkourVaultEndLocation = StartCenter + Forward * FallbackTravel;
			ParkourVaultPeakCenterZ = SPParkour::ComputeVaultPeakCenterZ(
				ParkourObstacleTopZ,
				HalfHeight,
				ParkourClearanceOverObstacle);
			ParkourWallClearForward = DistToWallFace + ObstacleDepth + Radius;

			if (SampleParkourGroundCenterZ(ParkourVaultEndLocation, true, ObstacleActor, ParkourVaultPeakCenterZ))
			{
				bParkourVaultEndValid = true;
			}

			const float TotalForwardDist = FVector::DotProduct(ParkourVaultEndLocation - StartCenter, Forward);
			if (TotalForwardDist > KINDA_SMALL_NUMBER)
			{
				const float ClearForwardAlpha = FMath::Clamp(ParkourWallClearForward / TotalForwardDist, 0.f, 1.f);
				ParkourWallClearMontageAlpha = 1.f - FMath::Pow(1.f - ClearForwardAlpha, 1.f / 1.85f);
				ParkourWallClearMontageAlpha = FMath::Min(ParkourWallClearMontageAlpha, 0.85f);
			}
			else
			{
				ParkourWallClearMontageAlpha = 0.45f;
			}
		}

		LogParkourDebug(
			bParkourVaultEndValid
				? TEXT("Vault targets recovered with fallback landing")
				: TEXT("Vault targets invalid - using start as landing"),
			FColor::Orange);

		if (!bParkourVaultEndValid)
		{
			ParkourVaultEndLocation = ParkourStartTransform.GetLocation();
			if (Capsule)
			{
				ParkourVaultPeakCenterZ = SPParkour::ComputeVaultPeakCenterZ(
					ParkourObstacleTopZ,
					Capsule->GetScaledCapsuleHalfHeight(),
					ParkourClearanceOverObstacle);
			}
			else
			{
				ParkourVaultPeakCenterZ = ParkourObstacleTopZ;
			}
			ParkourWallClearForward = 0.f;
			ParkourWallClearMontageAlpha = 0.45f;
			bParkourVaultEndValid = true;
		}
	}

	LogParkourDebug(
		FString::Printf(
			TEXT("Vault travel %.0fcm wallClear %.0fcm end %s peakZ %.0f"),
			FVector::DotProduct(
				ParkourVaultEndLocation - ParkourStartTransform.GetLocation(),
				ParkourStartTransform.GetRotation().GetForwardVector().GetSafeNormal2D()),
			ParkourWallClearForward,
			*ParkourVaultEndLocation.ToCompactString(),
			ParkourVaultPeakCenterZ),
		FColor::Silver);

	ParkourPlannedForwardTravel = FVector::DotProduct(
		ParkourVaultEndLocation - ParkourStartTransform.GetLocation(),
		ParkourStartTransform.GetRotation().GetForwardVector().GetSafeNormal2D());
	ParkourVaultEndGroundZ = ParkourVaultEndLocation.Z;
}

bool USPParkourComponent::ResolveParkourVaultTargets(
	const FTransform& StartTransform,
	const float StartFeetZ,
	const float ObstacleHeight,
	AActor* ObstacleActor,
	const FVector& ObstacleImpactPoint,
	FVector& OutEndLocation,
	float& OutPeakCenterZ,
	float& OutWallClearForward,
	float& OutWallClearMontageAlpha,
	FString* OutFailReason) const
{
	auto SetFail = [OutFailReason](const FString& Reason)
	{
		if (OutFailReason)
		{
			*OutFailReason = Reason;
		}
	};

	const ACharacter* Character = GetParkourCharacter();
	const UCapsuleComponent* Capsule = Character ? Character->GetCapsuleComponent() : nullptr;
	if (!Capsule || !GetWorld())
	{
		SetFail(TEXT("Capsule/World missing"));
		return false;
	}

	const FVector Forward = StartTransform.GetRotation().GetForwardVector().GetSafeNormal2D();
	const FVector StartCenter = StartTransform.GetLocation();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const float Radius = Capsule->GetScaledCapsuleRadius();

	const float ObstacleDepth = SPParkour::ComputeObstacleDepth(ObstacleActor, Forward);

	const float ImpactAlongForward = FVector::DotProduct(ObstacleImpactPoint - StartCenter, Forward);
	const float DistToWallFace = FMath::Max(ImpactAlongForward, 0.f);
	const float MinTravelPastWall = DistToWallFace + ObstacleDepth + Radius + ParkourMinLandingPastWallOffset;
	float TravelPastWall = DistToWallFace + ObstacleDepth + Radius + ParkourLandingForwardOffset;
	TravelPastWall = FMath::Max(TravelPastWall, MinTravelPastWall);
	TravelPastWall = FMath::Max(TravelPastWall, ImpactAlongForward + Radius + 95.f);
	if (bExemptFromWallVaultLimit)
	{
		TravelPastWall = FMath::Max(TravelPastWall, ImpactAlongForward + Radius + 120.f);
	}
	TravelPastWall = FMath::Clamp(TravelPastWall, MinTravelPastWall, ParkourMaxVaultTravel);

	if (MinTravelPastWall > ParkourMaxVaultTravel + 1.f)
	{
		TravelPastWall = ParkourMaxVaultTravel;
	}
	else
	{
		TravelPastWall = FMath::Clamp(TravelPastWall, MinTravelPastWall, ParkourMaxVaultTravel);
	}

	OutPeakCenterZ = SPParkour::ComputeVaultPeakCenterZ(
		StartFeetZ + ObstacleHeight,
		HalfHeight,
		ParkourClearanceOverObstacle);

	OutEndLocation = StartCenter + Forward * TravelPastWall;
	OutEndLocation.Z = StartCenter.Z;

	FHitResult PathHit;
	if (SweepParkourCapsulePath(StartCenter, OutEndLocation, Forward, PathHit, ObstacleActor, OutPeakCenterZ))
	{
		const FVector HitNormal2D = FVector(PathHit.ImpactNormal.X, PathHit.ImpactNormal.Y, 0.f).GetSafeNormal();
		const bool bWallLikeBlock = !HitNormal2D.IsNearlyZero()
			&& FVector::DotProduct(Forward, -HitNormal2D) > 0.35f;
		const bool bLandscapeBlock = PathHit.GetActor() && SPParkour::IsLandscapeLikeObstacle(PathHit.GetActor());

		if (bWallLikeBlock && !bLandscapeBlock && !bExemptFromWallVaultLimit)
		{
			const float BlockedTravel = FVector::DotProduct(PathHit.Location - StartCenter, Forward) - Radius - 8.f;
			if (BlockedTravel >= MinTravelPastWall)
			{
				TravelPastWall = FMath::Clamp(BlockedTravel, MinTravelPastWall, ParkourMaxVaultTravel);
				OutEndLocation = StartCenter + Forward * TravelPastWall;
				OutEndLocation.Z = StartCenter.Z;
			}
		}
	}

	FVector GroundSample = OutEndLocation;
	if (!SampleParkourGroundCenterZ(GroundSample, true, ObstacleActor, OutPeakCenterZ))
	{
		GroundSample.Z = StartCenter.Z;
	}
	OutEndLocation = GroundSample;

	if (!ValidateParkourLandingLocation(StartTransform, StartFeetZ, OutEndLocation, nullptr))
	{
		// Lenient: keep computed landing even if validation is imperfect.
	}

	const float TotalForwardDist = FVector::DotProduct(OutEndLocation - StartCenter, Forward);
	OutWallClearForward = DistToWallFace + ObstacleDepth + Radius;
	if (TotalForwardDist > KINDA_SMALL_NUMBER)
	{
		OutWallClearForward = FMath::Min(OutWallClearForward, TotalForwardDist * 0.82f);
		const float ClearForwardAlpha = FMath::Clamp(OutWallClearForward / TotalForwardDist, 0.f, 1.f);
		OutWallClearMontageAlpha = 1.f - FMath::Pow(1.f - ClearForwardAlpha, 1.f / 1.85f);
		OutWallClearMontageAlpha = FMath::Min(OutWallClearMontageAlpha, 0.85f);
	}
	else
	{
		OutWallClearMontageAlpha = 0.45f;
	}

	return true;
}

bool USPParkourComponent::SweepParkourCapsulePath(
	const FVector& Start,
	const FVector& End,
	const FVector& Forward,
	FHitResult& OutHit,
	AActor* IgnoredObstacle,
	float SweepCenterZOverride) const
{
	const ACharacter* Character = GetParkourCharacter();
	const UCapsuleComponent* Capsule = Character ? Character->GetCapsuleComponent() : nullptr;
	if (!Capsule || !GetWorld())
	{
		return false;
	}

	FVector SweepStart = Start;
	FVector SweepEnd = End;
	if (SweepCenterZOverride >= 0.f)
	{
		SweepStart.Z = SweepCenterZOverride;
		SweepEnd.Z = SweepCenterZOverride;
	}

	const FVector Delta = SweepEnd - SweepStart;
	if (Delta.SizeSquared2D() < FMath::Square(5.f))
	{
		return false;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ParkourVaultSweep), false, Character);
	if (IgnoredObstacle)
	{
		Params.AddIgnoredActor(IgnoredObstacle);
	}
	else if (AActor* ObstacleActor = CurrentParkourObstacle.Get())
	{
		Params.AddIgnoredActor(ObstacleActor);
	}

	const FCollisionShape SweepShape = FCollisionShape::MakeCapsule(
		Capsule->GetScaledCapsuleRadius(),
		Capsule->GetScaledCapsuleHalfHeight());

	const FCollisionObjectQueryParams ObjQuery = SPParkour::MakeObstacleObjectQuery();
	const bool bHit = GetWorld()->SweepSingleByObjectType(
		OutHit,
		SweepStart,
		SweepEnd,
		FQuat::Identity,
		ObjQuery,
		SweepShape,
		Params);

	(void)Forward;

	if (bDrawDebug)
	{
		DrawDebugCapsule(
			GetWorld(),
			SweepStart,
			Capsule->GetScaledCapsuleHalfHeight(),
			Capsule->GetScaledCapsuleRadius(),
			FQuat::Identity,
			FColor::Cyan,
			false,
			1.5f,
			0,
			1.f);
		DrawDebugLine(GetWorld(), SweepStart, SweepEnd, bHit ? FColor::Orange : FColor::Green, false, 1.5f, 0, 2.f);
		if (bHit)
		{
			DrawDebugSphere(GetWorld(), OutHit.Location, 10.f, 8, FColor::Orange, false, 1.5f);
		}
	}

	return bHit;
}

bool USPParkourComponent::ValidateParkourLandingLocation(
	const FTransform& StartTransform,
	const float StartFeetZ,
	const FVector& ProposedLocation,
	FString* OutFailReason) const
{
	auto SetFail = [OutFailReason](const FString& Reason)
	{
		if (OutFailReason)
		{
			*OutFailReason = Reason;
		}
	};

	const ACharacter* Character = GetParkourCharacter();
	const UCapsuleComponent* Capsule = Character ? Character->GetCapsuleComponent() : nullptr;
	if (!Capsule)
	{
		SetFail(TEXT("Capsule missing"));
		return false;
	}

	const FVector StartCenter = StartTransform.GetLocation();
	const FVector Forward = StartTransform.GetRotation().GetForwardVector().GetSafeNormal2D();
	const FVector ToLanding = ProposedLocation - StartCenter;
	const float ForwardTravel = FVector::DotProduct(ToLanding, Forward);
	if (ForwardTravel < 0.f)
	{
		SetFail(TEXT("Landing behind start"));
		return false;
	}
	if (ForwardTravel > ParkourMaxVaultTravel + 5.f)
	{
		SetFail(FString::Printf(TEXT("Landing too far (%.0f > %.0f)"), ForwardTravel, ParkourMaxVaultTravel));
		return false;
	}

	const FVector Lateral = ToLanding - Forward * ForwardTravel;
	if (Lateral.SizeSquared2D() > FMath::Square(45.f))
	{
		SetFail(TEXT("Landing too far sideways"));
		return false;
	}

	const float EffectiveStartFeetZ = StartFeetZ > 0.f
		? StartFeetZ
		: StartCenter.Z - Capsule->GetScaledCapsuleHalfHeight();
	const float LandingFeetZ = ProposedLocation.Z - Capsule->GetScaledCapsuleHalfHeight();
	const float Drop = EffectiveStartFeetZ - LandingFeetZ;
	if (Drop > ParkourMaxLandingDrop)
	{
		SetFail(FString::Printf(TEXT("Landing drop too large (%.0f)"), Drop));
		return false;
	}

	return true;
}

bool USPParkourComponent::PreviewParkourVault(
	const FRotator& FacingRotation,
	const float ObstacleHeight,
	AActor* ObstacleActor,
	const FVector& ObstacleImpactPoint,
	FString* OutFailReason) const
{
	const ACharacter* Character = GetParkourCharacter();
	const UCapsuleComponent* Capsule = Character ? Character->GetCapsuleComponent() : nullptr;
	if (!Character || !Capsule)
	{
		if (OutFailReason)
		{
			*OutFailReason = TEXT("Character/Capsule missing");
		}
		return false;
	}

	FTransform PreviewStart = Character->GetActorTransform();
	PreviewStart.SetRotation(FRotator(0.f, FacingRotation.Yaw, 0.f).Quaternion());

	const float PreviewFeetZ = GetParkourFeetZ(Character, Capsule);
	const float ClampedHeight = FMath::Clamp(ObstacleHeight, MinObstacleHeight, MaxObstacleHeight);

	const float PreviewPeakZ = PreviewFeetZ + ClampedHeight + Capsule->GetScaledCapsuleHalfHeight() + ParkourClearanceOverObstacle;
	(void)PreviewPeakZ;

	FVector PreviewEnd = FVector::ZeroVector;
	float PreviewPeakZOut = 0.f;
	float PreviewClearForward = 0.f;
	float PreviewClearAlpha = 0.f;
	if (ResolveParkourVaultTargets(
		PreviewStart,
		PreviewFeetZ,
		ClampedHeight,
		ObstacleActor,
		ObstacleImpactPoint,
		PreviewEnd,
		PreviewPeakZOut,
		PreviewClearForward,
		PreviewClearAlpha,
		nullptr))
	{
		return true;
	}

	const FVector Forward = PreviewStart.GetRotation().GetForwardVector().GetSafeNormal2D();
	const FVector StartCenter = PreviewStart.GetLocation();
	const float Radius = Capsule->GetScaledCapsuleRadius();

	const float ObstacleDepth = SPParkour::ComputeObstacleDepth(ObstacleActor, Forward);

	const float DistToWallFace = FMath::Max(
		FVector::DotProduct(ObstacleImpactPoint - StartCenter, Forward),
		0.f);
	const float FallbackTravel = FMath::Min(
		DistToWallFace + ObstacleDepth + Radius + ParkourMinLandingPastWallOffset,
		ParkourMaxVaultTravel);

	FVector FallbackEnd = StartCenter + Forward * FallbackTravel;
	if (SampleParkourGroundCenterZ(FallbackEnd, true, ObstacleActor, PreviewPeakZ))
	{
		return true;
	}

	if (OutFailReason)
	{
		*OutFailReason = TEXT("No valid landing past obstacle");
	}
	return false;
}

FVector USPParkourComponent::BuildParkourVaultLocation(float MontageAlpha) const
{
	const float Alpha = FMath::Clamp(MontageAlpha, 0.f, 1.f);
	const float ForwardMontageAlpha = FMath::Clamp(
		Alpha / FMath::Max(ParkourLandByMontageAlpha, 0.01f),
		0.f,
		1.f);
	const float ForwardAlpha = 1.f - FMath::Pow(1.f - ForwardMontageAlpha, 1.85f);

	const FVector Start = ParkourStartTransform.GetLocation();
	const FVector End = ParkourVaultEndLocation;

	const float ArcHeight = FMath::Max(0.f, ParkourVaultPeakCenterZ - FMath::Max(Start.Z, End.Z));
	const float ArcFactor = GetParkourArcFactor(Alpha);

	float GatedForwardAlpha = ForwardAlpha;
	const float ClearAlpha = FMath::Clamp(ParkourWallClearMontageAlpha, 0.12f, 0.9f);
	if (!bExemptFromWallVaultLimit && Alpha < ClearAlpha * 0.95f && ArcFactor < 0.72f)
	{
		const float HeightGate = FMath::Clamp(ArcFactor / 0.72f, 0.f, 1.f);
		const float MaxForwardBeforeClear = FMath::Lerp(ClearAlpha * 0.45f, ClearAlpha, HeightGate);
		GatedForwardAlpha = FMath::Min(ForwardAlpha, MaxForwardBeforeClear);
	}

	if (!bExemptFromWallVaultLimit && ParkourLandNotifyMontageAlpha > 0.f && Alpha >= ParkourLandNotifyMontageAlpha)
	{
		const float LandForwardAlpha = FMath::Clamp(
			ParkourLandNotifyMontageAlpha / FMath::Max(ParkourLandByMontageAlpha, 0.01f),
			0.f,
			1.f);
		const float MinForwardAfterLand = 1.f - FMath::Pow(1.f - FMath::Max(LandForwardAlpha, 0.72f), 1.85f);
		GatedForwardAlpha = FMath::Max(GatedForwardAlpha, MinForwardAfterLand);
	}

	FVector Location;
	Location.X = FMath::Lerp(Start.X, End.X, GatedForwardAlpha);
	Location.Y = FMath::Lerp(Start.Y, End.Y, GatedForwardAlpha);

	float GroundZ = bParkourVaultEndValid ? ParkourVaultEndGroundZ : End.Z;
	if (bParkourFeetOnGround)
	{
		FVector GroundSample = Location;
		if (SampleParkourGroundCenterZ(GroundSample))
		{
			GroundZ = GroundSample.Z;
		}
	}

	const float TravelT = FMath::SmoothStep(0.f, 1.f, Alpha);
	const float BaseZ = FMath::Lerp(Start.Z, GroundZ, TravelT);
	const float ArcZ = BaseZ + ArcHeight * ArcFactor;

	if (bExemptFromWallVaultLimit)
	{
		const float TouchdownAlpha = ParkourLandNotifyMontageAlpha > 0.f
			? ParkourLandNotifyMontageAlpha
			: ParkourLandByMontageAlpha;
		const float DescentStart = ParkourKillerDescentStartAlpha > 0.f
			? ParkourKillerDescentStartAlpha
			: FMath::Clamp(ParkourWallClearMontageAlpha + 0.10f, 0.45f, TouchdownAlpha - 0.04f);

		if (Alpha >= TouchdownAlpha || bParkourFeetOnGround)
		{
			Location.Z = GroundZ;
		}
		else if (Alpha > DescentStart)
		{
			const float DescentT = FMath::Clamp((Alpha - DescentStart) / FMath::Max(TouchdownAlpha - DescentStart, 0.01f), 0.f, 1.f);
			Location.Z = FMath::Lerp(ArcZ, GroundZ, FMath::SmoothStep(0.f, 1.f, DescentT));
		}
		else
		{
			Location.Z = ArcZ;
		}

		return Location;
	}

	if (bParkourFeetOnGround)
	{
		const FVector Forward = ParkourStartTransform.GetRotation().GetForwardVector().GetSafeNormal2D();
		const float PlannedTravel = FVector::DotProduct(End - Start, Forward);
		const float MinLandTravel = FMath::Max(PlannedTravel * 0.72f, ParkourWallClearForward + ParkourMinLandingPastWallOffset);
		const float CurrentTravel = FVector::DotProduct(Location - Start, Forward);
		if (CurrentTravel < MinLandTravel && PlannedTravel > KINDA_SMALL_NUMBER)
		{
			const float BoostedAlpha = FMath::Clamp(MinLandTravel / PlannedTravel, GatedForwardAlpha, 1.f);
			Location.X = FMath::Lerp(Start.X, End.X, BoostedAlpha);
			Location.Y = FMath::Lerp(Start.Y, End.Y, BoostedAlpha);
		}

		Location.Z = GroundZ;
		return Location;
	}

	Location.Z = BaseZ + ArcHeight * ArcFactor;
	return Location;
}

float USPParkourComponent::GetParkourArcFactor(float MontageAlpha) const
{
	const float Alpha = FMath::Clamp(MontageAlpha, 0.f, 1.f);
	const float PeakAlpha = FMath::Clamp(ParkourVaultPeakMontageAlpha, 0.2f, 0.65f);

	float ArcFactor = 0.f;
	if (Alpha <= PeakAlpha)
	{
		ArcFactor = FMath::Sin((Alpha / PeakAlpha) * UE_PI * 0.5f);
	}
	else
	{
		const float DescentSpan = FMath::Max(1.f - PeakAlpha, 0.01f);
		const float DescentT = (Alpha - PeakAlpha) / DescentSpan;
		ArcFactor = FMath::Cos(DescentT * UE_PI * 0.5f);
	}

	if (!bParkourFeetOnGround && !bExemptFromWallVaultLimit && ParkourLandNotifyMontageAlpha > 0.f)
	{
		const float TaperStart = ParkourLandNotifyMontageAlpha * 0.8f;
		if (Alpha > TaperStart)
		{
			const float TaperSpan = FMath::Max(ParkourLandNotifyMontageAlpha - TaperStart, 0.01f);
			const float TaperT = FMath::Clamp((Alpha - TaperStart) / TaperSpan, 0.f, 1.f);
			ArcFactor *= 1.f - FMath::SmoothStep(0.f, 1.f, TaperT);
		}
	}

	return ArcFactor;
}

void USPParkourComponent::CacheParkourLandNotifyTiming(UAnimMontage* Montage)
{
	ParkourLandNotifyMontageAlpha = -1.f;
	if (!Montage)
	{
		return;
	}

	const float MontageLength = Montage->GetPlayLength();
	if (MontageLength <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	for (const FAnimNotifyEvent& NotifyEvent : Montage->Notifies)
	{
		if (NotifyEvent.Notify && NotifyEvent.Notify->IsA(UAnimNotify_SPParkourLand::StaticClass()))
		{
			ParkourLandNotifyMontageAlpha = FMath::Clamp(NotifyEvent.GetTriggerTime() / MontageLength, 0.f, 1.f);
			LogParkourDebug(
				FString::Printf(TEXT("Land notify on montage @%.2f"), ParkourLandNotifyMontageAlpha),
				FColor::Silver);
			return;
		}
	}

	for (const FSlotAnimationTrack& SlotTrack : Montage->SlotAnimTracks)
	{
		for (const FAnimSegment& Segment : SlotTrack.AnimTrack.AnimSegments)
		{
			const UAnimSequenceBase* AnimReference = Segment.GetAnimReference().Get();
			if (!AnimReference)
			{
				continue;
			}

			for (const FAnimNotifyEvent& NotifyEvent : AnimReference->Notifies)
			{
				if (!NotifyEvent.Notify || !NotifyEvent.Notify->IsA(UAnimNotify_SPParkourLand::StaticClass()))
				{
					continue;
				}

				const float MontageTime = Segment.StartPos + NotifyEvent.GetTriggerTime() - Segment.AnimStartTime;
				ParkourLandNotifyMontageAlpha = FMath::Clamp(MontageTime / MontageLength, 0.f, 1.f);
				LogParkourDebug(
					FString::Printf(TEXT("Land notify on sequence @%.2f"), ParkourLandNotifyMontageAlpha),
					FColor::Silver);
				return;
			}
		}
	}
}

bool USPParkourComponent::SampleParkourGroundCenterZ(
	FVector& InOutCenterLocation,
	bool bIgnoreParkourObstacle,
	AActor* IgnoredObstacleOverride,
	float PeakCenterZOverride) const
{
	const ACharacter* Character = GetParkourCharacter();
	const UCapsuleComponent* Capsule = Character ? Character->GetCapsuleComponent() : nullptr;
	if (!Capsule || !GetWorld())
	{
		return false;
	}

	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const float PeakZ = PeakCenterZOverride >= 0.f ? PeakCenterZOverride : ParkourVaultPeakCenterZ;
	const float TraceHighZ = FMath::Max(InOutCenterLocation.Z, PeakZ) + HalfHeight + 120.f;
	const FVector TraceStart(InOutCenterLocation.X, InOutCenterLocation.Y, TraceHighZ);
	const FVector TraceEnd(InOutCenterLocation.X, InOutCenterLocation.Y, InOutCenterLocation.Z - 400.f);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ParkourLandSample), false, Character);
	if (bIgnoreParkourObstacle)
	{
		if (IgnoredObstacleOverride)
		{
			Params.AddIgnoredActor(IgnoredObstacleOverride);
		}
		else if (AActor* ObstacleActor = CurrentParkourObstacle.Get())
		{
			Params.AddIgnoredActor(ObstacleActor);
		}
	}

	FHitResult GroundHit;
	const ECollisionChannel GroundChannels[] = { ECC_Visibility, ECC_WorldStatic, ECC_WorldDynamic };
	for (const ECollisionChannel Channel : GroundChannels)
	{
		if (GetWorld()->LineTraceSingleByChannel(
			GroundHit,
			TraceStart,
			TraceEnd,
			Channel,
			Params))
		{
			InOutCenterLocation.Z = GroundHit.ImpactPoint.Z + HalfHeight;
			return true;
		}
	}

	return false;
}

FVector USPParkourComponent::BuildParkourLocationFromRootMotion(
	const FVector& RootMotionTranslation,
	float MontageTime,
	float MontageLength) const
{
	const FQuat StartRotation = ParkourStartTransform.GetRotation();
	FVector Location = ParkourStartTransform.GetLocation() + StartRotation.RotateVector(RootMotionTranslation);

	if (ParkourObstacleHeight <= KINDA_SMALL_NUMBER || MontageLength <= KINDA_SMALL_NUMBER)
	{
		return Location;
	}

	const ACharacter* Character = GetParkourCharacter();
	const UCapsuleComponent* Capsule = Character ? Character->GetCapsuleComponent() : nullptr;
	if (!Capsule)
	{
		return Location;
	}

	const float MontageAlpha = FMath::Clamp(MontageTime / MontageLength, 0.f, 1.f);
	const float VaultCurve = FMath::Sin(MontageAlpha * UE_PI);

	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const float StartCenterZ = ParkourStartTransform.GetLocation().Z;
	const float ObstacleTopZ = ParkourStartFeetZ + ParkourObstacleHeight;
	const float RequiredPeakCenterZ = ObstacleTopZ + HalfHeight + ParkourClearanceOverObstacle;

	const float AnimLiftZ = Location.Z - StartCenterZ;
	const float RequiredLiftZ = RequiredPeakCenterZ - StartCenterZ;
	if (RequiredLiftZ > AnimLiftZ)
	{
		Location.Z += (RequiredLiftZ - AnimLiftZ) * VaultCurve;
	}

	return Location;
}

void USPParkourComponent::OnParkourLandNotify()
{
	if (!bIsParkour)
	{
		return;
	}

	// Killer: snap to ground exactly when the anim land notify fires.
	if (bExemptFromWallVaultLimit)
	{
		ACharacter* Character = GetParkourCharacter();
		if (!Character)
		{
			return;
		}

		const FVector Forward = ParkourStartTransform.GetRotation().GetForwardVector().GetSafeNormal2D();
		const float CurrentTravel = FVector::DotProduct(
			Character->GetActorLocation() - ParkourStartTransform.GetLocation(),
			Forward);
		const float PlannedTravel = FMath::Max(ParkourPlannedForwardTravel, 1.f);
		const float MinClearTravel = FMath::Max(
			PlannedTravel * 0.75f,
			ParkourWallClearForward + ParkourMinLandingPastWallOffset * 0.35f);
		if (CurrentTravel < MinClearTravel)
		{
			LogParkourDebug(
				FString::Printf(
					TEXT("Killer land notify deferred (%.0f < %.0f)"),
					CurrentTravel,
					MinClearTravel),
				FColor::Yellow);
			return;
		}

		bParkourFeetOnGround = true;

		FVector GroundedLocation = BuildParkourVaultLocation(GetParkourMontageAlpha());
		if (!SnapParkourLocationToGround(GroundedLocation))
		{
			GroundedLocation = ParkourVaultEndLocation;
			SnapParkourLocationToGround(GroundedLocation);
		}

		if (UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
		{
			MoveComp->StopMovementImmediately();
			MoveComp->Velocity = FVector::ZeroVector;
		}

		Character->SetActorLocation(GroundedLocation, false, nullptr, ETeleportType::TeleportPhysics);
		Character->SetActorRotation(ParkourStartTransform.Rotator());

		if (UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
		{
			MoveComp->bJustTeleported = true;
		}

		LogParkourDebug(
			FString::Printf(
				TEXT("Killer land notify @%.2f -> %s"),
				GetParkourMontageAlpha(),
				*GroundedLocation.ToCompactString()),
			FColor::Green);
		return;
	}

	ACharacter* Character = GetParkourCharacter();
	if (!Character)
	{
		return;
	}

	const FVector Forward = ParkourStartTransform.GetRotation().GetForwardVector().GetSafeNormal2D();
	const float CurrentTravel = FVector::DotProduct(
		Character->GetActorLocation() - ParkourStartTransform.GetLocation(),
		Forward);
	const float PlannedTravel = FMath::Max(ParkourPlannedForwardTravel, 1.f);
	const float MinClearTravel = bExemptFromWallVaultLimit
		? FMath::Max(PlannedTravel * 0.88f, ParkourWallClearForward + ParkourMinLandingPastWallOffset * 0.5f)
		: FMath::Max(
			ParkourWallClearForward + ParkourMinLandingPastWallOffset * 0.35f,
			45.f);
	if (CurrentTravel < MinClearTravel)
	{
		LogParkourDebug(
			FString::Printf(
				TEXT("Land notify deferred (%.0f < %.0f, planned %.0f)"),
				CurrentTravel,
				MinClearTravel,
				PlannedTravel),
			FColor::Yellow);
		return;
	}

	bParkourFeetOnGround = true;

	FVector GroundedLocation = BuildParkourVaultLocation(GetParkourMontageAlpha());
	if (!SnapParkourLocationToGround(GroundedLocation))
	{
		GroundedLocation = ParkourVaultEndLocation;
		SnapParkourLocationToGround(GroundedLocation);
	}

	if (UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
	{
		MoveComp->StopMovementImmediately();
		MoveComp->Velocity = FVector::ZeroVector;
	}

	Character->SetActorLocation(GroundedLocation, false, nullptr, ETeleportType::TeleportPhysics);
	Character->SetActorRotation(ParkourStartTransform.Rotator());

	if (UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
	{
		MoveComp->bJustTeleported = true;
	}

	LogParkourDebug(
		FString::Printf(
			TEXT("Land notify @%.2f -> %s"),
			GetParkourMontageAlpha(),
			*GroundedLocation.ToCompactString()),
		FColor::Green);
}

void USPParkourComponent::RegisterParkourZoneOverlap(ASPParkourZone* Zone, bool bIsOverlapping)
{
	if (!Zone)
	{
		return;
	}

	if (bIsOverlapping)
	{
		OverlappingParkourZones.AddUnique(Zone);
		LogParkourDebug(FString::Printf(TEXT("Entered parkour zone %s"), *Zone->GetName()), FColor::Cyan);
	}
	else
	{
		OverlappingParkourZones.RemoveAll([Zone](const TWeakObjectPtr<ASPParkourZone>& Entry)
		{
			return !Entry.IsValid() || Entry.Get() == Zone;
		});
		LogParkourDebug(FString::Printf(TEXT("Left parkour zone %s"), *Zone->GetName()), FColor::Cyan);
	}

	OverlappingParkourZones.RemoveAll([](const TWeakObjectPtr<ASPParkourZone>& Entry)
	{
		return !Entry.IsValid();
	});
}

bool USPParkourComponent::IsInsideParkourZone() const
{
	if (!bRequireParkourZone)
	{
		return true;
	}

	const ACharacter* Character = GetParkourCharacter();
	if (!Character || !GetWorld())
	{
		return false;
	}

	const FVector CharacterLocation = Character->GetActorLocation();
	for (TActorIterator<ASPParkourZone> It(GetWorld()); It; ++It)
	{
		if (It->ContainsPoint(CharacterLocation))
		{
			return true;
		}
	}

	for (const TWeakObjectPtr<ASPParkourZone>& ZonePtr : OverlappingParkourZones)
	{
		if (ZonePtr.IsValid())
		{
			return true;
		}
	}

	return false;
}

FParkourWallVaultRecord* USPParkourComponent::FindWallVaultRecord(AActor* ObstacleActor)
{
	if (!ObstacleActor)
	{
		return nullptr;
	}

	WallVaultRecords.RemoveAll([](const FParkourWallVaultRecord& Record)
	{
		return !Record.Obstacle.IsValid() && Record.BlockedUntilTime <= 0.0 && Record.VaultTimestamps.Num() == 0;
	});

	for (FParkourWallVaultRecord& Record : WallVaultRecords)
	{
		if (Record.Obstacle.Get() == ObstacleActor)
		{
			return &Record;
		}
	}

	return nullptr;
}

const FParkourWallVaultRecord* USPParkourComponent::FindWallVaultRecord(AActor* ObstacleActor) const
{
	if (!ObstacleActor)
	{
		return nullptr;
	}

	for (const FParkourWallVaultRecord& Record : WallVaultRecords)
	{
		if (Record.Obstacle.Get() == ObstacleActor)
		{
			return &Record;
		}
	}

	return nullptr;
}

void USPParkourComponent::PruneWallVaultRecord(FParkourWallVaultRecord& Record, double Now) const
{
	const double WindowStart = Now - WallVaultWindowSeconds;
	Record.VaultTimestamps.RemoveAll([WindowStart](const double Timestamp)
	{
		return Timestamp < WindowStart;
	});
}

bool USPParkourComponent::IsObstacleVaultBlocked(AActor* ObstacleActor, FString* OutFailReason) const
{
	if (bExemptFromWallVaultLimit)
	{
		return false;
	}

	auto SetFail = [OutFailReason](const FString& Reason)
	{
		if (OutFailReason)
		{
			*OutFailReason = Reason;
		}
	};

	if (!ObstacleActor || !GetWorld())
	{
		return false;
	}

	const double Now = GetWorld()->GetTimeSeconds();
	const FParkourWallVaultRecord* Record = FindWallVaultRecord(ObstacleActor);
	if (!Record)
	{
		return false;
	}

	if (Now < Record->BlockedUntilTime)
	{
		const double RemainingSeconds = Record->BlockedUntilTime - Now;
		SetFail(FString::Printf(
			TEXT("Wall on cooldown (%.0fs left)"),
			RemainingSeconds));
		return true;
	}

	return false;
}

bool USPParkourComponent::TryRegisterWallVault(AActor* ObstacleActor, FString* OutFailReason)
{
	if (bExemptFromWallVaultLimit)
	{
		return true;
	}

	auto SetFail = [OutFailReason](const FString& Reason)
	{
		if (OutFailReason)
		{
			*OutFailReason = Reason;
		}
	};

	if (!ObstacleActor || !GetWorld())
	{
		return true;
	}

	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return true;
	}

	const double Now = GetWorld()->GetTimeSeconds();
	FParkourWallVaultRecord* Record = FindWallVaultRecord(ObstacleActor);
	if (!Record)
	{
		FParkourWallVaultRecord NewRecord;
		NewRecord.Obstacle = ObstacleActor;
		WallVaultRecords.Add(NewRecord);
		Record = &WallVaultRecords.Last();
	}

	if (Now < Record->BlockedUntilTime)
	{
		SetFail(TEXT("Wall on cooldown"));
		return false;
	}

	PruneWallVaultRecord(*Record, Now);
	Record->VaultTimestamps.Add(Now);

	if (Record->VaultTimestamps.Num() >= MaxVaultsPerWallInWindow)
	{
		Record->BlockedUntilTime = Now + WallVaultCooldownSeconds;
		LogParkourDebug(
			FString::Printf(
				TEXT("Wall %s locked for %.0fs after %d vaults"),
				*ObstacleActor->GetName(),
				WallVaultCooldownSeconds,
				MaxVaultsPerWallInWindow),
			FColor::Orange);
	}

	return true;
}

void USPParkourComponent::UpdateParkourRootMotion(float DeltaTime)
{
	if (!ActiveParkourMontage)
	{
		return;
	}

	ACharacter* Character = GetParkourCharacter();
	USkeletalMeshComponent* MeshComp = Character ? Character->GetMesh() : nullptr;
	UAnimInstance* AnimInstance = MeshComp ? MeshComp->GetAnimInstance() : nullptr;
	UCharacterMovementComponent* MoveComp = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Character || !AnimInstance || !MoveComp)
	{
		return;
	}

	const float MontageAlpha = GetParkourMontageAlpha();

	const float MontageTime = ParkourMontageDuration * MontageAlpha;
	FVector RootMotionTranslation = FVector::ZeroVector;
	FQuat RootMotionRotation = FQuat::Identity;
	ExtractMontageRootMotionAtTime(ActiveParkourMontage, MontageTime, RootMotionTranslation, RootMotionRotation);

	const FQuat StartRotation = ParkourStartTransform.GetRotation();
	const FVector TargetLocation = BuildParkourVaultLocation(MontageAlpha);
	const FRotator TargetRotation(0.f, (StartRotation * RootMotionRotation).Rotator().Yaw, 0.f);

	const FVector CurrentLocation = Character->GetActorLocation();
	const FVector NewLocation = TargetLocation;
	const FRotator NewRotation = bExemptFromWallVaultLimit
		? TargetRotation
		: FMath::RInterpTo(Character->GetActorRotation(), TargetRotation, DeltaTime, 14.f);

	if (!NewLocation.Equals(CurrentLocation, 0.1f) || !Character->GetActorRotation().Equals(NewRotation, 0.5f))
	{
		Character->SetActorLocation(NewLocation, false, nullptr, ETeleportType::TeleportPhysics);
		Character->SetActorRotation(NewRotation);
		MoveComp->Velocity = FVector::ZeroVector;
		MoveComp->bJustTeleported = true;
	}

	if (bDrawDebug)
	{
		DrawDebugSphere(GetWorld(), NewLocation, 8.f, 8, FColor::Magenta, false, 0.f, 0, 1.f);
	}
}

float USPParkourComponent::GetParkourMontageAlpha() const
{
	if (ParkourMontageDuration <= KINDA_SMALL_NUMBER)
	{
		return 1.f;
	}

	if (ActiveParkourMontage)
	{
		if (const ACharacter* Character = GetParkourCharacter())
		{
			if (const USkeletalMeshComponent* MeshComp = Character->GetMesh())
			{
				if (const UAnimInstance* AnimInstance = MeshComp->GetAnimInstance())
				{
					if (AnimInstance->Montage_IsPlaying(ActiveParkourMontage))
					{
						const float MontageLength = ActiveParkourMontage->GetPlayLength();
						if (MontageLength > KINDA_SMALL_NUMBER)
						{
							return FMath::Clamp(
								AnimInstance->Montage_GetPosition(ActiveParkourMontage) / MontageLength,
								0.f,
								1.f);
						}
					}
				}
			}
		}
	}

	return FMath::Clamp(ParkourMontageElapsed / ParkourMontageDuration, 0.f, 1.f);
}

bool USPParkourComponent::HasSufficientVaultForwardProgress(const FVector& Location) const
{
	if (bParkourFeetOnGround)
	{
		return true;
	}

	const FVector Forward = ParkourStartTransform.GetRotation().GetForwardVector().GetSafeNormal2D();
	const FVector StartCenter = ParkourStartTransform.GetLocation();
	const float PlannedTravel = FVector::DotProduct(ParkourVaultEndLocation - StartCenter, Forward);
	const float ActualTravel = FVector::DotProduct(Location - StartCenter, Forward);
	if (PlannedTravel <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	if (FVector::Dist2D(Location, ParkourVaultEndLocation) <= (bExemptFromWallVaultLimit ? 130.f : 100.f))
	{
		return true;
	}

	if (bExemptFromWallVaultLimit)
	{
		const float MinRequiredTravel = FMath::Max(
			ParkourWallClearForward + ParkourMinLandingPastWallOffset * 0.25f,
			PlannedTravel * 0.22f);
		return ActualTravel >= MinRequiredTravel;
	}

	const float MinRequiredTravel = FMath::Max(
		PlannedTravel * 0.3f,
		FMath::Max(ParkourWallClearForward * 0.25f, 30.f));
	return ActualTravel >= MinRequiredTravel;
}

bool USPParkourComponent::ExtractMontageRootMotionAtTime(
	const UAnimMontage* Montage,
	float MontageTime,
	FVector& OutTranslation,
	FQuat& OutRotation) const
{
	OutTranslation = FVector::ZeroVector;
	OutRotation = FQuat::Identity;

	if (!Montage)
	{
		return false;
	}

	MontageTime = FMath::Clamp(MontageTime, 0.f, Montage->GetPlayLength());
	const FTransform RootDelta = Montage->ExtractRootMotionFromTrackRange(0.f, MontageTime);
	OutTranslation = RootDelta.GetTranslation();
	OutRotation = RootDelta.GetRotation();
	return true;
}

bool USPParkourComponent::ComputeParkourLandingFromRootMotion(FVector& OutLocation, FRotator& OutRotation) const
{
	if (!ActiveParkourMontage)
	{
		return false;
	}

	FVector RootMotionTranslation = FVector::ZeroVector;
	FQuat RootMotionRotation = FQuat::Identity;
	const float EndMontageTime = ActiveParkourMontage->GetPlayLength();
	if (!ExtractMontageRootMotionAtTime(ActiveParkourMontage, EndMontageTime, RootMotionTranslation, RootMotionRotation))
	{
		return false;
	}

	const FQuat StartRotation = ParkourStartTransform.GetRotation();
	OutLocation = BuildParkourLocationFromRootMotion(RootMotionTranslation, EndMontageTime, EndMontageTime);
	OutRotation = FRotator(0.f, (StartRotation * RootMotionRotation).Rotator().Yaw, 0.f);
	if (!SnapParkourLocationToGround(OutLocation))
	{
		return false;
	}

	LogParkourDebug(
		FString::Printf(TEXT("RootMotion delta %s -> %s"), *RootMotionTranslation.ToCompactString(), *OutLocation.ToCompactString()),
		FColor::Green);
	return true;
}

bool USPParkourComponent::ResolveParkourLanding(FVector& OutLocation, FRotator& OutRotation) const
{
	if (!bParkourVaultEndValid)
	{
		return false;
	}

	OutRotation = ParkourStartTransform.Rotator();

	if (bExemptFromWallVaultLimit)
	{
		OutLocation = ParkourVaultEndLocation;
		FVector GroundedEnd = OutLocation;
		SnapParkourLocationToGround(GroundedEnd);
		OutLocation = GroundedEnd;
		LogParkourDebug(
			FString::Printf(TEXT("Killer vault land %s"), *OutLocation.ToCompactString()),
			FColor::Green);
		return true;
	}

	const ACharacter* Character = GetParkourCharacter();
	if (Character)
	{
		OutLocation = Character->GetActorLocation();
		FVector GroundedLocation = OutLocation;
		if (HasSufficientVaultForwardProgress(OutLocation) &&
			SampleParkourGroundCenterZ(GroundedLocation) &&
			ValidateParkourLandingLocation(ParkourStartTransform, ParkourStartFeetZ, GroundedLocation, nullptr))
		{
			OutLocation = GroundedLocation;
			LogParkourDebug(
				FString::Printf(TEXT("Vault land (tracked) %s"), *OutLocation.ToCompactString()),
				FColor::Green);
			return true;
		}
		else
		{
			const FVector Forward = ParkourStartTransform.GetRotation().GetForwardVector().GetSafeNormal2D();
			const float ActualTravel = FVector::DotProduct(OutLocation - ParkourStartTransform.GetLocation(), Forward);
			LogParkourDebug(
				FString::Printf(TEXT("Tracked landing insufficient (%.0fcm) - using vault end"), ActualTravel),
				FColor::Orange);
		}
	}

	OutLocation = ParkourVaultEndLocation;
	FVector ValidatedLocation = OutLocation;
	if (!SampleParkourGroundCenterZ(ValidatedLocation) ||
		!ValidateParkourLandingLocation(ParkourStartTransform, ParkourStartFeetZ, ValidatedLocation, nullptr))
	{
		return false;
	}

	OutLocation = ValidatedLocation;

	LogParkourDebug(
		FString::Printf(TEXT("Vault land %s"), *OutLocation.ToCompactString()),
		FColor::Green);
	return true;
}

bool USPParkourComponent::SnapParkourLocationToGround(FVector& InOutLocation, bool bIgnoreParkourObstacle) const
{
	return SampleParkourGroundCenterZ(InOutLocation, bIgnoreParkourObstacle);
}

void USPParkourComponent::SetParkourWallCollisionDisabled(bool bDisabled, AActor* PrimaryObstacle)
{
	ACharacter* Character = GetParkourCharacter();
	if (!Character || !GetWorld())
	{
		return;
	}

	UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	USkeletalMeshComponent* MeshComp = Character->GetMesh();

	auto SetActorIgnored = [&](AActor* Actor, bool bIgnore)
	{
		if (!Actor || Actor == Character || SPParkour::IsLandscapeLikeObstacle(Actor))
		{
			return;
		}

		if (Capsule)
		{
			Capsule->IgnoreActorWhenMoving(Actor, bIgnore);
		}

		if (MeshComp)
		{
			MeshComp->IgnoreActorWhenMoving(Actor, bIgnore);
		}
	};

	if (bDisabled)
	{
		if (AActor* ObstacleActor = PrimaryObstacle ? PrimaryObstacle : CurrentParkourObstacle.Get())
		{
			bool bAlreadyIgnored = false;
			for (const TWeakObjectPtr<AActor>& Existing : CachedIgnoredWallActors)
			{
				if (Existing.Get() == ObstacleActor)
				{
					bAlreadyIgnored = true;
					break;
				}
			}

			if (!bAlreadyIgnored)
			{
				CachedIgnoredWallActors.Add(ObstacleActor);
			}

			SetActorIgnored(ObstacleActor, true);
		}

		LogParkourDebug(
			FString::Printf(
				TEXT("Wall ignored for parkour character only (%d actors)"),
				CachedIgnoredWallActors.Num()),
			FColor::Silver);
		return;
	}

	for (const TWeakObjectPtr<AActor>& CachedActor : CachedIgnoredWallActors)
	{
		SetActorIgnored(CachedActor.Get(), false);
	}
	CachedIgnoredWallActors.Empty();
	LogParkourDebug(TEXT("Wall collision ignore restored"), FColor::Silver);
}

void USPParkourComponent::SetParkourCollisionSuppressed(bool bSuppressed)
{
	ACharacter* Character = GetParkourCharacter();
	if (!Character)
	{
		return;
	}

	if (bSuppressed)
	{
		if (bParkourCollisionSuppressed)
		{
			return;
		}

		if (UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
		{
			CachedCapsuleCollision = static_cast<uint8>(Capsule->GetCollisionEnabled());
			Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}

		if (USkeletalMeshComponent* MeshComp = Character->GetMesh())
		{
			CachedMeshCollision = static_cast<uint8>(MeshComp->GetCollisionEnabled());
			if (static_cast<ECollisionEnabled::Type>(CachedMeshCollision) != ECollisionEnabled::NoCollision)
			{
				MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			}
		}

		bParkourCollisionSuppressed = true;
		LogParkourDebug(TEXT("Parkour collision suppressed"), FColor::Silver);
		return;
	}

	if (!bParkourCollisionSuppressed)
	{
		return;
	}

	if (UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(static_cast<ECollisionEnabled::Type>(CachedCapsuleCollision));
	}

	if (USkeletalMeshComponent* MeshComp = Character->GetMesh())
	{
		if (static_cast<ECollisionEnabled::Type>(CachedMeshCollision) != ECollisionEnabled::NoCollision)
		{
			MeshComp->SetCollisionEnabled(static_cast<ECollisionEnabled::Type>(CachedMeshCollision));
		}
	}

	bParkourCollisionSuppressed = false;
	LogParkourDebug(TEXT("Parkour collision restored"), FColor::Silver);
}

void USPParkourComponent::ApplyParkourLanding(const FVector& WorldLocation, const FRotator& WorldRotation)
{
	ACharacter* Character = GetParkourCharacter();
	if (!Character)
	{
		return;
	}

	UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement();
	if (MoveComp)
	{
		MoveComp->StopMovementImmediately();
		MoveComp->Velocity = FVector::ZeroVector;
		MoveComp->ClearAccumulatedForces();
	}

	FVector ResolvedLocation = WorldLocation;
	SnapParkourLocationToGround(ResolvedLocation);

	const FVector CurrentLocation = Character->GetActorLocation();
	const bool bAlreadyAligned = FVector::Dist2D(CurrentLocation, ResolvedLocation) <= 12.f
		&& FMath::Abs(CurrentLocation.Z - ResolvedLocation.Z) <= 8.f;

	if (!bAlreadyAligned)
	{
		Character->TeleportTo(ResolvedLocation, WorldRotation, false, true);
	}
	else if (!Character->GetActorRotation().Equals(WorldRotation, 1.f))
	{
		Character->SetActorRotation(WorldRotation);
	}
	else if (FMath::Abs(CurrentLocation.Z - ResolvedLocation.Z) > 1.f)
	{
		FVector AdjustedLocation = CurrentLocation;
		AdjustedLocation.Z = ResolvedLocation.Z;
		Character->SetActorLocation(AdjustedLocation, false, nullptr, ETeleportType::TeleportPhysics);
	}

	if (MoveComp)
	{
		MoveComp->SetMovementMode(MOVE_Walking);
		MoveComp->bJustTeleported = true;
		MoveComp->bForceNextFloorCheck = true;

		if (UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
		{
			MoveComp->FindFloor(Capsule->GetComponentLocation(), MoveComp->CurrentFloor, false, nullptr);
		}
	}

	LogParkourDebug(
		bAlreadyAligned
			? FString::Printf(TEXT("Landing settled at %s"), *ResolvedLocation.ToCompactString())
			: FString::Printf(TEXT("Teleport to %s"), *ResolvedLocation.ToCompactString()),
		FColor::Green);
}

void USPParkourComponent::Multicast_SnapParkourLanding_Implementation(FVector_NetQuantize WorldLocation, FRotator WorldRotation)
{
	ApplyParkourLanding(WorldLocation, WorldRotation);
}

void USPParkourComponent::OnParkourEndTimer()
{
	if (bIsParkour)
	{
		EndParkour(false);
	}
}

void USPParkourComponent::OnParkourMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	EndParkour(bInterrupted);
}

void USPParkourComponent::EndParkour(bool bInterrupted)
{
	if (!bIsParkour)
	{
		return;
	}

	ACharacter* Character = GetParkourCharacter();

	if (!bInterrupted && bParkourVaultEndValid && Character)
	{
		ParkourMontageElapsed = ParkourMontageDuration;
		UpdateParkourRootMotion(1.f / 60.f);
	}

	bIsParkour = false;
	GetWorld()->GetTimerManager().ClearTimer(ParkourEndTimerHandle);

	FVector LandingLocation = ParkourStartTransform.GetLocation();
	FRotator LandingRotation = ParkourStartTransform.Rotator();
	bool bHasLanding = ResolveParkourLanding(LandingLocation, LandingRotation);
	if (!bHasLanding)
	{
		if (bParkourVaultEndValid)
		{
			LandingLocation = ParkourVaultEndLocation;
			SnapParkourLocationToGround(LandingLocation);
			LandingRotation = ParkourStartTransform.Rotator();
			bHasLanding = true;
			LogParkourDebug(TEXT("Invalid landing - using vault end"), FColor::Orange);
		}
		else
		{
			LandingLocation = ParkourStartTransform.GetLocation();
			LandingRotation = ParkourStartTransform.Rotator();
			bHasLanding = true;
			LogParkourDebug(TEXT("Invalid landing - snapped back to start"), FColor::Orange);
		}
	}

	if (Character)
	{
		if (USkeletalMeshComponent* MeshComp = Character->GetMesh())
		{
			if (UAnimInstance* AnimInstance = MeshComp->GetAnimInstance())
			{
				AnimInstance->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
			}
		}
	}

	if (bHasLanding)
	{
		if (GetOwner()->HasAuthority())
		{
			Multicast_SnapParkourLanding(LandingLocation, LandingRotation);
		}
		else
		{
			ApplyParkourLanding(LandingLocation, LandingRotation);
		}
	}

	if (Character)
	{
		if (USkeletalMeshComponent* MeshComp = Character->GetMesh())
		{
			if (UAnimInstance* AnimInstance = MeshComp->GetAnimInstance())
			{
				if (ActiveParkourMontage && AnimInstance->Montage_IsPlaying(ActiveParkourMontage))
				{
					AnimInstance->Montage_Stop(0.f, ActiveParkourMontage);
				}

				AnimInstance->SetRootMotionMode(CachedRootMotionMode);
			}
		}
	}

	if (CurrentParkourObstacle.IsValid() || !CachedIgnoredWallActors.IsEmpty())
	{
		SetParkourWallCollisionDisabled(false);
	}

	SetParkourCollisionSuppressed(false);

	if (Character)
	{
		if (UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
		{
			Character->PrimaryActorTick.RemovePrerequisite(MoveComp, MoveComp->PrimaryComponentTick);
			MoveComp->bAllowPhysicsRotationDuringAnimRootMotion = false;
			MoveComp->GravityScale = CachedGravityScale;
			MoveComp->NetworkSmoothingMode = CachedNetworkSmoothingMode;
			if (CachedMovementMode != MOVE_None)
			{
				MoveComp->SetMovementMode(CachedMovementMode);
			}
		}
	}

	CurrentParkourObstacle = nullptr;
	ActiveParkourMontage = nullptr;
	ParkourObstacleHeight = 0.f;
	ParkourObstacleTopZ = 0.f;
	ParkourStartFeetZ = 0.f;
	ParkourObstacleImpactPoint = FVector::ZeroVector;
	ParkourPlannedForwardTravel = 0.f;
	ParkourVaultEndGroundZ = 0.f;
	ParkourVaultEndLocation = FVector::ZeroVector;
	ParkourVaultPeakCenterZ = 0.f;
	ParkourWallClearForward = 0.f;
	ParkourWallClearMontageAlpha = 0.f;
	bParkourVaultEndValid = false;
	bParkourFeetOnGround = false;
	ParkourLandNotifyMontageAlpha = -1.f;
	ParkourKillerDescentStartAlpha = -1.f;
	ParkourMontageElapsed = 0.f;
	ParkourMontageDuration = 0.f;

	if (Character)
	{
		if (AController* Ctrl = Character->GetController())
		{
			Ctrl->ResetIgnoreMoveInput();
		}

		if (ASurvivorCharacter* Survivor = Cast<ASurvivorCharacter>(Character))
		{
			Survivor->NotifyParkourEnded();
		}
		else if (AKillerCharacter* Killer = Cast<AKillerCharacter>(Character))
		{
			Killer->NotifyParkourEnded();
		}
	}

	LogParkourDebug(bInterrupted ? TEXT("Parkour ended (interrupted)") : TEXT("Parkour ended"), FColor::Cyan);
}

void USPParkourComponent::LogParkourDebug(const FString& Message, FColor Color) const
{
	if (bDrawDebug && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.f, Color, FString::Printf(TEXT("[Parkour] %s"), *Message));
	}

	UE_LOG(LogTemp, Log, TEXT("[Parkour] %s"), *Message);
}

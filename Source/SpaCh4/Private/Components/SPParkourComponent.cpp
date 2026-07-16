#include "Components/SPParkourComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotify_SPParkourLand.h"
#include "Characters/Survivor/SurvivorCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/SPInteractionComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Gameplay/Collectibles/SPCollectibleItem.h"
#include "Gameplay/Parkour/SPParkourZone.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace SPParkour
{
	static constexpr TCHAR HurdleMontagePath[] =
		TEXT("/Game/Assets/Survivor_Locomotion/Miscellaneous/M_Neutral_Traversal_Hurdle_1_0_stand_F_V2_Rfoot1_Montage.M_Neutral_Traversal_Hurdle_1_0_stand_F_V2_Rfoot1_Montage");

	static FCollisionObjectQueryParams MakeObstacleObjectQuery()
	{
		FCollisionObjectQueryParams Params;
		Params.AddObjectTypesToQuery(ECC_WorldStatic);
		Params.AddObjectTypesToQuery(ECC_WorldDynamic);
		return Params;
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

	EnsureMontagesLoaded();

	if (ASurvivorCharacter* Survivor = GetSurvivor())
	{
		if (USkeletalMeshComponent* MeshComp = Survivor->GetMesh())
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
	UpdateParkourRootMotion(DeltaTime);

	ASurvivorCharacter* Survivor = GetSurvivor();
	UAnimInstance* AnimInstance = Survivor && Survivor->GetMesh() ? Survivor->GetMesh()->GetAnimInstance() : nullptr;
	if (!AnimInstance || !AnimInstance->Montage_IsPlaying(ActiveParkourMontage))
	{
		EndParkour(false);
	}
}

ASurvivorCharacter* USPParkourComponent::GetSurvivor() const
{
	return Cast<ASurvivorCharacter>(GetOwner());
}

bool USPParkourComponent::CanJumpOver() const
{
	if (bIsParkour)
	{
		return false;
	}

	const ASurvivorCharacter* Survivor = GetSurvivor();
	if (!Survivor)
	{
		return false;
	}

	if (const UCharacterMovementComponent* MoveComp = Survivor->GetCharacterMovement())
	{
		if (MoveComp->IsFalling())
		{
			return false;
		}
	}

	const ESurvivorState State = Survivor->GetSurvivorState();
	if (State != ESurvivorState::Healthy && State != ESurvivorState::Injured)
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
	LogParkourDebug(TEXT("JumpOver 입력"), FColor::Cyan);

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
	if (!ParkourMontage)
	{
		ParkourMontage = LoadObject<UAnimMontage>(nullptr, SPParkour::HurdleMontagePath);
	}
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
		LogParkourDebug(TEXT("ParkourMontage null — Hurdle 몽타주 경로 확인"), FColor::Red);
		return;
	}

	ASurvivorCharacter* Survivor = GetSurvivor();
	if (!Survivor)
	{
		return;
	}

	const FVector ToObstacle = ObstacleHit.ImpactPoint - Survivor->GetActorLocation();
	const FRotator FacingRotation = FRotator(0.f, FRotationMatrix::MakeFromX(ToObstacle.GetSafeNormal2D()).Rotator().Yaw, 0.f);

	FString VaultFailReason;
	if (!PreviewParkourVault(FacingRotation, ObstacleHeight, ObstacleHit.GetActor(), ObstacleHit.ImpactPoint, &VaultFailReason))
	{
		LogParkourDebug(
			VaultFailReason.IsEmpty() ? TEXT("Vault landing preview failed") : VaultFailReason,
			FColor::Orange);
		return;
	}

	FString WallLimitReason;
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

	LogParkourDebug(
		FString::Printf(TEXT("Start H:%.0f D:%.0f -> %s"), ObstacleHeight, FVector::Dist2D(Survivor->GetActorLocation(), ObstacleHit.ImpactPoint), *Montage->GetName()),
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
	const ASurvivorCharacter* Survivor = GetSurvivor();
	if (!Survivor)
	{
		OutForward = FVector::ForwardVector;
		OutRight = FVector::RightVector;
		return;
	}

	OutForward = Survivor->GetActorForwardVector().GetSafeNormal2D();
	OutRight = Survivor->GetActorRightVector().GetSafeNormal2D();

	if (const UCharacterMovementComponent* MoveComp = Survivor->GetCharacterMovement())
	{
		const FVector Velocity2D = FVector(MoveComp->Velocity.X, MoveComp->Velocity.Y, 0.f);
		if (!Velocity2D.IsNearlyZero(25.f))
		{
			OutForward = Velocity2D.GetSafeNormal();
			OutRight = FVector::CrossProduct(FVector::UpVector, OutForward).GetSafeNormal();
		}
	}
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

	const ASurvivorCharacter* Survivor = GetSurvivor();
	const UCapsuleComponent* Capsule = Survivor ? Survivor->GetCapsuleComponent() : nullptr;
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
	const FVector FeetLocation = Survivor->GetActorLocation() - FVector(0.f, 0.f, HalfHeight);
	const FVector TraceStart = FeetLocation + FVector(0.f, 0.f, HalfHeight * 0.6f);
	const FVector TraceEnd = TraceStart + Forward * ParkourTraceDistance;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ParkourTrace), false, Survivor);

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

	const float HitDistance = FVector::Dist2D(FeetLocation, OutObstacleHit.ImpactPoint);
	if (HitDistance < ParkourMinStartDistance)
	{
		SetFail(FString::Printf(TEXT("Too close (%.0f < %.0f)"), HitDistance, ParkourMinStartDistance));
		return false;
	}
	if (HitDistance > ParkourMaxStartDistance)
	{
		SetFail(FString::Printf(TEXT("Too far (%.0f > %.0f)"), HitDistance, ParkourMaxStartDistance));
		return false;
	}

	const FVector SurfaceNormal2D = FVector(OutObstacleHit.ImpactNormal.X, OutObstacleHit.ImpactNormal.Y, 0.f).GetSafeNormal();
	if (SurfaceNormal2D.IsNearlyZero() || FVector::DotProduct(Forward, -SurfaceNormal2D) < 0.25f)
	{
		SetFail(TEXT("Must face obstacle"));
		return false;
	}

	const FVector TopTraceStart(
		OutObstacleHit.ImpactPoint.X,
		OutObstacleHit.ImpactPoint.Y,
		FeetLocation.Z + MaxObstacleHeight + 80.f);
	const FVector TopTraceEnd(
		OutObstacleHit.ImpactPoint.X,
		OutObstacleHit.ImpactPoint.Y,
		FeetLocation.Z - 5.f);

	FHitResult TopHit;
	const bool bTopHit = GetWorld()->LineTraceSingleByObjectType(
		TopHit,
		TopTraceStart,
		TopTraceEnd,
		ObjQuery,
		Params);

	if (!bTopHit || !TopHit.bBlockingHit)
	{
		SetFail(TEXT("Obstacle height trace failed"));
		return false;
	}

	OutObstacleHeight = TopHit.ImpactPoint.Z - FeetLocation.Z;
	if (OutObstacleHeight < MinObstacleHeight)
	{
		SetFail(FString::Printf(TEXT("Too low (%.0f < %.0f)"), OutObstacleHeight, MinObstacleHeight));
		return false;
	}
	if (OutObstacleHeight > MaxObstacleHeight)
	{
		SetFail(FString::Printf(TEXT("Too high (%.0f > %.0f)"), OutObstacleHeight, MaxObstacleHeight));
		return false;
	}

	if (bRequireParkourZone && !IsParkourLocationAllowed(OutObstacleHit.ImpactPoint))
	{
		SetFail(TEXT("Obstacle outside parkour zone"));
		return false;
	}

	if (bDrawDebug)
	{
		DrawDebugLine(GetWorld(), TopTraceStart, TopTraceEnd, FColor::Cyan, false, 1.5f, 0, 1.5f);
		DrawDebugString(
			GetWorld(),
			TopHit.ImpactPoint,
			FString::Printf(TEXT("H:%.0f D:%.0f"), OutObstacleHeight, HitDistance),
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
	ASurvivorCharacter* Survivor = GetSurvivor();
	if (!Montage || !Survivor)
	{
		return;
	}

	USkeletalMeshComponent* MeshComp = Survivor->GetMesh();
	UAnimInstance* AnimInstance = MeshComp ? MeshComp->GetAnimInstance() : nullptr;
	if (!AnimInstance)
	{
		return;
	}

	UCharacterMovementComponent* MoveComp = Survivor->GetCharacterMovement();
	if (MoveComp)
	{
		CachedMovementMode = MoveComp->MovementMode;
		CachedGravityScale = MoveComp->GravityScale;
		MoveComp->StopMovementImmediately();
		MoveComp->Velocity = FVector::ZeroVector;
		MoveComp->GravityScale = 0.f;
		MoveComp->SetMovementMode(MOVE_None);
		MoveComp->bAllowPhysicsRotationDuringAnimRootMotion = true;
	}

	CachedRootMotionMode = AnimInstance->RootMotionMode;
	AnimInstance->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);

	ParkourStartTransform = Survivor->GetActorTransform();
	Survivor->SetActorRotation(FRotator(0.f, FacingRotation.Yaw, 0.f));
	ParkourStartTransform.SetRotation(Survivor->GetActorRotation().Quaternion());

	ParkourObstacleHeight = FMath::Clamp(ObstacleHeight, MinObstacleHeight, MaxObstacleHeight);
	if (const UCapsuleComponent* Capsule = Survivor->GetCapsuleComponent())
	{
		ParkourStartFeetZ = ParkourStartTransform.GetLocation().Z - Capsule->GetScaledCapsuleHalfHeight();
	}

	LogParkourDebug(
		FString::Printf(TEXT("Vault H:%.0f (ref %.0f)"), ParkourObstacleHeight, ParkourReferenceObstacleHeight),
		FColor::Silver);

	InitParkourVaultTargets(ObstacleActor, ObstacleImpactPoint);

	if (ObstacleActor)
	{
		CurrentParkourObstacle = ObstacleActor;
		SetParkourObstacleCollisionIgnored(true);
	}

	ActiveParkourMontage = Montage;
	bIsParkour = true;
	bParkourFeetOnGround = false;
	ParkourMontageElapsed = 0.f;

	if (MoveComp)
	{
		Survivor->PrimaryActorTick.AddPrerequisite(MoveComp, MoveComp->PrimaryComponentTick);
	}

	if (AController* Ctrl = Survivor->GetController())
	{
		Ctrl->SetIgnoreMoveInput(true);
	}

	ParkourMontageEndedDelegate.BindUObject(this, &USPParkourComponent::OnParkourMontageEnded);
	AnimInstance->Montage_SetEndDelegate(ParkourMontageEndedDelegate, Montage);

	const float Duration = AnimInstance->Montage_Play(Montage, 1.f);
	if (Duration <= 0.f)
	{
		LogParkourDebug(
			TEXT("Montage_Play 실패 — ABP Slot(DefaultSlot)과 몽타주 슬롯·AnimClass 확인"),
			FColor::Red);
		EndParkour(true);
		return;
	}

	ParkourMontageDuration = Duration;
	CacheParkourLandNotifyTiming(Montage);

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
		const ASurvivorCharacter* Survivor = GetSurvivor();
		const UCapsuleComponent* Capsule = Survivor ? Survivor->GetCapsuleComponent() : nullptr;
		if (Capsule)
		{
			const FVector Forward = ParkourStartTransform.GetRotation().GetForwardVector().GetSafeNormal2D();
			const FVector StartCenter = ParkourStartTransform.GetLocation();
			const float Radius = Capsule->GetScaledCapsuleRadius();
			const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();

			float ObstacleDepth = 35.f;
			if (ObstacleActor)
			{
				FVector Origin;
				FVector Extent;
				ObstacleActor->GetActorBounds(true, Origin, Extent);
				ObstacleDepth = FMath::Abs(Forward.X) * Extent.X * 2.f + FMath::Abs(Forward.Y) * Extent.Y * 2.f;
				ObstacleDepth = FMath::Max(ObstacleDepth, 25.f);
			}

			const float DistToWallFace = FMath::Max(
				FVector::DotProduct(ObstacleImpactPoint - StartCenter, Forward),
				0.f);
			const float FallbackTravel = FMath::Min(
				DistToWallFace + ObstacleDepth + Radius + ParkourMinLandingPastWallOffset,
				ParkourMaxVaultTravel);

			ParkourVaultEndLocation = StartCenter + Forward * FallbackTravel;
			ParkourVaultPeakCenterZ = ParkourStartFeetZ + ParkourObstacleHeight + HalfHeight + ParkourClearanceOverObstacle;
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
				: TEXT("Vault targets invalid — using start as landing"),
			FColor::Orange);

		if (!bParkourVaultEndValid)
		{
			ParkourVaultEndLocation = ParkourStartTransform.GetLocation();
			ParkourVaultPeakCenterZ = ParkourStartTransform.GetLocation().Z + ParkourObstacleHeight;
			ParkourWallClearForward = 0.f;
			ParkourWallClearMontageAlpha = 0.45f;
			bParkourVaultEndValid = true;
		}
	}

	LogParkourDebug(
		FString::Printf(
			TEXT("Vault path end %s peakZ %.0f peak@%.2f land@%.2f"),
			*ParkourVaultEndLocation.ToCompactString(),
			ParkourVaultPeakCenterZ,
			ParkourVaultPeakMontageAlpha,
			ParkourLandByMontageAlpha),
		FColor::Silver);
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

	const ASurvivorCharacter* Survivor = GetSurvivor();
	const UCapsuleComponent* Capsule = Survivor ? Survivor->GetCapsuleComponent() : nullptr;
	if (!Capsule || !GetWorld())
	{
		SetFail(TEXT("Capsule/World missing"));
		return false;
	}

	const FVector Forward = StartTransform.GetRotation().GetForwardVector().GetSafeNormal2D();
	const FVector StartCenter = StartTransform.GetLocation();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const float Radius = Capsule->GetScaledCapsuleRadius();

	float ObstacleDepth = 35.f;
	if (ObstacleActor)
	{
		FVector Origin;
		FVector Extent;
		ObstacleActor->GetActorBounds(true, Origin, Extent);
		ObstacleDepth = FMath::Abs(Forward.X) * Extent.X * 2.f + FMath::Abs(Forward.Y) * Extent.Y * 2.f;
		ObstacleDepth = FMath::Max(ObstacleDepth, 25.f);
	}

	const float DistToWallFace = FMath::Max(
		FVector::DotProduct(ObstacleImpactPoint - StartCenter, Forward),
		0.f);
	const float MinTravelPastWall = DistToWallFace + ObstacleDepth + Radius + ParkourMinLandingPastWallOffset;
	float TravelPastWall = DistToWallFace + ObstacleDepth + Radius + ParkourLandingForwardOffset;
	TravelPastWall = FMath::Clamp(TravelPastWall, MinTravelPastWall, ParkourMaxVaultTravel);

	if (MinTravelPastWall > ParkourMaxVaultTravel + 1.f)
	{
		SetFail(FString::Printf(
			TEXT("Need %.0fcm travel but max is %.0f"),
			MinTravelPastWall,
			ParkourMaxVaultTravel));
		return false;
	}

	const float ObstacleTopZ = StartFeetZ + ObstacleHeight;
	OutPeakCenterZ = ObstacleTopZ + HalfHeight + ParkourClearanceOverObstacle;

	OutEndLocation = StartCenter + Forward * TravelPastWall;
	OutEndLocation.Z = StartCenter.Z;

	FHitResult PathHit;
	if (SweepParkourCapsulePath(StartCenter, OutEndLocation, Forward, PathHit, ObstacleActor))
	{
		const FVector HitNormal2D = FVector(PathHit.ImpactNormal.X, PathHit.ImpactNormal.Y, 0.f).GetSafeNormal();
		const bool bWallLikeBlock = !HitNormal2D.IsNearlyZero()
			&& FVector::DotProduct(Forward, -HitNormal2D) > 0.35f;

		if (bWallLikeBlock)
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
		SetFail(TEXT("No ground at vault landing"));
		return false;
	}
	OutEndLocation = GroundSample;

	if (!ValidateParkourLandingLocation(StartTransform, StartFeetZ, OutEndLocation, OutFailReason))
	{
		return false;
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
	AActor* IgnoredObstacle) const
{
	const ASurvivorCharacter* Survivor = GetSurvivor();
	const UCapsuleComponent* Capsule = Survivor ? Survivor->GetCapsuleComponent() : nullptr;
	if (!Capsule || !GetWorld())
	{
		return false;
	}

	const FVector Delta = End - Start;
	if (Delta.SizeSquared2D() < FMath::Square(5.f))
	{
		return false;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ParkourVaultSweep), false, Survivor);
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
		Start,
		End,
		FQuat::Identity,
		ObjQuery,
		SweepShape,
		Params);

	(void)Forward;

	if (bDrawDebug)
	{
		DrawDebugCapsule(
			GetWorld(),
			Start,
			Capsule->GetScaledCapsuleHalfHeight(),
			Capsule->GetScaledCapsuleRadius(),
			FQuat::Identity,
			FColor::Cyan,
			false,
			1.5f,
			0,
			1.f);
		DrawDebugLine(GetWorld(), Start, End, bHit ? FColor::Orange : FColor::Green, false, 1.5f, 0, 2.f);
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

	const ASurvivorCharacter* Survivor = GetSurvivor();
	const UCapsuleComponent* Capsule = Survivor ? Survivor->GetCapsuleComponent() : nullptr;
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
	const ASurvivorCharacter* Survivor = GetSurvivor();
	const UCapsuleComponent* Capsule = Survivor ? Survivor->GetCapsuleComponent() : nullptr;
	if (!Survivor || !Capsule)
	{
		if (OutFailReason)
		{
			*OutFailReason = TEXT("Survivor/Capsule missing");
		}
		return false;
	}

	FTransform PreviewStart = Survivor->GetActorTransform();
	PreviewStart.SetRotation(FRotator(0.f, FacingRotation.Yaw, 0.f).Quaternion());

	const float PreviewFeetZ = PreviewStart.GetLocation().Z - Capsule->GetScaledCapsuleHalfHeight();
	const float ClampedHeight = FMath::Clamp(ObstacleHeight, MinObstacleHeight, MaxObstacleHeight);

	const float PreviewPeakZ = PreviewFeetZ + ClampedHeight + Capsule->GetScaledCapsuleHalfHeight() + ParkourClearanceOverObstacle;

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

	float ObstacleDepth = 35.f;
	if (ObstacleActor)
	{
		FVector Origin;
		FVector Extent;
		ObstacleActor->GetActorBounds(true, Origin, Extent);
		ObstacleDepth = FMath::Abs(Forward.X) * Extent.X * 2.f + FMath::Abs(Forward.Y) * Extent.Y * 2.f;
		ObstacleDepth = FMath::Max(ObstacleDepth, 25.f);
	}

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

	FVector Location;
	Location.X = FMath::Lerp(Start.X, End.X, ForwardAlpha);
	Location.Y = FMath::Lerp(Start.Y, End.Y, ForwardAlpha);

	const float ArcHeight = FMath::Max(0.f, ParkourVaultPeakCenterZ - FMath::Max(Start.Z, End.Z));

	float GroundZ = End.Z;
	FVector GroundSample = Location;
	if (SampleParkourGroundCenterZ(GroundSample))
	{
		GroundZ = GroundSample.Z;
	}

	const float TravelT = FMath::SmoothStep(0.f, 1.f, Alpha);
	const float BaseZ = FMath::Lerp(Start.Z, GroundZ, TravelT);

	if (bParkourFeetOnGround)
	{
		Location.Z = GroundZ;
		return Location;
	}

	const float ArcFactor = GetParkourArcFactor(Alpha);

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

	if (!bParkourFeetOnGround && ParkourLandNotifyMontageAlpha > 0.f)
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
	const ASurvivorCharacter* Survivor = GetSurvivor();
	const UCapsuleComponent* Capsule = Survivor ? Survivor->GetCapsuleComponent() : nullptr;
	if (!Capsule || !GetWorld())
	{
		return false;
	}

	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const float PeakZ = PeakCenterZOverride >= 0.f ? PeakCenterZOverride : ParkourVaultPeakCenterZ;
	const float TraceHighZ = FMath::Max(InOutCenterLocation.Z, PeakZ) + HalfHeight + 120.f;
	const FVector TraceStart(InOutCenterLocation.X, InOutCenterLocation.Y, TraceHighZ);
	const FVector TraceEnd(InOutCenterLocation.X, InOutCenterLocation.Y, InOutCenterLocation.Z - 400.f);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ParkourLandSample), false, Survivor);
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
	if (GetWorld()->LineTraceSingleByChannel(
		GroundHit,
		TraceStart,
		TraceEnd,
		ECC_Visibility,
		Params))
	{
		InOutCenterLocation.Z = GroundHit.ImpactPoint.Z + HalfHeight;
		return true;
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

	const ASurvivorCharacter* Survivor = GetSurvivor();
	const UCapsuleComponent* Capsule = Survivor ? Survivor->GetCapsuleComponent() : nullptr;
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

	const float ReferenceHeight = FMath::Max(ParkourReferenceObstacleHeight, 1.f);
	if (ParkourObstacleHeight > ReferenceHeight)
	{
		const float HeightScale = ParkourObstacleHeight / ReferenceHeight;
		const float ScaledLiftZ = AnimLiftZ * HeightScale;
		const float ScaledZ = StartCenterZ + ScaledLiftZ + (RequiredLiftZ - ScaledLiftZ) * VaultCurve;
		Location.Z = FMath::Max(Location.Z, ScaledZ);
	}

	return Location;
}

void USPParkourComponent::OnParkourLandNotify()
{
	if (!bIsParkour)
	{
		return;
	}

	ASurvivorCharacter* Survivor = GetSurvivor();
	if (!Survivor)
	{
		return;
	}

	bParkourFeetOnGround = true;

	FVector GroundedLocation = Survivor->GetActorLocation();
	if (SnapParkourLocationToGround(GroundedLocation))
	{
		if (UCharacterMovementComponent* MoveComp = Survivor->GetCharacterMovement())
		{
			MoveComp->StopMovementImmediately();
			MoveComp->Velocity = FVector::ZeroVector;
		}

		Survivor->TeleportTo(GroundedLocation, Survivor->GetActorRotation(), false, true);

		if (UCharacterMovementComponent* MoveComp = Survivor->GetCharacterMovement())
		{
			MoveComp->bJustTeleported = true;
		}
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

	for (const TWeakObjectPtr<ASPParkourZone>& ZonePtr : OverlappingParkourZones)
	{
		if (ZonePtr.IsValid())
		{
			return true;
		}
	}

	return false;
}

bool USPParkourComponent::IsParkourLocationAllowed(const FVector& WorldLocation) const
{
	if (!bRequireParkourZone)
	{
		return true;
	}

	for (const TWeakObjectPtr<ASPParkourZone>& ZonePtr : OverlappingParkourZones)
	{
		if (ZonePtr.IsValid() && ZonePtr->ContainsPoint(WorldLocation))
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

	ASurvivorCharacter* Survivor = GetSurvivor();
	USkeletalMeshComponent* MeshComp = Survivor ? Survivor->GetMesh() : nullptr;
	UAnimInstance* AnimInstance = MeshComp ? MeshComp->GetAnimInstance() : nullptr;
	UCharacterMovementComponent* MoveComp = Survivor ? Survivor->GetCharacterMovement() : nullptr;
	if (!Survivor || !AnimInstance || !MoveComp)
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

	const FVector CurrentLocation = Survivor->GetActorLocation();
	FVector NewLocation = TargetLocation;
	NewLocation.X = FMath::FInterpTo(CurrentLocation.X, TargetLocation.X, DeltaTime, 20.f);
	NewLocation.Y = FMath::FInterpTo(CurrentLocation.Y, TargetLocation.Y, DeltaTime, 20.f);

	const FRotator BlendedRotation = FMath::RInterpTo(
		Survivor->GetActorRotation(),
		TargetRotation,
		DeltaTime,
		14.f);

	if (!NewLocation.Equals(CurrentLocation, 0.25f) || !Survivor->GetActorRotation().Equals(BlendedRotation, 0.5f))
	{
		Survivor->TeleportTo(NewLocation, BlendedRotation, false, true);
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
	float ElapsedAlpha = 1.f;
	if (ParkourMontageDuration > KINDA_SMALL_NUMBER)
	{
		ElapsedAlpha = FMath::Clamp(ParkourMontageElapsed / ParkourMontageDuration, 0.f, 1.f);
	}

	float PositionAlpha = ElapsedAlpha;
	if (ActiveParkourMontage)
	{
		if (const ASurvivorCharacter* Survivor = GetSurvivor())
		{
			if (const USkeletalMeshComponent* MeshComp = Survivor->GetMesh())
			{
				if (const UAnimInstance* AnimInstance = MeshComp->GetAnimInstance())
				{
					const float MontageLength = ActiveParkourMontage->GetPlayLength();
					if (MontageLength > KINDA_SMALL_NUMBER)
					{
						PositionAlpha = FMath::Clamp(
							AnimInstance->Montage_GetPosition(ActiveParkourMontage) / MontageLength,
							0.f,
							1.f);
					}
				}
			}
		}
	}

	return FMath::Max(ElapsedAlpha, PositionAlpha);
}

bool USPParkourComponent::HasSufficientVaultForwardProgress(const FVector& Location) const
{
	const FVector Forward = ParkourStartTransform.GetRotation().GetForwardVector().GetSafeNormal2D();
	const FVector StartCenter = ParkourStartTransform.GetLocation();
	const float PlannedTravel = FVector::DotProduct(ParkourVaultEndLocation - StartCenter, Forward);
	const float ActualTravel = FVector::DotProduct(Location - StartCenter, Forward);
	if (PlannedTravel <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	if (FVector::Dist2D(Location, ParkourVaultEndLocation) <= 35.f)
	{
		return true;
	}

	return ActualTravel >= PlannedTravel * 0.85f;
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

	const ASurvivorCharacter* Survivor = GetSurvivor();
	if (Survivor)
	{
		OutLocation = Survivor->GetActorLocation();
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
		else if (bDrawDebug)
		{
			const FVector Forward = ParkourStartTransform.GetRotation().GetForwardVector().GetSafeNormal2D();
			const float ActualTravel = FVector::DotProduct(OutLocation - ParkourStartTransform.GetLocation(), Forward);
			LogParkourDebug(
				FString::Printf(TEXT("Tracked landing insufficient (%.0fcm) — using vault end"), ActualTravel),
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

void USPParkourComponent::SetParkourObstacleCollisionIgnored(bool bIgnore)
{
	AActor* ObstacleActor = CurrentParkourObstacle.Get();
	ASurvivorCharacter* Survivor = GetSurvivor();
	if (!ObstacleActor || !Survivor)
	{
		return;
	}

	UCapsuleComponent* Capsule = Survivor->GetCapsuleComponent();
	USkeletalMeshComponent* MeshComp = Survivor->GetMesh();

	TArray<UPrimitiveComponent*> CharacterPrimitives;
	if (Capsule)
	{
		CharacterPrimitives.Add(Capsule);
	}
	if (MeshComp && MeshComp->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
	{
		CharacterPrimitives.Add(MeshComp);
	}

	TArray<UPrimitiveComponent*> ObstaclePrimitives;
	ObstacleActor->GetComponents<UPrimitiveComponent>(ObstaclePrimitives);

	for (UPrimitiveComponent* CharacterPrim : CharacterPrimitives)
	{
		for (UPrimitiveComponent* ObstaclePrim : ObstaclePrimitives)
		{
			if (CharacterPrim && ObstaclePrim)
			{
				CharacterPrim->IgnoreComponentWhenMoving(ObstaclePrim, bIgnore);
				ObstaclePrim->IgnoreComponentWhenMoving(CharacterPrim, bIgnore);
			}
		}

		if (CharacterPrim)
		{
			CharacterPrim->IgnoreActorWhenMoving(ObstacleActor, bIgnore);
		}
	}

	if (bIgnore)
	{
		LogParkourDebug(FString::Printf(TEXT("Ignore collision: %s"), *ObstacleActor->GetName()), FColor::Silver);
	}
}

void USPParkourComponent::ApplyParkourLanding(const FVector& WorldLocation, const FRotator& WorldRotation)
{
	ASurvivorCharacter* Survivor = GetSurvivor();
	if (!Survivor)
	{
		return;
	}

	UCharacterMovementComponent* MoveComp = Survivor->GetCharacterMovement();
	if (MoveComp)
	{
		MoveComp->StopMovementImmediately();
		MoveComp->Velocity = FVector::ZeroVector;
		MoveComp->ClearAccumulatedForces();
	}

	FVector ResolvedLocation = WorldLocation;
	SnapParkourLocationToGround(ResolvedLocation);

	const FVector CurrentLocation = Survivor->GetActorLocation();
	const bool bAlreadyAligned =
		FVector::Dist2D(CurrentLocation, ResolvedLocation) <= 12.f &&
		FMath::Abs(CurrentLocation.Z - ResolvedLocation.Z) <= 5.f;

	if (!bAlreadyAligned)
	{
		Survivor->TeleportTo(ResolvedLocation, WorldRotation, false, true);
	}
	else if (!Survivor->GetActorRotation().Equals(WorldRotation, 1.f))
	{
		Survivor->SetActorRotation(WorldRotation);
	}
	else if (FMath::Abs(CurrentLocation.Z - ResolvedLocation.Z) > 1.f)
	{
		FVector AdjustedLocation = CurrentLocation;
		AdjustedLocation.Z = ResolvedLocation.Z;
		Survivor->SetActorLocation(AdjustedLocation, false, nullptr, ETeleportType::TeleportPhysics);
	}

	if (MoveComp)
	{
		MoveComp->SetMovementMode(MOVE_Walking);
		MoveComp->bJustTeleported = true;
		MoveComp->bForceNextFloorCheck = true;

		if (UCapsuleComponent* Capsule = Survivor->GetCapsuleComponent())
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

	ASurvivorCharacter* Survivor = GetSurvivor();

	if (!bInterrupted && bParkourVaultEndValid && Survivor)
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
		LandingLocation = ParkourStartTransform.GetLocation();
		LandingRotation = ParkourStartTransform.Rotator();
		bHasLanding = true;
		LogParkourDebug(TEXT("Invalid landing — snapped back to start"), FColor::Orange);
	}

	if (Survivor)
	{
		if (USkeletalMeshComponent* MeshComp = Survivor->GetMesh())
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

	if (Survivor)
	{
		if (USkeletalMeshComponent* MeshComp = Survivor->GetMesh())
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

	if (CurrentParkourObstacle.IsValid())
	{
		SetParkourObstacleCollisionIgnored(false);
	}

	if (Survivor)
	{
		if (UCharacterMovementComponent* MoveComp = Survivor->GetCharacterMovement())
		{
			Survivor->PrimaryActorTick.RemovePrerequisite(MoveComp, MoveComp->PrimaryComponentTick);
			MoveComp->bAllowPhysicsRotationDuringAnimRootMotion = false;
			MoveComp->GravityScale = CachedGravityScale;
			if (CachedMovementMode != MOVE_None)
			{
				MoveComp->SetMovementMode(CachedMovementMode);
			}
		}
	}

	CurrentParkourObstacle = nullptr;
	ActiveParkourMontage = nullptr;
	ParkourObstacleHeight = 0.f;
	ParkourStartFeetZ = 0.f;
	ParkourVaultEndLocation = FVector::ZeroVector;
	ParkourVaultPeakCenterZ = 0.f;
	ParkourWallClearForward = 0.f;
	ParkourWallClearMontageAlpha = 0.f;
	bParkourVaultEndValid = false;
	bParkourFeetOnGround = false;
	ParkourLandNotifyMontageAlpha = -1.f;
	ParkourMontageElapsed = 0.f;
	ParkourMontageDuration = 0.f;

	if (Survivor)
	{
		if (AController* Ctrl = Survivor->GetController())
		{
			Ctrl->ResetIgnoreMoveInput();
		}

		Survivor->NotifyParkourEnded();
	}

	LogParkourDebug(bInterrupted ? TEXT("Parkour ended (interrupted)") : TEXT("Parkour ended"), FColor::Cyan);
}

void USPParkourComponent::LogParkourDebug(const FString& Message, FColor Color) const
{
	if (!bDrawDebug)
	{
		return;
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.f, Color, FString::Printf(TEXT("[Parkour] %s"), *Message));
	}

	UE_LOG(LogTemp, Log, TEXT("[Parkour] %s"), *Message);
}

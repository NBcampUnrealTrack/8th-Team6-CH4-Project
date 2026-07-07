#include "Characters/Survivor/SurvivorCharacter.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Components/SPInteractionComponent.h"
#include "Components/SPMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Data/SPInputConfigData.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "EnhancedInputComponent.h"
#include "Gameplay/Collectibles/SPCollectibleItem.h"
#include "InputAction.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "Inventory/SPInventoryComponent.h"
#include "Net/UnrealNetwork.h"
#include "Systems/MatchGameMode.h"
#include "TimerManager.h"
#include "Type/SPGameplayTag.h"
#include "UI/GameHUD.h"
#include "UObject/ConstructorHelpers.h"

namespace SurvivorParkour
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


#include "Components/SPInteractionComponent.h"
#include "Components/SPMovementComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "Inventory/SPInventoryComponent.h"
#include "Net/UnrealNetwork.h"
#include "Systems/MatchGameMode.h"
#include "Type/SPGameplayTag.h"
#include "UI/GameHUD.h"

ASurvivorCharacter::ASurvivorCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;

	InteractionComponent = CreateDefaultSubobject<USPInteractionComponent>("InteractionComponent");
	MovementComponent = CreateDefaultSubobject<USPMovementComponent>("MovementComponent");
	InventoryComponent = CreateDefaultSubobject<USPInventoryComponent>(TEXT("InventoryComponent"));

	OwningTag.AddTag(SPGameplayTags::Character::Survivor);

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->bOrientRotationToMovement = true;
		MoveComp->RotationRate = FRotator(0.f, 540.f, 0.f);
	}

	static ConstructorHelpers::FObjectFinder<UAnimMontage> HurdleMontageFinder(SurvivorParkour::HurdleMontagePath);
	if (HurdleMontageFinder.Succeeded())
	{
		ParkourMontage = HurdleMontageFinder.Object;
	}
}

void ASurvivorCharacter::GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const
{
	TagContainer.AppendTags(OwningTag);
}

void ASurvivorCharacter::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASurvivorCharacter, SurvivorState);
	DOREPLIFETIME(ASurvivorCharacter, bIsParkour);
}

void ASurvivorCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bIsParkour && ActiveParkourMontage)
	{
		UpdateParkourRootMotion();

		UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
		if (!AnimInstance || !AnimInstance->Montage_IsPlaying(ActiveParkourMontage))
		{
			EndParkour(false);
		}
	}
}

void ASurvivorCharacter::Move(const FInputActionValue& Value)
{
	if (bIsParkour)
	{
		return;
	}

	if (InteractionComponent)
	{
		InteractionComponent->NotifyMoveInput();
	}

	Super::Move(Value);
}

void ASurvivorCharacter::Interact()
{
	Super::Interact();

	if (CanInteract() && InteractionComponent)
	{
		InteractionComponent->RequestInteract();
	}
}

void ASurvivorCharacter::JumpOver()
{
	Super::JumpOver();

	LogParkourDebug(TEXT("JumpOver 입력"), FColor::Cyan);

	if (!CanJumpOver())
	{
		LogParkourDebug(TEXT("CanJumpOver = false"), FColor::Red);
		return;
	}

	if (HasAuthority())
	{
		Server_JumpOver_Implementation();
	}
	else
	{
		Server_JumpOver();
	}
}

void ASurvivorCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority() && InventoryComponent)
	{
		InventoryComponent->InitializeDefaultLoadout();
	}

	BindInventoryHudRefresh();
	ApplyStateEffects();
	EnsureParkourMontagesLoaded();

	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		if (UAnimInstance* AnimInstance = MeshComp->GetAnimInstance())
		{
			AnimInstance->SetRootMotionMode(ERootMotionMode::RootMotionFromMontagesOnly);
			CachedRootMotionMode = ERootMotionMode::RootMotionFromMontagesOnly;
		}
	}
}

void ASurvivorCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	EnsureInputConfig();

	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		UInputAction* JumpOverAction = InputConfig ? InputConfig->JumpOverAction.Get() : nullptr;
		if (!JumpOverAction)
		{
			JumpOverAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/InputAction/IA_JumpOver.IA_JumpOver"));
		}

		if (JumpOverAction)
		{
			EnhancedInput->BindAction(JumpOverAction, ETriggerEvent::Started, this, &ASurvivorCharacter::JumpOver);
		}
	}
}

void ASurvivorCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	BindInventoryHudRefresh();
	RefreshLocalInventoryHud();
}

void ASurvivorCharacter::BindInventoryHudRefresh()
{
	if (!IsLocallyControlled() || !InventoryComponent)
	{
		return;
	}

	InventoryComponent->OnInventoryChanged.RemoveDynamic(this, &ASurvivorCharacter::HandleInventoryChanged);
	InventoryComponent->OnInventoryChanged.AddDynamic(this, &ASurvivorCharacter::HandleInventoryChanged);
}

void ASurvivorCharacter::HandleInventoryChanged()
{
	RefreshLocalInventoryHud();
}

void ASurvivorCharacter::RefreshLocalInventoryHud() const
{
	if (!IsLocallyControlled())
	{
		return;
	}

	const APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (!PlayerController)
	{
		return;
	}

	if (AGameHUD* GameHUD = Cast<AGameHUD>(PlayerController->GetHUD()))
	{
		GameHUD->RefreshInventoryPanels();
	}
}

bool ASurvivorCharacter::TryAcquireConsumable(const EConsumableItemType ItemType)
{
	return InventoryComponent && InventoryComponent->TryAddConsumable(ItemType);
}

void ASurvivorCharacter::SetSurvivorState(ESurvivorState NewState)
{
	if (!HasAuthority() || SurvivorState == NewState) return;

	const ESurvivorState OldState = SurvivorState;
	SurvivorState = NewState;

	if (MovementComponent)
	{
		MovementComponent->HandleStateTransition(OldState, NewState);
	}

	if (InteractionComponent)
	{
		InteractionComponent->CancelInteract();
	}

	ApplyStateEffects();
	NotifyMatchStateChange(NewState);
}

bool ASurvivorCharacter::CanMove() const
{
	if (bIsParkour)
	{
		return false;
	}

	return SurvivorState == ESurvivorState::Healthy || SurvivorState == ESurvivorState::Injured || SurvivorState == ESurvivorState::Downed;
}

bool ASurvivorCharacter::CanInteract() const
{
	return SurvivorState == ESurvivorState::Healthy || SurvivorState == ESurvivorState::Injured;
}

bool ASurvivorCharacter::CanJumpOver() const
{
	if (bIsParkour)
	{
		return false;
	}

	// 怨듭쨷?먯꽌??諛붾줈 ?섏? 紐삵븯?꾨줉
	if (const UCharacterMovementComponent* CMC = GetCharacterMovement())
	{
		if (CMC->IsFalling())
		{
			return false;
		}
	}

	// TODO: ?대컲 以묒씪 ?뚮룄 ?섏뼱媛????덉쓣吏?
	return SurvivorState == ESurvivorState::Healthy || SurvivorState == ESurvivorState::Injured;
}

void ASurvivorCharacter::LogParkourDebug(const FString& Message, FColor Color) const
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

void ASurvivorCharacter::EnsureParkourMontagesLoaded()
{
	if (!ParkourMontage)
	{
		ParkourMontage = LoadObject<UAnimMontage>(nullptr, SurvivorParkour::HurdleMontagePath);
	}
}

bool ASurvivorCharacter::Server_JumpOver_Validate()
{
	return true;
}

void ASurvivorCharacter::Server_JumpOver_Implementation()
{
	EnsureParkourMontagesLoaded();

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
		LogParkourDebug(TEXT("ParkourMontage null ??Hurdle 紐쏀?二?寃쎈줈 ?뺤씤"), FColor::Red);
		return;
	}

	const FVector ToObstacle = ObstacleHit.ImpactPoint - GetActorLocation();
	const FRotator FacingRotation = FRotationMatrix::MakeFromX(ToObstacle.GetSafeNormal2D()).Rotator();
	LogParkourDebug(
		FString::Printf(TEXT("Start H:%.0f D:%.0f -> %s"), ObstacleHeight, FVector::Dist2D(GetActorLocation(), ObstacleHit.ImpactPoint), *Montage->GetName()),
		FColor::Green);

	Multicast_PlayParkourMontage(Montage, ObstacleHit.GetActor(), FacingRotation, ObstacleHeight, ObstacleHit.ImpactPoint);
}

void ASurvivorCharacter::Multicast_PlayParkourMontage_Implementation(
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

void ASurvivorCharacter::GetParkourFacing(FVector& OutForward, FVector& OutRight) const
{
	OutForward = GetActorForwardVector().GetSafeNormal2D();
	OutRight = GetActorRightVector().GetSafeNormal2D();

	if (const UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		const FVector Velocity2D = FVector(MoveComp->Velocity.X, MoveComp->Velocity.Y, 0.f);
		if (!Velocity2D.IsNearlyZero(25.f))
		{
			OutForward = Velocity2D.GetSafeNormal();
			OutRight = FVector::CrossProduct(FVector::UpVector, OutForward).GetSafeNormal();
		}
	}
}

bool ASurvivorCharacter::TraceParkourObstacle(FHitResult& OutObstacleHit, float& OutObstacleHeight, FString* OutFailReason) const
{
	auto SetFail = [OutFailReason](const FString& Reason)
	{
		if (OutFailReason)
		{
			*OutFailReason = Reason;
		}
	};

	OutObstacleHeight = 0.f;

	const UCapsuleComponent* Capsule = GetCapsuleComponent();
	if (!Capsule || !GetWorld())
	{
		SetFail(TEXT("Capsule/World missing"));
		return false;
	}

	FVector Forward;
	FVector Right;
	GetParkourFacing(Forward, Right);
	(void)Right;

	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const FVector FeetLocation = GetActorLocation() - FVector(0.f, 0.f, HalfHeight);
	const FVector TraceStart = FeetLocation + FVector(0.f, 0.f, HalfHeight * 0.6f);
	const FVector TraceEnd = TraceStart + Forward * ParkourTraceDistance;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ParkourTrace), false, this);
	if (ASPCollectibleItem* CarriedItem = InteractionComponent ? InteractionComponent->GetCarriedItem() : nullptr)
	{
		Params.AddIgnoredActor(CarriedItem);
	}

	const FCollisionObjectQueryParams ObjQuery = SurvivorParkour::MakeObstacleObjectQuery();
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

	// ?μ븷臾?Impact XY 湲곕뫁?먯꽌 ?꾟넂?꾨옒濡??대젮 李띿뼱 ?곷떒 ?믪씠瑜?援ы븳??
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

void ASurvivorCharacter::PlayParkourMontage(
	UAnimMontage* Montage,
	AActor* ObstacleActor,
	const FRotator& FacingRotation,
	float ObstacleHeight,
	const FVector& ObstacleImpactPoint)
{
	if (!Montage)
	{
		return;
	}

	USkeletalMeshComponent* MeshComp = GetMesh();
	UAnimInstance* AnimInstance = MeshComp ? MeshComp->GetAnimInstance() : nullptr;
	if (!AnimInstance)
	{
		return;
	}

	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
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

	if (AnimInstance)
	{
		CachedRootMotionMode = AnimInstance->RootMotionMode;
		// Motion Matching ABP? 異⑸룎 諛⑹? ??猷⑦듃 紐⑥뀡? Tick?먯꽌 ?쒗??異붿텧濡?吏곸젒 ?곸슜.
		AnimInstance->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
	}

	ParkourStartTransform = GetActorTransform();
	SetActorRotation(FRotator(0.f, FacingRotation.Yaw, 0.f));
	ParkourStartTransform.SetRotation(GetActorRotation().Quaternion());

	ParkourObstacleHeight = FMath::Clamp(ObstacleHeight, MinObstacleHeight, MaxObstacleHeight);
	if (const UCapsuleComponent* Capsule = GetCapsuleComponent())
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

	if (MoveComp)
	{
		PrimaryActorTick.AddPrerequisite(MoveComp, MoveComp->PrimaryComponentTick);
	}

	if (AController* Ctrl = GetController())
	{
		Ctrl->SetIgnoreMoveInput(true);
	}

	ParkourMontageEndedDelegate.BindUObject(this, &ASurvivorCharacter::OnParkourMontageEnded);
	AnimInstance->Montage_SetEndDelegate(ParkourMontageEndedDelegate, Montage);

	const float Duration = AnimInstance->Montage_Play(Montage, 1.f);
	if (Duration <= 0.f)
	{
		LogParkourDebug(
			TEXT("Montage_Play ?ㅽ뙣 ??ABP Slot(DefaultSlot)怨?紐쏀?二??щ’쨌AnimClass ?뺤씤"),
			FColor::Red);
		EndParkour(true);
		return;
	}

	GetWorldTimerManager().ClearTimer(ParkourEndTimerHandle);
	GetWorldTimerManager().SetTimer(
		ParkourEndTimerHandle,
		this,
		&ASurvivorCharacter::OnParkourEndTimer,
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

void ASurvivorCharacter::InitParkourVaultTargets(AActor* ObstacleActor, const FVector& ObstacleImpactPoint)
{
	const UCapsuleComponent* Capsule = GetCapsuleComponent();
	if (!Capsule)
	{
		return;
	}

	const FVector Forward = ParkourStartTransform.GetRotation().GetForwardVector().GetSafeNormal2D();
	const FVector StartCenter = ParkourStartTransform.GetLocation();
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
	const float TravelPastWall = DistToWallFace + ObstacleDepth + Radius + ParkourLandingForwardOffset;
	ParkourWallClearForward = DistToWallFace + ObstacleDepth + Radius;

	ParkourVaultEndLocation = StartCenter + Forward * TravelPastWall;
	ParkourVaultEndLocation.Z = StartCenter.Z;
	SnapParkourLocationToGround(ParkourVaultEndLocation);

	const float ObstacleTopZ = ParkourStartFeetZ + ParkourObstacleHeight;
	ParkourVaultPeakCenterZ = ObstacleTopZ + HalfHeight + ParkourClearanceOverObstacle;

	const float TotalForwardDist = FVector::DotProduct(ParkourVaultEndLocation - StartCenter, Forward);
	if (TotalForwardDist > KINDA_SMALL_NUMBER)
	{
		const float ClearForwardAlpha = FMath::Clamp(ParkourWallClearForward / TotalForwardDist, 0.f, 1.f);
		ParkourWallClearMontageAlpha = 1.f - FMath::Pow(1.f - ClearForwardAlpha, 1.f / 1.85f);
	}
	else
	{
		ParkourWallClearMontageAlpha = 0.45f;
	}

	LogParkourDebug(
		FString::Printf(
			TEXT("Vault path end %s peakZ %.0f clear@%.2f land@%.2f"),
			*ParkourVaultEndLocation.ToCompactString(),
			ParkourVaultPeakCenterZ,
			ParkourWallClearMontageAlpha,
			ParkourLandByMontageAlpha),
		FColor::Silver);
}

FVector ASurvivorCharacter::BuildParkourVaultLocation(float MontageAlpha) const
{
	const float Alpha = FMath::Clamp(MontageAlpha, 0.f, 1.f);

	// 踰쎌쓣 癒쇱? ?섍릿 ??李⑹??섎룄濡??꾨갑 吏꾪뻾???욌떦源
	const float ForwardAlpha = 1.f - FMath::Pow(1.f - Alpha, 1.85f);

	const FVector Start = ParkourStartTransform.GetLocation();
	const FVector End = ParkourVaultEndLocation;

	FVector Location;
	Location.X = FMath::Lerp(Start.X, End.X, ForwardAlpha);
	Location.Y = FMath::Lerp(Start.Y, End.Y, ForwardAlpha);

	const float ArcHeight = FMath::Max(0.f, ParkourVaultPeakCenterZ - FMath::Max(Start.Z, End.Z));
	constexpr float PeakAt = 0.4f;
	const float PeakZ = Start.Z + ArcHeight;
	const float LandByAlpha = FMath::Clamp(ParkourLandByMontageAlpha, PeakAt + 0.05f, 0.95f);
	const float ClearAlpha = FMath::Clamp(ParkourWallClearMontageAlpha, PeakAt, LandByAlpha - 0.01f);

	if (Alpha >= LandByAlpha)
	{
		// ?쇱뼱?쒓린 援ш컙 ??吏硫댁뿉 怨좎젙
		Location.Z = End.Z;
	}
	else if (Alpha <= PeakAt)
	{
		const float RiseAlpha = Alpha / PeakAt;
		Location.Z = Start.Z + ArcHeight * FMath::Sin(RiseAlpha * UE_PI * 0.5f);
	}
	else if (Alpha < ClearAlpha)
	{
		// 踰??섍린 ???뺤젏 ?좎?
		Location.Z = PeakZ;
	}
	else
	{
		// 踰??섏? ??鍮좊Ⅴ寃?李⑹? (?쇱뼱?쒓린 ?꾩뿉 吏硫??꾨떖)
		const float DescentSpan = FMath::Max(LandByAlpha - ClearAlpha, KINDA_SMALL_NUMBER);
		const float DescentT = (Alpha - ClearAlpha) / DescentSpan;
		Location.Z = FMath::Lerp(PeakZ, End.Z, FMath::SmoothStep(0.f, 1.f, DescentT));
	}

	return Location;
}

FVector ASurvivorCharacter::BuildParkourLocationFromRootMotion(
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

	const UCapsuleComponent* Capsule = GetCapsuleComponent();
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

void ASurvivorCharacter::UpdateParkourRootMotion()
{
	if (!ActiveParkourMontage)
	{
		return;
	}

	USkeletalMeshComponent* MeshComp = GetMesh();
	UAnimInstance* AnimInstance = MeshComp ? MeshComp->GetAnimInstance() : nullptr;
	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	if (!AnimInstance || !MoveComp)
	{
		return;
	}

	const float MontageTime = AnimInstance->Montage_GetPosition(ActiveParkourMontage);
	const float MontageLength = ActiveParkourMontage->GetPlayLength();
	const float MontageAlpha = MontageLength > KINDA_SMALL_NUMBER ? MontageTime / MontageLength : 1.f;

	FVector RootMotionTranslation = FVector::ZeroVector;
	FQuat RootMotionRotation = FQuat::Identity;
	ExtractMontageRootMotionAtTime(ActiveParkourMontage, MontageTime, RootMotionTranslation, RootMotionRotation);

	const FQuat StartRotation = ParkourStartTransform.GetRotation();
	const FVector TargetLocation = BuildParkourVaultLocation(MontageAlpha);

	const FRotator TargetRotation(0.f, (StartRotation * RootMotionRotation).Rotator().Yaw, 0.f);

	const FVector Delta = TargetLocation - GetActorLocation();
	if (!Delta.IsNearlyZero(0.1f) || !GetActorRotation().Equals(TargetRotation, 0.5f))
	{
		SetActorLocationAndRotation(TargetLocation, TargetRotation, false, nullptr, ETeleportType::TeleportPhysics);
		MoveComp->Velocity = FVector::ZeroVector;
		MoveComp->bJustTeleported = true;
	}
}

bool ASurvivorCharacter::ExtractMontageRootMotionAtTime(
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

bool ASurvivorCharacter::ComputeParkourLandingFromRootMotion(FVector& OutLocation, FRotator& OutRotation) const
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
	SnapParkourLocationToGround(OutLocation);

	LogParkourDebug(
		FString::Printf(TEXT("RootMotion delta %s -> %s"), *RootMotionTranslation.ToCompactString(), *OutLocation.ToCompactString()),
		FColor::Green);
	return true;
}

bool ASurvivorCharacter::ResolveParkourLanding(FVector& OutLocation, FRotator& OutRotation) const
{
	OutLocation = ParkourVaultEndLocation;
	OutRotation = ParkourStartTransform.Rotator();
	SnapParkourLocationToGround(OutLocation);

	LogParkourDebug(
		FString::Printf(TEXT("Vault land %s"), *OutLocation.ToCompactString()),
		FColor::Green);
	return true;
}

void ASurvivorCharacter::SnapParkourLocationToGround(FVector& InOutLocation, bool bIgnoreParkourObstacle) const
{
	const UCapsuleComponent* Capsule = GetCapsuleComponent();
	if (!Capsule || !GetWorld())
	{
		return;
	}

	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const float TraceHighZ = FMath::Max(InOutLocation.Z, ParkourVaultPeakCenterZ) + HalfHeight + 120.f;
	const FVector TraceStart(InOutLocation.X, InOutLocation.Y, TraceHighZ);
	const FVector TraceEnd(InOutLocation.X, InOutLocation.Y, InOutLocation.Z - 400.f);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ParkourLand), false, this);
	if (bIgnoreParkourObstacle)
	{
		if (AActor* ObstacleActor = CurrentParkourObstacle.Get())
		{
			Params.AddIgnoredActor(ObstacleActor);
		}
	}
	if (ASPCollectibleItem* CarriedItem = InteractionComponent ? InteractionComponent->GetCarriedItem() : nullptr)
	{
		Params.AddIgnoredActor(CarriedItem);
	}

	FHitResult GroundHit;
	if (GetWorld()->LineTraceSingleByChannel(
		GroundHit,
		TraceStart,
		TraceEnd,
		ECC_Visibility,
		Params))
	{
		InOutLocation = FVector(InOutLocation.X, InOutLocation.Y, GroundHit.ImpactPoint.Z + HalfHeight);
	}
}

void ASurvivorCharacter::SetParkourObstacleCollisionIgnored(bool bIgnore)
{
	AActor* ObstacleActor = CurrentParkourObstacle.Get();
	if (!ObstacleActor)
	{
		return;
	}

	UCapsuleComponent* Capsule = GetCapsuleComponent();
	USkeletalMeshComponent* MeshComp = GetMesh();

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

void ASurvivorCharacter::ApplyParkourLanding(const FVector& WorldLocation, const FRotator& WorldRotation)
{
	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	if (MoveComp)
	{
		MoveComp->StopMovementImmediately();
		MoveComp->Velocity = FVector::ZeroVector;
		MoveComp->ClearAccumulatedForces();
	}

	TeleportTo(WorldLocation, WorldRotation, false, true);

	if (MoveComp)
	{
		MoveComp->SetMovementMode(MOVE_Walking);
		MoveComp->bJustTeleported = true;
		MoveComp->bForceNextFloorCheck = true;

		if (UCapsuleComponent* Capsule = GetCapsuleComponent())
		{
			MoveComp->FindFloor(Capsule->GetComponentLocation(), MoveComp->CurrentFloor, false, nullptr);
		}
	}

	LogParkourDebug(FString::Printf(TEXT("Teleport to %s"), *WorldLocation.ToCompactString()), FColor::Green);
}

void ASurvivorCharacter::Multicast_SnapParkourLanding_Implementation(FVector_NetQuantize WorldLocation, FRotator WorldRotation)
{
	ApplyParkourLanding(WorldLocation, WorldRotation);
}

void ASurvivorCharacter::OnParkourEndTimer()
{
	if (bIsParkour)
	{
		EndParkour(false);
	}
}

void ASurvivorCharacter::OnParkourMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	EndParkour(bInterrupted);
}

void ASurvivorCharacter::EndParkour(bool bInterrupted)
{
	if (!bIsParkour)
	{
		return;
	}

	bIsParkour = false;
	GetWorldTimerManager().ClearTimer(ParkourEndTimerHandle);

	FVector LandingLocation = ParkourStartTransform.GetLocation();
	FRotator LandingRotation = ParkourStartTransform.Rotator();
	const bool bHasLanding = ResolveParkourLanding(LandingLocation, LandingRotation);

	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		if (UAnimInstance* AnimInstance = MeshComp->GetAnimInstance())
		{
			AnimInstance->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
		}
	}

	if (bHasLanding)
	{
		if (HasAuthority())
		{
			Multicast_SnapParkourLanding(LandingLocation, LandingRotation);
		}
		else
		{
			ApplyParkourLanding(LandingLocation, LandingRotation);
		}
	}

	if (USkeletalMeshComponent* MeshComp = GetMesh())
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

	if (CurrentParkourObstacle.IsValid())
	{
		SetParkourObstacleCollisionIgnored(false);
	}

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		PrimaryActorTick.RemovePrerequisite(MoveComp, MoveComp->PrimaryComponentTick);
		MoveComp->bAllowPhysicsRotationDuringAnimRootMotion = false;
		MoveComp->GravityScale = CachedGravityScale;
		if (CachedMovementMode != MOVE_None)
		{
			MoveComp->SetMovementMode(CachedMovementMode);
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

	if (AController* Ctrl = GetController())
	{
		Ctrl->ResetIgnoreMoveInput();
	}

	ApplyStateEffects();
	LogParkourDebug(bInterrupted ? TEXT("Parkour ended (interrupted)") : TEXT("Parkour ended"), FColor::Cyan);
}


void ASurvivorCharacter::OnRep_SurvivorState(ESurvivorState OldState)
{
	if (MovementComponent)
	{
		MovementComponent->HandleStateTransition(OldState, SurvivorState);
	}
	ApplyStateEffects();
}

void ASurvivorCharacter::ApplyStateEffects()
{
	if (AController* Ctrl = GetController())
	{
		Ctrl->ResetIgnoreMoveInput();
		if (!CanMove())
		{
			Ctrl->SetIgnoreMoveInput(true);
		}
	}
}

void ASurvivorCharacter::BeginPickup(ASPCollectibleItem* Item)
{
	if (InteractionComponent)
	{
		InteractionComponent->BeginPickup(Item);
	}
}

void ASurvivorCharacter::BeginDelivery(ASPDeliveryStation* Station)
{
	if (InteractionComponent)
	{
		InteractionComponent->BeginDelivery(Station);
	}
}

void ASurvivorCharacter::BeginEscapeOpen(ASPEscapeGate* Gate)
{
	if (InteractionComponent)
	{
		InteractionComponent->BeginEscapeOpen(Gate);
	}
}

void ASurvivorCharacter::EndEscapeChanneling()
{
	if (InteractionComponent)
	{
		InteractionComponent->EndEscapeChanneling();
	}
}

void ASurvivorCharacter::BeginHatchEscape(ASPHatch* Hatch)
{
	if (InteractionComponent)
	{
		InteractionComponent->BeginHatchEscape(Hatch);
	}
}

void ASurvivorCharacter::CompleteHatchEscape()
{
	if (InteractionComponent)
	{
		InteractionComponent->CompleteHatchEscape();
	}
}

bool ASurvivorCharacter::IsCarrying() const
{
	return InteractionComponent && InteractionComponent->IsCarrying();
}

FGameplayTag ASurvivorCharacter::GetInteractableTag() const
{
	return InteractionComponent ? InteractionComponent->GetInteractableTag() : FGameplayTag();
}

void ASurvivorCharacter::NotifyMatchStateChange(ESurvivorState NewState)
{
	AMatchGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AMatchGameMode>() : nullptr;
	if (!GameMode)
	{
		return;
	}

	if (NewState == ESurvivorState::Escaped)
	{
		// 異뷀썑 ?듯빀 ???쒕?濡???ID瑜??섍린??諛⑹떇?쇰줈 援ы쁽
		GameMode->RegisterSurvivorEscaped(NAME_None);
	}
}

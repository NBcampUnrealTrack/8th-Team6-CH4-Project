#include "Components/SPTaserVFXComponent.h"

#include "Characters/Killer/KillerCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Character.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Systems/Data/KillerData.h"
#include "UObject/ConstructorHelpers.h"

USPTaserVFXComponent::USPTaserVFXComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> TetherBeamFinder(
		TEXT("/Game/Assets/Taser/VFX/TetherBeamFX/FX/NS_Electricity_1.NS_Electricity_1"));
	if (TetherBeamFinder.Succeeded())
	{
		TetherBeamSystem = TetherBeamFinder.Object;
	}
}

void USPTaserVFXComponent::BeginPlay()
{
	Super::BeginPlay();

	if (const AKillerCharacter* Killer = Cast<AKillerCharacter>(GetOwner()))
	{
		if (const UKillerData* Data = Killer->GetKillerData())
		{
			MaxBeamRange = Data->TaserRange;
		}
	}
}

void USPTaserVFXComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopAllVFX();
	Super::EndPlay(EndPlayReason);
}

UStaticMeshComponent* USPTaserVFXComponent::ResolveWeaponMesh() const
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	UStaticMeshComponent* NamedWeapon = nullptr;
	UStaticMeshComponent* TaserMesh = nullptr;

	for (UActorComponent* Comp : Owner->GetComponents())
	{
		UStaticMeshComponent* StaticMesh = Cast<UStaticMeshComponent>(Comp);
		if (!StaticMesh)
		{
			continue;
		}

		if (Comp->GetName().Contains(WeaponComponentName.ToString()))
		{
			NamedWeapon = StaticMesh;
		}

		if (const UStaticMesh* Asset = StaticMesh->GetStaticMesh())
		{
			const FString AssetName = Asset->GetName();
			if (AssetName.Contains(TEXT("Taser"), ESearchCase::IgnoreCase))
			{
				TaserMesh = StaticMesh;
			}
		}
	}

	return NamedWeapon ? NamedWeapon : TaserMesh;
}

USceneComponent* USPTaserVFXComponent::ResolveWeaponAttach() const
{
	if (UStaticMeshComponent* WeaponMesh = ResolveWeaponMesh())
	{
		return WeaponMesh;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	if (const ACharacter* Character = Cast<ACharacter>(Owner))
	{
		return Character->GetMesh();
	}

	return Owner->GetRootComponent();
}

FName USPTaserVFXComponent::ResolveCharacterWeaponSocket() const
{
	if (const ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		if (const USkeletalMeshComponent* Mesh = Character->GetMesh())
		{
			if (!CharacterWeaponSocketName.IsNone() && Mesh->DoesSocketExist(CharacterWeaponSocketName))
			{
				return CharacterWeaponSocketName;
			}

			static const FName FallbackSockets[] = {
				TEXT("weapon_r"),
				TEXT("WeaponSocket"),
				TEXT("ik_hand_gun"),
			};

			for (const FName& Candidate : FallbackSockets)
			{
				if (Mesh->DoesSocketExist(Candidate))
				{
					return Candidate;
				}
			}
		}
	}

	return NAME_None;
}

FVector USPTaserVFXComponent::ResolveWeaponTipLocalOffset(const UStaticMeshComponent* WeaponMesh) const
{
	if (!WeaponMesh)
	{
		return TipRelativeOffset;
	}

	if (!WeaponTipSocketName.IsNone() && WeaponMesh->DoesSocketExist(WeaponTipSocketName))
	{
		return WeaponMesh->GetSocketTransform(WeaponTipSocketName, RTS_Component).GetLocation();
	}

	if (!TipRelativeOffset.IsNearlyZero())
	{
		return TipRelativeOffset;
	}

	if (const UStaticMesh* Asset = WeaponMesh->GetStaticMesh())
	{
		const FBox Bounds = Asset->GetBoundingBox();
		const FVector LocalDir = BarrelLocalDirection.GetSafeNormal();
		const float Extent = FMath::Max3(
			FMath::Abs(Bounds.Max.X - Bounds.Min.X),
			FMath::Abs(Bounds.Max.Y - Bounds.Min.Y),
			FMath::Abs(Bounds.Max.Z - Bounds.Min.Z)) * 0.5f;
		return LocalDir * Extent;
	}

	return BarrelLocalDirection.GetSafeNormal() * 50.f;
}

FVector USPTaserVFXComponent::GetTipWorldLocation() const
{
	if (UStaticMeshComponent* WeaponMesh = ResolveWeaponMesh())
	{
		const FVector LocalTip = ResolveWeaponTipLocalOffset(WeaponMesh);
		return WeaponMesh->GetComponentTransform().TransformPosition(LocalTip);
	}

	if (const ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		if (USkeletalMeshComponent* Mesh = Character->GetMesh())
		{
			const FName SocketName = ResolveCharacterWeaponSocket();
			if (!SocketName.IsNone())
			{
				return Mesh->GetSocketLocation(SocketName);
			}
		}
	}

	if (USceneComponent* Attach = ResolveWeaponAttach())
	{
		return Attach->GetComponentTransform().TransformPosition(TipRelativeOffset);
	}

	if (const AActor* Owner = GetOwner())
	{
		return Owner->GetActorLocation() + GetAttackForward() * TipRelativeOffset.Size();
	}

	return FVector::ZeroVector;
}

FVector USPTaserVFXComponent::GetAttackForward() const
{
	if (UStaticMeshComponent* WeaponMesh = ResolveWeaponMesh())
	{
		const FVector LocalDir = BarrelLocalDirection.GetSafeNormal();
		if (!LocalDir.IsNearlyZero())
		{
			return WeaponMesh->GetComponentTransform().TransformVectorNoScale(LocalDir).GetSafeNormal();
		}

		return WeaponMesh->GetForwardVector().GetSafeNormal();
	}

	if (const AActor* Owner = GetOwner())
	{
		return Owner->GetActorForwardVector().GetSafeNormal();
	}

	return FVector::ForwardVector;
}

FVector USPTaserVFXComponent::ResolveBeamEnd(const FVector& Start, const FVector& EndWorld, const bool bHit) const
{
	const FVector Forward = GetAttackForward();

	if (bHit && !EndWorld.IsNearlyZero())
	{
		const FVector ToTarget = (EndWorld - Start).GetSafeNormal();
		if (!ToTarget.IsNearlyZero())
		{
			const float HitDistance = FVector::Dist(Start, EndWorld);
			return Start + ToTarget * FMath::Min(HitDistance, MaxBeamRange);
		}
	}

	return Start + Forward * MaxBeamRange;
}

void USPTaserVFXComponent::PlayDischarge(const FVector& EndWorld, const bool bHit)
{
	if (!TetherBeamSystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("SPTaserVFX: TetherBeamSystem is not assigned."));
		return;
	}

	ClearTetherBeam();

	const FVector Start = GetTipWorldLocation();
	const FVector End = ResolveBeamEnd(Start, EndWorld, bHit);

	if (bDrawDebugBeam)
	{
		if (UStaticMeshComponent* WeaponMesh = ResolveWeaponMesh())
		{
			DrawDebugSphere(GetWorld(), WeaponMesh->GetComponentLocation(), 8.f, 8, FColor::Yellow, false, TetherBeamLifetime);
		}
	}

	SpawnTetherBeam(Start, End);
	DrawDebugBeam(Start, End);
	ScheduleCleanup(TetherBeamLifetime);
}

void USPTaserVFXComponent::StopAllVFX()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CleanupTimerHandle);
	}

	ClearTetherBeam();
}

void USPTaserVFXComponent::ClearTetherBeam()
{
	if (ActiveTetherBeam)
	{
		ActiveTetherBeam->Deactivate();
		ActiveTetherBeam->DestroyComponent();
		ActiveTetherBeam = nullptr;
	}
}

void USPTaserVFXComponent::ApplyBeamParameters(UNiagaraComponent* BeamComponent, const float Length) const
{
	if (!BeamComponent)
	{
		return;
	}

	const FVector LocalEnd = FVector(Length, 0.f, 0.f);

	// TetherBeamFX reads start/end along the spawned component's forward axis.
	BeamComponent->SetVariableVec3(BeamStartParameter, FVector::ZeroVector);
	BeamComponent->SetVariableVec3(BeamEndParameter, LocalEnd);
	BeamComponent->SetVariableFloat(BeamLengthParameter, Length);
	BeamComponent->SetVariableVec3(BeamDirectionParameter, FVector::ForwardVector);
	BeamComponent->SetVariableFloat(TEXT("User.Beam"), Length);
	BeamComponent->SetVariableFloat(TEXT("User.Spawn"), 1.f);
	BeamComponent->SetVariableFloat(TEXT("User.Lifetime"), TetherBeamLifetime);
	BeamComponent->SetVariableFloat(TEXT("User.Loop"), 0.f);
}

void USPTaserVFXComponent::SpawnTetherBeam(const FVector& Start, const FVector& End)
{
	UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	if (!Owner || !TetherBeamSystem || !World)
	{
		return;
	}

	const float Length = FVector::Dist(Start, End);
	if (Length < KINDA_SMALL_NUMBER)
	{
		UE_LOG(LogTemp, Warning, TEXT("SPTaserVFX: Beam length too small (Start=%s End=%s)"), *Start.ToString(), *End.ToString());
		return;
	}

	const FRotator BeamRotation = (End - Start).GetSafeNormal().Rotation();

	ActiveTetherBeam = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		World,
		TetherBeamSystem,
		Start,
		BeamRotation,
		FVector::OneVector,
		true,
		false,
		ENCPoolMethod::None,
		true);

	if (!ActiveTetherBeam)
	{
		UE_LOG(LogTemp, Warning, TEXT("SPTaserVFX: Failed to spawn TetherBeam at %s"), *Start.ToString());
		return;
	}

	ActiveTetherBeam->SetAutoDestroy(true);
	ActiveTetherBeam->SetHiddenInGame(false);
	ActiveTetherBeam->SetVisibility(true);
	Owner->AddInstanceComponent(ActiveTetherBeam);
	ApplyBeamParameters(ActiveTetherBeam, Length);
	ActiveTetherBeam->Activate(true);

	UE_LOG(LogTemp, Warning, TEXT("SPTaserVFX: Beam Start=%s End=%s Length=%.1f"), *Start.ToString(), *End.ToString(), Length);
}

void USPTaserVFXComponent::DrawDebugBeam(const FVector& Start, const FVector& End) const
{
	if (!bDrawDebugBeam)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		DrawDebugLine(World, Start, End, FColor::Cyan, false, TetherBeamLifetime, 0, 3.f);
		DrawDebugSphere(World, Start, 12.f, 8, FColor::Green, false, TetherBeamLifetime);
		DrawDebugSphere(World, End, 12.f, 8, FColor::Red, false, TetherBeamLifetime);
	}
}

void USPTaserVFXComponent::ScheduleCleanup(const float Delay)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CleanupTimerHandle);
		World->GetTimerManager().SetTimer(
			CleanupTimerHandle,
			this,
			&USPTaserVFXComponent::StopAllVFX,
			FMath::Max(0.05f, Delay),
			false);
	}
}

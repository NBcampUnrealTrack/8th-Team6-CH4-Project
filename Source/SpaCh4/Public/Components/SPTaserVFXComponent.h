#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SPTaserVFXComponent.generated.h"

class UNiagaraSystem;
class UNiagaraComponent;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class ETaserVFXPhase : uint8
{
	Charge UMETA(DisplayName = "Charge"),
	Discharge UMETA(DisplayName = "Discharge")
};

/** Killer taser attack VFX using Tether Beam Niagara only. */
UCLASS(ClassGroup = (SP), meta = (BlueprintSpawnableComponent))
class SPACH4_API USPTaserVFXComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USPTaserVFXComponent();

	UFUNCTION(BlueprintCallable, Category = "SP|TaserVFX")
	void PlayDischarge(const FVector& EndWorld, bool bHit);

	UFUNCTION(BlueprintCallable, Category = "SP|TaserVFX")
	void StopAllVFX();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UStaticMeshComponent* ResolveWeaponMesh() const;
	USceneComponent* ResolveWeaponAttach() const;
	FName ResolveCharacterWeaponSocket() const;
	FVector ResolveWeaponTipLocalOffset(const UStaticMeshComponent* WeaponMesh) const;
	FVector GetTipWorldLocation() const;
	FVector GetAttackForward() const;
	FVector ResolveBeamEnd(const FVector& Start, const FVector& EndWorld, bool bHit) const;
	void ApplyBeamParameters(UNiagaraComponent* BeamComponent, float Length) const;
	void DrawDebugBeam(const FVector& Start, const FVector& End) const;
	void ClearTetherBeam();
	void SpawnTetherBeam(const FVector& Start, const FVector& End);
	void ScheduleCleanup(float Delay);

	UPROPERTY(EditDefaultsOnly, Category = "SP|TaserVFX|TetherBeam")
	TObjectPtr<UNiagaraSystem> TetherBeamSystem;

	UPROPERTY(EditDefaultsOnly, Category = "SP|TaserVFX|TetherBeam", meta = (ClampMin = "0.05"))
	float TetherBeamLifetime = 0.45f;

	UPROPERTY(EditDefaultsOnly, Category = "SP|TaserVFX|TetherBeam")
	FName BeamStartParameter = TEXT("BeamStart");

	UPROPERTY(EditDefaultsOnly, Category = "SP|TaserVFX|TetherBeam")
	FName BeamEndParameter = TEXT("BeamEnd");

	UPROPERTY(EditDefaultsOnly, Category = "SP|TaserVFX|TetherBeam")
	FName BeamLengthParameter = TEXT("BeamStartEndLength");

	UPROPERTY(EditDefaultsOnly, Category = "SP|TaserVFX|TetherBeam", meta = (ClampMin = "0.0"))
	float MaxBeamRange = 250.f;

	UPROPERTY(EditDefaultsOnly, Category = "SP|TaserVFX|TetherBeam")
	FName BeamDirectionParameter = TEXT("BeamDirection");

	/** Optional socket on the taser static mesh. Falls back to Tip Relative Offset / mesh bounds. */
	UPROPERTY(EditDefaultsOnly, Category = "SP|TaserVFX")
	FName WeaponTipSocketName = NAME_None;

	/** Tip offset in WeaponMesh local space. SM_Taser barrel is roughly +Z (~100uu). */
	UPROPERTY(EditDefaultsOnly, Category = "SP|TaserVFX")
	FVector TipRelativeOffset = FVector(0.f, 0.f, 100.f);

	/** Barrel direction in WeaponMesh local space. SM_Taser points along +Z. */
	UPROPERTY(EditDefaultsOnly, Category = "SP|TaserVFX")
	FVector BarrelLocalDirection = FVector(0.f, 0.f, 1.f);

	UPROPERTY(EditDefaultsOnly, Category = "SP|TaserVFX")
	FName WeaponComponentName = TEXT("WeaponMesh");

	/** Fallback when WeaponMesh is missing: character mesh weapon socket (not hand). */
	UPROPERTY(EditDefaultsOnly, Category = "SP|TaserVFX")
	FName CharacterWeaponSocketName = TEXT("weapon_r");

	UPROPERTY(EditDefaultsOnly, Category = "SP|TaserVFX|Debug")
	bool bDrawDebugBeam = true;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> ActiveTetherBeam;

	FTimerHandle CleanupTimerHandle;
};

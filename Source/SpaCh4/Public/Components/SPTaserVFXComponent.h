#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SPTaserVFXComponent.generated.h"

class UNiagaraSystem;
class UNiagaraComponent;

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
	USceneComponent* ResolveWeaponAttach() const;
	FName ResolveTipSocketName() const;
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

	UPROPERTY(EditDefaultsOnly, Category = "SP|TaserVFX")
	FName TipSocketName = TEXT("hand_r");

	UPROPERTY(EditDefaultsOnly, Category = "SP|TaserVFX")
	FVector TipRelativeOffset = FVector(18.f, 0.f, 2.f);

	UPROPERTY(EditDefaultsOnly, Category = "SP|TaserVFX")
	FName WeaponComponentName = TEXT("WeaponMesh");

	UPROPERTY(EditDefaultsOnly, Category = "SP|TaserVFX|Debug")
	bool bDrawDebugBeam = false;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> ActiveTetherBeam;

	FTimerHandle CleanupTimerHandle;
};

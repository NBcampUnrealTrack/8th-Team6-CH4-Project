#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SPScratchMarkComponent.generated.h"

class ASurvivorCharacter;
class USPMovementComponent;
class UMaterialInterface;

UCLASS(ClassGroup = (SP), meta = (BlueprintSpawnableComponent))
class SPACH4_API USPScratchMarkComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USPScratchMarkComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

private:
	ASurvivorCharacter* GetSurvivor() const;
	bool IsLeavingMarks() const;
	bool ShouldLocalViewerSeeMarks() const;
	void SpawnScratchMark(FVector Location, FVector Normal, float BaseYaw) const;
	float PickNextInterval() const;

	UPROPERTY(EditDefaultsOnly, Category = "SP|ScratchMark")
	TObjectPtr<UMaterialInterface> ScratchDecalMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "SP|ScratchMark")
	float SpawnDistanceInterval{90.f};

	UPROPERTY(EditDefaultsOnly, Category = "SP|ScratchMark")
	float SpawnDistanceJitter{35.f};

	UPROPERTY(EditDefaultsOnly, Category = "SP|ScratchMark")
	float MinSpeedToLeaveMarks{50.f};

	UPROPERTY(EditDefaultsOnly, Category = "SP|ScratchMark")
	FVector DecalSize{16.f, 32.f, 32.f};

	UPROPERTY(EditDefaultsOnly, Category = "SP|ScratchMark")
	float DecalSizeJitter{0.25f};

	UPROPERTY(EditDefaultsOnly, Category = "SP|ScratchMark")
	float LateralJitter{22.f};

	UPROPERTY(EditDefaultsOnly, Category = "SP|ScratchMark")
	float YawJitter{25.f};

	UPROPERTY(EditDefaultsOnly, Category = "SP|ScratchMark")
	float DecalLifetime{2.5f};

	UPROPERTY(EditDefaultsOnly, Category = "SP|ScratchMark")
	float DecalFadeDuration{0.6f};

	UPROPERTY(EditDefaultsOnly, Category = "SP|ScratchMark")
	float SurfaceTraceUpOffset{60.f};

	UPROPERTY(EditDefaultsOnly, Category = "SP|ScratchMark")
	float SurfaceTraceDownDistance{200.f};

	UPROPERTY(EditDefaultsOnly, Category = "SP|ScratchMark")
	bool bKillerOnly{true};

	TWeakObjectPtr<USPMovementComponent> CachedMovement;

	FVector LastSampleLocation{FVector::ZeroVector};
	float DistanceSinceLastMark{0.f};
	float NextInterval{90.f};
	bool bHasLastSample{false};
};

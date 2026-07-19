#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SPOilDripComponent.generated.h"

class ASurvivorCharacter;
class UMaterialInterface;

UCLASS(ClassGroup = (SP), meta = (BlueprintSpawnableComponent))
class SPACH4_API USPOilDripComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USPOilDripComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

private:
	ASurvivorCharacter* GetSurvivor() const;
	bool IsLeavingDrips() const;
	void SpawnOilDrip(FVector Location, FVector Normal) const;
	float PickNextInterval() const;

	UPROPERTY(EditDefaultsOnly, Category = "SP|OilDrip")
	TObjectPtr<UMaterialInterface> OilDecalMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "SP|OilDrip")
	float DripInterval{5.f};

	UPROPERTY(EditDefaultsOnly, Category = "SP|OilDrip")
	float DripIntervalJitter{0.6f};

	UPROPERTY(EditDefaultsOnly, Category = "SP|OilDrip")
	float DecalLifetime{7.f};

	UPROPERTY(EditDefaultsOnly, Category = "SP|OilDrip")
	float DecalFadeDuration{1.5f};

	UPROPERTY(EditDefaultsOnly, Category = "SP|OilDrip")
	FVector DecalSize{16.f, 32.f, 32.f};

	UPROPERTY(EditDefaultsOnly, Category = "SP|OilDrip")
	float DecalSizeJitter{0.25f};

	UPROPERTY(EditDefaultsOnly, Category = "SP|OilDrip")
	float LateralJitter{18.f};

	UPROPERTY(EditDefaultsOnly, Category = "SP|OilDrip")
	int32 DecalSortOrder{1};

	UPROPERTY(EditDefaultsOnly, Category = "SP|OilDrip")
	float SurfaceTraceUpOffset{60.f};

	UPROPERTY(EditDefaultsOnly, Category = "SP|OilDrip")
	float SurfaceTraceDownDistance{200.f};

	float TimeSinceLastDrip{0.f};
	float NextInterval{5.f};
};

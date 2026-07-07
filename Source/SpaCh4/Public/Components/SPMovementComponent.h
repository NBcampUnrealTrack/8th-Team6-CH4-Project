#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SPMovementComponent.generated.h"

class ASurvivorCharacter;
class USurvivorData;
enum class ESurvivorState : uint8;

UCLASS(ClassGroup = (SP), meta = (BlueprintSpawnableComponent))
class SPACH4_API USPMovementComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USPMovementComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void HandleStateTransition(ESurvivorState OldState, ESurvivorState NewState);
	void SnapToTargetSpeed();

protected:
	virtual void BeginPlay() override;

private:
	ASurvivorCharacter* GetSurvivor() const;
	const USurvivorData* GetSurvivorData() const;

	float ComputeTargetMoveSpeed() const;
	float GetBaseWalkSpeed() const;
	float GetSprintSpeedForState(ESurvivorState State) const;
	float GetCarryMoveSpeedMultiplier() const;
	void StartHitEscapeSprint(ESurvivorState PreviousState);
	
	UPROPERTY(EditDefaultsOnly, Category = "SP|Movement")
	float MoveSpeedInterpSpeed{8.f};
	
	bool bHitEscapeSprintActive{false};
	float HitEscapeSprintRemaining{0.f};
	float HitEscapeSprintSpeed{0.f};
};

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SPHatchManagerSubsystem.generated.h"

class ASPHatch;


UCLASS(BlueprintType)
class SPACH4_API USPHatchManagerSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	void RegisterHatch(ASPHatch* Hatch);
	void UnregisterHatch(ASPHatch* Hatch);

	UFUNCTION(BlueprintCallable, Category = "SP|Hatch")
	void RequestRandomActivation();

	UFUNCTION(BlueprintCallable, Category = "SP|Hatch")
	void ResetHatches();

	UFUNCTION(BlueprintPure, Category = "SP|Hatch")
	ASPHatch* GetActiveHatch() const;

private:
	bool HasServerAuthority() const;
	void TryActivateRandomHatch();
	void SchedulePendingActivation();
	void PruneInvalidHatches();

	TArray<TWeakObjectPtr<ASPHatch>> RegisteredHatches;
	TWeakObjectPtr<ASPHatch> ActiveHatch;
	bool bActivationRequested = false;
	bool bActivationScheduled = false;
};

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CheatManager.h"
#include "Characters/Survivor/SurvivorCharacter.h"
#include "SPCheatManager.generated.h"

/** Dev cheats for survivor state / locomotion testing. Console: SPDowned, SPHealthy, SPInjured */
UCLASS()
class SPACH4_API USPCheatManager : public UCheatManager
{
	GENERATED_BODY()

public:
	/** Force possessed survivor into Downed (prone crawl test). */
	UFUNCTION(Exec, Category = "SP|Cheat")
	void SPDowned();

	/** Restore possessed survivor to Healthy. */
	UFUNCTION(Exec, Category = "SP|Cheat")
	void SPHealthy();

	/** Force possessed survivor to Injured. */
	UFUNCTION(Exec, Category = "SP|Cheat")
	void SPInjured();

private:
	void ApplySurvivorState(ESurvivorState NewState);
};

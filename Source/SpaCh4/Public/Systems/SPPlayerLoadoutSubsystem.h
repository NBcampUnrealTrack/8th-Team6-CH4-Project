#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Player/SPPlayerLoadout.h"
#include "SPPlayerLoadoutSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSPCachedLoadoutChangedSignature, const FSPPlayerLoadout&, Loadout);

UCLASS()
class SPACH4_API USPPlayerLoadoutSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Loadout")
	void SaveCachedLoadout(const FSPPlayerLoadout& NewLoadout);

	UFUNCTION(BlueprintPure, Category = "Loadout")
	const FSPPlayerLoadout& GetCachedLoadout() const;

	UFUNCTION(BlueprintPure, Category = "Loadout")
	bool IsSurvivorLoadoutConfigured() const;

	UFUNCTION(BlueprintPure, Category = "Loadout")
	bool IsKillerLoadoutConfigured() const;

	UFUNCTION(BlueprintPure, Category = "Loadout")
	bool IsLoadoutComplete() const;

	UPROPERTY(BlueprintAssignable, Category = "Loadout")
	FSPCachedLoadoutChangedSignature OnCachedLoadoutChanged;

private:
	UPROPERTY()
	FSPPlayerLoadout CachedLoadout;
};

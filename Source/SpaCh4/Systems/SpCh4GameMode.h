#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SpCh4GameMode.generated.h"

class USpCh4BalanceData;

UCLASS()
class SPACH4_API ASpCh4GameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ASpCh4GameMode();

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Balance")
	TObjectPtr<USpCh4BalanceData> BalanceData;
};

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SpCh4EscapeGate.generated.h"

UCLASS()
class SPACH4_API ASpCh4EscapeGate : public AActor
{
	GENERATED_BODY()

public:
	ASpCh4EscapeGate();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Escape")
	float OpenDuration = 8.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Escape")
	float OpenProgress = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Escape")
	bool bIsActivated = false;
};

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EscapeGate.generated.h"

UCLASS()
class SPACH4_API AEscapeGate : public AActor
{
	GENERATED_BODY()

public:
	AEscapeGate();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Escape")
	float OpenDuration = 8.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Escape")
	float OpenProgress = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Escape")
	bool bIsActivated = false;
};

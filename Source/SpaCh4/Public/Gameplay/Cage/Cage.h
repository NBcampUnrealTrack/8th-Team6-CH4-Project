#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Cage.generated.h"

// 1. 여기에 전방 선언을 추가합니다.
class ASurvivorCharacter; 

UENUM(BlueprintType)
enum class ECageStatus : uint8
{
	Empty       = 0 UMETA(DisplayName = "Empty"),
	Occupied    = 1 UMETA(DisplayName = "Occupied"),
	Dead        = 2 UMETA(DisplayName = "Dead")
};

UCLASS()
class SPACH4_API ACage : public AActor
{
	GENERATED_BODY()

public:
	ACage();

	void SetCageStatus(ECageStatus NewStatus);

	// 현재 상태 확인 (살인마가 상호작용 가능 여부 판단용)
	UFUNCTION(BlueprintPure, Category = "Cage")
	ECageStatus GetCageStatus() const { return CurrentStatus; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cage")
	ECageStatus CurrentStatus = ECageStatus::Empty;

	UPROPERTY(Replicated)
    TObjectPtr<ASurvivorCharacter> TrappedSurvivor;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cage")
	TObjectPtr<UStaticMeshComponent> CageMesh;

public :
	
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cage")
	float StageOneDuration = 35.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cage")
	float StageTwoDuration = 35.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cage")
	float RescueDuration = 4.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cage")
	ECageStatus CageStage = ECageStatus::Empty;
};

#pragma once

#include "CoreMinimal.h"
#include "Characters/Base/CharacterBase.h"
#include "SurvivorCharacter.generated.h"

UENUM(BlueprintType)
enum class ESurvivorState : uint8
{
	Healthy,
	Injured,
	Downed,
	Carried,
	Caged,
	Dead,
	Escaped
};

UCLASS()
class SPACH4_API ASurvivorCharacter : public ACharacterBase
{
	GENERATED_BODY()

public:
	ASurvivorCharacter();
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	
	void SetSurvivorState(ESurvivorState NewState);
	bool CanMove() const;
	bool CanInteract() const;
private:
	UFUNCTION()
	void OnRep_SurvivorState();
	
	void ApplyStateEffects();
	
	UPROPERTY(ReplicatedUsing="OnRep_SurvivorState")
	ESurvivorState SurvivorState = ESurvivorState::Healthy;
	  
};

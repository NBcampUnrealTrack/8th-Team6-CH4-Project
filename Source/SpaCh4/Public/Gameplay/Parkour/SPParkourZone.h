#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SPParkourZone.generated.h"

class UBoxComponent;
class UBillboardComponent;
class ASurvivorCharacter;

UCLASS(Blueprintable)
class SPACH4_API ASPParkourZone : public AActor
{
	GENERATED_BODY()

public:
	ASPParkourZone();

	UFUNCTION(BlueprintPure, Category = "SP|Parkour")
	bool ContainsPoint(const FVector& WorldPoint) const;

	UFUNCTION(BlueprintPure, Category = "SP|Parkour")
	UBoxComponent* GetZoneBox() const { return ZoneBox; }

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnZoneBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void OnZoneEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SP|Parkour")
	TObjectPtr<UBoxComponent> ZoneBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SP|Parkour")
	TObjectPtr<UBillboardComponent> EditorSprite;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SP|Parkour|Debug")
	bool bDrawDebugBounds = false;

private:
	void HandleSurvivorOverlap(ASurvivorCharacter* Survivor, bool bEntered);
};

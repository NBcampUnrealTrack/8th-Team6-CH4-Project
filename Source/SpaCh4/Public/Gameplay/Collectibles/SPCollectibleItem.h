#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interface/SPInteractable.h"
#include "SPCollectibleItem.generated.h"

UENUM(BlueprintType)
enum class ECollectibleSize : uint8
{
	Small,
	Medium,
	Large,
	Hazardous
};

UCLASS()
class SPACH4_API ASPCollectibleItem : public AActor, public ISPInteractable
{
	GENERATED_BODY()

public:
	ASPCollectibleItem();
	
	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual void SetHighlight_Implementation(bool bEnabled) override;

	int32 GetValue() const { return Value; }
	ECollectibleSize GetCollectibleSize() const { return CollectibleSize; }
	bool BlocksParkourWhileCarried() const { return bBlocksParkourWhileCarried; }
	void SetPickupCollisionEnabled(bool bEnabled);


protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Collectible")
	TObjectPtr<UStaticMeshComponent> Mesh;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collectible")
	ECollectibleSize CollectibleSize = ECollectibleSize::Small;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collectible")
	int32 Value = 10;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collectible")
	bool bBlocksParkourWhileCarried = false;
};

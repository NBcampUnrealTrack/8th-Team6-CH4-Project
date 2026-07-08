#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interface/SPInteractable.h"
#include "SPCollectibleItem.generated.h"

class UTexture2D;

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
	virtual FGameplayTag GetInteractableTag_Implementation() const override;

	int32 GetValue() const { return Value; }
	ECollectibleSize GetCollectibleSize() const { return CollectibleSize; }
	TSoftObjectPtr<UTexture2D> GetIcon() const { return Icon; }
	void SetPickupCollisionEnabled(bool bEnabled);
	
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SP|Collectible")
	TObjectPtr<UStaticMeshComponent> Mesh;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SP|Collectible")
	ECollectibleSize CollectibleSize = ECollectibleSize::Small;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SP|Collectible")
	int32 Value = 10;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SP|Collectible")
	TSoftObjectPtr<UTexture2D> Icon;
};

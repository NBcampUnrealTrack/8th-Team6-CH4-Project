#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SPPlayerController.generated.h"

class UInputConfigData;
class UInputMappingContext;
class UEnhancedInputLocalPlayerSubsystem;

UCLASS()
class SPACH4_API ASPPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	// 런타임에 IMC를 추가/제거 (상황별 컨텍스트 레이어링용)
	UFUNCTION(BlueprintCallable, Category = "Input")
	void AddInputMappingContext(UInputMappingContext* MappingContext, int32 Priority = 0);

	UFUNCTION(BlueprintCallable, Category = "Input")
	void RemoveInputMappingContext(UInputMappingContext* MappingContext);

protected:
	virtual void BeginPlay() override;

private:
	UEnhancedInputLocalPlayerSubsystem* GetInputSubsystem() const;

	// 시작 시 등록할 IMC 목록을 담은 DataAsset
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputConfigData> InputConfig;
};

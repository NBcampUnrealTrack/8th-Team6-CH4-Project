#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraModifier.h"
#include "MainMenuExposureCameraModifier.generated.h"

UCLASS()
class SPACH4_API UMainMenuExposureCameraModifier : public UCameraModifier
{
	GENERATED_BODY()

public:
	UMainMenuExposureCameraModifier();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MainMenu|Exposure")
	float LockedExposureBias = -1.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MainMenu|Exposure")
	float LockedExposureBrightness = 0.16f;

protected:
	virtual void ModifyPostProcess(float DeltaTime, float& PostProcessBlendWeight, FPostProcessSettings& PostProcessSettings) override;
};

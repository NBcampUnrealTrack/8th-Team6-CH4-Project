#include "Player/MainMenuExposureCameraModifier.h"

#include "Engine/Scene.h"

UMainMenuExposureCameraModifier::UMainMenuExposureCameraModifier()
{
	Priority = 0;
}

void UMainMenuExposureCameraModifier::ModifyPostProcess(
	float DeltaTime,
	float& PostProcessBlendWeight,
	FPostProcessSettings& PostProcessSettings)
{
	PostProcessBlendWeight = 1.0f;

	PostProcessSettings.bOverride_AutoExposureMethod = true;
	PostProcessSettings.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
	PostProcessSettings.bOverride_AutoExposureBias = true;
	PostProcessSettings.AutoExposureBias = LockedExposureBias;
	PostProcessSettings.bOverride_AutoExposureMinBrightness = true;
	PostProcessSettings.bOverride_AutoExposureMaxBrightness = true;
	PostProcessSettings.AutoExposureMinBrightness = LockedExposureBrightness;
	PostProcessSettings.AutoExposureMaxBrightness = LockedExposureBrightness;
	PostProcessSettings.bOverride_AutoExposureSpeedUp = true;
	PostProcessSettings.AutoExposureSpeedUp = 0.0f;
	PostProcessSettings.bOverride_AutoExposureSpeedDown = true;
	PostProcessSettings.AutoExposureSpeedDown = 0.0f;
	PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
	PostProcessSettings.AutoExposureApplyPhysicalCameraExposure = false;
	PostProcessSettings.bOverride_LocalExposureHighlightContrastScale = true;
	PostProcessSettings.LocalExposureHighlightContrastScale = 0.42f;
	PostProcessSettings.bOverride_LocalExposureShadowContrastScale = true;
	PostProcessSettings.LocalExposureShadowContrastScale = 0.7f;
	PostProcessSettings.bOverride_BloomIntensity = true;
	PostProcessSettings.BloomIntensity = 0.22f;
	PostProcessSettings.bOverride_BloomThreshold = true;
	PostProcessSettings.BloomThreshold = 1.2f;
}

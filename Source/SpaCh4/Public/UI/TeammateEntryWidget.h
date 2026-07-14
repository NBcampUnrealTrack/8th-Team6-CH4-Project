#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/HUDTypes.h"
#include "TeammateEntryWidget.generated.h"

class UImage;
class UMaterialInstanceDynamic;
class UProgressBar;
class UTextBlock;
class UTexture2D;
class UWidget;
class USPGameHUDStyleData;

UCLASS(Abstract, Blueprintable)
class SPACH4_API UTeammateEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "HUD")
	void UpdateFromData(const FTeammateHUDData& Data);

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> PortraitImage;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> PortraitSlotFrame;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NicknameText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CageStackText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> DownedHealthBarRoot;

	/** WBP Brush·Image Size·Size Box 우선 — C++가 텍스처/크기 덮어쓰지 않음 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> DownedHealthBarBG;

	/** Legacy Image fill — ProgressBar 사용 시 제거 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> DownedHealthBarFill;

	/** WBP Size Box·Style·레이아웃 우선. C++는 런타임 Percent만 갱신 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> DownedHealthBarProgress;

	/** WBP 클래스 기본값에서 지정 — 노멀(Healthy/Caged/Escaped 등) 초상화 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|Portrait")
	TObjectPtr<UTexture2D> PortraitIconHealthy;

	/** WBP 클래스 기본값에서 지정 — 부상/다운/운반 초상화 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|Portrait")
	TObjectPtr<UTexture2D> PortraitIconInjured;

	/** WBP 클래스 기본값에서 지정 — 사망 초상화 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|Portrait")
	TObjectPtr<UTexture2D> PortraitIconDead;

	/** WBP 클래스 기본값에서 지정 — 초상화 테두리 프레임 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|Portrait")
	TObjectPtr<UTexture2D> PortraitSlotTexture;

	/** 0이면 PortraitImage 위젯 디자이너 크기 사용. 미설정 시 100x100 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|Portrait", meta = (ClampMin = "0"))
	FVector2D PortraitIconDisplaySize = FVector2D::ZeroVector;

	/** 0이면 PortraitSlotFrame 위젯 디자이너 크기 사용. 미설정 시 PortraitIcon과 동일 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|Portrait", meta = (ClampMin = "0"))
	FVector2D PortraitFrameDisplaySize = FVector2D::ZeroVector;

	/** 미지정 시 SPUIStyleLibrary 기본값 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|Style")
	TObjectPtr<USPGameHUDStyleData> VisualStyle;

	UFUNCTION(BlueprintImplementableEvent, Category = "HUD")
	void OnDisplayStateChanged(ESurvivorDisplayState NewState);

private:
	static constexpr float DownedHealthBarWidth = 248.0f;
	static constexpr float DownedHealthBarHeight = 27.0f;
	static constexpr float DefaultPortraitDisplaySize = 100.0f;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DownedHealthBarFillMID;

	UPROPERTY(Transient)
	bool bDownedHealthBarInitialized = false;

	UPROPERTY(Transient)
	int32 DownedHealthBarSetupRetries = 0;

	FTimerHandle DownedHealthBarSetupTimerHandle;

	UPROPERTY(Transient)
	FVector2D CachedPortraitIconSize = FVector2D::ZeroVector;

	UPROPERTY(Transient)
	FVector2D CachedPortraitFrameSize = FVector2D::ZeroVector;

	void CachePortraitLayoutSizes();
	FVector2D ResolvePortraitIconSize() const;
	FVector2D ResolvePortraitFrameSize() const;
	void EnsurePortraitOverlayZOrder();

	void CollapseLegacyDownedHealthFillIfNeeded();
	void SetupDownedHealthBar();
	void ApplyPortraitVisuals();
	void ResolveOptionalWidgetBindings();
	bool AreDownedHealthBarWidgetsReady() const;
	bool CanRunDeferredSetup() const;
	void ScheduleDownedHealthBarSetup();
	void ClearDownedHealthBarSetupTimer();
	void ResetDownedHealthBarRuntimeState();
	void UpdateNickname(const FText& Nickname);
	void UpdateCageStack(int32 Stack, int32 MaxStack);
	void UpdateDownedHealth(float HealthPercent, bool bShowBar);
	void UpdatePortraitFrame(ESurvivorDisplayState State);
	void UpdatePortraitImage(ESurvivorDisplayState State);
	const USPGameHUDStyleData& GetResolvedStyle() const;
	UTexture2D* ResolvePortraitTexture(ESurvivorDisplayState State) const;
	UTexture2D* ResolvePortraitSlotTexture() const;
};

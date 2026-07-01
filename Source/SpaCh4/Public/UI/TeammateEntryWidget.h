#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/HUDTypes.h"
#include "TeammateEntryWidget.generated.h"

class UImage;
class UMaterialInstanceDynamic;
class UTextBlock;
class UTexture2D;
class UWidget;

UCLASS(Abstract, Blueprintable)
class SPACH4_API UTeammateEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "HUD")
	void UpdateFromData(const FTeammateHUDData& Data);

protected:
	virtual void NativeConstruct() override;

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

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> DownedHealthBarBG;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> DownedHealthBarFill;

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

	UFUNCTION(BlueprintImplementableEvent, Category = "HUD")
	void OnDisplayStateChanged(ESurvivorDisplayState NewState);

private:
	static constexpr float DownedHealthBarWidth = 248.0f;
	static constexpr float DownedHealthBarHeight = 27.0f;
	static constexpr float DefaultPortraitDisplaySize = 100.0f;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DownedHealthBarFillMID;

	UPROPERTY(Transient)
	FVector2D CachedPortraitIconSize = FVector2D::ZeroVector;

	UPROPERTY(Transient)
	FVector2D CachedPortraitFrameSize = FVector2D::ZeroVector;

	void CachePortraitLayoutSizes();
	FVector2D ResolvePortraitIconSize() const;
	FVector2D ResolvePortraitFrameSize() const;
	void EnsurePortraitOverlayZOrder();

	void SetupDownedHealthBar();
	void UpdateNickname(const FText& Nickname);
	void UpdateCageStack(int32 Stack, int32 MaxStack);
	void UpdateDownedHealth(float HealthPercent, bool bShowBar);
	void UpdatePortraitFrame(ESurvivorDisplayState State);
	void UpdatePortraitImage(ESurvivorDisplayState State);
	UTexture2D* ResolvePortraitTexture(ESurvivorDisplayState State) const;
	UTexture2D* ResolvePortraitSlotTexture() const;
};

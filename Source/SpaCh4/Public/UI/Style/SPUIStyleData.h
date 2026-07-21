#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SPUIStyleData.generated.h"

class UFontFace;
class UMaterialInterface;
class UTexture2D;

/** 인게임 HUD 텍스처·머티리얼 (WBP 또는 DA에서 할당, 미할당 시 SPUIStyleLibrary 기본값) */
UCLASS(BlueprintType)
class SPACH4_API USPGameHUDStyleData : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Delivery")
	TObjectPtr<UTexture2D> DeliveryStationFrameA;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Delivery")
	TObjectPtr<UTexture2D> DeliveryStationFrameB;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Delivery|Fill")
	TObjectPtr<UMaterialInterface> DeliveryProgressFillMaterialInstance;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Delivery|Fill")
	TObjectPtr<UMaterialInterface> DeliveryProgressFillMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Delivery|Fill")
	TObjectPtr<UTexture2D> DeliveryProgressFillTexture;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Delivery|Fill")
	TObjectPtr<UTexture2D> DeliveryProgressFillTextureFallback;

	/** 인덱스 = 채워진 세그먼트 수 (0~10) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Delivery|Legacy")
	TArray<TObjectPtr<UTexture2D>> DeliveryProgressSegmentTextures;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Delivery|Legacy")
	TObjectPtr<UTexture2D> DeliveryProgressEmptyFallback;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Delivery|Legacy")
	TObjectPtr<UTexture2D> DeliveryProgressBackground;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Delivery|Legacy")
	TObjectPtr<UTexture2D> DeliveryStackEmpty;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Delivery|Legacy")
	TObjectPtr<UTexture2D> DeliveryStackFilled;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Teammate")
	TObjectPtr<UTexture2D> PortraitSlotFrame;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Teammate")
	TObjectPtr<UTexture2D> PortraitHealthy;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Teammate")
	TObjectPtr<UTexture2D> PortraitInjured;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Teammate")
	TObjectPtr<UTexture2D> PortraitCaged;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Teammate")
	TObjectPtr<UTexture2D> PortraitDead;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Teammate|Downed")
	TObjectPtr<UTexture2D> DownedHealthBarBackground;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Teammate|Downed")
	TObjectPtr<UMaterialInterface> DownedHealthFillMaterialInstance;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Teammate|Downed")
	TObjectPtr<UMaterialInterface> DownedHealthFillMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Teammate|Downed")
	TObjectPtr<UTexture2D> DownedHealthFillTexture;

	UTexture2D* GetDeliveryProgressSegmentTexture(int32 FilledSegments) const;
};

UCLASS(BlueprintType)
class SPACH4_API USPMainMenuStyleData : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MainMenu")
	TObjectPtr<UTexture2D> TitleTexture;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MainMenu|Buttons")
	TObjectPtr<UTexture2D> SurvivorButtonNormal;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MainMenu|Buttons")
	TObjectPtr<UTexture2D> SurvivorButtonHovered;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MainMenu|Buttons")
	TObjectPtr<UTexture2D> KillerButtonNormal;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MainMenu|Buttons")
	TObjectPtr<UTexture2D> KillerButtonHovered;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MainMenu|Buttons")
	TObjectPtr<UTexture2D> SettingsButtonNormal;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MainMenu|Buttons")
	TObjectPtr<UTexture2D> SettingsButtonHovered;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MainMenu|Buttons")
	TObjectPtr<UTexture2D> QuitButtonNormal;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MainMenu|Buttons")
	TObjectPtr<UTexture2D> QuitButtonHovered;
};

UCLASS(BlueprintType)
class SPACH4_API USPGameResultStyleData : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GameResult|Background")
	TObjectPtr<UTexture2D> Background;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GameResult|Role")
	TObjectPtr<UTexture2D> SurvivorCharacter;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GameResult|Role")
	TObjectPtr<UTexture2D> KillerCharacter;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GameResult|Grade")
	TObjectPtr<UTexture2D> SurvivorPerfectWinGrade;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GameResult|Grade")
	TObjectPtr<UTexture2D> SurvivorWinGrade;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GameResult|Grade")
	TObjectPtr<UTexture2D> SurvivorMinorWinGrade;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GameResult|Grade")
	TObjectPtr<UTexture2D> KillerWinGrade;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GameResult|Grade")
	TObjectPtr<UTexture2D> KillerPerfectWinGrade;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GameResult|Stats|Survivor")
	TObjectPtr<UTexture2D> DeliveredValueIcon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GameResult|Stats|Survivor")
	TObjectPtr<UTexture2D> DeliveryCountIcon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GameResult|Stats|Survivor")
	TObjectPtr<UTexture2D> RescueIcon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GameResult|Stats|Survivor")
	TObjectPtr<UTexture2D> SelfHealIcon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GameResult|Stats|Survivor")
	TObjectPtr<UTexture2D> CagedIcon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GameResult|Stats|Killer")
	TObjectPtr<UTexture2D> KilledSurvivorsIcon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GameResult|Stats|Killer")
	TObjectPtr<UTexture2D> ValidHitIcon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GameResult|Stats|Killer")
	TObjectPtr<UTexture2D> DownIcon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GameResult|Stats|Killer")
	TObjectPtr<UTexture2D> CageCountIcon;
};

UCLASS(BlueprintType)
class SPACH4_API USPUIFontStyleData : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fonts")
	TObjectPtr<UFontFace> FontSemiBold;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Fonts")
	TObjectPtr<UFontFace> FontMedium;
};

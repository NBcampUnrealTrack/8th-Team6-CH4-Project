#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Player/SPPlayerLoadout.h"
#include "SPLoadoutSettingsWidget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSPLoadoutSelectionChangedSignature, const FSPPlayerLoadout&, PendingLoadout);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSPLoadoutSettingsSavedSignature, const FSPPlayerLoadout&, SavedLoadout);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSPLoadoutSettingsClosedSignature);

UCLASS(Abstract, Blueprintable)
class SPACH4_API USPLoadoutSettingsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Loadout|Settings")
	void InitializeFromSavedLoadout();

	UFUNCTION(BlueprintCallable, Category = "Loadout|Settings")
	void SelectSurvivorItem(EConsumableItemType ItemType);

	UFUNCTION(BlueprintCallable, Category = "Loadout|Settings")
	bool ToggleSurvivorPerk(EPerkType PerkType);

	UFUNCTION(BlueprintCallable, Category = "Loadout|Settings")
	bool ToggleKillerPerk(EKillerPerkType PerkType);

	UFUNCTION(BlueprintCallable, Category = "Loadout|Settings")
	bool SaveLoadout();

	UFUNCTION(BlueprintCallable, Category = "Loadout|Settings")
	void CloseSettings();

	UFUNCTION(BlueprintPure, Category = "Loadout|Settings")
	const FSPPlayerLoadout& GetPendingLoadout() const;

	UFUNCTION(BlueprintPure, Category = "Loadout|Settings")
	bool IsSurvivorPerkSelected(EPerkType PerkType) const;

	UFUNCTION(BlueprintPure, Category = "Loadout|Settings")
	bool IsKillerPerkSelected(EKillerPerkType PerkType) const;

	UPROPERTY(BlueprintAssignable, Category = "Loadout|Settings")
	FSPLoadoutSelectionChangedSignature OnLoadoutSelectionChanged;

	UPROPERTY(BlueprintAssignable, Category = "Loadout|Settings")
	FSPLoadoutSettingsSavedSignature OnLoadoutSettingsSaved;

	UPROPERTY(BlueprintAssignable, Category = "Loadout|Settings")
	FSPLoadoutSettingsClosedSignature OnLoadoutSettingsClosed;

	UFUNCTION(BlueprintImplementableEvent, Category = "Loadout|Settings")
	void OnLoadoutValidationFailed(const FText& Message);

protected:
	virtual void NativeConstruct() override;

private:
	template <typename TEnum>
	bool TogglePerk(TArray<TEnum>& Perks, TEnum PerkType, int32 MaxCount);

	UPROPERTY(Transient)
	FSPPlayerLoadout PendingLoadout;
};

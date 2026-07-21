#include "UI/SPLoadoutSettingsWidget.h"

#include "Player/MainMenuPlayerController.h"
#include "Systems/SPPlayerLoadoutSubsystem.h"

void USPLoadoutSettingsWidget::NativeConstruct()
{
	Super::NativeConstruct();
	InitializeFromSavedLoadout();
}

void USPLoadoutSettingsWidget::InitializeFromSavedLoadout()
{
	const UGameInstance* GameInstance = GetGameInstance();
	const USPPlayerLoadoutSubsystem* LoadoutSubsystem = GameInstance
		? GameInstance->GetSubsystem<USPPlayerLoadoutSubsystem>()
		: nullptr;
	PendingLoadout = LoadoutSubsystem ? LoadoutSubsystem->GetCachedLoadout() : FSPPlayerLoadout();
	OnLoadoutSelectionChanged.Broadcast(PendingLoadout);
}

void USPLoadoutSettingsWidget::SelectSurvivorItem(const EConsumableItemType ItemType)
{
	if (!SPPlayerLoadout::IsValidSurvivorItem(ItemType))
	{
		return;
	}

	PendingLoadout.SurvivorItem = ItemType;
	OnLoadoutSelectionChanged.Broadcast(PendingLoadout);
}

bool USPLoadoutSettingsWidget::ToggleSurvivorPerk(const EPerkType PerkType)
{
	if (!SPPlayerLoadout::IsValidSurvivorPerk(PerkType))
	{
		return false;
	}

	return TogglePerk(PendingLoadout.SurvivorPerks, PerkType, SPPlayerLoadout::SurvivorPerkCount);
}

bool USPLoadoutSettingsWidget::ToggleKillerPerk(const EKillerPerkType PerkType)
{
	if (!SPPlayerLoadout::IsValidKillerPerk(PerkType))
	{
		return false;
	}

	return TogglePerk(PendingLoadout.KillerPerks, PerkType, SPPlayerLoadout::KillerPerkCount);
}

bool USPLoadoutSettingsWidget::SaveLoadout()
{
	if (!SPPlayerLoadout::IsSurvivorLoadoutCompleteOnlyItem(PendingLoadout))
	{
		OnLoadoutValidationFailed(FText::FromString(TEXT("기본 사용 아이템 하나를 선택해야 합니다.")));
		return false;
	}
	
	/*
	if (!SPPlayerLoadout::IsSurvivorLoadoutComplete(PendingLoadout))
	{
		OnLoadoutValidationFailed(FText::FromString(TEXT("생존자 아이템 1개와 서로 다른 퍽 2개를 선택해야 합니다.")));
		return false;
	}

	if (!SPPlayerLoadout::IsKillerLoadoutComplete(PendingLoadout))
	{
		OnLoadoutValidationFailed(FText::FromString(TEXT("살인마 퍽을 서로 다르게 2개 선택해야 합니다.")));
		return false;
	}*/


	AMainMenuPlayerController* PlayerController = GetOwningPlayer<AMainMenuPlayerController>();
	if (!PlayerController)
	{
		OnLoadoutValidationFailed(FText::FromString(TEXT("플레이어 컨트롤러를 찾을 수 없습니다.")));
		return false;
	}

	PlayerController->SavePlayerLoadout(PendingLoadout);

	OnLoadoutSettingsSaved.Broadcast(PendingLoadout);
	CloseSettings();
	return true;
}

void USPLoadoutSettingsWidget::CloseSettings()
{
	OnLoadoutSettingsClosed.Broadcast();
	RemoveFromParent();
}

const FSPPlayerLoadout& USPLoadoutSettingsWidget::GetPendingLoadout() const
{
	return PendingLoadout;
}

bool USPLoadoutSettingsWidget::IsSurvivorPerkSelected(const EPerkType PerkType) const
{
	return PendingLoadout.SurvivorPerks.Contains(PerkType);
}

bool USPLoadoutSettingsWidget::IsKillerPerkSelected(const EKillerPerkType PerkType) const
{
	return PendingLoadout.KillerPerks.Contains(PerkType);
}

template <typename TEnum>
bool USPLoadoutSettingsWidget::TogglePerk(TArray<TEnum>& Perks, const TEnum PerkType, const int32 MaxCount)
{
	if (Perks.RemoveSingle(PerkType) > 0)
	{
		OnLoadoutSelectionChanged.Broadcast(PendingLoadout);
		return true;
	}

	if (Perks.Num() >= MaxCount)
	{
		OnLoadoutValidationFailed(FText::FromString(FString::Printf(TEXT("퍽은 최대 %d개까지 선택할 수 있습니다."), MaxCount)));
		return false;
	}

	Perks.Add(PerkType);
	OnLoadoutSelectionChanged.Broadcast(PendingLoadout);
	return true;
}

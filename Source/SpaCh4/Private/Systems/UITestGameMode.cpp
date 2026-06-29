#include "Systems/UITestGameMode.h"

#include "UI/GameHUD.h"
#include "UObject/ConstructorHelpers.h"

AUITestGameMode::AUITestGameMode()
{
	static ConstructorHelpers::FClassFinder<AHUD> HUDClassFinder(
		TEXT("/Game/Blueprints/UI/BP_GameHUD.BP_GameHUD_C"));
	if (HUDClassFinder.Succeeded())
	{
		HUDClass = HUDClassFinder.Class;
	}
	else
	{
		HUDClass = AGameHUD::StaticClass();
	}
}

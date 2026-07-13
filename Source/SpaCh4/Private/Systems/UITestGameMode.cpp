#include "Systems/UITestGameMode.h"

#include "UI/GameHUD.h"

AUITestGameMode::AUITestGameMode()
{
	HUDClass = AGameHUD::StaticClass();
}

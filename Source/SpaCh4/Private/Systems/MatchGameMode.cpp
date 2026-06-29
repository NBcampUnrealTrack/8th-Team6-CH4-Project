#include "Systems/MatchGameMode.h"

#include "Player/SPPlayerController.h"
#include "Systems/MatchGameState.h"

AMatchGameMode::AMatchGameMode()
{
	GameStateClass = AMatchGameState::StaticClass();
	PlayerControllerClass = ASPPlayerController::StaticClass();
}

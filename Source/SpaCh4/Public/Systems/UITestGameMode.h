#pragma once

#include "CoreMinimal.h"
#include "Systems/MatchGameMode.h"
#include "UITestGameMode.generated.h"

/** L_UI_Test 전용 — 이 GameMode를 쓰는 맵에서만 HUD가 표시됩니다. */
UCLASS()
class SPACH4_API AUITestGameMode : public AMatchGameMode
{
	GENERATED_BODY()

public:
	AUITestGameMode();
};

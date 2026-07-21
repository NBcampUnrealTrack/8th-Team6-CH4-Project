#pragma once

#include "CoreMinimal.h"

class USPGameHUDStyleData;
class USPGameResultStyleData;
class USPMainMenuStyleData;
class USPUIFontStyleData;

/** 프로젝트 UI 에셋 경로·기본 스타일 해석 (C++ 위젯에서 LoadObject/ConstructorHelpers 사용 금지) */
namespace SPUIStyleLibrary
{
	SPACH4_API const USPGameHUDStyleData& ResolveGameHUDStyle(const USPGameHUDStyleData* Override = nullptr);
	SPACH4_API const USPGameResultStyleData& ResolveGameResultStyle(const USPGameResultStyleData* Override = nullptr);
	SPACH4_API const USPMainMenuStyleData& ResolveMainMenuStyle(const USPMainMenuStyleData* Override = nullptr);
	SPACH4_API const USPUIFontStyleData& ResolveFontStyle(const USPUIFontStyleData* Override = nullptr);
}

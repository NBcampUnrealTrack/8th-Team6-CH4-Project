# UEBridgeMCP — 로컬 전용 Build 도구

`trigger-live-coding`, `build-and-relaunch` MCP 도구는 **개인 에디터 자동화용**이며 원격 저장소에 올리지 않습니다.

## 왜 분리했는가

- `Tools/Build/` 소스는 `.gitignore`로 제외됩니다.
- `UEBridgeMCPEditor.cpp`에 include/등록을 직접 넣으면, pull한 팀원 PC에서 헤더가 없어 **C1083 빌드 실패**가 납니다.
- 공유 코드는 `__has_include`로 **로컬 등록 파일이 있을 때만** Build 도구를 붙입니다.

## 로컬에서 켜는 방법

1. 아래 템플릿을 복사합니다.

   | 템플릿 | 복사 대상 (gitignore) |
   |--------|------------------------|
   | `Templates/LocalBuildTools/McpLocalBuildToolsRegistration.h` | `Source/UEBridgeMCPEditor/Public/Tools/Build/McpLocalBuildToolsRegistration.h` |
   | `Templates/LocalBuildTools/McpLocalBuildToolsRegistration.cpp` | `Source/UEBridgeMCPEditor/Private/Tools/Build/McpLocalBuildToolsRegistration.cpp` |

2. `TriggerLiveCodingTool`, `BuildAndRelaunchTool` 소스가 같은 `Tools/Build/` 폴더에 있는지 확인합니다.

3. 에디터를 재시작하거나 C++를 리빌드합니다.

등록 파일이 없으면 Build 도구는 등록되지 않고, **팀 공용 빌드는 그대로 통과**합니다.

## 절대 하지 말 것

- `UEBridgeMCPEditor.cpp`에 `Tools/Build/*.h`를 **직접** `#include` 하거나 `RegisterToolClass`로 등록한 채 커밋
- `.gitignore`의 `Tools/Build/` 규칙을 제거한 뒤 Build 소스만 일부 커밋 (참조·파일 불일치 재발)
- 로컬 전용 MCP 변경을 `SpaCh4` 게임 소스 커밋과 섞어 푸시

## 관련 gitignore

```
Plugins/UEBridgeMCP/Source/UEBridgeMCPEditor/Private/Tools/Build/
Plugins/UEBridgeMCP/Source/UEBridgeMCPEditor/Public/Tools/Build/
```

에이전트/개발자는 `AGENTS.md`의 MCP 섹션도 함께 따릅니다.

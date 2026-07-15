# Unreal Cursor Agents
See `.cursor/skills/` for bp_agent, material_agent, level_design_agent.

| BP | /Game/Blueprints/ |
| Maps | /Game/Maps/ |

## UEBridgeMCP (로컬 전용 Build 도구)

`trigger-live-coding`, `build-and-relaunch`는 **개인 에디터 자동화**용입니다. 원격 저장소·팀 빌드에 넣지 않습니다.

### 규칙 (에이전트·개발자 공통)

1. **`UEBridgeMCPEditor.cpp`에 `Tools/Build/*.h` 직접 include / `RegisterToolClass` 커밋 금지**  
   → pull한 PC에서 `TriggerLiveCodingTool.h` 없음 → C1083 빌드 실패.

2. **Build 도구 소스 위치** — `.gitignore` 대상 (커밋하지 않음):
   - `Plugins/UEBridgeMCP/Source/UEBridgeMCPEditor/Private/Tools/Build/`
   - `Plugins/UEBridgeMCP/Source/UEBridgeMCPEditor/Public/Tools/Build/`

3. **로컬에서만 켜기** — `Templates/LocalBuildTools/` 템플릿을 위 `Tools/Build/`로 복사.  
   공유 모듈은 `__has_include("Tools/Build/McpLocalBuildToolsRegistration.h")`로 선택 등록.

4. **상세 절차** — [Plugins/UEBridgeMCP/Docs/LOCAL_BUILD_TOOLS.md](Plugins/UEBridgeMCP/Docs/LOCAL_BUILD_TOOLS.md)

### MCP 작업 시

- 에디터 MCP 연결 확인 (실패 시 사용자에게 에디터 실행 요청)
- Oblivio 경로: `/Game/Blueprints/`, `/Game/Maps/` 등 (`.cursor/rules/unreal-agent-router.mdc` 참고)

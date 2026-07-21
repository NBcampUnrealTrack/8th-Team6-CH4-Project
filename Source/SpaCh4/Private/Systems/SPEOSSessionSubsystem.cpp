#include "Systems/SPEOSSessionSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Online/OnlineSessionNames.h"
#include "OnlineSubsystemUtils.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"

namespace SPEOSSession
{
	const FName MatchSessionKey(TEXT("LAST_DROP_SESSION"));
	const FString MatchSessionValue(TEXT("LastDrop"));
}

void USPEOSSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	// GameInstanceSubsystem의 기본 초기화 과정을 먼저 실행합니다.
	Super::Initialize(Collection);
}

void USPEOSSessionSubsystem::Deinitialize()
{
	// 예약된 세션 재검색 타이머가 종료 이후 실행되지 않도록 해제합니다.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FindSessionsRetryTimerHandle);
	}

	// 로그인 완료 콜백이 남아 있으면 Identity 인터페이스에서 제거합니다.
	if (IOnlineIdentityPtr IdentityInterface = GetIdentityInterface())
	{
		if (LoginCompleteDelegateHandle.IsValid())
		{
			IdentityInterface->ClearOnLoginCompleteDelegate_Handle(LocalUserNum, LoginCompleteDelegateHandle);
		}
	}

	// 세션 관련 비동기 콜백을 모두 해제해 파괴된 객체로 호출되는 상황을 방지합니다.
	if (IOnlineSessionPtr SessionInterface = GetSessionInterface())
	{
		if (FindSessionsCompleteDelegateHandle.IsValid())
		{
			SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
		}
		if (CreateSessionCompleteDelegateHandle.IsValid())
		{
			SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
		}
		if (JoinSessionCompleteDelegateHandle.IsValid())
		{
			SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
		}
		if (StartSessionCompleteDelegateHandle.IsValid())
		{
			SessionInterface->ClearOnStartSessionCompleteDelegate_Handle(StartSessionCompleteDelegateHandle);
		}
		if (EndSessionCompleteDelegateHandle.IsValid())
		{
			SessionInterface->ClearOnEndSessionCompleteDelegate_Handle(EndSessionCompleteDelegateHandle);
		}
		if (DestroySessionCompleteDelegateHandle.IsValid())
		{
			SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
		}
	}

	// 자체 자원 정리 후 부모 서브시스템의 종료 처리를 실행합니다.
	Super::Deinitialize();
}

void USPEOSSessionSubsystem::Login()
{
	// EOS Identity 인터페이스를 가져오지 못하면 로그인 요청을 즉시 실패 처리합니다.
	IOnlineIdentityPtr IdentityInterface = GetIdentityInterface();
	if (!IdentityInterface.IsValid())
	{
		OnLoginCompleted.Broadcast(false, TEXT("Online identity interface is not available."));
		return;
	}

	// 이미 로그인된 사용자는 계정 정보만 갱신하고 성공 결과를 전달합니다.
	if (IdentityInterface->GetLoginStatus(LocalUserNum) == ELoginStatus::LoggedIn)
	{
		CacheLocalAccountInfo();
		OnLoginCompleted.Broadcast(true, TEXT("Already logged in."));
		return;
	}

	// 중복 콜백을 막기 위해 기존 로그인 완료 델리게이트를 먼저 제거합니다.
	if (LoginCompleteDelegateHandle.IsValid())
	{
		IdentityInterface->ClearOnLoginCompleteDelegate_Handle(LocalUserNum, LoginCompleteDelegateHandle);
	}

	// 새 로그인 완료 델리게이트를 등록해 비동기 EOS 로그인 결과를 수신합니다.
	LoginCompleteDelegateHandle = IdentityInterface->AddOnLoginCompleteDelegate_Handle(
		LocalUserNum,
		FOnLoginCompleteDelegate::CreateUObject(this, &USPEOSSessionSubsystem::HandleLoginComplete));

	// 외부 브라우저의 EOS 계정 포털을 사용하는 로그인 자격 정보를 구성합니다.
	FOnlineAccountCredentials Credentials;
	Credentials.Type = TEXT("accountportal");

	// 로그인 요청 자체를 시작하지 못한 경우 델리게이트를 정리하고 실패를 알립니다.
	if (!IdentityInterface->Login(LocalUserNum, Credentials))
	{
		IdentityInterface->ClearOnLoginCompleteDelegate_Handle(LocalUserNum, LoginCompleteDelegateHandle);
		LoginCompleteDelegateHandle.Reset();
		OnLoginCompleted.Broadcast(false, TEXT("Failed to start EOS login."));
	}
}

void USPEOSSessionSubsystem::StartMatchmaking(ELobbyPlayerRole SelectedRole, const FString& MainMenuLevelPath)
{
	// 역할이 선택되지 않은 요청은 세션 검색 전에 차단합니다.
	if (SelectedRole == ELobbyPlayerRole::None)
	{
		BroadcastMatchmakingStatus(false, TEXT("매치메이킹 역할이 선택되지 않았습니다."));
		return;
	}

	// 이후 비동기 단계에서 사용할 역할과 레벨 경로, 매칭 상태를 저장합니다.
	PendingMatchmakingRole = SelectedRole;
	PendingMatchmakingLevelPath = ResolveMainMenuLevelPath(MainMenuLevelPath);
	PendingMainMenuLevelPath = PendingMatchmakingLevelPath;
	bStartMatchmakingAfterLogin = true;
	bIsMatchmakingActive = true;
	FindSessionAttemptCount = 0;
	bCreateSessionIfSearchStillEmpty = false;

	// 이전 매칭에서 예약된 재검색 타이머가 있으면 초기화합니다.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FindSessionsRetryTimerHandle);
	}

	// 이동할 메인 메뉴 경로가 없으면 매칭을 진행하지 않고 오류를 전달합니다.
	if (PendingMatchmakingLevelPath.IsEmpty())
	{
		bIsMatchmakingActive = false;
		BroadcastMatchmakingStatus(false, TEXT("메인 메뉴 레벨 경로가 비어 있습니다."));
		return;
	}

	// 로그인 전이라면 매칭 요청을 유지한 채 EOS 로그인을 먼저 시작합니다.
	if (!IsLoggedIn())
	{
		BroadcastMatchmakingStatus(true, TEXT("EOS 로그인 중입니다."));
		Login();
		return;
	}

	// 로그인된 사용자의 정보를 갱신한 후 첫 세션 검색을 시작합니다.
	CacheLocalAccountInfo();
	BeginFindSessions();
}

void USPEOSSessionSubsystem::CancelMatchmaking()
{
	// 진행 상태와 후속 작업 플래그를 취소 상태로 초기화합니다.
	bStartMatchmakingAfterLogin = false;
	bCreateSessionIfSearchStillEmpty = false;
	bIsMatchmakingActive = false;
	PendingMatchmakingRole = ELobbyPlayerRole::None;
	FindSessionAttemptCount = 0;

	// 예약된 검색 재시도를 취소해 추가 온라인 요청을 막습니다.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FindSessionsRetryTimerHandle);
	}

	// 진행 중일 수 있는 검색·참가·생성 완료 델리게이트를 해제합니다.
	if (IOnlineSessionPtr SessionInterface = GetSessionInterface())
	{
		if (FindSessionsCompleteDelegateHandle.IsValid())
		{
			SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
			FindSessionsCompleteDelegateHandle.Reset();
		}
		if (JoinSessionCompleteDelegateHandle.IsValid())
		{
			SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
			JoinSessionCompleteDelegateHandle.Reset();
		}
		if (CreateSessionCompleteDelegateHandle.IsValid())
		{
			SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
			CreateSessionCompleteDelegateHandle.Reset();
		}
	}

	// UI에 매칭 취소 상태를 즉시 전달합니다.
	BroadcastMatchmakingStatus(true, TEXT("매치메이킹을 취소했습니다."));

	// 로컬에 등록된 세션이 있으면 제거하고, 없으면 종료 완료만 알립니다.
	IOnlineSessionPtr SessionInterface = GetSessionInterface();
	if (SessionInterface.IsValid() && SessionInterface->GetNamedSession(NAME_GameSession))
	{
		DestroyCurrentSession(true, false);
	}
	else
	{
		OnSessionEnded.Broadcast(true, TEXT("매치메이킹을 취소했습니다."));
	}
}

void USPEOSSessionSubsystem::StartSession()
{
	// 게임 시작 시 이전 매칭 검색 상태와 콜백을 정리합니다.
	ResetMatchmakingState();

	// 시작할 세션이 실제로 존재하는지 확인합니다.
	IOnlineSessionPtr SessionInterface = GetSessionInterface();
	if (!SessionInterface.IsValid() || !SessionInterface->GetNamedSession(NAME_GameSession))
	{
		return;
	}

	// 이미 시작 중이거나 진행 중인 세션에는 중복 요청을 보내지 않습니다.
	const EOnlineSessionState::Type SessionState = SessionInterface->GetSessionState(NAME_GameSession);
	if (SessionState == EOnlineSessionState::InProgress || SessionState == EOnlineSessionState::Starting)
	{
		return;
	}

	// EOS 세션은 대기 상태에서만 시작할 수 있으므로 다른 상태는 경고 후 중단합니다.
	if (SessionState != EOnlineSessionState::Pending)
	{
		UE_LOG(LogTemp, Warning, TEXT("EOS session cannot start from state %d."), static_cast<int32>(SessionState));
		return;
	}

	// 이전 시작 콜백을 제거한 뒤 이번 요청의 완료 델리게이트를 등록합니다.
	if (StartSessionCompleteDelegateHandle.IsValid())
	{
		SessionInterface->ClearOnStartSessionCompleteDelegate_Handle(StartSessionCompleteDelegateHandle);
	}

	StartSessionCompleteDelegateHandle = SessionInterface->AddOnStartSessionCompleteDelegate_Handle(
		FOnStartSessionCompleteDelegate::CreateUObject(this, &USPEOSSessionSubsystem::HandleStartSessionComplete));

	// 세션 시작 요청이 접수되지 않으면 등록한 델리게이트를 즉시 정리합니다.
	if (!SessionInterface->StartSession(NAME_GameSession))
	{
		SessionInterface->ClearOnStartSessionCompleteDelegate_Handle(StartSessionCompleteDelegateHandle);
		StartSessionCompleteDelegateHandle.Reset();
		UE_LOG(LogTemp, Warning, TEXT("Failed to start EOS session."));
	}
}

void USPEOSSessionSubsystem::EndSession()
{
	// 게임 종료 시 남아 있는 매칭 검색 상태를 먼저 정리합니다.
	ResetMatchmakingState();

	// 종료할 세션이 없으면 이미 정리된 것으로 간주해 성공을 알립니다.
	IOnlineSessionPtr SessionInterface = GetSessionInterface();
	if (!SessionInterface.IsValid() || !SessionInterface->GetNamedSession(NAME_GameSession))
	{
		OnSessionEnded.Broadcast(true, TEXT("No active session."));
		return;
	}

	// 이미 끝난 세션에는 EOS 종료 요청을 다시 보내지 않습니다.
	const EOnlineSessionState::Type SessionState = SessionInterface->GetSessionState(NAME_GameSession);
	if (SessionState == EOnlineSessionState::Ended || SessionState == EOnlineSessionState::NoSession)
	{
		OnSessionEnded.Broadcast(true, TEXT("EOS session is already ended."));
		return;
	}

	// 종료 요청이 진행 중이면 완료 콜백을 기다립니다.
	if (bEndSessionInProgress || SessionState == EOnlineSessionState::Ending)
	{
		return;
	}

	// 기존 종료 콜백을 교체하고 현재 종료 요청의 완료 델리게이트를 등록합니다.
	if (EndSessionCompleteDelegateHandle.IsValid())
	{
		SessionInterface->ClearOnEndSessionCompleteDelegate_Handle(EndSessionCompleteDelegateHandle);
	}

	EndSessionCompleteDelegateHandle = SessionInterface->AddOnEndSessionCompleteDelegate_Handle(
		FOnEndSessionCompleteDelegate::CreateUObject(this, &USPEOSSessionSubsystem::HandleEndSessionComplete));
	bEndSessionInProgress = true;

	// 세션 종료 요청을 시작하지 못하면 진행 상태와 델리게이트를 원래대로 되돌립니다.
	if (!SessionInterface->EndSession(NAME_GameSession))
	{
		SessionInterface->ClearOnEndSessionCompleteDelegate_Handle(EndSessionCompleteDelegateHandle);
		EndSessionCompleteDelegateHandle.Reset();
		bEndSessionInProgress = false;
		OnSessionEnded.Broadcast(false, TEXT("Failed to start EOS session end."));
	}
}

void USPEOSSessionSubsystem::ReturnToMainMenu(const FString& MainMenuLevelPath)
{
	// 매칭 상태를 초기화하고 세션 제거 후 이동할 메인 메뉴 경로를 저장합니다.
	ResetMatchmakingState();
	PendingMainMenuLevelPath = MainMenuLevelPath;

	// 세션 종료가 진행 중이면 완료된 다음 제거하도록 후속 작업만 예약합니다.
	if (bEndSessionInProgress)
	{
		bDestroySessionAfterEnd = true;
		return;
	}

	// 종료 대기 상태가 아니라면 현재 세션 제거와 메뉴 이동을 바로 시작합니다.
	DestroyCurrentSession(true, false);
}

bool USPEOSSessionSubsystem::IsLoggedIn() const
{
	// Identity 인터페이스와 로컬 사용자의 로그인 상태를 함께 검사합니다.
	IOnlineIdentityPtr IdentityInterface = GetIdentityInterface();
	return IdentityInterface.IsValid() && IdentityInterface->GetLoginStatus(LocalUserNum) == ELoginStatus::LoggedIn;
}

FString USPEOSSessionSubsystem::GetCachedNickname() const
{
	// 로그인 완료 시 저장된 닉네임을 외부에 제공합니다.
	return CachedNickname;
}

ELobbyPlayerRole USPEOSSessionSubsystem::GetPendingMatchmakingRole() const
{
	// 비동기 매칭 단계에서 유지 중인 선택 역할을 외부에 제공합니다.
	return PendingMatchmakingRole;
}

bool USPEOSSessionSubsystem::IsMatchmakingActive() const
{
	// UI가 매칭 진행 여부를 판단할 수 있도록 현재 플래그를 반환합니다.
	return bIsMatchmakingActive;
}

IOnlineIdentityPtr USPEOSSessionSubsystem::GetIdentityInterface() const
{
	// 현재 월드의 OnlineSubsystem에서 사용자 인증 인터페이스를 조회합니다.
	IOnlineSubsystem* OnlineSubsystem = Online::GetSubsystem(this->GetWorld());
	return OnlineSubsystem ? OnlineSubsystem->GetIdentityInterface() : nullptr;
}

IOnlineSessionPtr USPEOSSessionSubsystem::GetSessionInterface() const
{
	// 현재 월드의 OnlineSubsystem에서 세션 관리 인터페이스를 조회합니다.
	IOnlineSubsystem* OnlineSubsystem = Online::GetSubsystem(this->GetWorld());
	return OnlineSubsystem ? OnlineSubsystem->GetSessionInterface() : nullptr;
}

void USPEOSSessionSubsystem::CacheLocalAccountInfo()
{
	// Identity 인터페이스가 없으면 기존 캐시를 유지하고 종료합니다.
	IOnlineIdentityPtr IdentityInterface = GetIdentityInterface();
	if (!IdentityInterface.IsValid())
	{
		return;
	}

	// EOS 계정에 설정된 플레이어 닉네임을 우선 저장합니다.
	CachedNickname = IdentityInterface->GetPlayerNickname(LocalUserNum);
	if (!CachedNickname.IsEmpty())
	{
		return;
	}

	// 닉네임이 없으면 고유 ID를 사용하고, ID도 없으면 기본 이름을 사용합니다.
	const TSharedPtr<const FUniqueNetId> UserId = IdentityInterface->GetUniquePlayerId(LocalUserNum);
	CachedNickname = UserId.IsValid() ? UserId->ToString() : TEXT("Player");
}

void USPEOSSessionSubsystem::BeginFindSessions()
{
	// 세션 인터페이스를 사용할 수 없으면 검색을 시작하지 않습니다.
	IOnlineSessionPtr SessionInterface = GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		BroadcastMatchmakingStatus(false, TEXT("Online session interface is not available."));
		return;
	}

	// 중복 검색 완료 콜백을 방지하기 위해 이전 델리게이트를 제거합니다.
	if (FindSessionsCompleteDelegateHandle.IsValid())
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
	}

	// EOS 로비와 프로젝트 고유 키를 조건으로 최대 50개의 온라인 세션을 검색합니다.
	SessionSearch = MakeShared<FOnlineSessionSearch>();
	SessionSearch->MaxSearchResults = 50;
	SessionSearch->bIsLanQuery = false;
	SessionSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
	SessionSearch->QuerySettings.Set(SPEOSSession::MatchSessionKey, SPEOSSession::MatchSessionValue, EOnlineComparisonOp::Equals);

	// 비동기 검색 결과를 받을 완료 델리게이트를 등록합니다.
	FindSessionsCompleteDelegateHandle = SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(
		FOnFindSessionsCompleteDelegate::CreateUObject(this, &USPEOSSessionSubsystem::HandleFindSessionsComplete));

	// 검색 횟수와 UI 상태를 갱신한 후 EOS 세션 검색을 요청합니다.
	++FindSessionAttemptCount;
	BroadcastMatchmakingStatus(true, TEXT("세션 찾는중"));
	if (!SessionInterface->FindSessions(LocalUserNum, SessionSearch.ToSharedRef()))
	{
		// 요청 접수에 실패하면 콜백을 정리하고 재시도 정책으로 넘깁니다.
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
		FindSessionsCompleteDelegateHandle.Reset();
		HandleFindSessionsRetry();
	}
}

void USPEOSSessionSubsystem::HandleFindSessionsRetry()
{
	// 최대 검색 횟수에 도달하면 충돌 방지를 위한 마지막 지연 검색을 한 번 예약합니다.
	if (FindSessionAttemptCount >= MaxFindSessionAttempts)
	{
		if (!bCreateSessionIfSearchStillEmpty)
		{
			bCreateSessionIfSearchStillEmpty = true;

			// 마지막 검색을 예약할 월드가 없으면 매칭을 실패 처리합니다.
			UWorld* World = GetWorld();
			if (!World)
			{
				bIsMatchmakingActive = false;
				BroadcastMatchmakingStatus(false, TEXT("최종 세션 검색을 위한 월드가 없습니다."));
				return;
			}

			// 여러 클라이언트가 동시에 호스트가 되는 상황을 줄이도록 무작위 지연을 적용합니다.
			const float BackoffDelay = FMath::FRandRange(MinCreateSessionBackoff, MaxCreateSessionBackoff);
			BroadcastMatchmakingStatus(true, TEXT("세션 찾는중"));
			World->GetTimerManager().SetTimer(FindSessionsRetryTimerHandle, this, &USPEOSSessionSubsystem::BeginFindSessions, BackoffDelay, false);
			return;
		}

		// 마지막 검색도 비어 있으면 현재 클라이언트가 새 호스트 세션을 생성합니다.
		bCreateSessionIfSearchStillEmpty = false;
		BroadcastMatchmakingStatus(true, TEXT("세션이 없어 호스트 세션을 생성합니다."));
		BeginCreateSession();
		return;
	}

	// 일반 재검색을 예약할 월드가 있는지 확인합니다.
	UWorld* World = GetWorld();
	if (!World)
	{
		bIsMatchmakingActive = false;
		BroadcastMatchmakingStatus(false, TEXT("세션 재검색을 위한 월드가 없습니다."));
		return;
	}

	// 고정된 재시도 간격 후 다음 세션 검색을 시작합니다.
	BroadcastMatchmakingStatus(true, TEXT("세션 찾는중"));
	World->GetTimerManager().SetTimer(FindSessionsRetryTimerHandle, this, &USPEOSSessionSubsystem::BeginFindSessions, FindSessionRetryDelay, false);
}

void USPEOSSessionSubsystem::BeginCreateSession()
{
	// 세션 인터페이스를 사용할 수 없으면 생성 요청을 실패 처리합니다.
	IOnlineSessionPtr SessionInterface = GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		BroadcastMatchmakingStatus(false, TEXT("Online session interface is not available."));
		return;
	}

	// 같은 이름의 로컬 세션이 남아 있으면 제거한 뒤 생성을 다시 이어갑니다.
	if (SessionInterface->GetNamedSession(NAME_GameSession))
	{
		DestroyCurrentSession(false, true);
		return;
	}

	// 4인 P2P 로비로 검색·초대·중도 참가를 허용하는 EOS 세션 설정을 구성합니다.
	FOnlineSessionSettings SessionSettings;
	SessionSettings.NumPublicConnections = MaxSessionPlayers;
	SessionSettings.bIsDedicated = false;
	SessionSettings.bIsLANMatch = false;
	SessionSettings.bShouldAdvertise = true;
	SessionSettings.bAllowInvites = true;
	SessionSettings.bAllowJoinInProgress = true;
	SessionSettings.bAllowJoinViaPresence = true;
	SessionSettings.bUsesPresence = true;
	SessionSettings.bUseLobbiesIfAvailable = true;
	SessionSettings.BuildUniqueId = 1;
	SessionSettings.Set(SETTING_MAPNAME, PendingMatchmakingLevelPath, EOnlineDataAdvertisementType::ViaOnlineService);
	SessionSettings.Set(SPEOSSession::MatchSessionKey, SPEOSSession::MatchSessionValue, EOnlineDataAdvertisementType::ViaOnlineService);

	// 이전 생성 콜백을 제거해 같은 완료 함수가 중복 호출되지 않도록 합니다.
	if (CreateSessionCompleteDelegateHandle.IsValid())
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
	}

	// 비동기 생성 결과를 받을 완료 델리게이트를 등록합니다.
	CreateSessionCompleteDelegateHandle = SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(
		FOnCreateSessionCompleteDelegate::CreateUObject(this, &USPEOSSessionSubsystem::HandleCreateSessionComplete));

	// UI 상태를 갱신하고 로컬 사용자를 소유자로 하는 EOS 세션 생성을 요청합니다.
	BroadcastMatchmakingStatus(true, TEXT("세션 생성중"));
	if (!SessionInterface->CreateSession(LocalUserNum, NAME_GameSession, SessionSettings))
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
		CreateSessionCompleteDelegateHandle.Reset();
		bIsMatchmakingActive = false;
		BroadcastMatchmakingStatus(false, TEXT("Failed to start session creation."));
	}
}

void USPEOSSessionSubsystem::BeginJoinSession(const FOnlineSessionSearchResult& SearchResult)
{
	// 세션 인터페이스를 사용할 수 없으면 참가 요청을 실패 처리합니다.
	IOnlineSessionPtr SessionInterface = GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		BroadcastMatchmakingStatus(false, TEXT("Online session interface is not available."));
		return;
	}

	// 이전 참가 완료 콜백을 제거해 중복 처리 가능성을 없앱니다.
	if (JoinSessionCompleteDelegateHandle.IsValid())
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
	}

	// 비동기 참가 결과를 받을 완료 델리게이트를 등록합니다.
	JoinSessionCompleteDelegateHandle = SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(
		FOnJoinSessionCompleteDelegate::CreateUObject(this, &USPEOSSessionSubsystem::HandleJoinSessionComplete));

	// UI 상태를 갱신하고 선택한 검색 결과의 세션 참가를 요청합니다.
	BroadcastMatchmakingStatus(true, TEXT("세션 참가중"));
	if (!SessionInterface->JoinSession(LocalUserNum, NAME_GameSession, SearchResult))
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
		JoinSessionCompleteDelegateHandle.Reset();
		bIsMatchmakingActive = false;
		BroadcastMatchmakingStatus(false, TEXT("Failed to start session join."));
	}
}

void USPEOSSessionSubsystem::DestroyCurrentSession(bool bTravelToMainMenu, bool bCreateAfterDestroy)
{
	// 제거할 세션이 없으면 요청된 메뉴 이동 또는 종료 알림을 즉시 수행합니다.
	IOnlineSessionPtr SessionInterface = GetSessionInterface();
	if (!SessionInterface.IsValid() || !SessionInterface->GetNamedSession(NAME_GameSession))
	{
		if (bTravelToMainMenu)
		{
			OpenPendingMainMenuLevel();
		}
		else
		{
			OnSessionEnded.Broadcast(true, TEXT("No active session."));
		}
		return;
	}

	// 세션 제거 완료 후 수행할 메뉴 이동과 세션 재생성 여부를 저장합니다.
	bTravelToMainMenuAfterDestroy = bTravelToMainMenu;
	bCreateSessionAfterDestroy = bCreateAfterDestroy;

	// 이전 제거 완료 콜백을 정리한 뒤 이번 요청의 델리게이트를 등록합니다.
	if (DestroySessionCompleteDelegateHandle.IsValid())
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
	}

	DestroySessionCompleteDelegateHandle = SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(
		FOnDestroySessionCompleteDelegate::CreateUObject(this, &USPEOSSessionSubsystem::HandleDestroySessionComplete));

	// EOS 세션 제거 요청을 시작하지 못하면 델리게이트를 해제하고 실패를 알립니다.
	if (!SessionInterface->DestroySession(NAME_GameSession))
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
		DestroySessionCompleteDelegateHandle.Reset();
		OnSessionEnded.Broadcast(false, TEXT("Failed to start session destroy."));
	}
}

void USPEOSSessionSubsystem::ResetMatchmakingState()
{
	// 매칭 진행 여부와 후속 작업 플래그, 역할 및 검색 데이터를 기본값으로 되돌립니다.
	bStartMatchmakingAfterLogin = false;
	bCreateSessionIfSearchStillEmpty = false;
	bIsMatchmakingActive = false;
	bTravelToMainMenuAfterDestroy = false;
	bCreateSessionAfterDestroy = false;
	PendingMatchmakingRole = ELobbyPlayerRole::None;
	PendingMatchmakingLevelPath.Reset();
	FindSessionAttemptCount = 0;
	SessionSearch.Reset();

	// 예약된 세션 재검색 타이머를 제거합니다.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FindSessionsRetryTimerHandle);
	}

	// 세션 인터페이스가 없으면 상태 값 초기화만으로 종료합니다.
	IOnlineSessionPtr SessionInterface = GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		return;
	}

	// 매칭 단계에서 등록한 검색·참가·생성 완료 델리게이트를 모두 해제합니다.
	if (FindSessionsCompleteDelegateHandle.IsValid())
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
		FindSessionsCompleteDelegateHandle.Reset();
	}
	if (JoinSessionCompleteDelegateHandle.IsValid())
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
		JoinSessionCompleteDelegateHandle.Reset();
	}
	if (CreateSessionCompleteDelegateHandle.IsValid())
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
		CreateSessionCompleteDelegateHandle.Reset();
	}
}

void USPEOSSessionSubsystem::OpenMainMenuAsListenServer() const
{
	// 호스트가 열 월드와 레벨 경로가 모두 유효한지 확인합니다.
	UWorld* World = GetWorld();
	if (!World || PendingMatchmakingLevelPath.IsEmpty())
	{
		return;
	}

	// 세션 참가자가 접속할 수 있도록 메인 메뉴 레벨을 Listen Server로 엽니다.
	UGameplayStatics::OpenLevel(World, FName(*PendingMatchmakingLevelPath), true, TEXT("listen"));
}

void USPEOSSessionSubsystem::OpenPendingMainMenuLevel()
{
	// 세션 제거 후 이동할 메인 메뉴 경로가 없으면 실패를 알립니다.
	if (PendingMainMenuLevelPath.IsEmpty())
	{
		OnSessionEnded.Broadcast(false, TEXT("Main menu level path is empty."));
		return;
	}

	// 레벨 이동에 사용할 월드가 유효한지 확인합니다.
	UWorld* World = GetWorld();
	if (!World)
	{
		OnSessionEnded.Broadcast(false, TEXT("World is not available."));
		return;
	}

	// 네트워크 세션이 없는 일반 메인 메뉴 레벨로 이동합니다.
	UGameplayStatics::OpenLevel(World, FName(*PendingMainMenuLevelPath));
}

void USPEOSSessionSubsystem::BroadcastMatchmakingStatus(bool bWasSuccessful, const FString& StatusMessage) const
{
	// 성공 여부와 상태 문구를 매칭 UI 및 기타 구독 객체에 전달합니다.
	OnMatchmakingStatusChanged.Broadcast(bWasSuccessful, StatusMessage);
}

FString USPEOSSessionSubsystem::ResolveMainMenuLevelPath(const FString& MainMenuLevelPath) const
{
	// 호출자가 경로를 전달했다면 별도 변환 없이 그대로 사용합니다.
	if (!MainMenuLevelPath.IsEmpty())
	{
		return MainMenuLevelPath;
	}

	// 빈 경로는 현재 로드된 월드의 패키지 경로로 보완합니다.
	const UWorld* World = GetWorld();
	return World && World->GetOutermost() ? World->GetOutermost()->GetName() : FString();
}

void USPEOSSessionSubsystem::HandleLoginComplete(int32 LocalUserNumber, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error)
{
	// 한 번만 사용되는 로그인 완료 델리게이트를 Identity 인터페이스에서 해제합니다.
	IOnlineIdentityPtr IdentityInterface = GetIdentityInterface();
	if (IdentityInterface.IsValid())
	{
		IdentityInterface->ClearOnLoginCompleteDelegate_Handle(LocalUserNumber, LoginCompleteDelegateHandle);
	}
	LoginCompleteDelegateHandle.Reset();

	// 로그인 실패 시 원인 메시지를 전달하고 대기 중인 매칭 상태를 종료합니다.
	if (!bWasSuccessful)
	{
		const FString Message = Error.IsEmpty() ? TEXT("EOS login failed.") : Error;
		OnLoginCompleted.Broadcast(false, Message);
		BroadcastMatchmakingStatus(false, Message);
		bStartMatchmakingAfterLogin = false;
		bIsMatchmakingActive = false;
		return;
	}

	// 로그인 성공 시 계정 정보를 저장하고 UI에 완료 결과를 알립니다.
	CacheLocalAccountInfo();
	OnLoginCompleted.Broadcast(true, CachedNickname);

	// 로그인 때문에 보류했던 매칭 요청이 있으면 세션 검색부터 이어서 실행합니다.
	if (bStartMatchmakingAfterLogin)
	{
		BeginFindSessions();
	}
}

void USPEOSSessionSubsystem::HandleFindSessionsComplete(bool bWasSuccessful)
{
	// 검색 완료 델리게이트를 해제해 다음 검색에서 새로 등록할 수 있게 합니다.
	IOnlineSessionPtr SessionInterface = GetSessionInterface();
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
	}
	FindSessionsCompleteDelegateHandle.Reset();

	// 검색 실패 또는 결과 객체 손실 시 재검색 정책으로 처리를 넘깁니다.
	if (!bWasSuccessful || !SessionSearch.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("EOS FindSessions failed. Attempt=%d/%d"), FindSessionAttemptCount, MaxFindSessionAttempts);
		HandleFindSessionsRetry();
		return;
	}

	// 검색 횟수와 전체 결과 수를 로그에 기록해 매칭 과정을 추적합니다.
	UE_LOG(LogTemp, Warning, TEXT("EOS FindSessions complete. Attempt=%d/%d Results=%d"), FindSessionAttemptCount, MaxFindSessionAttempts, SessionSearch->SearchResults.Num());

	// 검색 결과를 순회하며 프로젝트 세션 키와 공개 빈자리가 모두 유효한 방을 찾습니다.
	for (const FOnlineSessionSearchResult& SearchResult : SessionSearch->SearchResults)
	{
		FString SessionType;
		SearchResult.Session.SessionSettings.Get(SPEOSSession::MatchSessionKey, SessionType);

		UE_LOG(LogTemp, Warning, TEXT("EOS Found Session Type=%s Open=%d Owner=%s"),
			*SessionType,
			SearchResult.Session.NumOpenPublicConnections,
			*SearchResult.Session.OwningUserName);

		// 첫 번째 참가 가능한 프로젝트 세션을 선택해 비동기 참가를 시작합니다.
		if (SessionType == SPEOSSession::MatchSessionValue && SearchResult.Session.NumOpenPublicConnections > 0)
		{
			bCreateSessionIfSearchStillEmpty = false;
			BeginJoinSession(SearchResult);
			return;
		}
	}

	// 참가할 세션이 하나도 없으면 재검색 또는 호스트 생성 단계로 진행합니다.
	HandleFindSessionsRetry();
}

void USPEOSSessionSubsystem::HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	// 세션 생성 완료 델리게이트를 해제하고 로그인 후 매칭 대기 플래그를 종료합니다.
	IOnlineSessionPtr SessionInterface = GetSessionInterface();
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
	}
	CreateSessionCompleteDelegateHandle.Reset();
	bStartMatchmakingAfterLogin = false;

	// 생성 실패 시 매칭 상태를 종료하고 UI에 실패 메시지를 전달합니다.
	if (!bWasSuccessful)
	{
		bIsMatchmakingActive = false;
		BroadcastMatchmakingStatus(false, TEXT("EOS 세션 생성에 실패했습니다."));
		return;
	}

	// 생성 성공 시 상태를 알리고 참가자가 접속할 메인 메뉴 Listen Server를 엽니다.
	BroadcastMatchmakingStatus(true, TEXT("세션 생성 완료. 메인 메뉴 대기방을 엽니다."));
	OpenMainMenuAsListenServer();
}

void USPEOSSessionSubsystem::HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	// 세션 참가 완료 델리게이트를 해제하고 로그인 후 매칭 대기 플래그를 종료합니다.
	IOnlineSessionPtr SessionInterface = GetSessionInterface();
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
	}
	JoinSessionCompleteDelegateHandle.Reset();
	bStartMatchmakingAfterLogin = false;

	// EOS 세션 참가 결과가 실패했거나 인터페이스가 사라졌으면 매칭을 종료합니다.
	if (Result != EOnJoinSessionCompleteResult::Success || !SessionInterface.IsValid())
	{
		bIsMatchmakingActive = false;
		BroadcastMatchmakingStatus(false, TEXT("EOS 세션 참가에 실패했습니다."));
		return;
	}

	// 참가한 세션에서 호스트의 EOS P2P 접속 주소를 해석합니다.
	FString ConnectString;
	if (!SessionInterface->GetResolvedConnectString(NAME_GameSession, ConnectString) || ConnectString.IsEmpty())
	{
		bIsMatchmakingActive = false;
		BroadcastMatchmakingStatus(false, TEXT("EOS 세션 접속 주소를 가져오지 못했습니다."));
		return;
	}

	// 실제 네트워크 이동을 수행할 로컬 플레이어 컨트롤러를 조회합니다.
	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, LocalUserNum);
	if (!PlayerController)
	{
		bIsMatchmakingActive = false;
		BroadcastMatchmakingStatus(false, TEXT("로컬 플레이어 컨트롤러가 없습니다."));
		return;
	}

	// UI에 입장 상태를 알린 뒤 해석된 주소로 절대 경로 ClientTravel을 실행합니다.
	BroadcastMatchmakingStatus(true, TEXT("게임 입장중"));
	PlayerController->ClientTravel(ConnectString, TRAVEL_Absolute);
}

void USPEOSSessionSubsystem::HandleStartSessionComplete(FName SessionName, bool bWasSuccessful)
{
	// 세션 시작 완료 델리게이트를 정리해 중복 콜백을 방지합니다.
	IOnlineSessionPtr SessionInterface = GetSessionInterface();
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnStartSessionCompleteDelegate_Handle(StartSessionCompleteDelegateHandle);
	}
	StartSessionCompleteDelegateHandle.Reset();

	// 시작 성공 여부를 로그에 남겨 호스트의 세션 상태 전환을 추적합니다.
	UE_LOG(LogTemp, Log, TEXT("EOS session start completed: %s"), bWasSuccessful ? TEXT("Success") : TEXT("Failed"));
}

void USPEOSSessionSubsystem::HandleEndSessionComplete(FName SessionName, bool bWasSuccessful)
{
	// 세션 종료 완료 델리게이트와 진행 중 플래그를 정리합니다.
	IOnlineSessionPtr SessionInterface = GetSessionInterface();
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnEndSessionCompleteDelegate_Handle(EndSessionCompleteDelegateHandle);
	}
	EndSessionCompleteDelegateHandle.Reset();
	bEndSessionInProgress = false;

	// 세션 종료 결과를 UI와 게임 흐름을 구독하는 객체에 전달합니다.
	OnSessionEnded.Broadcast(
		bWasSuccessful,
		bWasSuccessful ? TEXT("EOS session ended. Match level remains loaded.") : TEXT("EOS session end failed."));

	// 메인 메뉴 복귀가 예약되어 있다면 세션 제거와 레벨 이동을 이어서 수행합니다.
	if (bDestroySessionAfterEnd)
	{
		bDestroySessionAfterEnd = false;
		DestroyCurrentSession(true, false);
	}
}

void USPEOSSessionSubsystem::HandleDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
	// 세션 제거 완료 델리게이트를 정리합니다.
	IOnlineSessionPtr SessionInterface = GetSessionInterface();
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
	}
	DestroySessionCompleteDelegateHandle.Reset();

	// 제거 전 예약한 후속 작업을 지역 변수로 옮기고 멤버 플래그를 초기화합니다.
	const bool bShouldTravelToMainMenu = bTravelToMainMenuAfterDestroy;
	const bool bShouldCreateSession = bCreateSessionAfterDestroy;
	bTravelToMainMenuAfterDestroy = false;
	bCreateSessionAfterDestroy = false;

	// 제거 실패 시 후속 작업을 실행하지 않고 실패 결과만 전달합니다.
	if (!bWasSuccessful)
	{
		OnSessionEnded.Broadcast(false, TEXT("EOS session destroy failed."));
		return;
	}

	// 정상 제거 결과를 알린 뒤 예약된 작업을 우선순위에 따라 처리합니다.
	OnSessionEnded.Broadcast(true, TEXT("EOS session ended."));

	// 기존 세션 정리 후 생성 요청이었다면 새 호스트 세션 생성을 이어갑니다.
	if (bShouldCreateSession)
	{
		BeginCreateSession();
		return;
	}

	// 메뉴 복귀 요청이었다면 저장된 메인 메뉴 레벨로 이동합니다.
	if (bShouldTravelToMainMenu)
	{
		OpenPendingMainMenuLevel();
	}
}

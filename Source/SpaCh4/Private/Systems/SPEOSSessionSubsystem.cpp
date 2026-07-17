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
	Super::Initialize(Collection);
}

void USPEOSSessionSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FindSessionsRetryTimerHandle);
	}

	if (IOnlineIdentityPtr IdentityInterface = GetIdentityInterface())
	{
		if (LoginCompleteDelegateHandle.IsValid())
		{
			IdentityInterface->ClearOnLoginCompleteDelegate_Handle(LocalUserNum, LoginCompleteDelegateHandle);
		}
	}

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

	Super::Deinitialize();
}

void USPEOSSessionSubsystem::Login()
{
	IOnlineIdentityPtr IdentityInterface = GetIdentityInterface();
	if (!IdentityInterface.IsValid())
	{
		OnLoginCompleted.Broadcast(false, TEXT("Online identity interface is not available."));
		return;
	}

	if (IdentityInterface->GetLoginStatus(LocalUserNum) == ELoginStatus::LoggedIn)
	{
		CacheLocalAccountInfo();
		OnLoginCompleted.Broadcast(true, TEXT("Already logged in."));
		return;
	}

	if (LoginCompleteDelegateHandle.IsValid())
	{
		IdentityInterface->ClearOnLoginCompleteDelegate_Handle(LocalUserNum, LoginCompleteDelegateHandle);
	}

	LoginCompleteDelegateHandle = IdentityInterface->AddOnLoginCompleteDelegate_Handle(
		LocalUserNum,
		FOnLoginCompleteDelegate::CreateUObject(this, &USPEOSSessionSubsystem::HandleLoginComplete));

	FOnlineAccountCredentials Credentials;
	Credentials.Type = TEXT("accountportal");

	if (!IdentityInterface->Login(LocalUserNum, Credentials))
	{
		IdentityInterface->ClearOnLoginCompleteDelegate_Handle(LocalUserNum, LoginCompleteDelegateHandle);
		LoginCompleteDelegateHandle.Reset();
		OnLoginCompleted.Broadcast(false, TEXT("Failed to start EOS login."));
	}
}

void USPEOSSessionSubsystem::StartMatchmaking(ELobbyPlayerRole SelectedRole, const FString& MainMenuLevelPath)
{
	if (SelectedRole == ELobbyPlayerRole::None)
	{
		BroadcastMatchmakingStatus(false, TEXT("매치메이킹 역할이 선택되지 않았습니다."));
		return;
	}

	PendingMatchmakingRole = SelectedRole;
	PendingMatchmakingLevelPath = ResolveMainMenuLevelPath(MainMenuLevelPath);
	PendingMainMenuLevelPath = PendingMatchmakingLevelPath;
	bStartMatchmakingAfterLogin = true;
	bIsMatchmakingActive = true;
	FindSessionAttemptCount = 0;
	bCreateSessionIfSearchStillEmpty = false;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FindSessionsRetryTimerHandle);
	}

	if (PendingMatchmakingLevelPath.IsEmpty())
	{
		bIsMatchmakingActive = false;
		BroadcastMatchmakingStatus(false, TEXT("메인 메뉴 레벨 경로가 비어 있습니다."));
		return;
	}

	if (!IsLoggedIn())
	{
		BroadcastMatchmakingStatus(true, TEXT("EOS 로그인 중입니다."));
		Login();
		return;
	}

	CacheLocalAccountInfo();
	BeginFindSessions();
}

void USPEOSSessionSubsystem::CancelMatchmaking()
{
	bStartMatchmakingAfterLogin = false;
	bCreateSessionIfSearchStillEmpty = false;
	bIsMatchmakingActive = false;
	PendingMatchmakingRole = ELobbyPlayerRole::None;
	FindSessionAttemptCount = 0;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FindSessionsRetryTimerHandle);
	}

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

	BroadcastMatchmakingStatus(true, TEXT("매치메이킹을 취소했습니다."));

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
	ResetMatchmakingState();

	IOnlineSessionPtr SessionInterface = GetSessionInterface();
	if (!SessionInterface.IsValid() || !SessionInterface->GetNamedSession(NAME_GameSession))
	{
		return;
	}

	const EOnlineSessionState::Type SessionState = SessionInterface->GetSessionState(NAME_GameSession);
	if (SessionState == EOnlineSessionState::InProgress || SessionState == EOnlineSessionState::Starting)
	{
		return;
	}

	if (SessionState != EOnlineSessionState::Pending)
	{
		UE_LOG(LogTemp, Warning, TEXT("EOS session cannot start from state %d."), static_cast<int32>(SessionState));
		return;
	}

	if (StartSessionCompleteDelegateHandle.IsValid())
	{
		SessionInterface->ClearOnStartSessionCompleteDelegate_Handle(StartSessionCompleteDelegateHandle);
	}

	StartSessionCompleteDelegateHandle = SessionInterface->AddOnStartSessionCompleteDelegate_Handle(
		FOnStartSessionCompleteDelegate::CreateUObject(this, &USPEOSSessionSubsystem::HandleStartSessionComplete));

	if (!SessionInterface->StartSession(NAME_GameSession))
	{
		SessionInterface->ClearOnStartSessionCompleteDelegate_Handle(StartSessionCompleteDelegateHandle);
		StartSessionCompleteDelegateHandle.Reset();
		UE_LOG(LogTemp, Warning, TEXT("Failed to start EOS session."));
	}
}

void USPEOSSessionSubsystem::EndSession()
{
	ResetMatchmakingState();

	IOnlineSessionPtr SessionInterface = GetSessionInterface();
	if (!SessionInterface.IsValid() || !SessionInterface->GetNamedSession(NAME_GameSession))
	{
		OnSessionEnded.Broadcast(true, TEXT("No active session."));
		return;
	}

	const EOnlineSessionState::Type SessionState = SessionInterface->GetSessionState(NAME_GameSession);
	if (SessionState == EOnlineSessionState::Ended || SessionState == EOnlineSessionState::NoSession)
	{
		OnSessionEnded.Broadcast(true, TEXT("EOS session is already ended."));
		return;
	}

	if (bEndSessionInProgress || SessionState == EOnlineSessionState::Ending)
	{
		return;
	}

	if (EndSessionCompleteDelegateHandle.IsValid())
	{
		SessionInterface->ClearOnEndSessionCompleteDelegate_Handle(EndSessionCompleteDelegateHandle);
	}

	EndSessionCompleteDelegateHandle = SessionInterface->AddOnEndSessionCompleteDelegate_Handle(
		FOnEndSessionCompleteDelegate::CreateUObject(this, &USPEOSSessionSubsystem::HandleEndSessionComplete));
	bEndSessionInProgress = true;

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
	ResetMatchmakingState();
	PendingMainMenuLevelPath = MainMenuLevelPath;
	if (bEndSessionInProgress)
	{
		bDestroySessionAfterEnd = true;
		return;
	}
	DestroyCurrentSession(true, false);
}

bool USPEOSSessionSubsystem::IsLoggedIn() const
{
	IOnlineIdentityPtr IdentityInterface = GetIdentityInterface();
	return IdentityInterface.IsValid() && IdentityInterface->GetLoginStatus(LocalUserNum) == ELoginStatus::LoggedIn;
}

FString USPEOSSessionSubsystem::GetCachedNickname() const
{
	return CachedNickname;
}

ELobbyPlayerRole USPEOSSessionSubsystem::GetPendingMatchmakingRole() const
{
	return PendingMatchmakingRole;
}

bool USPEOSSessionSubsystem::IsMatchmakingActive() const
{
	return bIsMatchmakingActive;
}

IOnlineIdentityPtr USPEOSSessionSubsystem::GetIdentityInterface() const
{
	IOnlineSubsystem* OnlineSubsystem = Online::GetSubsystem(this->GetWorld());
	return OnlineSubsystem ? OnlineSubsystem->GetIdentityInterface() : nullptr;
}

IOnlineSessionPtr USPEOSSessionSubsystem::GetSessionInterface() const
{
	IOnlineSubsystem* OnlineSubsystem = Online::GetSubsystem(this->GetWorld());
	return OnlineSubsystem ? OnlineSubsystem->GetSessionInterface() : nullptr;
}

void USPEOSSessionSubsystem::CacheLocalAccountInfo()
{
	IOnlineIdentityPtr IdentityInterface = GetIdentityInterface();
	if (!IdentityInterface.IsValid())
	{
		return;
	}

	CachedNickname = IdentityInterface->GetPlayerNickname(LocalUserNum);
	if (!CachedNickname.IsEmpty())
	{
		return;
	}

	const TSharedPtr<const FUniqueNetId> UserId = IdentityInterface->GetUniquePlayerId(LocalUserNum);
	CachedNickname = UserId.IsValid() ? UserId->ToString() : TEXT("Player");
}

void USPEOSSessionSubsystem::BeginFindSessions()
{
	IOnlineSessionPtr SessionInterface = GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		BroadcastMatchmakingStatus(false, TEXT("Online session interface is not available."));
		return;
	}

	if (FindSessionsCompleteDelegateHandle.IsValid())
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
	}

	SessionSearch = MakeShared<FOnlineSessionSearch>();
	SessionSearch->MaxSearchResults = 50;
	SessionSearch->bIsLanQuery = false;
	SessionSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
	SessionSearch->QuerySettings.Set(SPEOSSession::MatchSessionKey, SPEOSSession::MatchSessionValue, EOnlineComparisonOp::Equals);

	FindSessionsCompleteDelegateHandle = SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(
		FOnFindSessionsCompleteDelegate::CreateUObject(this, &USPEOSSessionSubsystem::HandleFindSessionsComplete));

	++FindSessionAttemptCount;
	BroadcastMatchmakingStatus(true, TEXT("세션 찾는중"));
	if (!SessionInterface->FindSessions(LocalUserNum, SessionSearch.ToSharedRef()))
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
		FindSessionsCompleteDelegateHandle.Reset();
		HandleFindSessionsRetry();
	}
}

void USPEOSSessionSubsystem::HandleFindSessionsRetry()
{
	if (FindSessionAttemptCount >= MaxFindSessionAttempts)
	{
		if (!bCreateSessionIfSearchStillEmpty)
		{
			bCreateSessionIfSearchStillEmpty = true;

			UWorld* World = GetWorld();
			if (!World)
			{
				bIsMatchmakingActive = false;
				BroadcastMatchmakingStatus(false, TEXT("최종 세션 검색을 위한 월드가 없습니다."));
				return;
			}

			const float BackoffDelay = FMath::FRandRange(MinCreateSessionBackoff, MaxCreateSessionBackoff);
			BroadcastMatchmakingStatus(true, TEXT("세션 찾는중"));
			World->GetTimerManager().SetTimer(FindSessionsRetryTimerHandle, this, &USPEOSSessionSubsystem::BeginFindSessions, BackoffDelay, false);
			return;
		}

		bCreateSessionIfSearchStillEmpty = false;
		BroadcastMatchmakingStatus(true, TEXT("세션이 없어 호스트 세션을 생성합니다."));
		BeginCreateSession();
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		bIsMatchmakingActive = false;
		BroadcastMatchmakingStatus(false, TEXT("세션 재검색을 위한 월드가 없습니다."));
		return;
	}

	BroadcastMatchmakingStatus(true, TEXT("세션 찾는중"));
	World->GetTimerManager().SetTimer(FindSessionsRetryTimerHandle, this, &USPEOSSessionSubsystem::BeginFindSessions, FindSessionRetryDelay, false);
}

void USPEOSSessionSubsystem::BeginCreateSession()
{
	IOnlineSessionPtr SessionInterface = GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		BroadcastMatchmakingStatus(false, TEXT("Online session interface is not available."));
		return;
	}

	if (SessionInterface->GetNamedSession(NAME_GameSession))
	{
		DestroyCurrentSession(false, true);
		return;
	}

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

	if (CreateSessionCompleteDelegateHandle.IsValid())
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
	}

	CreateSessionCompleteDelegateHandle = SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(
		FOnCreateSessionCompleteDelegate::CreateUObject(this, &USPEOSSessionSubsystem::HandleCreateSessionComplete));

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
	IOnlineSessionPtr SessionInterface = GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		BroadcastMatchmakingStatus(false, TEXT("Online session interface is not available."));
		return;
	}

	if (JoinSessionCompleteDelegateHandle.IsValid())
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
	}

	JoinSessionCompleteDelegateHandle = SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(
		FOnJoinSessionCompleteDelegate::CreateUObject(this, &USPEOSSessionSubsystem::HandleJoinSessionComplete));

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

	bTravelToMainMenuAfterDestroy = bTravelToMainMenu;
	bCreateSessionAfterDestroy = bCreateAfterDestroy;

	if (DestroySessionCompleteDelegateHandle.IsValid())
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
	}

	DestroySessionCompleteDelegateHandle = SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(
		FOnDestroySessionCompleteDelegate::CreateUObject(this, &USPEOSSessionSubsystem::HandleDestroySessionComplete));

	if (!SessionInterface->DestroySession(NAME_GameSession))
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
		DestroySessionCompleteDelegateHandle.Reset();
		OnSessionEnded.Broadcast(false, TEXT("Failed to start session destroy."));
	}
}

void USPEOSSessionSubsystem::ResetMatchmakingState()
{
	bStartMatchmakingAfterLogin = false;
	bCreateSessionIfSearchStillEmpty = false;
	bIsMatchmakingActive = false;
	bTravelToMainMenuAfterDestroy = false;
	bCreateSessionAfterDestroy = false;
	PendingMatchmakingRole = ELobbyPlayerRole::None;
	PendingMatchmakingLevelPath.Reset();
	FindSessionAttemptCount = 0;
	SessionSearch.Reset();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FindSessionsRetryTimerHandle);
	}

	IOnlineSessionPtr SessionInterface = GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		return;
	}

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
	UWorld* World = GetWorld();
	if (!World || PendingMatchmakingLevelPath.IsEmpty())
	{
		return;
	}

	UGameplayStatics::OpenLevel(World, FName(*PendingMatchmakingLevelPath), true, TEXT("listen"));
}

void USPEOSSessionSubsystem::OpenPendingMainMenuLevel()
{
	if (PendingMainMenuLevelPath.IsEmpty())
	{
		OnSessionEnded.Broadcast(false, TEXT("Main menu level path is empty."));
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		OnSessionEnded.Broadcast(false, TEXT("World is not available."));
		return;
	}

	UGameplayStatics::OpenLevel(World, FName(*PendingMainMenuLevelPath));
}

void USPEOSSessionSubsystem::BroadcastMatchmakingStatus(bool bWasSuccessful, const FString& StatusMessage) const
{
	OnMatchmakingStatusChanged.Broadcast(bWasSuccessful, StatusMessage);
}

FString USPEOSSessionSubsystem::ResolveMainMenuLevelPath(const FString& MainMenuLevelPath) const
{
	if (!MainMenuLevelPath.IsEmpty())
	{
		return MainMenuLevelPath;
	}

	const UWorld* World = GetWorld();
	return World && World->GetOutermost() ? World->GetOutermost()->GetName() : FString();
}

void USPEOSSessionSubsystem::HandleLoginComplete(int32 LocalUserNumber, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error)
{
	IOnlineIdentityPtr IdentityInterface = GetIdentityInterface();
	if (IdentityInterface.IsValid())
	{
		IdentityInterface->ClearOnLoginCompleteDelegate_Handle(LocalUserNumber, LoginCompleteDelegateHandle);
	}
	LoginCompleteDelegateHandle.Reset();

	if (!bWasSuccessful)
	{
		const FString Message = Error.IsEmpty() ? TEXT("EOS login failed.") : Error;
		OnLoginCompleted.Broadcast(false, Message);
		BroadcastMatchmakingStatus(false, Message);
		bStartMatchmakingAfterLogin = false;
		bIsMatchmakingActive = false;
		return;
	}

	CacheLocalAccountInfo();
	OnLoginCompleted.Broadcast(true, CachedNickname);

	if (bStartMatchmakingAfterLogin)
	{
		BeginFindSessions();
	}
}

void USPEOSSessionSubsystem::HandleFindSessionsComplete(bool bWasSuccessful)
{
	IOnlineSessionPtr SessionInterface = GetSessionInterface();
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
	}
	FindSessionsCompleteDelegateHandle.Reset();

	if (!bWasSuccessful || !SessionSearch.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("EOS FindSessions failed. Attempt=%d/%d"), FindSessionAttemptCount, MaxFindSessionAttempts);
		HandleFindSessionsRetry();
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("EOS FindSessions complete. Attempt=%d/%d Results=%d"), FindSessionAttemptCount, MaxFindSessionAttempts, SessionSearch->SearchResults.Num());

	for (const FOnlineSessionSearchResult& SearchResult : SessionSearch->SearchResults)
	{
		FString SessionType;
		SearchResult.Session.SessionSettings.Get(SPEOSSession::MatchSessionKey, SessionType);

		UE_LOG(LogTemp, Warning, TEXT("EOS Found Session Type=%s Open=%d Owner=%s"),
			*SessionType,
			SearchResult.Session.NumOpenPublicConnections,
			*SearchResult.Session.OwningUserName);

		if (SessionType == SPEOSSession::MatchSessionValue && SearchResult.Session.NumOpenPublicConnections > 0)
		{
			bCreateSessionIfSearchStillEmpty = false;
			BeginJoinSession(SearchResult);
			return;
		}
	}

	HandleFindSessionsRetry();
}

void USPEOSSessionSubsystem::HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	IOnlineSessionPtr SessionInterface = GetSessionInterface();
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
	}
	CreateSessionCompleteDelegateHandle.Reset();
	bStartMatchmakingAfterLogin = false;

	if (!bWasSuccessful)
	{
		bIsMatchmakingActive = false;
		BroadcastMatchmakingStatus(false, TEXT("EOS 세션 생성에 실패했습니다."));
		return;
	}

	BroadcastMatchmakingStatus(true, TEXT("세션 생성 완료. 메인 메뉴 대기방을 엽니다."));
	OpenMainMenuAsListenServer();
}

void USPEOSSessionSubsystem::HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	IOnlineSessionPtr SessionInterface = GetSessionInterface();
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
	}
	JoinSessionCompleteDelegateHandle.Reset();
	bStartMatchmakingAfterLogin = false;

	if (Result != EOnJoinSessionCompleteResult::Success || !SessionInterface.IsValid())
	{
		bIsMatchmakingActive = false;
		BroadcastMatchmakingStatus(false, TEXT("EOS 세션 참가에 실패했습니다."));
		return;
	}

	FString ConnectString;
	if (!SessionInterface->GetResolvedConnectString(NAME_GameSession, ConnectString) || ConnectString.IsEmpty())
	{
		bIsMatchmakingActive = false;
		BroadcastMatchmakingStatus(false, TEXT("EOS 세션 접속 주소를 가져오지 못했습니다."));
		return;
	}

	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, LocalUserNum);
	if (!PlayerController)
	{
		bIsMatchmakingActive = false;
		BroadcastMatchmakingStatus(false, TEXT("로컬 플레이어 컨트롤러가 없습니다."));
		return;
	}

	BroadcastMatchmakingStatus(true, TEXT("게임 입장중"));
	PlayerController->ClientTravel(ConnectString, TRAVEL_Absolute);
}

void USPEOSSessionSubsystem::HandleStartSessionComplete(FName SessionName, bool bWasSuccessful)
{
	IOnlineSessionPtr SessionInterface = GetSessionInterface();
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnStartSessionCompleteDelegate_Handle(StartSessionCompleteDelegateHandle);
	}
	StartSessionCompleteDelegateHandle.Reset();

	UE_LOG(LogTemp, Log, TEXT("EOS session start completed: %s"), bWasSuccessful ? TEXT("Success") : TEXT("Failed"));
}

void USPEOSSessionSubsystem::HandleEndSessionComplete(FName SessionName, bool bWasSuccessful)
{
	IOnlineSessionPtr SessionInterface = GetSessionInterface();
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnEndSessionCompleteDelegate_Handle(EndSessionCompleteDelegateHandle);
	}
	EndSessionCompleteDelegateHandle.Reset();
	bEndSessionInProgress = false;

	OnSessionEnded.Broadcast(
		bWasSuccessful,
		bWasSuccessful ? TEXT("EOS session ended. Match level remains loaded.") : TEXT("EOS session end failed."));

	if (bDestroySessionAfterEnd)
	{
		bDestroySessionAfterEnd = false;
		DestroyCurrentSession(true, false);
	}
}

void USPEOSSessionSubsystem::HandleDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
	IOnlineSessionPtr SessionInterface = GetSessionInterface();
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
	}
	DestroySessionCompleteDelegateHandle.Reset();

	const bool bShouldTravelToMainMenu = bTravelToMainMenuAfterDestroy;
	const bool bShouldCreateSession = bCreateSessionAfterDestroy;
	bTravelToMainMenuAfterDestroy = false;
	bCreateSessionAfterDestroy = false;

	if (!bWasSuccessful)
	{
		OnSessionEnded.Broadcast(false, TEXT("EOS session destroy failed."));
		return;
	}

	OnSessionEnded.Broadcast(true, TEXT("EOS session ended."));

	if (bShouldCreateSession)
	{
		BeginCreateSession();
		return;
	}

	if (bShouldTravelToMainMenu)
	{
		OpenPendingMainMenuLevel();
	}
}

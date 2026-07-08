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

void USPEOSSessionSubsystem::StartMatchmaking(const FString& LobbyLevelPath)
{
	PendingLobbyLevelPath = LobbyLevelPath;
	bStartMatchmakingAfterLogin = true;
	FindSessionAttemptCount = 0;
	bCreateSessionIfSearchStillEmpty = false;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FindSessionsRetryTimerHandle);
	}

	if (PendingLobbyLevelPath.IsEmpty())
	{
		BroadcastMatchmakingStatus(false, TEXT("Lobby level path is empty."));
		return;
	}

	if (!IsLoggedIn())
	{
		BroadcastMatchmakingStatus(true, TEXT("Logging in to EOS."));
		Login();
		return;
	}

	CacheLocalAccountInfo();
	BeginFindSessions();
}

void USPEOSSessionSubsystem::EndSession()
{
	DestroyCurrentSession(false, false);
}

void USPEOSSessionSubsystem::ReturnToMainMenu(const FString& MainMenuLevelPath)
{
	PendingMainMenuLevelPath = MainMenuLevelPath;
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
	BroadcastMatchmakingStatus(true, FString::Printf(TEXT("Finding EOS sessions. Attempt %d/%d."), FindSessionAttemptCount, MaxFindSessionAttempts));
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
				BroadcastMatchmakingStatus(false, TEXT("World is not available for final session search."));
				return;
			}

			const float BackoffDelay = FMath::FRandRange(MinCreateSessionBackoff, MaxCreateSessionBackoff);
			BroadcastMatchmakingStatus(true, FString::Printf(TEXT("No EOS session found. Final search before hosting in %.1f seconds."), BackoffDelay));
			World->GetTimerManager().SetTimer(FindSessionsRetryTimerHandle, this, &USPEOSSessionSubsystem::BeginFindSessions, BackoffDelay, false);
			return;
		}

		bCreateSessionIfSearchStillEmpty = false;
		BroadcastMatchmakingStatus(true, TEXT("No joinable EOS session found after final search. Creating host session."));
		BeginCreateSession();
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		BroadcastMatchmakingStatus(false, TEXT("World is not available for session retry."));
		return;
	}

	BroadcastMatchmakingStatus(true, TEXT("No EOS session found yet. Retrying session search."));
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
	SessionSettings.Set(SETTING_MAPNAME, PendingLobbyLevelPath, EOnlineDataAdvertisementType::ViaOnlineService);
	SessionSettings.Set(SPEOSSession::MatchSessionKey, SPEOSSession::MatchSessionValue, EOnlineDataAdvertisementType::ViaOnlineService);

	if (CreateSessionCompleteDelegateHandle.IsValid())
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
	}

	CreateSessionCompleteDelegateHandle = SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(
		FOnCreateSessionCompleteDelegate::CreateUObject(this, &USPEOSSessionSubsystem::HandleCreateSessionComplete));

	BroadcastMatchmakingStatus(true, TEXT("Creating EOS session."));
	if (!SessionInterface->CreateSession(LocalUserNum, NAME_GameSession, SessionSettings))
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
		CreateSessionCompleteDelegateHandle.Reset();
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

	BroadcastMatchmakingStatus(true, TEXT("Joining EOS session."));
	if (!SessionInterface->JoinSession(LocalUserNum, NAME_GameSession, SearchResult))
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
		JoinSessionCompleteDelegateHandle.Reset();
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

void USPEOSSessionSubsystem::OpenLobbyAsListenServer() const
{
	UWorld* World = GetWorld();
	if (!World || PendingLobbyLevelPath.IsEmpty())
	{
		return;
	}

	UGameplayStatics::OpenLevel(World, FName(*PendingLobbyLevelPath), true, TEXT("listen"));
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
		BroadcastMatchmakingStatus(false, TEXT("EOS session creation failed."));
		return;
	}

	BroadcastMatchmakingStatus(true, TEXT("EOS session created. Opening lobby as listen server."));
	OpenLobbyAsListenServer();
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
		BroadcastMatchmakingStatus(false, TEXT("EOS session join failed."));
		return;
	}

	FString ConnectString;
	if (!SessionInterface->GetResolvedConnectString(NAME_GameSession, ConnectString) || ConnectString.IsEmpty())
	{
		BroadcastMatchmakingStatus(false, TEXT("Failed to resolve EOS session connect string."));
		return;
	}

	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, LocalUserNum);
	if (!PlayerController)
	{
		BroadcastMatchmakingStatus(false, TEXT("Local player controller is not available."));
		return;
	}

	BroadcastMatchmakingStatus(true, TEXT("EOS session joined. Traveling to host."));
	PlayerController->ClientTravel(ConnectString, TRAVEL_Absolute);
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

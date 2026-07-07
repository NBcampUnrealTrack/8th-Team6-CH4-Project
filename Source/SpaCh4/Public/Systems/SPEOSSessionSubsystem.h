#pragma once

#include "CoreMinimal.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TimerManager.h"
#include "SPEOSSessionSubsystem.generated.h"

// 델리게이트 시그니처만 같으면 하나로 여러개 돌려쓰는것도 가능함
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSPOnlineOperationSignature, bool, bWasSuccessful, const FString&, StatusMessage);

// 싱글톤 유지, 모듈화를 위해 인스턴스 대신 인스턴스서브시스템 사용
UCLASS()
class SPACH4_API USPEOSSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "Online|Login")
	void Login();

	// 세션을 찾고, 없으면 생성
	UFUNCTION(BlueprintCallable, Category = "Online|Session")
	void StartMatchmaking(const FString& LobbyLevelPath);
	
	UFUNCTION(BlueprintCallable, Category = "Online|Session")
	void EndSession();

	// 매칭 종료후 세션을
	UFUNCTION(BlueprintCallable, Category = "Online|Session")
	void ReturnToMainMenu(const FString& MainMenuLevelPath);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Online|Login")
	bool IsLoggedIn() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Online|Login")
	FString GetCachedNickname() const;

	// 초기 실행직후 로그인 시
	UPROPERTY(BlueprintAssignable, Category = "Online|Events")
	FSPOnlineOperationSignature OnLoginCompleted;
	
	// 메인 메뉴에서 매치메이킹에 사용. 세션을 찾는 중, 찾았음, 생성 등등...
	UPROPERTY(BlueprintAssignable, Category = "Online|Events")
	FSPOnlineOperationSignature OnMatchmakingStatusChanged;
	
	// 로비, 게임 중 세션이 닫혔을때
	UPROPERTY(BlueprintAssignable, Category = "Online|Events")
	FSPOnlineOperationSignature OnSessionEnded;

private:
	IOnlineIdentityPtr GetIdentityInterface() const;
	IOnlineSessionPtr GetSessionInterface() const;

	void CacheLocalAccountInfo();
	void BeginFindSessions();
	void HandleFindSessionsRetry();
	void BeginCreateSession();
	void BeginJoinSession(const FOnlineSessionSearchResult& SearchResult);
	void DestroyCurrentSession(bool bTravelToMainMenu, bool bCreateSessionAfterDestroy);
	void OpenLobbyAsListenServer() const;
	void OpenPendingMainMenuLevel();
	void BroadcastMatchmakingStatus(bool bWasSuccessful, const FString& StatusMessage) const;

	void HandleLoginComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error);
	void HandleFindSessionsComplete(bool bWasSuccessful);
	void HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	void HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	void HandleDestroySessionComplete(FName SessionName, bool bWasSuccessful);

	FDelegateHandle LoginCompleteDelegateHandle;
	FDelegateHandle FindSessionsCompleteDelegateHandle;
	FDelegateHandle CreateSessionCompleteDelegateHandle;
	FDelegateHandle JoinSessionCompleteDelegateHandle;
	FDelegateHandle DestroySessionCompleteDelegateHandle;

	TSharedPtr<FOnlineSessionSearch> SessionSearch;
	FTimerHandle FindSessionsRetryTimerHandle;

	FString PendingLobbyLevelPath;
	FString PendingMainMenuLevelPath;
	FString CachedNickname;

	int32 FindSessionAttemptCount = 0;

	bool bStartMatchmakingAfterLogin = false;
	bool bTravelToMainMenuAfterDestroy = false;
	bool bCreateSessionAfterDestroy = false;
	bool bCreateSessionIfSearchStillEmpty = false;

	static constexpr int32 LocalUserNum = 0;
	static constexpr int32 MaxSessionPlayers = 4;
	static constexpr int32 MaxFindSessionAttempts = 0;
	static constexpr float FindSessionRetryDelay = 1.0f;
	static constexpr float MinCreateSessionBackoff = 0.5f;
	static constexpr float MaxCreateSessionBackoff = 2.0f;
};

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Systems/LobbyGameState.h"
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
	// 서브시스템 생성 시 부모 초기화와 온라인 기능 사용 준비를 수행합니다.
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	// 서브시스템 종료 시 타이머와 온라인 델리게이트를 안전하게 해제합니다.
	virtual void Deinitialize() override;

	// EOS 계정 포털 로그인을 시작하거나 기존 로그인 정보를 재사용합니다.
	UFUNCTION(BlueprintCallable, Category = "Online|Login")
	void Login();

	// 선택한 역할로 세션을 검색하고 참가 가능한 세션이 없으면 새 세션을 생성합니다.
	UFUNCTION(BlueprintCallable, Category = "Online|Session")
	void StartMatchmaking(ELobbyPlayerRole SelectedRole, const FString& MainMenuLevelPath);

	// 진행 중인 검색과 예약 작업을 중단하고 참가한 세션이 있으면 정리합니다.
	UFUNCTION(BlueprintCallable, Category = "Online|Session")
	void CancelMatchmaking();

	// 대기 상태인 현재 EOS 세션을 실제 게임 진행 상태로 전환합니다.
	UFUNCTION(BlueprintCallable, Category = "Online|Session")
	void StartSession();

	// 현재 EOS 세션의 게임 진행 상태를 종료합니다.
	UFUNCTION(BlueprintCallable, Category = "Online|Session")
	void EndSession();

	// 현재 세션을 제거한 뒤 지정된 메인 메뉴 레벨로 이동합니다.
	UFUNCTION(BlueprintCallable, Category = "Online|Session")
	void ReturnToMainMenu(const FString& MainMenuLevelPath);

	// 로컬 사용자의 EOS 로그인 완료 여부를 반환합니다.
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Online|Login")
	bool IsLoggedIn() const;

	// 로그인 과정에서 저장한 로컬 사용자의 닉네임을 반환합니다.
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Online|Login")
	FString GetCachedNickname() const;

	// 현재 매칭 요청에 사용 중인 플레이어 역할을 반환합니다.
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Online|Session")
	ELobbyPlayerRole GetPendingMatchmakingRole() const;

	// 현재 세션 검색·생성·참가 과정이 진행 중인지 반환합니다.
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Online|Session")
	bool IsMatchmakingActive() const;

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
	// 현재 월드에 연결된 온라인 Identity 인터페이스를 반환합니다.
	IOnlineIdentityPtr GetIdentityInterface() const;
	// 현재 월드에 연결된 온라인 Session 인터페이스를 반환합니다.
	IOnlineSessionPtr GetSessionInterface() const;

	// 로컬 사용자의 닉네임을 조회해 이후 레벨에서도 사용할 수 있도록 저장합니다.
	void CacheLocalAccountInfo();
	// 조건에 맞는 EOS 로비 세션 검색을 시작합니다.
	void BeginFindSessions();
	// 검색 실패 또는 빈 결과에 따라 재검색하거나 호스트 세션 생성을 예약합니다.
	void HandleFindSessionsRetry();
	// 매칭용 설정을 구성해 새로운 EOS 세션 생성을 시작합니다.
	void BeginCreateSession();
	// 선택된 검색 결과를 이용해 EOS 세션 참가를 시작합니다.
	void BeginJoinSession(const FOnlineSessionSearchResult& SearchResult);
	// 현재 세션을 제거하고 완료 후 이동 또는 재생성 작업을 예약합니다.
	void DestroyCurrentSession(bool bTravelToMainMenu, bool bCreateSessionAfterDestroy);
	// 매칭 상태와 검색 결과, 관련 델리게이트 및 타이머를 초기화합니다.
	void ResetMatchmakingState();
	// 호스트가 대기할 메인 메뉴 레벨을 Listen Server로 엽니다.
	void OpenMainMenuAsListenServer() const;
	// 예약된 메인 메뉴 레벨을 일반 로컬 레벨로 엽니다.
	void OpenPendingMainMenuLevel();
	// 매칭 진행 결과와 안내 메시지를 UI 구독자에게 전달합니다.
	void BroadcastMatchmakingStatus(bool bWasSuccessful, const FString& StatusMessage) const;
	// 입력 경로가 비어 있으면 현재 월드 경로를 메인 메뉴 경로로 보완합니다.
	FString ResolveMainMenuLevelPath(const FString& MainMenuLevelPath) const;

	// EOS 로그인 완료 결과를 정리하고 대기 중인 매칭을 이어서 실행합니다.
	void HandleLoginComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error);
	// 세션 검색 결과에서 참가 가능한 방을 선택하거나 다음 검색 단계로 진행합니다.
	void HandleFindSessionsComplete(bool bWasSuccessful);
	// 세션 생성 결과를 처리하고 성공하면 메인 메뉴 Listen Server를 엽니다.
	void HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	// 세션 참가 결과와 접속 주소를 확인한 뒤 클라이언트를 호스트로 이동시킵니다.
	void HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	// 세션 시작 완료 델리게이트를 해제하고 결과를 로그로 남깁니다.
	void HandleStartSessionComplete(FName SessionName, bool bWasSuccessful);
	// 세션 종료 결과를 알리고 필요하면 세션 제거 과정을 이어서 실행합니다.
	void HandleEndSessionComplete(FName SessionName, bool bWasSuccessful);
	// 세션 제거 결과에 따라 메인 메뉴 이동 또는 새 세션 생성을 수행합니다.
	void HandleDestroySessionComplete(FName SessionName, bool bWasSuccessful);

	FDelegateHandle LoginCompleteDelegateHandle;
	FDelegateHandle FindSessionsCompleteDelegateHandle;
	FDelegateHandle CreateSessionCompleteDelegateHandle;
	FDelegateHandle JoinSessionCompleteDelegateHandle;
	FDelegateHandle StartSessionCompleteDelegateHandle;
	FDelegateHandle EndSessionCompleteDelegateHandle;
	FDelegateHandle DestroySessionCompleteDelegateHandle;

	TSharedPtr<FOnlineSessionSearch> SessionSearch;
	FTimerHandle FindSessionsRetryTimerHandle;

	FString PendingMatchmakingLevelPath;
	FString PendingMainMenuLevelPath;
	FString CachedNickname;
	ELobbyPlayerRole PendingMatchmakingRole = ELobbyPlayerRole::None;

	int32 FindSessionAttemptCount = 0;

	bool bStartMatchmakingAfterLogin = false;
	bool bTravelToMainMenuAfterDestroy = false;
	bool bCreateSessionAfterDestroy = false;
	bool bCreateSessionIfSearchStillEmpty = false;
	bool bIsMatchmakingActive = false;
	bool bEndSessionInProgress = false;
	bool bDestroySessionAfterEnd = false;

	static constexpr int32 LocalUserNum = 0;
	static constexpr int32 MaxSessionPlayers = 4;
	static constexpr int32 MaxFindSessionAttempts = 0;
	static constexpr float FindSessionRetryDelay = 1.0f;
	static constexpr float MinCreateSessionBackoff = 0.5f;
	static constexpr float MaxCreateSessionBackoff = 2.0f;
};

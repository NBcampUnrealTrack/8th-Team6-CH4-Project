#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SPSpectatorComponent.generated.h"

class ACameraActor;
class APlayerController;
class ASurvivorCharacter;
class UInputComponent;
class USPInputConfigData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSPSpectateTargetChangedSignature, ASurvivorCharacter*, NewTarget);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSPSpectateTargetsExhaustedSignature);

UCLASS(ClassGroup = (SP), meta = (BlueprintSpawnableComponent), Blueprintable, BlueprintType)
class SPACH4_API USPSpectatorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USPSpectatorComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void ConfigureInput(USPInputConfigData* InInputConfig);
	void BindInputActions(UInputComponent* InputComponent);
	void EnterSpectatorMode(float InitialViewDelay = 0.f);

	UFUNCTION(BlueprintPure, Category = "Spectator")
	bool IsSpectating() const { return bIsSpectating; }

	UFUNCTION(BlueprintPure, Category = "Spectator")
	ASurvivorCharacter* GetSpectateTarget() const;

	UPROPERTY(BlueprintAssignable, Category = "Spectator")
	FSPSpectateTargetChangedSignature OnSpectateTargetChanged;

	UPROPERTY(BlueprintAssignable, Category = "Spectator")
	FSPSpectateTargetsExhaustedSignature OnSpectateTargetsExhausted;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	APlayerController* GetPlayerController() const;
	void ValidateInputConfig() const;
	void SpectateNext();
	void SpectatePrevious();
	void SpectateLook(const struct FInputActionValue& Value);

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_CycleSpectatorTarget(bool bForward);

	UFUNCTION(Client, Reliable)
	void Client_BeginSpectate();

	UFUNCTION(Client, Reliable)
	void Client_SetSpectateView(ASurvivorCharacter* NewViewTarget);

	UFUNCTION(Client, Reliable)
	void Client_EndSpectateNoTargets();

	void BeginTeammateSpectate();
	void FocusSpectateTarget(int32 Direction);
	void ValidateSpectateTarget();
	TArray<ASurvivorCharacter*> GatherSpectatableSurvivors() const;
	void EnsureSpectatorCamera();
	void UpdateSpectatorCamera(float DeltaTime, bool bSnapToTarget = false);
	void ReleaseSpectatorCamera();

	UPROPERTY(Transient)
	TObjectPtr<USPInputConfigData> InputConfig;

	UPROPERTY(Replicated)
	bool bIsSpectating = false;

	TWeakObjectPtr<ASurvivorCharacter> CurrentSpectateTarget;
	TWeakObjectPtr<ASurvivorCharacter> ClientSpectateTarget;

	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> SpectatorCameraActor;

	FVector SmoothedSpectatorPivot = FVector::ZeroVector;
	bool bSpectatorCameraPivotInitialized = false;

	FTimerHandle SpectatorStartTimerHandle;
	FTimerHandle SpectatorValidateTimerHandle;

	UPROPERTY(EditDefaultsOnly, Category = "Spectator")
	float SpectatorBlendTime = 0.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Spectator")
	float SpectatorValidateInterval = 0.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Spectator|Input")
	int32 SpectatorMappingPriority = 100;

	UPROPERTY(EditDefaultsOnly, Category = "Spectator|Camera")
	float SpectatorCameraDistance = 300.f;

	UPROPERTY(EditDefaultsOnly, Category = "Spectator|Camera")
	float SpectatorCameraPivotHeight = 80.f;

	UPROPERTY(EditDefaultsOnly, Category = "Spectator|Camera")
	float SpectatorCameraInitialPitch = -10.f;

	UPROPERTY(EditDefaultsOnly, Category = "Spectator|Camera")
	float SpectatorCameraCollisionRadius = 12.f;

	UPROPERTY(EditDefaultsOnly, Category = "Spectator|Camera")
	float SpectatorCameraCollisionPadding = 5.f;

	UPROPERTY(EditDefaultsOnly, Category = "Spectator|Camera", meta = (ClampMin = "0.0"))
	float SpectatorCameraFollowInterpSpeed = 15.f;
};

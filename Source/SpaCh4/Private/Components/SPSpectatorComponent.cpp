#include "Components/SPSpectatorComponent.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Characters/Survivor/SurvivorCharacter.h"
#include "Data/SPInputConfigData.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

USPSpectatorComponent::USPSpectatorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetIsReplicatedByDefault(true);
}

void USPSpectatorComponent::BeginPlay()
{
	Super::BeginPlay();

	if (APlayerController* PlayerController = GetPlayerController())
	{
		AddTickPrerequisiteActor(PlayerController);
	}
}

void USPSpectatorComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(USPSpectatorComponent, bIsSpectating);
}

void USPSpectatorComponent::TickComponent(float DeltaTime, ELevelTick TickType,	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const APlayerController* PlayerController = GetPlayerController();
	if (PlayerController && PlayerController->IsLocalController() && bIsSpectating)
	{
		UpdateSpectatorCamera(DeltaTime);
	}
}

void USPSpectatorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(SpectatorStartTimerHandle);
		GetWorld()->GetTimerManager().ClearTimer(SpectatorValidateTimerHandle);
	}

	ReleaseSpectatorCamera();
	Super::EndPlay(EndPlayReason);
}

void USPSpectatorComponent::ConfigureInput(USPInputConfigData* InInputConfig)
{
	InputConfig = InInputConfig;
	ValidateInputConfig();
}

void USPSpectatorComponent::BindInputActions(UInputComponent* InputComponent)
{
	ValidateInputConfig();

	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent))
	{
		if (InputConfig->SpectateNextAction)
		{
			EnhancedInput->BindAction(InputConfig->SpectateNextAction,ETriggerEvent::Started,this,	&ThisClass::SpectateNext);
		}

		if (InputConfig->SpectatePreviousAction)
		{
			EnhancedInput->BindAction(InputConfig->SpectatePreviousAction,ETriggerEvent::Started,this,&ThisClass::SpectatePrevious);
		}

		if (InputConfig->LookAction)
		{
			EnhancedInput->BindAction(InputConfig->LookAction,ETriggerEvent::Triggered,this,&ThisClass::SpectateLook);
		}
	}
}

void USPSpectatorComponent::EnterSpectatorMode(float InitialViewDelay)
{
	APlayerController* PlayerController = GetPlayerController();
	if (!PlayerController || !PlayerController->HasAuthority() || bIsSpectating || !GetWorld())
	{
		return;
	}

	bIsSpectating = true;

	if (InitialViewDelay > 0.f)
	{
		GetWorld()->GetTimerManager().SetTimer(
			SpectatorStartTimerHandle,
			this,
			&ThisClass::BeginTeammateSpectate,
			InitialViewDelay,
			false);
	}
	else
	{
		GetWorld()->GetTimerManager().SetTimerForNextTick(this, &ThisClass::BeginTeammateSpectate);
	}
}

ASurvivorCharacter* USPSpectatorComponent::GetSpectateTarget() const
{
	return ClientSpectateTarget.Get();
}

APlayerController* USPSpectatorComponent::GetPlayerController() const
{
	return Cast<APlayerController>(GetOwner());
}

void USPSpectatorComponent::ValidateInputConfig() const
{
	if (!InputConfig)
	{
		UE_LOG(
			LogTemp,
			Fatal,
			TEXT("USPSpectatorComponent requires an InputConfig supplied by its owning PlayerController."));
	}
}

void USPSpectatorComponent::BeginTeammateSpectate()
{
	APlayerController* PlayerController = GetPlayerController();
	if (!PlayerController || !PlayerController->HasAuthority() || !bIsSpectating || !GetWorld())
	{
		return;
	}

	if (PlayerController->GetPawn())
	{
		PlayerController->UnPossess();
	}

	Client_BeginSpectate();
	FocusSpectateTarget(0);

	if (bIsSpectating)
	{
		GetWorld()->GetTimerManager().SetTimer(
			SpectatorValidateTimerHandle,
			this,
			&ThisClass::ValidateSpectateTarget,
			SpectatorValidateInterval,
			true);
	}
}

void USPSpectatorComponent::Client_BeginSpectate_Implementation()
{
	APlayerController* PlayerController = GetPlayerController();
	if (!PlayerController)
	{
		return;
	}

	bIsSpectating = true;
	SetComponentTickEnabled(true);
	PlayerController->bAutoManageActiveCameraTarget = false;
	PlayerController->SetInputMode(FInputModeGameOnly());
	PlayerController->bShowMouseCursor = false;
	PlayerController->SetIgnoreMoveInput(true);
	PlayerController->ResetIgnoreLookInput();
	ValidateInputConfig();

	if (InputConfig->SpectatorMappingContext)
	{
		if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			InputSubsystem->AddMappingContext(InputConfig->SpectatorMappingContext, SpectatorMappingPriority);
		}
	}
}

void USPSpectatorComponent::FocusSpectateTarget(int32 Direction)
{
	APlayerController* PlayerController = GetPlayerController();
	if (!PlayerController || !PlayerController->HasAuthority() || !bIsSpectating || !GetWorld())
	{
		return;
	}

	TArray<ASurvivorCharacter*> Candidates = GatherSpectatableSurvivors();
	if (Candidates.IsEmpty())
	{
		bIsSpectating = false;
		CurrentSpectateTarget.Reset();
		GetWorld()->GetTimerManager().ClearTimer(SpectatorStartTimerHandle);
		GetWorld()->GetTimerManager().ClearTimer(SpectatorValidateTimerHandle);
		Client_EndSpectateNoTargets();
		return;
	}

	int32 NextIndex = 0;
	if (Direction != 0 && CurrentSpectateTarget.IsValid())
	{
		const int32 CurrentIndex = Candidates.IndexOfByKey(CurrentSpectateTarget.Get());
		if (CurrentIndex != INDEX_NONE)
		{
			NextIndex = (CurrentIndex + Direction + Candidates.Num()) % Candidates.Num();
		}
	}

	ASurvivorCharacter* NewTarget = Candidates[NextIndex];
	CurrentSpectateTarget = NewTarget;
	PlayerController->SetViewTarget(NewTarget);
	Client_SetSpectateView(NewTarget);
}

void USPSpectatorComponent::Client_SetSpectateView_Implementation(ASurvivorCharacter* NewViewTarget)
{
	APlayerController* PlayerController = GetPlayerController();
	if (!PlayerController)
	{
		return;
	}

	const bool bTargetChanged = ClientSpectateTarget.Get() != NewViewTarget;
	if (ASurvivorCharacter* PreviousTarget = ClientSpectateTarget.Get())
	{
		RemoveTickPrerequisiteComponent(PreviousTarget->GetCharacterMovement());
	}

	ClientSpectateTarget = NewViewTarget;
	if (NewViewTarget)
	{
		AddTickPrerequisiteComponent(NewViewTarget->GetCharacterMovement());

		if (bTargetChanged)
		{
			PlayerController->SetControlRotation(FRotator(
				SpectatorCameraInitialPitch,
				NewViewTarget->GetActorRotation().Yaw,
				0.f));
			bSpectatorCameraPivotInitialized = false;
		}

		EnsureSpectatorCamera();
		UpdateSpectatorCamera(0.f, true);
		if (SpectatorCameraActor)
		{
			PlayerController->SetViewTargetWithBlend(SpectatorCameraActor, SpectatorBlendTime);
		}
	}

	OnSpectateTargetChanged.Broadcast(NewViewTarget);
}

void USPSpectatorComponent::Client_EndSpectateNoTargets_Implementation()
{
	APlayerController* PlayerController = GetPlayerController();
	if (!PlayerController)
	{
		return;
	}

	bIsSpectating = false;
	SetComponentTickEnabled(false);
	PlayerController->bAutoManageActiveCameraTarget = true;
	ValidateInputConfig();

	if (InputConfig->SpectatorMappingContext)
	{
		if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			InputSubsystem->RemoveMappingContext(InputConfig->SpectatorMappingContext);
		}
	}

	PlayerController->ResetIgnoreMoveInput();
	ReleaseSpectatorCamera();
	OnSpectateTargetChanged.Broadcast(nullptr);
	OnSpectateTargetsExhausted.Broadcast();
}

void USPSpectatorComponent::SpectateLook(const FInputActionValue& Value)
{
	APlayerController* PlayerController = GetPlayerController();
	if (!PlayerController || !bIsSpectating || !ClientSpectateTarget.IsValid())
	{
		return;
	}

	const FVector2D LookVector = Value.Get<FVector2D>();
	PlayerController->AddYawInput(LookVector.X);
	PlayerController->AddPitchInput(LookVector.Y);
}

void USPSpectatorComponent::EnsureSpectatorCamera()
{
	const APlayerController* PlayerController = GetPlayerController();
	if (IsValid(SpectatorCameraActor) || !PlayerController || !PlayerController->IsLocalController() || !GetWorld())
	{
		return;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = GetOwner();
	SpawnParameters.ObjectFlags |= RF_Transient;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	SpectatorCameraActor = GetWorld()->SpawnActor<ACameraActor>(
		ACameraActor::StaticClass(),
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		SpawnParameters);

	if (SpectatorCameraActor)
	{
		SpectatorCameraActor->SetReplicates(false);
		SpectatorCameraActor->SetActorEnableCollision(false);
		if (UCameraComponent* SpectatorCameraComponent = SpectatorCameraActor->GetCameraComponent())
		{
			SpectatorCameraComponent->SetConstraintAspectRatio(false);
		}
	}
}

void USPSpectatorComponent::UpdateSpectatorCamera(float DeltaTime, bool bSnapToTarget)
{
	const APlayerController* PlayerController = GetPlayerController();
	ASurvivorCharacter* Target = ClientSpectateTarget.Get();
	if (!PlayerController || !IsValid(Target))
	{
		return;
	}

	EnsureSpectatorCamera();
	if (!SpectatorCameraActor)
	{
		return;
	}

	const FVector TargetPivotLocation = Target->GetActorLocation() + FVector::UpVector * SpectatorCameraPivotHeight;
	if (!bSpectatorCameraPivotInitialized || bSnapToTarget || SpectatorCameraFollowInterpSpeed <= 0.f)
	{
		SmoothedSpectatorPivot = TargetPivotLocation;
		bSpectatorCameraPivotInitialized = true;
	}
	else if (DeltaTime > 0.f)
	{
		SmoothedSpectatorPivot = FMath::VInterpTo(
			SmoothedSpectatorPivot,
			TargetPivotLocation,
			DeltaTime,
			SpectatorCameraFollowInterpSpeed);
	}

	const FVector PivotLocation = SmoothedSpectatorPivot;
	const FRotator CameraRotation = PlayerController->GetControlRotation();
	const FVector DesiredLocation = PivotLocation - CameraRotation.Vector() * SpectatorCameraDistance;
	FVector ResolvedLocation = DesiredLocation;

	if (SpectatorCameraCollisionRadius > 0.f)
	{
		FHitResult Hit;
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(SpectatorCamera), false, Target);
		QueryParams.AddIgnoredActor(SpectatorCameraActor);
		if (GetWorld()->SweepSingleByChannel(
			Hit,
			PivotLocation,
			DesiredLocation,
			FQuat::Identity,
			ECC_Camera,
			FCollisionShape::MakeSphere(SpectatorCameraCollisionRadius),
			QueryParams))
		{
			ResolvedLocation = Hit.Location + Hit.Normal * SpectatorCameraCollisionPadding;
		}
	}

	SpectatorCameraActor->SetActorLocationAndRotation(ResolvedLocation, CameraRotation);
	if (UCameraComponent* SpectatorCameraComponent = SpectatorCameraActor->GetCameraComponent())
	{
		if (const UCameraComponent* TargetCameraComponent = Target->GetCameraComponent())
		{
			SpectatorCameraComponent->SetFieldOfView(TargetCameraComponent->FieldOfView);
		}
	}
}

void USPSpectatorComponent::ReleaseSpectatorCamera()
{
	if (ASurvivorCharacter* PreviousTarget = ClientSpectateTarget.Get())
	{
		RemoveTickPrerequisiteComponent(PreviousTarget->GetCharacterMovement());
	}

	if (IsValid(SpectatorCameraActor))
	{
		SpectatorCameraActor->Destroy();
	}

	SpectatorCameraActor = nullptr;
	ClientSpectateTarget.Reset();
	SmoothedSpectatorPivot = FVector::ZeroVector;
	bSpectatorCameraPivotInitialized = false;
}

void USPSpectatorComponent::ValidateSpectateTarget()
{
	APlayerController* PlayerController = GetPlayerController();
	if (!PlayerController || !PlayerController->HasAuthority() || !bIsSpectating)
	{
		return;
	}

	if (CurrentSpectateTarget.IsValid())
	{
		const ESurvivorState State = CurrentSpectateTarget->GetSurvivorState();
		if (State != ESurvivorState::Dead && State != ESurvivorState::Escaped)
		{
			return;
		}
	}

	FocusSpectateTarget(1);
}

TArray<ASurvivorCharacter*> USPSpectatorComponent::GatherSpectatableSurvivors() const
{
	TArray<ASurvivorCharacter*> Result;
	const APlayerController* PlayerController = GetPlayerController();
	UWorld* World = GetWorld();
	if (!PlayerController || !World)
	{
		return Result;
	}

	for (TActorIterator<ASurvivorCharacter> It(World); It; ++It)
	{
		ASurvivorCharacter* Survivor = *It;
		if (!IsValid(Survivor) || Survivor->GetController() == PlayerController)
		{
			continue;
		}

		const ESurvivorState State = Survivor->GetSurvivorState();
		if (State == ESurvivorState::Dead || State == ESurvivorState::Escaped)
		{
			continue;
		}

		Result.Add(Survivor);
	}

	Result.Sort([](const ASurvivorCharacter& A, const ASurvivorCharacter& B)
	{
		return A.GetName() < B.GetName();
	});

	return Result;
}

void USPSpectatorComponent::SpectateNext()
{
	if (bIsSpectating)
	{
		Server_CycleSpectatorTarget(true);
	}
}

void USPSpectatorComponent::SpectatePrevious()
{
	if (bIsSpectating)
	{
		Server_CycleSpectatorTarget(false);
	}
}

bool USPSpectatorComponent::Server_CycleSpectatorTarget_Validate(bool bForward)
{
	return true;
}

void USPSpectatorComponent::Server_CycleSpectatorTarget_Implementation(bool bForward)
{
	FocusSpectateTarget(bForward ? 1 : -1);
}

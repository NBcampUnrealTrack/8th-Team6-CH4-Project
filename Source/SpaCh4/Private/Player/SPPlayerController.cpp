#include "Player/SPPlayerController.h"

#include "EnhancedInputSubsystems.h"
#include "Data/SPInputConfigData.h"
#include "Systems/SPEOSSessionSubsystem.h"

ASPPlayerController::ASPPlayerController()
{
	SpectatorComponent = CreateDefaultSubobject<USPSpectatorComponent>(TEXT("SpectatorComponent"));
}

void ASPPlayerController::ReturnToMainMenu()
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (USPEOSSessionSubsystem* SessionSubsystem = GameInstance->GetSubsystem<USPEOSSessionSubsystem>())
		{
			SessionSubsystem->ReturnToMainMenu(MainMenuLevelPath);
		}
	}
}

void ASPPlayerController::BeginPlay()
{
	Super::BeginPlay();
	ValidateInputConfig();
	SpectatorComponent->ConfigureInput(InputConfig);
	SpectatorComponent->OnSpectateTargetChanged.RemoveDynamic(this, &ThisClass::HandleSpectateTargetChanged);
	SpectatorComponent->OnSpectateTargetChanged.AddDynamic(this, &ThisClass::HandleSpectateTargetChanged);
	SpectatorComponent->OnSpectateTargetsExhausted.RemoveDynamic(this, &ThisClass::HandleSpectateTargetsExhausted);
	SpectatorComponent->OnSpectateTargetsExhausted.AddDynamic(this, &ThisClass::HandleSpectateTargetsExhausted);

	if (IsLocalController())
	{
		SetInputMode(FInputModeGameOnly());
		bShowMouseCursor = false;
		ResetIgnoreMoveInput();
		ResetIgnoreLookInput();
	}

	for (const FInputMappingContextEntry& Entry : InputConfig->MappingContexts)
	{
		AddInputMappingContext(Entry.MappingContext, Entry.Priority);
	}
}

void ASPPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (SpectatorComponent)
	{
		SpectatorComponent->OnSpectateTargetChanged.RemoveDynamic(this, &ThisClass::HandleSpectateTargetChanged);
		SpectatorComponent->OnSpectateTargetsExhausted.RemoveDynamic(this, &ThisClass::HandleSpectateTargetsExhausted);
	}

	Super::EndPlay(EndPlayReason);
}

void ASPPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	ValidateInputConfig();
	SpectatorComponent->ConfigureInput(InputConfig);
	SpectatorComponent->BindInputActions(InputComponent);
}

void ASPPlayerController::AddInputMappingContext(UInputMappingContext* MappingContext, int32 Priority)
{
	if (!MappingContext)
	{
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = GetInputSubsystem())
	{
		Subsystem->AddMappingContext(MappingContext, Priority);
	}
}

void ASPPlayerController::RemoveInputMappingContext(UInputMappingContext* MappingContext)
{
	if (!MappingContext)
	{
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = GetInputSubsystem())
	{
		Subsystem->RemoveMappingContext(MappingContext);
	}
}

UEnhancedInputLocalPlayerSubsystem* ASPPlayerController::GetInputSubsystem() const
{
	return ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
}

void ASPPlayerController::ValidateInputConfig() const
{
	if (!InputConfig)
	{
		UE_LOG(
			LogTemp,
			Fatal,
			TEXT("ASPPlayerController requires InputConfig to be assigned on its class defaults. Runtime fallback loading is not allowed."));
	}
}

void ASPPlayerController::EnterSpectatorMode(float InitialViewDelay)
{
	SpectatorComponent->EnterSpectatorMode(InitialViewDelay);
}

bool ASPPlayerController::IsSpectating() const
{
	return SpectatorComponent->IsSpectating();
}

ASurvivorCharacter* ASPPlayerController::GetSpectateTarget() const
{
	return SpectatorComponent->GetSpectateTarget();
}

void ASPPlayerController::HandleSpectateTargetChanged(ASurvivorCharacter* NewTarget)
{
	OnSpectateTargetChanged.Broadcast(NewTarget);
}

void ASPPlayerController::HandleSpectateTargetsExhausted()
{
	OnSpectateTargetsExhausted.Broadcast();
}

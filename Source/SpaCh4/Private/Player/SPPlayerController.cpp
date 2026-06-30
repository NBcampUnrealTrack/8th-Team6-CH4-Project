#include "Player/SPPlayerController.h"

#include "EnhancedInputSubsystems.h"
#include "Data/SPInputConfigData.h"

void ASPPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!InputConfig)
	{
		return;
	}
	
	for (const FInputMappingContextEntry& Entry : InputConfig->MappingContexts)
	{
		AddInputMappingContext(Entry.MappingContext, Entry.Priority);
	}
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

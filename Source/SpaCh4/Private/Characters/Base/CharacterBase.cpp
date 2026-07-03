#include "Characters/Base/CharacterBase.h"

#include "Camera/CameraComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Data/SPInputConfigData.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "UObject/ConstructorHelpers.h"

namespace CharacterBaseInput
{
	static UInputAction* LoadInputAction(const TCHAR* AssetPath)
	{
		return LoadObject<UInputAction>(nullptr, AssetPath);
	}
}

ACharacterBase::ACharacterBase()
{
	PrimaryActorTick.bCanEverTick = false;

	SpringArm = CreateDefaultSubobject<USpringArmComponent>("SpringArm");
	SpringArm->SetupAttachment(GetRootComponent());
	SpringArm->TargetArmLength = 300.f;
	SpringArm->bEnableCameraLag = true;
	SpringArm->CameraLagSpeed = 15.f;
	SpringArm->bUsePawnControlRotation = true;

	Camera = CreateDefaultSubobject<UCameraComponent>("Camera");
	Camera->SetupAttachment(SpringArm);

	static ConstructorHelpers::FObjectFinder<USPInputConfigData> DefaultInputConfig(
		TEXT("/Game/Data/DA_InputConfig.DA_InputConfig"));
	if (DefaultInputConfig.Succeeded())
	{
		InputConfig = DefaultInputConfig.Object;
	}
}

void ACharacterBase::EnsureInputConfig()
{
	if (InputConfig)
	{
		return;
	}

	InputConfig = LoadObject<USPInputConfigData>(nullptr, TEXT("/Game/Data/DA_InputConfig.DA_InputConfig"));
}

void ACharacterBase::InitializeInputMappingContexts()
{
	EnsureInputConfig();
	if (!InputConfig)
	{
		return;
	}

	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (!PlayerController || !PlayerController->IsLocalController())
	{
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
	{
		for (const FInputMappingContextEntry& Entry : InputConfig->MappingContexts)
		{
			if (Entry.MappingContext)
			{
				InputSubsystem->AddMappingContext(Entry.MappingContext, Entry.Priority);
			}
		}
	}
}

void ACharacterBase::BeginPlay()
{
	Super::BeginPlay();
	InitializeInputMappingContexts();
}

void ACharacterBase::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	InitializeInputMappingContexts();
}

void ACharacterBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	EnsureInputConfig();

	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		UInputAction* MoveAction = InputConfig ? InputConfig->MoveAction.Get() : nullptr;
		UInputAction* LookAction = InputConfig ? InputConfig->LookAction.Get() : nullptr;
		UInputAction* InteractAction = InputConfig ? InputConfig->InteractAction.Get() : nullptr;
		UInputAction* JumpOverAction = InputConfig ? InputConfig->JumpOverAction.Get() : nullptr;

		if (!MoveAction)
		{
			MoveAction = CharacterBaseInput::LoadInputAction(TEXT("/Game/Input/InputAction/IA_Move.IA_Move"));
		}
		if (!LookAction)
		{
			LookAction = CharacterBaseInput::LoadInputAction(TEXT("/Game/Input/InputAction/IA_Look.IA_Look"));
		}
		if (!InteractAction)
		{
			InteractAction = CharacterBaseInput::LoadInputAction(TEXT("/Game/Input/InputAction/IA_Interact.IA_Interact"));
		}
		if (!JumpOverAction)
		{
			JumpOverAction = CharacterBaseInput::LoadInputAction(TEXT("/Game/Input/InputAction/IA_JumpOver.IA_JumpOver"));
		}

		if (MoveAction)
		{
			EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ACharacterBase::Move);
		}
		if (LookAction)
		{
			EnhancedInput->BindAction(LookAction, ETriggerEvent::Triggered, this, &ACharacterBase::Look);
		}
		if (InteractAction)
		{
			EnhancedInput->BindAction(InteractAction, ETriggerEvent::Started, this, &ThisClass::Interact);
		}
		if (JumpOverAction)
		{
			EnhancedInput->BindAction(JumpOverAction, ETriggerEvent::Started, this, &ThisClass::JumpOver);
		}
	}
}

void ACharacterBase::Move(const FInputActionValue& Value)
{
	const FVector2D MovementVector = Value.Get<FVector2D>();
	if (MovementVector.IsNearlyZero())
	{
		return;
	}

	const AController* OwnerController = GetController();
	if (!OwnerController)
	{
		return;
	}

	const FRotator YawRotation(0.f, OwnerController->GetControlRotation().Yaw, 0.f);
	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	AddMovementInput(ForwardDirection, MovementVector.Y);
	AddMovementInput(RightDirection, MovementVector.X);
}

void ACharacterBase::Look(const FInputActionValue& Value)
{
	const FVector2D LookVector = Value.Get<FVector2D>();

	AddControllerYawInput(LookVector.X);
	AddControllerPitchInput(LookVector.Y);
}

void ACharacterBase::Interact()
{
}

void ACharacterBase::JumpOver()
{
}

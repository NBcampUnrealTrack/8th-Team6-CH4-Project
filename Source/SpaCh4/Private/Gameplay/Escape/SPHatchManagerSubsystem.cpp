#include "Gameplay/Escape/SPHatchManagerSubsystem.h"

#include "Engine/World.h"
#include "Gameplay/Escape/SPHatch.h"
#include "TimerManager.h"

void USPHatchManagerSubsystem::RegisterHatch(ASPHatch* Hatch)
{
	if (!HasServerAuthority() || !IsValid(Hatch))
	{
		return;
	}

	RegisteredHatches.AddUnique(Hatch);

	if (ActiveHatch.Get() != Hatch)
	{
		Hatch->DeactivateHatch();
	}

	if (bActivationRequested && !ActiveHatch.IsValid())
	{
		SchedulePendingActivation();
	}
}

void USPHatchManagerSubsystem::UnregisterHatch(ASPHatch* Hatch)
{
	if (!IsValid(Hatch))
	{
		return;
	}

	RegisteredHatches.RemoveAll(
		[Hatch](const TWeakObjectPtr<ASPHatch>& RegisteredHatch)
		{
			return !RegisteredHatch.IsValid() || RegisteredHatch.Get() == Hatch;
		});

	if (ActiveHatch.Get() == Hatch)
	{
		ActiveHatch.Reset();
		if (bActivationRequested)
		{
			SchedulePendingActivation();
		}
	}
}

void USPHatchManagerSubsystem::RequestRandomActivation()
{
	if (!HasServerAuthority() || ActiveHatch.IsValid())
	{
		return;
	}

	bActivationRequested = true;
	TryActivateRandomHatch();
}

void USPHatchManagerSubsystem::ResetHatches()
{
	if (!HasServerAuthority())
	{
		return;
	}

	bActivationRequested = false;
	bActivationScheduled = false;
	ActiveHatch.Reset();
	PruneInvalidHatches();

	for (const TWeakObjectPtr<ASPHatch>& RegisteredHatch : RegisteredHatches)
	{
		if (ASPHatch* Hatch = RegisteredHatch.Get())
		{
			Hatch->DeactivateHatch();
		}
	}
}

ASPHatch* USPHatchManagerSubsystem::GetActiveHatch() const
{
	return ActiveHatch.Get();
}

bool USPHatchManagerSubsystem::HasServerAuthority() const
{
	const UWorld* World = GetWorld();
	return World && World->GetNetMode() != NM_Client;
}

void USPHatchManagerSubsystem::TryActivateRandomHatch()
{
	bActivationScheduled = false;

	if (!HasServerAuthority() || !bActivationRequested || ActiveHatch.IsValid())
	{
		return;
	}

	PruneInvalidHatches();

	TArray<ASPHatch*> Candidates;
	Candidates.Reserve(RegisteredHatches.Num());
	for (const TWeakObjectPtr<ASPHatch>& RegisteredHatch : RegisteredHatches)
	{
		ASPHatch* Hatch = RegisteredHatch.Get();
		if (IsValid(Hatch) && Hatch->CanBeSelected())
		{
			Candidates.Add(Hatch);
		}
	}

	if (Candidates.IsEmpty())
	{
		return;
	}

	ASPHatch* SelectedHatch = Candidates[FMath::RandHelper(Candidates.Num())];
	if (!IsValid(SelectedHatch))
	{
		return;
	}

	SelectedHatch->ActivateHatch();
	ActiveHatch = SelectedHatch;
}

void USPHatchManagerSubsystem::SchedulePendingActivation()
{
	if (bActivationScheduled)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	bActivationScheduled = true;
	World->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateUObject(this, &USPHatchManagerSubsystem::TryActivateRandomHatch));
}

void USPHatchManagerSubsystem::PruneInvalidHatches()
{
	RegisteredHatches.RemoveAll(
		[](const TWeakObjectPtr<ASPHatch>& RegisteredHatch)
		{
			return !RegisteredHatch.IsValid();
		});
}

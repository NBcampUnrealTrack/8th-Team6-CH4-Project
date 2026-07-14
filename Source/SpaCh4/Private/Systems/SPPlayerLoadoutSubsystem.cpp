#include "Systems/SPPlayerLoadoutSubsystem.h"

void USPPlayerLoadoutSubsystem::SaveCachedLoadout(const FSPPlayerLoadout& NewLoadout)
{
	CachedLoadout = NewLoadout;
	OnCachedLoadoutChanged.Broadcast(CachedLoadout);
}

const FSPPlayerLoadout& USPPlayerLoadoutSubsystem::GetCachedLoadout() const
{
	return CachedLoadout;
}

bool USPPlayerLoadoutSubsystem::IsSurvivorLoadoutConfigured() const
{
	return SPPlayerLoadout::IsSurvivorLoadoutComplete(CachedLoadout);
}

bool USPPlayerLoadoutSubsystem::IsKillerLoadoutConfigured() const
{
	return SPPlayerLoadout::IsKillerLoadoutComplete(CachedLoadout);
}

bool USPPlayerLoadoutSubsystem::IsLoadoutComplete() const
{
	return SPPlayerLoadout::IsComplete(CachedLoadout);
}

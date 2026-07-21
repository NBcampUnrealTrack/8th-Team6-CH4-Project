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
	// 기간상, 아이템만 체크함.
	return SPPlayerLoadout::IsSurvivorLoadoutCompleteOnlyItem(CachedLoadout);
	//return SPPlayerLoadout::IsSurvivorLoadoutComplete(CachedLoadout);
}

bool USPPlayerLoadoutSubsystem::IsKillerLoadoutConfigured() const
{
	return true;
	//return SPPlayerLoadout::IsKillerLoadoutComplete(CachedLoadout);
}

bool USPPlayerLoadoutSubsystem::IsLoadoutComplete() const
{
	return SPPlayerLoadout::IsComplete(CachedLoadout);
}

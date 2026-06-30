// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

namespace SPGameplayTags
{
	namespace Interactable
	{
		SPACH4_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Collectible)
		SPACH4_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cage)
		SPACH4_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Delivery)
		SPACH4_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Trap)
		namespace Escape
		{
			SPACH4_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Gate)
			SPACH4_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Hatch)
		}
	}
	namespace Character
	{
		SPACH4_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Survivor)
		SPACH4_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Killer)
	}
}
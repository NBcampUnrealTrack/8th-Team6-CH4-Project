// Fill out your copyright notice in the Description page of Project Settings.


#include "Type/SPGameplayTag.h"

namespace SPGameplayTags
{
	namespace Interactable
	{
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(Collectible, "Interactable.Collectible", "수집품 태그")
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cage, "Interactable.Cage", "생존자를 넣을 케이지 태그")
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(Delivery, "Interactable.Delivery", "수집품을 제출할 수 있는 제출처 태그")
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(Trap, "Interactable.Trap", "레벨에 배치된 함정 태그")
		namespace Escape
		{
			UE_DEFINE_GAMEPLAY_TAG_COMMENT(Gate, "Interactable.Escape.Gate", "탈출구 태그")
			UE_DEFINE_GAMEPLAY_TAG_COMMENT(Hatch, "Interactable.Escape.Hatch", "개구멍 태그")
		}
	}
	namespace Character
	{
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(Survivor, "Character.Survivor", "생존자 태그")
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(Killer, "Character.Killer", "살인마 태그")
	}
}
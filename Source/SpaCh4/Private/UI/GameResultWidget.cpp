// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/GameResultWidget.h"
#include "Components/TextBlock.h"

void UGameResultWidget::SetResultText(EMatchResult MatchResult)
{
	if (ResultTitleText)
	{
		const UEnum* CharStateEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EMatchResult"), true);
		if (CharStateEnum)
		{
			FString EnumToString = CharStateEnum->GetNameStringByValue((int64)MatchResult);
			ResultTitleText->SetText(FText::FromString(EnumToString));
		}
		
	}
	
}

void UGameResultWidget::SetInformationText(FString InfoText)
{
	if (InformationText)
	{
		InformationText->SetText(FText::FromString(InfoText));
	}
}

void UGameResultWidget::SetSurvivorStats(const FSurvivorMatchStats& SurvivorStats)
{
	if (!ScoreText)
	{
		return;
	}

	FString OutcomeText = TEXT("미탈출");
	if (SurvivorStats.bKilled)
	{
		OutcomeText = TEXT("사망");
	}
	else if (SurvivorStats.bEscaped)
	{
		switch (SurvivorStats.EscapeMethod)
		{
		case ESurvivorEscapeMethod::Gate:
			OutcomeText = TEXT("탈출 성공");
			break;
		case ESurvivorEscapeMethod::Hatch:
			OutcomeText = TEXT("개구멍 탈출");
			break;
		default:
			OutcomeText = TEXT("탈출 성공");
			break;
		}
	}

	const FString StatsText = FString::Printf(
		TEXT("납품 가치: %d\n납품 완료: %d회\n케이지 구조: %d회\n자가 치료: %d회\n감금: %d회\n최종 상태: %s"),
		SurvivorStats.DeliveredValue,
		SurvivorStats.SuccessfulDeliveryCount,
		SurvivorStats.CageRescueCount,
		SurvivorStats.SelfHealCount,
		SurvivorStats.CagedCount,
		*OutcomeText);
	ScoreText->SetText(FText::FromString(StatsText));
}

void UGameResultWidget::SetKillerStats(const FKillerMatchStats& KillerStats)
{
	if (!ScoreText)
	{
		return;
	}

	const FString StatsText = FString::Printf(
		TEXT("유효 타격: %d회\n다운: %d회\n감금: %d회\n처치: %d명"),
		KillerStats.ValidHitCount,
		KillerStats.DownCount,
		KillerStats.CageCount,
		KillerStats.KilledSurvivorCount);
	ScoreText->SetText(FText::FromString(StatsText));
}

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
		InformationText->SetText(InfoText);
	}
}

// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/GameResultWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "Player/LDPlayerState.h"
#include "UI/SPResultPrimaryStatWidget.h"
#include "UI/SPResultStatRowWidget.h"
#include "UI/Style/SPUIStyleData.h"
#include "UI/Style/SPUIStyleLibrary.h"

namespace ResultStatsPanelIndex
{
	constexpr int32 Survivor = 0;
	constexpr int32 Killer = 1;
}

namespace ResultIntroAnimation
{
	constexpr float BackgroundFadeDuration = 0.6f;
	constexpr float PanelFadeStartTime = 0.4f;
	constexpr float PanelFadeDuration = 0.8f;
	constexpr float TitleFadeStartTime = 0.4f;
	constexpr float TitleFadeDuration = 0.8f;
	constexpr float TitleStartOffsetY = 12.0f;
	constexpr float StatsStartTime = 0.88f;
	constexpr float StatFadeDuration = 0.4f;
	constexpr float StatStagger = 0.14f;
	constexpr int32 StatStageCount = 5;
	constexpr float ButtonFadeStartTime = 1.84f;
	constexpr float TotalDuration = 2.f;
}

void UGameResultWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ApplyBackground();
	RefreshPlayerIdentity();
	if (Text_ResultTitle)
	{
		ResultTitleBaseTranslation = Text_ResultTitle->GetRenderTransform().Translation;
	}
	CompleteResultIntroAnimation();
	if (WBP_ResultStatRow_Temp)
	{
		WBP_ResultStatRow_Temp->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (CurrentMatchResult != EMatchResult::None)
	{
		PlayResultIntroAnimation();
	}
}

void UGameResultWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bIsResultIntroPlaying)
	{
		return;
	}

	ResultIntroElapsedTime += InDeltaTime;
	UpdateResultIntroAnimation();
	if (ResultIntroElapsedTime >= ResultIntroAnimation::TotalDuration)
	{
		CompleteResultIntroAnimation();
	}
}

void UGameResultWidget::SetResultText(EMatchResult MatchResult)
{
	const bool bShouldPlayIntro = MatchResult != EMatchResult::None
		&& (!bHasPlayedResultIntro || CurrentMatchResult != MatchResult);
	CurrentMatchResult = MatchResult;
	ApplyResultEmblem(MatchResult);
	const UEnum* MatchResultEnum = StaticEnum<EMatchResult>();
	const FText ResultText = MatchResultEnum
		? MatchResultEnum->GetDisplayNameTextByValue(static_cast<int64>(MatchResult))
		: FText::GetEmpty();

	if (ResultTitleText)
	{
		ResultTitleText->SetText(ResultText);
	}
	if (Text_ResultTitle)
	{
		Text_ResultTitle->SetText(ResultText);
	}

	if (bShouldPlayIntro)
	{
		PlayResultIntroAnimation();
	}
}

void UGameResultWidget::PlayResultIntroAnimation()
{
	bHasPlayedResultIntro = true;
	bIsResultIntroPlaying = true;
	ResultIntroElapsedTime = 0.0f;

	if (Text_ResultTitle)
	{
		ResultTitleBaseTranslation = Text_ResultTitle->GetRenderTransform().Translation;
	}

	UImage* BackgroundImage = Image_ResultBackground
		? Image_ResultBackground.Get()
		: Image_RoleBackground.Get();
	SetWidgetRenderOpacity(BackgroundImage, 0.0f);
	SetWidgetRenderOpacity(Overlay_Main, 0.0f);
	SetWidgetRenderOpacity(Text_ResultTitle, 0.0f);
	SetWidgetRenderOpacity(ResultTitleText, 0.0f);

	TArray<float> InitialStageOpacities;
	InitialStageOpacities.Init(0.0f, ResultIntroAnimation::StatStageCount);
	SetStatsRenderOpacity(0.0f, 0.0f, InitialStageOpacities);

	if (Text_ResultTitle)
	{
		FWidgetTransform TitleTransform = Text_ResultTitle->GetRenderTransform();
		TitleTransform.Translation = ResultTitleBaseTranslation
			+ FVector2D(0.0f, ResultIntroAnimation::TitleStartOffsetY);
		Text_ResultTitle->SetRenderTransform(TitleTransform);
	}

	if (ReturnToMainMenuButton)
	{
		ReturnToMainMenuButton->SetIsEnabled(false);
		ReturnToMainMenuButton->SetRenderOpacity(0.0f);
	}

	UpdateResultIntroAnimation();
}

void UGameResultWidget::UpdateResultIntroAnimation()
{
	UImage* BackgroundImage = Image_ResultBackground
		? Image_ResultBackground.Get()
		: Image_RoleBackground.Get();
	SetWidgetRenderOpacity(
		BackgroundImage,
		GetIntroAnimationAlpha(
			ResultIntroElapsedTime,
			0.0f,
			ResultIntroAnimation::BackgroundFadeDuration));

	SetWidgetRenderOpacity(
		Overlay_Main,
		GetIntroAnimationAlpha(
			ResultIntroElapsedTime,
			ResultIntroAnimation::PanelFadeStartTime,
			ResultIntroAnimation::PanelFadeDuration));

	const float TitleAlpha = GetIntroAnimationAlpha(
		ResultIntroElapsedTime,
		ResultIntroAnimation::TitleFadeStartTime,
		ResultIntroAnimation::TitleFadeDuration);
	SetWidgetRenderOpacity(Text_ResultTitle, TitleAlpha);
	SetWidgetRenderOpacity(ResultTitleText, TitleAlpha);
	if (Text_ResultTitle)
	{
		FWidgetTransform TitleTransform = Text_ResultTitle->GetRenderTransform();
		TitleTransform.Translation = ResultTitleBaseTranslation + FVector2D(
			0.0f,
			FMath::Lerp(ResultIntroAnimation::TitleStartOffsetY, 0.0f, TitleAlpha));
		Text_ResultTitle->SetRenderTransform(TitleTransform);
	}

	TArray<float> StageOpacities;
	StageOpacities.Reserve(ResultIntroAnimation::StatStageCount);
	for (int32 StageIndex = 0; StageIndex < ResultIntroAnimation::StatStageCount; ++StageIndex)
	{
		StageOpacities.Add(GetIntroAnimationAlpha(
			ResultIntroElapsedTime,
			ResultIntroAnimation::StatsStartTime
				+ ResultIntroAnimation::StatStagger * static_cast<float>(StageIndex),
			ResultIntroAnimation::StatFadeDuration));
	}
	SetStatsRenderOpacity(StageOpacities[0], StageOpacities[0], StageOpacities);

	if (ReturnToMainMenuButton)
	{
		const float ButtonFadeDuration = ResultIntroAnimation::TotalDuration
			- ResultIntroAnimation::ButtonFadeStartTime;
		const float ButtonAlpha = GetIntroAnimationAlpha(
			ResultIntroElapsedTime,
			ResultIntroAnimation::ButtonFadeStartTime,
			ButtonFadeDuration);
		ReturnToMainMenuButton->SetRenderOpacity(ButtonAlpha);
		ReturnToMainMenuButton->SetIsEnabled(
			ResultIntroElapsedTime >= ResultIntroAnimation::TotalDuration);
	}
}

void UGameResultWidget::CompleteResultIntroAnimation()
{
	bIsResultIntroPlaying = false;
	ResultIntroElapsedTime = ResultIntroAnimation::TotalDuration;

	UImage* BackgroundImage = Image_ResultBackground
		? Image_ResultBackground.Get()
		: Image_RoleBackground.Get();
	SetWidgetRenderOpacity(BackgroundImage, 1.0f);
	SetWidgetRenderOpacity(Overlay_Main, 1.0f);
	SetWidgetRenderOpacity(Text_ResultTitle, 1.0f);
	SetWidgetRenderOpacity(ResultTitleText, 1.0f);

	if (Text_ResultTitle)
	{
		FWidgetTransform TitleTransform = Text_ResultTitle->GetRenderTransform();
		TitleTransform.Translation = ResultTitleBaseTranslation;
		Text_ResultTitle->SetRenderTransform(TitleTransform);
	}

	TArray<float> FinalStageOpacities;
	FinalStageOpacities.Init(1.0f, ResultIntroAnimation::StatStageCount);
	SetStatsRenderOpacity(1.0f, 1.0f, FinalStageOpacities);

	if (ReturnToMainMenuButton)
	{
		ReturnToMainMenuButton->SetRenderOpacity(1.0f);
		ReturnToMainMenuButton->SetIsEnabled(true);
	}
}

void UGameResultWidget::SetStatsRenderOpacity(
	const float SurvivorPrimaryOpacity,
	const float KillerPrimaryOpacity,
	const TArray<float>& StageOpacities)
{
	SetWidgetRenderOpacity(WBP_ResultStat_Survivor, SurvivorPrimaryOpacity);
	SetWidgetRenderOpacity(WBP_ResultStat_Killer, KillerPrimaryOpacity);

	if (StageOpacities.Num() < ResultIntroAnimation::StatStageCount)
	{
		return;
	}

	SetWidgetRenderOpacity(WBP_ResultStatRow_DeliveryCount, StageOpacities[1]);
	SetWidgetRenderOpacity(WBP_ResultStatRow_ValidHitCount, StageOpacities[1]);
	SetWidgetRenderOpacity(WBP_ResultStatRow_Rescue, StageOpacities[2]);
	SetWidgetRenderOpacity(WBP_ResultStatRow_DownCount, StageOpacities[2]);
	SetWidgetRenderOpacity(WBP_ResultStatRow_SelfHeal, StageOpacities[3]);
	SetWidgetRenderOpacity(WBP_ResultStatRow_CageCount, StageOpacities[3]);
	SetWidgetRenderOpacity(WBP_ResultStatRow_Caged, StageOpacities[4]);
	SetWidgetRenderOpacity(WBP_ResultStatRow_Temp, StageOpacities[4]);
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
	const USPGameResultStyleData& Style = GetResolvedStyle();
	ApplyRoleVisual(ELobbyPlayerRole::Survivor);

	if (WidgetSwitcher_Stats)
	{
		WidgetSwitcher_Stats->SetActiveWidgetIndex(ResultStatsPanelIndex::Survivor);
	}
	if (Text_StatHeader)
	{
		Text_StatHeader->SetText(NSLOCTEXT("SpaCh4", "SurvivorResultStatsHeader", "생존자 활동 기록"));
	}

	FText OutcomeText = NSLOCTEXT("SpaCh4", "SurvivorOutcomeNotEscaped", "미탈출");
	if (SurvivorStats.bKilled)
	{
		OutcomeText = NSLOCTEXT("SpaCh4", "SurvivorOutcomeKilled", "사망");
	}
	else if (SurvivorStats.bEscaped)
	{
		switch (SurvivorStats.EscapeMethod)
		{
		case ESurvivorEscapeMethod::Gate:
			OutcomeText = NSLOCTEXT("SpaCh4", "SurvivorOutcomeGate", "탈출구 탈출");
			break;
		case ESurvivorEscapeMethod::Hatch:
			OutcomeText = NSLOCTEXT("SpaCh4", "SurvivorOutcomeHatch", "개구멍 탈출");
			break;
		default:
			OutcomeText = NSLOCTEXT("SpaCh4", "SurvivorOutcomeEscaped", "탈출 성공");
			break;
		}
	}
	if (Text_PersonalOutcome)
	{
		Text_PersonalOutcome->SetText(OutcomeText);
	}

	SetPrimaryStat(
		WBP_ResultStat_Survivor,
		NSLOCTEXT("SpaCh4", "ResultStatDeliveredValue", "납품 가치"),
		NSLOCTEXT("SpaCh4", "ResultStatDeliveredValueDescription", "납품소 목표에 실제 반영된 가치"),
		SurvivorStats.DeliveredValue,
		Style.DeliveredValueIcon);
	if (WBP_ResultStatRow_DeliveryCount)
	{
		WBP_ResultStatRow_DeliveryCount->SetStat(NSLOCTEXT("SpaCh4", "ResultStatDeliveryCount", "납품 횟수"), SurvivorStats.SuccessfulDeliveryCount);
		WBP_ResultStatRow_DeliveryCount->SetStatIcon(Style.DeliveryCountIcon);
	}
	if (WBP_ResultStatRow_Rescue)
	{
		WBP_ResultStatRow_Rescue->SetStat(NSLOCTEXT("SpaCh4", "ResultStatCageRescue", "구조 횟수"), SurvivorStats.CageRescueCount);
		WBP_ResultStatRow_Rescue->SetStatIcon(Style.RescueIcon);
	}
	if (WBP_ResultStatRow_SelfHeal)
	{
		WBP_ResultStatRow_SelfHeal->SetStat(NSLOCTEXT("SpaCh4", "ResultStatSelfHeal", "자가 치료"), SurvivorStats.SelfHealCount);
		WBP_ResultStatRow_SelfHeal->SetStatIcon(Style.SelfHealIcon);
	}
	if (WBP_ResultStatRow_Caged)
	{
		WBP_ResultStatRow_Caged->SetStat(NSLOCTEXT("SpaCh4", "ResultStatCaged", "감금 횟수"), SurvivorStats.CagedCount);
		WBP_ResultStatRow_Caged->SetStatIcon(Style.CagedIcon);
	}

	if (HasStructuredStatsWidgets())
	{
		if (ScoreText)
		{
			ScoreText->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	const FString StatsText = FString::Printf(
		TEXT("납품 가치: %d\n납품 완료: %d회\n케이지 구조: %d회\n자가 치료: %d회\n감금: %d회\n최종 상태: %s"),
		SurvivorStats.DeliveredValue,
		SurvivorStats.SuccessfulDeliveryCount,
		SurvivorStats.CageRescueCount,
		SurvivorStats.SelfHealCount,
		SurvivorStats.CagedCount,
		*OutcomeText.ToString());
	if (ScoreText)
	{
		ScoreText->SetText(FText::FromString(StatsText));
	}
}

void UGameResultWidget::SetKillerStats(const FKillerMatchStats& KillerStats)
{
	const USPGameResultStyleData& Style = GetResolvedStyle();
	ApplyRoleVisual(ELobbyPlayerRole::Killer);

	if (WidgetSwitcher_Stats)
	{
		WidgetSwitcher_Stats->SetActiveWidgetIndex(ResultStatsPanelIndex::Killer);
	}
	if (Text_StatHeader)
	{
		Text_StatHeader->SetText(NSLOCTEXT("SpaCh4", "KillerResultStatsHeader", "살인마 활동 기록"));
	}
	if (Text_PersonalOutcome)
	{
		const bool bKillerWon = CurrentMatchResult == EMatchResult::KillerWin
			|| CurrentMatchResult == EMatchResult::KillerPerfectWin;
		Text_PersonalOutcome->SetText(bKillerWon
			? NSLOCTEXT("SpaCh4", "KillerOutcomeWon", "사냥 성공")
			: NSLOCTEXT("SpaCh4", "KillerOutcomeLost", "사냥 실패"));
	}

	SetPrimaryStat(
		WBP_ResultStat_Killer,
		NSLOCTEXT("SpaCh4", "ResultStatKilledSurvivors", "처치 인원"),
		NSLOCTEXT("SpaCh4", "ResultStatKilledSurvivorsDescription", "매치에서 처치한 생존자 수"),
		KillerStats.KilledSurvivorCount,
		Style.KilledSurvivorsIcon);
	if (WBP_ResultStatRow_ValidHitCount)
	{
		WBP_ResultStatRow_ValidHitCount->SetStat(NSLOCTEXT("SpaCh4", "ResultStatValidHits", "유효 타격"), KillerStats.ValidHitCount);
		WBP_ResultStatRow_ValidHitCount->SetStatIcon(Style.ValidHitIcon);
	}
	if (WBP_ResultStatRow_DownCount)
	{
		WBP_ResultStatRow_DownCount->SetStat(NSLOCTEXT("SpaCh4", "ResultStatDowns", "다운 횟수"), KillerStats.DownCount);
		WBP_ResultStatRow_DownCount->SetStatIcon(Style.DownIcon);
	}
	if (WBP_ResultStatRow_CageCount)
	{
		WBP_ResultStatRow_CageCount->SetStat(NSLOCTEXT("SpaCh4", "ResultStatKillerCages", "감금 횟수"), KillerStats.CageCount);
		WBP_ResultStatRow_CageCount->SetStatIcon(Style.CageCountIcon);
	}
	if (WBP_ResultStatRow_Temp)
	{
		WBP_ResultStatRow_Temp->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (HasStructuredStatsWidgets())
	{
		if (ScoreText)
		{
			ScoreText->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	const FString StatsText = FString::Printf(
		TEXT("유효 타격: %d회\n다운: %d회\n감금: %d회\n처치: %d명"),
		KillerStats.ValidHitCount,
		KillerStats.DownCount,
		KillerStats.CageCount,
		KillerStats.KilledSurvivorCount);
	if (ScoreText)
	{
		ScoreText->SetText(FText::FromString(StatsText));
	}
}

const USPGameResultStyleData& UGameResultWidget::GetResolvedStyle() const
{
	return SPUIStyleLibrary::ResolveGameResultStyle(VisualStyle);
}

void UGameResultWidget::ApplyBackground()
{
	UImage* BackgroundImage = Image_ResultBackground
		? Image_ResultBackground.Get()
		: Image_RoleBackground.Get();
	SetImageTexture(BackgroundImage, GetResolvedStyle().Background);
}

void UGameResultWidget::ApplyRoleVisual(const ELobbyPlayerRole PlayerRole)
{
	const USPGameResultStyleData& Style = GetResolvedStyle();
	UTexture2D* CharacterTexture = nullptr;
	if (PlayerRole == ELobbyPlayerRole::Survivor)
	{
		CharacterTexture = Style.SurvivorCharacter;
	}
	else if (PlayerRole == ELobbyPlayerRole::Killer)
	{
		CharacterTexture = Style.KillerCharacter;
	}

	SetImageTexture(Image_RoleCharacter, CharacterTexture);
}

void UGameResultWidget::ApplyResultEmblem(const EMatchResult MatchResult)
{
	const USPGameResultStyleData& Style = GetResolvedStyle();
	UTexture2D* EmblemTexture = nullptr;
	switch (MatchResult)
	{
	case EMatchResult::SurvivorPerfectWin:
		EmblemTexture = Style.SurvivorPerfectWinGrade;
		break;
	case EMatchResult::SurvivorWin:
		EmblemTexture = Style.SurvivorWinGrade;
		break;
	case EMatchResult::SurvivorMinorWin:
		EmblemTexture = Style.SurvivorMinorWinGrade;
		break;
	case EMatchResult::KillerWin:
		EmblemTexture = Style.KillerWinGrade;
		break;
	case EMatchResult::KillerPerfectWin:
		EmblemTexture = Style.KillerPerfectWinGrade;
		break;
	default:
		break;
	}

	SetImageTexture(Image_ResultEmblem, EmblemTexture);
}

void UGameResultWidget::RefreshPlayerIdentity()
{
	const APlayerController* PlayerController = GetOwningPlayer();
	const ALDPlayerState* LDPlayerState = PlayerController
		? PlayerController->GetPlayerState<ALDPlayerState>()
		: nullptr;
	if (!LDPlayerState)
	{
		return;
	}

	if (Text_PlayerNickname)
	{
		Text_PlayerNickname->SetText(FText::FromString(LDPlayerState->GetPlayerName()));
	}
	if (Text_PlayerRole)
	{
		FText RoleText = NSLOCTEXT("SpaCh4", "ResultRoleUnknown", "역할 미정");
		if (LDPlayerState->GetPlayerRole() == ELobbyPlayerRole::Survivor)
		{
			RoleText = NSLOCTEXT("SpaCh4", "ResultRoleSurvivor", "생존자");
		}
		else if (LDPlayerState->GetPlayerRole() == ELobbyPlayerRole::Killer)
		{
			RoleText = NSLOCTEXT("SpaCh4", "ResultRoleKiller", "살인마");
		}
		Text_PlayerRole->SetText(RoleText);
	}

	ApplyRoleVisual(LDPlayerState->GetPlayerRole());
}

void UGameResultWidget::SetPrimaryStat(
	USPResultPrimaryStatWidget* PrimaryStatWidget,
	const FText& Label,
	const FText& Description,
	const int32 Value,
	UTexture2D* IconTexture)
{
	if (!PrimaryStatWidget)
	{
		return;
	}

	PrimaryStatWidget->SetStat(Label, Value);
	PrimaryStatWidget->SetDescription(Description);
	PrimaryStatWidget->SetStatIcon(IconTexture);
}

bool UGameResultWidget::HasStructuredStatsWidgets() const
{
	return WBP_ResultStat_Survivor
		|| WBP_ResultStat_Killer
		|| WBP_ResultStatRow_DeliveryCount
		|| WBP_ResultStatRow_ValidHitCount;
}

float UGameResultWidget::GetIntroAnimationAlpha(
	const float ElapsedTime,
	const float StartTime,
	const float Duration)
{
	if (Duration <= UE_SMALL_NUMBER)
	{
		return ElapsedTime >= StartTime ? 1.0f : 0.0f;
	}

	const float LinearAlpha = FMath::Clamp((ElapsedTime - StartTime) / Duration, 0.0f, 1.0f);
	const float InverseAlpha = 1.0f - LinearAlpha;
	return 1.0f - InverseAlpha * InverseAlpha * InverseAlpha;
}

void UGameResultWidget::SetWidgetRenderOpacity(UWidget* Widget, const float Opacity)
{
	if (Widget)
	{
		Widget->SetRenderOpacity(Opacity);
	}
}

void UGameResultWidget::SetImageTexture(UImage* Image, UTexture2D* Texture)
{
	if (!Image)
	{
		return;
	}

	if (Texture)
	{
		Image->SetBrushFromTexture(Texture, false);
		Image->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	else
	{
		Image->SetVisibility(ESlateVisibility::Collapsed);
	}
}

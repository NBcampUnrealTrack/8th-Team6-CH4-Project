#include "UI/SPResultStatRowWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"

void USPResultStatRowWidget::SetStat(const FText& Label, const int32 Value)
{
	SetStatText(Label, FText::AsNumber(Value));
}

void USPResultStatRowWidget::SetStatText(const FText& Label, const FText& Value)
{
	if (Text_StatLabel)
	{
		Text_StatLabel->SetText(Label);
	}
	if (Text_Value)
	{
		Text_Value->SetText(Value);
	}
}

void USPResultStatRowWidget::SetStatIcon(UTexture2D* IconTexture)
{
	if (Image_Icon && IconTexture)
	{
		Image_Icon->SetBrushFromTexture(IconTexture, false);
	}
}

#include "UI/SPResultPrimaryStatWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"

void USPResultPrimaryStatWidget::SetStat(const FText& Label, const int32 Value)
{
	SetStatText(Label, FText::AsNumber(Value));
}

void USPResultPrimaryStatWidget::SetStatText(const FText& Label, const FText& Value)
{
	if (Text_Label)
	{
		Text_Label->SetText(Label);
	}
	if (Text_Value)
	{
		Text_Value->SetText(Value);
	}
}

void USPResultPrimaryStatWidget::SetDescription(const FText& Description)
{
	if (Text_Description)
	{
		Text_Description->SetText(Description);
	}
}

void USPResultPrimaryStatWidget::SetStatIcon(UTexture2D* IconTexture)
{
	if (Image_Icon && IconTexture)
	{
		Image_Icon->SetBrushFromTexture(IconTexture, false);
	}
}

// Copyright uuuuzz 2024-2026. All Rights Reserved.

#include "Tools/Widget/EditWidgetBlueprintTool.h"
#include "Utils/McpAssetModifier.h"
#include "UEBridgeMCPEditor.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/OverlaySlot.h"
#include "Engine/Texture2D.h"
#include "Engine/FontFace.h"
#include "Styling/SlateTypes.h"
#include "ScopedTransaction.h"

FString UEditWidgetBlueprintTool::GetToolDescription() const
{
	return TEXT("Edit widgets in a WidgetBlueprint: canvas slot layout, image textures, sizes, text color/size, progress bar fills.");
}

TMap<FString, FMcpSchemaProperty> UEditWidgetBlueprintTool::GetInputSchema() const
{
	TMap<FString, FMcpSchemaProperty> Schema;

	Schema.Add(TEXT("asset_path"), FMcpSchemaProperty::Make(TEXT("string"),
		TEXT("WidgetBlueprint asset path"), true));

	Schema.Add(TEXT("edits"), FMcpSchemaProperty::MakeArray(
		TEXT("Array of edit operations"), TEXT("object"), true));

	return Schema;
}

TArray<FString> UEditWidgetBlueprintTool::GetRequiredParams() const
{
	return { TEXT("asset_path"), TEXT("edits") };
}

static bool ParseVector2(const TArray<TSharedPtr<FJsonValue>>* Arr, FVector2D& OutVec)
{
	if (!Arr || Arr->Num() < 2)
	{
		return false;
	}
	OutVec.X = static_cast<float>((*Arr)[0]->AsNumber());
	OutVec.Y = static_cast<float>((*Arr)[1]->AsNumber());
	return true;
}

UWidget* UEditWidgetBlueprintTool::FindWidgetByName(UWidgetBlueprint* WidgetBP, const FString& WidgetName, int32 Occurrence)
{
	if (!WidgetBP || !WidgetBP->WidgetTree)
	{
		return nullptr;
	}

	int32 MatchIndex = 0;
	UWidget* Found = nullptr;
	WidgetBP->WidgetTree->ForEachWidget([&](UWidget* Widget)
	{
		if (Found || !Widget || !Widget->GetName().Equals(WidgetName, ESearchCase::IgnoreCase))
		{
			return;
		}

		if (MatchIndex == Occurrence)
		{
			Found = Widget;
		}
		++MatchIndex;
	});
	return Found;
}

bool UEditWidgetBlueprintTool::RemoveWidget(UWidgetBlueprint* WidgetBP, UWidget* Widget)
{
	if (!WidgetBP || !Widget || !WidgetBP->WidgetTree)
	{
		return false;
	}

	if (UPanelWidget* Parent = Widget->GetParent())
	{
		Parent->RemoveChild(Widget);
	}

	WidgetBP->WidgetTree->RemoveWidget(Widget);
	return true;
}

bool UEditWidgetBlueprintTool::ApplyCanvasSlot(UWidget* Widget, const TSharedPtr<FJsonObject>& SlotObj, FString& OutError)
{
	if (!Widget || !SlotObj.IsValid())
	{
		return true;
	}

	UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot);
	if (!CanvasSlot)
	{
		OutError = FString::Printf(TEXT("Widget '%s' is not in a CanvasPanel slot"), *Widget->GetName());
		return false;
	}

	const TSharedPtr<FJsonObject>* AnchorsObj = nullptr;
	if (SlotObj->TryGetObjectField(TEXT("anchors"), AnchorsObj) && AnchorsObj && (*AnchorsObj).IsValid())
	{
		FAnchors Anchors;
		const TArray<TSharedPtr<FJsonValue>>* MinArr = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* MaxArr = nullptr;
		if ((*AnchorsObj)->TryGetArrayField(TEXT("min"), MinArr))
		{
			FVector2D MinVec;
			if (ParseVector2(MinArr, MinVec))
			{
				Anchors.Minimum = MinVec;
			}
		}
		if ((*AnchorsObj)->TryGetArrayField(TEXT("max"), MaxArr))
		{
			FVector2D MaxVec;
			if (ParseVector2(MaxArr, MaxVec))
			{
				Anchors.Maximum = MaxVec;
			}
		}
		CanvasSlot->SetAnchors(Anchors);
	}

	const TArray<TSharedPtr<FJsonValue>>* AlignArr = nullptr;
	if (SlotObj->TryGetArrayField(TEXT("alignment"), AlignArr))
	{
		FVector2D Alignment;
		if (ParseVector2(AlignArr, Alignment))
		{
			CanvasSlot->SetAlignment(Alignment);
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* PosArr = nullptr;
	if (SlotObj->TryGetArrayField(TEXT("position"), PosArr))
	{
		FVector2D Position;
		if (ParseVector2(PosArr, Position))
		{
			CanvasSlot->SetPosition(Position);
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* SizeArr = nullptr;
	if (SlotObj->TryGetArrayField(TEXT("size"), SizeArr))
	{
		FVector2D Size;
		if (ParseVector2(SizeArr, Size))
		{
			CanvasSlot->SetSize(Size);
		}
	}

	bool bAutoSize = false;
	if (SlotObj->TryGetBoolField(TEXT("auto_size"), bAutoSize))
	{
		CanvasSlot->SetAutoSize(bAutoSize);
	}

	int32 ZOrder = 0;
	if (SlotObj->TryGetNumberField(TEXT("z_order"), ZOrder))
	{
		CanvasSlot->SetZOrder(ZOrder);
	}

	return true;
}

static EHorizontalAlignment ParseHorizontalAlignment(const FString& Value)
{
	if (Value.Equals(TEXT("Left"), ESearchCase::IgnoreCase))
	{
		return HAlign_Left;
	}
	if (Value.Equals(TEXT("Center"), ESearchCase::IgnoreCase))
	{
		return HAlign_Center;
	}
	if (Value.Equals(TEXT("Right"), ESearchCase::IgnoreCase))
	{
		return HAlign_Right;
	}
	return HAlign_Fill;
}

static EVerticalAlignment ParseVerticalAlignment(const FString& Value)
{
	if (Value.Equals(TEXT("Top"), ESearchCase::IgnoreCase))
	{
		return VAlign_Top;
	}
	if (Value.Equals(TEXT("Center"), ESearchCase::IgnoreCase))
	{
		return VAlign_Center;
	}
	if (Value.Equals(TEXT("Bottom"), ESearchCase::IgnoreCase))
	{
		return VAlign_Bottom;
	}
	return VAlign_Fill;
}

static bool ApplyPanelSlot(UWidget* Widget, const TSharedPtr<FJsonObject>& SlotObj, FString& OutError)
{
	if (!Widget || !SlotObj.IsValid())
	{
		return true;
	}

	auto ApplyHBoxSlot = [&SlotObj](UHorizontalBoxSlot* Slot)
	{
		FString SizeRule;
		if (SlotObj->TryGetStringField(TEXT("size_rule"), SizeRule))
		{
			FSlateChildSize ChildSize = Slot->GetSize();
			ChildSize.SizeRule = SizeRule.Equals(TEXT("Fill"), ESearchCase::IgnoreCase)
				? ESlateSizeRule::Fill
				: ESlateSizeRule::Automatic;
			Slot->SetSize(ChildSize);
		}

		double SizeValue = 0.0;
		if (SlotObj->TryGetNumberField(TEXT("size_value"), SizeValue) && SizeValue > 0.0)
		{
			FSlateChildSize ChildSize = Slot->GetSize();
			ChildSize.Value = static_cast<float>(SizeValue);
			Slot->SetSize(ChildSize);
		}

		FString HAlign;
		if (SlotObj->TryGetStringField(TEXT("horizontal_alignment"), HAlign))
		{
			Slot->SetHorizontalAlignment(ParseHorizontalAlignment(HAlign));
		}

		FString VAlign;
		if (SlotObj->TryGetStringField(TEXT("vertical_alignment"), VAlign))
		{
			Slot->SetVerticalAlignment(ParseVerticalAlignment(VAlign));
		}

		const TSharedPtr<FJsonObject>* PadObj = nullptr;
		if (SlotObj->TryGetObjectField(TEXT("padding"), PadObj) && PadObj && (*PadObj).IsValid())
		{
			double Left = 0.0;
			double Top = 0.0;
			double Right = 0.0;
			double Bottom = 0.0;
			(*PadObj)->TryGetNumberField(TEXT("left"), Left);
			(*PadObj)->TryGetNumberField(TEXT("top"), Top);
			(*PadObj)->TryGetNumberField(TEXT("right"), Right);
			(*PadObj)->TryGetNumberField(TEXT("bottom"), Bottom);
			Slot->SetPadding(FMargin(static_cast<float>(Left), static_cast<float>(Top),
				static_cast<float>(Right), static_cast<float>(Bottom)));
		}
	};

	auto ApplyVBoxSlot = [&SlotObj](UVerticalBoxSlot* Slot)
	{
		FString SizeRule;
		if (SlotObj->TryGetStringField(TEXT("size_rule"), SizeRule))
		{
			FSlateChildSize ChildSize = Slot->GetSize();
			ChildSize.SizeRule = SizeRule.Equals(TEXT("Fill"), ESearchCase::IgnoreCase)
				? ESlateSizeRule::Fill
				: ESlateSizeRule::Automatic;
			Slot->SetSize(ChildSize);
		}

		FString HAlign;
		if (SlotObj->TryGetStringField(TEXT("horizontal_alignment"), HAlign))
		{
			Slot->SetHorizontalAlignment(ParseHorizontalAlignment(HAlign));
		}

		FString VAlign;
		if (SlotObj->TryGetStringField(TEXT("vertical_alignment"), VAlign))
		{
			Slot->SetVerticalAlignment(ParseVerticalAlignment(VAlign));
		}
	};

	if (UHorizontalBoxSlot* HSlot = Cast<UHorizontalBoxSlot>(Widget->Slot))
	{
		ApplyHBoxSlot(HSlot);
		return true;
	}

	if (UVerticalBoxSlot* VSlot = Cast<UVerticalBoxSlot>(Widget->Slot))
	{
		ApplyVBoxSlot(VSlot);
		return true;
	}

	return true;
}

static bool ApplyOverlaySlot(UWidget* Widget, const TSharedPtr<FJsonObject>& SlotObj, FString& OutError)
{
	if (!Widget || !SlotObj.IsValid())
	{
		return true;
	}

	UOverlaySlot* Slot = Cast<UOverlaySlot>(Widget->Slot);
	if (!Slot)
	{
		OutError = FString::Printf(TEXT("Widget '%s' is not in an Overlay slot"), *Widget->GetName());
		return false;
	}

	FString HAlign;
	if (SlotObj->TryGetStringField(TEXT("horizontal_alignment"), HAlign))
	{
		Slot->SetHorizontalAlignment(ParseHorizontalAlignment(HAlign));
	}

	FString VAlign;
	if (SlotObj->TryGetStringField(TEXT("vertical_alignment"), VAlign))
	{
		Slot->SetVerticalAlignment(ParseVerticalAlignment(VAlign));
	}

	const TSharedPtr<FJsonObject>* PadObj = nullptr;
	if (SlotObj->TryGetObjectField(TEXT("padding"), PadObj) && PadObj && (*PadObj).IsValid())
	{
		double Left = 0.0;
		double Top = 0.0;
		double Right = 0.0;
		double Bottom = 0.0;
		(*PadObj)->TryGetNumberField(TEXT("left"), Left);
		(*PadObj)->TryGetNumberField(TEXT("top"), Top);
		(*PadObj)->TryGetNumberField(TEXT("right"), Right);
		(*PadObj)->TryGetNumberField(TEXT("bottom"), Bottom);
		Slot->SetPadding(FMargin(static_cast<float>(Left), static_cast<float>(Top),
			static_cast<float>(Right), static_cast<float>(Bottom)));
	}

	return true;
}

static UTexture2D* LoadTexture(const FString& TexturePath)
{
	if (TexturePath.IsEmpty())
	{
		return nullptr;
	}

	FString Path = TexturePath;
	if (!Path.Contains(TEXT(".")))
	{
		const FString AssetName = FPackageName::GetLongPackageAssetName(TexturePath);
		Path = FString::Printf(TEXT("%s.%s"), *TexturePath, *AssetName);
	}

	return LoadObject<UTexture2D>(nullptr, *Path);
}

static UFontFace* LoadFontFace(const FString& FontPath)
{
	if (FontPath.IsEmpty())
	{
		return nullptr;
	}

	FString Path = FontPath;
	if (!Path.Contains(TEXT(".")))
	{
		const FString AssetName = FPackageName::GetLongPackageAssetName(FontPath);
		Path = FString::Printf(TEXT("%s.%s"), *FontPath, *AssetName);
	}

	return LoadObject<UFontFace>(nullptr, *Path);
}

bool UEditWidgetBlueprintTool::ApplyProperties(UWidget* Widget, const TSharedPtr<FJsonObject>& PropsObj, FString& OutError)
{
	if (!Widget || !PropsObj.IsValid())
	{
		return true;
	}

	FString ImageTexture;
	if (PropsObj->TryGetStringField(TEXT("image_texture"), ImageTexture))
	{
		if (UImage* Image = Cast<UImage>(Widget))
		{
			if (UTexture2D* Tex = LoadTexture(ImageTexture))
			{
				Image->SetBrushFromTexture(Tex, true);
			}
			else
			{
				OutError = FString::Printf(TEXT("Texture not found: %s"), *ImageTexture);
				return false;
			}
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* DesiredSizeArr = nullptr;
	if (PropsObj->TryGetArrayField(TEXT("desired_size"), DesiredSizeArr))
	{
		FVector2D Size;
		if (ParseVector2(DesiredSizeArr, Size))
		{
		if (UImage* Image = Cast<UImage>(Widget))
		{
			FSlateBrush Brush = Image->GetBrush();
			Brush.ImageSize = Size;
			Image->SetBrush(Brush);
		}
	}

	if (UImage* Image = Cast<UImage>(Widget))
	{
		const TArray<TSharedPtr<FJsonValue>>* ColorArr = nullptr;
		if (PropsObj->TryGetArrayField(TEXT("image_color"), ColorArr) && ColorArr->Num() >= 3)
		{
			const float R = static_cast<float>((*ColorArr)[0]->AsNumber());
			const float G = static_cast<float>((*ColorArr)[1]->AsNumber());
			const float B = static_cast<float>((*ColorArr)[2]->AsNumber());
			const float A = ColorArr->Num() >= 4 ? static_cast<float>((*ColorArr)[3]->AsNumber()) : 1.0f;
			Image->SetColorAndOpacity(FLinearColor(R, G, B, A));
		}
	}
	}

	if (UTextBlock* Text = Cast<UTextBlock>(Widget))
	{
		FString TextValue;
		if (PropsObj->TryGetStringField(TEXT("text"), TextValue))
		{
			Text->SetText(FText::FromString(TextValue));
		}

		const TArray<TSharedPtr<FJsonValue>>* ColorArr = nullptr;
		if (PropsObj->TryGetArrayField(TEXT("text_color"), ColorArr) && ColorArr->Num() >= 3)
		{
			const float R = static_cast<float>((*ColorArr)[0]->AsNumber());
			const float G = static_cast<float>((*ColorArr)[1]->AsNumber());
			const float B = static_cast<float>((*ColorArr)[2]->AsNumber());
			const float A = ColorArr->Num() >= 4 ? static_cast<float>((*ColorArr)[3]->AsNumber()) : 1.0f;
			Text->SetColorAndOpacity(FSlateColor(FLinearColor(R, G, B, A)));
		}

		double FontSize = 0.0;
		if (PropsObj->TryGetNumberField(TEXT("font_size"), FontSize) && FontSize > 0.0)
		{
			FSlateFontInfo FontInfo = Text->GetFont();
			FontInfo.Size = static_cast<int32>(FontSize);
			Text->SetFont(FontInfo);
		}

		FString FontPath;
		if (PropsObj->TryGetStringField(TEXT("font_path"), FontPath))
		{
			if (UFontFace* FontFace = LoadFontFace(FontPath))
			{
				const float ResolvedSize = FontSize > 0.0
					? static_cast<float>(FontSize)
					: Text->GetFont().Size;
				FSlateFontInfo FontInfo;
				FontInfo.FontObject = FontFace;
				FontInfo.Size = ResolvedSize;
				Text->SetFont(FontInfo);
			}
			else
			{
				OutError = FString::Printf(TEXT("Font not found: %s"), *FontPath);
				return false;
			}
		}
	}

	if (UProgressBar* Progress = Cast<UProgressBar>(Widget))
	{
		FString FillTex;
		if (PropsObj->TryGetStringField(TEXT("fill_texture"), FillTex))
		{
			if (UTexture2D* Tex = LoadTexture(FillTex))
			{
				FProgressBarStyle Style = Progress->GetWidgetStyle();
				Style.FillImage.SetResourceObject(Tex);
				Progress->SetWidgetStyle(Style);
			}
		}

		FString BgTex;
		if (PropsObj->TryGetStringField(TEXT("background_texture"), BgTex))
		{
			if (UTexture2D* Tex = LoadTexture(BgTex))
			{
				FProgressBarStyle Style = Progress->GetWidgetStyle();
				Style.BackgroundImage.SetResourceObject(Tex);
				Progress->SetWidgetStyle(Style);
			}
		}

		double Percent = -1.0;
		if (PropsObj->TryGetNumberField(TEXT("percent"), Percent) && Percent >= 0.0)
		{
			Progress->SetPercent(static_cast<float>(Percent));
		}
	}

	FString VisibilityStr;
	if (PropsObj->TryGetStringField(TEXT("visibility"), VisibilityStr))
	{
		if (VisibilityStr.Equals(TEXT("Visible"), ESearchCase::IgnoreCase))
		{
			Widget->SetVisibility(ESlateVisibility::Visible);
		}
		else if (VisibilityStr.Equals(TEXT("Collapsed"), ESearchCase::IgnoreCase))
		{
			Widget->SetVisibility(ESlateVisibility::Collapsed);
		}
		else if (VisibilityStr.Equals(TEXT("Hidden"), ESearchCase::IgnoreCase))
		{
			Widget->SetVisibility(ESlateVisibility::Hidden);
		}
		else
		{
			Widget->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
	}

	return true;
}

FMcpToolResult UEditWidgetBlueprintTool::Execute(
	const TSharedPtr<FJsonObject>& Arguments,
	const FMcpToolContext& Context)
{
	const FString AssetPath = GetStringArgOrDefault(Arguments, TEXT("asset_path"));
	const TArray<TSharedPtr<FJsonValue>>* EditsArray = nullptr;
	if (AssetPath.IsEmpty() || !Arguments->TryGetArrayField(TEXT("edits"), EditsArray) || !EditsArray)
	{
		return FMcpToolResult::Error(TEXT("asset_path and edits array are required"));
	}

	FString LoadError;
	UWidgetBlueprint* WidgetBP = FMcpAssetModifier::LoadAssetByPath<UWidgetBlueprint>(AssetPath, LoadError);
	if (!WidgetBP)
	{
		return FMcpToolResult::Error(LoadError);
	}

	TSharedPtr<FScopedTransaction> Transaction = FMcpAssetModifier::BeginTransaction(
		FText::Format(NSLOCTEXT("MCP", "EditWidgetBP", "Edit widgets in {0}"), FText::FromString(AssetPath)));
	FMcpAssetModifier::MarkModified(WidgetBP);

	TArray<TSharedPtr<FJsonValue>> Results;
	int32 Applied = 0;

	for (const TSharedPtr<FJsonValue>& EditValue : *EditsArray)
	{
		const TSharedPtr<FJsonObject> EditObj = EditValue->AsObject();
		if (!EditObj.IsValid())
		{
			continue;
		}

		const FString WidgetName = GetStringArgOrDefault(EditObj, TEXT("widget_name"));
		if (WidgetName.IsEmpty())
		{
			continue;
		}

		int32 Occurrence = 0;
		EditObj->TryGetNumberField(TEXT("occurrence"), Occurrence);

		FString Action;
		EditObj->TryGetStringField(TEXT("action"), Action);
		if (Action.Equals(TEXT("remove"), ESearchCase::IgnoreCase))
		{
			UWidget* Widget = FindWidgetByName(WidgetBP, WidgetName, Occurrence);
			if (!Widget)
			{
				continue;
			}

			if (!RemoveWidget(WidgetBP, Widget))
			{
				return FMcpToolResult::Error(FString::Printf(TEXT("Failed to remove widget: %s"), *WidgetName));
			}

			TSharedPtr<FJsonObject> ResultObj = MakeShareable(new FJsonObject);
			ResultObj->SetStringField(TEXT("widget_name"), WidgetName);
			ResultObj->SetStringField(TEXT("action"), TEXT("remove"));
			ResultObj->SetBoolField(TEXT("success"), true);
			Results.Add(MakeShareable(new FJsonValueObject(ResultObj)));
			++Applied;
			continue;
		}

		UWidget* Widget = FindWidgetByName(WidgetBP, WidgetName, Occurrence);
		if (!Widget)
		{
			return FMcpToolResult::Error(FString::Printf(TEXT("Widget not found: %s"), *WidgetName));
		}

		const TSharedPtr<FJsonObject>* SlotObj = nullptr;
		if (EditObj->TryGetObjectField(TEXT("canvas_slot"), SlotObj) && SlotObj)
		{
			FString SlotError;
			if (!ApplyCanvasSlot(Widget, *SlotObj, SlotError))
			{
				return FMcpToolResult::Error(SlotError);
			}
		}

		const TSharedPtr<FJsonObject>* PanelSlotObj = nullptr;
		if (EditObj->TryGetObjectField(TEXT("panel_slot"), PanelSlotObj) && PanelSlotObj)
		{
			FString PanelSlotError;
			if (!ApplyPanelSlot(Widget, *PanelSlotObj, PanelSlotError))
			{
				return FMcpToolResult::Error(PanelSlotError);
			}
		}

		const TSharedPtr<FJsonObject>* OverlaySlotObj = nullptr;
		if (EditObj->TryGetObjectField(TEXT("overlay_slot"), OverlaySlotObj) && OverlaySlotObj)
		{
			FString OverlaySlotError;
			if (!ApplyOverlaySlot(Widget, *OverlaySlotObj, OverlaySlotError))
			{
				return FMcpToolResult::Error(OverlaySlotError);
			}
		}

		const TSharedPtr<FJsonObject>* PropsObj = nullptr;
		if (EditObj->TryGetObjectField(TEXT("properties"), PropsObj) && PropsObj)
		{
			FString PropError;
			if (!ApplyProperties(Widget, *PropsObj, PropError))
			{
				return FMcpToolResult::Error(PropError);
			}
		}

		TSharedPtr<FJsonObject> ResultObj = MakeShareable(new FJsonObject);
		ResultObj->SetStringField(TEXT("widget_name"), WidgetName);
		ResultObj->SetBoolField(TEXT("success"), true);
		Results.Add(MakeShareable(new FJsonValueObject(ResultObj)));
		++Applied;
	}

	FMcpAssetModifier::MarkPackageDirty(WidgetBP);

	TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
	Response->SetStringField(TEXT("asset_path"), AssetPath);
	Response->SetNumberField(TEXT("applied"), Applied);
	Response->SetArrayField(TEXT("results"), Results);
	Response->SetBoolField(TEXT("needs_compile"), true);
	Response->SetBoolField(TEXT("needs_save"), true);

	return FMcpToolResult::Json(Response);
}

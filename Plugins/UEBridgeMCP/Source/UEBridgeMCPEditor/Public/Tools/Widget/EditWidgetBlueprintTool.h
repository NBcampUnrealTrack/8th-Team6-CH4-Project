// Copyright uuuuzz 2024-2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tools/McpToolBase.h"
#include "EditWidgetBlueprintTool.generated.h"

class UWidget;
class UWidgetBlueprint;

/**
 * Edit existing widgets in a WidgetBlueprint (canvas slots, brushes, sizes, text).
 */
UCLASS()
class UEBRIDGEMCPEDITOR_API UEditWidgetBlueprintTool : public UMcpToolBase
{
	GENERATED_BODY()

public:
	virtual FString GetToolName() const override { return TEXT("edit-widget-blueprint"); }
	virtual FString GetToolDescription() const override;
	virtual TMap<FString, FMcpSchemaProperty> GetInputSchema() const override;
	virtual TArray<FString> GetRequiredParams() const override;
	virtual FMcpToolResult Execute(
		const TSharedPtr<FJsonObject>& Arguments,
		const FMcpToolContext& Context) override;
	virtual bool RequiresGameThread() const override { return true; }

private:
	static UWidget* FindWidgetByName(UWidgetBlueprint* WidgetBP, const FString& WidgetName, int32 Occurrence = 0);
	static bool RemoveWidget(UWidgetBlueprint* WidgetBP, UWidget* Widget);
	static bool ApplyCanvasSlot(UWidget* Widget, const TSharedPtr<FJsonObject>& SlotObj, FString& OutError);
	static bool ApplyProperties(UWidget* Widget, const TSharedPtr<FJsonObject>& PropsObj, FString& OutError);
};

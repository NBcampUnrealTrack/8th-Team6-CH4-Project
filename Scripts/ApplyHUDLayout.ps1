# DBD-inspired HUD layout: left-center teammates, bottom-left delivery, bottom-center inventory, bottom-right perks
param([string]$BaseUrl = "http://127.0.0.1:8080/mcp")

$ErrorActionPreference = "Stop"
$T = "/Game/UI/HUD"
$FontSemiBold = "/Game/UI/HUD/Fonts/Rajdhani-SemiBold"
$FontMedium = "/Game/UI/HUD/Fonts/Rajdhani-Medium"

function Invoke-McpRaw {
    param([string]$Method, [object]$Params, [string]$SessionId = $null)
    $body = @{ jsonrpc = "2.0"; id = 1; method = $Method; params = $Params } | ConvertTo-Json -Depth 40 -Compress
    $headers = @{ "Content-Type" = "application/json"; "Accept" = "application/json, text/event-stream" }
    if ($SessionId) { $headers["Mcp-Session-Id"] = $SessionId }
    return Invoke-WebRequest -Uri $BaseUrl -Method POST -Headers $headers -Body $body -UseBasicParsing
}

$init = Invoke-McpRaw -Method "initialize" -Params @{
    protocolVersion = "2024-11-05"; capabilities = @{}; clientInfo = @{ name = "HUDLayoutDBD"; version = "3.0" }
}
$session = $init.Headers["Mcp-Session-Id"]
Invoke-McpRaw -Method "notifications/initialized" -Params @{} -SessionId $session | Out-Null

function Edit-Wbp([string]$AssetPath, [array]$Edits) {
    $resp = Invoke-McpRaw -Method "tools/call" -Params @{
        name = "edit-widget-blueprint"
        arguments = @{ asset_path = $AssetPath; edits = $Edits }
    } -SessionId $session
    if ($resp.Content -match '"isError"\s*:\s*true') {
        Write-Host $resp.Content
        throw "edit-widget-blueprint failed for $AssetPath"
    }
    Write-Host "[edit] $AssetPath ($($Edits.Count) ops)"
}

function Invoke-McpTool([string]$Name, [object]$Arguments) {
    $resp = Invoke-McpRaw -Method "tools/call" -Params @{
        name = $Name; arguments = $Arguments
    } -SessionId $session
    if ($resp.Content -match '"isError"\s*:\s*true') {
        Write-Host $resp.Content
        throw "$Name failed"
    }
    return $resp.Content
}

function Test-AssetExists([string]$AssetPath) {
    try {
        $resp = Invoke-McpRaw -Method "tools/call" -Params @{
            name = "query-asset"
            arguments = @{ asset_path = $AssetPath }
        } -SessionId $session
        return ($resp.Content -notmatch '"isError"\s*:\s*true' -and $resp.Content -match '"class"')
    }
    catch {
        return $false
    }
}

function Resolve-HudTexture {
    param([string[]]$Candidates, [string]$Fallback)
    foreach ($candidate in $Candidates) {
        if (Test-AssetExists $candidate) {
            Write-Host "[texture] using $candidate"
            return $candidate
        }
    }
    Write-Host "[texture] fallback $Fallback"
    return $Fallback
}

function Get-WbpWidgetNames([string]$AssetPath) {
    $resp = Invoke-McpRaw -Method "tools/call" -Params @{
        name = "inspect-widget-blueprint"
        arguments = @{ asset_path = $AssetPath; depth_limit = 1 }
    } -SessionId $session
    $json = $resp.Content | ConvertFrom-Json
    $names = $json.result.structuredContent.all_widgets
    if ($names) { return @($names) }
    return @()
}

function Add-WidgetIfMissing {
    param(
        [string]$AssetPath,
        [string]$WidgetClass,
        [string]$WidgetName,
        [string]$ParentWidget,
        [string[]]$ExistingNames
    )
    if ($ExistingNames -contains $WidgetName) { return $ExistingNames }
    Invoke-McpTool "add-widget" @{
        asset_path   = $AssetPath
        widget_class = $WidgetClass
        widget_name  = $WidgetName
        parent_widget = $ParentWidget
    } | Out-Null
    Write-Host "[add-widget] $WidgetName -> $ParentWidget"
    return ($ExistingNames + $WidgetName)
}

$DeliverySegments = 10
$hudPath = "/Game/Blueprints/UI/WBP_GameHUD"
$HudPad = 12

function Add-PanelWithBg {
    param(
        [System.Collections.Generic.List[object]]$Edits,
        [string]$BgName,
        [string]$PanelName,
        [array]$AnchorMin,
        [array]$AnchorMax,
        [array]$Align,
        [int]$PosX,
        [int]$PosY,
        [int]$ContentW,
        [int]$ContentH,
        [string]$BgTex,
        [int]$ZPanel = 10
    )
    $bgW = $ContentW + $HudPad * 2
    $bgH = $ContentH + $HudPad * 2
    $bgX = switch ($Align[0]) {
        0 { $PosX - $HudPad }
        0.5 { $PosX }
        1 { $PosX + $HudPad }
        Default { $PosX - $HudPad }
    }
    $bgY = switch ($Align[1]) {
        0 { $PosY + $HudPad }
        0.5 { $PosY }
        1 { $PosY - $HudPad }
        Default { $PosY }
    }

    $Edits.Add(@{
        widget_name = $BgName
        properties  = @{
            image_texture = $BgTex
            desired_size  = @($bgW, $bgH)
            visibility    = "HitTestInvisible"
            image_color   = @(1.0, 1.0, 1.0, 1.0)
        }
        canvas_slot = @{
            anchors   = @{ min = $AnchorMin; max = $AnchorMax }
            alignment = $Align
            position  = @($bgX, $bgY)
            size      = @($bgW, $bgH)
            auto_size = $false
            z_order   = 5
        }
    })
    $Edits.Add(@{
        widget_name = $PanelName
        canvas_slot = @{
            anchors   = @{ min = $AnchorMin; max = $AnchorMax }
            alignment = $Align
            position  = @($PosX, $PosY)
            size      = @($ContentW, $ContentH)
            auto_size = $false
            z_order   = $ZPanel
        }
    })
}

function Get-WidgetOccurrenceCount([string[]]$Names, [string]$WidgetName) {
    return @($Names | Where-Object { $_ -eq $WidgetName }).Count
}

function Remove-AllDuplicateWidgets {
    param([string]$AssetPath, [string]$WidgetName)
    $names = Get-WbpWidgetNames $AssetPath
    while ((Get-WidgetOccurrenceCount $names $WidgetName) -gt 1) {
        Edit-Wbp $AssetPath @(@{
            widget_name = $WidgetName
            action      = "remove"
            occurrence  = 1
        })
        $names = Get-WbpWidgetNames $AssetPath
    }
}

function Remove-WidgetIfExists {
    param([string]$AssetPath, [string]$WidgetName)
    $names = Get-WbpWidgetNames $AssetPath
    if ($names -contains $WidgetName) {
        Edit-Wbp $AssetPath @(@{
            widget_name = $WidgetName
            action      = "remove"
            occurrence  = 0
        })
    }
}

$widgetNames = Get-WbpWidgetNames $hudPath
Remove-WidgetIfExists $hudPath "DeliveryRowA"
Remove-WidgetIfExists $hudPath "DeliveryRowB"
$widgetNames = Get-WbpWidgetNames $hudPath
Remove-AllDuplicateWidgets $hudPath "DeliveryLineA"
Remove-AllDuplicateWidgets $hudPath "DeliveryLineB"
$widgetNames = Get-WbpWidgetNames $hudPath
Write-Host "[cleanup] delivery rows: A=$(Get-WidgetOccurrenceCount $widgetNames 'DeliveryLineA') B=$(Get-WidgetOccurrenceCount $widgetNames 'DeliveryLineB')"

foreach ($row in @("A", "B")) {
    $line = "DeliveryLine$row"
    $stack = "DeliveryProgressStack$row"
    $bar = "DeliveryProgressBar$row"
    $widgetNames = Add-WidgetIfMissing $hudPath "HorizontalBox" $line "DeliveryPanel" $widgetNames
    $widgetNames = Add-WidgetIfMissing $hudPath "TextBlock" "DeliveryLabel$row" $line $widgetNames
    $widgetNames = Add-WidgetIfMissing $hudPath "Image" "DeliveryIcon$row" $line $widgetNames
    $widgetNames = Add-WidgetIfMissing $hudPath "Image" $bar $line $widgetNames
    $widgetNames = Add-WidgetIfMissing $hudPath "HorizontalBox" $stack $line $widgetNames
    for ($i = 0; $i -lt $DeliverySegments; $i++) {
        $widgetNames = Add-WidgetIfMissing $hudPath "Image" "DeliveryStack${row}_$i" $stack $widgetNames
    }
}

# --- Teammate entry (portrait frame layout: run ApplyHUDTeammatePortraitLayout.ps1 once for Overlay tree) ---
$entryEdits = @(
    @{
        widget_name = "PortraitImage"
        properties  = @{
            image_texture = "$T/Textures/Teammate/T_HUD_Portrait_Placeholder"
            desired_size  = @(68, 68)
            image_color   = @(1.0, 1.0, 1.0, 1.0)
        }
    },
    @{
        widget_name = "PortraitSlotFrame"
        properties  = @{
            image_texture = "$T/Textures/Teammate/T_HUD_Portrait_Slot"
            desired_size  = @(80, 80)
            visibility    = "HitTestInvisible"
        }
    },
    @{
        widget_name = "NicknameText"
        properties  = @{
            font_path  = $FontSemiBold
            text_color = @(0.97, 0.98, 0.99, 1.0)
            font_size  = 20
        }
        panel_slot  = @{
            size_rule            = "Automatic"
            horizontal_alignment = "Left"
            vertical_alignment   = "Center"
        }
    },
    @{
        widget_name = "CageStackText"
        properties  = @{
            font_path  = $FontMedium
            text_color = @(0.82, 0.72, 0.48, 1.0)
            font_size  = 16
        }
        panel_slot  = @{
            size_rule            = "Automatic"
            horizontal_alignment = "Left"
            vertical_alignment   = "Center"
            padding              = @{ top = 2 }
        }
    },
    @{
        widget_name = "DownedHealthBar"
        properties  = @{
            fill_texture       = "$T/Textures/Teammate/T_HUD_Bar_Fill_Downed"
            background_texture = "$T/Textures/Common/T_HUD_Bar_BG"
            percent            = 0.65
            visibility         = "HitTestInvisible"
        }
        panel_slot  = @{
            size_rule            = "Fill"
            horizontal_alignment = "Fill"
            vertical_alignment   = "Center"
            padding              = @{ top = 6; right = 8 }
        }
    },
    @{
        widget_name = "InfoVBox"
        panel_slot  = @{
            size_rule            = "Fill"
            horizontal_alignment = "Fill"
            vertical_alignment   = "Center"
        }
    },
    @{
        widget_name = "RootHBox"
        panel_slot  = @{
            size_rule            = "Fill"
            horizontal_alignment = "Fill"
            vertical_alignment   = "Center"
            padding              = @{ left = 4; right = 4; top = 2; bottom = 2 }
        }
    }
)
Edit-Wbp "/Game/Blueprints/UI/WBP_TeammateEntry" $entryEdits

# Ensure panel BG images exist on RootCanvas (behind content, z_order 5)
$widgetNames = Get-WbpWidgetNames $hudPath
foreach ($bgName in @("DeliveryPanelBG", "InventoryPanelBG", "PerkPanelBG")) {
    $widgetNames = Add-WidgetIfMissing $hudPath "Image" $bgName "RootCanvas" $widgetNames
}

$gameEdits = [System.Collections.Generic.List[object]]::new()
$PanelTex = Resolve-HudTexture @(
    "$T/Textures/Common/T_HUD_Panel_BG",
    "$T/T_HUD_Panel_BG",
    "$T/Textures/Common/T_HUD_Bar_BG"
) "$T/Textures/Common/T_HUD_Bar_BG"
$PanelWideTex = Resolve-HudTexture @(
    "$T/Textures/Common/T_HUD_Panel_BG_Wide",
    "$T/Textures/Common/T_HUD_Panel_BG"
) $PanelTex
$PanelBarTex = Resolve-HudTexture @(
    "$T/Textures/Common/T_HUD_Panel_BG_Bar",
    "$T/Textures/Common/T_HUD_Panel_BG"
) $PanelTex

$PanelBarTex = Resolve-HudTexture @(
    "$T/Textures/Common/T_HUD_Panel_BG_Bar",
    "$T/Textures/Common/T_HUD_Panel_BG"
) $PanelTex

# Content sizes tuned to widgets (portrait 84 + info ~180, 3 rows)
$TmateW, $TmateH = 268, 276
$DelW, $DelH = 356, 100
$InvW, $InvH = 342, 78
$PerkW, $PerkH = 170, 78

# Teammate: no panel BG — icon frame only (see ApplyHUDTeammatePortraitLayout.ps1)
Remove-WidgetIfExists $hudPath "TeammatePanelBG"
$gameEdits.Add(@{
    widget_name = "TeammatePanel"
    canvas_slot = @{
        anchors   = @{ min = @(0, 0.5); max = @(0, 0.5) }
        alignment = @(0, 0.5)
        position  = @(32, 0)
        size      = @($TmateW, $TmateH)
        auto_size = $false
        z_order   = 10
    }
})

Add-PanelWithBg $gameEdits "DeliveryPanelBG" "DeliveryPanel" @(0, 1) @(0, 1) @(0, 1) 32 -24 $DelW $DelH $PanelBarTex
Add-PanelWithBg $gameEdits "InventoryPanelBG" "InventoryPanel" @(0.5, 1) @(0.5, 1) @(0.5, 1) 0 -24 $InvW $InvH $PanelBarTex
Add-PanelWithBg $gameEdits "PerkPanelBG" "PerkPanel" @(1, 1) @(1, 1) @(1, 1) -32 -24 $PerkW $PerkH $PanelTex

for ($i = 0; $i -lt 3; $i++) {
    $pad = @{ }
    if ($i -lt 2) { $pad.bottom = 6 }
    $gameEdits.Add(@{
        widget_name = "TeammateEntries_$i"
        panel_slot  = @{
            size_rule            = "Automatic"
            horizontal_alignment = "Fill"
            vertical_alignment   = "Center"
            padding              = $pad
        }
    })
}

foreach ($row in @("A", "B")) {
    $line = "DeliveryLine$row"
    $stack = "DeliveryProgressStack$row"
    $bar = "DeliveryProgressBar$row"
    $previewFill = if ($row -eq "A") { 3 } else { 7 }

    $rowPad = if ($row -eq "A") { @{ bottom = 4 } } else { @{ top = 4 } }
    $gameEdits.Add(@{
        widget_name = $line
        properties  = @{ visibility = "HitTestInvisible" }
        panel_slot  = @{
            size_rule            = "Automatic"
            horizontal_alignment = "Fill"
            vertical_alignment   = "Center"
            padding              = $rowPad
        }
    })
    $labelCandidates = if ($row -eq "A") {
        @("$T/Textures/Delivery/T_HUD_Delivery_Label_A")
    } else {
        @("$T/Textures/Delivery/T_HUD_Delivery_Label_B")
    }
    $labelTex = Resolve-HudTexture $labelCandidates $null
    if ($labelTex) {
        $gameEdits.Add(@{
            widget_name = "DeliveryLabel$row"
            properties  = @{ visibility = "Collapsed" }
        })
        $gameEdits.Add(@{
            widget_name = "DeliveryIcon$row"
            properties  = @{
                image_texture = $labelTex
                desired_size  = @(40, 40)
                image_color   = @(1.0, 1.0, 1.0, 1.0)
            }
            panel_slot  = @{
                size_rule            = "Automatic"
                horizontal_alignment = "Left"
                vertical_alignment   = "Center"
                padding              = @{ right = 6 }
            }
        })
    } else {
        $gameEdits.Add(@{
            widget_name = "DeliveryLabel$row"
            properties  = @{
                text       = $row
                font_path  = $FontSemiBold
                text_color = @(0.97, 0.98, 0.99, 1.0)
                font_size  = 24
                visibility = "HitTestInvisible"
            }
            panel_slot  = @{
                size_rule            = "Automatic"
                horizontal_alignment = "Left"
                vertical_alignment   = "Center"
                padding              = @{ right = 6 }
            }
        })
        $gameEdits.Add(@{
            widget_name = "DeliveryIcon$row"
            properties  = @{ visibility = "Collapsed" }
        })
    }
    $stationName = "DeliveryStationIcon$row"
    $widgetNames = Add-WidgetIfMissing $hudPath "Image" $stationName $line $widgetNames
    $gameEdits.Add(@{
        widget_name = $stationName
        properties  = @{
            image_texture = "$T/Textures/Delivery/T_HUD_Delivery_Slot"
            desired_size  = @(44, 44)
            image_color   = @(1.0, 1.0, 1.0, 1.0)
        }
        panel_slot  = @{
            size_rule            = "Automatic"
            horizontal_alignment = "Left"
            vertical_alignment   = "Center"
            padding              = @{ right = 8 }
        }
    })
    $progressTex = Resolve-HudTexture @(
        "$T/Textures/Delivery/T_HUD_Delivery_Progress_03",
        "$T/Textures/Delivery/T_HUD_Delivery_Progress_BG"
    ) "$T/Textures/Delivery/T_HUD_Delivery_Progress_BG"
    if ($row -eq "B") {
        $progressTex = Resolve-HudTexture @(
            "$T/Textures/Delivery/T_HUD_Delivery_Progress_07",
            "$T/Textures/Delivery/T_HUD_Delivery_Progress_BG"
        ) "$T/Textures/Delivery/T_HUD_Delivery_Progress_BG"
    }
    $gameEdits.Add(@{
        widget_name = $bar
        properties  = @{
            image_texture = $progressTex
            desired_size  = @(248, 24)
            image_color   = @(1.0, 1.0, 1.0, 1.0)
        }
        panel_slot  = @{
            size_rule            = "Fill"
            horizontal_alignment = "Fill"
            vertical_alignment   = "Center"
        }
    })
    $gameEdits.Add(@{
        widget_name = $stack
        properties  = @{ visibility = "Collapsed" }
    })

    for ($i = 0; $i -lt $DeliverySegments; $i++) {
        $gameEdits.Add(@{
            widget_name = "DeliveryStack${row}_$i"
            properties  = @{ visibility = "Collapsed" }
        })
    }
}

for ($i = 0; $i -lt 4; $i++) {
    $slotPad = @{ }
    if ($i -lt 3) { $slotPad.right = 6 }
    $gameEdits.Add(@{
        widget_name = "InventorySlots_$i"
        properties  = @{
            image_texture = "$T/Textures/Inventory/T_HUD_Inventory_Slot_Empty"
            desired_size  = @(78, 78)
        }
        panel_slot  = @{
            size_rule            = "Automatic"
            horizontal_alignment = "Left"
            vertical_alignment   = "Center"
            padding              = $slotPad
        }
    })
}
for ($i = 0; $i -lt 2; $i++) {
    $slotPad = @{ }
    if ($i -eq 0) { $slotPad.right = 6 }
    $gameEdits.Add(@{
        widget_name = "PerkSlots_$i"
        properties  = @{
            image_texture = "$T/Textures/Perk/T_HUD_Perk_Slot_Empty"
            desired_size  = @(78, 78)
        }
        panel_slot  = @{
            size_rule            = "Automatic"
            horizontal_alignment = "Left"
            vertical_alignment   = "Center"
            padding              = $slotPad
        }
    })
}

Edit-Wbp "/Game/Blueprints/UI/WBP_GameHUD" $gameEdits

$paths = @("/Game/Blueprints/UI/WBP_TeammateEntry", "/Game/Blueprints/UI/WBP_GameHUD")
Invoke-McpRaw -Method "tools/call" -Params @{
    name = "compile-assets"; arguments = @{ asset_paths = $paths }
} -SessionId $session | Out-Null
Write-Host "[compile-assets] ok"

$actions = $paths | ForEach-Object { @{ action = "save"; asset_path = $_ } }
Invoke-McpRaw -Method "tools/call" -Params @{
    name = "manage-assets"; arguments = @{ actions = $actions; save = $true }
} -SessionId $session | Out-Null
Write-Host "[save] ok"
Write-Host "DBD-inspired HUD layout applied."

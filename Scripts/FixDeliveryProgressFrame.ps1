# Delivery progress helper — keeps WBP Brush Image Size; only wires textures/fill preview.
param([string]$BaseUrl = "http://127.0.0.1:8080/mcp")

$ErrorActionPreference = "Stop"
$T = "/Game/UI/HUD"
$Hud = "/Game/Blueprints/UI/WBP_GameHUD"
$MI = "$T/Materials/MI_HUD_DeliveryProgressFill"
$FrameA = "$T/Textures/Delivery/T_HUD_Delivery_Station_A"
$FrameB = "$T/Textures/Delivery/T_HUD_Delivery_Station_B"
$BarW = 420
$BarH = 76
# Design reference for fill preview only; frame size comes from WBP Brush Image Size.
$FillLeft = 64
$FillTop = 15
$FillH = 47
$FillMaxW = 256
$PreviewA = [int]($FillMaxW * 0.30)
$PreviewB = [int]($FillMaxW * 0.70)

function Invoke-McpRaw {
    param([string]$Method, [object]$Params, [string]$SessionId = $null)
    $body = @{ jsonrpc = "2.0"; id = 1; method = $Method; params = $Params } | ConvertTo-Json -Depth 50 -Compress
    $headers = @{ "Content-Type" = "application/json"; "Accept" = "application/json, text/event-stream" }
    if ($SessionId) { $headers["Mcp-Session-Id"] = $SessionId }
    return Invoke-WebRequest -Uri $BaseUrl -Method POST -Headers $headers -Body $body -UseBasicParsing
}

function Call-Tool([string]$Name, [hashtable]$ToolArgs, [string]$SessionId) {
    $resp = Invoke-McpRaw -Method "tools/call" -Params @{ name = $Name; arguments = $ToolArgs } -SessionId $session
    if ($resp.Content -match '"isError"\s*:\s*true') { Write-Host $resp.Content; throw "$Name failed" }
    return $resp
}

for ($i = 0; $i -lt 30; $i += 2) {
    try {
        if ((Invoke-WebRequest -Uri $BaseUrl -Method GET -UseBasicParsing -TimeoutSec 2).StatusCode -eq 200) { break }
    } catch { Start-Sleep -Seconds 2 }
}

$init = Invoke-McpRaw -Method "initialize" -Params @{
    protocolVersion = "2024-11-05"; capabilities = @{}; clientInfo = @{ name = "FixFrame"; version = "3" }
}
$session = $init.Headers["Mcp-Session-Id"]
Invoke-McpRaw -Method "notifications/initialized" -Params @{} -SessionId $session | Out-Null

function Get-WidgetNames {
    $resp = Call-Tool "inspect-widget-blueprint" @{ asset_path = $Hud; depth_limit = -1 } $session
    return @(($resp.Content | ConvertFrom-Json).result.structuredContent.all_widgets)
}

function Ensure-Row([string]$Row, [string]$FrameTex, [int]$PreviewFillW) {
    $line = "DeliveryLine$row"
    $sizeBox = "DeliveryProgressSize$row"
    $root = "DeliveryProgressRoot$row"
    $frameWidget = "DeliveryProgressBG$row"
    $fillWidget = "DeliveryProgressFill$row"
    $legacy = "DeliveryProgressBar$row"
    $icon = "DeliveryIcon$row"
    $names = Get-WidgetNames

    foreach ($w in @($legacy, $sizeBox)) {
        if ($names -contains $w) {
            Call-Tool "edit-widget-blueprint" @{
                asset_path = $Hud; edits = @(@{ widget_name = $w; action = "remove"; occurrence = 0 })
            } $session | Out-Null
            Write-Host "[remove] $w"
            $names = Get-WidgetNames
        }
    }

    if ($names -contains $icon) {
        Call-Tool "edit-widget-blueprint" @{
            asset_path = $Hud
            edits      = @(@{ widget_name = $icon; properties = @{ visibility = "Collapsed" } })
        } $session | Out-Null
    }

    if (-not ($names -contains $root)) {
        Call-Tool "add-widget" @{
            asset_path = $Hud; widget_class = "Overlay"; widget_name = $root; parent_widget = $line
        } $session | Out-Null
    }
    if (-not ($names -contains $fillWidget)) {
        Call-Tool "add-widget" @{
            asset_path = $Hud; widget_class = "Image"; widget_name = $fillWidget; parent_widget = $root
        } $session | Out-Null
    }
    if (-not ($names -contains $frameWidget)) {
        Call-Tool "add-widget" @{
            asset_path = $Hud; widget_class = "Image"; widget_name = $frameWidget; parent_widget = $root
        } $session | Out-Null
    }

    Call-Tool "edit-widget-blueprint" @{
        asset_path = $Hud
        edits      = @(
            @{
                widget_name = $root
                properties  = @{ visibility = "HitTestInvisible" }
                panel_slot  = @{
                    size_rule            = "Automatic"
                    horizontal_alignment = "Left"
                    vertical_alignment   = "Center"
                }
            },
            @{
                widget_name = $frameWidget
                properties  = @{
                    image_texture = $FrameTex
                    image_color   = @(1, 1, 1, 1)
                }
                overlay_slot  = @{ horizontal_alignment = "Left"; vertical_alignment = "Top" }
            },
            @{
                widget_name = $fillWidget
                properties  = @{
                    image_material     = $MI
                    desired_size       = @($PreviewFillW, $FillH)
                    render_translation = @($FillLeft, $FillTop)
                    image_color        = @(1, 1, 1, 1)
                }
                overlay_slot  = @{
                    horizontal_alignment = "Left"
                    vertical_alignment   = "Top"
                }
            },
            @{
                widget_name = $root
                action      = "reorder_overlay"
                child_order = @($fillWidget, $frameWidget)
            }
        )
    } $session | Out-Null

    Write-Host "[ok] $root frame=$frameWidget fill at designer scale (preview ${PreviewFillW}x${FillH})"
}

Ensure-Row "A" $FrameA $PreviewA
Ensure-Row "B" $FrameB $PreviewB
Call-Tool "compile-assets" @{ asset_paths = @($Hud) } $session | Out-Null
Write-Host "Delivery progress bars fixed."

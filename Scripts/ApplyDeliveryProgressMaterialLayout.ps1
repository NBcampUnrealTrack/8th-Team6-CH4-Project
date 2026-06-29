# Delivery progress bars: Overlay(Frame BG + animated MI Fill), Fill behind frame.
param([string]$BaseUrl = "http://127.0.0.1:8080/mcp")

$ErrorActionPreference = "Stop"
$T = "/Game/UI/HUD"
$Hud = "/Game/Blueprints/UI/WBP_GameHUD"
$MI = "$T/Materials/MI_HUD_DeliveryProgressFill"
$BG = "$T/Textures/Delivery/T_HUD_Delivery_Progress_BG"
$BarW = 248
$BarH = 24

function Invoke-McpRaw {
    param([string]$Method, [object]$Params, [string]$SessionId = $null)
    $body = @{ jsonrpc = "2.0"; id = 1; method = $Method; params = $Params } | ConvertTo-Json -Depth 50 -Compress
    $headers = @{ "Content-Type" = "application/json"; "Accept" = "application/json, text/event-stream" }
    if ($SessionId) { $headers["Mcp-Session-Id"] = $SessionId }
    return Invoke-WebRequest -Uri $BaseUrl -Method POST -Headers $headers -Body $body -UseBasicParsing
}

function Assert-McpOk([object]$Resp, [string]$Label) {
    if ($Resp.Content -match '"isError"\s*:\s*true') {
        Write-Host $Resp.Content
        throw "$Label failed"
    }
}

for ($i = 0; $i -lt 60; $i += 2) {
    try {
        if ((Invoke-WebRequest -Uri $BaseUrl -Method GET -UseBasicParsing -TimeoutSec 2).StatusCode -eq 200) { break }
    } catch {}
    Start-Sleep -Seconds 2
}

$init = Invoke-McpRaw -Method "initialize" -Params @{
    protocolVersion = "2024-11-05"; capabilities = @{}; clientInfo = @{ name = "DeliveryBar"; version = "1" }
}
$session = $init.Headers["Mcp-Session-Id"]
Invoke-McpRaw -Method "notifications/initialized" -Params @{} -SessionId $session | Out-Null

function Call-Tool([string]$Name, [hashtable]$ToolArgs) {
    $resp = Invoke-McpRaw -Method "tools/call" -Params @{ name = $Name; arguments = $ToolArgs } -SessionId $session
    Assert-McpOk $resp $Name
    return $resp
}

function Get-WidgetNames {
    $resp = Call-Tool "inspect-widget-blueprint" @{ asset_path = $Hud; depth_limit = -1 }
    return @(($resp.Content | ConvertFrom-Json).result.structuredContent.all_widgets)
}

$matScript = "exec(open(r'E:/Unreal/git/SpaCh4/Scripts/BuildHUDDeliveryProgressAll.py', encoding='utf-8').read())"
Call-Tool "run-python-script" @{ script = $matScript } | Out-Null
Write-Host "[material] $MI"

function Ensure-DeliveryRow([string]$Row, [int]$PreviewFill) {
    $line = "DeliveryLine$row"
    $root = "DeliveryProgressRoot$row"
    $frameWidget = "DeliveryProgressBG$row"
    $fillWidget = "DeliveryProgressFill$row"
    $legacy = "DeliveryProgressBar$row"
    $names = Get-WidgetNames

    if ($names -contains $legacy) {
        Call-Tool "edit-widget-blueprint" @{
            asset_path = $Hud
            edits      = @(@{ widget_name = $legacy; action = "remove"; occurrence = 0 })
        } | Out-Null
        Write-Host "[remove] $legacy"
        $names = Get-WidgetNames
    }

    if (-not ($names -contains $root)) {
        Call-Tool "add-widget" @{
            asset_path    = $Hud
            widget_class  = "Overlay"
            widget_name   = $root
            parent_widget = $line
        } | Out-Null
        Write-Host "[add] $root"
    }

    $names = Get-WidgetNames
    if (-not ($names -contains $fillWidget)) {
        Call-Tool "add-widget" @{
            asset_path    = $Hud
            widget_class  = "Image"
            widget_name   = $fillWidget
            parent_widget = $root
        } | Out-Null
        Write-Host "[add] $fillWidget"
    }
    if (-not ($names -contains $frameWidget)) {
        Call-Tool "add-widget" @{
            asset_path    = $Hud
            widget_class  = "Image"
            widget_name   = $frameWidget
            parent_widget = $root
        } | Out-Null
        Write-Host "[add] $frameWidget"
    }

    $previewW = [int]($BarW * $PreviewFill / 10)
    Call-Tool "edit-widget-blueprint" @{
        asset_path = $Hud
        edits      = @(
            @{
                widget_name = $root
                properties  = @{ visibility = "HitTestInvisible" }
                panel_slot  = @{
                    size_rule            = "Fill"
                    horizontal_alignment = "Fill"
                    vertical_alignment   = "Center"
                }
            },
            @{
                widget_name = $fillWidget
                properties  = @{
                    image_material = $MI
                    desired_size   = @($previewW, $BarH)
                    image_color    = @(1, 1, 1, 1)
                }
                overlay_slot  = @{
                    horizontal_alignment = "Left"
                    vertical_alignment   = "Fill"
                }
            },
            @{
                widget_name = $frameWidget
                properties  = @{
                    image_texture = $BG
                    desired_size  = @($BarW, $BarH)
                    image_color   = @(1, 1, 1, 1)
                }
                overlay_slot  = @{
                    horizontal_alignment = "Fill"
                    vertical_alignment   = "Fill"
                }
            }
        )
    } | Out-Null

    try {
        Call-Tool "edit-widget-blueprint" @{
            asset_path = $Hud
            edits      = @(
                @{
                    widget_name = $root
                    action      = "reorder_overlay"
                    child_order = @($fillWidget, $frameWidget)
                }
            )
        } | Out-Null
    } catch {
        Write-Host "[warn] reorder_overlay skipped for $root"
    }
}

Ensure-DeliveryRow "A" 3
Ensure-DeliveryRow "B" 7

Call-Tool "compile-assets" @{ asset_paths = @($Hud) } | Out-Null
Write-Host "Delivery progress overlay + shimmer MI applied."

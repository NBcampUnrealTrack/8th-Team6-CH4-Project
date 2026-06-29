# Add DeliveryValueA/B percent labels beside progress bars.
param([string]$BaseUrl = "http://127.0.0.1:8080/mcp")

$ErrorActionPreference = "Stop"
$Hud = "/Game/Blueprints/UI/WBP_GameHUD"
$Font = "/Game/UI/HUD/Fonts/Rajdhani-Medium"

function Invoke-McpRaw {
    param([string]$Method, [object]$Params, [string]$SessionId = $null)
    $body = @{ jsonrpc = "2.0"; id = 1; method = $Method; params = $Params } | ConvertTo-Json -Depth 50 -Compress
    $headers = @{ "Content-Type" = "application/json"; "Accept" = "application/json, text/event-stream" }
    if ($SessionId) { $headers["Mcp-Session-Id"] = $SessionId }
    return Invoke-WebRequest -Uri $BaseUrl -Method POST -Headers $headers -Body $body -UseBasicParsing
}

$init = Invoke-McpRaw -Method "initialize" -Params @{
    protocolVersion = "2024-11-05"; capabilities = @{}; clientInfo = @{ name = "Pct"; version = "1" }
}
$session = $init.Headers["Mcp-Session-Id"]
Invoke-McpRaw -Method "notifications/initialized" -Params @{} -SessionId $session | Out-Null

function Call-Tool([string]$Name, [hashtable]$ToolArgs) {
    $resp = Invoke-McpRaw -Method "tools/call" -Params @{ name = $Name; arguments = $ToolArgs } -SessionId $session
    if ($resp.Content -match '"isError"\s*:\s*true') { Write-Host $resp.Content; throw "$Name failed" }
}

$namesResp = Call-Tool "inspect-widget-blueprint" @{ asset_path = $Hud; depth_limit = -1 }
$names = @(($namesResp.Content | ConvertFrom-Json).result.structuredContent.all_widgets)

foreach ($row in @("A", "B")) {
    $valueWidget = "DeliveryValue$row"
    $line = "DeliveryLine$row"
    if (-not ($names -contains $valueWidget)) {
        Call-Tool "add-widget" @{
            asset_path    = $Hud
            widget_class  = "TextBlock"
            widget_name   = $valueWidget
            parent_widget = $line
        } | Out-Null
        Write-Host "[add] $valueWidget"
    }
}

$previewPct = @{ A = 33; B = 67 }
$edits = @()
foreach ($row in @("A", "B")) {
    $edits += @{
        widget_name = "DeliveryValue$row"
        properties  = @{
            text       = "$($previewPct[$row])%"
            font_path  = $Font
            font_size  = 18
            text_color = @(0.72, 0.92, 0.78, 1.0)
            visibility = "HitTestInvisible"
        }
        panel_slot  = @{
            size_rule            = "Automatic"
            horizontal_alignment = "Left"
            vertical_alignment   = "Center"
            padding              = @{ left = 8 }
        }
    }
}

Call-Tool "edit-widget-blueprint" @{ asset_path = $Hud; edits = $edits } | Out-Null
Call-Tool "compile-assets" @{ asset_paths = @($Hud) } | Out-Null
Write-Host "Delivery percent labels added."

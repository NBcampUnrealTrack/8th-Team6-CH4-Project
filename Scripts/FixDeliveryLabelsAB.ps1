# Fix delivery row labels: A + B (not AA). Run after reimporting Label PNGs in Content Browser.
param([string]$BaseUrl = "http://127.0.0.1:8080/mcp")

$ErrorActionPreference = "Stop"
$T = "/Game/UI/HUD"
$hudPath = "/Game/Blueprints/UI/WBP_GameHUD"

function Invoke-McpRaw {
    param([string]$Method, [object]$Params, [string]$SessionId = $null)
    $body = @{ jsonrpc = "2.0"; id = 1; method = $Method; params = $Params } | ConvertTo-Json -Depth 40 -Compress
    $headers = @{ "Content-Type" = "application/json"; "Accept" = "application/json, text/event-stream" }
    if ($SessionId) { $headers["Mcp-Session-Id"] = $SessionId }
    return Invoke-WebRequest -Uri $BaseUrl -Method POST -Headers $headers -Body $body -UseBasicParsing
}

function Call-Tool([string]$Name, [object]$Arguments, [string]$SessionId) {
    $resp = Invoke-McpRaw -Method "tools/call" -Params @{ name = $Name; arguments = $Arguments } -SessionId $SessionId
    if ($resp.Content -match '"isError"\s*:\s*true') { Write-Host $resp.Content; throw "$Name failed" }
}

function Test-AssetExists([string]$AssetPath, [string]$SessionId) {
    $resp = Invoke-McpRaw -Method "tools/call" -Params @{
        name = "query-asset"
        arguments = @{ asset_path = $AssetPath }
    } -SessionId $SessionId
    return ($resp.Content -notmatch '"isError"\s*:\s*true' -and $resp.Content -match '"class"')
}

$init = Invoke-McpRaw -Method "initialize" -Params @{
    protocolVersion = "2024-11-05"; capabilities = @{}; clientInfo = @{ name = "FixDeliveryLabels"; version = "1" }
}
$session = $init.Headers["Mcp-Session-Id"]
Invoke-McpRaw -Method "notifications/initialized" -Params @{} -SessionId $session | Out-Null

$labelA = "$T/Textures/Delivery/T_HUD_Delivery_Label_A"
$labelB = "$T/Textures/Delivery/T_HUD_Delivery_Label_B"
$station = "$T/Textures/Delivery/T_HUD_Delivery_Slot"

foreach ($path in @($labelA, $labelB)) {
    if (-not (Test-AssetExists $path $session)) {
        throw "Missing texture: $path`nReimport the PNG in Content Browser first, then rerun this script."
    }
}

$edits = @(
    @{
        widget_name = "DeliveryLabelA"
        properties  = @{ visibility = "Collapsed" }
    },
    @{
        widget_name = "DeliveryLabelB"
        properties  = @{ visibility = "Collapsed" }
    },
    @{
        widget_name = "DeliveryIconA"
        properties  = @{
            image_texture = $labelA
            desired_size  = @(40, 40)
            visibility    = "HitTestInvisible"
        }
        panel_slot  = @{ padding = @{ right = 6 } }
    },
    @{
        widget_name = "DeliveryIconB"
        properties  = @{
            image_texture = $labelB
            desired_size  = @(40, 40)
            visibility    = "HitTestInvisible"
        }
        panel_slot  = @{ padding = @{ right = 6 } }
    }
)

function Get-Names([string]$SessionId) {
    $resp = Invoke-McpRaw -Method "tools/call" -Params @{
        name = "inspect-widget-blueprint"
        arguments = @{ asset_path = $hudPath; depth_limit = 1 }
    } -SessionId $SessionId
    $json = $resp.Content | ConvertFrom-Json
    return @($json.result.structuredContent.all_widgets)
}

$names = Get-Names $session
foreach ($row in @("A", "B")) {
    $stationWidget = "DeliveryStationIcon$row"
    if ($names -notcontains $stationWidget) {
        Call-Tool "add-widget" @{
            asset_path    = $hudPath
            widget_class  = "Image"
            widget_name   = $stationWidget
            parent_widget = "DeliveryLine$row"
        } $session
    }
}

$edits += @(
    @{
        widget_name = "DeliveryStationIconA"
        properties  = @{
            image_texture = $station
            desired_size  = @(44, 44)
            visibility    = "HitTestInvisible"
        }
        panel_slot  = @{ padding = @{ right = 8 } }
    },
    @{
        widget_name = "DeliveryStationIconB"
        properties  = @{
            image_texture = $station
            desired_size  = @(44, 44)
            visibility    = "HitTestInvisible"
        }
        panel_slot  = @{ padding = @{ right = 8 } }
    }
)

Call-Tool "edit-widget-blueprint" @{ asset_path = $hudPath; edits = $edits } $session
Call-Tool "compile-assets" @{ asset_paths = @($hudPath) } $session
Call-Tool "manage-assets" @{ actions = @(@{ action = "save"; asset_path = $hudPath }); save = $true } $session
Write-Host "Delivery labels fixed: A + B."

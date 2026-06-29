param([string]$BaseUrl = "http://127.0.0.1:8080/mcp")
$ErrorActionPreference = "Stop"
$T = "/Game/UI/HUD"
$hudPath = "/Game/Blueprints/UI/WBP_GameHUD"
$labelA = "$T/Textures/Delivery/T_HUD_Delivery_Label_A"
$labelB = "$T/Textures/Delivery/T_HUD_Delivery_Label_B"

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
    return $resp.Content
}

function Test-AssetExists([string]$AssetPath, [string]$SessionId) {
    $resp = Invoke-McpRaw -Method "tools/call" -Params @{
        name = "query-asset"
        arguments = @{ asset_path = $AssetPath }
    } -SessionId $SessionId
    return ($resp.Content -notmatch '"isError"\s*:\s*true' -and $resp.Content -match '"class"')
}

$init = Invoke-McpRaw -Method "initialize" -Params @{
    protocolVersion = "2024-11-05"; capabilities = @{}; clientInfo = @{ name = "EnsureLabelB"; version = "1" }
}
$session = $init.Headers["Mcp-Session-Id"]
Invoke-McpRaw -Method "notifications/initialized" -Params @{} -SessionId $session | Out-Null

if (-not (Test-AssetExists $labelB $session)) {
    Write-Host "[asset] Creating $labelB by duplicating Label_A (reimport B PNG after this)"
    Call-Tool "manage-assets" @{
        actions = @(@{
            action           = "duplicate"
            asset_path       = $labelA
            destination_path = "$T/Textures/Delivery"
            new_name         = "T_HUD_Delivery_Label_B"
        })
        save = $true
    } $session
}

if (-not (Test-AssetExists $labelB $session)) {
    throw "Label_B uasset still missing. Reimport Content/UI/HUD/Textures/Delivery/T_HUD_Delivery_Label_B.png in Content Browser."
}

$edits = @(
    @{ widget_name = "DeliveryLabelA"; properties = @{ visibility = "Collapsed" } },
    @{ widget_name = "DeliveryLabelB"; properties = @{ visibility = "Collapsed" } },
    @{
        widget_name = "DeliveryIconA"
        properties  = @{
            image_texture = $labelA
            desired_size  = @(40, 40)
            image_color   = @(1.0, 1.0, 1.0, 1.0)
            visibility    = "Visible"
        }
        panel_slot  = @{ padding = @{ right = 6 } }
    },
    @{
        widget_name = "DeliveryIconB"
        properties  = @{
            image_texture = $labelB
            desired_size  = @(40, 40)
            image_color   = @(1.0, 1.0, 1.0, 1.0)
            visibility    = "Visible"
        }
        panel_slot  = @{ padding = @{ right = 6 } }
    }
)

Call-Tool "edit-widget-blueprint" @{ asset_path = $hudPath; edits = $edits } $session
Call-Tool "compile-assets" @{ asset_paths = @($hudPath) } $session
Call-Tool "manage-assets" @{ actions = @(@{ action = "save"; asset_path = $hudPath }); save = $true } $session
Write-Host "Done. Reimport T_HUD_Delivery_Label_B.png so B shows the correct letter (duplicate starts as A)."

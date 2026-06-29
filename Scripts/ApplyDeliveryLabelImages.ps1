# Apply Label A/B image widgets (requires reimported label uassets).
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
}

$init = Invoke-McpRaw -Method "initialize" -Params @{
    protocolVersion = "2024-11-05"; capabilities = @{}; clientInfo = @{ name = "ApplyDeliveryLabelImages"; version = "1" }
}
$session = $init.Headers["Mcp-Session-Id"]
Invoke-McpRaw -Method "notifications/initialized" -Params @{} -SessionId $session | Out-Null

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
        panel_slot  = @{
            size_rule            = "Automatic"
            horizontal_alignment = "Left"
            vertical_alignment   = "Center"
            padding              = @{ right = 6 }
        }
    },
    @{
        widget_name = "DeliveryIconB"
        properties  = @{
            image_texture = $labelB
            desired_size  = @(40, 40)
            image_color   = @(1.0, 1.0, 1.0, 1.0)
            visibility    = "Visible"
        }
        panel_slot  = @{
            size_rule            = "Automatic"
            horizontal_alignment = "Left"
            vertical_alignment   = "Center"
            padding              = @{ right = 6 }
        }
    }
)

Call-Tool "edit-widget-blueprint" @{ asset_path = $hudPath; edits = $edits } $session
Call-Tool "compile-assets" @{ asset_paths = @($hudPath) } $session
Call-Tool "manage-assets" @{ actions = @(@{ action = "save"; asset_path = $hudPath }); save = $true } $session
Write-Host "WBP delivery labels set to Label_A / Label_B images."

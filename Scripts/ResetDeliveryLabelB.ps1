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

$init = Invoke-McpRaw -Method "initialize" -Params @{
    protocolVersion = "2024-11-05"; capabilities = @{}; clientInfo = @{ name = "ResetLabelB"; version = "1" }
}
$session = $init.Headers["Mcp-Session-Id"]
Invoke-McpRaw -Method "notifications/initialized" -Params @{} -SessionId $session | Out-Null

$labelB = "$T/Textures/Delivery/T_HUD_Delivery_Label_B"
Write-Host "[reset] Deleting stale Label_B uasset (was duplicated from A)"
try {
    Call-Tool "manage-assets" @{
        actions = @(@{ action = "delete"; asset_path = $labelB })
        save    = $true
    } $session
    Write-Host "[reset] Deleted. Reimport T_HUD_Delivery_Label_B.png from Content Browser."
}
catch {
    Write-Host "[reset] Delete skipped or failed: $_"
}

$labelA = "$T/Textures/Delivery/T_HUD_Delivery_Label_A"
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
Write-Host "WBP: IconA=Label_A, IconB=Label_B. Manual reimport both PNGs required."

# Fix duplicate portrait frame + center icon in overlay
param([string]$BaseUrl = "http://127.0.0.1:8080/mcp")

$ErrorActionPreference = "Stop"
$T = "/Game/UI/HUD"
$entryPath = "/Game/Blueprints/UI/WBP_TeammateEntry"

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
    protocolVersion = "2024-11-05"; capabilities = @{}; clientInfo = @{ name = "FixPortraitCenter"; version = "1" }
}
$session = $init.Headers["Mcp-Session-Id"]
Invoke-McpRaw -Method "notifications/initialized" -Params @{} -SessionId $session | Out-Null

# Remove duplicate PortraitFrame inside overlay
Call-Tool "edit-widget-blueprint" @{
    asset_path = $entryPath
    edits      = @(@{ widget_name = "PortraitFrame"; action = "remove"; occurrence = 0 })
} $session

$edits = @(
    @{
        widget_name = "PortraitOverlay"
        panel_slot  = @{
            size_rule            = "Automatic"
            horizontal_alignment = "Left"
            vertical_alignment   = "Center"
            padding              = @{ right = 10 }
        }
    },
    @{
        widget_name = "PortraitSlotFrame"
        properties  = @{
            image_texture = "$T/Textures/Teammate/T_HUD_Portrait_Slot"
            desired_size  = @(80, 80)
            visibility    = "HitTestInvisible"
        }
        overlay_slot = @{
            horizontal_alignment = "Fill"
            vertical_alignment   = "Fill"
        }
    },
    @{
        widget_name = "PortraitImage"
        properties  = @{
            image_texture = "$T/Textures/Teammate/T_HUD_Portrait_Placeholder"
            desired_size  = @(64, 64)
            visibility    = "HitTestInvisible"
        }
        overlay_slot = @{
            horizontal_alignment = "Center"
            vertical_alignment   = "Center"
        }
    }
)

Call-Tool "edit-widget-blueprint" @{ asset_path = $entryPath; edits = $edits } $session
Call-Tool "compile-assets" @{ asset_paths = @($entryPath) } $session
Call-Tool "manage-assets" @{ actions = @(@{ action = "save"; asset_path = $entryPath }); save = $true } $session
Write-Host "Removed duplicate frame; icon centered in overlay."

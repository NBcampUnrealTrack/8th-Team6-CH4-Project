# Sync delivery progress bar A/B overlay slots and row padding.
param([string]$BaseUrl = "http://127.0.0.1:8080/mcp")

$ErrorActionPreference = "Stop"
$Hud = "/Game/Blueprints/UI/WBP_GameHUD"

function Invoke-McpRaw {
    param([string]$Method, [object]$Params, [string]$SessionId = $null)
    $body = @{ jsonrpc = "2.0"; id = 1; method = $Method; params = $Params } | ConvertTo-Json -Depth 50 -Compress
    $headers = @{ "Content-Type" = "application/json"; "Accept" = "application/json, text/event-stream" }
    if ($SessionId) { $headers["Mcp-Session-Id"] = $SessionId }
    return Invoke-WebRequest -Uri $BaseUrl -Method POST -Headers $headers -Body $body -UseBasicParsing
}

$init = Invoke-McpRaw -Method "initialize" -Params @{
    protocolVersion = "2024-11-05"; capabilities = @{}; clientInfo = @{ name = "SyncBar"; version = "1" }
}
$session = $init.Headers["Mcp-Session-Id"]
Invoke-McpRaw -Method "notifications/initialized" -Params @{} -SessionId $session | Out-Null

$edits = @()
foreach ($row in @("A", "B")) {
    $edits += @{
        widget_name = "DeliveryLine$row"
        panel_slot  = @{
            size_rule            = "Fill"
            horizontal_alignment = "Fill"
            vertical_alignment   = "Center"
            padding              = @{ bottom = 4; top = 4 }
        }
    }
    $edits += @{
        widget_name = "DeliveryProgressRoot$row"
        panel_slot  = @{
            size_rule            = "Fill"
            horizontal_alignment = "Fill"
            vertical_alignment   = "Center"
        }
    }
    $edits += @{
        widget_name = "DeliveryProgressFill$row"
        overlay_slot = @{
            horizontal_alignment = "Left"
            vertical_alignment   = "Fill"
        }
    }
    $edits += @{
        widget_name = "DeliveryProgressBG$row"
        overlay_slot = @{
            horizontal_alignment = "Fill"
            vertical_alignment   = "Fill"
        }
    }
    $edits += @{
        widget_name = "DeliveryProgressRoot$row"
        action      = "reorder_overlay"
        child_order = @("DeliveryProgressFill$row", "DeliveryProgressBG$row")
    }
}

$body2 = @{ jsonrpc = "2.0"; id = 2; method = "tools/call"; params = @{
    name = "edit-widget-blueprint"; arguments = @{ asset_path = $Hud; edits = $edits }
} } | ConvertTo-Json -Depth 20 -Compress
$r = Invoke-WebRequest -Uri $BaseUrl -Method POST -Headers @{
    "Content-Type"="application/json"; "Accept"="application/json, text/event-stream"; "Mcp-Session-Id"=$session
} -Body $body2 -UseBasicParsing
if ($r.Content -match '"isError"\s*:\s*true') { Write-Host $r.Content; throw "edit failed" }
Write-Host "Delivery progress bars A/B synced (Fill Left+Fill, Frame Fill+Fill)."

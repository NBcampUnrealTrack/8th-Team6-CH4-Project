# Partial fix when Label_B uasset is missing: A=image, B=text until reimport.
param([string]$BaseUrl = "http://127.0.0.1:8080/mcp")

$ErrorActionPreference = "Stop"
$T = "/Game/UI/HUD"
$hudPath = "/Game/Blueprints/UI/WBP_GameHUD"
$FontSemiBold = "/Game/UI/HUD/Fonts/Rajdhani-SemiBold"

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
    protocolVersion = "2024-11-05"; capabilities = @{}; clientInfo = @{ name = "FixDeliveryLabelsNow"; version = "1" }
}
$session = $init.Headers["Mcp-Session-Id"]
Invoke-McpRaw -Method "notifications/initialized" -Params @{} -SessionId $session | Out-Null

$labelA = "$T/Textures/Delivery/T_HUD_Delivery_Label_A"
$labelB = "$T/Textures/Delivery/T_HUD_Delivery_Label_B"
$station = "$T/Textures/Delivery/T_HUD_Delivery_Slot"
$hasLabelB = Test-AssetExists $labelB $session

$edits = @(
    @{
        widget_name = "DeliveryLabelA"
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
    }
)

if ($hasLabelB) {
    Write-Host "[labels] Label_B found — image mode for both rows"
    $edits += @(
        @{
            widget_name = "DeliveryLabelB"
            properties  = @{ visibility = "Collapsed" }
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
} else {
    Write-Host "[labels] Label_B missing — row B uses TextBlock until reimport"
    $edits += @(
        @{
            widget_name = "DeliveryIconB"
            properties  = @{ visibility = "Collapsed" }
        },
        @{
            widget_name = "DeliveryLabelB"
            properties  = @{
                text       = "B"
                font_path  = $FontSemiBold
                text_color = @(0.97, 0.98, 0.99, 1.0)
                font_size  = 24
                visibility = "HitTestInvisible"
            }
            panel_slot  = @{
                horizontal_alignment = "Left"
                vertical_alignment   = "Center"
                padding              = @{ right = 6 }
            }
        }
    )
}

Call-Tool "edit-widget-blueprint" @{ asset_path = $hudPath; edits = $edits } $session
Call-Tool "compile-assets" @{ asset_paths = @($hudPath) } $session
Call-Tool "manage-assets" @{ actions = @(@{ action = "save"; asset_path = $hudPath }); save = $true } $session
if ($hasLabelB) {
    Write-Host "Done: A + B image labels."
} else {
    Write-Host "Done: A image + B text. Reimport T_HUD_Delivery_Label_B.png then run FixDeliveryLabelsAB.ps1"
}

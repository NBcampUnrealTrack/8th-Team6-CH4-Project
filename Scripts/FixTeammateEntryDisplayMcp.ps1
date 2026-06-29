# Fix teammate row layout defaults (text + entry width + health bar preview)
param([string]$BaseUrl = "http://127.0.0.1:8080/mcp")

$ErrorActionPreference = "Stop"
$T = "/Game/UI/HUD"
$FontSemiBold = "$T/Fonts/Rajdhani-SemiBold"
$FontMedium = "$T/Fonts/Rajdhani-Medium"
$Entry = "/Game/Blueprints/UI/WBP_TeammateEntry"
$Hud = "/Game/Blueprints/UI/WBP_GameHUD"
$BarW = 248
$BarH = 27
$MI = "$T/Materials/MI_HUD_HealthBarFill_Downed"
$BgTex = "$T/Textures/Common/T_HUD_Bar_BG"

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

$init = Invoke-McpRaw -Method "initialize" -Params @{
    protocolVersion = "2024-11-05"; capabilities = @{}; clientInfo = @{ name = "FixTeammateDisplay"; version = "1" }
}
$session = $init.Headers["Mcp-Session-Id"]
Invoke-McpRaw -Method "notifications/initialized" -Params @{} -SessionId $session | Out-Null

function Call-Tool([string]$Name, [hashtable]$ToolArgs) {
    $resp = Invoke-McpRaw -Method "tools/call" -Params @{ name = $Name; arguments = $ToolArgs } -SessionId $session
    Assert-McpOk $resp $Name
    return $resp
}

$entryEdits = @(
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
        widget_name = "InfoVBox"
        panel_slot  = @{
            size_rule            = "Fill"
            horizontal_alignment = "Fill"
            vertical_alignment   = "Center"
        }
    },
    @{
        widget_name = "NicknameText"
        properties  = @{
            text       = "Player"
            font_path  = $FontSemiBold
            text_color = @(0.97, 0.98, 0.99, 1.0)
            font_size  = 20
        }
    },
    @{
        widget_name = "CageStackText"
        properties  = @{
            text       = "0/2"
            font_path  = $FontMedium
            text_color = @(0.82, 0.72, 0.48, 1.0)
            font_size  = 16
        }
    },
    @{
        widget_name = "DownedHealthBarBG"
        properties  = @{
            image_texture = $BgTex
            desired_size  = @($BarW, $BarH)
        }
    },
    @{
        widget_name = "DownedHealthBarFill"
        properties  = @{
            image_material = $MI
            desired_size   = @([int]($BarW * 0.65), $BarH)
            image_color    = @(1, 1, 1, 1)
        }
    }
)

Call-Tool "edit-widget-blueprint" @{ asset_path = $Entry; edits = $entryEdits } | Out-Null

$gameEdits = @(
    @{
        widget_name = "TeammatePanel"
        canvas_slot = @{
            anchors   = @{ min = @(0, 0.5); max = @(0, 0.5) }
            alignment = @(0, 0.5)
            position  = @(32, 0)
            size      = @(320, 280)
            auto_size = $false
            z_order   = 10
        }
    }
)

for ($i = 0; $i -lt 3; $i++) {
    $pad = @{ }
    if ($i -gt 0) { $pad.top = 8 }
    $gameEdits += @{
        widget_name = "TeammateEntries_$i"
        panel_slot  = @{
            size_rule            = "Fill"
            horizontal_alignment = "Fill"
            vertical_alignment   = "Center"
            padding              = $pad
        }
    }
}

Call-Tool "edit-widget-blueprint" @{ asset_path = $Hud; edits = $gameEdits } | Out-Null
Call-Tool "compile-assets" @{ asset_paths = @($Entry, $Hud) } | Out-Null

Write-Host "Teammate entry display defaults applied."

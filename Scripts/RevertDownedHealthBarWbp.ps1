# Restore WBP_TeammateEntry to ProgressBar + texture fill (pre-material layout)
param([string]$BaseUrl = "http://127.0.0.1:8080/mcp")

$ErrorActionPreference = "Stop"
$T = "/Game/UI/HUD"
$Entry = "/Game/Blueprints/UI/WBP_TeammateEntry"
$Hud = "/Game/Blueprints/UI/WBP_GameHUD"

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
    protocolVersion = "2024-11-05"; capabilities = @{}; clientInfo = @{ name = "RevertHealthBar"; version = "1" }
}
$session = $init.Headers["Mcp-Session-Id"]
Invoke-McpRaw -Method "notifications/initialized" -Params @{} -SessionId $session | Out-Null

function Call-Tool([string]$Name, [hashtable]$ToolArgs) {
    $resp = Invoke-McpRaw -Method "tools/call" -Params @{ name = $Name; arguments = $ToolArgs } -SessionId $session
    Assert-McpOk $resp $Name
    return $resp
}

function Get-WidgetNames {
    $resp = Call-Tool "inspect-widget-blueprint" @{ asset_path = $Entry; depth_limit = -1 }
    return @(($resp.Content | ConvertFrom-Json).result.structuredContent.all_widgets)
}

$names = Get-WidgetNames

foreach ($w in @("DownedHealthBarFill", "DownedHealthBarBG", "DownedHealthBarRoot")) {
    if ($names -contains $w) {
        Call-Tool "edit-widget-blueprint" @{
            asset_path = $Entry
            edits      = @(@{ widget_name = $w; action = "remove"; occurrence = 0 })
        } | Out-Null
        Write-Host "[remove] $w"
    }
}

$names = Get-WidgetNames
if (-not ($names -contains "DownedHealthBar")) {
    Call-Tool "add-widget" @{
        asset_path    = $Entry
        widget_class  = "ProgressBar"
        widget_name   = "DownedHealthBar"
        parent_widget = "InfoVBox"
    } | Out-Null
    Write-Host "[add] DownedHealthBar ProgressBar"
}

Call-Tool "edit-widget-blueprint" @{
    asset_path = $Entry
    edits      = @(
        @{
            widget_name = "DownedHealthBar"
            properties  = @{
                fill_texture       = "$T/Textures/Teammate/T_HUD_Bar_Fill_Downed"
                background_texture = "$T/Textures/Common/T_HUD_Bar_BG"
                percent            = 0.65
                visibility         = "Collapsed"
            }
            panel_slot  = @{
                size_rule            = "Fill"
                horizontal_alignment = "Fill"
                vertical_alignment   = "Center"
                padding              = @{ top = 6; right = 8 }
            }
        }
    )
} | Out-Null

Call-Tool "edit-widget-blueprint" @{
    asset_path = $Hud
    edits      = @(
        @{
            widget_name = "TeammatePanel"
            canvas_slot = @{
                anchors   = @{ min = @(0, 0.5); max = @(0, 0.5) }
                alignment = @(0, 0.5)
                position  = @(32, 0)
                size      = @(268, 276)
                auto_size = $false
                z_order   = 10
            }
        }
    )
} | Out-Null

Call-Tool "compile-assets" @{ asset_paths = @($Entry, $Hud) } | Out-Null
Write-Host "WBP reverted to ProgressBar + texture fill."

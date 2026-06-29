# Replace DownedHealthBar ProgressBar with Overlay(Image BG + Image Material Fill)
param([string]$BaseUrl = "http://127.0.0.1:8080/mcp")

$ErrorActionPreference = "Stop"
$T = "/Game/UI/HUD"
$Entry = "/Game/Blueprints/UI/WBP_TeammateEntry"
$MI = "$T/Materials/MI_HUD_HealthBarFill_Downed"
$BG = "$T/Textures/Common/T_HUD_Bar_BG"
$BarW = 248
$BarH = 27

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

for ($i = 0; $i -lt 60; $i += 2) {
    try {
        if ((Invoke-WebRequest -Uri $BaseUrl -Method GET -UseBasicParsing -TimeoutSec 2).StatusCode -eq 200) { break }
    } catch {}
    Start-Sleep -Seconds 2
}

$init = Invoke-McpRaw -Method "initialize" -Params @{
    protocolVersion = "2024-11-05"; capabilities = @{}; clientInfo = @{ name = "DownedHealthBar"; version = "1" }
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

# Ensure animated fill material exists (clip-free wave graph)
$matScript = "exec(open(r'E:/Unreal/git/SpaCh4/Scripts/BuildHUDHealthBarMaterial.py', encoding='utf-8').read())"
Call-Tool "run-python-script" @{ script = $matScript } | Out-Null

$names = Get-WidgetNames

if ($names -contains "DownedHealthBar" -and -not ($names -contains "DownedHealthBarRoot")) {
    Call-Tool "edit-widget-blueprint" @{
        asset_path = $Entry
        edits      = @(@{ widget_name = "DownedHealthBar"; action = "remove"; occurrence = 0 })
    } | Out-Null
    Write-Host "[remove] DownedHealthBar ProgressBar"
}

$names = Get-WidgetNames

if (-not ($names -contains "DownedHealthBarRoot")) {
    Call-Tool "add-widget" @{
        asset_path    = $Entry
        widget_class  = "Overlay"
        widget_name   = "DownedHealthBarRoot"
        parent_widget = "InfoVBox"
    } | Out-Null
    Write-Host "[add] DownedHealthBarRoot"
}

$names = Get-WidgetNames

if (-not ($names -contains "DownedHealthBarFill")) {
    Call-Tool "add-widget" @{
        asset_path    = $Entry
        widget_class  = "Image"
        widget_name   = "DownedHealthBarFill"
        parent_widget = "DownedHealthBarRoot"
    } | Out-Null
    Write-Host "[add] DownedHealthBarFill"
}

if (-not ($names -contains "DownedHealthBarBG")) {
    Call-Tool "add-widget" @{
        asset_path    = $Entry
        widget_class  = "Image"
        widget_name   = "DownedHealthBarBG"
        parent_widget = "DownedHealthBarRoot"
    } | Out-Null
    Write-Host "[add] DownedHealthBarBG"
}

Call-Tool "edit-widget-blueprint" @{
    asset_path = $Entry
    edits      = @(
        @{
            widget_name = "DownedHealthBarRoot"
            properties  = @{ visibility = "Collapsed" }
            panel_slot  = @{
                size_rule            = "Automatic"
                horizontal_alignment = "Fill"
                vertical_alignment   = "Fill"
            }
        },
        @{
            widget_name = "DownedHealthBarBG"
            properties  = @{
                image_texture = $BG
                desired_size  = @($BarW, $BarH)
                image_color   = @(1, 1, 1, 1)
            }
            overlay_slot  = @{
                horizontal_alignment = "Fill"
                vertical_alignment   = "Fill"
            }
        },
        @{
            widget_name = "DownedHealthBarFill"
            properties  = @{
                image_material = $MI
                desired_size   = @([int]($BarW * 0.65), $BarH)
                image_color    = @(1, 1, 1, 1)
            }
            overlay_slot  = @{
                horizontal_alignment = "Left"
                vertical_alignment   = "Fill"
            }
        }
    )
} | Out-Null

$reorderScript = Join-Path $PSScriptRoot "ReorderDownedHealthBarLayers.ps1"
& $reorderScript -BaseUrl $BaseUrl
Write-Host "[reorder] Fill behind BG frame"

Call-Tool "compile-assets" @{ asset_paths = @($Entry) } | Out-Null

Call-Tool "edit-widget-blueprint" @{
    asset_path = "/Game/Blueprints/UI/WBP_GameHUD"
    edits      = @(
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
} | Out-Null

Write-Host "Downed health bar layout applied: BG=$BG Fill=$MI"

# Teammate HUD: remove panel BG, icon-only portrait frame layout
param([string]$BaseUrl = "http://127.0.0.1:8080/mcp")

$ErrorActionPreference = "Stop"
$T = "/Game/UI/HUD"
$FontSemiBold = "/Game/UI/HUD/Fonts/Rajdhani-SemiBold"
$FontMedium = "/Game/UI/HUD/Fonts/Rajdhani-Medium"
$hudPath = "/Game/Blueprints/UI/WBP_GameHUD"
$entryPath = "/Game/Blueprints/UI/WBP_TeammateEntry"

python "$PSScriptRoot\RegenerateHUDPortraitSlotOnly.py" | Write-Host

function Invoke-McpRaw {
    param([string]$Method, [object]$Params, [string]$SessionId = $null)
    $body = @{ jsonrpc = "2.0"; id = 1; method = $Method; params = $Params } | ConvertTo-Json -Depth 40 -Compress
    $headers = @{ "Content-Type" = "application/json"; "Accept" = "application/json, text/event-stream" }
    if ($SessionId) { $headers["Mcp-Session-Id"] = $SessionId }
    return Invoke-WebRequest -Uri $BaseUrl -Method POST -Headers $headers -Body $body -UseBasicParsing
}

$init = Invoke-McpRaw -Method "initialize" -Params @{
    protocolVersion = "2024-11-05"; capabilities = @{}; clientInfo = @{ name = "HUDTeammatePortrait"; version = "1.0" }
}
$session = $init.Headers["Mcp-Session-Id"]
Invoke-McpRaw -Method "notifications/initialized" -Params @{} -SessionId $session | Out-Null

function Edit-Wbp([string]$AssetPath, [array]$Edits) {
    $resp = Invoke-McpRaw -Method "tools/call" -Params @{
        name = "edit-widget-blueprint"
        arguments = @{ asset_path = $AssetPath; edits = $Edits }
    } -SessionId $session
    if ($resp.Content -match '"isError"\s*:\s*true') {
        Write-Host $resp.Content
        throw "edit-widget-blueprint failed for $AssetPath"
    }
}

function Get-WbpWidgetNames([string]$AssetPath) {
    $resp = Invoke-McpRaw -Method "tools/call" -Params @{
        name = "inspect-widget-blueprint"
        arguments = @{ asset_path = $AssetPath; depth_limit = 2 }
    } -SessionId $session
    $json = $resp.Content | ConvertFrom-Json
    return @($json.result.structuredContent.all_widgets)
}

function Remove-WidgetIfExists([string]$AssetPath, [string]$WidgetName) {
    $names = Get-WbpWidgetNames $AssetPath
    if ($names -contains $WidgetName) {
        Edit-Wbp $AssetPath @(@{ widget_name = $WidgetName; action = "remove"; occurrence = 0 })
        Write-Host "[remove] $WidgetName"
    }
}

# Overlay tree (frame + icon) via editor Python
$py = Get-Content "$PSScriptRoot\FixTeammatePortraitOverlay.py" -Raw
$resp = Invoke-McpRaw -Method "tools/call" -Params @{
    name = "run-python-script"
    arguments = @{ script = $py }
} -SessionId $session
if ($resp.Content -match '"isError"\s*:\s*true') {
    Write-Host $resp.Content
    throw "FixTeammatePortraitOverlay.py failed — is Unreal Editor running?"
}

$entryEdits = @(
    @{
        widget_name = "NicknameText"
        properties  = @{
            font_path  = $FontSemiBold
            text_color = @(0.97, 0.98, 0.99, 1.0)
            font_size  = 20
        }
        panel_slot  = @{
            size_rule            = "Automatic"
            horizontal_alignment = "Left"
            vertical_alignment   = "Center"
        }
    },
    @{
        widget_name = "CageStackText"
        properties  = @{
            font_path  = $FontMedium
            text_color = @(0.82, 0.72, 0.48, 1.0)
            font_size  = 16
        }
        panel_slot  = @{
            size_rule            = "Automatic"
            horizontal_alignment = "Left"
            vertical_alignment   = "Center"
            padding              = @{ top = 2 }
        }
    },
    @{
        widget_name = "DownedHealthBar"
        properties  = @{
            fill_texture       = "$T/Textures/Teammate/T_HUD_Bar_Fill_Downed"
            background_texture = "$T/Textures/Common/T_HUD_Bar_BG"
            percent            = 0.65
            visibility         = "HitTestInvisible"
        }
        panel_slot  = @{
            size_rule            = "Fill"
            horizontal_alignment = "Fill"
            vertical_alignment   = "Center"
            padding              = @{ top = 6; right = 4 }
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
        widget_name = "RootHBox"
        panel_slot  = @{
            size_rule            = "Fill"
            horizontal_alignment = "Fill"
            vertical_alignment   = "Center"
            padding              = @{ left = 0; right = 0; top = 0; bottom = 0 }
        }
    }
)
Edit-Wbp $entryPath $entryEdits

Remove-WidgetIfExists $hudPath "TeammatePanelBG"

$gameEdits = [System.Collections.Generic.List[object]]::new()
$TmateW, $TmateH = 268, 276

$gameEdits.Add(@{
    widget_name = "TeammatePanel"
    canvas_slot = @{
        anchors   = @{ min = @(0, 0.5); max = @(0, 0.5) }
        alignment = @(0, 0.5)
        position  = @(32, 0)
        size      = @($TmateW, $TmateH)
        auto_size = $false
        z_order   = 10
    }
})

for ($i = 0; $i -lt 3; $i++) {
    $pad = @{ }
    if ($i -lt 2) { $pad.bottom = 8 }
    $gameEdits.Add(@{
        widget_name = "TeammateEntries_$i"
        panel_slot  = @{
            size_rule            = "Automatic"
            horizontal_alignment = "Fill"
            vertical_alignment   = "Center"
            padding              = $pad
        }
    })
}

Edit-Wbp $hudPath $gameEdits

$paths = @($entryPath, $hudPath)
Invoke-McpRaw -Method "tools/call" -Params @{
    name = "compile-assets"; arguments = @{ asset_paths = $paths }
} -SessionId $session | Out-Null

$actions = $paths | ForEach-Object { @{ action = "save"; asset_path = $_ } }
Invoke-McpRaw -Method "tools/call" -Params @{
    name = "manage-assets"; arguments = @{ actions = $actions; save = $true }
} -SessionId $session | Out-Null

Write-Host "Teammate portrait layout applied (no TeammatePanelBG)."
Write-Host "Reimport Textures/Teammate for T_HUD_Portrait_Slot.png"

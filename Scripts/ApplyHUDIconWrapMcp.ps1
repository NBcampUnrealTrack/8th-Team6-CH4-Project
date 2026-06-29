# Rebuild teammate entry: [IconWrap Overlay] + [InfoVBox] — icon frame only, no panel BG
param([string]$BaseUrl = "http://127.0.0.1:8080/mcp")

$ErrorActionPreference = "Stop"
$T = "/Game/UI/HUD"
$entryPath = "/Game/Blueprints/UI/WBP_TeammateEntry"
$frameTex = "$T/Textures/Teammate/T_HUD_Portrait_Slot"
$iconTex = "$T/Textures/Teammate/T_HUD_Portrait_Placeholder"

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

function Edit-Wbp([string]$AssetPath, [array]$Edits, [string]$SessionId) {
    Call-Tool "edit-widget-blueprint" @{ asset_path = $AssetPath; edits = $Edits } $SessionId
    Write-Host "[edit] $($Edits.Count) ops"
}

function Remove-W([string]$Name, [string]$SessionId) {
    Edit-Wbp $entryPath @(@{ widget_name = $Name; action = "remove"; occurrence = 0 }) $SessionId
}

function Add-W([string]$Class, [string]$Name, [string]$Parent, [string]$SessionId) {
    Call-Tool "add-widget" @{
        asset_path = $entryPath; widget_class = $Class; widget_name = $Name; parent_widget = $Parent
    } $SessionId
    Write-Host "[add] $Name -> $Parent"
}

$init = Invoke-McpRaw -Method "initialize" -Params @{
    protocolVersion = "2024-11-05"; capabilities = @{}; clientInfo = @{ name = "IconWrap2"; version = "1" }
}
$session = $init.Headers["Mcp-Session-Id"]
Invoke-McpRaw -Method "notifications/initialized" -Params @{} -SessionId $session | Out-Null

# Tear down flat / broken tree
foreach ($n in @("PortraitImage", "PortraitFrame", "PortraitSlotFrame", "PortraitOverlay", "NicknameText", "CageStackText", "DownedHealthBar", "InfoVBox")) {
    try { Remove-W $n $session } catch { Write-Host "[skip remove] $n" }
}

# Icon-only wrap (left) + info column (right)
Add-W "Overlay" "PortraitOverlay" "RootHBox" $session
Add-W "Image" "PortraitSlotFrame" "PortraitOverlay" $session
Add-W "Image" "PortraitImage" "PortraitOverlay" $session
Add-W "VerticalBox" "InfoVBox" "RootHBox" $session
Add-W "TextBlock" "NicknameText" "InfoVBox" $session
Add-W "TextBlock" "CageStackText" "InfoVBox" $session
Add-W "ProgressBar" "DownedHealthBar" "InfoVBox" $session

$FontSemiBold = "/Game/UI/HUD/Fonts/Rajdhani-SemiBold"
$FontMedium = "/Game/UI/HUD/Fonts/Rajdhani-Medium"

Edit-Wbp $entryPath @(
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
            image_texture = $frameTex
            desired_size  = @(80, 80)
            visibility    = "HitTestInvisible"
        }
    },
    @{
        widget_name = "PortraitImage"
        properties  = @{
            image_texture = $iconTex
            desired_size  = @(64, 64)
            visibility    = "HitTestInvisible"
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
            font_path  = $FontSemiBold
            text_color = @(0.97, 0.98, 0.99, 1.0)
            font_size  = 20
        }
    },
    @{
        widget_name = "CageStackText"
        properties  = @{
            font_path  = $FontMedium
            text_color = @(0.82, 0.72, 0.48, 1.0)
            font_size  = 16
        }
        panel_slot  = @{ padding = @{ top = 2 } }
    },
    @{
        widget_name = "DownedHealthBar"
        properties  = @{
            fill_texture       = "$T/Textures/Teammate/T_HUD_Bar_Fill_Downed"
            background_texture = "$T/Textures/Common/T_HUD_Bar_BG"
            percent            = 0.65
            visibility         = "HitTestInvisible"
        }
        panel_slot  = @{ padding = @{ top = 6 }; size_rule = "Fill" }
    }
) $session

Call-Tool "compile-assets" @{ asset_paths = @($entryPath) } $session
Call-Tool "manage-assets" @{ actions = @(@{ action = "save"; asset_path = $entryPath }); save = $true } $session
Write-Host "Done: icon-only wrap on left, info on right."

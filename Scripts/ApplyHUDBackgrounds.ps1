# Add panel background images behind HUD sections
param([string]$BaseUrl = "http://127.0.0.1:8080/mcp")
$ErrorActionPreference = "Stop"
$T = "/Game/UI/HUD"

function Invoke-McpRaw {
    param([string]$Method, [object]$Params, [string]$SessionId = $null)
    $body = @{ jsonrpc = "2.0"; id = 1; method = $Method; params = $Params } | ConvertTo-Json -Depth 40 -Compress
    $headers = @{ "Content-Type" = "application/json"; "Accept" = "application/json, text/event-stream" }
    if ($SessionId) { $headers["Mcp-Session-Id"] = $SessionId }
    Invoke-WebRequest -Uri $BaseUrl -Method POST -Headers $headers -Body $body -UseBasicParsing
}

$init = Invoke-McpRaw -Method "initialize" -Params @{
    protocolVersion = "2024-11-05"; capabilities = @{}; clientInfo = @{ name = "HUDBg"; version = "1" }
}
$session = $init.Headers["Mcp-Session-Id"]
Invoke-McpRaw -Method "notifications/initialized" -Params @{} -SessionId $session | Out-Null

function Call-Tool($Name, [hashtable]$ToolArguments) {
    $r = Invoke-McpRaw -Method "tools/call" -Params @{ name = $Name; arguments = $ToolArguments } -SessionId $session
    if ($r.Content -match '"isError"\s*:\s*true') { Write-Host $r.Content; throw $Name }
}

$hud = "/Game/Blueprints/UI/WBP_GameHUD"
$entry = "/Game/Blueprints/UI/WBP_TeammateEntry"

# skip if already added
$inspect = Invoke-McpRaw -Method "tools/call" -Params @{
    name = "inspect-widget-blueprint"; arguments = @{ asset_path = $hud }
} -SessionId $session
if ($inspect.Content -notmatch "TeammatePanelBG") {
    foreach ($n in @("TeammatePanelBG","DeliveryPanelBG","InventoryPanelBG","PerkPanelBG")) {
        Call-Tool "add-widget" @{ asset_path = $hud; widget_class = "Image"; widget_name = $n; parent_widget = "RootCanvas" }
    }
}

Call-Tool "edit-widget-blueprint" @{
    asset_path = $hud
    edits = @(
        @{
            widget_name = "TeammatePanelBG"
            canvas_slot = @{ anchors = @{min=@(0,0);max=@(0,0)}; alignment=@(0,0); position=@(16,16); size=@(368,244); z_order=5; auto_size=$false }
            properties  = @{ image_texture = "$T/Textures/Common/T_HUD_Panel_BG"; desired_size = @(368,244) }
        },
        @{
            widget_name = "DeliveryPanelBG"
            canvas_slot = @{ anchors = @{min=@(0,1);max=@(0,1)}; alignment=@(0,1); position=@(16,-16); size=@(288,108); z_order=5; auto_size=$false }
            properties  = @{ image_texture = "$T/Textures/Common/T_HUD_Panel_BG"; desired_size = @(288,108) }
        },
        @{
            widget_name = "InventoryPanelBG"
            canvas_slot = @{ anchors = @{min=@(0.5,1);max=@(0.5,1)}; alignment=@(0.5,1); position=@(0,-16); size=@(228,64); z_order=5; auto_size=$false }
            properties  = @{ image_texture = "$T/Textures/Common/T_HUD_Panel_BG"; desired_size = @(228,64) }
        },
        @{
            widget_name = "PerkPanelBG"
            canvas_slot = @{ anchors = @{min=@(1,1);max=@(1,1)}; alignment=@(1,1); position=@(-16,-16); size=@(124,64); z_order=5; auto_size=$false }
            properties  = @{ image_texture = "$T/Textures/Common/T_HUD_Panel_BG"; desired_size = @(124,64) }
        }
    )
}

# portrait frame on teammate entry
if ($inspect.Content -notmatch "PortraitFrame") {
    Call-Tool "add-widget" @{ asset_path = $entry; widget_class = "Image"; widget_name = "PortraitFrame"; parent_widget = "RootHBox" }
}
Call-Tool "edit-widget-blueprint" @{
    asset_path = $entry
    edits = @(
        @{
            widget_name = "PortraitFrame"
            properties  = @{
                image_texture = "$T/Textures/Teammate/T_HUD_Portrait_Frame"
                desired_size  = @(48, 48)
            }
        },
        @{
            widget_name = "RootHBox"
            properties  = @{ visibility = "HitTestInvisible" }
        }
    )
}

Call-Tool "compile-assets" @{ asset_paths = @($hud, $entry) }
Call-Tool "manage-assets" @{
    actions = @(
        @{ action = "save"; asset_path = $hud },
        @{ action = "save"; asset_path = $entry }
    ); save = $true
}
Write-Host "HUD backgrounds applied."

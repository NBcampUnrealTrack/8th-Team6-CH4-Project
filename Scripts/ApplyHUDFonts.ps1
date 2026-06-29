# Import Rajdhani HUD fonts and apply to WBP TextBlocks
param([string]$BaseUrl = "http://127.0.0.1:8080/mcp")

$ErrorActionPreference = "Stop"
$FontDir = Join-Path (Split-Path $PSScriptRoot -Parent) "Content\UI\HUD\Fonts"
$SemiBold = "/Game/UI/HUD/Fonts/Rajdhani-SemiBold"
$Medium = "/Game/UI/HUD/Fonts/Rajdhani-Medium"

function Invoke-McpRaw {
    param([string]$Method, [object]$Params, [string]$SessionId = $null)
    $body = @{ jsonrpc = "2.0"; id = 1; method = $Method; params = $Params } | ConvertTo-Json -Depth 40 -Compress
    $headers = @{ "Content-Type" = "application/json"; "Accept" = "application/json, text/event-stream" }
    if ($SessionId) { $headers["Mcp-Session-Id"] = $SessionId }
    return Invoke-WebRequest -Uri $BaseUrl -Method POST -Headers $headers -Body $body -UseBasicParsing
}

$init = Invoke-McpRaw -Method "initialize" -Params @{
    protocolVersion = "2024-11-05"; capabilities = @{}; clientInfo = @{ name = "HUDFonts"; version = "1.0" }
}
$session = $init.Headers["Mcp-Session-Id"]
Invoke-McpRaw -Method "notifications/initialized" -Params @{} -SessionId $session | Out-Null

function Invoke-McpTool([string]$Name, [object]$Arguments) {
    $resp = Invoke-McpRaw -Method "tools/call" -Params @{
        name = $Name; arguments = $Arguments
    } -SessionId $session
    if ($resp.Content -match '"isError"\s*:\s*true') {
        Write-Host $resp.Content
        throw "$Name failed"
    }
    return $resp.Content
}

Write-Host "[import-assets] Rajdhani Medium + SemiBold"
$importArgs = @{
    imports = @(
        @{
            source_file      = "$FontDir\Rajdhani-Medium.ttf"
            destination_path = "/Game/UI/HUD/Fonts"
            destination_name = "Rajdhani-Medium"
            replace_existing = $true
        },
        @{
            source_file      = "$FontDir\Rajdhani-SemiBold.ttf"
            destination_path = "/Game/UI/HUD/Fonts"
            destination_name = "Rajdhani-SemiBold"
            replace_existing = $true
        }
    )
    save = $true
}

try {
    Invoke-McpTool "import-assets" $importArgs | Out-Null
}
catch {
    Write-Host "[import-assets] auto-detect failed, retrying with FontFactory..."
    $importArgs.imports[0].factory_name = "FontFactory"
    $importArgs.imports[1].factory_name = "FontFactory"
    Invoke-McpTool "import-assets" $importArgs | Out-Null
}

function Edit-Wbp([string]$AssetPath, [array]$Edits) {
    if ($Edits.Count -eq 0) { return }
    $resp = Invoke-McpRaw -Method "tools/call" -Params @{
        name = "edit-widget-blueprint"
        arguments = @{ asset_path = $AssetPath; edits = $Edits }
    } -SessionId $session
    if ($resp.Content -match '"isError"\s*:\s*true') {
        Write-Host $resp.Content
        throw "edit-widget-blueprint failed for $AssetPath"
    }
    Write-Host "[edit] $AssetPath ($($Edits.Count) ops)"
}

function Get-WbpWidgetNames([string]$AssetPath) {
    $resp = Invoke-McpRaw -Method "tools/call" -Params @{
        name = "inspect-widget-blueprint"
        arguments = @{ asset_path = $AssetPath; depth_limit = 1 }
    } -SessionId $session
    return @(($resp.Content | ConvertFrom-Json).result.structuredContent.all_widgets)
}

$entryEdits = @(
    @{
        widget_name = "NicknameText"
        properties  = @{
            font_path  = $SemiBold
            font_size  = 22
            text_color = @(0.97, 0.98, 0.99, 1.0)
        }
    },
    @{
        widget_name = "CageStackText"
        properties  = @{
            font_path  = $Medium
            font_size  = 18
            text_color = @(0.82, 0.72, 0.48, 1.0)
        }
    }
)
Edit-Wbp "/Game/Blueprints/UI/WBP_TeammateEntry" $entryEdits

$gameNames = Get-WbpWidgetNames "/Game/Blueprints/UI/WBP_GameHUD"
$gameEdits = [System.Collections.Generic.List[object]]::new()
foreach ($pair in @(
        @{ Name = "DeliveryLabelA"; Size = 30 },
        @{ Name = "DeliveryLabelB"; Size = 30 }
    )) {
    if ($gameNames -contains $pair.Name) {
        $gameEdits.Add(@{
            widget_name = $pair.Name
            properties  = @{
                font_path  = $SemiBold
                font_size  = $pair.Size
                text_color = @(0.97, 0.98, 0.99, 1.0)
            }
        })
    }
}
Edit-Wbp "/Game/Blueprints/UI/WBP_GameHUD" @($gameEdits)

Invoke-McpTool "manage-assets" @{
    actions = @(
        @{ action = "save"; asset_path = "/Game/Blueprints/UI/WBP_TeammateEntry" },
        @{ action = "save"; asset_path = "/Game/Blueprints/UI/WBP_GameHUD" }
    )
    save = $true
} | Out-Null

Invoke-McpTool "compile-assets" @{
    asset_paths = @("/Game/Blueprints/UI/WBP_TeammateEntry", "/Game/Blueprints/UI/WBP_GameHUD")
} | Out-Null

Write-Host "HUD fonts imported and applied (Rajdhani)."

# SpaCh4 HUD WBP 생성 (UEBridgeMCP HTTP)
param(
    [string]$BaseUrl = "http://127.0.0.1:8080/mcp",
    [int]$WaitSeconds = 180
)

$ErrorActionPreference = "Stop"

function Invoke-McpRaw {
    param([string]$Method, [object]$Params, [string]$SessionId = $null)
    $body = @{ jsonrpc = "2.0"; id = 1; method = $Method; params = $Params } | ConvertTo-Json -Depth 30 -Compress
    $headers = @{ "Content-Type" = "application/json"; "Accept" = "application/json, text/event-stream" }
    if ($SessionId) { $headers["Mcp-Session-Id"] = $SessionId }
    return Invoke-WebRequest -Uri $BaseUrl -Method POST -Headers $headers -Body $body -UseBasicParsing
}

function Wait-McpServer {
    for ($i = 0; $i -lt $WaitSeconds; $i += 2) {
        try {
            $r = Invoke-WebRequest -Uri $BaseUrl -Method GET -UseBasicParsing -TimeoutSec 2
            if ($r.StatusCode -eq 200) { return $true }
        } catch {}
        Start-Sleep -Seconds 2
    }
    return $false
}

function Initialize-Mcp {
    $init = Invoke-McpRaw -Method "initialize" -Params @{
        protocolVersion = "2024-11-05"
        capabilities    = @{}
        clientInfo      = @{ name = "SpaCh4HUD"; version = "1.0" }
    }
    $session = $init.Headers["Mcp-Session-Id"]
    if (-not $session) { throw "No MCP session id" }
    Invoke-McpRaw -Method "notifications/initialized" -Params @{} -SessionId $session | Out-Null
    return $session
}

function Call-Tool {
    param([string]$Session, [string]$Name, [hashtable]$ToolArguments)
    $resp = Invoke-McpRaw -Method "tools/call" -Params @{ name = $Name; arguments = $ToolArguments } -SessionId $Session
    if ($resp.Content -match '"isError"\s*:\s*true') {
        Write-Host $resp.Content
        throw "Tool failed: $Name"
    }
    Write-Host "[$Name] ok"
}

if (-not (Wait-McpServer)) {
    throw "MCP server not reachable at $BaseUrl. Open SpaCh4_Copy editor (UEBridgeMCP enabled) and retry."
}

$session = Initialize-Mcp
Write-Host "MCP ready"

$entryPath = "/Game/Blueprints/UI/WBP_TeammateEntry"
$hudPath = "/Game/Blueprints/UI/WBP_GameHUD"
$bpHudPath = "/Game/Blueprints/UI/BP_GameHUD"
$bpGmPath = "/Game/Blueprints/BP_MatchGameMode"

function New-AssetIfMissing([string]$Path, [string]$Class, [string]$Parent) {
    $toolArguments = @{ asset_path = $Path; asset_class = $Class }
    if ($Parent) { $toolArguments.parent_class = $Parent }
    $resp = Invoke-McpRaw -Method "tools/call" -Params @{ name = "create-asset"; arguments = $toolArguments } -SessionId $session
    if ($resp.Content -match "already exists") {
        Write-Host "exists: $Path"
        return
    }
    if ($resp.Content -match '"isError"\s*:\s*true') { throw "create-asset failed: $Path" }
    Write-Host "created: $Path"
}

function Add-Widget([string]$Asset, [string]$Class, [string]$Name, [string]$Parent = "") {
    $toolArguments = @{ asset_path = $Asset; widget_class = $Class; widget_name = $Name }
    if ($Parent) { $toolArguments.parent_widget = $Parent }
    Call-Tool $session "add-widget" $toolArguments
}

New-AssetIfMissing $entryPath "WidgetBlueprint" "/Script/SpaCh4.TeammateEntryWidget"
New-AssetIfMissing $hudPath "WidgetBlueprint" "/Script/SpaCh4.GameHUDWidget"
New-AssetIfMissing $bpHudPath "Blueprint" "/Script/SpaCh4.GameHUD"
New-AssetIfMissing $bpGmPath "Blueprint" "/Script/SpaCh4.MatchGameMode"

Add-Widget $entryPath "HorizontalBox" "RootHBox"
Add-Widget $entryPath "Image" "PortraitImage" "RootHBox"
Add-Widget $entryPath "VerticalBox" "InfoVBox" "RootHBox"
Add-Widget $entryPath "TextBlock" "NicknameText" "InfoVBox"
Add-Widget $entryPath "TextBlock" "CageStackText" "InfoVBox"
Add-Widget $entryPath "ProgressBar" "DownedHealthBar" "InfoVBox"
Call-Tool $session "compile-assets" @{ asset_paths = @($entryPath); save_before_compile = $true }
Call-Tool $session "manage-assets" @{
    actions = @(@{ action = "save"; asset_path = $entryPath })
    save    = $true
}

Add-Widget $hudPath "CanvasPanel" "RootCanvas"
Add-Widget $hudPath "VerticalBox" "TeammatePanel" "RootCanvas"
Add-Widget $hudPath $entryPath "TeammateEntries_0" "TeammatePanel"
Add-Widget $hudPath $entryPath "TeammateEntries_1" "TeammatePanel"
Add-Widget $hudPath $entryPath "TeammateEntries_2" "TeammatePanel"

Add-Widget $hudPath "VerticalBox" "DeliveryPanel" "RootCanvas"
Add-Widget $hudPath "HorizontalBox" "DeliveryRowA" "DeliveryPanel"
Add-Widget $hudPath "Image" "DeliverySlotA" "DeliveryRowA"
Add-Widget $hudPath "TextBlock" "DeliveryValueA" "DeliveryRowA"
Add-Widget $hudPath "ProgressBar" "DeliveryProgressA" "DeliveryRowA"
Add-Widget $hudPath "HorizontalBox" "DeliveryRowB" "DeliveryPanel"
Add-Widget $hudPath "Image" "DeliverySlotB" "DeliveryRowB"
Add-Widget $hudPath "TextBlock" "DeliveryValueB" "DeliveryRowB"
Add-Widget $hudPath "ProgressBar" "DeliveryProgressB" "DeliveryRowB"

Add-Widget $hudPath "HorizontalBox" "InventoryPanel" "RootCanvas"
for ($i = 0; $i -lt 4; $i++) { Add-Widget $hudPath "Image" "InventorySlots_$i" "InventoryPanel" }

Add-Widget $hudPath "HorizontalBox" "PerkPanel" "RootCanvas"
for ($i = 0; $i -lt 2; $i++) { Add-Widget $hudPath "Image" "PerkSlots_$i" "PerkPanel" }

Call-Tool $session "run-python-script" @{
    script_path = "E:/Unreal/git/SpaCh4_Copy/Scripts/ConfigureHUDBPDefaults.py"
}

Call-Tool $session "compile-assets" @{ asset_paths = @($entryPath, $hudPath, $bpHudPath, $bpGmPath) }
Call-Tool $session "manage-assets" @{
    actions = @(
        @{ action = "save"; asset_path = $entryPath },
        @{ action = "save"; asset_path = $hudPath },
        @{ action = "save"; asset_path = $bpHudPath },
        @{ action = "save"; asset_path = $bpGmPath }
    )
    save = $true
}

Write-Host "HUD MCP build finished."

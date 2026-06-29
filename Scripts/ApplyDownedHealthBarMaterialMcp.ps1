# Apply material to DownedHealthBar via MCP edit-widget-blueprint
param([string]$BaseUrl = "http://127.0.0.1:8080/mcp")

$ErrorActionPreference = "Stop"
$T = "/Game/UI/HUD"
$Entry = "/Game/Blueprints/UI/WBP_TeammateEntry"
$MI = "$T/Materials/MI_HUD_HealthBarFill_Downed"

function Invoke-McpRaw {
    param([string]$Method, [object]$Params, [string]$SessionId = $null)
    $body = @{ jsonrpc = "2.0"; id = 1; method = $Method; params = $Params } | ConvertTo-Json -Depth 40 -Compress
    $headers = @{ "Content-Type" = "application/json"; "Accept" = "application/json, text/event-stream" }
    if ($SessionId) { $headers["Mcp-Session-Id"] = $SessionId }
    return Invoke-WebRequest -Uri $BaseUrl -Method POST -Headers $headers -Body $body -UseBasicParsing
}

$init = Invoke-McpRaw -Method "initialize" -Params @{
    protocolVersion = "2024-11-05"; capabilities = @{}; clientInfo = @{ name = "ApplyHealthMat"; version = "1" }
}
$session = $init.Headers["Mcp-Session-Id"]
Invoke-McpRaw -Method "notifications/initialized" -Params @{} -SessionId $session | Out-Null

# Build material first
Invoke-McpRaw -Method "tools/call" -Params @{
    name = "run-python-script"
    arguments = @{ script_path = (Join-Path $PSScriptRoot "BuildHUDHealthBarMaterial.py") }
} -SessionId $session | Out-Null

Invoke-McpRaw -Method "tools/call" -Params @{
    name = "run-python-script"
    arguments = @{ script_path = (Join-Path $PSScriptRoot "ForceDownedHealthBarMaterial.py") }
} -SessionId $session | Out-Null

$resp = Invoke-McpRaw -Method "tools/call" -Params @{
    name = "edit-widget-blueprint"
    arguments = @{
        asset_path = $Entry
        edits      = @(
            @{
                widget_name = "DownedHealthBar"
                properties  = @{
                    fill_material = $MI
                    percent       = 0.65
                    visibility    = "Collapsed"
                }
            }
        )
    }
} -SessionId $session

Write-Host $resp.Content

Invoke-McpRaw -Method "tools/call" -Params @{
    name = "compile-assets"; arguments = @{ asset_paths = @($Entry) }
} -SessionId $session | Out-Null

Write-Host "Applied $MI to DownedHealthBar fill_material"

# Builds animated downed-health-bar UI material via Unreal MCP Python.
param([string]$BaseUrl = "http://127.0.0.1:8080/mcp")

$ErrorActionPreference = "Stop"
$ScriptPath = Join-Path $PSScriptRoot "BuildHUDHealthBarMaterial.py"

function Invoke-McpRaw {
    param([string]$Method, [object]$Params, [string]$SessionId = $null)
    $body = @{ jsonrpc = "2.0"; id = 1; method = $Method; params = $Params } | ConvertTo-Json -Depth 40 -Compress
    $headers = @{ "Content-Type" = "application/json"; "Accept" = "application/json, text/event-stream" }
    if ($SessionId) { $headers["Mcp-Session-Id"] = $SessionId }
    return Invoke-WebRequest -Uri $BaseUrl -Method POST -Headers $headers -Body $body -UseBasicParsing
}

$init = Invoke-McpRaw -Method "initialize" -Params @{
    protocolVersion = "2024-11-05"; capabilities = @{}; clientInfo = @{ name = "HUDHealthMat"; version = "1" }
}
$session = $init.Headers["Mcp-Session-Id"]
Invoke-McpRaw -Method "notifications/initialized" -Params @{} -SessionId $session | Out-Null

$resp = Invoke-McpRaw -Method "tools/call" -Params @{
    name = "run-python-script"
    arguments = @{ script_path = $ScriptPath }
} -SessionId $session

if ($resp.Content -match '"isError"\s*:\s*true') {
    Write-Host $resp.Content
    throw "run-python-script failed"
}

Write-Host $resp.Content
Write-Host "Done. Recompile SpaCh4 C++ and test L_UI_Test."

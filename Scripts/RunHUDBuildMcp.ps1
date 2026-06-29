# Run HUD build inside open editor via MCP
param([string]$BaseUrl = "http://127.0.0.1:8080/mcp", [int]$WaitSeconds = 120)

$ErrorActionPreference = "Stop"
$scriptPath = Join-Path $PSScriptRoot "BuildHUDWidgetsEditor.py"
$script = Get-Content $scriptPath -Raw

function Invoke-McpRaw {
    param([string]$Method, [object]$Params, [string]$SessionId = $null)
    $body = @{ jsonrpc = "2.0"; id = 1; method = $Method; params = $Params } | ConvertTo-Json -Depth 30 -Compress
    $headers = @{ "Content-Type" = "application/json"; "Accept" = "application/json, text/event-stream" }
    if ($SessionId) { $headers["Mcp-Session-Id"] = $SessionId }
    return Invoke-WebRequest -Uri $BaseUrl -Method POST -Headers $headers -Body $body -UseBasicParsing
}

for ($i = 0; $i -lt $WaitSeconds; $i += 2) {
    try {
        if ((Invoke-WebRequest -Uri $BaseUrl -Method GET -UseBasicParsing -TimeoutSec 2).StatusCode -eq 200) { break }
    } catch {}
    Start-Sleep 2
}

$init = Invoke-McpRaw -Method "initialize" -Params @{
    protocolVersion = "2024-11-05"; capabilities = @{}; clientInfo = @{ name = "SpaCh4HUD"; version = "1.0" }
}
$session = $init.Headers["Mcp-Session-Id"]
Invoke-McpRaw -Method "notifications/initialized" -Params @{} -SessionId $session | Out-Null

$resp = Invoke-McpRaw -Method "tools/call" -Params @{
    name = "run-python-script"
    arguments = @{ script_path = $scriptPath }
} -SessionId $session

Write-Host $resp.Content
if ($resp.Content -match '"isError"\s*:\s*true') { throw "run-python-script failed" }
Write-Host "Done."

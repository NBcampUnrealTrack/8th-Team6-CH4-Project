param([string]$BaseUrl = "http://127.0.0.1:8080/mcp")
$ErrorActionPreference = "Stop"
function Invoke-McpRaw {
    param([string]$Method, [object]$Params, [string]$SessionId = $null)
    $body = @{ jsonrpc = "2.0"; id = 1; method = $Method; params = $Params } | ConvertTo-Json -Depth 20 -Compress
    $headers = @{ "Content-Type" = "application/json"; "Accept" = "application/json, text/event-stream" }
    if ($SessionId) { $headers["Mcp-Session-Id"] = $SessionId }
    return Invoke-WebRequest -Uri $BaseUrl -Method POST -Headers $headers -Body $body -UseBasicParsing
}
$init = Invoke-McpRaw -Method "initialize" -Params @{ protocolVersion = "2024-11-05"; capabilities = @{}; clientInfo = @{ name = "LC"; version = "1" } }
$session = $init.Headers["Mcp-Session-Id"]
Invoke-McpRaw -Method "notifications/initialized" -Params @{} -SessionId $session | Out-Null
$resp = Invoke-McpRaw -Method "tools/call" -Params @{ name = "trigger-live-coding"; arguments = @{} } -SessionId $session
Write-Host $resp.Content

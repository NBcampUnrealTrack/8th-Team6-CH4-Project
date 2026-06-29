param([string]$BaseUrl = "http://127.0.0.1:8080/mcp")
$ErrorActionPreference = "Stop"
function Invoke-McpRaw {
    param([string]$Method, [object]$Params, [string]$SessionId = $null)
    $body = @{ jsonrpc = "2.0"; id = 1; method = $Method; params = $Params } | ConvertTo-Json -Depth 40 -Compress
    $headers = @{ "Content-Type" = "application/json"; "Accept" = "application/json, text/event-stream" }
    if ($SessionId) { $headers["Mcp-Session-Id"] = $SessionId }
    return Invoke-WebRequest -Uri $BaseUrl -Method POST -Headers $headers -Body $body -UseBasicParsing
}
$init = Invoke-McpRaw -Method "initialize" -Params @{ protocolVersion = "2024-11-05"; capabilities = @{}; clientInfo = @{ name = "x"; version = "1" } }
$session = $init.Headers["Mcp-Session-Id"]
Invoke-McpRaw -Method "notifications/initialized" -Params @{} -SessionId $session | Out-Null
$resp = Invoke-McpRaw -Method "tools/call" -Params @{
    name = "inspect-widget-blueprint"
    arguments = @{
        asset_path = "/Game/Blueprints/UI/WBP_GameHUD"
        depth_limit = 5
        include_defaults = $true
    }
} -SessionId $session
$json = $resp.Content | ConvertFrom-Json
function Find-Widget($node, [string]$name) {
    if ($node.name -eq $name) { return $node }
    foreach ($c in @($node.children)) {
        $f = Find-Widget $c $name
        if ($f) { return $f }
    }
    return $null
}
$root = $json.result.structuredContent.root_widget
foreach ($w in @("DeliveryLabelA","DeliveryIconA","DeliveryLabelB","DeliveryIconB")) {
    $n = Find-Widget $root $w
    if ($n) {
        Write-Host "=== $w ==="
        Write-Host ($n | ConvertTo-Json -Depth 8)
    } else {
        Write-Host "=== $w NOT FOUND ==="
    }
}

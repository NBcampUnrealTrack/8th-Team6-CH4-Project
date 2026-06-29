param([string]$BaseUrl = "http://127.0.0.1:8080/mcp")
$ErrorActionPreference = "Stop"

function Invoke-McpRaw {
    param([string]$Method, [object]$Params, [string]$SessionId = $null)
    $body = @{ jsonrpc = "2.0"; id = 1; method = $Method; params = $Params } | ConvertTo-Json -Depth 40 -Compress
    $headers = @{ "Content-Type" = "application/json"; "Accept" = "application/json, text/event-stream" }
    if ($SessionId) { $headers["Mcp-Session-Id"] = $SessionId }
    return Invoke-WebRequest -Uri $BaseUrl -Method POST -Headers $headers -Body $body -UseBasicParsing
}

$init = Invoke-McpRaw -Method "initialize" -Params @{
    protocolVersion = "2024-11-05"; capabilities = @{}; clientInfo = @{ name = "InspectDelivery"; version = "1" }
}
$session = $init.Headers["Mcp-Session-Id"]
Invoke-McpRaw -Method "notifications/initialized" -Params @{} -SessionId $session | Out-Null

$resp = Invoke-McpRaw -Method "tools/call" -Params @{
    name = "inspect-widget-blueprint"
    arguments = @{ asset_path = "/Game/Blueprints/UI/WBP_GameHUD"; depth_limit = 4 }
} -SessionId $session
Write-Host $resp.Content

foreach ($path in @(
    "/Game/UI/HUD/Textures/Delivery/T_HUD_Delivery_Label_A",
    "/Game/UI/HUD/Textures/Delivery/T_HUD_Delivery_Label_B",
    "/Game/UI/HUD/Textures/Delivery/T_HUD_Delivery_Slot_A",
    "/Game/UI/HUD/Textures/Delivery/T_HUD_Delivery_Slot_B",
    "/Game/UI/HUD/Textures/Delivery/T_HUD_Delivery_Slot"
)) {
    $q = Invoke-McpRaw -Method "tools/call" -Params @{
        name = "query-asset"
        arguments = @{ asset_path = $path }
    } -SessionId $session
    $ok = ($q.Content -notmatch '"isError"\s*:\s*true')
    Write-Host "$path exists=$ok"
}

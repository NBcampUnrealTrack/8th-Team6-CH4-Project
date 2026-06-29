# UEBridgeMCP HTTP helper for SpaCh4_Copy HUD build
param(
    [string]$BaseUrl = "http://127.0.0.1:8080/mcp"
)

$ErrorActionPreference = "Stop"

function Invoke-Mcp {
    param(
        [string]$Method,
        [hashtable]$Params = @{},
        [string]$SessionId = $null
    )

    $body = @{
        jsonrpc = "2.0"
        id      = [guid]::NewGuid().ToString()
        method  = $Method
        params  = $Params
    } | ConvertTo-Json -Depth 20 -Compress

    $headers = @{
        "Content-Type" = "application/json"
        "Accept"       = "application/json, text/event-stream"
    }
    if ($SessionId) {
        $headers["Mcp-Session-Id"] = $SessionId
    }

    $response = Invoke-WebRequest -Uri $BaseUrl -Method POST -Headers $headers -Body $body -UseBasicParsing
    return $response
}

function Initialize-McpSession {
    $init = Invoke-Mcp -Method "initialize" -Params @{
        protocolVersion = "2024-11-05"
        capabilities    = @{}
        clientInfo      = @{ name = "SpaCh4HUD"; version = "1.0" }
    }

    $sessionId = $init.Headers["Mcp-Session-Id"]
    if (-not $sessionId) {
        throw "MCP initialize did not return Mcp-Session-Id"
    }

    Invoke-Mcp -Method "notifications/initialized" -Params @{} -SessionId $sessionId | Out-Null
    return $sessionId
}

function Call-McpTool {
    param(
        [string]$SessionId,
        [string]$ToolName,
        [hashtable]$Arguments
    )

    $response = Invoke-Mcp -Method "tools/call" -Params @{
        name      = $ToolName
        arguments = $Arguments
    } -SessionId $SessionId

    return $response.Content
}

$session = Initialize-McpSession
Write-Output "MCP session: $session"

$tools = (Invoke-Mcp -Method "tools/list" -Params @{} -SessionId $session).Content
Write-Output $tools

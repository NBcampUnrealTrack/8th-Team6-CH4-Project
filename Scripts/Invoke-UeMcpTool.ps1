param(
    [Parameter(Mandatory = $true)][string]$ToolName,
    [hashtable]$Arguments = @{}
)

$body = @{
    jsonrpc = "2.0"
    id      = 1
    method  = "tools/call"
    params  = @{
        name      = $ToolName
        arguments = $Arguments
    }
} | ConvertTo-Json -Depth 20 -Compress

$response = Invoke-RestMethod -Uri "http://127.0.0.1:8080/mcp" -Method POST -ContentType "application/json" -Body $body -TimeoutSec 300
$response | ConvertTo-Json -Depth 30

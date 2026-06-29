# Create/update MI to match downed health bar material (green fill texture).
param([string]$BaseUrl = "http://127.0.0.1:8080/mcp")

$scriptPath = "E:/Unreal/git/SpaCh4/Scripts/SetupDeliveryProgressHealthBarStyle.py"
$script = Get-Content -Raw -Encoding UTF8 $scriptPath

$body = @{ jsonrpc = "2.0"; id = 1; method = "initialize"; params = @{ protocolVersion = "2024-11-05"; capabilities = @{}; clientInfo = @{ name = "m"; version = "1" } } } | ConvertTo-Json -Compress
$init = Invoke-WebRequest -Uri $BaseUrl -Method POST -Headers @{ "Content-Type"="application/json"; "Accept"="application/json, text/event-stream" } -Body $body -UseBasicParsing
$session = $init.Headers["Mcp-Session-Id"]
$body2 = @{ jsonrpc = "2.0"; id = 2; method = "tools/call"; params = @{ name = "run-python-script"; arguments = @{ script = $script } } } | ConvertTo-Json -Depth 6 -Compress
$r = Invoke-WebRequest -Uri $BaseUrl -Method POST -Headers @{ "Content-Type"="application/json"; "Accept"="application/json, text/event-stream"; "Mcp-Session-Id"=$session } -Body $body2 -UseBasicParsing
Write-Host ($r.Content | ConvertFrom-Json).result.structuredContent.output

# Creates HUD UI material + default MI for soft-tinted icon rendering in UMG.
param([string]$BaseUrl = "http://127.0.0.1:8080/mcp")

$ErrorActionPreference = "Stop"
$MatPath = "/Game/UI/HUD/Materials/M_HUD_Icon"
$MiPath = "/Game/UI/HUD/Materials/MI_HUD_Icon"

function Invoke-McpRaw {
    param([string]$Method, [object]$Params, [string]$SessionId = $null)
    $body = @{ jsonrpc = "2.0"; id = 1; method = $Method; params = $Params } | ConvertTo-Json -Depth 40 -Compress
    $headers = @{ "Content-Type" = "application/json"; "Accept" = "application/json, text/event-stream" }
    if ($SessionId) { $headers["Mcp-Session-Id"] = $SessionId }
    return Invoke-WebRequest -Uri $BaseUrl -Method POST -Headers $headers -Body $body -UseBasicParsing
}

function Call-Tool([string]$Name, [object]$Args, [string]$SessionId) {
    $resp = Invoke-McpRaw -Method "tools/call" -Params @{
        name = $Name; arguments = $Args
    } -SessionId $SessionId
    if ($resp.Content -match '"isError"\s*:\s*true') {
        Write-Host $resp.Content
        throw "$Name failed"
    }
    return $resp.Content
}

$init = Invoke-McpRaw -Method "initialize" -Params @{
    protocolVersion = "2024-11-05"; capabilities = @{}; clientInfo = @{ name = "HUDMat"; version = "1" }
}
$session = $init.Headers["Mcp-Session-Id"]
Invoke-McpRaw -Method "notifications/initialized" -Params @{} -SessionId $session | Out-Null

Call-Tool "create-asset" @{
    asset_path = $MatPath
    asset_class = "Material"
} -SessionId $session | Out-Null
Write-Host "[create] $MatPath"

$nodes = @(
    @{ node_class = "MaterialExpressionTextureSampleParameter2D"; node_name = "TexParam"; position = @(0, 0); properties = @{ ParameterName = "IconTexture"; SamplerType = "Color" } }
    @{ node_class = "MaterialExpressionVectorParameter"; node_name = "TintParam"; position = @(0, 220); properties = @{ ParameterName = "Tint"; DefaultValue = @(0.92, 0.94, 0.96, 1.0) } }
    @{ node_class = "MaterialExpressionMultiply"; node_name = "TintMul"; position = @(280, 80) }
    @{ node_class = "MaterialExpressionComponentMask"; node_name = "AlphaMask"; position = @(280, 260); properties = @{ R = $false; G = $false; B = $false; A = $true } }
)

foreach ($n in $nodes) {
    Call-Tool "add-graph-node" @{
        asset_path = $MatPath
        node_class = $n.node_class
        node_name  = $n.node_name
        position   = $n.position
        properties = $n.properties
    } -SessionId $session | Out-Null
    Write-Host "[node] $($n.node_name)"
}

Call-Tool "connect-graph-pins" @{
    asset_path   = $MatPath
    source_node  = "TexParam"
    source_pin   = "RGB"
    target_node  = "TintMul"
    target_pin   = "A"
} -SessionId $session | Out-Null

Call-Tool "connect-graph-pins" @{
    asset_path   = $MatPath
    source_node  = "TintParam"
    source_pin   = "RGB"
    target_node  = "TintMul"
    target_pin   = "B"
} -SessionId $session | Out-Null

Call-Tool "connect-graph-pins" @{
    asset_path   = $MatPath
    source_node  = "TintMul"
    source_pin   = "Output"
    target_node  = "Material"
    target_pin   = "EmissiveColor"
} -SessionId $session | Out-Null

Call-Tool "connect-graph-pins" @{
    asset_path   = $MatPath
    source_node  = "TexParam"
    source_pin   = "A"
    target_node  = "AlphaMask"
    target_pin   = "Input"
} -SessionId $session | Out-Null

Call-Tool "connect-graph-pins" @{
    asset_path   = $MatPath
    source_node  = "AlphaMask"
    source_pin   = "Output"
    target_node  = "Material"
    target_pin   = "Opacity"
} -SessionId $session | Out-Null

Call-Tool "create-asset" @{
    asset_path  = $MiPath
    asset_class = "MaterialInstanceConstant"
    parent_class = $MatPath
} -SessionId $session | Out-Null
Write-Host "[create] $MiPath"

Call-Tool "compile-assets" @{ asset_paths = @($MatPath, $MiPath) } -SessionId $session | Out-Null
Call-Tool "manage-assets" @{ actions = @(@{ action = "save"; asset_path = $MatPath }, @{ action = "save"; asset_path = $MiPath }); save = $true } -SessionId $session | Out-Null
Write-Host "HUD UI material ready: $MiPath"

# Import Station A/B PNGs via MCP Python.
param([string]$BaseUrl = "http://127.0.0.1:8080/mcp")

$script = @'
import unreal
from pathlib import Path
ROOT = Path(r"E:/Unreal/git/SpaCh4/Content/UI/HUD/Textures/Delivery")
for name in ["T_HUD_Delivery_Station_A", "T_HUD_Delivery_Station_B"]:
    png = str(ROOT / f"{name}.png").replace("\\", "/")
    if not Path(png).is_file():
        raise RuntimeError(f"Missing {png}")
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", png)
    task.set_editor_property("destination_path", "/Game/UI/HUD/Textures/Delivery")
    task.set_editor_property("destination_name", name)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    tex = unreal.EditorAssetLibrary.load_asset(f"/Game/UI/HUD/Textures/Delivery/{name}.{name}")
    if not tex:
        raise RuntimeError(f"Import failed: {name}")
    tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_BC7)
    tex.set_editor_property("filter", unreal.TextureFilter.TF_BILINEAR)
    tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    tex.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
    tex.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
    unreal.EditorAssetLibrary.save_loaded_asset(tex)
    unreal.log("Imported " + tex.get_path_name())
'@

$body = @{ jsonrpc = "2.0"; id = 1; method = "initialize"; params = @{ protocolVersion = "2024-11-05"; capabilities = @{}; clientInfo = @{ name = "m"; version = "1" } } } | ConvertTo-Json -Compress
$init = Invoke-WebRequest -Uri $BaseUrl -Method POST -Headers @{ "Content-Type"="application/json"; "Accept"="application/json, text/event-stream" } -Body $body -UseBasicParsing
$session = $init.Headers["Mcp-Session-Id"]
$body2 = @{ jsonrpc = "2.0"; id = 2; method = "tools/call"; params = @{ name = "run-python-script"; arguments = @{ script = $script } } } | ConvertTo-Json -Depth 6 -Compress
$r = Invoke-WebRequest -Uri $BaseUrl -Method POST -Headers @{ "Content-Type"="application/json"; "Accept"="application/json, text/event-stream"; "Mcp-Session-Id"=$session } -Body $body2 -UseBasicParsing
Write-Host ($r.Content | ConvertFrom-Json).result.structuredContent.output

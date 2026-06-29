"""Ensure WBP_GameHUD uses preview teammate data for L_UI_Test."""
import unreal

WBP = "/Game/Blueprints/UI/WBP_GameHUD"
BP = unreal.EditorAssetLibrary.load_asset(WBP)
if not BP:
    raise RuntimeError(f"Missing {WBP}")

cdo = unreal.get_default_object(BP.generated_class())
cdo.set_editor_property("bUsePreviewData", True)
cdo.set_editor_property("PreviewTeammateData", [])

unreal.EditorAssetLibrary.save_loaded_asset(BP)
unreal.log("WBP_GameHUD bUsePreviewData=true, preview array reset.")

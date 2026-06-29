import unreal

BP_HUD = "/Game/Blueprints/UI/BP_GameHUD"
BP_GM = "/Game/Blueprints/BP_MatchGameMode"
WBP_HUD = "/Game/Blueprints/UI/WBP_GameHUD.WBP_GameHUD_C"
BP_HUD_CLASS = "/Game/Blueprints/UI/BP_GameHUD.BP_GameHUD_C"


def load_cls(path):
    cls = unreal.load_class(None, path)
    if not cls:
        raise RuntimeError(f"load_class failed: {path}")
    unreal.log(f"loaded {path} -> {cls.get_name()}")
    return cls


def set_and_save(bp_path, prop, cls):
    bp = unreal.load_asset(bp_path)
    cdo = unreal.get_default_object(bp.generated_class())
    before = cdo.get_editor_property(prop)
    unreal.log(f"before {bp_path}.{prop} = {before.get_name() if before else None}")
    cdo.set_editor_property(prop, cls)
    after = cdo.get_editor_property(prop)
    unreal.log(f"after  {bp_path}.{prop} = {after.get_name() if after else None}")
    unreal.EditorAssetLibrary.save_loaded_asset(bp)


wbp_cls = load_cls(WBP_HUD)
bp_hud_cls = load_cls(BP_HUD_CLASS)

set_and_save(BP_HUD, "game_hud_widget_class", wbp_cls)
set_and_save(BP_GM, "hud_class", bp_hud_cls)

# verify from reloaded asset
bp_gm = unreal.load_asset(BP_GM)
cdo = unreal.get_default_object(bp_gm.generated_class())
final = cdo.get_editor_property("hud_class")
unreal.log(f"final hud_class = {final.get_name() if final else None}")
if not final or final.get_name() != "BP_GameHUD_C":
    raise RuntimeError("BP_MatchGameMode hud_class not persisted")

print("defaults configured")

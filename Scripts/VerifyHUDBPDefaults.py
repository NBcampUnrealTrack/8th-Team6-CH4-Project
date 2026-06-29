import unreal

def verify(path, prop, expected_suffix):
    bp = unreal.load_asset(path)
    cdo = unreal.get_default_object(bp.generated_class())
    val = cdo.get_editor_property(prop)
    name = val.get_name() if val else "None"
    unreal.log(f"{path} {prop} = {name}")
    if expected_suffix not in name:
        raise RuntimeError(f"Expected {expected_suffix} in {name}")

verify("/Game/Blueprints/UI/BP_GameHUD", "game_hud_widget_class", "WBP_GameHUD")
verify("/Game/Blueprints/BP_MatchGameMode", "hud_class", "BP_GameHUD")
print("defaults ok")

"""
Apply MI_HUD_HealthBarFill_Downed to downed health bar widgets in WBP_TeammateEntry.

Works on UE 5.5+ (uses EditorUtilityLibrary, not protected WidgetTree).

Run:
  python via MCP run-python-script
"""

from __future__ import annotations

import unreal

ENTRY = "/Game/Blueprints/UI/WBP_TeammateEntry"
MI = "/Game/UI/HUD/Materials/MI_HUD_HealthBarFill_Downed"
MAT = "/Game/UI/HUD/Materials/M_HUD_HealthBarFill"
FILL_TEX = "/Game/UI/HUD/Textures/Teammate/T_HUD_Bar_Fill_Downed"
BAR_SIZE = unreal.Vector2D(560.0, 60.0)


def load_obj(path: str):
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.EditorAssetLibrary.load_asset(path)
    short = path.split("/")[-1]
    full = f"{path}.{short}"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.load_asset(full)
    return unreal.load_asset(full)


def ensure_mi():
    mi = load_obj(MI)
    if mi:
        return mi

    mat = load_obj(MAT)
    if not mat:
        raise RuntimeError(f"Missing material: {MAT}")

    tools = unreal.AssetToolsHelpers.get_asset_tools()
    factory = unreal.MaterialInstanceConstantFactoryNew()
    mi = tools.create_asset(
        "MI_HUD_HealthBarFill_Downed",
        "/Game/UI/HUD/Materials",
        unreal.MaterialInstanceConstant,
        factory,
    )
    unreal.MaterialEditingLibrary.set_material_instance_parent(mi, mat)
    tex = load_obj(FILL_TEX)
    if tex:
        unreal.MaterialEditingLibrary.set_material_instance_texture_parameter_value(
            mi, "FillTexture", tex
        )
    unreal.EditorAssetLibrary.save_loaded_asset(mi)
    return mi


def find_widget(wbp, name: str):
    return unreal.EditorUtilityLibrary.find_source_widget_by_name(wbp, name)


def apply_material_to_progress_bar(bar, mi) -> bool:
    if not bar:
        return False
    style = bar.get_editor_property("widget_style")
    fill = style.get_editor_property("fill_image")
    fill.set_editor_property("resource_object", mi)
    fill.set_editor_property("draw_as", unreal.SlateBrushDrawType.IMAGE)
    fill.set_editor_property("image_size", BAR_SIZE)
    style.set_editor_property("fill_image", fill)
    bar.set_editor_property("widget_style", style)
    bar.set_editor_property("fill_color_and_opacity", unreal.LinearColor(1, 1, 1, 1))
    return True


def apply_material_to_image(image, mi) -> bool:
    if not image:
        return False
    brush = unreal.SlateBrush()
    brush.set_editor_property("resource_object", mi)
    brush.set_editor_property("draw_as", unreal.SlateBrushDrawType.IMAGE)
    brush.set_editor_property("tiling", unreal.SlateBrushTileType.NO_TILE)
    brush.set_editor_property("image_size", BAR_SIZE)
    image.set_brush(brush)
    image.set_editor_property("desired_size_override", BAR_SIZE)
    image.set_color_and_opacity(unreal.LinearColor(1, 1, 1, 1))
    return True


def main() -> None:
    wbp = load_obj(ENTRY)
    if not wbp:
        raise RuntimeError(f"Missing widget blueprint: {ENTRY}")

    mi = ensure_mi()
    applied = False

    bar = find_widget(wbp, "DownedHealthBar")
    if apply_material_to_progress_bar(bar, mi):
        applied = True
        unreal.log("Applied MI to DownedHealthBar ProgressBar fill.")

    fill = find_widget(wbp, "DownedHealthBarFill")
    if apply_material_to_image(fill, mi):
        applied = True
        unreal.log("Applied MI to DownedHealthBarFill Image.")

    if not applied:
        raise RuntimeError("Neither DownedHealthBar nor DownedHealthBarFill found in WBP.")

    unreal.EditorAssetLibrary.save_loaded_asset(wbp)
    unreal.EditorAssetLibrary.save_loaded_asset(mi)
    unreal.log("Downed health bar material applied and saved.")


main()

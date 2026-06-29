"""
Assign MI_HUD_HealthBarFill_Downed to DownedHealthBarFill Image in WBP.

Run via MCP run-python-script.
"""

import unreal

ENTRY_PATH = "/Game/Blueprints/UI/WBP_TeammateEntry"
MI_PATH = "/Game/UI/HUD/Materials/MI_HUD_HealthBarFill_Downed"
BAR_SIZE = unreal.Vector2D(560.0, 60.0)


def find_widget(tree, name):
    found = [None]

    def visit(widget):
        if found[0]:
            return
        if widget.get_name() == name:
            found[0] = widget

    tree.for_each_widget(visit)
    return found[0]


def make_mat_brush(material):
    brush = unreal.SlateBrush()
    brush.set_editor_property("resource_object", material)
    brush.set_editor_property("draw_as", unreal.SlateBrushDrawType.IMAGE)
    brush.set_editor_property("tiling", unreal.SlateBrushTileType.NO_TILE)
    brush.set_editor_property("image_size", BAR_SIZE)
    return brush


def apply():
    if not unreal.EditorAssetLibrary.does_asset_exist(MI_PATH):
        raise RuntimeError(f"Missing {MI_PATH}. Run BuildHUDHealthBarMaterial.py first.")

    wbp = unreal.load_asset(ENTRY_PATH)
    if not wbp:
        raise RuntimeError(f"Missing {ENTRY_PATH}")

    fill = find_widget(wbp.get_editor_property("WidgetTree"), "DownedHealthBarFill")
    if not fill:
        raise RuntimeError("DownedHealthBarFill not found. Run FixDownedHealthBarLayout.py first.")

    mi = unreal.load_asset(MI_PATH)
    fill.set_brush(make_mat_brush(mi))
    fill.set_editor_property("desired_size_override", BAR_SIZE)
    fill.set_color_and_opacity(unreal.LinearColor(1, 1, 1, 1))
    fill.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)

    unreal.EditorAssetLibrary.save_loaded_asset(wbp)
    unreal.log(f"Applied {MI_PATH} to DownedHealthBarFill.")


apply()

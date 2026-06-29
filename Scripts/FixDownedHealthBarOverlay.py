"""
Rebuild WBP_TeammateEntry downed health bar: Overlay(BG frame + animated MI fill).

Run via MCP run-python-script while Unreal Editor is open.
"""

import unreal

ENTRY_PATH = "/Game/Blueprints/UI/WBP_TeammateEntry"
T = "/Game/UI/HUD"
BG_TEX = f"{T}/Textures/Common/T_HUD_Bar_BG"
MI = f"{T}/Materials/MI_HUD_HealthBarFill_Downed"
BAR_SIZE = unreal.Vector2D(248.0, 27.0)
PREVIEW_FILL = unreal.Vector2D(248.0 * 0.65, 27.0)


def load_obj(path):
    full = path if "." in path else f"{path}.{path.rsplit('/', 1)[-1]}"
    return unreal.load_object(None, full)


def make_tex_brush(tex, size):
    brush = unreal.SlateBrush()
    if tex:
        brush.set_editor_property("resource_object", tex)
    brush.set_editor_property("draw_as", unreal.SlateBrushDrawType.IMAGE)
    brush.set_editor_property("tiling", unreal.SlateBrushTileType.NO_TILE)
    brush.set_editor_property("image_size", size)
    return brush


def make_mat_brush(material, size):
    brush = unreal.SlateBrush()
    if material:
        brush.set_editor_property("resource_object", material)
    brush.set_editor_property("draw_as", unreal.SlateBrushDrawType.IMAGE)
    brush.set_editor_property("tiling", unreal.SlateBrushTileType.NO_TILE)
    brush.set_editor_property("image_size", size)
    return brush


def find_widget(tree, name):
    if hasattr(tree, "find_widget"):
        return tree.find_widget(name)
    found = [None]

    def visit(widget):
        if found[0]:
            return
        if widget.get_name() == name:
            found[0] = widget

    tree.for_each_widget(visit)
    return found[0]


def remove_if_exists(tree, name):
    widget = find_widget(tree, name)
    if not widget:
        return
    parent = widget.get_parent()
    if parent:
        parent.remove_child(widget)
    tree.remove_widget(widget)


def rebuild():
    wbp = unreal.load_asset(ENTRY_PATH)
    if not wbp:
        raise RuntimeError(f"Missing {ENTRY_PATH}")

    tree = wbp.get_editor_property("WidgetTree")
    info = find_widget(tree, "InfoVBox")
    if not info:
        raise RuntimeError("InfoVBox not found")

    remove_if_exists(tree, "DownedHealthBar")
    remove_if_exists(tree, "DownedHealthBarRoot")
    remove_if_exists(tree, "DownedHealthBarBG")
    remove_if_exists(tree, "DownedHealthBarFill")

    root = tree.construct_widget(unreal.Overlay, "DownedHealthBarRoot")
    bg = tree.construct_widget(unreal.Image, "DownedHealthBarBG")
    fill = tree.construct_widget(unreal.Image, "DownedHealthBarFill")
    info.add_child(root)
    root.add_child(fill)
    root.add_child(bg)

    info_slot = None
    for slot in info.get_slots():
        if slot.get_content() == root:
            info_slot = slot
            break
    if info_slot:
        info_slot.set_size(unreal.SlateChildSize(unreal.SlateSizeRule.AUTOMATIC))
        info_slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
        info_slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)

    bg_slot = bg.slot
    bg_slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
    bg_slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_FILL)

    fill_slot = fill.slot
    fill_slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_LEFT)
    fill_slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_FILL)

    bg_tex = load_obj(BG_TEX)
    mi = load_obj(MI)
    bg.set_brush(make_tex_brush(bg_tex, BAR_SIZE))
    fill.set_brush(make_mat_brush(mi, BAR_SIZE))
    bg.set_editor_property("desired_size_override", BAR_SIZE)
    fill.set_editor_property("desired_size_override", PREVIEW_FILL)
    bg.set_color_and_opacity(unreal.LinearColor(1, 1, 1, 1))
    fill.set_color_and_opacity(unreal.LinearColor(1, 1, 1, 1))
    bg.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
    fill.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
    root.set_visibility(unreal.SlateVisibility.COLLAPSED)

    unreal.KismetEditorUtilities.compile_blueprint(wbp)
    unreal.EditorAssetLibrary.save_loaded_asset(wbp)
    unreal.log("Downed health bar overlay + MI fill applied.")


rebuild()

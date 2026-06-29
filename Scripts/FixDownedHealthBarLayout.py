"""
Replace DownedHealthBar ProgressBar with Overlay(Image BG + Image Fill) for UI material support.

Run via MCP run-python-script while Unreal Editor is open.
"""

import unreal

ENTRY_PATH = "/Game/Blueprints/UI/WBP_TeammateEntry"
T = "/Game/UI/HUD"
BG_TEX = f"{T}/Textures/Common/T_HUD_Bar_BG"
BAR_SIZE = unreal.Vector2D(560.0, 60.0)


def load_tex(path: str):
    full = path if "." in path else f"{path}.{path.rsplit('/', 1)[-1]}"
    return unreal.load_object(None, full)


def make_tex_brush(tex):
    brush = unreal.SlateBrush()
    if tex:
        brush.set_editor_property("resource_object", tex)
    brush.set_editor_property("draw_as", unreal.SlateBrushDrawType.IMAGE)
    brush.set_editor_property("tiling", unreal.SlateBrushTileType.NO_TILE)
    brush.set_editor_property("image_size", BAR_SIZE)
    return brush


def find_widget(tree, name):
    found = [None]

    def visit(widget):
        if found[0]:
            return
        if widget.get_name() == name:
            found[0] = widget

    tree.for_each_widget(visit)
    return found[0]


def remove_widget(tree, name):
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

    remove_widget(tree, "DownedHealthBar")

    root = find_widget(tree, "DownedHealthBarRoot")
    if not root:
        root = tree.construct_widget(unreal.Overlay, "DownedHealthBarRoot")
        info.add_child(root)

    bg = find_widget(tree, "DownedHealthBarBG")
    fill = find_widget(tree, "DownedHealthBarFill")
    if not fill:
        fill = tree.construct_widget(unreal.Image, "DownedHealthBarFill")
    if not bg:
        bg = tree.construct_widget(unreal.Image, "DownedHealthBarBG")

    # Fill behind frame: remove and re-add in z-order (back -> front).
    if fill.get_parent() == root:
        root.remove_child(fill)
    if bg.get_parent() == root:
        root.remove_child(bg)
    root.add_child(fill)
    root.add_child(bg)

    bg_slot = bg.slot
    bg_slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
    bg_slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_FILL)

    fill_slot = fill.slot
    fill_slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_LEFT)
    fill_slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_FILL)

    info_slot = None
    for slot in info.get_slots():
        if slot.get_content() == root:
            info_slot = slot
            break
    if info_slot:
        info_slot.set_size(unreal.SlateChildSize(unreal.SlateSizeRule.FILL))
        info_slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
        info_slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
        info_slot.set_padding(unreal.Margin(0, 6, 4, 0))

    bg_tex = load_tex(BG_TEX)
    bg.set_brush(make_tex_brush(bg_tex))
    bg.set_editor_property("desired_size_override", BAR_SIZE)
    fill.set_editor_property("desired_size_override", BAR_SIZE)
    bg.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
    fill.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
    root.set_visibility(unreal.SlateVisibility.COLLAPSED)

    unreal.EditorAssetLibrary.save_loaded_asset(wbp)
    unreal.log("Downed health bar layout rebuilt (Overlay + Image fill).")


rebuild()

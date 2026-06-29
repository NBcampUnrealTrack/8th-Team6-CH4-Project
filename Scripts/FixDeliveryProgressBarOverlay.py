"""
Rebuild delivery progress overlay: fixed 420x76 SizeBox + frame on top + fill positioned in slot.

Run via MCP run-python-script while Unreal Editor is open.
"""

from __future__ import annotations

import unreal

HUD = "/Game/Blueprints/UI/WBP_GameHUD"
T = "/Game/UI/HUD"
FRAME_A = f"{T}/Textures/Delivery/T_HUD_Delivery_Station_A"
FRAME_B = f"{T}/Textures/Delivery/T_HUD_Delivery_Station_B"
MI = f"{T}/Materials/MI_HUD_DeliveryProgressFill"

BAR_W = 420.0
BAR_H = 76.0
FILL_LEFT = 72.0
FILL_TOP = 17.0
FILL_RIGHT = 102.0
FILL_BOTTOM = 16.0
FILL_W = 246.0
FILL_H = BAR_H - FILL_TOP - FILL_BOTTOM
PREVIEW_A = FILL_W * 0.30
PREVIEW_B = FILL_W * 0.70


def load_obj(path: str):
    full = path if "." in path else f"{path}.{path.rsplit('/', 1)[-1]}"
    return unreal.load_object(None, full)


def make_tex_brush(tex, size: unreal.Vector2D):
    brush = unreal.SlateBrush()
    if tex:
        brush.set_editor_property("resource_object", tex)
    brush.set_editor_property("draw_as", unreal.SlateBrushDrawType.IMAGE)
    brush.set_editor_property("tiling", unreal.SlateBrushTileType.NO_TILE)
    brush.set_editor_property("image_size", size)
    return brush


def make_mat_brush(material, size: unreal.Vector2D):
    brush = unreal.SlateBrush()
    if material:
        brush.set_editor_property("resource_object", material)
    brush.set_editor_property("draw_as", unreal.SlateBrushDrawType.IMAGE)
    brush.set_editor_property("tiling", unreal.SlateBrushTileType.NO_TILE)
    brush.set_editor_property("image_size", size)
    return brush


def find_widget(tree, name: str):
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


def remove_if_exists(tree, name: str) -> None:
    widget = find_widget(tree, name)
    if not widget:
        return
    parent = widget.get_parent()
    if parent:
        parent.remove_child(widget)
    tree.remove_widget(widget)


def configure_overlay_slot(widget, h_align, v_align, padding=None) -> None:
    slot = widget.slot
    slot.set_horizontal_alignment(h_align)
    slot.set_vertical_alignment(v_align)
    if padding is not None:
        slot.set_padding(padding)


def reparent_child(parent, child) -> None:
    if not parent or not child:
        return
    current = child.get_parent()
    if current:
        current.remove_child(child)
    parent.add_child(child)


def ensure_row(tree, row: str, frame_tex: str, preview_fill_w: float) -> None:
    line = find_widget(tree, f"DeliveryLine{row}")
    if not line:
        raise RuntimeError(f"DeliveryLine{row} not found")

    size_name = f"DeliveryProgressSize{row}"
    root_name = f"DeliveryProgressRoot{row}"
    fill_name = f"DeliveryProgressFill{row}"
    frame_name = f"DeliveryProgressBG{row}"

    remove_if_exists(tree, f"DeliveryProgressBar{row}")

    size_box = find_widget(tree, size_name)
    if not size_box:
        size_box = tree.construct_widget(unreal.SizeBox, size_name)

    root = find_widget(tree, root_name)
    if not root:
        root = tree.construct_widget(unreal.Overlay, root_name)

    fill = find_widget(tree, fill_name)
    if not fill:
        fill = tree.construct_widget(unreal.Image, fill_name)

    frame = find_widget(tree, frame_name)
    if not frame:
        frame = tree.construct_widget(unreal.Image, frame_name)

    reparent_child(root, fill)
    reparent_child(root, frame)
    reparent_child(size_box, root)
    reparent_child(line, size_box)

    # DeliveryLine slot: fixed-size bar, not stretch-to-fill.
    for slot in line.get_slots():
        if slot.get_content() == size_box:
            slot.set_size(unreal.SlateChildSize(unreal.SlateSizeRule.AUTOMATIC))
            slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_LEFT)
            slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
            slot.set_padding(unreal.Margin(0.0, 4.0, 0.0, 4.0))
            break

    size_box.set_width_override(BAR_W)
    size_box.set_height_override(BAR_H)
    size_box.clear_min_desired_width()
    size_box.clear_min_desired_height()

    fill_size = unreal.Vector2D(preview_fill_w, FILL_H)
    bar_size = unreal.Vector2D(BAR_W, BAR_H)
    fill_brush_size = unreal.Vector2D(FILL_W, FILL_H)

    frame_tex_obj = load_obj(frame_tex)
    mi = load_obj(MI)

    frame.set_brush(make_tex_brush(frame_tex_obj, bar_size))
    fill.set_brush(make_mat_brush(mi, fill_brush_size))

    frame.set_editor_property("desired_size_override", bar_size)
    fill.set_editor_property("desired_size_override", fill_size)

    frame.set_render_transform_pivot(unreal.Vector2D(0.0, 0.0))
    fill.set_render_transform_pivot(unreal.Vector2D(0.0, 0.0))
    frame.set_render_translation(unreal.Vector2D(0.0, 0.0))
    fill.set_render_translation(unreal.Vector2D(FILL_LEFT, FILL_TOP))

    configure_overlay_slot(
        frame,
        unreal.HorizontalAlignment.H_ALIGN_FILL,
        unreal.VerticalAlignment.V_ALIGN_FILL,
    )
    configure_overlay_slot(
        fill,
        unreal.HorizontalAlignment.H_ALIGN_LEFT,
        unreal.VerticalAlignment.V_ALIGN_TOP,
    )

    root.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
    frame.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
    fill.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
    frame.set_color_and_opacity(unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
    fill.set_color_and_opacity(unreal.LinearColor(1.0, 1.0, 1.0, 1.0))

    # Fill behind frame.
    root.remove_child(fill)
    root.remove_child(frame)
    root.add_child(fill)
    root.add_child(frame)

    icon = find_widget(tree, f"DeliveryIcon{row}")
    if icon:
        icon.set_visibility(unreal.SlateVisibility.COLLAPSED)

    unreal.log(f"[DeliveryOverlay] {row}: SizeBox {BAR_W}x{BAR_H}, fill at ({FILL_LEFT},{FILL_TOP}) size {fill_size.x}x{fill_size.y}")


def main() -> None:
    wbp = unreal.load_asset(HUD)
    if not wbp:
        raise RuntimeError(f"Missing {HUD}")

    tree = wbp.get_editor_property("WidgetTree")
    ensure_row(tree, "A", FRAME_A, PREVIEW_A)
    ensure_row(tree, "B", FRAME_B, PREVIEW_B)

    unreal.KismetEditorUtilities.compile_blueprint(wbp)
    unreal.EditorAssetLibrary.save_loaded_asset(wbp)
    unreal.log("Delivery progress overlay layout rebuilt.")


main()

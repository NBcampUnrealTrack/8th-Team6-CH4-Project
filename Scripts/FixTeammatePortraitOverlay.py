"""
Rebuild WBP_TeammateEntry portrait area: Overlay(frame + icon), no panel BG dependency.

Run via MCP run-python-script or Unreal editor Python console.
"""

import unreal

ENTRY_PATH = "/Game/Blueprints/UI/WBP_TeammateEntry"
T = "/Game/UI/HUD"
FRAME_TEX = f"{T}/Textures/Teammate/T_HUD_Portrait_Slot"
ICON_TEX = f"{T}/Textures/Teammate/T_HUD_Portrait_Placeholder"
FRAME_SIZE = unreal.Vector2D(80.0, 80.0)
ICON_SIZE = unreal.Vector2D(64.0, 64.0)
ICON_PAD = 8.0


def load_tex(path):
    if not path:
        return None
    full = path if "." in path else f"{path}.{path.rsplit('/', 1)[-1]}"
    return unreal.load_object(None, full)


def make_brush(tex):
    brush = unreal.SlateBrush()
    if tex:
        brush.set_editor_property("resource_object", tex)
    brush.set_editor_property("draw_as", unreal.SlateBrushDrawType.IMAGE)
    brush.set_editor_property("tiling", unreal.SlateBrushTileType.NO_TILE)
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


def remove_if_exists(tree, parent, name):
    widget = find_widget(tree, name)
    if widget and parent:
        parent.remove_child(widget)
        tree.remove_widget(widget)
    elif widget:
        tree.remove_widget(widget)


def rebuild():
    wbp = unreal.load_asset(ENTRY_PATH)
    if not wbp:
        raise RuntimeError(f"Missing {ENTRY_PATH}")

    tree = wbp.get_editor_property("WidgetTree")
    root = find_widget(tree, "RootHBox")
    if not root:
        raise RuntimeError("RootHBox not found")

    portrait = find_widget(tree, "PortraitImage")
    if not portrait:
        portrait = tree.construct_widget(unreal.Image, "PortraitImage")

    # Drop legacy siblings / collapsed frame on hbox
    for legacy in ("PortraitFrame", "PortraitOverlay"):
        legacy_w = find_widget(tree, legacy)
        if legacy_w and legacy_w.get_parent():
            legacy_w.get_parent().remove_child(legacy_w)
        if legacy_w:
            tree.remove_widget(legacy_w)

    if portrait.get_parent() is root:
        root.remove_child(portrait)

    info = find_widget(tree, "InfoVBox")
    if info and info.get_parent() is root:
        root.remove_child(info)

    overlay = tree.construct_widget(unreal.Overlay, "PortraitOverlay")
    frame = tree.construct_widget(unreal.Image, "PortraitFrame")

    root.add_child(overlay)
    if info:
        root.add_child(info)
    overlay.add_child(frame)
    overlay.add_child(portrait)

    # PortraitOverlay = icon wrap only (fixed 80x80). InfoVBox stays outside.
    overlay_slot = None
    for slot in root.get_slots():
        if slot.get_content() == overlay:
            overlay_slot = slot
            break
    if overlay_slot:
        overlay_slot.set_size(unreal.SlateChildSize(unreal.SlateSizeRule.AUTOMATIC))
        overlay_slot.set_padding(unreal.Margin(0, 0, 10, 0))
        overlay_slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_LEFT)
        overlay_slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)

    frame_slot = frame.slot
    frame_slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
    frame_slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_FILL)

    icon_slot = portrait.slot
    icon_slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER)
    icon_slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
    icon_slot.set_padding(unreal.Margin(ICON_PAD, ICON_PAD, ICON_PAD, ICON_PAD))

    frame_tex = load_tex(FRAME_TEX)
    icon_tex = load_tex(ICON_TEX)
    frame.set_brush(make_brush(frame_tex))
    portrait.set_brush(make_brush(icon_tex))
    frame.set_editor_property("desired_size_override", FRAME_SIZE)
    portrait.set_editor_property("desired_size_override", ICON_SIZE)
    frame.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
    portrait.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)

    unreal.EditorAssetLibrary.save_loaded_asset(wbp)
    unreal.log("Teammate portrait overlay layout applied.")


rebuild()

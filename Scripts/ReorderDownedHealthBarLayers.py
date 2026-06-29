"""
Put DownedHealthBarFill behind DownedHealthBarBG in the Overlay z-order.

Overlay draws children in add order: first = back, last = front.

Run in Unreal Editor:
  exec(open(r'E:/Unreal/git/SpaCh4/Scripts/ReorderDownedHealthBarLayers.py', encoding='utf-8').read())
"""

import unreal

ENTRY_PATH = "/Game/Blueprints/UI/WBP_TeammateEntry"


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


def reorder():
    wbp = unreal.load_asset(ENTRY_PATH)
    if not wbp:
        raise RuntimeError(f"Missing {ENTRY_PATH}")

    tree = wbp.get_editor_property("WidgetTree")
    root = find_widget(tree, "DownedHealthBarRoot")
    bg = find_widget(tree, "DownedHealthBarBG")
    fill = find_widget(tree, "DownedHealthBarFill")
    if not root or not bg or not fill:
        raise RuntimeError("DownedHealthBarRoot/BG/Fill not found")

    root.remove_child(fill)
    root.remove_child(bg)
    root.add_child(fill)
    root.add_child(bg)

    fill_slot = fill.slot
    fill_slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_LEFT)
    fill_slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_FILL)

    bg_slot = bg.slot
    bg_slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL)
    bg_slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_FILL)

    unreal.KismetEditorUtilities.compile_blueprint(wbp)
    unreal.EditorAssetLibrary.save_loaded_asset(wbp)
    unreal.log("Downed health bar layer order: Fill (back) -> BG frame (front).")


reorder()

import unreal

ENTRY = "/Game/Blueprints/UI/WBP_TeammateEntry"
HUD = "/Game/Blueprints/UI/WBP_GameHUD"
BP_HUD = "/Game/Blueprints/UI/BP_GameHUD"
BP_GM = "/Game/Blueprints/BP_MatchGameMode"

UI_ROOT = "/Game/Blueprints/UI"
BP_ROOT = "/Game/Blueprints"


def log(msg):
    unreal.log(f"[BuildHUDWidgets] {msg}")


def ensure_dir(path):
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.EditorAssetLibrary.make_directory(path)


def delete_if_exists(path):
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
        log(f"deleted {path}")


def create_widget_bp(name, package, parent_class_path):
    delete_if_exists(f"{package}/{name}")
    factory = unreal.WidgetBlueprintFactory()
    factory.set_editor_property("parent_class", unreal.load_class(None, parent_class_path))
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, package, None, factory)
    if not asset:
        raise RuntimeError(f"Failed to create widget bp {name}")
    return asset


def create_actor_bp(name, package, parent_class_path):
    delete_if_exists(f"{package}/{name}")
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", unreal.load_class(None, parent_class_path))
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, package, unreal.Blueprint, factory)
    if not asset:
        raise RuntimeError(f"Failed to create blueprint {name}")
    return asset


def get_tree(widget_bp):
    tree = widget_bp.get_editor_property("WidgetTree")
    if not tree:
        raise RuntimeError(f"No WidgetTree on {widget_bp.get_name()}")
    return tree


def compile_save(asset):
    unreal.KismetEditorUtilities.compile_blueprint(asset)
    unreal.EditorAssetLibrary.save_loaded_asset(asset)


def build_teammate_entry():
    wbp = create_widget_bp("WBP_TeammateEntry", UI_ROOT, "/Script/SpaCh4.TeammateEntryWidget")
    tree = get_tree(wbp)
    root = tree.construct_widget(unreal.HorizontalBox, "RootHBox")
    tree.root_widget = root

    portrait = tree.construct_widget(unreal.Image, "PortraitImage")
    portrait.set_editor_property("desired_size_override", unreal.Vector2D(64, 64))

    info = tree.construct_widget(unreal.VerticalBox, "InfoVBox")
    nickname = tree.construct_widget(unreal.TextBlock, "NicknameText")
    nickname.set_editor_property("text", unreal.Text.from_string("Player"))
    cage = tree.construct_widget(unreal.TextBlock, "CageStackText")
    cage.set_editor_property("text", unreal.Text.from_string("0/2"))
    downed = tree.construct_widget(unreal.ProgressBar, "DownedHealthBar")
    downed.set_visibility(unreal.SlateVisibility.COLLAPSED)

    root.add_child(portrait)
    root.add_child(info)
    info.add_child(nickname)
    info.add_child(cage)
    info.add_child(downed)

    compile_save(wbp)
    return wbp


def add_canvas(canvas, widget, pos, size):
    slot = canvas.add_child_to_canvas(widget)
    slot.set_anchors(unreal.Anchors(0, 0, 0, 0))
    slot.set_alignment(unreal.Vector2D(0, 0))
    slot.set_position(pos)
    slot.set_size(size)
    slot.set_auto_size(False)


def build_game_hud(entry_wbp):
    wbp = create_widget_bp("WBP_GameHUD", UI_ROOT, "/Script/SpaCh4.GameHUDWidget")
    tree = get_tree(wbp)
    entry_class = entry_wbp.generated_class()

    canvas = tree.construct_widget(unreal.CanvasPanel, "RootCanvas")
    tree.root_widget = canvas

    teammate_panel = tree.construct_widget(unreal.VerticalBox, "TeammatePanel")
    for i in range(3):
        teammate_panel.add_child(tree.construct_widget(entry_class, f"TeammateEntries_{i}"))
    add_canvas(canvas, teammate_panel, unreal.Vector2D(48, 48), unreal.Vector2D(360, 240))

    delivery_panel = tree.construct_widget(unreal.VerticalBox, "DeliveryPanel")
    for row_name, value_name, progress_name in [
        ("DeliveryRowA", "DeliveryValueA", "DeliveryProgressA"),
        ("DeliveryRowB", "DeliveryValueB", "DeliveryProgressB"),
    ]:
        row = tree.construct_widget(unreal.HorizontalBox, row_name)
        row.add_child(tree.construct_widget(unreal.Image, f"{row_name}_Icon"))
        row.add_child(tree.construct_widget(unreal.TextBlock, value_name))
        row.add_child(tree.construct_widget(unreal.ProgressBar, progress_name))
        delivery_panel.add_child(row)
    add_canvas(canvas, delivery_panel, unreal.Vector2D(48, -220), unreal.Vector2D(360, 160))

    inventory = tree.construct_widget(unreal.HorizontalBox, "InventoryPanel")
    for i in range(4):
        slot = tree.construct_widget(unreal.Image, f"InventorySlots_{i}")
        slot.set_editor_property("desired_size_override", unreal.Vector2D(72, 72))
        inventory.add_child(slot)
    inv_slot = canvas.add_child_to_canvas(inventory)
    inv_slot.set_anchors(unreal.Anchors(0.5, 1.0, 0.5, 1.0))
    inv_slot.set_alignment(unreal.Vector2D(0.5, 1.0))
    inv_slot.set_position(unreal.Vector2D(-160, -96))
    inv_slot.set_size(unreal.Vector2D(320, 80))

    perk = tree.construct_widget(unreal.HorizontalBox, "PerkPanel")
    for i in range(2):
        slot = tree.construct_widget(unreal.Image, f"PerkSlots_{i}")
        slot.set_editor_property("desired_size_override", unreal.Vector2D(64, 64))
        perk.add_child(slot)
    perk_slot = canvas.add_child_to_canvas(perk)
    perk_slot.set_anchors(unreal.Anchors(1.0, 1.0, 1.0, 1.0))
    perk_slot.set_alignment(unreal.Vector2D(1.0, 1.0))
    perk_slot.set_position(unreal.Vector2D(-176, -96))
    perk_slot.set_size(unreal.Vector2D(144, 72))

    compile_save(wbp)
    return wbp


def configure_defaults():
    bp_hud = create_actor_bp("BP_GameHUD", UI_ROOT, "/Script/SpaCh4.GameHUD")
    bp_gm = create_actor_bp("BP_MatchGameMode", BP_ROOT, "/Script/SpaCh4.MatchGameMode")

    hud_cdo = unreal.get_default_object(bp_hud.generated_class())
    hud_cdo.set_editor_property(
        "game_hud_widget_class",
        unreal.load_class(None, "/Game/Blueprints/UI/WBP_GameHUD.WBP_GameHUD_C"),
    )

    gm_cdo = unreal.get_default_object(bp_gm.generated_class())
    gm_cdo.set_editor_property(
        "hud_class",
        unreal.load_class(None, "/Game/Blueprints/UI/BP_GameHUD.BP_GameHUD_C"),
    )

    compile_save(bp_hud)
    compile_save(bp_gm)


def main():
    ensure_dir(UI_ROOT)
    ensure_dir(BP_ROOT)
    entry = build_teammate_entry()
    build_game_hud(entry)
    configure_defaults()
    log("HUD build complete")


main()

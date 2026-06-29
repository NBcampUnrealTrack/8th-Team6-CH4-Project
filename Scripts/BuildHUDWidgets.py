"""
SpaCh4 HUD WBP 자동 생성 스크립트.
에디터에서 실행: Tools > Execute Python Script
또는: UnrealEditor.exe SpaCh4.uproject -ExecutePythonScript=Scripts/BuildHUDWidgets.py
"""

import os
import unreal

PROJECT_ROOT = unreal.Paths.convert_relative_path_to_full(
    unreal.Paths.project_dir()
)
HUD_SOURCE_ROOT = os.path.join(PROJECT_ROOT, "Content", "UI", "HUD")
UI_GAME_PATH = "/Game/Blueprints/UI"
TEXTURE_GAME_ROOT = "/Game/UI/HUD"

TEXTURE_IMPORTS = [
    ("Textures/Common/T_HUD_Panel_BG.png", f"{TEXTURE_GAME_ROOT}/Textures/Common", "T_HUD_Panel_BG"),
    ("Textures/Common/T_HUD_Bar_BG.png", f"{TEXTURE_GAME_ROOT}/Textures/Common", "T_HUD_Bar_BG"),
    ("Textures/Common/T_HUD_Bar_Fill.png", f"{TEXTURE_GAME_ROOT}/Textures/Common", "T_HUD_Bar_Fill"),
    ("Textures/Teammate/T_HUD_Portrait_Frame.png", f"{TEXTURE_GAME_ROOT}/Textures/Teammate", "T_HUD_Portrait_Frame"),
    ("Textures/Teammate/T_HUD_Portrait_BG.png", f"{TEXTURE_GAME_ROOT}/Textures/Teammate", "T_HUD_Portrait_BG"),
    ("Textures/Teammate/T_HUD_Portrait_Placeholder.png", f"{TEXTURE_GAME_ROOT}/Textures/Teammate", "T_HUD_Portrait_Placeholder"),
    ("Textures/Teammate/T_HUD_Bar_Fill_Downed.png", f"{TEXTURE_GAME_ROOT}/Textures/Teammate", "T_HUD_Bar_Fill_Downed"),
    ("Icons/CageStack/T_HUD_CageStack_Filled.png", f"{TEXTURE_GAME_ROOT}/Icons/CageStack", "T_HUD_CageStack_Filled"),
    ("Icons/CageStack/T_HUD_CageStack_Empty.png", f"{TEXTURE_GAME_ROOT}/Icons/CageStack", "T_HUD_CageStack_Empty"),
    ("Textures/Delivery/T_HUD_Delivery_Slot.png", f"{TEXTURE_GAME_ROOT}/Textures/Delivery", "T_HUD_Delivery_Slot"),
    ("Textures/Delivery/T_HUD_Delivery_Progress_BG.png", f"{TEXTURE_GAME_ROOT}/Textures/Delivery", "T_HUD_Delivery_Progress_BG"),
    ("Textures/Delivery/T_HUD_Delivery_Progress_Fill.png", f"{TEXTURE_GAME_ROOT}/Textures/Delivery", "T_HUD_Delivery_Progress_Fill"),
    ("Textures/Inventory/T_HUD_Inventory_Slot_Empty.png", f"{TEXTURE_GAME_ROOT}/Textures/Inventory", "T_HUD_Inventory_Slot_Empty"),
    ("Textures/Perk/T_HUD_Perk_Slot_Empty.png", f"{TEXTURE_GAME_ROOT}/Textures/Perk", "T_HUD_Perk_Slot_Empty"),
]


def log(msg: str) -> None:
    unreal.log(f"[BuildHUDWidgets] {msg}")


def ensure_directory(path: str) -> None:
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.EditorAssetLibrary.make_directory(path)


def set_ui_texture_settings(texture) -> None:
    for attr in ("TC_USERINTERFACE2D", "TC_EDITOR_ICON", "TC_DEFAULT"):
        if hasattr(unreal.TextureCompressionSettings, attr):
            texture.set_editor_property(
                "compression_settings",
                getattr(unreal.TextureCompressionSettings, attr),
            )
            break

    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)


def import_textures() -> dict:
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    imported = {}

    for rel_path, dest_path, asset_name in TEXTURE_IMPORTS:
        ensure_directory(dest_path)
        source_file = os.path.join(HUD_SOURCE_ROOT, rel_path.replace("/", os.sep))
        dest_asset = f"{dest_path}/{asset_name}"

        if unreal.EditorAssetLibrary.does_asset_exist(dest_asset):
            imported[asset_name] = unreal.load_asset(dest_asset)
            continue

        if not os.path.isfile(source_file):
            log(f"SKIP missing source: {source_file}")
            continue

        task = unreal.AssetImportTask()
        task.set_editor_property("filename", source_file)
        task.set_editor_property("destination_path", dest_path)
        task.set_editor_property("destination_name", asset_name)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("automated", True)
        task.set_editor_property("save", True)
        asset_tools.import_asset_tasks([task])

        texture = unreal.load_asset(dest_asset)
        if texture:
            set_ui_texture_settings(texture)
            unreal.EditorAssetLibrary.save_loaded_asset(texture)
            imported[asset_name] = texture
            log(f"Imported {dest_asset}")

    return imported


def make_brush(texture) -> unreal.SlateBrush:
    brush = unreal.SlateBrush()
    brush.set_editor_property("image_type", unreal.SlateBrushImageType.FULL_COLOR)
    brush.set_editor_property("draw_as", unreal.SlateBrushDrawType.IMAGE)
    if texture:
        brush.set_editor_property("resource_object", texture)
    return brush


def load_class(path: str):
    cls = unreal.load_class(None, path)
    if not cls:
        raise RuntimeError(f"Failed to load class: {path}")
    return cls


def create_widget_blueprint(name: str, parent_class_path: str, force_rebuild: bool = False):
    package_path = UI_GAME_PATH
    asset_path = f"{package_path}/{name}"
    ensure_directory(package_path)

    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        log(f"Reuse {asset_path}")
        existing = unreal.load_asset(asset_path)
        if force_rebuild:
            unreal.EditorAssetLibrary.delete_asset(asset_path)
        else:
            return existing

    factory = unreal.WidgetBlueprintFactory()
    factory.set_editor_property("parent_class", load_class(parent_class_path))

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    widget_bp = asset_tools.create_asset(name, package_path, None, factory)
    if not widget_bp:
        raise RuntimeError(f"Failed to create widget blueprint: {name}")

    log(f"Created {asset_path}")
    return widget_bp


def compile_and_save(asset) -> None:
    unreal.KismetEditorUtilities.compile_blueprint(asset)
    unreal.EditorAssetLibrary.save_loaded_asset(asset)


def get_widget_tree(widget_bp):
    tree = widget_bp.get_editor_property("WidgetTree")
    if tree:
        return tree
    raise RuntimeError("WidgetBlueprint has no WidgetTree. Open the project in editor once and retry.")


def build_teammate_entry(textures: dict):
    widget_bp = create_widget_blueprint(
        "WBP_TeammateEntry",
        "/Script/SpaCh4.TeammateEntryWidget",
        force_rebuild=True,
    )
    tree = get_widget_tree(widget_bp)

    root = tree.construct_widget(unreal.HorizontalBox, "RootHBox")
    tree.root_widget = root

    portrait = tree.construct_widget(unreal.Image, "PortraitImage")
    portrait.set_editor_property("desired_size_override", unreal.Vector2D(64.0, 64.0))
    portrait.set_brush(make_brush(textures.get("T_HUD_Portrait_Frame")))

    info_box = tree.construct_widget(unreal.VerticalBox, "InfoVBox")

    nickname = tree.construct_widget(unreal.TextBlock, "NicknameText")
    nickname.set_editor_property("text", unreal.Text.from_string("Player"))
    nickname.set_editor_property("color_and_opacity", unreal.SlateColor(unreal.LinearColor(0.9, 0.9, 0.9, 1.0)))

    cage_stack = tree.construct_widget(unreal.TextBlock, "CageStackText")
    cage_stack.set_editor_property("text", unreal.Text.from_string("0/2"))
    cage_stack.set_editor_property("color_and_opacity", unreal.SlateColor(unreal.LinearColor(0.7, 0.7, 0.75, 1.0)))

    downed_bar = tree.construct_widget(unreal.ProgressBar, "DownedHealthBar")
    downed_bar.set_editor_property("percent", 0.65)
    downed_bar.set_editor_property("fill_color_and_opacity", unreal.LinearColor(0.77, 0.29, 0.29, 1.0))
    downed_bar.set_visibility(unreal.SlateVisibility.COLLAPSED)

    root.add_child(portrait)
    root.add_child(info_box)
    info_box.add_child(nickname)
    info_box.add_child(cage_stack)
    info_box.add_child(downed_bar)

    compile_and_save(widget_bp)
    return widget_bp


def add_canvas_child(canvas, widget, anchors, alignment, position, size):
    slot = canvas.add_child_to_canvas(widget)
    slot.set_anchors(anchors)
    slot.set_alignment(alignment)
    slot.set_position(position)
    slot.set_size(size)
    slot.set_auto_size(False)
    return slot


def build_game_hud(textures: dict, entry_widget_bp):
    widget_bp = create_widget_blueprint(
        "WBP_GameHUD",
        "/Script/SpaCh4.GameHUDWidget",
        force_rebuild=True,
    )
    tree = get_widget_tree(widget_bp)
    entry_class = entry_widget_bp.generated_class() if callable(entry_widget_bp.generated_class) else entry_widget_bp.generated_class

    canvas = tree.construct_widget(unreal.CanvasPanel, "RootCanvas")
    tree.root_widget = canvas

    # 좌상단: 팀원 상태
    teammate_panel = tree.construct_widget(unreal.VerticalBox, "TeammatePanel")
    for index in range(3):
        entry = tree.construct_widget(entry_class, f"TeammateEntries_{index}")
        teammate_panel.add_child(entry)

    top_left = unreal.Anchors()
    top_left.minimum = unreal.Vector2D(0.0, 0.0)
    top_left.maximum = unreal.Vector2D(0.0, 0.0)
    add_canvas_child(
        canvas,
        teammate_panel,
        top_left,
        unreal.Vector2D(0.0, 0.0),
        unreal.Vector2D(48.0, 48.0),
        unreal.Vector2D(360.0, 240.0),
    )

    # 좌하단: 납품 A/B
    delivery_panel = tree.construct_widget(unreal.VerticalBox, "DeliveryPanel")
    for station_name, progress_name, value_name in [
        ("DeliveryRowA", "DeliveryProgressA", "DeliveryValueA"),
        ("DeliveryRowB", "DeliveryProgressB", "DeliveryValueB"),
    ]:
        row = tree.construct_widget(unreal.HorizontalBox, station_name)
        slot_image = tree.construct_widget(unreal.Image, f"{station_name}_Icon")
        slot_image.set_editor_property("desired_size_override", unreal.Vector2D(56.0, 56.0))
        slot_image.set_brush(make_brush(textures.get("T_HUD_Delivery_Slot")))

        value_text = tree.construct_widget(unreal.TextBlock, value_name)
        value_text.set_editor_property("text", unreal.Text.from_string("0/200"))

        progress = tree.construct_widget(unreal.ProgressBar, progress_name)
        progress.set_editor_property("percent", 0.0)
        progress.set_editor_property("fill_color_and_opacity", unreal.LinearColor(0.83, 0.66, 0.29, 1.0))

        row.add_child(slot_image)
        row.add_child(value_text)
        row.add_child(progress)
        delivery_panel.add_child(row)

    bottom_left = unreal.Anchors()
    bottom_left.minimum = unreal.Vector2D(0.0, 1.0)
    bottom_left.maximum = unreal.Vector2D(0.0, 1.0)
    add_canvas_child(
        canvas,
        delivery_panel,
        bottom_left,
        unreal.Vector2D(0.0, 1.0),
        unreal.Vector2D(48.0, -220.0),
        unreal.Vector2D(360.0, 160.0),
    )

    # 하단 중앙: 인벤토리 4칸
    inventory_panel = tree.construct_widget(unreal.HorizontalBox, "InventoryPanel")
    for index in range(4):
        slot = tree.construct_widget(unreal.Image, f"InventorySlots_{index}")
        slot.set_editor_property("desired_size_override", unreal.Vector2D(72.0, 72.0))
        slot.set_brush(make_brush(textures.get("T_HUD_Inventory_Slot_Empty")))
        inventory_panel.add_child(slot)

    bottom_center = unreal.Anchors()
    bottom_center.minimum = unreal.Vector2D(0.5, 1.0)
    bottom_center.maximum = unreal.Vector2D(0.5, 1.0)
    add_canvas_child(
        canvas,
        inventory_panel,
        bottom_center,
        unreal.Vector2D(0.5, 1.0),
        unreal.Vector2D(-160.0, -96.0),
        unreal.Vector2D(320.0, 80.0),
    )

    # 우하단: 퍽 2칸
    perk_panel = tree.construct_widget(unreal.HorizontalBox, "PerkPanel")
    for index in range(2):
        slot = tree.construct_widget(unreal.Image, f"PerkSlots_{index}")
        slot.set_editor_property("desired_size_override", unreal.Vector2D(64.0, 64.0))
        slot.set_brush(make_brush(textures.get("T_HUD_Perk_Slot_Empty")))
        perk_panel.add_child(slot)

    bottom_right = unreal.Anchors()
    bottom_right.minimum = unreal.Vector2D(1.0, 1.0)
    bottom_right.maximum = unreal.Vector2D(1.0, 1.0)
    add_canvas_child(
        canvas,
        perk_panel,
        bottom_right,
        unreal.Vector2D(1.0, 1.0),
        unreal.Vector2D(-176.0, -96.0),
        unreal.Vector2D(144.0, 72.0),
    )

    compile_and_save(widget_bp)
    return widget_bp


def create_bp_game_hud(game_hud_widget_bp):
    package_path = UI_GAME_PATH
    asset_path = f"{package_path}/BP_GameHUD"
    ensure_directory(package_path)

    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        bp = unreal.load_asset(asset_path)
    else:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", load_class("/Script/SpaCh4.GameHUD"))
        asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
        bp = asset_tools.create_asset("BP_GameHUD", package_path, unreal.Blueprint, factory)
        if not bp:
            raise RuntimeError("Failed to create BP_GameHUD")

    cdo = unreal.get_default_object(bp.generated_class())
    cdo.set_editor_property(
        "game_hud_widget_class",
        game_hud_widget_bp.generated_class() if callable(game_hud_widget_bp.generated_class) else game_hud_widget_bp.generated_class,
    )
    compile_and_save(bp)
    log(f"Configured {asset_path}")
    return bp


def create_bp_match_game_mode(hud_bp):
    package_path = "/Game/Blueprints"
    asset_path = f"{package_path}/BP_MatchGameMode"
    ensure_directory(package_path)

    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        bp = unreal.load_asset(asset_path)
    else:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", load_class("/Script/SpaCh4.MatchGameMode"))
        asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
        bp = asset_tools.create_asset("BP_MatchGameMode", package_path, unreal.Blueprint, factory)
        if not bp:
            raise RuntimeError("Failed to create BP_MatchGameMode")

    cdo = unreal.get_default_object(bp.generated_class())
    hud_class = hud_bp.generated_class() if callable(hud_bp.generated_class) else hud_bp.generated_class
    cdo.set_editor_property("hud_class", hud_class)
    compile_and_save(bp)
    log(f"Configured {asset_path}")
    return bp


def main():
    log("Start HUD build")
    textures = import_textures()
    entry_bp = build_teammate_entry(textures)
    game_hud_bp = build_game_hud(textures, entry_bp)
    hud_bp = create_bp_game_hud(game_hud_bp)
    create_bp_match_game_mode(hud_bp)
    log("HUD build complete")


if __name__ == "__main__":
    main()

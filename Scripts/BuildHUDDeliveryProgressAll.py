"""
Build delivery progress shimmer material + MI without import-assets.

Uses existing delivery textures already in Content/.
Safe to run while debugging — does not call import-assets MCP.

Run in Unreal Editor:
  exec(open(r'E:/Unreal/git/SpaCh4/Scripts/BuildHUDDeliveryProgressAll.py', encoding='utf-8').read())
"""

from __future__ import annotations

from pathlib import Path

import unreal

MAT_PATH = "/Game/UI/HUD/Materials/M_HUD_HealthBarFill"
MI_PATH = "/Game/UI/HUD/Materials/MI_HUD_DeliveryProgressFill"
FILL_CANDIDATES = (
    "/Game/UI/HUD/Textures/Delivery/T_HUD_Delivery_Fill_Smooth",
    "/Game/UI/HUD/Textures/Delivery/T_HUD_Delivery_Fill",
    "/Game/UI/HUD/Textures/Delivery/T_HUD_Delivery_Progress_10",
    "/Game/UI/HUD/Textures/Common/T_HUD_Bar_Fill",
)
SMOOTH_PNG = Path(r"E:/Unreal/git/SpaCh4/Content/UI/HUD/Textures/Delivery/T_HUD_Delivery_Fill_Smooth.png")


def _load_asset(path: str):
    name = path.rsplit("/", 1)[-1]
    obj_path = f"{path}.{name}"
    asset = unreal.load_asset(obj_path)
    if asset:
        return asset
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.EditorAssetLibrary.load_asset(path)
    return None


def _load_fill_texture():
    for path in FILL_CANDIDATES:
        tex = _load_asset(path)
        if tex:
            tex.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
            tex.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
            unreal.EditorAssetLibrary.save_loaded_asset(tex)
            unreal.log(f"Delivery fill texture: {path}")
            return tex
    raise RuntimeError(f"No delivery fill texture found. Tried: {FILL_CANDIDATES}")


def _ensure_parent_material(fill_tex) -> unreal.Material:
    mat = _load_asset(MAT_PATH)
    if mat:
        return mat

    # Reuse health bar builder if available.
    builder = r"E:/Unreal/git/SpaCh4/Scripts/BuildHUDHealthBarMaterial.py"
    with open(builder, encoding="utf-8") as f:
        code = f.read()
    # Temporarily point health bar fill to delivery texture for first-time create.
    code = code.replace(
        'FILL_TEX = "/Game/UI/HUD/Textures/Teammate/T_HUD_Bar_Fill_Downed"',
        f'FILL_TEX = "{fill_tex.get_path_name().split(".")[0]}"',
    )
    exec(compile(code, builder, "exec"), {"__name__": "__main__"})
    mat = _load_asset(MAT_PATH)
    if not mat:
        raise RuntimeError(f"Failed to create parent material: {MAT_PATH}")
    return mat


def _ensure_mi(parent: unreal.Material, fill_tex) -> unreal.MaterialInstanceConstant:
    mi = _load_asset(MI_PATH)
    if not mi:
        tools = unreal.AssetToolsHelpers.get_asset_tools()
        factory = unreal.MaterialInstanceConstantFactoryNew()
        mi = tools.create_asset(
            "MI_HUD_DeliveryProgressFill",
            "/Game/UI/HUD/Materials",
            unreal.MaterialInstanceConstant,
            factory,
        )
        if not mi:
            raise RuntimeError("Failed to create MI_HUD_DeliveryProgressFill")

    unreal.MaterialEditingLibrary.set_material_instance_parent(mi, parent)
    unreal.MaterialEditingLibrary.set_material_instance_texture_parameter_value(
        mi, "FillTexture", fill_tex
    )
    unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(
        mi, "ShimmerStrength", 0.18
    )
    unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(
        mi, "EmissiveBoost", 1.45
    )
    unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(
        mi, "PanSpeed", 3.5
    )
    unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(
        mi, "WaveAmplitude", 0.018
    )
    unreal.EditorAssetLibrary.save_loaded_asset(mi)
    return mi


def _reimport_smooth_png() -> None:
    if not SMOOTH_PNG.exists():
        return
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(SMOOTH_PNG).replace("\\", "/"))
    task.set_editor_property("destination_path", "/Game/UI/HUD/Textures/Delivery")
    task.set_editor_property("destination_name", "T_HUD_Delivery_Fill_Smooth")
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    tools.import_asset_tasks([task])


def main() -> None:
    _reimport_smooth_png()
    fill_tex = _load_fill_texture()
    parent = _ensure_parent_material(fill_tex)
    mi = _ensure_mi(parent, fill_tex)
    unreal.log(f"Delivery progress shimmer ready: {mi.get_path_name()}")


if __name__ == "__main__":
    main()

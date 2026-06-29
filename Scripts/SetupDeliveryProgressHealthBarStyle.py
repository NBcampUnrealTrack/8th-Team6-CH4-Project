"""
Import green delivery fill texture and configure MI like downed health bar.

Run via MCP run-python-script or Unreal Python console.
"""

from __future__ import annotations

import os
import unreal

PNG = r"E:/Unreal/git/SpaCh4/Content/UI/HUD/Textures/Delivery/T_HUD_Bar_Fill_Delivery.png"
TEX_PATH = "/Game/UI/HUD/Textures/Delivery/T_HUD_Bar_Fill_Delivery"
MI_PATH = "/Game/UI/HUD/Materials/MI_HUD_DeliveryProgressFill"
PARENT = "/Game/UI/HUD/Materials/M_HUD_HealthBarFill"
DOWNED_MI = "/Game/UI/HUD/Materials/MI_HUD_HealthBarFill_Downed"


def _ensure_folder(path: str) -> None:
    folder = "/".join(path.split("/")[:-1])
    if not unreal.EditorAssetLibrary.does_directory_exist(folder):
        unreal.EditorAssetLibrary.make_directory(folder)


def _import_or_update_texture() -> unreal.Texture2D:
    _ensure_folder(TEX_PATH)
    if not os.path.isfile(PNG):
        raise RuntimeError(f"Missing PNG: {PNG}")

    if unreal.EditorAssetLibrary.does_asset_exist(TEX_PATH):
        tex = unreal.EditorAssetLibrary.load_asset(TEX_PATH)
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", PNG)
        task.set_editor_property("destination_path", "/Game/UI/HUD/Textures/Delivery")
        task.set_editor_property("destination_name", "T_HUD_Bar_Fill_Delivery")
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("automated", True)
        task.set_editor_property("save", True)
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        tex = unreal.EditorAssetLibrary.load_asset(TEX_PATH)
    else:
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", PNG)
        task.set_editor_property("destination_path", "/Game/UI/HUD/Textures/Delivery")
        task.set_editor_property("destination_name", "T_HUD_Bar_Fill_Delivery")
        task.set_editor_property("replace_existing", False)
        task.set_editor_property("automated", True)
        task.set_editor_property("save", True)
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        tex = unreal.EditorAssetLibrary.load_asset(TEX_PATH)

    if not tex:
        raise RuntimeError("Failed to import delivery fill texture")

    tex.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
    tex.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
    unreal.EditorAssetLibrary.save_loaded_asset(tex)
    return tex


def _read_scalar_from_mi(mi: unreal.MaterialInstanceConstant, name: str, default: float) -> float:
    if not mi:
        return default
    try:
        return float(unreal.MaterialEditingLibrary.get_material_instance_scalar_parameter_value(mi, name))
    except Exception:
        return default


def _setup_mi(tex: unreal.Texture2D) -> unreal.MaterialInstanceConstant:
    parent = unreal.EditorAssetLibrary.load_asset(PARENT)
    downed_mi = unreal.EditorAssetLibrary.load_asset(DOWNED_MI)
    if not parent or not tex:
        raise RuntimeError("Missing parent material or fill texture")

    shimmer = _read_scalar_from_mi(downed_mi, "ShimmerStrength", 0.14)
    emissive = _read_scalar_from_mi(downed_mi, "EmissiveBoost", 1.15)

    _ensure_folder(MI_PATH)
    if unreal.EditorAssetLibrary.does_asset_exist(MI_PATH):
        mi = unreal.EditorAssetLibrary.load_asset(MI_PATH)
    else:
        mi = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "MI_HUD_DeliveryProgressFill",
            "/Game/UI/HUD/Materials",
            unreal.MaterialInstanceConstant,
            unreal.MaterialInstanceConstantFactoryNew(),
        )

    unreal.MaterialEditingLibrary.set_material_instance_parent(mi, parent)
    unreal.MaterialEditingLibrary.set_material_instance_texture_parameter_value(mi, "FillTexture", tex)
    unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(mi, "ShimmerStrength", shimmer)
    unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(mi, "EmissiveBoost", emissive)
    unreal.EditorAssetLibrary.save_loaded_asset(mi)
    return mi


def main() -> None:
    tex = _import_or_update_texture()
    mi = _setup_mi(tex)
    unreal.log(f"Delivery progress health-bar style ready: {mi.get_path_name()}")


if __name__ == "__main__":
    main()

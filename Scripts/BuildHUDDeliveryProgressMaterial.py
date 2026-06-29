"""
Create MI_HUD_DeliveryProgressFill using the shared shimmer UI material parent.

Requires:
  /Game/UI/HUD/Materials/M_HUD_HealthBarFill  (run BuildHUDHealthBarMaterial.py first)
  /Game/UI/HUD/Textures/Delivery/T_HUD_Delivery_Fill

Run in Unreal Editor:
  exec(open(r'E:/Unreal/git/SpaCh4/Scripts/BuildHUDDeliveryProgressMaterial.py', encoding='utf-8').read())
"""

from __future__ import annotations

import unreal

MAT_PARENT = "/Game/UI/HUD/Materials/M_HUD_HealthBarFill"
MI_PATH = "/Game/UI/HUD/Materials/MI_HUD_DeliveryProgressFill"
FILL_TEX = "/Game/UI/HUD/Textures/Delivery/T_HUD_Delivery_Fill"
FILL_TEX_FALLBACK = "/Game/UI/HUD/Textures/Delivery/T_HUD_Delivery_Progress_10"


def _load_fill_texture():
    path = FILL_TEX if unreal.EditorAssetLibrary.does_asset_exist(FILL_TEX) else FILL_TEX_FALLBACK
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        raise RuntimeError(f"Missing delivery fill texture: {FILL_TEX}")
    tex = unreal.EditorAssetLibrary.load_asset(path)
    tex.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
    tex.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
    unreal.EditorAssetLibrary.save_loaded_asset(tex)
    return tex


def main() -> None:
    if not unreal.EditorAssetLibrary.does_asset_exist(MAT_PARENT):
        raise RuntimeError(f"Missing parent material: {MAT_PARENT}")

    parent = unreal.EditorAssetLibrary.load_asset(MAT_PARENT)
    fill_tex = _load_fill_texture()
    folder = "/Game/UI/HUD/Materials"

    if unreal.EditorAssetLibrary.does_asset_exist(MI_PATH):
        mi = unreal.EditorAssetLibrary.load_asset(MI_PATH)
    else:
        tools = unreal.AssetToolsHelpers.get_asset_tools()
        factory = unreal.MaterialInstanceConstantFactoryNew()
        mi = tools.create_asset(
            "MI_HUD_DeliveryProgressFill",
            folder,
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
        mi, "ShimmerStrength", 0.14
    )
    unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(
        mi, "EmissiveBoost", 1.35
    )
    unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(
        mi, "PanSpeed", 3.0
    )
    unreal.EditorAssetLibrary.save_loaded_asset(mi)
    unreal.log(f"Delivery progress MI ready: {MI_PATH}")


if __name__ == "__main__":
    main()

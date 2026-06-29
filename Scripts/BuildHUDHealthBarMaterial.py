"""
Rebuild M_HUD_HealthBarFill from scratch (delete + recreate).

Verified graph for UE 5.5:
  UV pan + sin wave -> FillTexture -> Emissive/BaseColor + Opacity
  Sine input pin MUST be "" (not "Input").

Run in Unreal Editor:
  exec(open(r'E:/Unreal/git/SpaCh4/Scripts/BuildHUDHealthBarMaterial.py', encoding='utf-8').read())
"""

from __future__ import annotations

import unreal

MAT_PATH = "/Game/UI/HUD/Materials/M_HUD_HealthBarFill"
MI_PATH = "/Game/UI/HUD/Materials/MI_HUD_HealthBarFill_Downed"
FILL_TEX = "/Game/UI/HUD/Textures/Teammate/T_HUD_Bar_Fill_Downed"


def _ensure_folder(path: str) -> None:
    folder = "/".join(path.split("/")[:-1])
    if not unreal.EditorAssetLibrary.does_directory_exist(folder):
        unreal.EditorAssetLibrary.make_directory(folder)


def _load_fill_texture():
    if not unreal.EditorAssetLibrary.does_asset_exist(FILL_TEX):
        raise RuntimeError(f"Missing texture: {FILL_TEX}")
    tex = unreal.EditorAssetLibrary.load_asset(FILL_TEX)
    # Prevent horizontal wrap seams on non-tileable fill art.
    tex.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
    tex.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
    unreal.EditorAssetLibrary.save_loaded_asset(tex)
    return tex


def _delete_assets() -> None:
    subsystem = unreal.get_editor_subsystem(unreal.AssetEditorSubsystem)
    for path in (MI_PATH, MAT_PATH):
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            asset = unreal.EditorAssetLibrary.load_asset(path)
            if asset and subsystem:
                subsystem.close_all_editors_for_asset(asset)
            unreal.EditorAssetLibrary.delete_asset(path)
            unreal.log(f"Deleted: {path}")


def _create_material() -> unreal.Material:
    _ensure_folder(MAT_PATH)
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    factory = unreal.MaterialFactoryNew()
    mat = tools.create_asset(
        "M_HUD_HealthBarFill",
        "/Game/UI/HUD/Materials",
        unreal.Material,
        factory,
    )
    if not mat:
        raise RuntimeError("Failed to create M_HUD_HealthBarFill")
    return mat


def _scalar_param(mel, material, name, default, x, y):
    node = mel.create_material_expression(
        material, unreal.MaterialExpressionScalarParameter, x, y
    )
    node.set_editor_property("parameter_name", unreal.Name(name))
    node.set_editor_property("default_value", float(default))
    return node


def _mask_channel(mel, material, src, r, g, b, a, x, y):
    node = mel.create_material_expression(
        material, unreal.MaterialExpressionComponentMask, x, y
    )
    node.set_editor_property("r", r)
    node.set_editor_property("g", g)
    node.set_editor_property("b", b)
    node.set_editor_property("a", a)
    mel.connect_material_expressions(src, "", node, "")
    return node


def _build_graph(material: unreal.Material, fill_tex) -> None:
    mel = unreal.MaterialEditingLibrary
    mel.delete_all_material_expressions(material)

    tex_coord = mel.create_material_expression(
        material, unreal.MaterialExpressionTextureCoordinate, -800, 0
    )
    time_node = mel.create_material_expression(
        material, unreal.MaterialExpressionTime, -800, 200
    )

    pan_speed = _scalar_param(mel, material, "PanSpeed", 2.5, -600, 280)
    wave_freq = _scalar_param(mel, material, "WaveFrequency", 10.0, -600, 360)
    wave_speed = _scalar_param(mel, material, "WaveSpeed", 1.8, -600, 440)
    wave_amp = _scalar_param(mel, material, "WaveAmplitude", 0.012, -200, 240)
    shimmer = _scalar_param(mel, material, "ShimmerStrength", 0.08, -600, 520)
    emissive_boost = _scalar_param(mel, material, "EmissiveBoost", 1.15, 620, -80)

    u = _mask_channel(mel, material, tex_coord, True, False, False, False, -600, 0)
    v = _mask_channel(mel, material, tex_coord, False, True, False, False, -600, 120)

    # V-only wave (no U scroll -> no wrap seam on non-tileable art)
    freq_mul = mel.create_material_expression(
        material, unreal.MaterialExpressionMultiply, -400, 80
    )
    mel.connect_material_expressions(u, "", freq_mul, "A")
    mel.connect_material_expressions(wave_freq, "", freq_mul, "B")

    speed_mul = mel.create_material_expression(
        material, unreal.MaterialExpressionMultiply, -400, 200
    )
    mel.connect_material_expressions(time_node, "", speed_mul, "A")
    mel.connect_material_expressions(wave_speed, "", speed_mul, "B")

    phase = mel.create_material_expression(material, unreal.MaterialExpressionAdd, -400, 120)
    mel.connect_material_expressions(freq_mul, "", phase, "A")
    mel.connect_material_expressions(speed_mul, "", phase, "B")

    sin_node = mel.create_material_expression(
        material, unreal.MaterialExpressionSine, -200, 120
    )
    sin_node.set_editor_property("period", 1.0)
    mel.connect_material_expressions(phase, "", sin_node, "")

    wave_mul = mel.create_material_expression(
        material, unreal.MaterialExpressionMultiply, 0, 120
    )
    mel.connect_material_expressions(sin_node, "", wave_mul, "A")
    mel.connect_material_expressions(wave_amp, "", wave_mul, "B")

    wave_v = mel.create_material_expression(material, unreal.MaterialExpressionAdd, 200, 100)
    mel.connect_material_expressions(v, "", wave_v, "A")
    mel.connect_material_expressions(wave_mul, "", wave_v, "B")

    uv = mel.create_material_expression(
        material, unreal.MaterialExpressionAppendVector, 400, 40
    )
    mel.connect_material_expressions(u, "", uv, "A")
    mel.connect_material_expressions(wave_v, "", uv, "B")

    uv_clamp = mel.create_material_expression(
        material, unreal.MaterialExpressionSaturate, 520, 40
    )
    mel.connect_material_expressions(uv, "", uv_clamp, "")

    tex_sample = mel.create_material_expression(
        material, unreal.MaterialExpressionTextureSampleParameter2D, 620, 40
    )
    tex_sample.set_editor_property("parameter_name", unreal.Name("FillTexture"))
    tex_sample.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    tex_sample.set_editor_property("texture", fill_tex)
    mel.connect_material_expressions(uv_clamp, "", tex_sample, "UVs")

    # Shimmer pulse (brightness) instead of U pan — avoids texture wrap seam.
    pulse = mel.create_material_expression(
        material, unreal.MaterialExpressionMultiply, 620, 220
    )
    mel.connect_material_expressions(time_node, "", pulse, "A")
    mel.connect_material_expressions(pan_speed, "", pulse, "B")

    pulse_sin = mel.create_material_expression(
        material, unreal.MaterialExpressionSine, 780, 220
    )
    pulse_sin.set_editor_property("period", 1.0)
    mel.connect_material_expressions(pulse, "", pulse_sin, "")

    pulse_scale = mel.create_material_expression(
        material, unreal.MaterialExpressionMultiply, 940, 220
    )
    mel.connect_material_expressions(pulse_sin, "", pulse_scale, "A")
    mel.connect_material_expressions(shimmer, "", pulse_scale, "B")

    pulse_add = mel.create_material_expression(
        material, unreal.MaterialExpressionAdd, 1100, 180
    )
    one = mel.create_material_expression(
        material, unreal.MaterialExpressionConstant, 940, 320
    )
    one.set_editor_property("r", 1.0)
    mel.connect_material_expressions(one, "", pulse_add, "A")
    mel.connect_material_expressions(pulse_scale, "", pulse_add, "B")

    color = mel.create_material_expression(
        material, unreal.MaterialExpressionMultiply, 860, 20
    )
    mel.connect_material_expressions(tex_sample, "", color, "A")
    mel.connect_material_expressions(emissive_boost, "", color, "B")

    color_pulse = mel.create_material_expression(
        material, unreal.MaterialExpressionMultiply, 1040, 20
    )
    mel.connect_material_expressions(color, "", color_pulse, "A")
    mel.connect_material_expressions(pulse_add, "", color_pulse, "B")

    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_UI)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)

    mel.connect_material_property(color_pulse, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(color_pulse, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(tex_sample, "A", unreal.MaterialProperty.MP_OPACITY)

    mel.recompile_material(material)


def _create_mi(material: unreal.Material, fill_tex) -> unreal.MaterialInstanceConstant:
    _ensure_folder(MI_PATH)
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    factory = unreal.MaterialInstanceConstantFactoryNew()
    mi = tools.create_asset(
        "MI_HUD_HealthBarFill_Downed",
        "/Game/UI/HUD/Materials",
        unreal.MaterialInstanceConstant,
        factory,
    )
    if not mi:
        raise RuntimeError("Failed to create MI_HUD_HealthBarFill_Downed")
    unreal.MaterialEditingLibrary.set_material_instance_parent(mi, material)
    unreal.MaterialEditingLibrary.set_material_instance_texture_parameter_value(
        mi, "FillTexture", fill_tex
    )
    return mi


def main() -> None:
    fill_tex = _load_fill_texture()
    subsystem = unreal.get_editor_subsystem(unreal.AssetEditorSubsystem)

    if unreal.EditorAssetLibrary.does_asset_exist(MAT_PATH):
        material = unreal.EditorAssetLibrary.load_asset(MAT_PATH)
        if subsystem and material:
            subsystem.close_all_editors_for_asset(material)
        _build_graph(material, fill_tex)
        if unreal.EditorAssetLibrary.does_asset_exist(MI_PATH):
            mi = unreal.EditorAssetLibrary.load_asset(MI_PATH)
            unreal.MaterialEditingLibrary.set_material_instance_parent(mi, material)
        else:
            mi = _create_mi(material, fill_tex)
        _assign_mi_texture(mi, fill_tex)
    else:
        material = _create_material()
        _build_graph(material, fill_tex)
        mi = _create_mi(material, fill_tex)

    unreal.EditorAssetLibrary.save_loaded_asset(material)
    unreal.EditorAssetLibrary.save_loaded_asset(mi)
    if subsystem and material:
        subsystem.open_editor_for_assets([material])
    unreal.log(f"HUD health bar material rebuilt: {MAT_PATH}")


def _assign_mi_texture(mi: unreal.MaterialInstanceConstant, fill_tex) -> None:
    unreal.MaterialEditingLibrary.set_material_instance_texture_parameter_value(
        mi, "FillTexture", fill_tex
    )


if __name__ == "__main__":
    main()

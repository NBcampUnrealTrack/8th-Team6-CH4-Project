"""
Trim, resize, and import Station A/B delivery composite frame PNGs.

Source:
  SourceArt/HUD/누끼/진행도/image-Photoroom (12|13).png

Output:
  Content/UI/HUD/Textures/Delivery/T_HUD_Delivery_Station_A.png
  Content/UI/HUD/Textures/Delivery/T_HUD_Delivery_Station_B.png

Run in editor via MCP run-python-script.
"""

from __future__ import annotations

import os
from pathlib import Path

import numpy as np
from PIL import Image

try:
    import unreal
except ImportError:
    unreal = None  # type: ignore

ROOT = Path(r"E:/Unreal/git/SpaCh4")
SRC_DIR = ROOT / "SourceArt/HUD/누끼/진행도"
OUT_DIR = ROOT / "Content/UI/HUD/Textures/Delivery"
TARGET_W = 420
TARGET_H = 76

ASSETS = [
    ("image-Photoroom (12).png", "T_HUD_Delivery_Station_A"),
    ("image-Photoroom (13).png", "T_HUD_Delivery_Station_B"),
]


def trim_and_resize(src: Path, dst: Path) -> None:
    im = Image.open(src).convert("RGBA")
    arr = np.array(im)
    alpha = arr[:, :, 3]
    ys, xs = np.where(alpha > 10)
    if ys.size == 0:
        raise RuntimeError(f"No opaque pixels in {src}")
    cropped = im.crop((xs.min(), ys.min(), xs.max() + 1, ys.max() + 1))
    resized = cropped.resize((TARGET_W, TARGET_H), Image.Resampling.LANCZOS)
    dst.parent.mkdir(parents=True, exist_ok=True)
    resized.save(dst)


def import_texture(png: Path, asset_name: str) -> unreal.Texture2D:
    dest = "/Game/UI/HUD/Textures/Delivery"
    full_path = f"{dest}/{asset_name}.{asset_name}"

    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(png).replace("\\", "/"))
    task.set_editor_property("destination_path", dest)
    task.set_editor_property("destination_name", asset_name)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    tex = unreal.EditorAssetLibrary.load_asset(full_path)
    if not tex:
        raise RuntimeError(f"Import failed: {full_path}")

    tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_BC7)
    tex.set_editor_property("filter", unreal.TextureFilter.TF_BILINEAR)
    tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    tex.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
    tex.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
    unreal.EditorAssetLibrary.save_loaded_asset(tex)
    return tex


def main() -> None:
    if unreal is None:
        raise RuntimeError("Run this script inside Unreal Editor Python")
    for src_name, asset_name in ASSETS:
        src = SRC_DIR / src_name
        if not src.exists():
            raise FileNotFoundError(src)
        png_out = OUT_DIR / f"{asset_name}.png"
        trim_and_resize(src, png_out)
        tex = import_texture(png_out, asset_name)
        unreal.log(f"Imported {tex.get_path_name()} ({TARGET_W}x{TARGET_H})")


if __name__ == "__main__":
    main()

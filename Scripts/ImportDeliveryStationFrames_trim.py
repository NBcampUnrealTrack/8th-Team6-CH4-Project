"""Trim SourceArt cutouts to 420x76 PNGs for UE import."""
from __future__ import annotations

from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(r"E:/Unreal/git/SpaCh4")
SRC_DIR = ROOT / "SourceArt" / "HUD" / "누끼" / "진행도"
OUT_DIR = ROOT / "Content" / "UI" / "HUD" / "Textures" / "Delivery"
TARGET_W = 420
TARGET_H = 76

ASSETS = [
    ("image-Photoroom (12).png", "T_HUD_Delivery_Station_A", "alpha"),
    ("image-Photoroom (14).png", "T_HUD_Delivery_Station_B", "alpha"),
]


def trim_alpha_crop(im: Image.Image) -> Image.Image:
    arr = np.array(im.convert("RGBA"))
    alpha = arr[:, :, 3]
    ys, xs = np.where(alpha > 10)
    if ys.size == 0:
        raise RuntimeError("No opaque pixels in source image")
    return im.crop((xs.min(), ys.min(), xs.max() + 1, ys.max() + 1))


def trim_nonwhite_crop(im: Image.Image) -> Image.Image:
    arr = np.array(im.convert("RGBA"))
    rgb = arr[:, :, :3]
    nonwhite = ~((rgb[:, :, 0] > 250) & (rgb[:, :, 1] > 250) & (rgb[:, :, 2] > 250))
    ys, xs = np.where(nonwhite)
    if ys.size == 0:
        raise RuntimeError("No frame pixels in source image")
    return im.crop((xs.min(), ys.min(), xs.max() + 1, ys.max() + 1))


def punch_white_fill_slot(im: Image.Image) -> Image.Image:
    """Make the empty progress slot transparent (white interior on B variant)."""
    arr = np.array(im.convert("RGBA"))
    lum = arr[:, :, :3].astype(np.float32).mean(axis=2)
    h = arr.shape[0]
    band_top = int(h * 0.12)
    band_bottom = int(h * 0.88)
    white = lum > 235
    slot = np.zeros_like(white, dtype=bool)
    slot[band_top:band_bottom, :] = white[band_top:band_bottom, :]
    arr[slot, 3] = 0
    return Image.fromarray(arr, mode="RGBA")


def trim_and_resize(src: Path, dst: Path, mode: str) -> None:
    im = Image.open(src)
    if mode == "alpha":
        cropped = trim_alpha_crop(im)
    elif mode == "punch_white_slot":
        cropped = trim_nonwhite_crop(im)
        cropped = punch_white_fill_slot(cropped)
    else:
        raise ValueError(f"Unknown trim mode: {mode}")

    resized = cropped.resize((TARGET_W, TARGET_H), Image.Resampling.LANCZOS)
    dst.parent.mkdir(parents=True, exist_ok=True)
    resized.save(dst)


def main() -> None:
    for src_name, asset_name, mode in ASSETS:
        src = SRC_DIR / src_name
        if not src.exists():
            raise FileNotFoundError(src)
        out = OUT_DIR / f"{asset_name}.png"
        trim_and_resize(src, out, mode)
        print(out)


if __name__ == "__main__":
    main()

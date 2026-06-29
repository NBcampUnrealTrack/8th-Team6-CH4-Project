"""
Import inventory slot border from Photoroom-matted source art.

Source:
  SourceArt/HUD/누끼/인벤토리/테두리.png

Output:
  Content/UI/HUD/Textures/Inventory/T_HUD_Inventory_Slot_Empty.png

Usage:
  python Scripts/ImportInventorySlotBorder.py
"""

from __future__ import annotations

from pathlib import Path

import numpy as np
from PIL import Image

SOURCE_DIR = Path(r"E:\Unreal\git\SpaCh4\SourceArt\HUD\누끼\인벤토리")
OUTPUT = Path(
    r"E:\Unreal\git\SpaCh4\Content\UI\HUD\Textures\Inventory\T_HUD_Inventory_Slot_Empty.png"
)
TARGET_SIZE = 128
WORK_SIZE = 512


def resolve_source() -> Path:
    pngs = sorted(SOURCE_DIR.glob("*.png"))
    if not pngs:
        raise FileNotFoundError(f"No PNG in {SOURCE_DIR}")
    return pngs[0]


def clean_photoroom(rgba: np.ndarray, dark: float = 22.0) -> np.ndarray:
    out = rgba.copy()
    rgb = out[:, :, :3].astype(np.float32)
    alpha = out[:, :, 3].astype(np.float32)
    lum = rgb.mean(axis=2)

    backdrop = lum < dark
    out[backdrop, 3] = 0
    out[backdrop, :3] = 0

    fringe = (lum < 55) & (alpha > 0)
    out[fringe, 3] = np.clip(alpha[fringe] * (lum[fringe] - 8.0) / 45.0, 0, 255).astype(
        np.uint8
    )
    return out


def content_bounds(alpha: np.ndarray, padding: int = 6) -> tuple[int, int, int, int]:
    ys, xs = np.where(alpha > 12)
    if ys.size == 0:
        h, w = alpha.shape
        return 0, h, 0, w
    y0 = max(int(ys.min()) - padding, 0)
    y1 = min(int(ys.max()) + padding + 1, alpha.shape[0])
    x0 = max(int(xs.min()) - padding, 0)
    x1 = min(int(xs.max()) + padding + 1, alpha.shape[1])
    return y0, y1, x0, x1


def resize_square(rgba: np.ndarray, size: int) -> np.ndarray:
    rgb = Image.fromarray(rgba[:, :, :3], mode="RGB").resize(
        (size, size), Image.Resampling.LANCZOS
    )
    alpha = Image.fromarray(rgba[:, :, 3], mode="L").resize(
        (size, size), Image.Resampling.BILINEAR
    )
    out = np.dstack([np.array(rgb), np.array(alpha)]).astype(np.uint8)
    out[out[:, :, 3] < 8, :3] = 0
    return out


def import_slot(source: Path) -> np.ndarray:
    rgba = clean_photoroom(np.array(Image.open(source).convert("RGBA")))
    y0, y1, x0, x1 = content_bounds(rgba[:, :, 3])
    cropped = rgba[y0:y1, x0:x1]
    work = resize_square(cropped, WORK_SIZE)
    return resize_square(work, TARGET_SIZE)


def main() -> None:
    source = resolve_source()
    print(f"Inventory slot border <- {source.name}")
    out = import_slot(source)
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    Image.fromarray(out, mode="RGBA").save(OUTPUT, optimize=True)
    print(f"  {OUTPUT.name} ({out.shape[1]}x{out.shape[0]})")
    print("Done. Reimport Textures/Inventory in Content Browser.")


if __name__ == "__main__":
    main()

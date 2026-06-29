"""
Create green delivery progress fill from downed health bar fill art.

Output:
  Content/UI/HUD/Textures/Delivery/T_HUD_Bar_Fill_Delivery.png
"""

from __future__ import annotations

from pathlib import Path

import numpy as np
from PIL import Image

SRC = Path(r"E:\Unreal\git\SpaCh4\Content\UI\HUD\Textures\Teammate\T_HUD_Bar_Fill_Downed.png")
OUT = Path(r"E:\Unreal\git\SpaCh4\Content\UI\HUD\Textures\Delivery\T_HUD_Bar_Fill_Delivery.png")
TARGET_W = 256
TARGET_H = 47


def solidify_edges(
    src: np.ndarray,
    vertical_px: int = 3,
    left_px: int = 6,
    right_px: int = 4,
) -> np.ndarray:
    """Boost edge alpha so the shimmer fill visually reaches the slot borders."""
    out = src.copy()
    alpha = out[:, :, 3].astype(np.float32)
    h, w = alpha.shape
    if vertical_px > 0:
        for row in range(min(vertical_px, h)):
            alpha[row, :] = np.maximum(alpha[row, :], 245.0)
            alpha[h - 1 - row, :] = np.maximum(alpha[h - 1 - row, :], 245.0)
    if left_px > 0:
        for col in range(min(left_px, w)):
            alpha[:, col] = np.maximum(alpha[:, col], 235.0)
    if right_px > 0:
        for col in range(min(right_px, w)):
            alpha[:, w - 1 - col] = np.maximum(alpha[:, w - 1 - col], 230.0)
    out[:, :, 3] = np.clip(alpha, 0.0, 255.0).astype(np.uint8)
    return out


def red_to_green_fill(src: np.ndarray) -> np.ndarray:
    """Shift red emissive downed fill to green while keeping glow/circuit detail."""
    rgb = src[:, :, :3].astype(np.float32)
    alpha = src[:, :, 3:4].astype(np.float32)

    r = rgb[:, :, 0:1]
    g = rgb[:, :, 1:2]
    b = rgb[:, :, 2:3]

    red_energy = np.clip(r - np.maximum(g, b), 0.0, 255.0)
    red_mask = red_energy > 12.0

    new_r = np.where(red_mask, r * 0.22 + g * 0.18, r)
    new_g = np.where(red_mask, np.clip(g + red_energy * 0.95 + 18.0, 0.0, 255.0), g)
    new_b = np.where(red_mask, np.clip(b * 0.35 + red_energy * 0.08, 0.0, 255.0), b)

    out = np.concatenate([new_r, new_g, new_b, alpha], axis=2)
    return np.clip(out, 0.0, 255.0).astype(np.uint8)


def crop_opaque_vertical(src: np.ndarray, alpha_threshold: float = 200.0) -> np.ndarray:
    """Remove soft vertical alpha padding so the fill reaches slot top/bottom."""
    alpha = src[:, :, 3]
    rows = np.where(alpha.max(axis=1) >= alpha_threshold)[0]
    if rows.size == 0:
        return src
    return src[rows.min() : rows.max() + 1, :, :]


def resize_to_slot(src: np.ndarray, width: int, height: int) -> np.ndarray:
    """Fit source art to the delivery slot with minimal vertical fade."""
    from PIL import Image

    im = Image.fromarray(src, mode="RGBA")
    return np.array(im.resize((width, height), Image.Resampling.LANCZOS))


def main() -> None:
    if not SRC.exists():
        raise FileNotFoundError(f"Missing source texture: {SRC}")

    OUT.parent.mkdir(parents=True, exist_ok=True)
    src = np.array(Image.open(SRC).convert("RGBA"))
    converted = red_to_green_fill(src)
    cropped = crop_opaque_vertical(converted)
    fitted = resize_to_slot(cropped, TARGET_W, TARGET_H)
    fitted = solidify_edges(fitted)
    Image.fromarray(fitted, mode="RGBA").save(OUT)
    print(f"Wrote {OUT} ({fitted.shape[1]}x{fitted.shape[0]})")


if __name__ == "__main__":
    main()

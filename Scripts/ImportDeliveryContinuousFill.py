"""
Generate a continuous (non-segmented) delivery progress fill texture.

Output:
  Content/UI/HUD/Textures/Delivery/T_HUD_Delivery_Fill_Smooth.png

Usage:
  python Scripts/ImportDeliveryContinuousFill.py
"""

from __future__ import annotations

import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageFilter

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from ImportDeliveryProgressBar import (
    OUTPUT_SIZE,
    SLOT_MARGINS,
    clean_photoroom,
    compose_bar,
    crop_bar,
    load_bars,
    resize_bar,
    save_png,
)

OUTPUT_DIR = Path(r"E:\Unreal\git\SpaCh4\Content\UI\HUD\Textures\Delivery")
OUTPUT_NAME = "T_HUD_Delivery_Fill_Smooth.png"


def extract_continuous_fill(empty_bar: np.ndarray, full_bar: np.ndarray) -> np.ndarray:
    """Full inner glow as one strip — blur removes slot divider seams."""
    ml, mr, mt, mb = SLOT_MARGINS
    h, w = empty_bar.shape[:2]
    composite = compose_bar(empty_bar, full_bar, 10)
    inner = composite[mt : h - mb, ml : w - mr].copy()

    pil = Image.fromarray(inner, mode="RGBA")
    # Wider horizontal blur smooths vertical slot seams.
    pil = pil.filter(ImageFilter.GaussianBlur(radius=3))
    inner = np.array(pil, dtype=np.uint8)

    # Boost green emissive read at HUD scale.
    rgb = inner[:, :, :3].astype(np.float32)
    alpha = inner[:, :, 3:4].astype(np.float32) / 255.0
    lum = rgb.mean(axis=2, keepdims=True)
    green_mask = (rgb[:, :, 1:2] > rgb[:, :, 0:1] + 10) & (rgb[:, :, 1:2] > rgb[:, :, 2:3] + 8)
    rgb = np.where(green_mask, np.minimum(rgb * 1.08 + 8, 255), rgb)
    inner[:, :, :3] = np.clip(rgb, 0, 255).astype(np.uint8)
    inner[:, :, 3:4] = np.clip(alpha * 255 + green_mask.astype(np.float32) * 12, 0, 255).astype(
        np.uint8
    )

    out = np.zeros_like(empty_bar)
    out[mt : h - mb, ml : w - mr] = inner
    return out


def main() -> None:
    empty_bar, full_bar = load_bars()
    native = extract_continuous_fill(empty_bar, full_bar)
    out = resize_bar(native, OUTPUT_SIZE[0], OUTPUT_SIZE[1])
    save_png(OUTPUT_DIR / OUTPUT_NAME, out)
    print(f"Done. Reimport {OUTPUT_NAME} in Unreal (or run BuildHUDDeliveryProgressAll.py).")


if __name__ == "__main__":
    main()

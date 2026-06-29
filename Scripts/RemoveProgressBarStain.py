"""
Remove white diamond artifacts from generated progress bar source art (1024x682).

Usage:
  python Scripts/RemoveProgressBarStain.py
"""

from __future__ import annotations

from pathlib import Path

import numpy as np
from PIL import Image

HUD_DIR = Path(r"E:\Unreal\git\SpaCh4\SourceArt\HUD")
FILES = [
    "ProgressBar_Gen_Empty_10Slots.png",
    "ProgressBar_Gen_Full_10Slots.png",
]

# (x0, y0, x1, y1, min_lum) — only pixels at or above min_lum are treated as artifacts.
STAIN_RECTS: dict[str, list[tuple[int, int, int, int, int]]] = {
    "ProgressBar_Gen_Empty_10Slots.png": [(943, 386, 948, 389, 30)],
    "ProgressBar_Gen_Full_10Slots.png": [
        (949, 376, 972, 390, 120),  # frame corner triangle
        (958, 368, 973, 386, 150),  # slot-10 inner corner
        (980, 389, 987, 395, 190),  # bezel speck
    ],
}


def inpaint_rect(rgb: np.ndarray, rect: tuple[int, int, int, int, int]) -> int:
    x0, y0, x1, y1, min_lum = rect
    patch = rgb[y0:y1, x0:x1].astype(np.float32)
    ph, pw = patch.shape[:2]
    lum = patch.mean(axis=2)

    # Artifact pixels are locally brighter than the surrounding dark frame.
    base = float(np.percentile(lum, 35))
    stain = (lum > base + 18) & (lum >= min_lum)

    fixed = 0
    for j in range(ph):
        for i in range(pw):
            if not stain[j, i]:
                continue
            refs = []
            for dj in range(-14, 15):
                for di in range(-14, 15):
                    ny, nx = y0 + j + dj, x0 + i + di
                    if ny < 0 or nx < 0 or ny >= rgb.shape[0] or nx >= rgb.shape[1]:
                        continue
                    if y0 <= ny < y1 and x0 <= nx < x1 and stain[ny - y0, nx - x0]:
                        continue
                    rp = rgb[ny, nx].astype(np.float32)
                    if rp.mean() < lum[j, i] - 10:
                        refs.append(rp)
            if refs:
                patch[j, i] = np.median(refs, axis=0)
                fixed += 1

    rgb[y0:y1, x0:x1] = np.clip(patch, 0, 255).astype(np.uint8)
    return fixed


def process(path: Path) -> None:
    rgb = np.array(Image.open(path).convert("RGB"))
    total = 0
    for rect in STAIN_RECTS.get(path.name, []):
        total += inpaint_rect(rgb, rect)
    Image.fromarray(rgb).save(path)
    print(f"{path.name}: fixed {total} px")


def main() -> None:
    for name in FILES:
        process(HUD_DIR / name)


if __name__ == "__main__":
    main()

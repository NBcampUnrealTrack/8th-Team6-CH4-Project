"""Measure progress fill slot insets from station frame PNGs."""
from __future__ import annotations

from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(r"E:/Unreal/git/SpaCh4")
SRC = ROOT / "SourceArt/HUD/누끼/진행도"
TARGET_W = 420
TARGET_H = 76

SAMPLES = [
    ("image-Photoroom (12).png", "A"),
    ("image-Photoroom (14).png", "B"),
]


def crop_content(path: Path) -> np.ndarray:
    arr = np.array(Image.open(path).convert("RGBA"))
    alpha = arr[:, :, 3]
    ys, xs = np.where(alpha > 10)
    if ys.size:
        return arr[ys.min() : ys.max() + 1, xs.min() : xs.max() + 1]
    rgb = arr[:, :, :3]
    mask = ~((rgb[:, :, 0] > 250) & (rgb[:, :, 1] > 250) & (rgb[:, :, 2] > 250))
    ys, xs = np.where(mask)
    return arr[ys.min() : ys.max() + 1, xs.min() : xs.max() + 1]


def longest_run(indices: np.ndarray) -> tuple[int, int, int]:
    if indices.size == 0:
        return 0, 0, 0
    best = (0, 0, 0)
    start = int(indices[0])
    prev = start
    for value in indices[1:]:
        value = int(value)
        if value == prev + 1:
            prev = value
            continue
        length = prev - start
        if length > best[2]:
            best = (start, prev, length)
        start = prev = value
    length = prev - start
    if length > best[2]:
        best = (start, prev, length)
    return best


def measure_slot(crop: np.ndarray, label: str, out_w: int, out_h: int) -> dict:
    h, w = crop.shape[:2]
    lum = crop[:, :, :3].astype(np.float32).mean(axis=2)
    alpha = crop[:, :, 3]
    dark = ((alpha > 150) & (lum < 42)) | (alpha < 30)

    y0, y1 = int(h * 0.18), int(h * 0.82)
    mid_row = dark[y0 + (y1 - y0) // 2]
    idx = np.where(mid_row)[0]
    start, end, _ = longest_run(idx)
    left, right = start, end

    col_dark = dark[y0:y1, left : right + 1].mean(axis=1) > 0.85
    rows = np.where(col_dark)[0]
    top = int(rows[0]) + y0 if rows.size else y0
    bottom = int(rows[-1]) + y0 if rows.size else y1

    sx, sy = out_w / w, out_h / h
    result = {
        "label": label,
        "left": round(left * sx, 1),
        "right": round((w - right - 1) * sx, 1),
        "width": round((right - left + 1) * sx, 1),
        "top": round(top * sy, 1),
        "bottom": round((h - bottom - 1) * sy, 1),
        "height": round((bottom - top + 1) * sy, 1),
    }
    print(
        f"{label}: L={result['left']} R={result['right']} W={result['width']} "
        f"T={result['top']} B={result['bottom']} H={result['height']}"
    )
    return result


def main() -> None:
    for filename, label in SAMPLES:
        crop = crop_content(SRC / filename)
        resized = np.array(
            Image.fromarray(crop).resize((TARGET_W, TARGET_H), Image.Resampling.LANCZOS)
        )
        print(f"--- {label} source ---")
        measure_slot(crop, label + "_native", TARGET_W, TARGET_H)
        print(f"--- {label} resized ---")
        measure_slot(resized, label + "_420", TARGET_W, TARGET_H)


if __name__ == "__main__":
    main()

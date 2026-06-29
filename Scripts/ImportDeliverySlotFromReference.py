"""
Cut out T_HUD_Delivery_Slot from reference art with clean alpha.

Usage:
  python Scripts/ImportDeliverySlotFromReference.py [source_image_path]
"""

from __future__ import annotations

import sys
from collections import deque
from pathlib import Path

import numpy as np
from PIL import Image

DEFAULT_SOURCE = Path(
    r"E:\Unreal\git\SpaCh4\Scripts\Reference_DeliverySlot_Source.png"
)
OUTPUT = Path(
    r"E:\Unreal\git\SpaCh4\Content\UI\HUD\Textures\Delivery\T_HUD_Delivery_Slot.png"
)
TARGET_SIZE = 256


def lum(r, g, b) -> float:
    return (float(r) + float(g) + float(b)) / 3.0


def is_gray(r: int, g: int, b: int, tolerance: int = 10) -> bool:
    return abs(int(r) - int(g)) <= tolerance and abs(int(g) - int(b)) <= tolerance


def is_checker(r: int, g: int, b: int) -> bool:
    if not is_gray(r, g, b):
        return False
    l = lum(r, g, b)
    return (56 <= l <= 74) or (112 <= l <= 130)


def is_bg_shadow(r: int, g: int, b: int) -> bool:
    if not is_gray(r, g, b):
        return False
    l = lum(r, g, b)
    return 74 <= l <= 112


def can_flood_bg(r: int, g: int, b: int) -> bool:
    return is_checker(r, g, b) or is_bg_shadow(r, g, b)


def flood_background(rgb: np.ndarray) -> np.ndarray:
    h, w, _ = rgb.shape
    bg = np.zeros((h, w), dtype=bool)
    q: deque[tuple[int, int]] = deque()

    def push(y: int, x: int) -> None:
        if y < 0 or y >= h or x < 0 or x >= w or bg[y, x]:
            return
        r, g, b = rgb[y, x]
        if can_flood_bg(int(r), int(g), int(b)):
            bg[y, x] = True
            q.append((y, x))

    for x in range(w):
        push(0, x)
        push(h - 1, x)
    for y in range(h):
        push(y, 0)
        push(y, w - 1)

    while q:
        y, x = q.popleft()
        push(y - 1, x)
        push(y + 1, x)
        push(y, x - 1)
        push(y, x + 1)

    return bg


def is_hollow_interior(r: int, g: int, b: int) -> bool:
    l = lum(float(r), float(g), float(b))
    if l < 55:
        return False
    if is_checker(r, g, b):
        return True
    if is_bg_shadow(r, g, b):
        return True
    return l >= 165


def flood_inner_window(rgb: np.ndarray) -> np.ndarray:
    h, w, _ = rgb.shape
    hole = np.zeros((h, w), dtype=bool)
    y0, y1 = int(h * 0.24), int(h * 0.40)
    x0, x1 = int(w * 0.36), int(w * 0.64)

    seeds: list[tuple[int, int]] = [(y0 + y1) // 2, (x0 + x1) // 2]
    seeds = [((y0 + y1) // 2, (x0 + x1) // 2)]
    for y in range(y0, y1, 4):
        for x in range(x0, x1, 4):
            r, g, b = rgb[y, x]
            if is_checker(int(r), int(g), int(b)):
                seeds.append((y, x))

    q: deque[tuple[int, int]] = deque()
    for sy, sx in seeds:
        if hole[sy, sx]:
            continue
        r, g, b = rgb[sy, sx]
        if not is_hollow_interior(int(r), int(g), int(b)):
            continue
        hole[sy, sx] = True
        q.append((sy, sx))

    while q:
        y, x = q.popleft()
        for ny, nx in ((y - 1, x), (y + 1, x), (y, x - 1), (y, x + 1)):
            if ny < y0 or ny >= y1 or nx < x0 or nx >= x1 or hole[ny, nx]:
                continue
            pr, pg, pb = rgb[ny, nx]
            if is_hollow_interior(int(pr), int(pg), int(pb)):
                hole[ny, nx] = True
                q.append((ny, nx))

    return hole


def build_alpha(rgb: np.ndarray, bg: np.ndarray, hole: np.ndarray) -> np.ndarray:
    h, w, _ = rgb.shape
    alpha = np.full((h, w), 255, dtype=np.uint8)

    for y in range(h):
        for x in range(w):
            if hole[y, x]:
                alpha[y, x] = 0
                continue
            if not bg[y, x]:
                continue

            r, g, b = rgb[y, x]
            if is_checker(int(r), int(g), int(b)):
                alpha[y, x] = 0
            elif is_bg_shadow(int(r), int(g), int(b)):
                l = lum(r, g, b)
                alpha[y, x] = int(np.clip((112.0 - l) / 38.0 * 120.0, 12, 120))
            else:
                alpha[y, x] = 0

    return alpha


def resize_rgba(rgba: np.ndarray, hole: np.ndarray, size: int) -> np.ndarray:
    """Downscale RGBA; keep the inner window hole crisp via nearest-neighbor mask."""
    h, w = rgba.shape[:2]
    if h == size and w == size:
        return rgba

    rgb = Image.fromarray(rgba[:, :, :3], mode="RGB").resize(
        (size, size), Image.Resampling.LANCZOS
    )
    alpha = Image.fromarray(rgba[:, :, 3], mode="L").resize(
        (size, size), Image.Resampling.BILINEAR
    )
    hole_mask = Image.fromarray((hole.astype(np.uint8) * 255), mode="L").resize(
        (size, size), Image.Resampling.NEAREST
    )

    out = np.dstack([np.array(rgb), np.array(alpha)]).astype(np.uint8)
    out[np.array(hole_mask) > 0, 3] = 0
    transparent = out[:, :, 3] < 8
    out[transparent, :3] = 0
    return out


def content_bounds(alpha: np.ndarray, padding: int) -> tuple[int, int, int, int]:
    ys, xs = np.where(alpha > 8)
    if ys.size == 0:
        h, w = alpha.shape
        return 0, h, 0, w

    y0 = max(int(ys.min()) - padding, 0)
    y1 = min(int(ys.max()) + padding + 1, alpha.shape[0])
    x0 = max(int(xs.min()) - padding, 0)
    x1 = min(int(xs.max()) + padding + 1, alpha.shape[1])
    return y0, y1, x0, x1


def trim_to_content(rgba: np.ndarray, padding: int = 10) -> np.ndarray:
    y0, y1, x0, x1 = content_bounds(rgba[:, :, 3], padding)
    return rgba[y0:y1, x0:x1]


def process(source: Path, output: Path, size: int) -> None:
    img = Image.open(source).convert("RGBA")
    rgb = np.array(img)[:, :, :3]
    bg = flood_background(rgb)
    hole = flood_inner_window(rgb)
    alpha = build_alpha(rgb, bg, hole)

    rgba = np.dstack([rgb, alpha])
    y0, y1, x0, x1 = content_bounds(alpha, padding=12)
    cropped = rgba[y0:y1, x0:x1]
    cropped_hole = hole[y0:y1, x0:x1]
    resized = resize_rgba(cropped, cropped_hole, size)

    output.parent.mkdir(parents=True, exist_ok=True)
    Image.fromarray(resized, mode="RGBA").save(output)
    print(f"Saved {output} ({size}x{size}, RGBA)")


def main() -> None:
    source = Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_SOURCE
    if not source.exists():
        raise FileNotFoundError(source)
    process(source, OUTPUT, TARGET_SIZE)


if __name__ == "__main__":
    main()

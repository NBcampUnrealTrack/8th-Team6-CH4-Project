"""
Composite perk slot: keep existing inner well background, replace frame with new art.

Sources:
  SourceArt/HUD/누끼/퍽슬롯/테두리.png  — new diamond frame (transparent center)
  Content/.../T_HUD_Perk_Slot_Empty.png — current slot (well extracted from here)

Output:
  Content/UI/HUD/Textures/Perk/T_HUD_Perk_Slot_Empty.png

The border is scaled so its inner aperture matches the existing well radius.

Usage:
  python Scripts/ImportPerkSlotBorder.py
"""

from __future__ import annotations

from pathlib import Path

import numpy as np
from PIL import Image

SOURCE_DIR = Path(r"E:\Unreal\git\SpaCh4\SourceArt\HUD\누끼\퍽슬롯")
CURRENT_SLOT = Path(
    r"E:\Unreal\git\SpaCh4\Content\UI\HUD\Textures\Perk\T_HUD_Perk_Slot_Empty.png"
)
WELL_REFERENCE = Path(
    r"E:\Unreal\git\SpaCh4\Content\UI\HUD\Textures\Perk\_Perk_Slot_WellReference.png"
)
OUTPUT = CURRENT_SLOT
TARGET_SIZE = 128
WELL_LUM_THRESH = 30.0
OUTER_MARGIN = 1.0


def resolve_border_source() -> Path:
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


def content_bounds(alpha: np.ndarray, padding: int = 4) -> tuple[int, int, int, int]:
    ys, xs = np.where(alpha > 12)
    if ys.size == 0:
        h, w = alpha.shape
        return 0, h, 0, w
    y0 = max(int(ys.min()) - padding, 0)
    y1 = min(int(ys.max()) + padding + 1, alpha.shape[0])
    x0 = max(int(xs.min()) - padding, 0)
    x1 = min(int(xs.max()) + padding + 1, alpha.shape[1])
    return y0, y1, x0, x1


def diamond_radii(alpha: np.ndarray, cx: int, cy: int, thresh: int = 20) -> tuple[list[int], list[int]]:
    """Walk from center along 4 axes: inner hole radius, outer frame radius."""
    h, w = alpha.shape
    dirs = [(0, -1), (1, 0), (0, 1), (-1, 0)]
    inner: list[int] = []
    outer: list[int] = []
    for dx, dy in dirs:
        x, y = cx, cy
        r_in = 0
        while 0 <= x < w and 0 <= y < h and alpha[y, x] <= thresh:
            r_in += 1
            x += dx
            y += dy
        inner.append(r_in)

        r_out = r_in
        while 0 <= x < w and 0 <= y < h and alpha[y, x] > thresh:
            r_out += 1
            x += dx
            y += dy
        outer.append(r_out)
    return inner, outer


def measure_well_radius(slot: np.ndarray) -> float:
    lum = slot[:, :, :3].astype(np.float32).mean(axis=2)
    h, w = lum.shape
    cx, cy = w // 2, h // 2
    well = lum < WELL_LUM_THRESH
    dirs = [(0, -1), (1, 0), (0, 1), (-1, 0)]
    radii: list[int] = []
    for dx, dy in dirs:
        x, y = cx, cy
        r = 0
        while 0 <= x < w and 0 <= y < h and well[y, x]:
            r += 1
            x += dx
            y += dy
        radii.append(r)
    return float(np.mean(radii))


def extract_well_background(slot: np.ndarray, well_r: float) -> np.ndarray:
    """Keep only the recessed inner diamond; scale to match border inner aperture."""
    h, w = slot.shape[:2]
    cx, cy = w // 2, h // 2
    lum = slot[:, :, :3].astype(np.float32).mean(axis=2)
    well = lum < WELL_LUM_THRESH

    yy, xx = np.ogrid[:h, :w]
    source_r = measure_well_radius(slot)
    diamond = (np.abs(xx - cx) + np.abs(yy - cy) <= source_r + 0.5) & well

    well_layer = np.zeros_like(slot)
    well_layer[diamond] = slot[diamond]

    scale = well_r / source_r if source_r > 0 else 1.0
    scaled_size = max(1, int(round(w * scale)))
    scaled = resize_rgba(well_layer, (scaled_size, scaled_size))

    out = np.zeros((TARGET_SIZE, TARGET_SIZE, 4), dtype=np.uint8)
    ox = (TARGET_SIZE - scaled_size) // 2
    oy = (TARGET_SIZE - scaled_size) // 2
    out[oy : oy + scaled_size, ox : ox + scaled_size] = scaled
    return out


def ensure_well_reference(current_slot: Path) -> np.ndarray:
    """Preserve the original procedural well before the first border swap."""
    if WELL_REFERENCE.is_file():
        return np.array(Image.open(WELL_REFERENCE).convert("RGBA"))

    slot = np.array(Image.open(current_slot).convert("RGBA"))
    WELL_REFERENCE.parent.mkdir(parents=True, exist_ok=True)
    Image.fromarray(slot, mode="RGBA").save(WELL_REFERENCE, optimize=True)
    return slot


def resize_rgba(rgba: np.ndarray, size: tuple[int, int]) -> np.ndarray:
    tw, th = size
    rgb = Image.fromarray(rgba[:, :, :3], mode="RGB").resize((tw, th), Image.Resampling.LANCZOS)
    alpha = Image.fromarray(rgba[:, :, 3], mode="L").resize((tw, th), Image.Resampling.BILINEAR)
    out = np.dstack([np.array(rgb), np.array(alpha)]).astype(np.uint8)
    out[out[:, :, 3] < 8, :3] = 0
    return out


def prepare_border(source: Path, canvas: int) -> tuple[np.ndarray, float]:
    """Scale border to fit canvas; return layer + inner aperture radius."""
    rgba = clean_photoroom(np.array(Image.open(source).convert("RGBA")))
    y0, y1, x0, x1 = content_bounds(rgba[:, :, 3])
    cropped = rgba[y0:y1, x0:x1]

    ch, cw = cropped.shape[:2]
    ccx, ccy = cw // 2, ch // 2
    inner, outer = diamond_radii(cropped[:, :, 3], ccx, ccy)
    inner_r = float(np.mean(inner))
    outer_r = float(max(outer))
    if inner_r < 8 or outer_r < inner_r:
        raise RuntimeError("Could not detect border inner/outer radii")

    target_outer = canvas / 2.0 - OUTER_MARGIN
    scale = target_outer / outer_r
    new_w = max(1, int(round(cw * scale)))
    new_h = max(1, int(round(ch * scale)))
    scaled = resize_rgba(cropped, (new_w, new_h))

    canvas_img = np.zeros((canvas, canvas, 4), dtype=np.uint8)
    ox = (canvas - new_w) // 2
    oy = (canvas - new_h) // 2
    canvas_img[oy : oy + new_h, ox : ox + new_w] = scaled

    scaled_inner = inner_r * scale
    return canvas_img, scaled_inner


def alpha_composite(bottom: np.ndarray, top: np.ndarray) -> np.ndarray:
    out = bottom.astype(np.float32)
    src = top.astype(np.float32)
    sa = src[:, :, 3:4] / 255.0
    da = out[:, :, 3:4] / 255.0
    out_a = sa + da * (1.0 - sa)
    safe_a = np.maximum(out_a, 1e-6)
    out_rgb = (src[:, :, :3] * sa + out[:, :, :3] * da * (1.0 - sa)) / safe_a
    result = np.dstack([out_rgb, out_a * 255.0]).astype(np.uint8)
    result[result[:, :, 3] < 8, :3] = 0
    return result


def build_slot(border_source: Path, current_slot: Path, size: int = TARGET_SIZE) -> np.ndarray:
    slot = ensure_well_reference(current_slot)
    if slot.shape[0] != size or slot.shape[1] != size:
        slot = resize_rgba(slot, (size, size))

    border, well_r = prepare_border(border_source, size)
    background = extract_well_background(slot, well_r)
    return alpha_composite(background, border), well_r


def main() -> None:
    border_source = resolve_border_source()
    if not CURRENT_SLOT.is_file():
        raise FileNotFoundError(f"Missing current slot texture: {CURRENT_SLOT}")

    print(f"Perk slot border <- {border_source.name}")
    print(f"  background from {CURRENT_SLOT.name}")

    out, well_r = build_slot(border_source, CURRENT_SLOT)
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    Image.fromarray(out, mode="RGBA").save(OUTPUT, optimize=True)
    print(f"  {OUTPUT.name} — inner aperture {well_r:.1f}px @ {out.shape[1]}x{out.shape[0]}")
    print("Done. Reimport Textures/Perk in Content Browser.")


if __name__ == "__main__":
    main()

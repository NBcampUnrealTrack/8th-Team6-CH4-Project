"""
Build delivery progress bar textures from Photoroom-matted source art.

Sources (RGBA, pre-matted):
  SourceArt/HUD/누끼/image-Photoroom (5).png              -> 0/10 empty bar
  SourceArt/HUD/누끼/ProgressBar_Gen_Full_10Slots-Photoroom.png -> 10/10 full bar
  Intermediate states composite empty + filled slot glows.

Usage:
  python Scripts/ImportDeliveryProgressBar.py
"""

from __future__ import annotations

from pathlib import Path

import numpy as np
from PIL import Image

SOURCE_DIR = Path(r"E:\Unreal\git\SpaCh4\SourceArt\HUD\누끼")
EMPTY_SOURCE = SOURCE_DIR / "image-Photoroom (5).png"
FULL_SOURCE = SOURCE_DIR / "ProgressBar_Gen_Full_10Slots-Photoroom.png"
OUTPUT_DIR = Path(
    r"E:\Unreal\git\SpaCh4\Content\UI\HUD\Textures\Delivery"
)
SEGMENTS = 10
OUTPUT_SIZE = (560, 95)

# Inner slot grid margins on the cropped bar (tuned against 1536px source art).
SLOT_MARGINS = (48, 48, 38, 38)  # left, right, top, bottom


def clean_photoroom(rgba: np.ndarray, dark: float = 28.0) -> np.ndarray:
    """Strip Photoroom's leftover dark backdrop from RGBA."""
    out = rgba.copy()
    rgb = out[:, :, :3].astype(np.float32)
    alpha = out[:, :, 3].astype(np.float32)
    lum = rgb.mean(axis=2)
    green = (rgb[:, :, 1] > rgb[:, :, 0] + 25) & (rgb[:, :, 1] > rgb[:, :, 2] + 15)

    backdrop = (lum < dark) & (~green)
    out[backdrop, 3] = 0
    out[backdrop, :3] = 0

    fringe = (lum < 55) & (~green) & (alpha > 0)
    out[fringe, 3] = np.clip(alpha[fringe] * (lum[fringe] - 10.0) / 45.0, 0, 255).astype(
        np.uint8
    )
    return out


def opaque_bounds(rgba: np.ndarray, threshold: int = 40) -> tuple[int, int, int, int]:
    alpha = rgba[:, :, 3]
    ys, xs = np.where(alpha > threshold)
    if ys.size == 0:
        h, w = rgba.shape[:2]
        return 0, 0, w, h
    return int(xs.min()), int(ys.min()), int(xs.max()) + 1, int(ys.max()) + 1


def crop_bar(rgba: np.ndarray) -> np.ndarray:
    x0, y0, x1, y1 = opaque_bounds(rgba)
    return rgba[y0:y1, x0:x1].copy()


def resize_bar(rgba: np.ndarray, width: int, height: int) -> np.ndarray:
    rgb = Image.fromarray(rgba[:, :, :3], mode="RGB").resize(
        (width, height), Image.Resampling.LANCZOS
    )
    alpha = Image.fromarray(rgba[:, :, 3], mode="L").resize(
        (width, height), Image.Resampling.BILINEAR
    )
    out = np.dstack([np.array(rgb), np.array(alpha)]).astype(np.uint8)
    out[out[:, :, 3] < 8, :3] = 0
    return out


def alpha_over(base: np.ndarray, layer: np.ndarray) -> np.ndarray:
    out = base.copy().astype(np.float32)
    layer_alpha = layer[:, :, 3:4].astype(np.float32) / 255.0
    out[:, :, :3] = layer[:, :, :3] * layer_alpha + out[:, :, :3] * (1.0 - layer_alpha)
    base_alpha = out[:, :, 3:4].astype(np.float32) / 255.0
    combined = layer_alpha + base_alpha * (1.0 - layer_alpha)
    out[:, :, 3] = np.clip(combined[:, :, 0] * 255.0, 0, 255)
    return out.astype(np.uint8)


def slot_rects(width: int, height: int, count: int) -> list[tuple[int, int, int, int]]:
    ml, mr, mt, mb = SLOT_MARGINS
    inner_w = width - ml - mr
    slot_w = inner_w / count
    y0, y1 = mt, height - mb
    return [
        (int(round(ml + i * slot_w)), int(round(ml + (i + 1) * slot_w)), y0, y1)
        for i in range(count)
    ]


def extract_fill_texture(full_bar: np.ndarray) -> np.ndarray:
    """Green slot glow only — transparent elsewhere, for animated UI material fill."""
    out = np.zeros_like(full_bar)
    for x0, x1, y0, y1 in slot_rects(full_bar.shape[1], full_bar.shape[0], SEGMENTS):
        glow = full_bar[y0:y1, x0:x1]
        out[y0:y1, x0:x1] = alpha_over(out[y0:y1, x0:x1], glow)
    return out


def compose_bar(empty_bar: np.ndarray, full_bar: np.ndarray, filled: int) -> np.ndarray:
    if filled <= 0:
        return empty_bar.copy()
    if filled >= SEGMENTS:
        out = empty_bar.copy()
        ml, mr, mt, mb = SLOT_MARGINS
        x0, x1, y0, y1 = ml, empty_bar.shape[1] - mr, mt, empty_bar.shape[0] - mb
        out[y0:y1, x0:x1] = alpha_over(out[y0:y1, x0:x1], full_bar[y0:y1, x0:x1])
        return out

    out = empty_bar.copy()
    for i, (x0, x1, y0, y1) in enumerate(slot_rects(empty_bar.shape[1], empty_bar.shape[0], SEGMENTS)):
        if i >= filled:
            break
        glow = full_bar[y0:y1, x0:x1]
        out[y0:y1, x0:x1] = alpha_over(out[y0:y1, x0:x1], glow)
    return out


def load_bars() -> tuple[np.ndarray, np.ndarray]:
    if not EMPTY_SOURCE.exists():
        raise FileNotFoundError(EMPTY_SOURCE)
    if not FULL_SOURCE.exists():
        raise FileNotFoundError(FULL_SOURCE)

    empty = clean_photoroom(np.array(Image.open(EMPTY_SOURCE).convert("RGBA")))
    full = clean_photoroom(np.array(Image.open(FULL_SOURCE).convert("RGBA")))

    empty_bar = crop_bar(empty)
    full_bar = crop_bar(full)
    full_bar = np.array(
        Image.fromarray(full_bar, mode="RGBA").resize(
            (empty_bar.shape[1], empty_bar.shape[0]), Image.Resampling.LANCZOS
        )
    )
    return empty_bar, full_bar


def save_png(path: Path, rgba: np.ndarray) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    Image.fromarray(rgba, mode="RGBA").save(path, optimize=True)
    print(f"  {path.name} ({rgba.shape[1]}x{rgba.shape[0]})")


def main() -> None:
    empty_bar, full_bar = load_bars()
    width, height = OUTPUT_SIZE

    print("Delivery progress bar textures (Photoroom sources):")
    fill_native = extract_fill_texture(full_bar)
    fill_out = resize_bar(fill_native, width, height)
    save_png(OUTPUT_DIR / "T_HUD_Delivery_Fill.png", fill_out)

    for filled in range(SEGMENTS + 1):
        native = compose_bar(empty_bar, full_bar, filled)
        out = resize_bar(native, width, height)
        save_png(OUTPUT_DIR / f"T_HUD_Delivery_Progress_{filled:02d}.png", out)
        if filled == 0:
            save_png(OUTPUT_DIR / "T_HUD_Delivery_Progress_BG.png", out)

    print("Done. Reimport all T_HUD_Delivery_Progress_*.png in Content Browser.")


if __name__ == "__main__":
    main()

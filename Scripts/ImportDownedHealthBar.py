"""
Import downed health bar textures from Photoroom-matted source art.

Sources:
  SourceArt/HUD/누끼/체력바/프레임.png -> T_HUD_Bar_BG (track + heart frame)
  SourceArt/HUD/누끼/체력바/체력바.png -> T_HUD_Bar_Fill_Downed (red fill)

Usage:
  python Scripts/ImportDownedHealthBar.py
"""

from __future__ import annotations

from pathlib import Path

import numpy as np
from PIL import Image

SOURCE_DIR = Path(r"E:\Unreal\git\SpaCh4\SourceArt\HUD\누끼\체력바")
BG_OUTPUT = Path(r"E:\Unreal\git\SpaCh4\Content\UI\HUD\Textures\Common\T_HUD_Bar_BG.png")
FILL_OUTPUT = Path(
    r"E:\Unreal\git\SpaCh4\Content\UI\HUD\Textures\Teammate\T_HUD_Bar_Fill_Downed.png"
)
OUTPUT_SIZE = (560, 60)


def resolve_sources() -> tuple[Path, Path]:
    pngs = sorted(SOURCE_DIR.glob("*.png"), key=lambda p: Image.open(p).size[1])
    if len(pngs) < 2:
        raise FileNotFoundError(f"Expected 2 PNGs in {SOURCE_DIR}")
    # Shorter image = red fill tube, taller = heart frame.
    fill_source, frame_source = pngs[0], pngs[1]
    return frame_source, fill_source


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


def content_bounds(alpha: np.ndarray, threshold: int = 16, padding: int = 4) -> tuple[int, int, int, int]:
    ys, xs = np.where(alpha > threshold)
    if ys.size == 0:
        h, w = alpha.shape
        return 0, h, 0, w
    y0 = max(int(ys.min()) - padding, 0)
    y1 = min(int(ys.max()) + padding + 1, alpha.shape[0])
    x0 = max(int(xs.min()) - padding, 0)
    x1 = min(int(xs.max()) + padding + 1, alpha.shape[1])
    return y0, y1, x0, x1


def resize_rgba(rgba: np.ndarray, size: tuple[int, int]) -> np.ndarray:
    width, height = size
    rgb = Image.fromarray(rgba[:, :, :3], mode="RGB").resize(
        (width, height), Image.Resampling.LANCZOS
    )
    alpha = Image.fromarray(rgba[:, :, 3], mode="L").resize(
        (width, height), Image.Resampling.BILINEAR
    )
    out = np.dstack([np.array(rgb), np.array(alpha)]).astype(np.uint8)
    out[out[:, :, 3] < 8, :3] = 0
    return out


def import_bar(source: Path) -> np.ndarray:
    if not source.exists():
        raise FileNotFoundError(source)
    rgba = clean_photoroom(np.array(Image.open(source).convert("RGBA")))
    y0, y1, x0, x1 = content_bounds(rgba[:, :, 3])
    cropped = rgba[y0:y1, x0:x1]
    return resize_rgba(cropped, OUTPUT_SIZE)


def save_rgba(path: Path, rgba: np.ndarray) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    Image.fromarray(rgba, mode="RGBA").save(path, optimize=True)
    print(f"  {path.name} ({rgba.shape[1]}x{rgba.shape[0]})")


def main() -> None:
    frame_source, fill_source = resolve_sources()
    print("Downed health bar textures (Photoroom sources):")
    print(f"  frame <- {frame_source.name}")
    print(f"  fill  <- {fill_source.name}")
    save_rgba(BG_OUTPUT, import_bar(frame_source))
    save_rgba(FILL_OUTPUT, import_bar(fill_source))
    print("Done. Reimport T_HUD_Bar_BG + T_HUD_Bar_Fill_Downed in Content Browser.")


if __name__ == "__main__":
    main()

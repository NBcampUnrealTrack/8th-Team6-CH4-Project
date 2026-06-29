"""
Import survivor teammate portrait icons from Photoroom-matted source art.

Sources:
  SourceArt/HUD/누끼/캐릭터/노멀.png  -> healthy / placeholder / robot slots
  SourceArt/HUD/누끼/캐릭터/부상.png  -> injured + downed (optional)
  SourceArt/HUD/누끼/캐릭터/사망.png  -> dead (optional)

Usage:
  python Scripts/ImportSurvivorPortraitIcons.py
"""

from __future__ import annotations

from pathlib import Path

import numpy as np
from PIL import Image

SOURCE_DIR = Path(r"E:\Unreal\git\SpaCh4\SourceArt\HUD\누끼\캐릭터")
OUTPUT_DIR = Path(r"E:\Unreal\git\SpaCh4\Content\UI\HUD\Textures\Teammate")
TARGET_SIZE = 256
WORK_SIZE = 512

NORMAL_SOURCE = SOURCE_DIR / "노멀.png"
INJURED_SOURCE = SOURCE_DIR / "부상.png"
DEAD_SOURCE = SOURCE_DIR / "사망.png"

OUTPUTS = {
    NORMAL_SOURCE: [
        "T_HUD_Portrait_Placeholder.png",
        "T_HUD_Portrait_Robot_A.png",
        "T_HUD_Portrait_Robot_B.png",
        "T_HUD_Portrait_Robot_C.png",
    ],
    INJURED_SOURCE: ["T_HUD_Portrait_Injured.png"],
    DEAD_SOURCE: ["T_HUD_Portrait_Dead.png"],
}


def clean_photoroom(rgba: np.ndarray, dark: float = 24.0) -> np.ndarray:
    out = rgba.copy()
    rgb = out[:, :, :3].astype(np.float32)
    alpha = out[:, :, 3].astype(np.float32)
    lum = rgb.mean(axis=2)

    backdrop = lum < dark
    out[backdrop, 3] = 0
    out[backdrop, :3] = 0

    fringe = (lum < 50) & (alpha > 0)
    out[fringe, 3] = np.clip(alpha[fringe] * (lum[fringe] - 8.0) / 42.0, 0, 255).astype(
        np.uint8
    )
    return out


def content_bounds(alpha: np.ndarray, padding: int = 8) -> tuple[int, int, int, int]:
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


def import_icon(source: Path) -> np.ndarray:
    rgba = clean_photoroom(np.array(Image.open(source).convert("RGBA")))
    y0, y1, x0, x1 = content_bounds(rgba[:, :, 3])
    cropped = rgba[y0:y1, x0:x1]
    work = resize_square(cropped, WORK_SIZE)
    return resize_square(work, TARGET_SIZE)


def save_rgba(path: Path, rgba: np.ndarray) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    Image.fromarray(rgba, mode="RGBA").save(path, optimize=True)
    print(f"  {path.name} ({rgba.shape[1]}x{rgba.shape[0]})")


def main() -> None:
    if not NORMAL_SOURCE.exists():
        raise FileNotFoundError(NORMAL_SOURCE)

    print("Survivor portrait icons (Photoroom sources):")
    for source, names in OUTPUTS.items():
        if not source.exists():
            print(f"  skip missing {source.name}")
            continue
        icon = import_icon(source)
        for name in names:
            save_rgba(OUTPUT_DIR / name, icon)

    print("Done. Reimport Textures/Teammate in Content Browser.")


if __name__ == "__main__":
    main()

"""
Import delivery row label icons (A/B) from Photoroom PNGs.

Usage:
  python Scripts/ImportDeliveryLabelIcons.py

Outputs:
  Content/UI/HUD/Textures/Delivery/T_HUD_Delivery_Label_A.png
  Content/UI/HUD/Textures/Delivery/T_HUD_Delivery_Label_B.png
"""

from __future__ import annotations

import shutil
from pathlib import Path

import numpy as np
from PIL import Image

REFERENCE_A = Path(
    r"C:\Users\TaitoP\.cursor\projects\e-Unreal-git-SpaCh4\assets"
    r"\c__Users_TaitoP_AppData_Roaming_Cursor_User_workspaceStorage_empty-window_images"
    r"_image-Photoroom-774f5bdd-831a-4e3b-80b7-a288461940e7.png"
)
REFERENCE_B = Path(
    r"C:\Users\TaitoP\.cursor\projects\e-Unreal-git-SpaCh4\assets"
    r"\c__Users_TaitoP_AppData_Roaming_Cursor_User_workspaceStorage_empty-window_images"
    r"_image-Photoroom__1_-357176d4-8402-4630-8b9b-db9f8037162f.png"
)
FALLBACK_A = Path(r"E:\Unreal\git\SpaCh4\Scripts\Reference_DeliveryLabel_A_Source.png")
FALLBACK_B = Path(r"E:\Unreal\git\SpaCh4\Scripts\Reference_DeliveryLabel_B_Source.png")
OUTPUT_DIR = Path(
    r"E:\Unreal\git\SpaCh4\Content\UI\HUD\Textures\Delivery"
)
TARGET_SIZE = 96
WORK_SIZE = 384


def resolve_reference(primary: Path, fallback: Path) -> Path:
    if primary.exists():
        return primary
    if fallback.exists():
        return fallback
    raise FileNotFoundError(f"Reference not found: {primary} or {fallback}")


def content_bounds(alpha: np.ndarray, padding: int = 6) -> tuple[int, int, int, int]:
    ys, xs = np.where(alpha > 8)
    if ys.size == 0:
        h, w = alpha.shape
        return 0, h, 0, w
    y0 = max(int(ys.min()) - padding, 0)
    y1 = min(int(ys.max()) + padding + 1, alpha.shape[0])
    x0 = max(int(xs.min()) - padding, 0)
    x1 = min(int(xs.max()) + padding + 1, alpha.shape[1])
    return y0, y1, x0, x1


def resize_rgba(rgba: np.ndarray, size: int) -> np.ndarray:
    rgb = Image.fromarray(rgba[:, :, :3], mode="RGB").resize(
        (size, size), Image.Resampling.LANCZOS
    )
    alpha = Image.fromarray(rgba[:, :, 3], mode="L").resize(
        (size, size), Image.Resampling.BILINEAR
    )
    out = np.dstack([np.array(rgb), np.array(alpha)]).astype(np.uint8)
    transparent = out[:, :, 3] < 8
    out[transparent, :3] = 0
    return out


def import_icon(source: Path, size: int) -> np.ndarray:
    rgba = np.array(Image.open(source).convert("RGBA"))
    y0, y1, x0, x1 = content_bounds(rgba[:, :, 3], padding=6)
    cropped = rgba[y0:y1, x0:x1]
    work = resize_rgba(cropped, WORK_SIZE)
    return resize_rgba(work, size)


def save_rgba(path: Path, rgba: np.ndarray) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    Image.fromarray(rgba, mode="RGBA").save(path)
    print(f"Saved {path} ({rgba.shape[1]}x{rgba.shape[0]}, RGBA)")


def main() -> None:
    source_a = resolve_reference(REFERENCE_A, FALLBACK_A)
    source_b = resolve_reference(REFERENCE_B, FALLBACK_B)

    if source_a == REFERENCE_A and not FALLBACK_A.exists():
        shutil.copy2(source_a, FALLBACK_A)
    if source_b == REFERENCE_B and not FALLBACK_B.exists():
        shutil.copy2(source_b, FALLBACK_B)

    save_rgba(OUTPUT_DIR / "T_HUD_Delivery_Label_A.png", import_icon(source_a, TARGET_SIZE))
    save_rgba(OUTPUT_DIR / "T_HUD_Delivery_Label_B.png", import_icon(source_b, TARGET_SIZE))


if __name__ == "__main__":
    main()

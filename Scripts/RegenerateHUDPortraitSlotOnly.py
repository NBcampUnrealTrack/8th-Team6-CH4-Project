"""
Portrait icon wrap — corner brackets only (transparent center, no panel fill).

Run: python Scripts/RegenerateHUDPortraitSlotOnly.py
Then Reimport Content/UI/HUD/Textures/Teammate in editor.
"""

from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(r"E:\Unreal\git\SpaCh4_Copy\Content\UI\HUD")
BRACKET = (190, 198, 212, 220)
BRACKET_HI = (230, 236, 248, 240)
SIZE = 96
SUPERSAMPLE = 6


def new(size):
    return Image.new("RGBA", size, (0, 0, 0, 0))


def save(path, image):
    path.parent.mkdir(parents=True, exist_ok=True)
    image.save(path)


def upscale(image, size):
    return image.resize(size, Image.Resampling.LANCZOS)


def portrait_icon_wrap(size=(96, 96)):
    """TechUI-style corner brackets — wraps icon only, nothing else."""
    scale = SUPERSAMPLE
    s = size[0] * scale
    canvas = new((s, s))
    draw = ImageDraw.Draw(canvas)

    inset = 4 * scale
    arm = 14 * scale
    lw = max(2, scale // 2)
    diag = 10 * scale

    corners = [
        (inset, inset, 1, 1),
        (s - inset, inset, -1, 1),
        (inset, s - inset, 1, -1),
        (s - inset, s - inset, -1, -1),
    ]
    for cx, cy, dx, dy in corners:
        # L-bracket
        draw.line((cx, cy, cx + dx * arm, cy), fill=BRACKET_HI, width=lw)
        draw.line((cx, cy, cx, cy + dy * arm), fill=BRACKET_HI, width=lw)
        # Diagonal chamfer tick
        draw.line((cx, cy + dy * diag, cx + dx * diag, cy), fill=BRACKET, width=max(1, lw - 1))

    # Subtle outer hairline (square, not filled)
    pad = 2 * scale
    draw.rounded_rectangle(
        (pad, pad, s - pad - 1, s - pad - 1),
        3 * scale,
        outline=(120, 128, 142, 80),
        width=max(1, scale // 3),
    )

    return upscale(canvas, size)


def main():
    out = ROOT / "Textures/Teammate/T_HUD_Portrait_Slot.png"
    save(out, portrait_icon_wrap((SIZE, SIZE)))
    print("Portrait icon wrap written. Reimport Textures/Teammate.")


if __name__ == "__main__":
    main()

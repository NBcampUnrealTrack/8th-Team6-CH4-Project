"""
Delivery station row labels — separate A and B image icons.

Run: python Scripts/RegenerateHUDDeliveryLabelsOnly.py

Reimport in Unreal (manual — do NOT use MCP run-python-script; it breaks the debugger):
  Content/UI/HUD/Textures/Delivery/T_HUD_Delivery_Label_A.png
  Content/UI/HUD/Textures/Delivery/T_HUD_Delivery_Label_B.png
Then: powershell -ExecutionPolicy Bypass -File Scripts/ApplyDeliveryLabelImages.ps1
"""

from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(r"E:\Unreal\git\SpaCh4_Copy\Content\UI\HUD")

ICON = (248, 250, 252, 255)
OUTLINE = (8, 10, 14, 220)
CHROME = (12, 13, 16, 255)
CHROME_HI = (42, 46, 54, 255)
CHROME_EDGE = (255, 255, 255, 48)
SLOT_VOID = (36, 39, 46, 255)
BLUE = (110, 160, 220, 255)

SIZE = 96
SUPERSAMPLE = 4


def new(size):
    return Image.new("RGBA", size, (0, 0, 0, 0))


def save(path, image):
    path.parent.mkdir(parents=True, exist_ok=True)
    image.save(path)
    print(f"  {path.relative_to(ROOT.parent.parent)}")


def upscale(image, size):
    return image.resize(size, Image.Resampling.LANCZOS)


def hud_bevel_slot(size=(96, 96), inner=SLOT_VOID):
    scale = SUPERSAMPLE
    big = (size[0] * scale, size[1] * scale)
    image = new(big)
    draw = ImageDraw.Draw(image)
    pad = 10 * scale
    box = (pad, pad, big[0] - pad - 1, big[1] - pad - 1)
    radius = max(8, scale * 2)
    draw.rounded_rectangle(box, radius, fill=CHROME)
    draw.rounded_rectangle(
        (box[0] + scale, box[1] + scale, box[2] - scale, box[3] - scale),
        max(2, radius - scale),
        fill=inner,
    )
    draw.line(
        (box[0] + 8 * scale, box[1] + 5 * scale, box[2] - 8 * scale, box[1] + 5 * scale),
        fill=CHROME_HI,
    )
    draw.line(
        (box[0] + 8 * scale, box[3] - 5 * scale, box[2] - 8 * scale, box[3] - 5 * scale),
        fill=(0, 0, 0, 80),
    )
    draw.rounded_rectangle(box, radius, outline=CHROME_EDGE, width=max(1, scale // 2))
    return upscale(image, size)


def load_label_font(pixel_size):
    candidates = [
        Path(r"C:\Windows\Fonts\arialbd.ttf"),
        Path(r"C:\Windows\Fonts\segoeuib.ttf"),
        Path(r"C:\Windows\Fonts\calibrib.ttf"),
        Path(r"C:\Windows\Fonts\bahnschrift.ttf"),
    ]
    for path in candidates:
        if path.exists():
            return ImageFont.truetype(str(path), pixel_size)
    return ImageFont.load_default()


def delivery_label_icon(letter: str, accent: tuple, size=(96, 96)):
    scale = SUPERSAMPLE
    s = size[0] * scale
    base = hud_bevel_slot(size)

    letter_layer = new((s, s))
    draw = ImageDraw.Draw(letter_layer)
    font = load_label_font(int(54 * scale))

    bbox = draw.textbbox((0, 0), letter, font=font)
    tw = bbox[2] - bbox[0]
    th = bbox[3] - bbox[1]
    x = (s - tw) // 2 - bbox[0]
    y = (s - th) // 2 - bbox[1] - int(2 * scale)

    draw.text((x + scale, y + scale), letter, font=font, fill=OUTLINE)
    draw.text((x, y), letter, font=font, fill=ICON)

    accent_layer = new((s, s))
    ad = ImageDraw.Draw(accent_layer)
    bar_y = s - int(18 * scale)
    ad.rounded_rectangle(
        (int(22 * scale), bar_y, s - int(22 * scale), bar_y + int(4 * scale)),
        int(2 * scale),
        fill=accent,
    )
    ad.ellipse(
        (s // 2 - int(3 * scale), int(14 * scale), s // 2 + int(3 * scale), int(20 * scale)),
        fill=accent,
    )

    out = base.copy()
    out.alpha_composite(upscale(letter_layer, size))
    out.alpha_composite(upscale(accent_layer, size))
    return out


def main():
    out_dir = ROOT / "Textures/Delivery"
    style = BLUE
    print("Delivery label PNGs (matching A/B design):" )
    save(out_dir / "T_HUD_Delivery_Label_A.png", delivery_label_icon("A", style, (SIZE, SIZE)))
    save(out_dir / "T_HUD_Delivery_Label_B.png", delivery_label_icon("B", style, (SIZE, SIZE)))
    print("Done. Reimport both PNGs in Content Browser.")


if __name__ == "__main__":
    main()

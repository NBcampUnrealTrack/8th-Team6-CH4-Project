"""
HUD panel border / frame textures — incremental quality pass.

References:
- TechUI ScifiPanel B3: diagonal chamfer corner marks, glass center, minimal chrome
  https://techui.net/docs/en/tuiPrime/scifiPanel/b3.html
- Sci-fi trim sheets: bevel edge strips catch top-left highlights (PixelSanctuary)
- Substance sci-fi control panels: brushed steel shell + beveled inset + hairline rim
- DBD Status HUD: muted gray chrome, no heavy black boxes — content reads first

9-slice: all variants use BORDER_PX=8 frame — set UE texture margins to 8 on each side.

Run: python Scripts/RegenerateHUDBordersOnly.py
Then Reimport Content/UI/HUD/Textures/Common (+ Teammate) in editor (no MCP import).
"""

from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

ROOT = Path(r"E:\Unreal\git\SpaCh4_Copy\Content\UI\HUD")

# Opacity — inner glass ~30%, frame slightly stronger so rim still reads
BG_OPACITY = 0.30
FRAME_OPACITY = 0.45

SHELL_OUTER = (22, 26, 34, int(255 * FRAME_OPACITY))
SHELL_MID = (88, 96, 110, int(255 * FRAME_OPACITY))
SHELL_HI = (178, 186, 200, int(255 * FRAME_OPACITY))
SHELL_LO = (32, 36, 44, int(255 * FRAME_OPACITY))
RIM_OUTER = (130, 138, 152, int(255 * FRAME_OPACITY))
RIM_INNER = (255, 255, 255, int(255 * BG_OPACITY * 0.5))
GLASS_FILL = (6, 8, 12, int(255 * BG_OPACITY))
GLASS_VIGNETTE = (0, 0, 0, int(255 * BG_OPACITY * 0.35))
CHAMFER = (200, 208, 220, int(255 * FRAME_OPACITY * 0.85))
CHAMFER_DIM = (120, 128, 142, int(255 * FRAME_OPACITY * 0.55))

BORDER_PX = 8
CORNER_RADIUS = 5
CHAMFER_LEN_PX = 10
SUPERSAMPLE = 6


def new(size):
    return Image.new("RGBA", size, (0, 0, 0, 0))


def save(path, image):
    path.parent.mkdir(parents=True, exist_ok=True)
    image.save(path)


def upscale(image, size):
    return image.resize(size, Image.Resampling.LANCZOS)


def _clamp(v, lo=0, hi=255):
    return max(lo, min(hi, v))


def _brush_noise(image, box, amount=11, step=2, seed=0):
    import random

    rng = random.Random(seed)
    px = image.load()
    x0, y0, x1, y1 = box
    for y in range(y0, y1, step):
        for x in range(x0, x1, step):
            r, g, b, a = px[x, y]
            n = rng.randint(-amount, amount)
            px[x, y] = (_clamp(r + n), _clamp(g + n), _clamp(b + n), a)


def _draw_chamfer_corners(draw, w, h, inset, length, scale, bright=True):
    """TechUI-style diagonal chamfer marks at four corners."""
    color = CHAMFER if bright else CHAMFER_DIM
    lw = max(1, scale // 3)
    corners = [
        (inset, inset, 1, 1),
        (w - inset, inset, -1, 1),
        (inset, h - inset, 1, -1),
        (w - inset, h - inset, -1, -1),
    ]
    for cx, cy, dx, dy in corners:
        # Diagonal slit along corner bisector
        draw.line(
            (cx, cy + dy * length, cx + dx * length, cy),
            fill=color,
            width=lw,
        )
        # Edge ticks into frame
        tick = length // 2
        draw.line((cx, cy, cx + dx * tick, cy), fill=(color[0], color[1], color[2], color[3] // 2), width=lw)
        draw.line((cx, cy, cx, cy + dy * tick), fill=(color[0], color[1], color[2], color[3] // 2), width=lw)


def hud_panel_border(size, radius=CORNER_RADIUS, border=BORDER_PX, variant="square", seed=0):
    """
    Brushed steel frame + dark glass inset.
    Border thickness is fixed in pixels so 9-slice margins stay consistent when stretched.
    """
    scale = SUPERSAMPLE
    w, h = size[0] * scale, size[1] * scale
    b = border * scale
    r = max(2, radius * scale)
    canvas = new((w, h))
    draw = ImageDraw.Draw(canvas)
    outer = (0, 0, w - 1, h - 1)

    # --- Thin steel shell (single bevel band, not stacked rings) ---
    draw.rounded_rectangle(outer, r, fill=SHELL_OUTER)
    frame = (b // 3, b // 3, w - b // 3 - 1, h - b // 3 - 1)
    draw.rounded_rectangle(frame, max(2, r - scale // 3), fill=SHELL_MID)
    _brush_noise(canvas, frame, amount=8, seed=seed + 11)

    inner = (b, b, w - b - 1, h - b - 1)
    draw.rounded_rectangle(inner, max(2, r - scale // 2), fill=SHELL_LO)

    # Hairline specular / shadow on frame edge
    spec_a = int(255 * FRAME_OPACITY * 0.35)
    shadow_a = int(255 * FRAME_OPACITY * 0.55)
    draw.line(
        (b, b, w - b, b),
        fill=(255, 255, 255, spec_a),
        width=max(1, scale // 3),
    )
    draw.line(
        (b, h - b, w - b, h - b),
        fill=(0, 0, 0, shadow_a),
        width=max(1, scale // 3),
    )

    # --- Glass well ---
    inner = (b + scale // 2, b + scale // 2, w - b - scale // 2 - 1, h - b - scale // 2 - 1)
    draw.rounded_rectangle(inner, max(2, r - scale), fill=GLASS_FILL)

    # Inner vignette (subtle depth)
    vignette = new((w, h))
    vd = ImageDraw.Draw(vignette)
    vd.rounded_rectangle(inner, max(2, r - scale), fill=GLASS_VIGNETTE)
    vignette = vignette.filter(ImageFilter.GaussianBlur(max(2, scale * 2)))
    canvas.alpha_composite(vignette)
    draw = ImageDraw.Draw(canvas)
    draw.rounded_rectangle(inner, max(2, r - scale), fill=GLASS_FILL)

    # Bar panels: faint horizontal groove (control-panel seam)
    if variant == "bar":
        gy = b + 3 * scale
        draw.line(
            (inner[0] + 4 * scale, gy, inner[2] - 4 * scale, gy),
            fill=(255, 255, 255, int(255 * BG_OPACITY * 0.25)),
            width=max(1, scale // 3),
        )

    # Hairline rims
    draw.rounded_rectangle(inner, max(2, r - scale), outline=RIM_INNER, width=max(1, scale // 3))
    draw.rounded_rectangle(outer, r, outline=RIM_OUTER, width=max(1, scale // 2))

    # TechUI diagonal chamfer corner marks (inside frame)
    chamfer_inset = b + 2 * scale
    chamfer_len = CHAMFER_LEN_PX * scale
    _draw_chamfer_corners(draw, w, h, chamfer_inset, chamfer_len, scale)

    return upscale(canvas, size)


def hud_divider(size=(6, 128)):
    """Vertical beveled separator — thin metallic strip."""
    scale = SUPERSAMPLE
    w, h = size[0] * scale, size[1] * scale
    canvas = new((w, h))
    draw = ImageDraw.Draw(canvas)
    cx = w // 2
    draw.rectangle((0, 0, w - 1, h - 1), fill=SHELL_LO)
    draw.rectangle((scale, 0, w - scale - 1, h - 1), fill=SHELL_MID)
    _brush_noise(canvas, (scale, 0, w - scale, h), amount=8, seed=77)
    draw.line((cx - scale // 2, 0, cx - scale // 2, h - 1), fill=SHELL_HI, width=max(1, scale // 2))
    draw.line((cx + scale // 2, 0, cx + scale // 2, h - 1), fill=(0, 0, 0, 80), width=max(1, scale // 3))
    return upscale(canvas, size)


def main():
    common = ROOT / "Textures/Common"
    teammate = ROOT / "Textures/Teammate"

    save(common / "T_HUD_Panel_BG.png", hud_panel_border((160, 160), variant="square", seed=1))
    save(common / "T_HUD_Panel_BG_Wide.png", hud_panel_border((308, 252), variant="square", seed=2))
    save(common / "T_HUD_Panel_BG_Bar.png", hud_panel_border((380, 108), variant="bar", seed=3))
    save(teammate / "T_HUD_Entry_Row_BG.png", hud_panel_border((308, 72), variant="bar", seed=4))
    save(common / "T_HUD_Divider.png", hud_divider((6, 128)))

    print("Border PNGs written.")
    print(f"  Inner glass opacity: {int(BG_OPACITY * 100)}%, frame opacity: {int(FRAME_OPACITY * 100)}%")
    print(f"  9-slice margin: {BORDER_PX}px on all sides (per texture import settings).")
    print("  Reimport Textures/Common and Textures/Teammate in Unreal Editor.")


if __name__ == "__main__":
    main()

"""
HUD slot textures only — incremental quality pass.

References:
- DBD survivor item bar: dark rounded square wells, minimal chrome, white icons inside
- ELV Games Free Sci-Fi Inventory: 9-slice cell, recessed center, beveled metal rim
- Dark SCI-FI UI Pack (Unity): thin frame + separate inner/outer, hover brightens rim

Run: python Scripts/RegenerateHUDSlotsOnly.py
Then Reimport Content/UI/HUD/Textures/Inventory and .../Perk in editor (no MCP import).
"""

from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

ROOT = Path(r"E:\Unreal\git\SpaCh4_Copy\Content\UI\HUD")

# Slot palette — dark chrome well + steel rim (no neon cyan)
FRAME_OUTER = (26, 30, 38, 255)
FRAME_MID = (96, 104, 118, 255)
FRAME_HI = (188, 196, 210, 255)
FRAME_LO = (18, 20, 26, 255)
RIM = (140, 148, 162, 220)
RIM_HOVER = (210, 218, 232, 255)
WELL_EMPTY = (8, 10, 14, 255)
WELL_HOVER = (14, 16, 22, 255)
WELL_FILLED = (20, 18, 14, 255)
ACCENT_GOLD = (196, 164, 88, 255)
ACCENT_COOLDOWN = (48, 52, 60, 255)

SUPERSAMPLE = 6


def new(size=(128, 128)):
    return Image.new("RGBA", size, (0, 0, 0, 0))


def save(path, image):
    path.parent.mkdir(parents=True, exist_ok=True)
    image.save(path)


def upscale(image, size):
    return image.resize(size, Image.Resampling.LANCZOS)


def _clamp(v, lo=0, hi=255):
    return max(lo, min(hi, v))


def _brush_noise(image, box, amount=10, step=2, seed=0):
    import random

    rng = random.Random(seed)
    px = image.load()
    x0, y0, x1, y1 = box
    for y in range(y0, y1, step):
        for x in range(x0, x1, step):
            r, g, b, a = px[x, y]
            n = rng.randint(-amount, amount)
            px[x, y] = (_clamp(r + n), _clamp(g + n), _clamp(b + n), a)


def _rounded_rect_mask(size, box, radius):
    scale = 1
    mask = Image.new("L", size, 0)
    ImageDraw.Draw(mask).rounded_rectangle(box, radius, fill=255)
    return mask


def _compose_masked(base_rgba, mask):
    out = new(base_rgba.size)
    out.paste(base_rgba, mask=mask)
    return out


def _slot_palette(state):
    if state == "hover":
        return WELL_HOVER, RIM_HOVER, (255, 255, 255, 55)
    if state == "filled":
        return WELL_FILLED, RIM, ACCENT_GOLD
    if state == "cooldown":
        return (12, 12, 14, 255), (90, 94, 102, 200), ACCENT_COOLDOWN
    if state == "equipped":
        return (16, 28, 22, 255), RIM, (90, 180, 120, 200)
    return WELL_EMPTY, RIM, (255, 255, 255, 38)


def hud_inventory_slot(size=(128, 128), state="empty"):
    """
    Square slot — DBD item-bar / sci-fi inventory cell.
    9-slice friendly: ~10px frame at 128px base.
    """
    scale = SUPERSAMPLE
    s = size[0] * scale
    frame = 10 * scale
    radius = 8 * scale
    well, rim_color, corner = _slot_palette(state)

    canvas = new((s, s))
    draw = ImageDraw.Draw(canvas)
    outer = (0, 0, s - 1, s - 1)

    # Steel shell
    draw.rounded_rectangle(outer, radius, fill=FRAME_OUTER)
    shell = (frame // 3, frame // 3, s - frame // 3 - 1, s - frame // 3 - 1)
    draw.rounded_rectangle(shell, max(2, radius - scale), fill=FRAME_MID)
    _brush_noise(canvas, shell, amount=9, seed=17 + hash(state) % 97)

    # Bevel
    bevel = (frame // 2, frame // 2, s - frame // 2 - 1, s - frame // 2 - 1)
    draw.rounded_rectangle(bevel, max(2, radius - scale // 2), fill=FRAME_HI)
    draw.rounded_rectangle(
        (frame, frame, s - frame - 1, s - frame - 1),
        max(2, radius - scale),
        fill=FRAME_LO,
    )

    # Recessed well
    inner = (frame + scale, frame + scale, s - frame - scale - 1, s - frame - scale - 1)
    draw.rounded_rectangle(inner, max(2, radius - scale), fill=well)

    # Inner shadow (top darker band)
    shadow = new((s, s))
    sd = ImageDraw.Draw(shadow)
    sd.rounded_rectangle(inner, max(2, radius - scale), fill=(0, 0, 0, 70))
    shadow = shadow.filter(ImageFilter.GaussianBlur(max(1, scale)))
    canvas.alpha_composite(shadow)

    # Well re-draw after shadow
    draw = ImageDraw.Draw(canvas)
    draw.rounded_rectangle(inner, max(2, radius - scale), fill=well)

    # Specular top edge
    draw.line(
        (inner[0] + 2 * scale, inner[1] + scale, inner[2] - 2 * scale, inner[1] + scale),
        fill=(255, 255, 255, 35 if state != "hover" else 55),
        width=max(1, scale // 2),
    )

    # Outer rim
    draw.rounded_rectangle(outer, radius, outline=rim_color, width=max(1, scale // 2))

    # Corner ticks (DBD-minimal bracket hint)
    tick = 11 * scale
    inset = frame + 2 * scale
    tick_w = max(1, scale // 2)
    corners = [
        (inset, inset, 1, 1),
        (s - inset, inset, -1, 1),
        (inset, s - inset, 1, -1),
        (s - inset, s - inset, -1, -1),
    ]
    for x0, y0, dx, dy in corners:
        draw.line((x0, y0, x0 + dx * tick, y0), fill=corner, width=tick_w)
        draw.line((x0, y0, x0, y0 + dy * tick), fill=corner, width=tick_w)

    if state == "filled":
        draw.rounded_rectangle(inner, max(2, radius - scale), outline=(196, 164, 88, 90), width=max(1, scale // 2))
    elif state == "equipped":
        draw.rounded_rectangle(inner, max(2, radius - scale), outline=(90, 180, 120, 100), width=max(1, scale // 2))

    return upscale(canvas, size)


def _diamond_points(cx, cy, r):
    return [(cx, cy - r), (cx + r, cy), (cx, cy + r), (cx - r, cy)]


def hud_perk_slot(size=(128, 128), state="empty"):
    """Diamond perk slot — same material language as inventory square."""
    scale = SUPERSAMPLE
    s = size[0] * scale
    cx = cy = s // 2
    r_outer = s // 2 - scale
    r_frame = r_outer - 6 * scale
    r_well = r_frame - 5 * scale
    well, rim_color, corner = _slot_palette(state if state != "equipped" else "equipped")

    canvas = new((s, s))
    draw = ImageDraw.Draw(canvas)

    # Outer diamond frame
    draw.polygon(_diamond_points(cx, cy, r_outer), fill=FRAME_OUTER)
    draw.polygon(_diamond_points(cx, cy, r_outer - scale), fill=FRAME_MID)
    draw.polygon(_diamond_points(cx, cy, r_frame), fill=FRAME_HI)
    draw.polygon(_diamond_points(cx, cy, r_frame - 2 * scale), fill=FRAME_LO)
    draw.polygon(_diamond_points(cx, cy, r_well), fill=well)

    # Rim stroke
    pts = _diamond_points(cx, cy, r_outer) + [_diamond_points(cx, cy, r_outer)[0]]
    draw.line(pts, fill=rim_color, width=max(2, scale))

    # Corner ticks on diamond vertices
    tick = 9 * scale
    for px, py in _diamond_points(cx, cy, r_well - 2 * scale):
        if py <= cy:
            draw.line((px, py, px, py - tick // 2), fill=corner, width=max(1, scale // 2))
        else:
            draw.line((px, py, px, py + tick // 2), fill=corner, width=max(1, scale // 2))

    if state == "cooldown":
        overlay = new((s, s))
        od = ImageDraw.Draw(overlay)
        od.polygon(_diamond_points(cx, cy, r_well), fill=(0, 0, 0, 120))
        canvas.alpha_composite(overlay)

    return upscale(canvas, size)


def main():
    inv = ROOT / "Textures/Inventory"
    perk = ROOT / "Textures/Perk"

    save(inv / "T_HUD_Inventory_Slot_Empty.png", hud_inventory_slot((128, 128), "empty"))
    save(inv / "T_HUD_Inventory_Slot_Hover.png", hud_inventory_slot((128, 128), "hover"))
    save(inv / "T_HUD_Inventory_Slot_Filled.png", hud_inventory_slot((128, 128), "filled"))

    save(perk / "T_HUD_Perk_Slot_Empty.png", hud_perk_slot((128, 128), "empty"))
    save(perk / "T_HUD_Perk_Slot_Equipped.png", hud_perk_slot((128, 128), "equipped"))
    save(perk / "T_HUD_Perk_Slot_Cooldown.png", hud_perk_slot((128, 128), "cooldown"))

    print("Slot PNGs written. Reimport Inventory + Perk folders in Unreal Editor.")


if __name__ == "__main__":
    main()

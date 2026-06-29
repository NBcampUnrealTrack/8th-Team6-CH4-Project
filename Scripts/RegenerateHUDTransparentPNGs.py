from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter, ImageFont


ROOT = Path(r"E:\Unreal\git\SpaCh4_Copy\Content\UI\HUD")

# DBD-style HUD: crisp white silhouettes, dark chrome UI chrome, readable green progress
ICON = (248, 250, 252, 255)
ICON_MID = (220, 224, 230, 255)
ICON_EDGE = (180, 186, 196, 255)
SHADOW = (0, 0, 0, 130)
OUTLINE = (8, 10, 14, 220)
CHROME = (12, 13, 16, 255)
CHROME_HI = (42, 46, 54, 255)
CHROME_EDGE = (255, 255, 255, 48)
SLOT_VOID = (36, 39, 46, 255)
SLOT_INSET = (24, 26, 32, 255)
PROGRESS = (72, 188, 108, 255)
PROGRESS_HI = (118, 228, 148, 255)
PROGRESS_LO = (48, 140, 78, 255)
DANGER = (220, 72, 72, 255)
WARN = (220, 176, 64, 255)
MUTED = (150, 156, 168, 255)
GOLD = (220, 186, 96, 255)

WHITE = ICON
CYAN = (140, 200, 230, 255)
CYAN_DIM = (100, 140, 170, 180)
CYAN_GLOW = (255, 255, 255, 30)
DARK = CHROME
DARK_MID = (22, 24, 28, 255)
YELLOW = WARN
RED = DANGER
GREEN = PROGRESS
BLUE = (110, 160, 220, 255)
PURPLE = (170, 130, 210, 255)
GRAY = MUTED
STACK_GREEN = PROGRESS
STACK_EMPTY = SLOT_INSET
STACK_TRACK = CHROME

SUPERSAMPLE = 6


def new(size=(128, 128)):
    return Image.new("RGBA", size, (0, 0, 0, 0))


def save(path, image):
    path.parent.mkdir(parents=True, exist_ok=True)
    image.save(path)


def upscale(image, size):
    return image.resize(size, Image.Resampling.LANCZOS)


def _scale(v, s, base=256):
    return int(v * s / base)


def dilate(mask, radius):
    if radius <= 0:
        return mask.copy()
    out = mask
    for _ in range(radius):
        out = out.filter(ImageFilter.MaxFilter(3))
    return out


def erode(mask, radius):
    if radius <= 0:
        return mask.copy()
    out = mask
    for _ in range(radius):
        out = out.filter(ImageFilter.MinFilter(3))
    return out


def compose_silhouette(paint_mask, size=(256, 256), fill=ICON, with_shadow=True):
    """Dual-stroke icon: dark rim + bright body + top highlight. Renders at 6x."""
    scale = SUPERSAMPLE
    s = size[0] * scale
    mask = Image.new("L", (s, s), 0)
    paint_mask(ImageDraw.Draw(mask), s)

    rim = dilate(mask, max(1, scale // 2))
    outer = dilate(mask, max(2, scale))

    image = new((s, s))
    if with_shadow:
        shadow = Image.new("RGBA", (s, s), SHADOW)
        shadow.putalpha(mask)
        shadow = shadow.filter(ImageFilter.GaussianBlur(max(2, scale)))
        image.alpha_composite(shadow, dest=(scale, scale * 2))

    rim_layer = Image.new("RGBA", (s, s), OUTLINE)
    rim_layer.putalpha(rim)
    image.alpha_composite(rim_layer)

    body = Image.new("RGBA", (s, s), fill)
    body.putalpha(mask)
    image.alpha_composite(body)

    highlight = Image.new("RGBA", (s, s), (255, 255, 255, 70))
    highlight.putalpha(erode(mask, max(1, scale // 3)))
    image.alpha_composite(highlight)

    edge_hi = Image.new("RGBA", (s, s), ICON_MID)
    edge_hi.putalpha(erode(mask, 1))
    image.alpha_composite(edge_hi)

    return upscale(image, size)


def draw_hud_brackets(draw, w, h, color=CYAN, length=16, inset=6, width=3):
    corners = [
        (inset, inset, 1, 1),
        (w - inset - 1, inset, -1, 1),
        (inset, h - inset - 1, 1, -1),
        (w - inset - 1, h - inset - 1, -1, -1),
    ]
    for x, y, dx, dy in corners:
        draw.line((x, y, x + dx * length, y), fill=color, width=width)
        draw.line((x, y, x, y + dy * length), fill=color, width=width)


def draw_arc_ring(draw, cx, cy, r, color=CYAN_DIM, width=2):
    for start, end in ((30, 150), (210, 330)):
        draw.arc((cx - r, cy - r, cx + r, cy + r), start, end, fill=color, width=width)


def _paint_robot_mask(draw, s):
    """Detailed round-machine robot silhouette."""
    cx = s // 2
    cy = s // 2 + _scale(6, s)
    body_r = _scale(38, s)
    by = cy

    draw.ellipse((cx - body_r, by - body_r, cx + body_r, by + body_r), fill=255)

    tread_h = _scale(14, s)
    draw.rounded_rectangle(
        (cx - _scale(30, s), by + body_r - _scale(6, s), cx + _scale(30, s), by + body_r + tread_h),
        _scale(5, s),
        fill=255,
    )
    for tx in range(cx - _scale(24, s), cx + _scale(24, s), _scale(8, s)):
        draw.rectangle((tx, by + body_r, tx + _scale(4, s), by + body_r + tread_h - _scale(2, s)), fill=0)

    for px in (cx - _scale(44, s), cx + _scale(44, s)):
        draw.rounded_rectangle(
            (px - _scale(10, s), by - _scale(4, s), px + _scale(10, s), by + _scale(22, s)),
            _scale(5, s),
            fill=255,
        )
        draw.ellipse((px - _scale(4, s), by + _scale(14, s), px + _scale(4, s), by + _scale(22, s)), fill=0)

    draw.rounded_rectangle(
        (cx - _scale(20, s), by - _scale(10, s), cx + _scale(20, s), by + _scale(10, s)),
        _scale(6, s),
        fill=255,
    )
    draw.rectangle(
        (cx - _scale(16, s), by - _scale(2, s), cx + _scale(16, s), by + _scale(2, s)),
        fill=0,
    )
    draw.rectangle(
        (cx - _scale(12, s), by - _scale(1, s), cx + _scale(12, s), by + _scale(1, s)),
        fill=255,
    )

    draw.line((cx, by - body_r, cx, by - body_r - _scale(16, s)), fill=255, width=max(2, _scale(4, s)))
    ant_r = _scale(7, s)
    draw.ellipse(
        (cx - ant_r, by - body_r - _scale(22, s), cx + ant_r, by - body_r - _scale(8, s)),
        fill=255,
    )
    draw.ellipse(
        (cx - _scale(3, s), by - body_r - _scale(20, s), cx + _scale(3, s), by - body_r - _scale(14, s)),
        fill=0,
    )

    panel_y = by + _scale(12, s)
    for py in (panel_y, panel_y + _scale(10, s)):
        draw.line((cx - _scale(18, s), py, cx + _scale(18, s), py), fill=0, width=max(1, _scale(2, s)))


def dbd_teammate_robot(size=(256, 256)):
    return compose_silhouette(_paint_robot_mask, size)


def _paint_dbd_station_mask(draw, s):
    white = 255
    frame_in = _scale(18, s)
    frame_out = s - _scale(18, s)
    pad = _scale(24, s)

    ax0, ay0 = frame_in + pad, frame_in + pad
    ax1, ay1 = frame_out - pad, frame_out - pad
    cx = (ax0 + ax1) // 2
    aw, ah = ax1 - ax0, ay1 - ay0

    tw = int(aw * 0.52)
    tl, tr = cx - tw // 2, cx + tw // 2
    tt = ay0 + int(ah * 0.02)
    tb = ay0 + int(ah * 0.62)

    cap_h = _scale(12, s)
    draw.polygon(
        [
            (tl - _scale(8, s), tt),
            (tr + _scale(8, s), tt),
            (tr + _scale(4, s), tt + cap_h),
            (tl - _scale(4, s), tt + cap_h),
        ],
        fill=white,
    )

    led_y = tt + cap_h + _scale(4, s)
    led_h = max(3, _scale(5, s))
    draw.rounded_rectangle(
        (tl + tw // 5, led_y, tr - tw // 5, led_y + led_h),
        radius=max(1, _scale(2, s)),
        fill=white,
    )

    body_t = led_y + led_h + _scale(8, s)
    chamfer = _scale(10, s)
    draw.polygon(
        [
            (tl + chamfer, body_t),
            (tr - chamfer, body_t),
            (tr, body_t + chamfer),
            (tr, tb - chamfer),
            (tr - chamfer, tb),
            (tl + chamfer, tb),
            (tl, tb - chamfer),
            (tl, body_t + chamfer),
        ],
        fill=white,
    )

    side_inset = _scale(6, s)
    strip_w = max(2, _scale(4, s))
    for sx in (tl + side_inset, tr - side_inset - strip_w):
        draw.rectangle((sx, body_t + _scale(16, s), sx + strip_w, tb - _scale(18, s)), fill=white)

    sm = int(tw * 0.15)
    st = body_t + _scale(12, s)
    sb = body_t + int((tb - body_t) * 0.48)
    sl, sr = tl + sm, tr - sm
    bezel = max(4, _scale(6, s))
    draw.rounded_rectangle((sl, st, sr, sb), radius=_scale(4, s), outline=white, width=bezel)

    rivet_r = max(2, _scale(3, s))
    for px, py in (
        (sl + bezel, st + bezel),
        (sr - bezel, st + bezel),
        (sl + bezel, sb - bezel),
        (sr - bezel, sb - bezel),
    ):
        draw.ellipse((px - rivet_r, py - rivet_r, px + rivet_r, py + rivet_r), fill=white)

    panel_y = sb + _scale(10, s)
    line_w = max(2, _scale(3, s))
    draw.rectangle((tl + sm, panel_y, tr - sm, panel_y + line_w), fill=white)
    draw.rectangle((tl + sm, panel_y + _scale(9, s), tr - sm, panel_y + _scale(9, s) + line_w), fill=white)

    chute_t = tb - _scale(2, s)
    chute_b = ay1 - _scale(2, s)
    flare = int(tw * 0.20)
    draw.polygon(
        [
            (tl + _scale(2, s), chute_t),
            (tr - _scale(2, s), chute_t),
            (tr + flare, chute_b - _scale(10, s)),
            (tl - flare, chute_b - _scale(10, s)),
        ],
        fill=white,
    )

    lip = _scale(5, s)
    draw.rectangle((tl - flare + lip, chute_b - _scale(10, s), tr + flare - lip, chute_b), fill=white)


def _paint_dbd_station_with_holes(draw, s):
    holes = _paint_dbd_station_mask(draw, s)
    return holes


def dbd_delivery_icon(size=(256, 256)):
    scale = SUPERSAMPLE
    s = size[0] * scale
    mask = Image.new("L", (s, s), 0)
    mdraw = ImageDraw.Draw(mask)
    _paint_dbd_station_mask(mdraw, s)

    sm = int((s - _scale(18, s) * 2 - _scale(24, s) * 2) * 0.52 * 0.15)
    frame_in = _scale(18, s)
    pad = _scale(24, s)
    ax0 = frame_in + pad
    ay0 = frame_in + pad
    aw = s - (frame_in + pad) * 2
    ah = aw
    cx = ax0 + aw // 2
    tw = int(aw * 0.52)
    tl, tr = cx - tw // 2, cx + tw // 2
    cap_h = _scale(12, s)
    tt = ay0 + int(ah * 0.02)
    led_y = tt + cap_h + _scale(4, s)
    led_h = max(3, _scale(5, s))
    body_t = led_y + led_h + _scale(8, s)
    tb = ay0 + int(ah * 0.62)
    st = body_t + _scale(12, s)
    sb = body_t + int((tb - body_t) * 0.48)
    sl, sr = tl + sm, tr - sm
    bezel = max(4, _scale(6, s))
    mdraw.rectangle((sl + bezel, st + bezel, sr - bezel, sb - bezel), fill=0)
    mdraw.rectangle(
        (tl + _scale(12, s), tb - _scale(2, s) + _scale(8, s), tr - _scale(12, s), ay0 + ah - _scale(16, s)),
        fill=0,
    )

    image = new((s, s))
    shadow = Image.new("RGBA", (s, s), SHADOW)
    shadow.putalpha(mask)
    shadow = shadow.filter(ImageFilter.GaussianBlur(max(2, scale)))
    image.alpha_composite(shadow, dest=(scale, scale * 2))

    rim = dilate(mask, max(1, scale // 2))
    rim_layer = Image.new("RGBA", (s, s), OUTLINE)
    rim_layer.putalpha(rim)
    image.alpha_composite(rim_layer)

    body = Image.new("RGBA", (s, s), ICON)
    body.putalpha(mask)
    image.alpha_composite(body)

    highlight = Image.new("RGBA", (s, s), (255, 255, 255, 80))
    highlight.putalpha(erode(mask, max(1, scale // 3)))
    image.alpha_composite(highlight)

    return upscale(image, size)


def dbd_delivery_station_white(size=(256, 256)):
    return dbd_delivery_icon(size)


def sci_fi_robot_portrait(size=(128, 128)):
    base = dbd_teammate_robot((256, 256))
    if size != (256, 256):
        return base.resize(size, Image.Resampling.LANCZOS)
    return base


METAL_HI = (198, 206, 218, 255)
METAL_MID = (118, 126, 138, 255)
METAL_LO = (38, 42, 50, 255)
METAL_EDGE = (160, 170, 184, 255)
METAL_SPEC = (235, 240, 248, 180)
PANEL_FILL = (6, 8, 12, 235)


def _clamp(v, lo=0, hi=255):
    return max(lo, min(hi, v))


def _apply_brush_noise(image, box, amount=14, step=2, seed=42):
    import random

    rng = random.Random(seed)
    px = image.load()
    x0, y0, x1, y1 = box
    for y in range(y0, y1, step):
        for x in range(x0, x1, step):
            r, g, b, a = px[x, y]
            n = rng.randint(-amount, amount)
            px[x, y] = (_clamp(r + n), _clamp(g + n), _clamp(b + n), a)


def hud_metallic_panel(size=(256, 128), radius=8, border=10):
    """Brushed steel frame + dark glass inset. Border stays thin when stretched."""
    scale = 6
    big = (size[0] * scale, size[1] * scale)
    image = new(big)
    draw = ImageDraw.Draw(image)
    b = border * scale
    r = max(2, radius * scale)
    outer = (0, 0, big[0] - 1, big[1] - 1)

    # Outer steel shell
    draw.rounded_rectangle(outer, r, fill=(28, 32, 38, 255))
    draw.rounded_rectangle((b // 4, b // 4, big[0] - b // 4 - 1, big[1] - b // 4 - 1), r, fill=METAL_MID)
    _apply_brush_noise(image, (b // 4, b // 4, big[0] - b // 4, big[1] - b // 4), amount=10, seed=11)

    # Bevel: top-left highlight band
    bevel_outer = (b // 2, b // 2, big[0] - b // 2 - 1, big[1] - b // 2 - 1)
    draw.rounded_rectangle(bevel_outer, max(2, r - scale), fill=METAL_HI)
    draw.rounded_rectangle((b, b, big[0] - b - 1, big[1] - b - 1), max(2, r - scale // 2), fill=METAL_LO)

    # Inner glass
    inner = (b + scale, b + scale, big[0] - b - scale - 1, big[1] - b - scale - 1)
    draw.rounded_rectangle(inner, max(2, r - b // 2), fill=PANEL_FILL)
    _apply_brush_noise(image, inner, amount=6, seed=29)

    # Specular top edge
    draw.line(
        (b + 3 * scale, b + scale, big[0] - b - 3 * scale, b + scale),
        fill=METAL_SPEC,
        width=max(1, scale // 2),
    )
    draw.line(
        (b + 3 * scale, big[1] - b - scale, big[0] - b - 3 * scale, big[1] - b - scale),
        fill=(0, 0, 0, 120),
        width=max(1, scale // 2),
    )

    # Inner hairline
    draw.rounded_rectangle(inner, max(2, r - b // 2), outline=(255, 255, 255, 28), width=max(1, scale // 3))
    draw.rounded_rectangle(outer, r, outline=METAL_EDGE, width=max(1, scale // 2))

    # Corner bolts (subtle)
    bolt = max(2, scale // 2)
    for px, py in (
        (b + 2 * scale, b + 2 * scale),
        (big[0] - b - 2 * scale, b + 2 * scale),
        (b + 2 * scale, big[1] - b - 2 * scale),
        (big[0] - b - 2 * scale, big[1] - b - 2 * scale),
    ):
        draw.ellipse((px - bolt, py - bolt, px + bolt, py + bolt), fill=(90, 96, 108, 255))
        draw.point((px, py), fill=(220, 226, 236, 255))

    return upscale(image, size)


def hud_metallic_row(size=(512, 96)):
    """Horizontal strip for a single teammate entry row."""
    return hud_metallic_panel(size, radius=8, border=10)


def hud_bevel_slot(size=(128, 128), inner=SLOT_VOID):
    scale = 4
    big = (size[0] * scale, size[1] * scale)
    image = new(big)
    draw = ImageDraw.Draw(image)
    pad = 10 * scale
    box = (pad, pad, big[0] - pad - 1, big[1] - pad - 1)
    radius = _scale(8, big[0])
    draw.rounded_rectangle(box, radius, fill=CHROME)
    draw.rounded_rectangle(
        (box[0] + scale, box[1] + scale, box[2] - scale, box[3] - scale),
        max(2, radius - scale),
        fill=inner,
    )
    draw.line((box[0] + 8 * scale, box[1] + 5 * scale, box[2] - 8 * scale, box[1] + 5 * scale), fill=CHROME_HI)
    draw.line((box[0] + 8 * scale, box[3] - 5 * scale, box[2] - 8 * scale, box[3] - 5 * scale), fill=(0, 0, 0, 80))
    draw.rounded_rectangle(box, radius, outline=CHROME_EDGE, width=max(1, scale // 2))
    return upscale(image, size)


def hud_minimal_slot(size=(128, 128), inner=SLOT_VOID):
    return hud_bevel_slot(size, inner)


def sci_fi_slot(size=(128, 128), accent=CHROME_EDGE, inner=SLOT_VOID):
    return hud_bevel_slot(size, inner)


def sci_fi_diamond_slot(size=(128, 128), accent=CHROME_EDGE, inner=SLOT_VOID):
    scale = 4
    bw, bh = size[0] * scale, size[1] * scale
    image = new((bw, bh))
    draw = ImageDraw.Draw(image)
    cx, cy = bw // 2, bh // 2
    r = min(bw, bh) // 2 - 10 * scale
    pts = [(cx, cy - r), (cx + r, cy), (cx, cy + r), (cx - r, cy)]
    draw.polygon(pts, fill=inner)
    draw.polygon(
        [(cx, cy - r + 4 * scale), (cx + r - 4 * scale, cy), (cx, cy + r - 4 * scale), (cx - r + 4 * scale, cy)],
        fill=CHROME,
    )
    draw.polygon(
        [(cx, cy - r + 8 * scale), (cx + r - 8 * scale, cy), (cx, cy + r - 8 * scale), (cx - r + 8 * scale, cy)],
        fill=inner,
    )
    draw.line(pts + [pts[0]], fill=accent, width=max(2, scale))
    return upscale(image, size)


def sci_fi_bar(size=(280, 22), outline=CHROME_EDGE):
    return hud_bar_track(size)


def sci_fi_fill_bar(size=(280, 22), color=PROGRESS):
    return hud_bar_fill(size, color)


def hud_bar_track(size=(280, 22)):
    scale = 4
    big = (size[0] * scale, size[1] * scale)
    image = new(big)
    draw = ImageDraw.Draw(image)
    draw.rounded_rectangle((0, 0, big[0] - 1, big[1] - 1), max(4, scale), fill=CHROME)
    draw.rounded_rectangle(
        (2 * scale, 2 * scale, big[0] - 2 * scale - 1, big[1] - 2 * scale - 1),
        max(3, scale),
        outline=CHROME_EDGE,
        width=max(1, scale // 2),
    )
    return upscale(image, size)


def hud_bar_fill(size=(280, 22), color=PROGRESS):
    scale = 4
    big = (size[0] * scale, size[1] * scale)
    image = new(big)
    draw = ImageDraw.Draw(image)
    draw.rounded_rectangle((scale, scale, big[0] - scale - 1, big[1] - scale - 1), max(3, scale), fill=color)
    hi = Image.new("RGBA", big, (255, 255, 255, 50))
    draw_hi = ImageDraw.Draw(hi)
    draw_hi.rounded_rectangle(
        (scale * 2, scale, big[0] - scale * 2, big[1] // 2),
        max(2, scale),
        fill=(255, 255, 255, 40),
    )
    image.alpha_composite(hi)
    return upscale(image, size)


def dbd_state_ring(size=(128, 128), color=ICON):
    """Full status ring — readable at HUD scale."""
    scale = 4
    bw, bh = size[0] * scale, size[1] * scale
    image = new((bw, bh))
    draw = ImageDraw.Draw(image)
    cx, cy = bw // 2, bh // 2
    r = min(bw, bh) // 2 - 8 * scale
    width = max(3, scale)
    for start, end in ((215, 505), (35, 145)):
        draw.arc((cx - r, cy - r, cx + r, cy + r), start, end, fill=color, width=width)
    return upscale(image, size)


def sci_fi_frame_ring(size=(128, 128), color=ICON):
    return dbd_state_ring(size, color)


def delivery_progress_bar(filled_slots, total=10, size=(280, 28)):
    """Single composite progress bar — 0..total filled slots. No widget stretching."""
    scale = 6
    big = (size[0] * scale, size[1] * scale)
    image = new(big)
    draw = ImageDraw.Draw(image)

    draw.rounded_rectangle((0, 0, big[0] - 1, big[1] - 1), max(4, scale), fill=CHROME)
    draw.rounded_rectangle(
        (scale, scale, big[0] - scale - 1, big[1] - scale - 1),
        max(3, scale),
        fill=STACK_TRACK,
    )

    pad_x = 8 * scale
    pad_y = 6 * scale
    inner_w = big[0] - pad_x * 2
    slot_w = inner_w // total
    slot_h = big[1] - pad_y * 2
    gap = max(1, scale // 2)

    for i in range(total):
        sx = pad_x + i * slot_w + gap
        sy = pad_y
        cell = (sx, sy, sx + slot_w - gap * 2, sy + slot_h)
        draw.rectangle(cell, fill=STACK_EMPTY)
        if i < filled_slots:
            draw.rectangle(cell, fill=PROGRESS_LO)
            hi_cell = (cell[0], cell[1], cell[2], cell[1] + (cell[3] - cell[1]) // 2)
            draw.rectangle(hi_cell, fill=PROGRESS_HI)

    draw.rounded_rectangle((0, 0, big[0] - 1, big[1] - 1), max(4, scale), outline=CHROME_EDGE, width=max(1, scale // 2))
    return upscale(image, size)


def delivery_stack_track(size=(240, 22), slots=10):
    return delivery_progress_bar(0, slots, size)


def delivery_stack_segment(filled=False, size=(24, 22)):
    return delivery_progress_bar(1 if filled else 0, 10, (240, 22)).resize(size, Image.Resampling.LANCZOS)


def delivery_stack_slot(filled=False, index=0, total=10, size=(24, 22)):
    return delivery_stack_segment(filled, size)


def cage_stack(filled=False):
    scale = 4
    big = 256
    image = new((big, big))
    draw = ImageDraw.Draw(image)
    color = GOLD if filled else MUTED
    fill = (220, 186, 96, 100) if filled else (0, 0, 0, 0)
    draw.rounded_rectangle((28, 28, 228, 228), 18, outline=color, width=10, fill=fill)
    for x in (76, 128, 180):
        draw.line((x, 44, x, 212), fill=color, width=8)
    if filled:
        draw.line((44, 128, 212, 128), fill=GOLD, width=6)
    return upscale(image, (64, 64))


def label(letter, color=MUTED):
    image = hud_bevel_slot((96, 96))
    draw = ImageDraw.Draw(image)
    font = ImageFont.load_default()
    draw.text((38, 36), letter, fill=color, font=font)
    return image


def main():
    save(ROOT / "Textures/Common/T_HUD_Panel_BG.png", hud_metallic_panel((160, 160), radius=8, border=10))
    save(ROOT / "Textures/Common/T_HUD_Panel_BG_Wide.png", hud_metallic_panel((308, 252), radius=8, border=10))
    save(ROOT / "Textures/Common/T_HUD_Panel_BG_Bar.png", hud_metallic_panel((380, 108), radius=6, border=8))
    save(ROOT / "Textures/Teammate/T_HUD_Entry_Row_BG.png", hud_metallic_panel((308, 72), radius=6, border=8))
    save(ROOT / "Textures/Common/T_HUD_Bar_BG.png", sci_fi_bar())
    save(ROOT / "Textures/Common/T_HUD_Bar_Fill.png", sci_fi_fill_bar(color=GREEN))
    save(ROOT / "Textures/Common/T_HUD_Divider.png", sci_fi_bar((8, 128)))

    save(ROOT / "Textures/Teammate/T_HUD_Portrait_Placeholder.png", dbd_teammate_robot((256, 256)))
    save(ROOT / "Textures/Teammate/T_HUD_Portrait_Frame.png", dbd_state_ring((128, 128), ICON))
    save(ROOT / "Textures/Teammate/T_HUD_Portrait_Frame_Healthy.png", dbd_state_ring((128, 128), PROGRESS))
    save(ROOT / "Textures/Teammate/T_HUD_Portrait_Frame_Injured.png", dbd_state_ring((128, 128), WARN))
    save(ROOT / "Textures/Teammate/T_HUD_Portrait_Frame_Downed.png", dbd_state_ring((128, 128), DANGER))
    save(ROOT / "Textures/Teammate/T_HUD_Portrait_Frame_Caged.png", dbd_state_ring((128, 128), BLUE))
    save(ROOT / "Textures/Teammate/T_HUD_Portrait_Frame_Dead.png", dbd_state_ring((128, 128), MUTED))
    save(ROOT / "Textures/Teammate/T_HUD_Portrait_Frame_Escaped.png", dbd_state_ring((128, 128), ICON_MID))
    save(ROOT / "Textures/Teammate/T_HUD_Bar_Fill_Downed.png", sci_fi_fill_bar(color=RED))

    save(ROOT / "Icons/CageStack/T_HUD_CageStack_Empty.png", cage_stack(False))
    save(ROOT / "Icons/CageStack/T_HUD_CageStack_Filled.png", cage_stack(True))

    save(ROOT / "Textures/Delivery/T_HUD_Delivery_Label_A.png", label("A", BLUE))
    save(ROOT / "Textures/Delivery/T_HUD_Delivery_Label_B.png", label("B", PURPLE))
    save(ROOT / "Textures/Delivery/T_HUD_Delivery_Slot.png", dbd_delivery_station_white())
    save(ROOT / "Textures/Delivery/T_HUD_Delivery_Slot_A.png", dbd_delivery_station_white())
    save(ROOT / "Textures/Delivery/T_HUD_Delivery_Slot_B.png", dbd_delivery_station_white())
    save(ROOT / "Textures/Delivery/T_HUD_Delivery_Stack_Track.png", delivery_stack_track())
    save(ROOT / "Textures/Delivery/T_HUD_Delivery_Stack_Empty.png", delivery_stack_segment(False))
    save(ROOT / "Textures/Delivery/T_HUD_Delivery_Stack_Filled.png", delivery_stack_segment(True))
    save(ROOT / "Textures/Delivery/T_HUD_Delivery_Progress_BG.png", hud_bar_track((280, 20)))
    save(ROOT / "Textures/Delivery/T_HUD_Delivery_Progress_Fill.png", hud_bar_fill((280, 20), PROGRESS))

    for filled in range(11):
        name = f"T_HUD_Delivery_Progress_{filled:02d}.png"
        save(ROOT / f"Textures/Delivery/{name}", delivery_progress_bar(filled, 10, (280, 28)))

    save(ROOT / "Textures/Inventory/T_HUD_Inventory_Slot_Empty.png", hud_bevel_slot())
    save(ROOT / "Textures/Inventory/T_HUD_Inventory_Slot_Filled.png", hud_bevel_slot(inner=(62, 54, 34, 255)))
    save(ROOT / "Textures/Inventory/T_HUD_Inventory_Slot_Hover.png", hud_bevel_slot(inner=(34, 38, 48, 255)))

    save(ROOT / "Textures/Perk/T_HUD_Perk_Slot_Empty.png", sci_fi_diamond_slot())
    save(ROOT / "Textures/Perk/T_HUD_Perk_Slot_Equipped.png", sci_fi_diamond_slot(inner=(26, 50, 36, 255)))
    save(ROOT / "Textures/Perk/T_HUD_Perk_Slot_Cooldown.png", sci_fi_diamond_slot(inner=(34, 34, 38, 255)))


if __name__ == "__main__":
    main()

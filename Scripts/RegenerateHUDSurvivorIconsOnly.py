"""
Survivor (round-machine) portrait icons — incremental quality pass.

References:
- DBD Status HUD portraits: fill frame, high contrast, readable at ~64–80px
  https://deadbydaylight.fandom.com/wiki/Status_HUD
- NightLight portrait template: keep art inside frame, no outer border in icon
  https://docs.nightlight.gg/icon-packs/tutorial
- Sci-fi drone/bot HUD glyphs: bold sphere + sensor pods + tread base (Halo/Destiny style simplification)

Run: python Scripts/RegenerateHUDSurvivorIconsOnly.py
Then Reimport Content/UI/HUD/Textures/Teammate in editor (no MCP import).
"""

from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

ROOT = Path(r"E:\Unreal\git\SpaCh4_Copy\Content\UI\HUD")

ICON = (248, 250, 252, 255)
ICON_MID = (210, 216, 224, 255)
OUTLINE = (10, 12, 16, 230)
SHADOW = (0, 0, 0, 120)
EYE_GLOW = (88, 210, 140, 255)
EYE_CORE = (200, 255, 220, 255)
ACCENT_WARM = (220, 180, 100, 255)

SUPERSAMPLE = 6


def new(size):
    return Image.new("RGBA", size, (0, 0, 0, 0))


def save(path, image):
    path.parent.mkdir(parents=True, exist_ok=True)
    image.save(path)


def upscale(image, size):
    return image.resize(size, Image.Resampling.LANCZOS)


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


def compose_silhouette(paint_mask, size=(256, 256), fill=ICON, rim_strength=1.0):
    """Dual-stroke icon: dark rim + bright body + top highlight."""
    scale = SUPERSAMPLE
    s = size[0] * scale
    mask = Image.new("L", (s, s), 0)
    paint_mask(ImageDraw.Draw(mask), s)

    rim_px = max(1, int(scale * rim_strength))
    rim = dilate(mask, rim_px)
    image = new((s, s))

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

    highlight = Image.new("RGBA", (s, s), (255, 255, 255, 75))
    highlight.putalpha(erode(mask, max(1, scale // 3)))
    image.alpha_composite(highlight)

    edge_hi = Image.new("RGBA", (s, s), ICON_MID)
    edge_hi.putalpha(erode(mask, 1))
    image.alpha_composite(edge_hi)

    return upscale(image, size), mask


def _scale(v, s, base=256):
    return int(v * s / base)


def _paint_round_machine(draw, s, variant=0):
    """
    Round-machine survivor silhouette — bold shapes for 72px HUD portrait.
    variant: 0=default, 1=wide chassis, 2=tall scout
    """
    cx = s // 2
    cy = s // 2 + _scale(8, s)
    body_rx = _scale(40 if variant != 1 else 44, s)
    body_ry = _scale(38 if variant != 2 else 42, s)
    by = cy

    # Main chassis sphere
    draw.ellipse((cx - body_rx, by - body_ry, cx + body_rx, by + body_ry), fill=255)

    # Side actuator pods
    pod_w = _scale(14, s)
    pod_h = _scale(22, s)
    for side in (-1, 1):
        px = cx + side * (body_rx + _scale(4, s))
        draw.rounded_rectangle(
            (px - pod_w // 2, by - _scale(6, s), px + pod_w // 2, by + pod_h),
            _scale(5, s),
            fill=255,
        )
        # Sensor lens cutout
        lr = _scale(4, s)
        draw.ellipse(
            (px - lr, by + _scale(10, s), px + lr, by + _scale(18, s)),
            fill=0,
        )

    # Face visor plate
    visor_w = _scale(34, s)
    visor_h = _scale(16, s)
    vy = by - _scale(8, s)
    draw.rounded_rectangle(
        (cx - visor_w, vy - visor_h // 2, cx + visor_w, vy + visor_h // 2),
        _scale(6, s),
        fill=255,
    )

    # Large LED eyes — readable at 72px
    eye_r = _scale(7 if variant != 1 else 6, s)
    eye_y = vy
    eye_spread = _scale(14 if variant != 1 else 16, s)
    if variant == 2:
        eye_spread = _scale(12, s)
    for ex in (cx - eye_spread, cx + eye_spread):
        draw.ellipse((ex - eye_r, eye_y - eye_r, ex + eye_r, eye_y + eye_r), fill=0)

    # Mouth vent — three bold slots (single cutout block + re-draw center dividers as erode? use rects)
    vent_y = by + _scale(10, s)
    vent_w = _scale(22, s)
    vent_h = _scale(8, s)
    draw.rounded_rectangle(
        (cx - vent_w, vent_y, cx + vent_w, vent_y + vent_h),
        _scale(3, s),
        fill=0,
    )
    slot_w = max(2, _scale(3, s))
    for sx in (cx - _scale(8, s), cx, cx + _scale(8, s)):
        draw.rectangle((sx - slot_w // 2, vent_y + _scale(2, s), sx + slot_w // 2, vent_y + vent_h - _scale(2, s)), fill=255)

    # Tread base
    tread_w = _scale(52 if variant != 1 else 58, s)
    tread_h = _scale(14, s)
    tread_top = by + body_ry - _scale(4, s)
    draw.rounded_rectangle(
        (cx - tread_w // 2, tread_top, cx + tread_w // 2, tread_top + tread_h),
        _scale(5, s),
        fill=255,
    )
    gap = _scale(6, s)
    for tx in range(cx - tread_w // 2 + gap, cx + tread_w // 2 - gap, _scale(10, s)):
        draw.rectangle(
            (tx, tread_top + _scale(2, s), tx + _scale(4, s), tread_top + tread_h - _scale(2, s)),
            fill=0,
        )

    # Antenna
    ant_h = _scale(20 if variant != 2 else 28, s)
    stem_w = max(2, _scale(4, s))
    top_y = by - body_ry - ant_h
    draw.rectangle((cx - stem_w // 2, top_y + _scale(8, s), cx + stem_w // 2, by - body_ry + _scale(2, s)), fill=255)
    tip_r = _scale(6, s)
    draw.ellipse((cx - tip_r, top_y, cx + tip_r, top_y + tip_r * 2), fill=255)
    draw.ellipse((cx - _scale(3, s), top_y + _scale(3, s), cx + _scale(3, s), top_y + _scale(9, s)), fill=0)

    # Variant accents
    if variant == 1:
        # Shoulder plate
        draw.rounded_rectangle(
            (cx - body_rx + _scale(4, s), by - body_ry + _scale(6, s), cx + body_rx - _scale(4, s), by - _scale(2, s)),
            _scale(4, s),
            outline=255,
            width=max(2, _scale(3, s)),
        )
    elif variant == 2:
        # Extra dorsal fin
        draw.polygon(
            [
                (cx, by - body_ry - _scale(4, s)),
                (cx + _scale(8, s), by - body_ry - _scale(14, s)),
                (cx - _scale(8, s), by - body_ry - _scale(14, s)),
            ],
            fill=255,
        )


def _add_eye_glow(image, mask_fullres, size, variant=0):
    """Soft green LED eyes on top of silhouette."""
    scale = SUPERSAMPLE
    s = size[0] * scale
    cx = s // 2
    cy = s // 2 + _scale(8, s)
    vy = cy - _scale(8, s)
    eye_spread = _scale(14 if variant != 1 else 16, s)
    if variant == 2:
        eye_spread = _scale(12, s)
    eye_r = _scale(5 if variant != 1 else 4, s)

    glow = new((s, s))
    gd = ImageDraw.Draw(glow)
    for ex in (cx - eye_spread, cx + eye_spread):
        gr = eye_r + _scale(3, s)
        gd.ellipse((ex - gr, vy - gr, ex + gr, vy + gr), fill=(*EYE_GLOW[:3], 90))
        gd.ellipse((ex - eye_r, vy - eye_r, ex + eye_r, vy + eye_r), fill=EYE_CORE)

    glow = glow.filter(ImageFilter.GaussianBlur(max(1, scale // 3)))
    out = image.copy()
    out_big = out.resize((s, s), Image.Resampling.NEAREST)
    # Only show glow where eyes were cut from mask (invert small regions — paste at eye coords)
    eye_mask = Image.new("L", (s, s), 0)
    ed = ImageDraw.Draw(eye_mask)
    for ex in (cx - eye_spread, cx + eye_spread):
        ed.ellipse((ex - eye_r - scale, vy - eye_r - scale, ex + eye_r + scale, vy + eye_r + scale), fill=255)
    eye_mask = eye_mask.filter(ImageFilter.GaussianBlur(max(1, scale // 2)))
    glow.putalpha(eye_mask)
    out_big.alpha_composite(glow)
    return upscale(out_big, size)


def survivor_portrait(size=(256, 256), variant=0):
    def paint(draw, s):
        _paint_round_machine(draw, s, variant)

    icon, _ = compose_silhouette(paint, size, rim_strength=1.2)
    return _add_eye_glow(icon, None, size, variant)


def portrait_bg(size=(128, 128)):
    """Dark recessed circle behind portrait — matches HUD slot palette."""
    scale = SUPERSAMPLE
    s = size[0] * scale
    canvas = new((s, s))
    draw = ImageDraw.Draw(canvas)
    pad = _scale(6, s)
    draw.ellipse((pad, pad, s - pad, s - pad), fill=(8, 10, 14, 255))
    draw.ellipse((pad + scale, pad + scale, s - pad - scale, s - pad - scale), outline=(255, 255, 255, 24), width=max(1, scale // 2))
    inner = (pad + 3 * scale, pad + 3 * scale, s - pad - 3 * scale, s - pad - 3 * scale)
    draw.ellipse(inner, fill=(4, 5, 8, 255))
    return upscale(canvas, size)


def main():
    teammate = ROOT / "Textures/Teammate"

    save(teammate / "T_HUD_Portrait_Placeholder.png", survivor_portrait((256, 256), 0))
    save(teammate / "T_HUD_Portrait_Robot_A.png", survivor_portrait((256, 256), 0))
    save(teammate / "T_HUD_Portrait_Robot_B.png", survivor_portrait((256, 256), 1))
    save(teammate / "T_HUD_Portrait_Robot_C.png", survivor_portrait((256, 256), 2))
    save(teammate / "T_HUD_Portrait_BG.png", portrait_bg((128, 128)))

    print("Survivor icon PNGs written.")
    print("  Reimport Content/UI/HUD/Textures/Teammate in Unreal Editor.")


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Generate screenshots and GIFs for Metro Effects Pack v0.1.0 launch.

Optimized with numpy vectorization where possible.
"""

from PIL import Image, ImageDraw, ImageFont
import math
import os
import numpy as np

OUT = os.path.dirname(os.path.abspath(__file__))
W, H = 480, 270


def _clamp(v, lo=0.0, hi=1.0):
    return max(lo, min(hi, v))


def luminance(r, g, b):
    return 0.299 * r + 0.587 * g + 0.114 * b


# ── Test pattern generator ──────────────────────────────────────────────


def make_test_pattern(w=W, h=H, t=0.0):
    img = Image.new("RGBA", (w, h))
    pix = img.load()
    for y in range(h):
        for x in range(w):
            u, v = x / w, y / h
            r = 0.1 + 0.6 * (1 - v)
            g = 0.2 + 0.7 * (1 - v)
            b = 0.5 + 0.5 * (1 - v)
            sd = math.hypot(u - 0.8, v - 0.25)
            if sd < 0.08:
                r, g, b = 1.0, 0.9, 0.3
            elif sd < 0.1:
                glow = (0.1 - sd) / 0.02
                r = r * (1 - glow) + 1.0 * glow
                g = g * (1 - glow) + 0.9 * glow
                b = b * (1 - glow) + 0.3 * glow
            if v > 0.7:
                r, g, b = 0.3, 0.6, 0.2
            if v > 0.7 and abs(u - 0.5) < 0.08 + 0.06 * (v - 0.7) / 0.3:
                r, g, b = 0.5, 0.4, 0.3
            bx = 0.2 + 0.6 * (0.5 + 0.5 * math.sin(t * 2.0))
            by = 0.55 + 0.15 * math.cos(t * 3.0)
            if math.hypot(u - bx, v - by) < 0.04:
                r, g, b = 1.0, 0.2, 0.2
            bx2 = 0.2 + 0.6 * (0.5 + 0.5 * math.cos(t * 1.7 + 1.0))
            by2 = 0.58 + 0.12 * math.sin(t * 2.3 + 0.5)
            if math.hypot(u - bx2, v - by2) < 0.03:
                r, g, b = 0.2, 0.6, 1.0
            pix[x, y] = (int(r * 255), int(g * 255), int(b * 255), 255)
    draw = ImageDraw.Draw(img)
    try:
        font = ImageFont.truetype(
            "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 20
        )
    except Exception:
        font = ImageFont.load_default()
    draw.text(
        (10, 8), "Metro Effects Pack v0.1.0", fill=(255, 255, 255, 200), font=font
    )
    return img


# ── metro-ascii ─────────────────────────────────────────────────────────

ASCII_CHARSET = " .:-=+*#%@"


def apply_ascii(src, cell_size=8, font_aspect=0.5, color_pass=True, contrast=1.0):
    arr = np.array(src, dtype=np.float32) / 255.0
    h, w = arr.shape[:2]
    dst = arr.copy()
    cols, rows = w // cell_size, h // cell_size
    cs = contrast
    co = (1.0 - cs) * 0.5
    for ry in range(rows):
        y0 = ry * cell_size
        for rx in range(cols):
            x0 = rx * cell_size
            tile = dst[y0 : y0 + cell_size, x0 : x0 + cell_size]
            avg = tile.mean(axis=(0, 1))
            luma = luminance(avg[0], avg[1], avg[2])
            stretched = _clamp((luma - 0.5) * cs + 0.5 + co)
            idx = int(stretched * 9 + 0.5)
            idx = max(0, min(idx, 9))
            ch = ASCII_CHARSET[idx]
            density = 0 if ch == " " else (ord(ch) - 32) * 4
            density = min(density, 255)
            cr = avg[0] if color_pass else 1.0
            cg = avg[1] if color_pass else 1.0
            cb = avg[2] if color_pass else 1.0
            cw = max(1, int(cell_size * font_aspect))
            if cw > cell_size:
                cw = cell_size
            sh = max(1, cell_size // 5)
            cx2 = x0 + (cell_size - cw) // 2
            cy2 = y0 + cell_size // 3
            blend = density / 255.0
            for dy in range(sh):
                py = cy2 + dy
                if py >= y0 + cell_size or py >= h:
                    break
                for dx in range(cw):
                    px = cx2 + dx
                    if px >= x0 + cell_size or px >= w:
                        break
                    dst[py, px, 0] = dst[py, px, 0] * (1 - blend) + cr * blend
                    dst[py, px, 1] = dst[py, px, 1] * (1 - blend) + cg * blend
                    dst[py, px, 2] = dst[py, px, 2] * (1 - blend) + cb * blend
                    dst[py, px, 3] = 1.0
    out = (np.clip(dst, 0, 1) * 255).astype(np.uint8)
    return Image.fromarray(out, "RGBA")


def render_ascii_demo():
    src = make_test_pattern()
    src.save(os.path.join(OUT, "metro-ascii-before.png"))
    after = apply_ascii(
        src, cell_size=6, font_aspect=0.5, color_pass=True, contrast=1.2
    )
    after.save(os.path.join(OUT, "metro-ascii-after.png"))
    side = Image.new("RGBA", (W * 2, H))
    side.paste(after, (W, 0))
    side.paste(src, (0, 0))
    side.save(os.path.join(OUT, "metro-ascii-comparison.png"))
    cells = [4, 6, 8, 10, 12, 10, 8, 6]
    frames = [apply_ascii(src, cell_size=cs) for cs in cells]
    if frames:
        frames[0].save(
            os.path.join(OUT, "metro-ascii-cell-size.gif"),
            save_all=True,
            append_images=frames[1:],
            duration=300,
            loop=0,
        )
    contrasts = [0.3, 0.6, 1.0, 1.5, 2.0, 3.0, 2.0, 1.5, 1.0, 0.6, 0.3]
    frames = [apply_ascii(src, contrast=c) for c in contrasts]
    if frames:
        frames[0].save(
            os.path.join(OUT, "metro-ascii-contrast.gif"),
            save_all=True,
            append_images=frames[1:],
            duration=300,
            loop=0,
        )
    print("  metro-ascii: done")


# ── metro-chromab (vectorized) ──────────────────────────────────────────


def apply_chromab(
    src,
    r_shift=(0, 0),
    g_shift=(0, 0),
    b_shift=(0, 0),
    radial_falloff=0.0,
    stretch_angle=0.0,
    mix=1.0,
):
    """Vectorized chromatic aberration."""
    arr = np.array(src, dtype=np.float32) / 255.0
    h, w = arr.shape[:2]
    ys, xs = np.mgrid[0:h, 0:w]
    # Normalized coords [-1, 1]
    cxs = (xs / w) * 2.0 - 1.0
    cys = (ys / h) * 2.0 - 1.0
    dist = np.sqrt(cxs**2 + cys**2)
    falloff = dist ** (1.0 + radial_falloff * 4.0)
    angle_rad = math.radians(stretch_angle)
    cos_a, sin_a = math.cos(angle_rad), math.sin(angle_rad)

    def shift_ch(ch_shift):
        sx, sy = ch_shift
        off_x = sx * falloff
        off_y = sy * falloff
        rot_x = off_x * cos_a - off_y * sin_a
        rot_y = off_x * sin_a + off_y * cos_a
        # Source coordinates
        fx = np.clip(xs + rot_x, 0, w - 1)
        fy = np.clip(ys + rot_y, 0, h - 1)
        ix = np.floor(fx).astype(np.int32)
        iy = np.floor(fy).astype(np.int32)
        dx = fx - ix
        dy = fy - iy
        ix1 = np.clip(ix + 1, 0, w - 1)
        iy1 = np.clip(iy + 1, 0, h - 1)
        p00 = arr[iy, ix]
        p10 = arr[iy, ix1]
        p01 = arr[iy1, ix]
        p11 = arr[iy1, ix1]
        top = p00 + dx[:, :, None] * (p10 - p00)
        bot = p01 + dx[:, :, None] * (p11 - p01)
        return top + dy[:, :, None] * (bot - top)

    shifted_r = shift_ch(r_shift)[:, :, 0]
    shifted_g = shift_ch(g_shift)[:, :, 1]
    shifted_b = shift_ch(b_shift)[:, :, 2]
    dst = arr.copy()
    dst[:, :, 0] = arr[:, :, 0] + mix * (shifted_r - arr[:, :, 0])
    dst[:, :, 1] = arr[:, :, 1] + mix * (shifted_g - arr[:, :, 1])
    dst[:, :, 2] = arr[:, :, 2] + mix * (shifted_b - arr[:, :, 2])
    out = (np.clip(dst, 0, 1) * 255).astype(np.uint8)
    return Image.fromarray(out, "RGBA")


def render_chromab_demo():
    src = make_test_pattern()
    src.save(os.path.join(OUT, "metro-chromab-before.png"))
    after = apply_chromab(src, r_shift=(5, 0), b_shift=(-5, 0))
    after.save(os.path.join(OUT, "metro-chromab-after.png"))
    side = Image.new("RGBA", (W * 2, H))
    side.paste(after, (W, 0))
    side.paste(src, (0, 0))
    side.save(os.path.join(OUT, "metro-chromab-comparison.png"))
    shifts = list(range(0, 13)) + list(range(11, 0, -1))
    frames = [apply_chromab(src, r_shift=(s, 0), b_shift=(-s, 0)) for s in shifts]
    if frames:
        frames[0].save(
            os.path.join(OUT, "metro-chromab-shift.gif"),
            save_all=True,
            append_images=frames[1:],
            duration=200,
            loop=0,
        )
    falloffs = [r * 0.1 for r in range(0, 11)] + [r * 0.1 for r in range(9, -1, -1)]
    frames = [
        apply_chromab(src, r_shift=(8, 0), b_shift=(-8, 0), radial_falloff=rf)
        for rf in falloffs
    ]
    if frames:
        frames[0].save(
            os.path.join(OUT, "metro-chromab-falloff.gif"),
            save_all=True,
            append_images=frames[1:],
            duration=250,
            loop=0,
        )
    print("  metro-chromab: done")


# ── metro-blobtrack ─────────────────────────────────────────────────────


def blob_detect(gray, threshold=0.5, min_area=30, max_area=5000, proximity_merge=10.0):
    h, w = gray.shape
    raw = int(max(0.001, min(0.999, threshold)) * 255)
    binary = (gray >= raw).astype(np.int32)
    labels = np.zeros((h, w), dtype=np.int32)
    next_label = 0
    parent = {}

    def find(x):
        while parent[x] != x:
            parent[x] = parent[parent[x]]
            x = parent[x]
        return x

    def unite(a, b):
        ra, rb = find(a), find(b)
        if ra != rb:
            parent[rb] = ra

    for y in range(h):
        for x in range(w):
            if not binary[y, x]:
                continue
            left = labels[y, x - 1] if x > 0 else 0
            above = labels[y - 1, x] if y > 0 else 0
            if left == 0 and above == 0:
                next_label += 1
                labels[y, x] = next_label
                parent[next_label] = next_label
            elif left != 0 and above == 0:
                labels[y, x] = left
            elif left == 0 and above != 0:
                labels[y, x] = above
            else:
                labels[y, x] = min(left, above)
                if left != above:
                    unite(left, above)
    if next_label == 0:
        return []
    stats = {}
    for y in range(h):
        row = labels[y]
        for x in range(w):
            l = row[x]
            if l == 0:
                continue
            root = find(l)
            if root not in stats:
                stats[root] = {
                    "area": 0,
                    "sx": 0.0,
                    "sy": 0.0,
                    "minx": w,
                    "miny": h,
                    "maxx": 0,
                    "maxy": 0,
                }
            s = stats[root]
            s["area"] += 1
            s["sx"] += x
            s["sy"] += y
            if x < s["minx"]:
                s["minx"] = x
            if y < s["miny"]:
                s["miny"] = y
            if x > s["maxx"]:
                s["maxx"] = x
            if y > s["maxy"]:
                s["maxy"] = y
    blobs = []
    for s in stats.values():
        if s["area"] < min_area or s["area"] > max_area:
            continue
        blobs.append(
            {
                "id": len(blobs) + 1,
                "area": s["area"],
                "cx": s["sx"] / s["area"],
                "cy": s["sy"] / s["area"],
                "minx": s["minx"],
                "miny": s["miny"],
                "maxx": s["maxx"],
                "maxy": s["maxy"],
            }
        )
    if proximity_merge > 0 and len(blobs) > 1:
        md2 = proximity_merge**2
        changed = True
        while changed:
            changed = False
            for i in range(len(blobs)):
                for j in range(i + 1, len(blobs)):
                    dx = blobs[i]["cx"] - blobs[j]["cx"]
                    dy = blobs[i]["cy"] - blobs[j]["cy"]
                    if dx * dx + dy * dy <= md2:
                        a1, a2 = blobs[i]["area"], blobs[j]["area"]
                        ta = a1 + a2
                        blobs[i]["cx"] = (
                            blobs[i]["cx"] * a1 + blobs[j]["cx"] * a2
                        ) / ta
                        blobs[i]["cy"] = (
                            blobs[i]["cy"] * a1 + blobs[j]["cy"] * a2
                        ) / ta
                        blobs[i]["area"] = ta
                        blobs[i]["minx"] = min(blobs[i]["minx"], blobs[j]["minx"])
                        blobs[i]["miny"] = min(blobs[i]["miny"], blobs[j]["miny"])
                        blobs[i]["maxx"] = max(blobs[i]["maxx"], blobs[j]["maxx"])
                        blobs[i]["maxy"] = max(blobs[i]["maxy"], blobs[j]["maxy"])
                        del blobs[j]
                        changed = True
                        break
                if changed:
                    break
        for i, b in enumerate(blobs):
            b["id"] = i + 1
    return blobs


def draw_blob_overlay(dst, tracked, show_trails=True, max_trail=16, opacity=0.6):
    arr = np.array(dst, dtype=np.float32)
    h, w = arr.shape[:2]
    a = int(max(0, min(255, opacity * 255)))
    for tb in tracked:
        mx0 = max(0, tb["minx"])
        my0 = max(0, tb["miny"])
        mx1 = min(w - 1, tb["maxx"])
        my1 = min(h - 1, tb["maxy"])
        # Green bounding box
        for x in range(mx0, mx1 + 1):
            if my0 < h:
                arr[my0, x, 1] = (arr[my0, x, 1] * (255 - a) + 255 * a) / 255
            if my1 < h and my1 != my0:
                arr[my1, x, 1] = (arr[my1, x, 1] * (255 - a) + 255 * a) / 255
        for y in range(my0, my1 + 1):
            if mx0 < w:
                arr[y, mx0, 1] = (arr[y, mx0, 1] * (255 - a) + 255 * a) / 255
            if mx1 < w and mx1 != mx0:
                arr[y, mx1, 1] = (arr[y, mx1, 1] * (255 - a) + 255 * a) / 255
        cx, cy = int(tb["cx"] + 0.5), int(tb["cy"] + 0.5)
        for dx in range(-3, 4):
            px = cx + dx
            if 0 <= px < w and 0 <= cy < h:
                arr[cy, px, 0] = (arr[cy, px, 0] * (255 - a) + 255 * a) / 255
        for dy in range(-3, 4):
            py = cy + dy
            if 0 <= cx < w and 0 <= py < h:
                arr[py, cx, 0] = (arr[py, cx, 0] * (255 - a) + 255 * a) / 255
        if show_trails and len(tb.get("trail", [])) >= 2:
            trail = tb["trail"]
            dl = min(len(trail), max_trail)
            for i in range(dl - 1):
                x1 = int(trail[-dl + i][0] + 0.5)
                y1 = int(trail[-dl + i][1] + 0.5)
                x2 = int(trail[-dl + i + 1][0] + 0.5)
                y2 = int(trail[-dl + i + 1][1] + 0.5)
                ldx = abs(x2 - x1)
                sx = 1 if x1 < x2 else -1
                ldy = abs(y2 - y1)
                sy = 1 if y1 < y2 else -1
                err = ldx - ldy
                cx2, cy2 = x1, y1
                while True:
                    if 0 <= cx2 < w and 0 <= cy2 < h:
                        arr[cy2, cx2, 2] = (
                            arr[cy2, cx2, 2] * (255 - a) + 255 * a
                        ) / 255
                    if cx2 == x2 and cy2 == y2:
                        break
                    e2 = 2 * err
                    if e2 > -ldy:
                        err -= ldy
                        cx2 += sx
                    if e2 < ldx:
                        err += ldx
                        cy2 += sy
    out = np.clip(arr, 0, 255).astype(np.uint8)
    return Image.fromarray(out, "RGBA")


def render_blobtrack_demo():
    frame = make_test_pattern()
    arr = np.array(frame, dtype=np.uint8)
    gray = np.dot(arr[..., :3], [0.299, 0.587, 0.114]).astype(np.uint8)
    blobs = blob_detect(
        gray, threshold=0.5, min_area=30, max_area=5000, proximity_merge=15
    )
    tracked = []
    for b in blobs:
        tracked.append({**b, "trail": [(b["cx"], b["cy"])]})
    overlay = draw_blob_overlay(frame, tracked, opacity=0.6)
    overlay.save(os.path.join(OUT, "metro-blobtrack-overlay.png"))
    side = Image.new("RGBA", (W * 2, H))
    side.paste(frame, (0, 0))
    side.paste(overlay, (W, 0))
    side.save(os.path.join(OUT, "metro-blobtrack-comparison.png"))
    # GIF: tracking across frames
    nf = 20
    frames = []
    trail_store = {}
    for fi in range(nf):
        t = fi / nf * 2 * math.pi
        frame_f = make_test_pattern(t=t)
        arr_f = np.array(frame_f, dtype=np.uint8)
        gray_f = np.dot(arr_f[..., :3], [0.299, 0.587, 0.114]).astype(np.uint8)
        blobs_f = blob_detect(
            gray_f, threshold=0.5, min_area=30, max_area=5000, proximity_merge=15
        )
        tracked_f = []
        new_store = {}
        for b in blobs_f:
            best_d = 50.0
            match = None
            for pid, prev in trail_store.items():
                d = math.hypot(b["cx"] - prev["cx"], b["cy"] - prev["cy"])
                if d < best_d:
                    best_d = d
                    match = pid
            if match is not None:
                trail = trail_store[match]["trail"][:]
                bid = trail_store[match]["id"]
            else:
                trail = []
                bid = b["id"]
            trail.append((b["cx"], b["cy"]))
            if len(trail) > 16:
                trail = trail[-16:]
            entry = {**b, "id": bid, "trail": trail}
            new_store[bid] = entry
            tracked_f.append(entry)
        trail_store = new_store
        frames.append(draw_blob_overlay(frame_f, tracked_f, opacity=0.6))
    if frames:
        frames[0].save(
            os.path.join(OUT, "metro-blobtrack-tracking.gif"),
            save_all=True,
            append_images=frames[1:],
            duration=150,
            loop=0,
        )
    print("  metro-blobtrack: done")


# ── metro-sample ────────────────────────────────────────────────────────


def render_sample_demo():
    img = Image.new("RGBA", (W, H), (30, 30, 40, 255))
    draw = ImageDraw.Draw(img)
    try:
        fl = ImageFont.truetype(
            "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 28
        )
        fm = ImageFont.truetype(
            "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 18
        )
        fs = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 14)
    except Exception:
        fl = fm = fs = ImageFont.load_default()
    draw.rectangle([0, 0, W, 32], fill=(50, 50, 60, 255))
    draw.text(
        (12, 6),
        "DaVinci Resolve Studio 18 — OpenFX",
        fill=(200, 200, 200, 255),
        font=fs,
    )
    draw.rectangle([0, 32, 180, H], fill=(40, 40, 50, 255))
    draw.text((8, 38), "OpenFX Plugins", fill=(180, 180, 180, 255), font=fs)
    draw.rectangle([6, 62, 174, 82], fill=(55, 55, 70, 255))
    draw.text((10, 64), "  Metro Design", fill=(220, 180, 80, 255), font=fs)
    plugins = [
        "    Metro ASCII Art",
        "    Metro Blob Tracker",
        "    Metro Chroma. Aberration",
        "    Metro Sample",
    ]
    for i, p in enumerate(plugins):
        c = (255, 200, 100, 255) if i == 3 else (180, 180, 180, 255)
        draw.text((8, 88 + i * 24), p, fill=c, font=fs)
    draw.rectangle([180, 32, W, H], fill=(25, 25, 32, 255))
    draw.text((195, 45), "Metro Sample Plugin", fill=(255, 255, 255, 255), font=fl)
    draw.text(
        (195, 78),
        "Reference plugin — Metro OFX Plugin Framework",
        fill=(180, 180, 180, 255),
        font=fs,
    )
    info = [
        "Identifier: com.metrodesign.sample",
        "Version: 1.0.0",
        "Category: Metro Design",
        "",
        "Parameters:",
        "  Enable: [X]",
        "  Opacity: [=========>---] 0.75",
    ]
    for i, line in enumerate(info):
        c = (200, 200, 200) if line == "Parameters:" else (140, 140, 160)
        draw.text((195, 108 + i * 22), line, fill=c, font=fs)
    img.save(os.path.join(OUT, "metro-sample-plugin.png"))
    print("  metro-sample: done")


# ── Main ───────────────────────────────────────────────────────────────


def main():
    os.makedirs(OUT, exist_ok=True)
    print("Generating Metro Effects Pack v0.1.0 screenshots...")
    for name, fn in [
        ("metro-sample", render_sample_demo),
        ("metro-ascii", render_ascii_demo),
        ("metro-chromab", render_chromab_demo),
        ("metro-blobtrack", render_blobtrack_demo),
    ]:
        print(f"\n[{name}]")
        fn()
    print()
    files = sorted(os.listdir(OUT))
    media = [f for f in files if f.endswith((".png", ".gif"))]
    print(f"Generated {len(media)} files:")
    for f in media:
        sz = os.path.getsize(os.path.join(OUT, f))
        print(f"  {f:40s} {sz:>8,} bytes")


if __name__ == "__main__":
    main()

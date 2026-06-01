#!/usr/bin/env python3
"""Generate screenshots and GIFs for Metro Effects Pack v0.1.0 launch.

Faithfully implements each plugin's rendering algorithm.
Output: screenshots/ directory with PNG and GIF files.
"""

from PIL import Image, ImageDraw, ImageFont
import math
import os
import numpy as np

OUT = os.path.dirname(os.path.abspath(__file__))
W, H = 640, 360  # 16:9 output


def _clamp(v, lo=0.0, hi=1.0):
    return max(lo, min(hi, v))


def luminance(r, g, b):
    return 0.299 * r + 0.587 * g + 0.114 * b


# ── Test pattern generator ──────────────────────────────────────────────


def make_test_pattern(w=W, h=H, t=0.0):
    """Generate a colorful 16:9 test scene with gradient sky, shapes, text."""
    img = Image.new("RGBA", (w, h))
    pix = img.load()
    for y in range(h):
        for x in range(w):
            u = x / w
            v = y / h
            # Gradient sky
            r = 0.1 + 0.6 * (1 - v)
            g = 0.2 + 0.7 * (1 - v)
            b = 0.5 + 0.5 * (1 - v)
            # Sun
            cx, cy = 0.8, 0.25
            sd = math.hypot(u - cx, v - cy)
            if sd < 0.08:
                r, g, b = 1.0, 0.9, 0.3
            elif sd < 0.1:
                glow = (0.1 - sd) / 0.02
                r = r * (1 - glow) + 1.0 * glow
                g = g * (1 - glow) + 0.9 * glow
                b = b * (1 - glow) + 0.3 * glow
            # Ground
            if v > 0.7:
                r, g, b = 0.3, 0.6, 0.2
            # Path
            if v > 0.7 and abs(u - 0.5) < 0.08 + 0.06 * (v - 0.7) / 0.3:
                r, g, b = 0.5, 0.4, 0.3
            # Moving ball (for blobtrack)
            bx = 0.2 + 0.6 * (0.5 + 0.5 * math.sin(t * 2.0))
            by = 0.55 + 0.15 * math.cos(t * 3.0)
            bd = math.hypot(u - bx, v - by)
            if bd < 0.04:
                r, g, b = 1.0, 0.2, 0.2
            # Second moving ball
            bx2 = 0.2 + 0.6 * (0.5 + 0.5 * math.cos(t * 1.7 + 1.0))
            by2 = 0.58 + 0.12 * math.sin(t * 2.3 + 0.5)
            bd2 = math.hypot(u - bx2, v - by2)
            if bd2 < 0.03:
                r, g, b = 0.2, 0.6, 1.0
            pix[x, y] = (int(r * 255), int(g * 255), int(b * 255), 255)
    # Draw text label
    draw = ImageDraw.Draw(img)
    try:
        font = ImageFont.truetype(
            "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 28
        )
        font_small = ImageFont.truetype(
            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 16
        )
    except Exception:
        font = ImageFont.load_default()
        font_small = font
    draw.text(
        (20, 20), "Metro Effects Pack v0.1.0", fill=(255, 255, 255, 200), font=font
    )
    return img


# ── metro-ascii faithful implementation ─────────────────────────────────

ASCII_CHARSET = " .:-=+*#%@"


def ascii_map_luminance(luma):
    clamped = _clamp(luma)
    idx = int(clamped * (len(ASCII_CHARSET) - 1) + 0.5)
    idx = max(0, min(idx, len(ASCII_CHARSET) - 1))
    return ASCII_CHARSET[idx]


def apply_ascii(src, cell_size=8, font_aspect=0.5, color_pass=True, contrast=1.0):
    """Apply ASCII art effect matching ASCIIPlugin::render algorithm."""
    arr = np.array(src, dtype=np.float32) / 255.0
    h, w = arr.shape[:2]
    dst = arr.copy()
    cols = w // cell_size
    rows = h // cell_size
    contrast_scale = contrast
    contrast_offset = (1.0 - contrast_scale) * 0.5
    for row in range(rows):
        for col in range(cols):
            y0 = row * cell_size
            x0 = col * cell_size
            tile = dst[y0 : y0 + cell_size, x0 : x0 + cell_size]
            avg = tile.mean(axis=(0, 1))
            luma = luminance(avg[0], avg[1], avg[2])
            stretched = (luma - 0.5) * contrast_scale + 0.5 + contrast_offset
            stretched = _clamp(stretched)
            ch = ascii_map_luminance(stretched)
            # Determine density
            density = 0 if ch == " " else (ord(ch) - ord(" ")) * 4
            density = min(density, 255)
            char_r = avg[0] if color_pass else 1.0
            char_g = avg[1] if color_pass else 1.0
            char_b = avg[2] if color_pass else 1.0
            # Draw character as a small block
            char_w = int(cell_size * font_aspect)
            if char_w < 1:
                char_w = 1
            if char_w > cell_size:
                char_w = cell_size
            stroke_h = max(1, cell_size // 5)
            center_x = x0 + (cell_size - char_w) // 2
            center_y = y0 + cell_size // 3
            blend = density / 255.0
            for dy in range(stroke_h):
                py = center_y + dy
                if py >= y0 + cell_size or py >= h:
                    break
                for dx in range(char_w):
                    px = center_x + dx
                    if px >= x0 + cell_size or px >= w:
                        break
                    dst[py, px, 0] = dst[py, px, 0] * (1 - blend) + char_r * blend
                    dst[py, px, 1] = dst[py, px, 1] * (1 - blend) + char_g * blend
                    dst[py, px, 2] = dst[py, px, 2] * (1 - blend) + char_b * blend
                    dst[py, px, 3] = 1.0
    out = (np.clip(dst, 0, 1) * 255).astype(np.uint8)
    return Image.fromarray(out, "RGBA")


def render_ascii_demo():
    """Generate metro-ascii before/after PNG and parameter GIF."""
    src = make_test_pattern()
    src.save(os.path.join(OUT, "metro-ascii-before.png"))
    after = apply_ascii(
        src, cell_size=8, font_aspect=0.5, color_pass=True, contrast=1.2
    )
    after.save(os.path.join(OUT, "metro-ascii-after.png"))
    # Before/after side-by-side
    side = Image.new("RGBA", (W * 2, H))
    side.paste(src, (0, 0))
    side.paste(after, (W, 0))
    side.save(os.path.join(OUT, "metro-ascii-comparison.png"))
    # GIF: varying cell size
    frames = []
    for cs in range(4, 20, 2):
        frames.append(apply_ascii(src, cell_size=cs, font_aspect=0.5, color_pass=True))
    for cs in range(18, 4, -2):
        frames.append(apply_ascii(src, cell_size=cs, font_aspect=0.5, color_pass=True))
    if frames:
        frames[0].save(
            os.path.join(OUT, "metro-ascii-cell-size.gif"),
            save_all=True,
            append_images=frames[1:],
            duration=200,
            loop=0,
        )
    # GIF: varying contrast
    frames = []
    for c in [c * 0.3 for c in range(0, 14)]:
        frames.append(apply_ascii(src, contrast=c))
    for c in [c * 0.3 for c in range(12, 0, -1)]:
        frames.append(apply_ascii(src, contrast=c))
    if frames:
        frames[0].save(
            os.path.join(OUT, "metro-ascii-contrast.gif"),
            save_all=True,
            append_images=frames[1:],
            duration=200,
            loop=0,
        )
    print("  metro-ascii: before, after, comparison, cell-size GIF, contrast GIF")


# ── metro-chromab faithful implementation ──────────────────────────────


def bilerp(arr, fx, fy, ch):
    """Bilinear interpolation matching ChromaticAberrationPlugin::bilerp."""
    h, w = arr.shape[:2]
    cx = _clamp(fx, 0, w - 1)
    cy = _clamp(fy, 0, h - 1)
    ix, iy = int(cx), int(cy)
    dx, dy = cx - ix, cy - iy
    ix1 = min(ix + 1, w - 1)
    iy1 = min(iy + 1, h - 1)
    p00 = arr[iy, ix, ch]
    p10 = arr[iy, ix1, ch]
    p01 = arr[iy1, ix, ch]
    p11 = arr[iy1, ix1, ch]
    top = p00 + dx * (p10 - p00)
    bot = p01 + dx * (p11 - p01)
    return top + dy * (bot - top)


def apply_chromab(
    src,
    r_shift=(0, 0),
    g_shift=(0, 0),
    b_shift=(0, 0),
    radial_falloff=0.0,
    stretch_angle=0.0,
    mix=1.0,
):
    """Apply chromatic aberration matching ChromaticAberrationPlugin::render."""
    arr = np.array(src, dtype=np.float32) / 255.0
    h, w = arr.shape[:2]
    dst = np.zeros_like(arr)
    angle_rad = math.radians(stretch_angle)
    cos_a, sin_a = math.cos(angle_rad), math.sin(angle_rad)
    for y in range(h):
        for x in range(w):
            cx = (x / w) * 2.0 - 1.0
            cy = (y / h) * 2.0 - 1.0
            dist = math.hypot(cx, cy)
            falloff = dist ** (1.0 + radial_falloff * 4.0)
            r_off_x = r_shift[0] * falloff
            r_off_y = r_shift[1] * falloff
            g_off_x = g_shift[0] * falloff
            g_off_y = g_shift[1] * falloff
            b_off_x = b_shift[0] * falloff
            b_off_y = b_shift[1] * falloff
            r_rot_x = r_off_x * cos_a - r_off_y * sin_a
            r_rot_y = r_off_x * sin_a + r_off_y * cos_a
            g_rot_x = g_off_x * cos_a - g_off_y * sin_a
            g_rot_y = g_off_x * sin_a + g_off_y * cos_a
            b_rot_x = b_off_x * cos_a - b_off_y * sin_a
            b_rot_y = b_off_x * sin_a + b_off_y * cos_a
            orig_r = arr[y, x, 0]
            orig_g = arr[y, x, 1]
            orig_b = arr[y, x, 2]
            orig_a = arr[y, x, 3]
            shift_r = bilerp(arr, x + r_rot_x, y + r_rot_y, 0)
            shift_g = bilerp(arr, x + g_rot_x, y + g_rot_y, 1)
            shift_b = bilerp(arr, x + b_rot_x, y + b_rot_y, 2)
            dst[y, x, 0] = orig_r + mix * (shift_r - orig_r)
            dst[y, x, 1] = orig_g + mix * (shift_g - orig_g)
            dst[y, x, 2] = orig_b + mix * (shift_b - orig_b)
            dst[y, x, 3] = orig_a
    out = (np.clip(dst, 0, 1) * 255).astype(np.uint8)
    return Image.fromarray(out, "RGBA")


def render_chromab_demo():
    """Generate metro-chromab before/after PNG and parameter GIF."""
    src = make_test_pattern()
    src.save(os.path.join(OUT, "metro-chromab-before.png"))
    after = apply_chromab(
        src,
        r_shift=(6, 0),
        g_shift=(0, 0),
        b_shift=(-6, 0),
        radial_falloff=0.0,
        stretch_angle=0.0,
        mix=1.0,
    )
    after.save(os.path.join(OUT, "metro-chromab-after.png"))
    side = Image.new("RGBA", (W * 2, H))
    side.paste(src, (0, 0))
    side.paste(after, (W, 0))
    side.save(os.path.join(OUT, "metro-chromab-comparison.png"))
    # GIF: varying shift amount
    frames = []
    for s in range(0, 15):
        frames.append(
            apply_chromab(
                src,
                r_shift=(s, 0),
                b_shift=(-s, 0),
                radial_falloff=0.0,
                stretch_angle=0.0,
                mix=1.0,
            )
        )
    for s in range(13, 0, -1):
        frames.append(
            apply_chromab(
                src,
                r_shift=(s, 0),
                b_shift=(-s, 0),
                radial_falloff=0.0,
                stretch_angle=0.0,
                mix=1.0,
            )
        )
    if frames:
        frames[0].save(
            os.path.join(OUT, "metro-chromab-shift.gif"),
            save_all=True,
            append_images=frames[1:],
            duration=150,
            loop=0,
        )
    # GIF: varying radial falloff
    frames = []
    for rf in [r * 0.1 for r in range(0, 11)]:
        frames.append(
            apply_chromab(
                src,
                r_shift=(10, 0),
                b_shift=(-10, 0),
                radial_falloff=rf,
                stretch_angle=0.0,
                mix=1.0,
            )
        )
    for rf in [r * 0.1 for r in range(9, -1, -1)]:
        frames.append(
            apply_chromab(
                src,
                r_shift=(10, 0),
                b_shift=(-10, 0),
                radial_falloff=rf,
                stretch_angle=0.0,
                mix=1.0,
            )
        )
    if frames:
        frames[0].save(
            os.path.join(OUT, "metro-chromab-falloff.gif"),
            save_all=True,
            append_images=frames[1:],
            duration=200,
            loop=0,
        )
    print("  metro-chromab: before, after, comparison, shift GIF, falloff GIF")


# ── metro-blobtrack faithful implementation ────────────────────────────


def blob_detect(gray, threshold=0.5, min_area=10, max_area=5000, proximity_merge=10.0):
    """Blob detection matching BlobDetector::detect algorithm (4-connectivity)."""
    h, w = gray.shape
    raw_thresh = int(max(0.001, min(0.999, threshold)) * 255)
    labels = np.zeros((h, w), dtype=np.int32)
    next_label = 0
    # Simple union-find
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
            if gray[y, x] < raw_thresh:
                labels[y, x] = 0
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
    # Resolve labels & accumulate stats
    stats = {}
    for y in range(h):
        for x in range(w):
            l = labels[y, x]
            if l == 0:
                continue
            root = find(l)
            labels[y, x] = root
            if root not in stats:
                stats[root] = {
                    "area": 0,
                    "sum_x": 0.0,
                    "sum_y": 0.0,
                    "min_x": w,
                    "min_y": h,
                    "max_x": 0,
                    "max_y": 0,
                }
            s = stats[root]
            s["area"] += 1
            s["sum_x"] += x
            s["sum_y"] += y
            s["min_x"] = min(s["min_x"], x)
            s["min_y"] = min(s["min_y"], y)
            s["max_x"] = max(s["max_x"], x)
            s["max_y"] = max(s["max_y"], y)
    blobs = []
    for root, s in stats.items():
        if s["area"] == 0:
            continue
        if s["area"] < min_area:
            continue
        if s["area"] > max_area:
            continue
        blobs.append(
            {
                "id": len(blobs) + 1,
                "area": s["area"],
                "centroid_x": s["sum_x"] / s["area"],
                "centroid_y": s["sum_y"] / s["area"],
                "min_x": s["min_x"],
                "min_y": s["min_y"],
                "max_x": s["max_x"],
                "max_y": s["max_y"],
            }
        )
    # Proximity merge
    if proximity_merge > 0 and len(blobs) > 1:
        merge_dist_sq = proximity_merge**2
        merged = True
        while merged:
            merged = False
            for i in range(len(blobs)):
                for j in range(i + 1, len(blobs)):
                    dx = blobs[i]["centroid_x"] - blobs[j]["centroid_x"]
                    dy = blobs[i]["centroid_y"] - blobs[j]["centroid_y"]
                    if dx * dx + dy * dy <= merge_dist_sq:
                        a1, a2 = blobs[i]["area"], blobs[j]["area"]
                        ta = a1 + a2
                        blobs[i]["centroid_x"] = (
                            blobs[i]["centroid_x"] * a1 + blobs[j]["centroid_x"] * a2
                        ) / ta
                        blobs[i]["centroid_y"] = (
                            blobs[i]["centroid_y"] * a1 + blobs[j]["centroid_y"] * a2
                        ) / ta
                        blobs[i]["area"] = ta
                        blobs[i]["min_x"] = min(blobs[i]["min_x"], blobs[j]["min_x"])
                        blobs[i]["min_y"] = min(blobs[i]["min_y"], blobs[j]["min_y"])
                        blobs[i]["max_x"] = max(blobs[i]["max_x"], blobs[j]["max_x"])
                        blobs[i]["max_y"] = max(blobs[i]["max_y"], blobs[j]["max_y"])
                        del blobs[j]
                        merged = True
                        break
                if merged:
                    break
        for i, b in enumerate(blobs):
            b["id"] = i + 1
    return blobs


def draw_blob_overlay(
    dst, tracked_blobs, show_trails=True, max_trail_len=16, opacity=0.5
):
    """Draw overlay matching BlobTrackPlugin::drawOverlay algorithm."""
    arr = np.array(dst, dtype=np.float32)
    h, w = arr.shape[:2]
    alpha = int(opacity * 255)
    alpha = max(0, min(255, alpha))
    for tb in tracked_blobs:
        min_x = max(0, tb["min_x"])
        min_y = max(0, tb["min_y"])
        max_x = min(w - 1, tb["max_x"])
        max_y = min(h - 1, tb["max_y"])
        # Top/bottom edges (green)
        for x in range(min_x, max_x + 1):
            if min_y < h:
                arr[min_y, x, 1] = (
                    arr[min_y, x, 1] * (255 - alpha) + 255 * alpha
                ) / 255
            if max_y < h and max_y != min_y:
                arr[max_y, x, 1] = (
                    arr[max_y, x, 1] * (255 - alpha) + 255 * alpha
                ) / 255
        # Left/right edges (green)
        for y in range(min_y, max_y + 1):
            if min_x < w:
                arr[y, min_x, 1] = (
                    arr[y, min_x, 1] * (255 - alpha) + 255 * alpha
                ) / 255
            if max_x < w and max_x != min_x:
                arr[y, max_x, 1] = (
                    arr[y, max_x, 1] * (255 - alpha) + 255 * alpha
                ) / 255
        # Centroid crosshair (red)
        cx, cy = int(tb["centroid_x"] + 0.5), int(tb["centroid_y"] + 0.5)
        for dx in range(-3, 4):
            px = cx + dx
            if 0 <= px < w and 0 <= cy < h:
                arr[cy, px, 0] = (arr[cy, px, 0] * (255 - alpha) + 255 * alpha) / 255
        for dy in range(-3, 4):
            py = cy + dy
            if 0 <= cx < w and 0 <= py < h:
                arr[py, cx, 0] = (arr[py, cx, 0] * (255 - alpha) + 255 * alpha) / 255
        # Trails (blue lines)
        if show_trails and len(tb.get("trail", [])) >= 2:
            trail = tb["trail"]
            draw_len = min(len(trail), max_trail_len)
            for i in range(draw_len - 1):
                x1, y1 = trail[-draw_len + i]
                x2, y2 = trail[-draw_len + i + 1]
                # Bresenham line
                ldx = abs(x2 - x1)
                sx = 1 if x1 < x2 else -1
                ldy = abs(y2 - y1)
                sy = 1 if y1 < y2 else -1
                err = ldx - ldy
                cx2, cy2 = x1, y1
                while True:
                    if 0 <= cx2 < w and 0 <= cy2 < h:
                        arr[cy2, cx2, 2] = (
                            arr[cy2, cx2, 2] * (255 - alpha) + 255 * alpha
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
    """Generate metro-blobtrack overlay PNG and tracking GIF."""
    # Generate a frame with moving blobs
    frame = make_test_pattern()
    arr = np.array(frame, dtype=np.uint8)
    gray = np.dot(arr[..., :3], [0.299, 0.587, 0.114]).astype(np.uint8)
    blobs = blob_detect(
        gray, threshold=0.5, min_area=30, max_area=5000, proximity_merge=15
    )
    tracked = []
    for b in blobs:
        tracked.append(
            {
                **b,
                "trail": [(b["centroid_x"], b["centroid_y"])],
            }
        )
    overlay = draw_blob_overlay(frame, tracked, show_trails=True, opacity=0.6)
    overlay.save(os.path.join(OUT, "metro-blobtrack-overlay.png"))
    # Composite: original + overlay side-by-side
    side = Image.new("RGBA", (W * 2, H))
    side.paste(frame, (0, 0))
    side.paste(overlay, (W, 0))
    side.save(os.path.join(OUT, "metro-blobtrack-comparison.png"))
    # GIF: tracking across multiple frames (simulate motion)
    n_frames = 30
    frames = []
    trail_store = {}
    for fi in range(n_frames):
        t = fi / n_frames * 2 * math.pi
        frame_f = make_test_pattern(t=t)
        arr_f = np.array(frame_f, dtype=np.uint8)
        gray_f = np.dot(arr_f[..., :3], [0.299, 0.587, 0.114]).astype(np.uint8)
        blobs_f = blob_detect(
            gray_f, threshold=0.5, min_area=30, max_area=5000, proximity_merge=15
        )
        tracked_f = []
        new_trail = {}
        for b in blobs_f:
            # Simple ID matching (nearest neighbor)
            bid = b["id"]
            best_d = 1e9
            best_prev = None
            for pid, prev in trail_store.items():
                d = math.hypot(
                    b["centroid_x"] - prev["centroid_x"],
                    b["centroid_y"] - prev["centroid_y"],
                )
                if d < best_d:
                    best_d = d
                    best_prev = pid
            if best_prev is not None and best_d < 50:
                trail = trail_store[best_prev]["trail"][:]
                bid = trail_store[best_prev]["id"]
            else:
                trail = []
            trail.append((b["centroid_x"], b["centroid_y"]))
            if len(trail) > 16:
                trail = trail[-16:]
            new_trail[bid] = {
                **b,
                "id": bid,
                "trail": trail,
                "centroid_x": b["centroid_x"],
                "centroid_y": b["centroid_y"],
                "min_x": b["min_x"],
                "min_y": b["min_y"],
                "max_x": b["max_x"],
                "max_y": b["max_y"],
            }
            tracked_f.append(new_trail[bid])
        trail_store = new_trail
        overlay_f = draw_blob_overlay(frame_f, tracked_f, show_trails=True, opacity=0.6)
        frames.append(overlay_f)
    if frames:
        frames[0].save(
            os.path.join(OUT, "metro-blobtrack-tracking.gif"),
            save_all=True,
            append_images=frames[1:],
            duration=100,
            loop=0,
        )
    print("  metro-blobtrack: overlay, comparison, tracking GIF")


# ── metro-sample screenshot ─────────────────────────────────────────────


def render_sample_demo():
    """Generate metro-sample screenshot showing plugin info."""
    img = Image.new("RGBA", (W, H), (30, 30, 40, 255))
    draw = ImageDraw.Draw(img)
    try:
        font_l = ImageFont.truetype(
            "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 36
        )
        font_m = ImageFont.truetype(
            "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 24
        )
        font_s = ImageFont.truetype(
            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 18
        )
    except Exception:
        font_l = font_m = font_s = ImageFont.load_default()
    # DaVinci Resolve-style UI mockup
    draw.rectangle([0, 0, W, 40], fill=(50, 50, 60, 255))
    draw.text(
        (20, 8),
        "DaVinci Resolve Studio 18 — OpenFX",
        fill=(200, 200, 200, 255),
        font=font_s,
    )
    draw.rectangle([0, 40, 220, H], fill=(40, 40, 50, 255))
    draw.text((10, 50), "OpenFX Plugins", fill=(180, 180, 180, 255), font=font_s)
    draw.rectangle([10, 80, 210, 105], fill=(55, 55, 70, 255))
    draw.text((15, 84), "  Metro Design", fill=(220, 180, 80, 255), font=font_s)
    plugins = [
        "    Metro ASCII Art",
        "    Metro Blob Tracker",
        "    Metro Chromatic Aberration",
        "    Metro Sample",
    ]
    for i, p in enumerate(plugins):
        yp = 110 + i * 28
        color = (255, 200, 100, 255) if i == 3 else (180, 180, 180, 255)
        draw.text((15, yp), p, fill=color, font=font_s)
    draw.rectangle([220, 40, W, H], fill=(25, 25, 32, 255))
    draw.text((240, 60), "Metro Sample Plugin", fill=(255, 255, 255, 255), font=font_l)
    draw.text(
        (240, 105),
        "Reference plugin — Metro OFX Plugin Framework",
        fill=(180, 180, 180, 255),
        font=font_s,
    )
    draw.text(
        (240, 140),
        "Identifier: com.metrodesign.sample",
        fill=(120, 120, 140, 255),
        font=font_s,
    )
    draw.text((240, 165), "Version: 1.0.0", fill=(120, 120, 140, 255), font=font_s)
    draw.text(
        (240, 190), "Category: Metro Design", fill=(120, 120, 140, 255), font=font_s
    )
    draw.text((240, 230), "Parameters:", fill=(200, 200, 200, 255), font=font_m)
    params = [
        "  Enable: [X]",
        "  Opacity: [=========>---] 0.75",
    ]
    for i, p in enumerate(params):
        draw.text((240, 265 + i * 30), p, fill=(160, 160, 180, 255), font=font_s)
    img.save(os.path.join(OUT, "metro-sample-plugin.png"))
    print("  metro-sample: plugin info screenshot")


# ── Main ───────────────────────────────────────────────────────────────


def main():
    os.makedirs(OUT, exist_ok=True)
    print("Generating Metro Effects Pack v0.1.0 screenshots...")
    print()
    print("[metro-sample]")
    render_sample_demo()
    print()
    print("[metro-ascii]")
    render_ascii_demo()
    print()
    print("[metro-chromab]")
    render_chromab_demo()
    print()
    print("[metro-blobtrack]")
    render_blobtrack_demo()
    print()
    # Summary
    files = sorted(os.listdir(OUT))
    print(
        f"Generated {len([f for f in files if f.endswith(('.png', '.gif'))])} files in {OUT}/"
    )
    for f in files:
        if f.endswith((".png", ".gif")) and f != "generate.py":
            fpath = os.path.join(OUT, f)
            size = os.path.getsize(fpath)
            print(f"  {f:40s} {size:>8,} bytes")


if __name__ == "__main__":
    main()

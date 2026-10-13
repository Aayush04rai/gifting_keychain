"""Generates the news-card category artwork (PNG) for the SD card.

Each image is a transparent RGBA illustration (glowing, layered, in the
category tint) that fills the right half of the card and fades out on its left
edge, so the card's own gradient shows through and no seam is possible. Run
build_assets.py afterwards to convert to LVGL .bin files.

    python tools/make_art.py            # system python with Pillow
"""
import math, os, random
from PIL import Image, ImageDraw, ImageFilter, ImageChops

W, H = 176, 172   # art size; card.c places it at the top-right corner
S = 3             # supersample for the crisp layer

CATS = {  # name: (accent, scene)
    "ai": (0x5b8cff, "network"), "robotics": (0x5b8cff, "gear"), "semi": (0x6aa8ff, "chip"),
    "space": (0x9a8cff, "rocket"), "tech": (0x3ec6d8, "hex"), "cyber": (0xb06bff, "shield"),
    "finance": (0x2fd08f, "chart"), "ev": (0x39d98a, "ev"), "health": (0x3fd0b8, "heart"),
    "defence": (0xe0a050, "radar"), "f1": (0xff6b5b, "flag"), "football": (0x7bd35b, "ball"), "travel": (0x5cc8ff, "plane"), "generic": (0x8aa0c0, "globe"),
}


def rgb(v): return ((v >> 16) & 255, (v >> 8) & 255, v & 255)
def mix(a, b, t): return tuple(round(a[i] + (b[i] - a[i]) * t) for i in range(3))


class Canvas:
    """Draw crisp shapes at S x resolution; glow() adds a blurred halo of a layer."""

    def __init__(self):
        self.base = Image.new("RGBA", (W, H), (0, 0, 0, 0))

    def layer(self):
        return Image.new("RGBA", (W * S, H * S), (0, 0, 0, 0))

    def put(self, lay, glow=0, glow_alpha=1.0):
        small = lay.resize((W, H), Image.LANCZOS)
        if glow:
            g = small.filter(ImageFilter.GaussianBlur(glow))
            if glow_alpha != 1.0:
                g.putalpha(g.getchannel("A").point(lambda v: int(v * glow_alpha)))
            self.base.alpha_composite(g)
        self.base.alpha_composite(small)


def grad_fill(size, c0, c1, vertical=True):
    w, h = size
    g = Image.new("RGB", size)
    px = g.load()
    for y in range(h):
        for x in range(w):
            t = (y / max(1, h - 1)) if vertical else (x / max(1, w - 1))
            px[x, y] = mix(c0, c1, t)
    return g


def masked_gradient(lay, mask, c0, c1):
    """Paste a vertical gradient through `mask` (L) onto lay."""
    g = grad_fill(lay.size, c0, c1).convert("RGBA")
    g.putalpha(mask)
    lay.alpha_composite(g)


def orb(cv, cx, cy, r, col, a=110, blur=26):
    m = Image.new("L", (W, H), 0)
    ImageDraw.Draw(m).ellipse([cx - r, cy - r, cx + r, cy + r], fill=a)
    m = m.filter(ImageFilter.GaussianBlur(blur))
    t = Image.new("RGBA", (W, H), col + (0,))
    t.putalpha(m)
    cv.base.alpha_composite(t)


def sc(*v): return [x * S for x in v]


def stars(cv, col, n=26, seed=3):
    rnd = random.Random(seed)
    lay = cv.layer()
    d = ImageDraw.Draw(lay)
    for _ in range(n):
        x, y, r = rnd.uniform(40, W - 4), rnd.uniform(26, H - 6), rnd.choice((.6, .8, 1.1))
        d.ellipse(sc(x - r, y - r, x + r, y + r), fill=mix(col, (255, 255, 255), .7) + (rnd.randint(90, 210),))
    cv.put(lay)


def scene(kind, col):
    cv = Canvas()
    cx, cy = 104, 96
    light = mix(col, (255, 255, 255), .55)
    dark = mix(col, (6, 10, 24), .55)
    orb(cv, cx, cy, 56, col, 120, 24)

    lay = cv.layer(); d = ImageDraw.Draw(lay)
    if kind == "network":
        rnd = random.Random(7)
        pts = [(cx, cy)] + [(cx + math.cos(a) * r, cy + math.sin(a) * r * .9)
                            for a, r in [(i * 0.9 + rnd.uniform(-.2, .2), rnd.choice((30, 48, 62))) for i in range(7)]]
        for i, p in enumerate(pts[1:], 1):
            d.line(sc(*pts[0], *p), fill=col + (150,), width=int(1.4 * S))
            q = pts[(i % (len(pts) - 1)) + 1]
            d.line(sc(*p, *q), fill=col + (80,), width=S)
        for i, (x, y) in enumerate(pts):
            r = 9 if i == 0 else rnd.choice((3.5, 4.5, 5.5))
            d.ellipse(sc(x - r, y - r, x + r, y + r), fill=(light if i == 0 else col) + (255,))
        cv.put(lay, glow=5)
    elif kind == "gear":
        teeth, ro, ri = 10, 44, 34
        poly = []
        for i in range(teeth * 2):
            a = math.pi * i / teeth
            r = ro if i % 2 == 0 else ri
            for da in (-.12, .12):
                poly.append((cx + math.cos(a + da) * r, cy + math.sin(a + da) * r))
        d.polygon([(x * S, y * S) for x, y in poly], fill=dark + (255,), outline=col + (255,), width=int(2.2 * S))
        d.ellipse(sc(cx - 18, cy - 18, cx + 18, cy + 18), fill=(8, 12, 26, 255), outline=light + (255,), width=int(2.4 * S))
        d.ellipse(sc(cx - 6, cy - 6, cx + 6, cy + 6), fill=light + (255,))
        cv.put(lay, glow=6)
    elif kind == "chip":
        s = 36
        for i in range(-3, 4):
            o = i * 10
            for sgn in (-1, 1):
                d.rounded_rectangle(sc(cx + o - 2, cy + sgn * s - (0 if sgn > 0 else 12) + (0 if sgn < 0 else 0),
                                       cx + o + 2, cy + sgn * s + (12 if sgn > 0 else 0)), 1, fill=col + (200,))
                d.rounded_rectangle(sc(cx + sgn * s - (0 if sgn > 0 else 12), cy + o - 2,
                                       cx + sgn * s + (12 if sgn > 0 else 0), cy + o + 2), 1, fill=col + (200,))
        d.rounded_rectangle(sc(cx - s, cy - s, cx + s, cy + s), 8 * S, fill=(10, 16, 34, 255), outline=col + (255,), width=int(2.2 * S))
        d.rounded_rectangle(sc(cx - 20, cy - 20, cx + 20, cy + 20), 5 * S, fill=dark + (255,), outline=light + (255,), width=int(1.6 * S))
        d.rounded_rectangle(sc(cx - 8, cy - 8, cx + 8, cy + 8), 3 * S, fill=light + (255,))
        cv.put(lay, glow=5)
    elif kind == "rocket":
        stars(cv, col)
        # exhaust
        ex = Image.new("L", (W, H), 0)
        ImageDraw.Draw(ex).polygon([(cx - 9, cy + 34), (cx + 9, cy + 34), (cx, cy + 82)], fill=230)
        ex = ex.filter(ImageFilter.GaussianBlur(5))
        t = Image.new("RGBA", (W, H), (255, 170, 80, 0)); t.putalpha(ex); cv.base.alpha_composite(t)
        # body
        d.polygon(sc(cx, cy - 52, cx + 15, cy - 14, cx + 15, cy + 34, cx - 15, cy + 34, cx - 15, cy - 14),
                  fill=(226, 232, 248, 255))
        d.polygon(sc(cx, cy - 52, cx + 15, cy - 14, cx - 15, cy - 14), fill=col + (255,))
        d.ellipse(sc(cx - 7, cy - 6, cx + 7, cy + 8), fill=(18, 28, 60, 255), outline=light + (255,), width=int(1.8 * S))
        d.polygon(sc(cx - 15, cy + 12, cx - 30, cy + 40, cx - 15, cy + 34), fill=col + (255,))
        d.polygon(sc(cx + 15, cy + 12, cx + 30, cy + 40, cx + 15, cy + 34), fill=col + (255,))
        cv.put(lay, glow=4, glow_alpha=.8)
    elif kind == "hex":
        for k, (dx, dy, r) in enumerate([(0, 0, 40), (-34, -22, 16), (34, -20, 14), (-30, 30, 13), (36, 32, 16)]):
            pts = [(cx + dx + math.cos(math.pi / 3 * i + .5) * r, cy + dy + math.sin(math.pi / 3 * i + .5) * r) for i in range(6)]
            d.polygon([(x * S, y * S) for x, y in pts], fill=(dark if k else (10, 16, 34)) + (255,), outline=(col if k else light) + (255,), width=int(2 * S))
        pts = [(cx - 4, cy - 22), (cx - 13, cy + 3), (cx - 2, cy + 3), (cx - 7, cy + 24), (cx + 13, cy - 6), (cx + 2, cy - 6), (cx + 7, cy - 22)]
        d.polygon([(x * S, y * S) for x, y in pts], fill=light + (255,))
        cv.put(lay, glow=5)
    elif kind == "shield":
        pts = [(cx, cy - 52), (cx + 40, cy - 36), (cx + 38, cy + 12), (cx, cy + 54), (cx - 38, cy + 12), (cx - 40, cy - 36)]
        d.polygon([(x * S, y * S) for x, y in pts], fill=(255, 255, 255, 255))
        m = lay.getchannel("A")
        lay = cv.layer()
        masked_gradient(lay, m, mix(col, (255, 255, 255), .25), dark)
        d = ImageDraw.Draw(lay)
        d.line([(x * S, y * S) for x, y in pts + [pts[0]]], fill=light + (255,), width=int(2.4 * S), joint="curve")
        d.rounded_rectangle(sc(cx - 14, cy - 4, cx + 14, cy + 22), 4 * S, fill=(236, 240, 255, 255))
        d.arc(sc(cx - 9, cy - 24, cx + 9, cy + 2), 180, 360, fill=(236, 240, 255, 255), width=int(3.4 * S))
        d.ellipse(sc(cx - 3, cy + 4, cx + 3, cy + 10), fill=dark + (255,))
        cv.put(lay, glow=6)
    elif kind == "chart":
        rnd = random.Random(2)
        ys = [cy + 36, cy + 26, cy + 30, cy + 10, cy + 14, cy - 8, cy - 4, cy - 30]
        xs = [cx - 56 + i * 16 for i in range(8)]
        area = Image.new("L", (W * S, H * S), 0)
        ImageDraw.Draw(area).polygon([(x * S, y * S) for x, y in zip(xs, ys)] + [(xs[-1] * S, (cy + 52) * S), (xs[0] * S, (cy + 52) * S)], fill=255)
        masked = cv.layer()
        g = grad_fill(masked.size, col, (6, 10, 24), True).convert("RGBA")
        g.putalpha(ImageChops.multiply(area, Image.new("L", area.size, 70)))
        masked.alpha_composite(g)
        cv.put(masked)
        for gy in (cy - 30, cy - 5, cy + 20, cy + 45):
            d.line(sc(cx - 60, gy, cx + 64, gy), fill=col + (36,), width=S)
        d.line([(x * S, y * S) for x, y in zip(xs, ys)], fill=light + (255,), width=int(2.6 * S), joint="curve")
        for x, y in zip(xs, ys):
            d.ellipse(sc(x - 2.4, y - 2.4, x + 2.4, y + 2.4), fill=(255, 255, 255, 255))
        d.polygon(sc(xs[-1] + 4, ys[-1] - 14, xs[-1] + 12, ys[-1] + 2, xs[-1] - 4, ys[-1] - 2), fill=light + (255,))
        cv.put(lay, glow=5)
    elif kind == "ev":
        d.rounded_rectangle(sc(cx - 54, cy + 4, cx + 54, cy + 30), 9 * S, fill=dark + (255,), outline=col + (255,), width=int(2.2 * S))
        d.polygon(sc(cx - 34, cy + 4, cx - 20, cy - 22, cx + 24, cy - 22, cx + 42, cy + 4), fill=dark + (255,), outline=col + (255,))
        d.line(sc(cx - 34, cy + 4, cx - 20, cy - 22, cx + 24, cy - 22, cx + 42, cy + 4, cx - 34, cy + 4), fill=col + (255,), width=int(2.2 * S), joint="curve")
        for dx in (-30, 30):
            d.ellipse(sc(cx + dx - 11, cy + 20, cx + dx + 11, cy + 42), fill=(8, 12, 26, 255), outline=light + (255,), width=int(2.4 * S))
        bolt = [(cx + 4, cy - 18), (cx - 8, cy + 2), (cx, cy + 2), (cx - 4, cy + 18), (cx + 10, cy - 2), (cx + 2, cy - 2)]
        d.polygon([(x * S, y * S + 0) for x, y in [(x, y - 1) for x, y in bolt]], fill=light + (255,))
        cv.put(lay, glow=5)
    elif kind == "heart":
        pts = []
        for i in range(120):
            t = i / 120 * 2 * math.pi
            pts.append((cx + 3.1 * 16 * math.sin(t) ** 3 / 1.0 * .9,
                        cy - 2.4 * (13 * math.cos(t) - 5 * math.cos(2 * t) - 2 * math.cos(3 * t) - math.cos(4 * t)) * .95 - 4))
        d.polygon([(x * S, y * S) for x, y in pts], fill=(255, 255, 255, 255))
        m = lay.getchannel("A"); lay = cv.layer()
        masked_gradient(lay, m, mix(col, (255, 255, 255), .2), dark)
        d = ImageDraw.Draw(lay)
        ecg = [(cx - 52, cy + 4), (cx - 24, cy + 4), (cx - 16, cy - 14), (cx - 6, cy + 24), (cx + 4, cy - 28), (cx + 12, cy + 4), (cx + 52, cy + 4)]
        d.line([(x * S, y * S) for x, y in ecg], fill=(255, 255, 255, 255), width=int(2.4 * S), joint="curve")
        cv.put(lay, glow=5)
    elif kind == "radar":
        for r, a in ((52, 200), (36, 160), (20, 120)):
            d.ellipse(sc(cx - r, cy - r, cx + r, cy + r), outline=col + (a,), width=int(1.8 * S))
        d.line(sc(cx - 52, cy, cx + 52, cy), fill=col + (90,), width=S)
        d.line(sc(cx, cy - 52, cx, cy + 52), fill=col + (90,), width=S)
        sweep = Image.new("L", (W * S, H * S), 0)
        ImageDraw.Draw(sweep).pieslice(sc(cx - 52, cy - 52, cx + 52, cy + 52), -80, -20, fill=120)
        t = cv.layer(); t.paste(Image.new("RGBA", t.size, light + (255,)), (0, 0), sweep); lay.alpha_composite(t)
        for (x, y) in ((cx + 22, cy - 26), (cx - 18, cy + 20)):
            d.ellipse(sc(x - 3.5, y - 3.5, x + 3.5, y + 3.5), fill=(255, 255, 255, 255))
        cv.put(lay, glow=5)
    elif kind == "plane":
        arc = [(cx - 56 + i * 7, cy + 40 - math.sin(i / 16 * math.pi) * 54) for i in range(17)]
        for i in range(0, len(arc) - 1, 2):
            d.line(sc(*arc[i], *arc[i + 1]), fill=col + (170,), width=int(1.8 * S))
        px, py = arc[12]
        pl = [(px + 24, py - 6), (px - 6, py - 22), (px - 2, py - 4), (px - 24, py + 4), (px - 22, py + 8), (px - 2, py + 8),
              (px - 4, py + 20), (px + 4, py + 14)]
        d.polygon([(x * S, y * S) for x, y in pl], fill=(236, 242, 255, 255), outline=light + (255,))
        for (x, y) in (arc[0], arc[-1]):
            d.ellipse(sc(x - 4, y - 4, x + 4, y + 4), fill=light + (255,))
        cv.put(lay, glow=5)
    elif kind == "flag":
        d.line(sc(cx - 34, cy + 52, cx - 34, cy - 50), fill=light + (255,), width=int(3 * S))
        x0, y0, cw, ch = cx - 31, cy - 48, 15, 13
        for row in range(4):
            for colm in range(5):
                fill = (255, 255, 255, 255) if (row + colm) % 2 == 0 else (14, 18, 30, 255)
                d.rectangle(sc(x0 + colm * cw, y0 + row * ch, x0 + (colm + 1) * cw, y0 + (row + 1) * ch), fill=fill)
        d.rectangle(sc(x0, y0, x0 + 5 * cw, y0 + 4 * ch), outline=col + (255,), width=int(2 * S))
        cv.put(lay, glow=5)
    elif kind == "ball":
        r = 42
        d.ellipse(sc(cx - r, cy - r, cx + r, cy + r), fill=(236, 242, 255, 255), outline=col + (255,), width=int(2.4 * S))
        pent = [(cx + math.cos(math.radians(-90 + 72 * i)) * 15, cy + math.sin(math.radians(-90 + 72 * i)) * 15) for i in range(5)]
        d.polygon([(x * S, y * S) for x, y in pent], fill=(18, 26, 44, 255))
        for i in range(5):
            a = math.radians(-90 + 72 * i)
            d.line(sc(cx + math.cos(a) * 15, cy + math.sin(a) * 15, cx + math.cos(a) * 34, cy + math.sin(a) * 34), fill=(18, 26, 44, 255), width=int(2.4 * S))
            px_, py_ = cx + math.cos(a) * 38, cy + math.sin(a) * 38
            d.ellipse(sc(px_ - 7, py_ - 7, px_ + 7, py_ + 7), fill=(18, 26, 44, 255))
        cv.put(lay, glow=5)
    else:  # globe
        r = 44
        d.ellipse(sc(cx - r, cy - r, cx + r, cy + r), fill=(10, 16, 34, 255), outline=col + (255,), width=int(2.2 * S))
        for k in (.35, .7):
            d.ellipse(sc(cx - r * k, cy - r, cx + r * k, cy + r), outline=col + (150,), width=int(1.4 * S))
        for yy in (-.5, 0, .5):
            w = math.sqrt(1 - yy * yy) * r
            d.line(sc(cx - w, cy + yy * r, cx + w, cy + yy * r), fill=col + (150,), width=int(1.4 * S))
        cv.put(lay, glow=5)

    # Fade the illustration out toward the card's text (left) and under the
    # status bar (top), so it melts into the card instead of sitting on it.
    a = cv.base.getchannel("A")
    ramp = Image.new("L", (W, H), 255)
    px = ramp.load()
    for y in range(H):
        for x in range(W):
            v = min(1.0, x / 60.0) * min(1.0, max(0.0, (y - 14) / 22.0))
            px[x, y] = int(255 * v)
    cv.base.putalpha(ImageChops.multiply(a, ramp))
    return cv.base


if __name__ == "__main__":
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "art_png")
    os.makedirs(out, exist_ok=True)
    for n, (a, k) in CATS.items():
        scene(k, rgb(a)).save(os.path.join(out, f"cat_{n}.png"))
        print("cat_%s.png" % n)


def home_art():
    """The greeting card's background: a dusk mountain scene, darker on the left
    where the text sits. Opaque 320 x 172 (converted as plain RGB565)."""
    w, h = 320, 172
    sky = [(0.0, (12, 18, 38)), (0.45, (40, 30, 84)), (0.75, (150, 70, 120)), (1.0, (240, 140, 90))]
    im = Image.new("RGB", (w, h))
    px = im.load()
    for y in range(h):
        t = y / (h - 1)
        for i in range(len(sky) - 1):
            if sky[i][0] <= t <= sky[i + 1][0]:
                k = (t - sky[i][0]) / (sky[i + 1][0] - sky[i][0])
                c = mix(sky[i][1], sky[i + 1][1], k)
                break
        for x in range(w):
            px[x, y] = c
    big = im.resize((w * 2, h * 2), Image.LANCZOS)
    d = ImageDraw.Draw(big, "RGBA")
    rnd = random.Random(11)

    def ridge(base, amp, col, seed):
        r = random.Random(seed)
        pts = [(0, h * 2)]
        x, y = 0, base
        while x <= w * 2:
            pts.append((x, y - abs(math.sin(x / 70.0 + seed)) * amp - r.uniform(0, amp * .35)))
            x += 14
        pts.append((w * 2, h * 2))
        d.polygon(pts, fill=col)

    d.ellipse([w * 1.3, h * 1.05, w * 1.7, h * 1.65], fill=(255, 190, 120, 70))  # low sun glow
    ridge(h * 1.55, 70, (70, 48, 110, 255), 1)
    ridge(h * 1.72, 55, (44, 30, 78, 255), 3)
    ridge(h * 1.88, 40, (22, 16, 44, 255), 5)
    im = big.resize((w, h), Image.LANCZOS)
    # darken the left for legibility
    shade = Image.new("L", (w, h))
    sp = shade.load()
    for x in range(w):
        for y in range(h):
            sp[x, y] = int(170 * max(0.0, 1.0 - x / (w * 0.75)))
    im = Image.composite(Image.new("RGB", (w, h), (8, 12, 22)), im, shade)
    return im


if __name__ == "__main__":
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "art_png")
    home_art().save(os.path.join(out, "home.png"))
    print("home.png")

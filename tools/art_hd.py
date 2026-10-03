# Detailed character art, painted from shaded shapes rather than typed pixel by pixel.
# Each sprite is drawn at twice the world's density (2 sprite pixels per world cell) and facing right.
#   python tools/art_hd.py preview out.png   renders every sprite at 6x (needs Pillow)
#   python tools/art_hd.py emit > sprites_hd.h
# Shapes are lit from the upper left: every primitive picks its tone from a ramp (dark -> light) using
# its surface normal, so volumes read without hand-placed highlights. A dark outline goes on last.
import sys, math

L = (-0.55, -0.65, 0.53)
_n = math.sqrt(sum(v * v for v in L)); L = tuple(v / _n for v in L)

# ramps, dark -> light
STEEL = [(36, 40, 48), (70, 76, 88), (118, 126, 140), (176, 184, 196), (230, 234, 240)]
DARKSTEEL = [(14, 14, 18), (26, 26, 32), (42, 42, 50), (66, 66, 76), (110, 110, 122)]
BLUESTEEL = [(24, 30, 46), (40, 52, 78), (64, 80, 116), (98, 116, 156), (150, 166, 200)]
SKIN = [(104, 64, 46), (152, 102, 76), (200, 146, 108), (228, 182, 146)]
GINGER = [(84, 42, 20), (132, 68, 32), (182, 104, 50), (220, 150, 82)]
BLOND = [(104, 78, 38), (152, 118, 60), (200, 164, 92), (230, 202, 136)]
FUR = [(58, 46, 36), (96, 80, 62), (140, 120, 94), (186, 166, 136)]
BLUE = [(28, 38, 56), (44, 62, 88), (66, 90, 122), (94, 122, 156)]
RED = [(64, 16, 18), (112, 30, 28), (162, 50, 42), (200, 82, 64)]
DRED = [(36, 10, 12), (62, 18, 20), (92, 28, 28), (124, 42, 38)]
LEATHER = [(46, 30, 20), (78, 54, 34), (116, 82, 52), (154, 114, 74)]
TROUSER = [(36, 30, 26), (60, 48, 38), (88, 72, 56), (116, 96, 74)]
BROWNCLOTH = [(44, 36, 30), (70, 58, 46), (100, 84, 66), (130, 112, 88)]
GREYCLOTH = [(40, 40, 42), (66, 66, 68), (98, 98, 100), (134, 132, 130)]
WRAP = [(70, 64, 56), (108, 100, 88), (146, 138, 122), (180, 172, 156)]
WOOD = [(56, 36, 22), (92, 62, 38), (132, 94, 58), (166, 124, 80)]
CREAM = [(130, 116, 92), (178, 164, 134), (214, 202, 172), (236, 228, 204)]
GOLD = [(96, 70, 26), (156, 118, 44), (210, 170, 74), (242, 214, 130)]
BONE = [(96, 90, 76), (150, 142, 120), (200, 192, 168), (234, 228, 208)]
EMBER = [(150, 30, 20), (230, 70, 40), (255, 150, 90)]
OUTLINE = (18, 14, 14)


def hsh(x, y, s=0):
    h = (x * 374761393 + y * 668265263 + s * 2246822519) & 0xffffffff
    h = ((h ^ (h >> 13)) * 1274126177) & 0xffffffff
    return ((h ^ (h >> 16)) & 0xffffff) / 16777216.0


class Canvas:
    def __init__(self, w, h):
        self.w, self.h = w, h
        self.px = [[None] * w for _ in range(h)]

    def put(self, x, y, c):
        if 0 <= x < self.w and 0 <= y < self.h and c is not None:
            self.px[y][x] = c

    def get(self, x, y):
        return self.px[y][x] if 0 <= x < self.w and 0 <= y < self.h else None

    # tone from a lit normal, nudged by a texture
    def tone(self, ramp, nx, ny, nz, x, y, tex):
        l = nx * L[0] + ny * L[1] + nz * L[2]
        t = (l + 0.05) / 1.25  # most of a surface sits in the mid tones; only edges facing the light catch the top one
        if tex == 'mail': t += 0.22 if (x + (y // 1) % 2) % 2 == 0 else -0.12
        elif tex == 'fur': t += (hsh(x, y, 3) - 0.5) * 0.6
        elif tex == 'cloth': t += 0.1 * math.sin(x * 1.7 + y * 0.15)
        elif tex == 'wood': t += -0.15 if (y + x // 2) % 3 == 0 else 0.0
        elif tex == 'wrap': t += -0.25 if (y + x // 3) % 3 == 0 else 0.05
        elif tex == 'flat': t = 0.55 + (t - 0.55) * 0.35
        t = max(0.0, min(0.999, t))
        return ramp[int(t * len(ramp))]

    def ellipse(self, cx, cy, rx, ry, ramp, tex=None, clip=None):
        for y in range(int(cy - ry) - 1, int(cy + ry) + 2):
            for x in range(int(cx - rx) - 1, int(cx + rx) + 2):
                nx, ny = (x + 0.5 - cx) / rx, (y + 0.5 - cy) / ry
                d = nx * nx + ny * ny
                if d > 1 or (clip and not clip(x, y)): continue
                self.put(x, y, self.tone(ramp, nx, ny, math.sqrt(1 - d), x, y, tex))

    def capsule(self, x0, y0, x1, y1, r, ramp, tex=None):
        dx, dy = x1 - x0, y1 - y0
        ll = dx * dx + dy * dy or 1
        for y in range(int(min(y0, y1) - r) - 1, int(max(y0, y1) + r) + 2):
            for x in range(int(min(x0, x1) - r) - 1, int(max(x0, x1) + r) + 2):
                px, py = x + 0.5, y + 0.5
                t = max(0, min(1, ((px - x0) * dx + (py - y0) * dy) / ll))
                cx, cy = x0 + dx * t, y0 + dy * t
                nx, ny = (px - cx) / r, (py - cy) / r
                d = nx * nx + ny * ny
                if d > 1: continue
                self.put(x, y, self.tone(ramp, nx, ny, math.sqrt(1 - d), x, y, tex))

    def poly(self, pts, ramp, tex=None, bulge=0.8):
        ys = [p[1] for p in pts]
        for y in range(int(min(ys)), int(math.ceil(max(ys))) + 1):
            yc = y + 0.5
            xs = []
            for i in range(len(pts)):
                (ax, ay), (bx, by) = pts[i], pts[(i + 1) % len(pts)]
                if (ay <= yc < by) or (by <= yc < ay):
                    xs.append(ax + (yc - ay) * (bx - ax) / (by - ay))
            xs.sort()
            for k in range(0, len(xs) - 1, 2):
                xa, xb = xs[k], xs[k + 1]
                mid, half = (xa + xb) / 2, max(0.5, (xb - xa) / 2)
                for x in range(int(round(xa)), int(round(xb))):
                    nx = (x + 0.5 - mid) / half * bulge
                    nx = max(-0.95, min(0.95, nx))
                    self.put(x, y, self.tone(ramp, nx, -0.15, math.sqrt(1 - nx * nx), x, y, tex))

    def line(self, x0, y0, x1, y1, c):
        n = int(max(abs(x1 - x0), abs(y1 - y0))) + 1
        for i in range(n + 1):
            t = i / n
            self.put(int(round(x0 + (x1 - x0) * t)), int(round(y0 + (y1 - y0) * t)), c)

    def outline(self):
        add = []
        for y in range(self.h):
            for x in range(self.w):
                if self.px[y][x] is None and any(self.get(x + a, y + b) not in (None, OUTLINE) for a, b in ((1, 0), (-1, 0), (0, 1), (0, -1))):
                    add.append((x, y))
        for x, y in add: self.px[y][x] = OUTLINE


# ---------------------------------------------------------------- parts

def boot(c, ax, ay, toe, ramp=LEATHER, cuff=FUR):
    """A turnshoe from the ankle (ax, ay) down to the ground at y = c.h - 1, toe pointing `toe` (+1 right)."""
    g = c.h - 1
    c.poly([(ax - 2.6, ay), (ax + 2.6, ay), (ax + 2.6 + (3 if toe > 0 else 0), g - 1.5), (ax + 2.6 + (4 if toe > 0 else 0), g + 1),
            (ax - 2.6 - (4 if toe < 0 else 0), g + 1), (ax - 2.6 - (3 if toe < 0 else 0), g - 1.5)], ramp)
    if cuff: c.ellipse(ax, ay + 0.5, 3.0, 1.4, cuff, 'fur')


def leg(c, hip, knee, ankle, thigh, shin, shin_tex='wrap', r=2.5):
    c.capsule(*hip, *knee, r, thigh, 'cloth')
    c.capsule(*knee, *ankle, r - 0.4, shin, shin_tex)


def round_shield(c, cx, cy, rx, ry, paint):
    """paint(nx, ny) -> ramp for the face; an iron rim and a domed boss."""
    for y in range(int(cy - ry) - 1, int(cy + ry) + 2):
        for x in range(int(cx - rx) - 1, int(cx + rx) + 2):
            nx, ny = (x + 0.5 - cx) / rx, (y + 0.5 - cy) / ry
            d = nx * nx + ny * ny
            if d > 1: continue
            nz = math.sqrt(1 - d)
            if d > 0.78: c.put(x, y, c.tone(STEEL[:4], nx, ny, nz * 0.6, x, y, None))
            else: c.put(x, y, c.tone(paint(nx, ny), nx * 0.35, ny * 0.35, 0.9, x, y, None))
    c.ellipse(cx + 0.3, cy, 2.1, 2.2, STEEL)


def axe_head(c, x, y, s=1.0, ramp=STEEL):
    """A bearded axe blade whose haft meets it at (x, y), edge facing left."""
    c.poly([(x, y - 2 * s), (x - 3 * s, y - 3.5 * s), (x - 5 * s, y - 2 * s), (x - 5.5 * s, y + 2 * s), (x - 3.5 * s, y + 4.5 * s), (x - 1, y + 1.5 * s)], ramp, None, 0.5)
    c.line(x - 5 * s, y - 2 * s, x - 5.5 * s, y + 2 * s, ramp[-1])


def nasal_helm(c, cx, cy, r, ramp=STEEL):
    c.ellipse(cx, cy, r, r * 1.08, ramp, None, clip=lambda x, y: y <= cy + 1)
    c.line(cx - r, cy + 1, cx + r - 0.5, cy + 1, ramp[1])
    c.line(cx - r + 1, cy + 1, cx + r - 1, cy + 1, ramp[2])
    c.line(cx + r - 2, cy + 1, cx + r - 2, cy + 5, ramp[3])  # the nasal
    c.put(int(cx), int(cy - r * 1.05), ramp[3])


# ---------------------------------------------------------------- rigged creatures
# Every creature is painted as separate parts - head, torso, upper arm, forearm, thigh, shin, weapon,
# shield (legs and a tail for four-footed beasts, wings for bats and imps) - each on its own canvas with a
# pivot (the joint it hangs from) and a bone end (the next joint). rig.cpp poses the parts on a skeleton
# every frame (walking, swinging, recoiling) and, when the creature dies, the same skeleton becomes a
# ragdoll, so the limbs flop. Parts face right, at half a world unit a pixel; a limb's length is the
# distance from its pivot to its bone end. Styled after the user's reference sheets (reference/README.md):
# dark, muted, lit from the upper left, red tabards with white crosses for Dunmoor.

ROT = [(36, 46, 28), (60, 76, 44), (92, 110, 66), (128, 146, 94)]
DRAUGRSKIN = [(38, 52, 64), (60, 80, 96), (90, 114, 130), (128, 152, 166)]
GOBLINSKIN = [(40, 54, 26), (68, 90, 40), (104, 128, 62), (142, 164, 92)]
TROLLSKIN = [(44, 46, 30), (72, 76, 48), (104, 108, 72), (138, 140, 100)]
STONE = [(42, 40, 44), (70, 68, 72), (104, 100, 102), (140, 136, 134), (176, 172, 166)]
IMPSKIN = [(80, 18, 14), (140, 36, 22), (196, 72, 36), (236, 132, 76)]
ICEGHOST = [(16, 34, 52), (32, 66, 94), (64, 112, 150), (120, 172, 210), (190, 228, 248)]
PALEGHOST = [(60, 64, 80), (104, 108, 126), (156, 160, 178), (210, 214, 228)]
PURPLE = [(28, 18, 38), (48, 32, 64), (74, 52, 96), (106, 80, 130)]
BLACKCLOTH = [(12, 10, 14), (24, 20, 26), (38, 32, 40), (56, 48, 58)]
BLACKPLATE = [(16, 16, 22), (32, 32, 42), (52, 52, 66), (82, 82, 100), (128, 128, 148)]
RUST = [(48, 28, 18), (82, 50, 28), (120, 78, 44), (156, 110, 68)]
WOLFFUR = [(30, 30, 32), (54, 54, 58), (84, 82, 86), (118, 116, 118), (152, 150, 150)]
HORSE = [(12, 22, 30), (22, 40, 52), (36, 62, 76), (58, 90, 104)]
WEED = [(18, 42, 28), (32, 70, 42), (52, 100, 60), (80, 132, 84)]
BATSKIN = [(30, 20, 24), (54, 36, 40), (82, 56, 58), (112, 82, 82)]
MEMBRANE = [(40, 20, 26), (66, 36, 42), (94, 56, 60)]
OLIVE = [(40, 40, 26), (64, 64, 40), (92, 92, 60), (122, 120, 84)]
CYANGLOW = (150, 255, 240)
REDGLOW = (255, 70, 40)
GREENGLOW = (150, 255, 110)
EYE = (24, 16, 14)


class Part:
    def __init__(self, w, h, piv, end):
        self.c = Canvas(int(math.ceil(w)), int(math.ceil(h)))
        self.piv, self.end = piv, end


# ---- limbs: a bone hanging down from its pivot

def _cap(c, cx, y, w, ramp, tex):
    c.ellipse(cx, y, w / 2, w * 0.42, ramp, tex)  # rounds the joint, and laps over the part above it


def upper_part(L, r, ramp, tex=None, pauldron=None, sleeve=None):
    w = r * 2
    pad = w / 2 + 3
    W, H = w + 8, L + w + 6
    cx = W / 2
    p = Part(W, H, (cx, pad), (cx, pad + L))
    c = p.c
    c.poly([(cx - w / 2, pad - 1), (cx + w / 2, pad - 1), (cx + w * 0.44, pad + L + 1), (cx - w * 0.44, pad + L + 1)], ramp, tex, 0.9)
    _cap(c, cx, pad - 0.5, w, ramp, tex)
    if sleeve: c.poly([(cx - w / 2 - 0.3, pad - 1), (cx + w / 2 + 0.3, pad - 1), (cx + w / 2, pad + L * 0.5), (cx - w / 2, pad + L * 0.5)], sleeve, 'cloth', 0.9)
    if pauldron: c.ellipse(cx, pad, w / 2 + 1.8, w / 2 + 1, pauldron)
    c.outline()
    return p


def fore_part(L, r, ramp, hand, tex=None, cuff=None, claws=None):
    w = r * 2
    pad = w / 2 + 3
    W, H = w + 9, L + w + 8
    cx = W / 2
    p = Part(W, H, (cx, pad), (cx, pad + L))
    c = p.c
    c.poly([(cx - w * 0.45, pad - 1), (cx + w * 0.45, pad - 1), (cx + w * 0.4, pad + L - 1), (cx - w * 0.4, pad + L - 1)], ramp, tex, 0.9)
    _cap(c, cx, pad - 0.5, w * 0.9, ramp, tex)
    if cuff: c.line(cx - w * 0.5, pad + L - 2, cx + w * 0.5, pad + L - 2, cuff[2])
    hw = w * 0.5 + 0.4
    c.poly([(cx - hw, pad + L - 1.5), (cx + hw, pad + L - 1.5), (cx + hw, pad + L + hw * 1.1), (cx - hw + 0.6, pad + L + hw * 1.25)], hand, None, 0.7)  # a fist
    if claws:
        for k in (-1, 0, 1): c.line(cx + k * 1.2, pad + L + hw, cx + k * 1.5 + 0.5, pad + L + hw + 2, claws)
    c.outline()
    return p


def shin_part(L, r, ramp, boot, tex=None, toe=3.0, cuff=None, bare=False):
    w = r * 2
    pad = w / 2 + 3
    W, H = w + toe + 8, L + pad + 3
    cx = pad + 1
    p = Part(W, H, (cx, pad), (cx, pad + L))
    c = p.c
    g = pad + L
    ank = g - (3 if bare else 4.5)
    c.poly([(cx - w / 2, pad - 1), (cx + w / 2, pad - 1), (cx + w * 0.42, ank), (cx - w * 0.42, ank)], ramp, tex, 0.9)
    _cap(c, cx, pad - 0.5, w, ramp, tex)
    c.poly([(cx - w * 0.5, ank - 1), (cx + w * 0.5, ank - 1), (cx + w * 0.5 + toe * 0.6, g - 2), (cx + w * 0.5 + toe, g - 1), (cx + w * 0.5 + toe, g + 0.6), (cx - w * 0.55, g + 0.6)], boot, None, 0.6)
    if bare:
        for k in range(3): c.put(int(cx + w * 0.5 + toe - k * 1.5), int(g), (20, 16, 14))
    if cuff: c.ellipse(cx, ank - 1, w * 0.55 + 0.4, 1.3, cuff, 'fur')
    c.outline()
    return p


def thigh_part(L, r, ramp, tex='cloth', knee=None):
    p = upper_part(L, r, ramp, tex)
    if knee:
        cx = p.piv[0]
        p.c.ellipse(cx, p.end[1], r * 0.95, r * 0.8, knee)
        p.c.outline()
    return p


def bone_limb(L, r, foot=False, hand=False):
    """A skeleton's limb: a thin bone with knobbed ends (and a bony hand or foot)."""
    pad = r + 3
    W, H = 2 * pad + (5 if foot else 2), L + 2 * pad + 2
    cx = pad + 1
    p = Part(W, H, (cx, pad), (cx, pad + L))
    c = p.c
    c.capsule(cx, pad + 1, cx, pad + L - 1, r, BONE)
    c.ellipse(cx, pad + 0.5, r + 0.8, r + 0.6, BONE)
    c.ellipse(cx, pad + L - 0.5, r + 0.7, r + 0.5, BONE)
    if foot: c.poly([(cx - 1, pad + L - 1), (cx + 1.5, pad + L - 1), (cx + 5, pad + L + 0.5), (cx - 1.5, pad + L + 0.5)], BONE)
    if hand:
        for k in (-1, 0, 1): c.line(cx + k * 0.8, pad + L, cx + k * 1.2 + 0.4, pad + L + 2.5, BONE[2])
    c.outline()
    return p


# ---- weapons and shields: the pivot is the grip, the bone runs to the tip

def blade_part(L, w=1.4, ramp=STEEL, guard=GOLD, hilt=3.5):
    W, H = 10 + w * 2, L + hilt + 6
    cx, gy = W / 2, hilt + 2
    p = Part(W, H, (cx, gy), (cx, gy + L))
    c = p.c
    c.ellipse(cx, gy - hilt + 0.5, 1.4, 1.4, guard)
    c.capsule(cx, gy - hilt + 1, cx, gy + 1, 0.8, LEATHER)
    c.capsule(cx - 3.2, gy + 1.5, cx + 3.2, gy + 1.5, 0.75, guard)
    c.poly([(cx - w, gy + 2.5), (cx + w, gy + 2.5), (cx + w * 0.75, gy + L - 2.5), (cx, gy + L), (cx - w * 0.75, gy + L - 2.5)], ramp, None, 0.45)
    c.line(cx, gy + 3, cx, gy + L - 2, ramp[-1])
    c.outline()
    return p


def haft_part(L, head, ramp=WOOD, butt=3, r=0.8):
    """A shafted weapon; `head(c, cx, tipY)` paints its business end at the tip."""
    W, H = 18, L + butt + 10
    cx, gy = W / 2, butt + 2
    p = Part(W, H, (cx, gy), (cx, gy + L))
    p.c.capsule(cx, gy - butt, cx, gy + L - 1, r, ramp, 'wood')
    head(p.c, cx, gy + L)
    p.c.outline()
    return p


def spear_head(c, cx, ty):
    c.poly([(cx, ty + 6), (cx + 1.8, ty), (cx, ty - 3), (cx - 1.8, ty)], STEEL, None, 0.5)
    c.line(cx, ty - 2, cx, ty + 5, STEEL[-1])


def fork_head(c, cx, ty):
    c.line(cx - 3, ty - 1, cx + 3, ty - 1, RUST[2]); c.line(cx - 3, ty - 2, cx + 3, ty - 2, RUST[1])
    for k in (-3, 0, 3):
        c.line(cx + k, ty - 1, cx + k * 1.1, ty + 6, RUST[3] if k < 0 else RUST[2])


def axe_blade(c, cx, ty, s=1.0, ramp=STEEL):
    x, y = cx, ty - 2
    c.poly([(x - 0.5, y - 3 * s), (x - 4 * s, y - 4.5 * s), (x - 6.5 * s, y - 2 * s), (x - 7 * s, y + 3 * s), (x - 4.5 * s, y + 5.5 * s), (x - 0.5, y + 2 * s)], ramp, None, 0.5)
    c.line(x - 6.5 * s, y - 2 * s, x - 7 * s, y + 3 * s, ramp[-1])


def club_head(c, cx, ty, spikes=True, ramp=WOOD):
    c.ellipse(cx, ty - 2, 3.4, 5.5, ramp, 'wood')
    if spikes:
        for k, (dx, dy) in enumerate([(-3.5, -5), (3.5, -4), (-4, 0), (4, 1), (-3, 3.5), (3, 4.5), (0, 3.8)]):
            c.line(cx + dx * 0.8, ty - 2 + dy * 0.8, cx + dx * 1.25, ty - 2 + dy * 1.15, STEEL[3])


def mace_head(c, cx, ty):
    c.ellipse(cx, ty - 1, 3, 3, STEEL)
    for dx, dy in [(-4, -1), (4, -1), (0, 3.5), (-3, 2.5), (3, 2.5), (0, -4.5)]: c.put(int(cx + dx), int(ty - 1 + dy), STEEL[3])


def orb_head(glow, ramp=WOOD, skull=False):
    def h(c, cx, ty):
        c.line(cx - 2, ty - 3, cx - 2, ty + 1, ramp[2]); c.line(cx + 2, ty - 3, cx + 2, ty + 1, ramp[1])  # a forked crown holding it
        if skull:
            c.ellipse(cx, ty + 2, 3.2, 3.4, BONE)
            c.put(int(cx - 1), int(ty + 2), glow); c.put(int(cx + 1), int(ty + 2), glow)
            c.line(cx - 1, ty + 4.5, cx + 1, ty + 4.5, (40, 34, 28))
        else:
            c.ellipse(cx, ty + 2, 2.6, 2.6, [tuple(int(v * k) for v in glow) for k in (0.45, 0.7, 0.9, 1.0)])
            c.put(int(cx - 1), int(ty + 1), (255, 255, 240))
    return h


def bow_part(L, ramp=WOOD):
    """A bow, held at its middle; the bone runs to its upper tip (the game holds it upright)."""
    W, H = 10, 2 * L + 4
    gx, gy = 3, L + 2
    p = Part(W, H, (gx, gy), (gx, gy - L))
    c = p.c
    for i in range(41):
        t = i / 40 * 2 - 1
        x, y = gx + 3.2 * (1 - t * t), gy + t * L
        c.put(int(x), int(y), ramp[2] if t < 0 else ramp[1])
        c.put(int(x) + 1, int(y), ramp[3] if abs(t) < 0.4 else ramp[1])
    c.line(gx, gy - L + 1, gx, gy + L - 1, CREAM[2])  # the string
    c.ellipse(gx + 3.4, gy, 1.2, 1.6, LEATHER)
    c.outline()
    return p


def bomb_part():
    W, H = 12, 14
    p = Part(W, H, (6, 4), (6, 8))
    c = p.c
    c.ellipse(6, 8, 3.6, 3.6, DARKSTEEL)
    c.line(6, 4, 8, 2, CREAM[1])
    c.put(8, 1, EMBER[2]); c.put(9, 1, EMBER[1]); c.put(8, 0, (255, 240, 160))
    c.outline()
    return p


def lantern_part(glow):
    W, H = 12, 18
    p = Part(W, H, (6, 2), (6, 10))
    c = p.c
    c.line(6, 2, 6, 5, IRONC)
    c.poly([(3.5, 5), (8.5, 5), (8.5, 13), (3.5, 13)], DARKSTEEL, None, 0.4)
    c.poly([(4.5, 6.5), (7.5, 6.5), (7.5, 11.5), (4.5, 11.5)], [tuple(int(v * k) for v in glow) for k in (0.5, 0.75, 1.0)], 'flat')
    c.line(3, 5, 9, 5, DARKSTEEL[3]); c.line(3, 13, 9, 13, DARKSTEEL[2])
    c.outline()
    return p


IRONC = (60, 60, 66)


def shield_part(kind, r, paint=None):
    W, H = 2 * r + 4, 2 * r * 1.25 + 4
    cx, cy = W / 2, H / 2
    p = Part(W, H, (cx, cy), (cx, cy - r))
    c = p.c
    if kind == 'round':
        round_shield(c, cx, cy, r, r * 1.1, paint)
    else:  # a heater, red with a white cross (Dunmoor), or black with a red one
        face, mark = paint
        top, bot = cy - r * 1.1, cy + r * 1.2
        pts = [(cx - r, top), (cx + r, top), (cx + r, cy + r * 0.2), (cx, bot), (cx - r, cy + r * 0.2)]
        c.poly(pts, face, None, 0.6)
        for x in range(int(cx - r), int(cx + r) + 1):
            c.put(x, int(top), STEEL[2])
        cw = max(1, r / 4)
        for y in range(int(top + 3), int(bot - 3)):
            for x in range(int(cx - cw / 2), int(cx + cw / 2) + 1):
                if c.get(x, y): c.put(x, y, mark[2] if x <= cx else mark[1])
        for x in range(int(cx - r * 0.55), int(cx + r * 0.55) + 1):
            for y in range(int(cy - r * 0.35 - cw / 2), int(cy - r * 0.35 + cw / 2) + 1):
                if c.get(x, y): c.put(x, y, mark[2] if y <= cy - r * 0.35 else mark[1])
    c.outline()
    return p


def wing_part(L, ramp, bone):
    """A bat wing from its root (pivot) out to its tip, membrane between finger bones."""
    W, H = L + 6, L * 0.75 + 6
    rx, ry = 2, 2
    p = Part(W, H, (rx, ry), (rx + L, ry + 1))
    c = p.c
    tips = [(rx + L, ry + 1), (rx + L * 0.8, ry + L * 0.45), (rx + L * 0.5, ry + L * 0.62), (rx + L * 0.2, ry + L * 0.55)]
    c.poly([(rx, ry)] + tips + [(rx, ry + L * 0.3)], ramp, None, 0.4)
    for tx, ty in tips[:3]: c.line(rx, ry, tx, ty, bone)
    c.outline()
    return p


# ---- torsos

def torso_part(k):
    T, tw, skirt = k['torso'], k['tw'], k.get('skirt', 4)
    style = k['style']
    capeL = k.get('capeL', T + skirt)
    W = tw + 12
    top = 4
    H = top + max(T + skirt, capeL) + 5
    cx = W / 2 + 1
    p = Part(W, H, (cx, top), (cx, top + T))
    c = p.c
    if k.get('cape'):  # behind the body, hanging in tatters
        cl = top + capeL
        pts = [(cx - tw * 0.5 - 1, top), (cx + tw * 0.5 + 1, top), (cx + tw * 0.62, cl - 2)]
        n = 8
        for i in range(n + 1):
            x = cx + tw * 0.62 - (tw * 1.3) * i / n
            pts.append((x, cl - (2 if i % 2 else -1) + hsh(i, 1, 9) * 3))
        pts.append((cx - tw * 0.68, top + 3))
        c.poly(pts, k['cape'], 'cloth', 0.5)
    sx, wx = tw / 2, tw * 0.42
    body = [(cx - sx + 1, top), (cx + sx - 1, top), (cx + sx, top + 1.5), (cx + sx * 0.96, top + T * 0.55), (cx + wx, top + T * 0.85),
            (cx + wx, top + T + 0.5), (cx - wx, top + T + 0.5), (cx - wx, top + T * 0.85), (cx - sx * 0.96, top + T * 0.55), (cx - sx, top + 1.5)]
    if style == 'ribs':  # a skeleton: spine, ribcage, pelvis
        c.capsule(cx, top, cx, top + T, 1.1, BONE)
        for i in range(4):  # ribs curving round both sides of the breastbone
            y = top + 2.5 + i * T * 0.14
            half = sx * (0.95 - i * 0.12)
            for xx in range(-int(half), int(half) + 1):
                yy = y + (abs(xx) / max(1, half)) ** 2 * 2
                c.put(int(cx + xx), int(yy), BONE[2] if xx > -half * 0.3 else BONE[1])
                c.put(int(cx + xx), int(yy) + 1, BONE[1] if abs(xx) > 1 else BONE[2])
        c.ellipse(cx - 0.5, top + T - 0.5, wx * 0.9, 2.2, BONE)
        c.put(int(cx), int(top + T - 1), (30, 26, 22))
        if k.get('rag'):
            c.poly([(cx - sx, top), (cx + sx * 0.6, top), (cx + sx * 0.3, top + T * 0.55), (cx - sx * 0.2, top + T * 0.75), (cx - sx * 1.1, top + T * 0.6)], k['rag'], 'cloth', 0.5)
    elif style == 'ghost':  # a hooded robe trailing away into tatters, no legs beneath
        L = T + skirt
        pts = [(cx - sx, top + 1), (cx + sx - 0.5, top), (cx + sx + 1, top + L * 0.45), (cx + sx * 0.6, top + L * 0.8)]
        for i in range(6):
            pts.append((cx + sx * 0.6 - (sx * 2.2) * i / 5, top + L - (3 if i % 2 else 0) - hsh(i, 2, 4) * 2))
        pts.append((cx - sx - 1, top + L * 0.4))
        c.poly(pts, k['body'], 'cloth', 0.7)
        for i in range(3): c.line(cx - sx * 0.5 + i * 2.5, top + 4, cx - sx * 0.9 + i * 2.8, top + L - 4, k['body'][1])
    else:
        tex = {'mail': 'mail', 'plate': None, 'stone': None}.get(style, 'cloth')
        ramp = k['body']
        if style in ('bare', 'fur'):
            c.poly(body, ramp, None, 0.95)
            c.ellipse(cx + sx * 0.25, top + T * 0.62, sx * 0.85, T * 0.3, ramp)  # a belly
            c.line(cx + sx * 0.3, top + 4, cx + sx * 0.5, top + T * 0.35, ramp[0])  # chest
        elif style == 'stone':
            c.poly(body, STONE, None, 0.9)
            for i in range(5):  # cracks between the blocks
                y0 = top + 2 + i * T / 5
                c.line(cx - sx + 1, y0, cx + sx - 1, y0 + hsh(i, 0, 5) * 2 - 1, STONE[0])
            c.ellipse(cx + 1, top + T * 0.4, 1.5, 1.5, [(40, 120, 110), (80, 200, 180), CYANGLOW])  # the rune at its heart
        else:
            c.poly(body, ramp, tex, 0.9)
        if style == 'plate':
            for y in (top + T * 0.4, top + T * 0.58, top + T * 0.76):
                c.line(cx - sx * 0.8, y, cx + sx * 0.85, y, ramp[0])
                if k.get('gilt'): c.line(cx - sx * 0.8, y + 1, cx + sx * 0.85, y + 1, k['gilt'][1])
            if k.get('gilt'):
                c.ellipse(cx + 0.5, top + T * 0.25, 2.2, 2.0, BONE)  # a skull on the breast
                c.put(int(cx - 0.5), int(top + T * 0.25), (20, 16, 16)); c.put(int(cx + 1.5), int(top + T * 0.25), (20, 16, 16))
        if k.get('tabard'):  # down the front: Dunmoor's colours
            tb = k['tabard']
            tl = top + T + skirt - 1
            c.poly([(cx - sx * 0.45, top + 2), (cx + sx * 0.55, top + 2), (cx + sx * 0.6, tl), (cx - sx * 0.5, tl)], tb, 'cloth', 0.6)
            if k.get('cross'):
                mx, my = cx + 0.5, top + T * 0.45
                for y in range(int(my - 4), int(my + 5)): c.put(int(mx), y, CREAM[2])
                for x in range(int(mx - 2), int(mx + 3)): c.put(x, int(my - 1), CREAM[3])
        if style == 'rags':  # holes in the tunic, rot showing through
            for i in range(4):
                hx, hy = cx + (hsh(i, 3, 1) - 0.5) * tw * 0.8, top + 3 + hsh(i, 4, 1) * (T - 6)
                c.ellipse(hx, hy, 1.3, 1.0, ROT)
        if style == 'robe':
            L = T + skirt
            c.poly([(cx - wx - 0.5, top + T * 0.7), (cx + wx + 0.5, top + T * 0.7), (cx + wx + 3, top + L), (cx - wx - 3.5, top + L)], ramp, 'cloth', 0.7)
            if k.get('trim'):
                c.line(cx + wx + 3, top + L - 0.5, cx - wx - 3.5, top + L - 0.5, k['trim'][2])
                c.line(cx + sx * 0.3, top + 1, cx + wx + 2, top + L - 1, k['trim'][1])
        elif skirt > 0 and style not in ('bare', 'fur', 'stone'):  # a hem below the belt
            sk = k.get('skirtramp', ramp)
            c.poly([(cx - wx - 0.5, top + T - 1), (cx + wx + 0.5, top + T - 1), (cx + wx + 1.5, top + T + skirt), (cx - wx - 1.5, top + T + skirt)], sk, 'cloth' if sk is not ramp else tex, 0.7)
        if k.get('loin'):  # a fur loincloth on a hide belt
            c.poly([(cx - wx - 1, top + T - 1.5), (cx + wx + 1, top + T - 1.5), (cx + wx, top + T + 6), (cx + 1, top + T + 4), (cx - wx, top + T + 6.5)], FUR, 'fur', 0.6)
        if k.get('mantle'): c.ellipse(cx, top + 1.5, sx + 1.5, 3.2, k['mantle'], 'fur')
        if k.get('belt'):
            b = k['belt']
            c.line(cx - wx - 0.5, top + T - 2, cx + wx + 0.5, top + T - 2, b[1]); c.line(cx - wx - 0.5, top + T - 1, cx + wx + 0.5, top + T - 1, b[2])
            c.put(int(cx + wx * 0.4), int(top + T - 2), GOLD[2])
        if k.get('satchel'):
            c.line(cx - sx, top + 1, cx + wx, top + T - 3, LEATHER[2])
            c.ellipse(cx - wx, top + T - 1, 2.6, 2.4, LEATHER)
            c.ellipse(cx - wx - 1, top + T - 3.5, 1.5, 1.5, DARKSTEEL)
    if k.get('wings'): pass
    c.outline()
    return p


# ---- heads: the pivot is the neck, the bone runs up to the crown; the face looks right

def head_part(k):
    hs, style, skin = k['hs'], k['head'], k.get('skin', SKIN)
    W, H = hs * 2 + 14, hs * 2 + 16
    cx = W / 2 - 1
    neck = H - 4
    cy = neck - hs - 1.5
    p = Part(W, H, (cx, neck), (cx, cy - hs))
    c = p.c
    ex, ey = cx + hs * 0.5, cy - 0.3  # where the eye goes
    if style not in ('skull', 'crown', 'golem', 'wraith', 'banshee'):
        c.capsule(cx - 0.6, cy + hs * 0.5, cx - 0.6, neck, hs * 0.36, skin)
    def face(r=hs):
        c.ellipse(cx, cy, r, r * 1.05, skin)
        c.ellipse(cx + r * 0.85, cy + 0.6, 1.0, 1.3, skin)  # nose
    def beard(ramp, long=4):
        c.poly([(cx - hs * 0.2, cy + 1), (cx + hs * 1.0, cy + 1.2), (cx + hs * 0.7, cy + hs + long * 0.5), (cx + hs * 0.1, cy + hs + long), (cx - hs * 0.45, cy + hs * 0.6)], ramp, 'fur', 0.6)
    def eye(col=EYE, w=1):
        for i in range(w): c.put(int(ex) + i, int(ey), col)
    if style == 'kettle':  # a soldier: mail coif and a broad-brimmed iron hat
        c.ellipse(cx - 0.5, cy + 0.5, hs + 0.8, hs + 1.2, STEEL, 'mail')
        face(hs * 0.82)
        c.ellipse(cx - 0.3, cy - hs * 0.45, hs * 0.95, hs * 0.75, STEEL, None, clip=lambda x, y: y <= cy - hs * 0.2)
        c.line(cx - hs - 2.5, cy - hs * 0.25, cx + hs + 2.5, cy - hs * 0.15, STEEL[3]); c.line(cx - hs - 2, cy - hs * 0.25 + 1, cx + hs + 2, cy - hs * 0.15 + 1, STEEL[1])
        eye()
        beard(k.get('beard', BROWNCLOTH), 2)
    elif style == 'great':  # a knight's great helm: flat-topped, an eye slit, breaths, a crest
        c.poly([(cx - hs, cy - hs), (cx + hs * 0.9, cy - hs), (cx + hs + 0.8, cy + hs * 0.7), (cx + hs * 0.5, cy + hs + 1), (cx - hs, cy + hs + 1)], k.get('helm', STEEL), None, 0.9)
        c.line(cx + 0.5, cy - 1, cx + hs + 0.5, cy - 1, (14, 14, 18)); c.line(cx + 0.5, cy, cx + hs + 0.3, cy, (40, 40, 48))
        c.line(cx + hs * 0.35, cy - hs + 1, cx + hs * 0.35, cy + hs, STEEL[3])
        for i in range(3): c.put(int(cx + hs * 0.65), int(cy + 2 + i * 1.5), (20, 20, 24))
        if k.get('crest'):
            for i in range(9): c.line(cx - hs * 0.6 + i * 0.5, cy - hs, cx - hs * 0.9 - i * 0.2, cy - hs - 3 - math.sin(i / 8 * math.pi) * 3, k['crest'][2 if i % 2 else 1])
    elif style in ('skull', 'crown'):
        c.ellipse(cx, cy - 0.5, hs, hs * 1.02, BONE)
        c.poly([(cx - hs * 0.2, cy + hs * 0.4), (cx + hs * 0.95, cy + hs * 0.3), (cx + hs * 0.8, cy + hs + 1.5), (cx - hs * 0.1, cy + hs + 1.2)], BONE, None, 0.6)  # jaw
        glow = k.get('eyes', (20, 16, 16))
        c.ellipse(ex, ey, 1.3, 1.5, [(16, 12, 12), (16, 12, 12)])
        c.put(int(ex), int(ey), glow)
        c.put(int(cx + hs * 0.85), int(cy + 1.5), (30, 24, 20))
        for i in range(4): c.put(int(cx + hs * 0.1 + i * 1.2), int(cy + hs * 0.95), (40, 34, 28))
        if k.get('hood'):
            c.ellipse(cx - 1, cy - 0.5, hs + 1.8, hs + 2, k['hood'], 'cloth', clip=lambda x, y: x < cx + hs * 0.35 or y < cy - hs * 0.55)
        if style == 'crown':
            for i in range(5):
                x = cx - hs + i * hs * 0.48
                c.poly([(x - 0.8, cy - hs + 1), (x, cy - hs - 3 - (i % 2) * 2), (x + 0.8, cy - hs + 1)], GOLD)
            c.line(cx - hs, cy - hs + 1, cx + hs, cy - hs + 1, GOLD[2]); c.put(int(cx), int(cy - hs + 1), (120, 255, 120))
    elif style in ('goblin', 'bomber'):
        c.poly([(cx - hs * 0.4, cy - 1), (cx - hs * 1.9, cy - hs * 0.9), (cx - hs * 0.6, cy + 1.5)], skin, None, 0.6)  # the long ear
        face()
        c.ellipse(cx + hs * 1.05, cy + 1.2, 1.8, 1.6, skin)  # a big nose
        c.put(int(ex), int(ey), (255, 220, 60)); c.put(int(ex) + 1, int(ey), (40, 30, 10))
        c.line(cx + hs * 0.2, cy + hs * 0.6, cx + hs * 0.9, cy + hs * 0.5, (40, 20, 16))
        c.put(int(cx + hs * 0.5), int(cy + hs * 0.7), CREAM[3])
        if style == 'bomber':
            c.ellipse(cx - 0.5, cy - hs * 0.55, hs * 0.95, hs * 0.55, RED, 'cloth', clip=lambda x, y: y <= cy - hs * 0.15)
            c.line(cx - hs, cy - hs * 0.2, cx - hs - 3, cy + 2, RED[2])
    elif style == 'troll':
        face(hs)
        c.ellipse(cx + hs * 0.35, cy - hs * 0.35, hs * 0.75, hs * 0.3, skin)  # the brow
        c.ellipse(cx + hs * 0.95, cy + 1, 2.4, 2.2, skin)  # a heavy nose
        c.ellipse(cx + hs * 0.4, cy + hs * 0.65, hs * 0.65, hs * 0.38, skin)  # the jaw thrust out
        c.line(cx + hs * 0.6, cy + hs * 0.55, cx + hs * 0.75, cy + hs * 0.1, CREAM[3])  # a tusk
        c.put(int(ex), int(ey), (240, 200, 60))
        for i in range(4): c.line(cx - hs * 0.4 + i * 1.5, cy - hs * 0.9, cx - hs * 0.8 + i, cy - hs - 2, FUR[1])
    elif style in ('hood', 'risen'):
        hood = k.get('hood', BROWNCLOTH)
        c.ellipse(cx - 1, cy - 0.5, hs + 1.6, hs + 2, hood, 'cloth')
        c.poly([(cx - hs, cy), (cx + hs * 0.3, cy + 1), (cx + hs * 0.5, neck), (cx - hs * 1.2, neck)], hood, 'cloth', 0.6)
        c.ellipse(cx + hs * 0.55, cy + 0.8, hs * 0.62, hs * 0.85, skin)  # the face in its shadow
        c.line(cx + hs * 0.05, cy - hs * 0.4, cx + hs * 0.05, cy + hs * 0.9, (16, 12, 12))
        glow = k.get('eyes')
        c.put(int(ex + 0.5), int(ey + 0.5), glow if glow else EYE)
        if style == 'risen':
            c.put(int(ex + 1.5), int(ey + 0.5), glow or EYE)
            c.line(cx + hs * 0.6, cy + hs * 0.55, cx + hs * 1.0, cy + hs * 0.5, (30, 16, 14))
            c.put(int(cx + hs * 0.9), int(cy - hs * 0.1), DRED[2])
        if k.get('beard'): beard(k['beard'], 3)
    elif style == 'draugr':
        face(hs * 0.95)
        c.ellipse(cx - 0.3, cy - hs * 0.4, hs * 1.0, hs * 0.75, RUST, None, clip=lambda x, y: y <= cy - hs * 0.15)
        c.line(cx + hs * 0.6, cy - hs * 0.2, cx + hs * 0.6, cy + 2.5, RUST[2])  # nasal
        c.put(int(ex), int(ey + 0.5), CYANGLOW); c.put(int(ex) + 1, int(ey + 0.5), (90, 200, 230))
        beard(GREYCLOTH, 5)
        c.line(cx + hs * 0.35, cy + hs + 1.5, cx + hs * 0.35, cy + hs + 4, GOLD[1])  # a bead in the braid
    elif style == 'redcap':
        face(hs)
        eye(REDGLOW)
        beard(GREYCLOTH, 5)
        c.poly([(cx - hs * 1.05, cy - hs * 0.15), (cx + hs * 0.9, cy - hs * 0.35), (cx - hs * 0.2, cy - hs * 1.3), (cx - hs * 1.6, cy - hs * 2.2), (cx - hs * 1.0, cy - hs * 0.9)], RED, 'cloth', 0.6)
    elif style == 'imp':
        face(hs)
        for d in (-1, 1): c.poly([(cx + d * hs * 0.3 - 1, cy - hs * 0.6), (cx + d * hs * 0.4 - hs * 0.4, cy - hs - 3.5), (cx + d * hs * 0.3 + 1, cy - hs * 0.7)], BONE)
        eye((255, 230, 80), 2)
        c.line(cx + hs * 0.3, cy + hs * 0.55, cx + hs * 0.95, cy + hs * 0.45, (40, 10, 8))
    elif style == 'golem':
        c.poly([(cx - hs, cy - hs * 0.9), (cx + hs * 0.9, cy - hs), (cx + hs + 1, cy + hs * 0.8), (cx - hs * 0.9, cy + hs + 1)], STONE, None, 0.8)
        c.line(cx - hs * 0.5, cy - hs * 0.3, cx + hs * 0.2, cy + hs * 0.5, STONE[0])
        c.line(cx + hs * 0.2, cy - 0.5, cx + hs * 0.95, cy - 0.5, CYANGLOW); c.line(cx + hs * 0.2, cy + 0.5, cx + hs * 0.95, cy + 0.5, (60, 160, 150))
    elif style == 'wraith':
        hood = k.get('hood', ICEGHOST)
        c.ellipse(cx - 1, cy, hs + 1.6, hs + 2.2, hood, 'cloth')
        c.poly([(cx - hs - 1, cy), (cx + hs * 0.5, cy + 2), (cx + hs * 0.4, neck), (cx - hs * 1.3, neck)], hood, 'cloth', 0.6)
        c.ellipse(cx + hs * 0.55, cy + 1, hs * 0.6, hs * 0.85, [(6, 8, 14), (10, 14, 22)])
        g = k.get('eyes', CYANGLOW)
        c.put(int(ex), int(ey + 1), g); c.put(int(ex) + 2, int(ey + 1), g)
    elif style == 'banshee':
        for i in range(9):  # long white hair streaming back
            y0 = cy - hs * 0.8 + i * 1.4
            c.line(cx - hs * 0.2, y0, cx - hs * 2.0 - hsh(i, 1, 2) * 3, y0 + 6 + i * 0.6, PALEGHOST[2 if i % 2 else 3])
        c.ellipse(cx + 0.5, cy, hs * 0.85, hs, PALEGHOST)
        c.ellipse(ex, ey, 1.1, 1.6, [(10, 10, 16), (10, 10, 16)])
        c.ellipse(cx + hs * 0.5, cy + hs * 0.6, 1.0, 1.6, [(10, 10, 16), (10, 10, 16)])  # the mouth, open in a wail
    elif style == 'antler':  # the Black Knight: a closed black helm crowned with antlers, eyes burning
        for d in (-1, 1):
            bx = cx + d * hs * 0.35
            pts = [(bx, cy - hs + 1), (bx - d * 1 - 1.5, cy - hs * 2.2), (bx - 4.5, cy - hs * 2.9)]
            for (a, b) in zip(pts, pts[1:]): c.capsule(*a, *b, 1.4, BONE)
            c.capsule(bx - 1, cy - hs * 1.8, bx + d * 3.5 + 1, cy - hs * 2.7, 1.1, BONE)
            c.capsule(bx - 2.5, cy - hs * 2.4, bx - 1.5, cy - hs * 3.3, 1.0, BONE)
            c.capsule(bx - 3.5, cy - hs * 2.8, bx - 7, cy - hs * 3.0, 0.9, BONE)
        c.poly([(cx - hs, cy - hs), (cx + hs * 0.9, cy - hs), (cx + hs + 1, cy + hs * 0.6), (cx + hs * 0.6, cy + hs + 1.5), (cx - hs, cy + hs + 1.5)], BLACKPLATE, None, 0.9)
        c.line(cx - hs, cy - hs + 1, cx + hs * 0.9, cy - hs + 1, GOLD[1])
        c.line(cx + hs * 0.2, cy - 0.5, cx + hs + 0.5, cy - 0.5, (6, 6, 8))
        c.put(int(cx + hs * 0.55), int(cy - 0.5), REDGLOW); c.put(int(cx + hs * 0.8), int(cy - 0.5), REDGLOW)
        c.line(cx + hs * 0.45, cy - hs + 1, cx + hs * 0.45, cy + hs, DARKSTEEL[3])
    else:  # a bare head, with hair
        face()
        eye()
        c.ellipse(cx - 1, cy - hs * 0.5, hs * 0.95, hs * 0.65, k.get('hair', BROWNCLOTH), 'fur', clip=lambda x, y: x < cx + hs * 0.5)
    if style in ('kettle', 'goblin', 'bomber', 'troll', 'draugr', 'redcap', 'imp', 'bare', 'skull', 'crown', 'hood', 'risen'):
        e1 = c.get(int(ex), int(ey)) or c.get(int(ex + 0.5), int(ey + 0.5))
        if e1: c.put(int(cx - hs * 0.15), int(ey + (0.5 if style in ('hood', 'risen') else 0)), e1)  # the far eye
    c.outline()
    return p


# ---- four-footed beasts and bats

def quad_body(L, d, ramp, tex='fur', mane=None, belly=None):
    """The trunk, from the shoulders (pivot, right) back to the hips (bone end, left)."""
    W, H = L + d + 6, d * 2 + 8
    fx, y = L + d / 2 + 2, d + 3
    p = Part(W, H, (fx, y), (fx - L, y))
    c = p.c
    c.ellipse(fx - L * 0.5, y, L * 0.5 + d * 0.4, d, ramp, tex)
    c.ellipse(fx - 1, y - 0.5, d * 0.9, d * 1.05, ramp, tex)  # the deep chest
    c.ellipse(fx - L + 1, y - 0.5, d * 0.8, d * 0.9, ramp, tex)  # the haunch
    if mane:
        for i in range(12): c.line(fx - i * 1.2 + 1, y - d + 0.5, fx - i * 1.2 - 1, y - d - 2 - hsh(i, 2, 3) * 2, mane[2 if i % 2 else 1])
    if belly: c.ellipse(fx - L * 0.45, y + d * 0.55, L * 0.4, d * 0.35, belly, tex)
    c.outline()
    return p


def wolf_head(L, ramp):
    W, H = L + 8, L * 0.8 + 6
    nx, ny = 4, H * 0.55
    p = Part(W, H, (nx, ny), (nx + L, ny + 1.5))
    c = p.c
    c.ellipse(nx + L * 0.3, ny - 0.5, L * 0.36, L * 0.32, ramp, 'fur')
    c.poly([(nx + L * 0.45, ny - L * 0.2), (nx + L + 0.5, ny + 0.5), (nx + L, ny + 2.5), (nx + L * 0.4, ny + L * 0.22)], ramp, 'fur', 0.6)  # the muzzle
    c.poly([(nx + L * 0.05, ny - L * 0.2), (nx + L * 0.15, ny - L * 0.62), (nx + L * 0.35, ny - L * 0.25)], ramp)  # ear
    c.line(nx + L * 0.55, ny + L * 0.18, nx + L, ny + 2.5, (60, 10, 12))  # the snarl
    for i in range(3): c.put(int(nx + L * 0.65 + i * 1.5), int(ny + L * 0.12), CREAM[3])
    c.put(int(nx + L * 0.48), int(ny - L * 0.12), (230, 190, 60))
    c.put(int(nx + L + 0.5), int(ny + 0.5), (16, 14, 14))
    c.outline()
    return p


def horse_head(L, ramp, mane):
    W, H = L + 10, L + 8
    nx, ny = 4, H - 6
    p = Part(W, H, (nx, ny), (nx + L * 0.75, ny - L * 0.75))
    c = p.c
    c.capsule(nx, ny, nx + L * 0.45, ny - L * 0.55, L * 0.17, ramp)  # the neck
    tip = (nx + L * 0.95, ny - L * 0.4)
    c.capsule(nx + L * 0.45, ny - L * 0.6, tip[0], tip[1], L * 0.14, ramp)  # the long face
    c.poly([(nx + L * 0.38, ny - L * 0.72), (nx + L * 0.42, ny - L * 0.95), (nx + L * 0.52, ny - L * 0.7)], ramp)
    c.put(int(nx + L * 0.55), int(ny - L * 0.62), CYANGLOW)
    c.put(int(tip[0]), int(tip[1] + 1), (10, 10, 14))
    for i in range(10): c.line(nx + i * L * 0.05, ny - i * L * 0.07, nx - 2.5 + i * L * 0.05, ny - i * L * 0.07 + 3 + hsh(i, 5, 1) * 3, mane[i % 3])
    c.outline()
    return p


def tail_part(L, ramp, tex='fur', w=1.8):
    W, H = L + 6, L + 6
    p = Part(W, H, (W - 3, 3), (W - 3 - L * 0.75, 3 + L * 0.65))
    p.c.capsule(W - 3, 3, W - 3 - L * 0.75, 3 + L * 0.65, w, ramp, tex)
    p.c.outline()
    return p


def bat_body(L, ramp):
    W, H = L + 10, L + 8
    cx = W / 2
    p = Part(W, H, (cx, 5), (cx, 5 + L))
    c = p.c
    c.ellipse(cx, 5 + L * 0.55, L * 0.36, L * 0.48, ramp, 'fur')
    c.ellipse(cx + 1, 5 + 1, L * 0.3, L * 0.28, ramp, 'fur')  # head
    for d in (-1, 1): c.poly([(cx + d * 2 - 1, 4), (cx + d * 3, -1), (cx + d * 2 + 1.5, 4)], ramp)
    c.put(int(cx + 2), int(5.5), (255, 80, 60)); c.put(int(cx + 3), int(7), CREAM[3])
    c.outline()
    return p


# ---- the creatures

H_ = dict(hs=6.0, torso=15, tw=14, uarm=7.5, farm=7.5, thigh=9, shin=10, ar=2.4, lr=2.9)


def biped(name, k, weapon=None, wclass="RW_NONE", grip=1.3, shield=None, sgrip=2.5, shoulder=0.13):
    s = k.get('s', 1.0)
    for key in ('hs', 'torso', 'tw', 'uarm', 'farm', 'thigh', 'shin', 'ar', 'lr'):
        k.setdefault(key, H_[key] * s)
    sk = k.get('skin', SKIN)
    parts = {}
    parts['HEAD'] = head_part(k)
    parts['TORSO'] = torso_part(k)
    if k.get('bones'):
        parts['UARM'] = bone_limb(k['uarm'], k['ar'] * 0.55)
        parts['FARM'] = bone_limb(k['farm'], k['ar'] * 0.5, hand=True)
        parts['THIGH'] = bone_limb(k['thigh'], k['lr'] * 0.55)
        parts['SHIN'] = bone_limb(k['shin'], k['lr'] * 0.5, foot=True)
    else:
        parts['UARM'] = upper_part(k['uarm'], k['ar'], k.get('sleeve', sk), k.get('sleevetex', 'cloth'), k.get('pauldron'))
        parts['FARM'] = fore_part(k['farm'], k['ar'] * 0.92, k.get('fore', k.get('sleeve', sk)), k.get('hand', sk), k.get('foretex', 'cloth'), k.get('cuff'), k.get('claws'))
        if k['style'] != 'ghost':
            parts['THIGH'] = thigh_part(k['thigh'], k['lr'], k.get('legs', TROUSER), k.get('legtex', 'cloth'), k.get('knee'))
            parts['SHIN'] = shin_part(k['shin'], k['lr'] * 0.92, k.get('shinramp', k.get('legs', TROUSER)), k.get('boot', LEATHER), k.get('shintex', 'wrap'),
                                      3.0 * s, k.get('bootcuff'), k.get('barefoot', False))
    if weapon: parts['WEAPON'] = weapon
    if shield: parts['SHIELD'] = shield
    if k.get('wing'): parts['WING'] = k['wing']
    kind = 'RK_GHOST' if k['style'] == 'ghost' else 'RK_BIPED'
    RIGS[name] = dict(kind=kind, parts=parts, grip=grip, sgrip=sgrip, shoulder=shoulder, wclass=wclass, sw=k['tw'] / 2 - 1.2, hw=k['tw'] * 0.2)


RIGS = {}
SC = {'s': 1.0}

# Castle Dunmoor's soldiery: kettle hat, mail under a red tabard, the white cross on a red heater, a spear
biped('GUARD', dict(head='kettle', beard=GINGER, style='mail', body=STEEL, tabard=RED, cross=True, skirt=5, skirtramp=RED,
                    sleeve=STEEL, sleevetex='mail', fore=STEEL, foretex='mail', hand=LEATHER, legs=TROUSER, boot=LEATHER, belt=LEATHER),
      haft_part(26, spear_head, butt=10), 'RW_POLE', 2.55, shield_part('heater', 6, (RED, CREAM)))
# its knights: great helms with a red crest, plate over mail, the cross, a longsword
biped('KNIGHT', dict(s=1.05, head='great', crest=RED, style='plate', body=STEEL, tabard=RED, cross=True, skirt=6, skirtramp=STEEL,
                     sleeve=STEEL, sleevetex=None, fore=STEEL, foretex=None, hand=STEEL, pauldron=STEEL, legs=STEEL, legtex='mail',
                     knee=STEEL, shinramp=STEEL, shintex=None, boot=STEEL, belt=LEATHER),
      blade_part(17, 1.5), 'RW_BLADE', 1.35, shield_part('heater', 6.5, (RED, CREAM)))
biped('SKELETON', dict(s=0.95, head='skull', style='ribs', bones=True, skirt=0, tw=10),
      blade_part(13, 1.2, RUST, RUST), 'RW_BLADE', 1.35, shield_part('round', 6, lambda nx, ny: WOOD if ny < 0.1 else DRED))
biped('ARCHER', dict(s=0.95, head='skull', hood=PURPLE, eyes=(200, 120, 255), style='ribs', bones=True, rag=PURPLE, skirt=0, tw=10),
      bow_part(11), 'RW_BOW', 1.6)
biped('GOBLIN', dict(s=0.62, hs=5, head='goblin', skin=GOBLINSKIN, style='tunic', body=LEATHER, skirt=3, legs=LEATHER, shinramp=GOBLINSKIN,
                     boot=GOBLINSKIN, barefoot=True, shintex=None, sleeve=GOBLINSKIN, foretex=None, sleevetex=None, belt=LEATHER),
      haft_part(9, lambda c, cx, ty: axe_blade(c, cx, ty, 0.7, RUST), butt=2), 'RW_BLADE', 1.5)
biped('BOMBER', dict(s=0.62, hs=5, head='bomber', skin=GOBLINSKIN, style='tunic', body=BROWNCLOTH, skirt=3, legs=LEATHER, shinramp=GOBLINSKIN,
                     boot=GOBLINSKIN, barefoot=True, shintex=None, sleeve=GOBLINSKIN, foretex=None, sleevetex=None, satchel=True),
      bomb_part(), 'RW_THROW', 0.0)
biped('CULTIST', dict(head='hood', hood=DRED, eyes=EMBER[2], style='robe', body=DRED, skirt=17, trim=GOLD, sleeve=DRED, fore=DRED, hand=SKIN, belt=LEATHER,
                      legs=BLACKCLOTH, boot=BLACKCLOTH),
      haft_part(24, orb_head((255, 140, 50)), butt=8), 'RW_STAFF', 2.7)
biped('IMP', dict(s=0.55, hs=4.8, head='imp', skin=IMPSKIN, style='bare', body=IMPSKIN, skirt=0, sleeve=IMPSKIN, foretex=None, sleevetex=None, claws=BONE[2],
                  legs=IMPSKIN, legtex=None, shinramp=IMPSKIN, shintex=None, boot=IMPSKIN, barefoot=True, wing=wing_part(14, MEMBRANE, IMPSKIN[0])))
biped('WRAITH', dict(head='wraith', hood=ICEGHOST, style='ghost', body=ICEGHOST, skirt=16, sleeve=ICEGHOST, fore=ICEGHOST, hand=(ICEGHOST[3:] + ICEGHOST[3:])),
      lantern_part(CYANGLOW), 'RW_STAFF', -1.0)
biped('BANSHEE', dict(head='banshee', style='ghost', body=PALEGHOST, skirt=17, sleeve=PALEGHOST, fore=PALEGHOST, hand=PALEGHOST))
biped('GOLEM', dict(s=1.0, hs=6, torso=19, tw=17, uarm=10, farm=10, thigh=10, shin=11, ar=3.6, lr=3.9, head='golem', skin=STONE, style='stone', body=STONE,
                    skirt=0, sleeve=STONE, sleevetex=None, fore=STONE, foretex=None, hand=STONE, legs=STONE, legtex=None, shinramp=STONE, shintex=None, boot=STONE))
biped('REDCAP', dict(s=0.66, hs=5, head='redcap', style='tunic', body=BROWNCLOTH, skirt=3, legs=TROUSER, boot=DARKSTEEL, bootcuff=None,
                     sleeve=BROWNCLOTH, hand=SKIN, belt=LEATHER),
      haft_part(14, lambda c, cx, ty: axe_blade(c, cx, ty, 0.8)), 'RW_BLADE', 1.5)
biped('DRAUGR', dict(head='draugr', skin=DRAUGRSKIN, style='mail', body=RUST, tabard=BLUE, skirt=5, skirtramp=BLUE, sleeve=RUST, sleevetex='mail',
                     fore=DRAUGRSKIN, foretex=None, hand=DRAUGRSKIN, legs=BLUE, shinramp=DRAUGRSKIN, boot=LEATHER, belt=LEATHER),
      haft_part(15, lambda c, cx, ty: axe_blade(c, cx, ty, 1.0, RUST)), 'RW_BLADE', 1.5, shield_part('round', 6.5, lambda nx, ny: BLUE if nx < 0 else CREAM))
biped('TROLL', dict(s=1.2, hs=7, torso=21, tw=18, uarm=11, farm=11, thigh=11, shin=12, ar=3.8, lr=4.2, head='troll', skin=TROLLSKIN, style='bare',
                    body=TROLLSKIN, loin=True, belt=LEATHER, skirt=0, sleeve=TROLLSKIN, sleevetex=None, foretex=None, cuff=LEATHER, legs=TROLLSKIN,
                    legtex=None, shinramp=TROLLSKIN, shintex=None, boot=TROLLSKIN, barefoot=True),
      haft_part(16, club_head, r=1.4, butt=3), 'RW_BLADE', 1.6)
biped('RISEN', dict(s=0.97, head='risen', skin=ROT, hood=BROWNCLOTH, eyes=REDGLOW, style='rags', body=BROWNCLOTH, skirt=5, sleeve=BROWNCLOTH,
                    fore=ROT, foretex=None, hand=ROT, legs=TROUSER, shinramp=ROT, shintex=None, boot=WRAP, belt=WRAP),
      haft_part(24, fork_head, butt=6), 'RW_POLE', 2.5)
# the Black Knight: twice the size, black plate, a tattered cape, antlers, a greatsword
biped('BLACKKNIGHT', dict(s=2.2, head='antler', style='plate', body=BLACKPLATE, gilt=GOLD, cape=DRED, capeL=int(15 * 2.2 + 18), skirt=10, skirtramp=BLACKPLATE,
                          sleeve=BLACKPLATE, sleevetex=None, fore=BLACKPLATE, foretex=None, hand=BLACKPLATE, pauldron=BLACKPLATE, legs=BLACKPLATE,
                          legtex=None, knee=BLACKPLATE, shinramp=BLACKPLATE, shintex=None, boot=BLACKPLATE, belt=LEATHER),
      blade_part(44, 2.6, STEEL, DARKSTEEL, 7), 'RW_BLADE', 1.3)
# the Lich King: a crowned skull, rich robes over bone, a staff topped with a skull
biped('LICH', dict(s=1.9, head='crown', eyes=GREENGLOW, style='robe', body=PURPLE, skirt=int(22 * 1.9), trim=GOLD, sleeve=PURPLE, fore=PURPLE,
                   hand=BONE, legs=BLACKCLOTH, boot=BLACKCLOTH, belt=GOLD),
      haft_part(46, orb_head(GREENGLOW, WOOD, True), butt=12, r=1.1), 'RW_STAFF', 2.7)

# four-footed beasts: wolf and kelpie; and the bat
RIGS['WOLF'] = dict(kind='RK_QUAD', grip=0, sgrip=0, shoulder=0, wclass='RW_NONE', sw=0, hw=0, parts={
    'TORSO': quad_body(18, 5.5, WOLFFUR, mane=WOLFFUR, belly=CREAM), 'HEAD': wolf_head(12, WOLFFUR),
    'UARM': upper_part(6, 1.8, WOLFFUR, 'fur'), 'FARM': shin_part(7, 1.5, WOLFFUR, WOLFFUR, 'fur', 2.0, None, True),
    'THIGH': upper_part(6, 2.2, WOLFFUR, 'fur'), 'SHIN': shin_part(7, 1.5, WOLFFUR, WOLFFUR, 'fur', 2.0, None, True),
    'TAIL': tail_part(11, WOLFFUR)})
RIGS['KELPIE'] = dict(kind='RK_QUAD', grip=0, sgrip=0, shoulder=0, wclass='RW_NONE', sw=0, hw=0, parts={
    'TORSO': quad_body(24, 7, HORSE, 'cloth', mane=WEED), 'HEAD': horse_head(18, HORSE, WEED),
    'UARM': upper_part(8, 2.4, HORSE), 'FARM': shin_part(9, 1.8, HORSE, DARKSTEEL, None, 1.5),
    'THIGH': upper_part(8, 2.8, HORSE), 'SHIN': shin_part(9, 1.8, HORSE, DARKSTEEL, None, 1.5),
    'TAIL': tail_part(15, WEED, 'fur', 2.2)})
RIGS['BAT'] = dict(kind='RK_BAT', grip=0, sgrip=0, shoulder=0.2, wclass='RW_NONE', sw=0, hw=0, parts={
    'TORSO': bat_body(9, BATSKIN), 'UARM': wing_part(14, MEMBRANE, BATSKIN[0])})


# ---------------------------------------------------------------- the treasure chest
# A Norse chest: weathered planks framing deep blue panels, each carved with interlaced knotwork in pale
# relief, iron-nailed posts, a brass lock, and an arched lid. `opened` swings the lid back on a hoard.
OLDWOOD = [(44, 36, 30), (70, 58, 46), (100, 84, 66), (128, 110, 88), (150, 132, 108)]
POSTWOOD = [(34, 26, 20), (56, 42, 30), (82, 64, 46), (108, 88, 64)]
PANEL = [(14, 22, 42), (22, 34, 62), (30, 46, 82)]
RELIEF = [(54, 68, 96), (104, 120, 148), (156, 170, 194), (204, 214, 228)]
BRASS = [(96, 70, 26), (156, 118, 44), (206, 168, 80), (236, 210, 140)]
IRON = (40, 40, 46)


def knot_panel(c, x0, y0, x1, y1, kind):
    """A sunken blue panel carved with knotwork in pale relief: a two-strand braid, or a ring with a
    cross woven through it. Strands pass over and under each other where they cross."""
    cx, cy = (x0 + x1 + 1) / 2, (y0 + y1 + 1) / 2
    w, h = x1 - x0 + 1, y1 - y0 + 1
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            c.put(x, y, PANEL[0] if (x == x0 or y == y0) else PANEL[1]) # sunk: shadowed along the top and left
    marks = {}
    def mark(x, y, d, layer):
        if x0 < x < x1 and y0 < y < y1 and d < 0.62:
            old = marks.get((x, y))
            if old is None or layer > old[1] or (layer == old[1] and d < old[0]): marks[(x, y)] = (d, layer)
    ix0, ix1, iy0, iy1 = x0 + 1.5, x1 - 0.5, y0 + 1.5, y1 - 0.5
    if kind == 'braid':
        cyc = 1.0 if w <= 10 else 1.5
        A = (iy1 - iy0) / 2 - 0.6
        for i in range(400):
            t = i / 399
            x = ix0 + (ix1 - ix0) * t
            for ph, name in ((0, 0), (math.pi, 1)):
                y = cy + A * math.sin(t * cyc * 2 * math.pi + ph)
                seg = int(t * cyc * 2) # which half-wave: crossings alternate who's on top
                layer = 1 if (seg + name) % 2 == 0 else 0
                for yy in range(int(y) - 1, int(y) + 2):
                    for xx in range(int(x) - 1, int(x) + 2):
                        mark(xx, yy, math.hypot(xx + 0.5 - x, yy + 0.5 - y), layer)
    else: # a ring, with a cross passing under it and then over
        r = min(w, h) / 2 - 1.8
        for i in range(400):
            a = i / 400 * 2 * math.pi
            x, y = cx + math.cos(a) * r * w / h, cy + math.sin(a) * r
            for yy in range(int(y) - 1, int(y) + 2):
                for xx in range(int(x) - 1, int(x) + 2):
                    mark(xx, yy, math.hypot(xx + 0.5 - x, yy + 0.5 - y), 1 if (i // 50) % 2 else 0)
        for i in range(300):
            t = i / 299
            for (ax, ay, bx, by) in ((ix0, iy0, ix1, iy1), (ix0, iy1, ix1, iy0)):
                x, y = ax + (bx - ax) * t, ay + (by - ay) * t
                for yy in range(int(y) - 1, int(y) + 2):
                    for xx in range(int(x) - 1, int(x) + 2):
                        mark(xx, yy, math.hypot(xx + 0.5 - x, yy + 0.5 - y), 1 if (int(t * 4) % 2) else 0)
    for (x, y), (d, layer) in marks.items():
        lit = 3 if d < 0.3 and layer else 2 if d < 0.45 else 1
        c.put(x, y, RELIEF[lit])
        if c.get(x + 1, y + 1) in PANEL: c.put(x + 1, y + 1, PANEL[0]) # each strand casts a little shadow


def chest(opened):
    W, H = 40, 30
    c = Canvas(W, H)
    lid_top = lambda x: 1 + int(round(3.0 * ((x - 19.5) / 18.5) ** 2)) # a shallow arch
    for y in range(13, 28): # the body: weathered planks
        for x in range(2, 38):
            c.put(x, y, c.tone(OLDWOOD, 0, -0.1, 1, x, y, 'wood') if hsh(x // 3, y, 4) > 0.05 else OLDWOOD[0])
    knot_panel(c, 5, 15, 17, 25, 'braid')
    knot_panel(c, 22, 15, 34, 25, 'rings')
    for y in range(27, 30): # the plinth
        for x in range(0, 40):
            c.put(x, y, c.tone(POSTWOOD, 0, -0.6 if y == 27 else 0.4, 0.8, x, y, 'wood'))
    for px0, px1 in ((1, 4), (18, 21), (35, 38)): # posts at the corners and the middle, nailed with iron
        for y in range(11 if opened else 13, 28):
            for x in range(px0, px1 + 1):
                nx = (x + 0.5 - (px0 + px1 + 1) / 2) / ((px1 - px0 + 1) / 2)
                c.put(x, y, c.tone(POSTWOOD, nx * 0.9, -0.1, 0.6, x, y, 'wood'))
        for y in (15, 20, 25):
            c.put((px0 + px1) // 2, y, IRON)
    if not opened:
        for x in range(1, 39): # the lid: an arch of planks
            for y in range(lid_top(x), 13):
                c.put(x, y, c.tone(OLDWOOD, 0, -0.6 + (y - 1) / 11, 0.9, x, y, 'wood'))
        for x in range(1, 39): # its lip
            c.put(x, 11, POSTWOOD[2]); c.put(x, 12, POSTWOOD[0])
        for px0, px1 in ((3, 11), (15, 24), (28, 36)): # three carved panels
            knot_panel(c, px0, max(lid_top(px0), lid_top(px1)) + 1, px1, 9, 'rings' if px0 == 15 else 'braid')
        for bx0 in (12, 25): # bands between them
            for y in range(lid_top(bx0), 11):
                for k in range(3):
                    c.put(bx0 + k, y, POSTWOOD[2 if k == 0 else 1])
        for y in range(8, 15): # the brass lock, keyhole and all
            for x in range(17, 23):
                nx, ny = (x + 0.5 - 20) / 3, (y + 0.5 - 11.5) / 3.5
                c.put(x, y, c.tone(BRASS, nx, ny, 0.7, x, y, None))
        c.put(19, 11, (20, 14, 10)); c.put(20, 11, (20, 14, 10)); c.put(19, 12, (20, 14, 10)); c.put(20, 12, (20, 14, 10)); c.put(19, 13, (20, 14, 10))
    else: # the lid swung back: its dark inside face, and the hoard within
        for x in range(3, 37):
            for y in range(1, 10):
                c.put(x, y, c.tone(POSTWOOD, 0, 0.3, 0.8, x, y, 'wood'))
        for x in range(2, 38):
            c.put(x, 10, POSTWOOD[0]); c.put(x, 11, (16, 12, 10)); c.put(x, 12, (16, 12, 10))
        for x in range(5, 35):
            hgt = 2 + int(1.6 * math.sin((x - 5) / 30 * math.pi) + hsh(x, 1, 7) * 1.5)
            for y in range(13 - hgt, 13):
                c.put(x, y, BRASS[1 + int(hsh(x, y, 8) * 3)] if (x + y) % 3 else BRASS[3])
        for x in range(17, 23):
            c.put(x, 13, BRASS[2]); c.put(x, 14, BRASS[1])
    c.outline()
    return c


SPRITES = {}
SPRITES['CHEST'] = chest(False)
SPRITES['CHEST_OPEN'] = chest(True)
SLOTS = ['HEAD', 'TORSO', 'UARM', 'FARM', 'THIGH', 'SHIN', 'WEAPON', 'SHIELD', 'WING', 'TAIL']
for _name, _r in RIGS.items():
    for _slot, _part in _r['parts'].items():
        SPRITES[f'RIG_{_name}_{_slot}'] = _part.c

ALPHABET = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz!#$%&()*+,-/:;<=>?@[]^_`{|}~"


def preview(path, scale=6):
    """Every sprite, and each rig's parts laid out in a row."""
    from PIL import Image
    pad = 4
    rows = [[v for k, v in SPRITES.items() if not k.startswith('RIG_')]] + [[p.c for p in r['parts'].values()] for r in RIGS.values()]
    W = max(sum(c.w + pad for c in row) for row in rows) + pad
    H = sum(max(c.h for c in row) + pad for row in rows) + pad
    im = Image.new('RGB', (W, H), (24, 22, 30))
    oy = pad
    for row in rows:
        rh = max(c.h for c in row)
        ox = pad
        for c in row:
            for y in range(c.h):
                for x in range(c.w):
                    if c.px[y][x]: im.putpixel((ox + x, oy + rh - c.h + y), c.px[y][x])
            ox += c.w + pad
        oy += rh + pad
    im.resize((W * scale, H * scale), Image.NEAREST).save(path)


def emit():
    out = ["// Generated by tools/art_hd.py - edit there, then: python tools/art_hd.py emit > sprites_hd.h",
           "#pragma once", "#include <raylib.h>", "",
           "// Detailed character art at half a world cell per pixel. Each sprite carries its own palette; a row",
           "// character is an index into HD_ALPHABET, '.' is transparent.",
           "struct HDSprite { int w, h; const Color* pal; const char* const* rows; };",
           f'static const char HD_ALPHABET[] = "{ALPHABET}";', ""]
    out += ["// A creature's parts, posed on a skeleton by rig.cpp (and flung about as a ragdoll when it dies). Each part",
            "// hangs from its pivot (px, py) with its bone running to (ex, ey), in sprite pixels.",
            "enum RigKind { RK_BIPED, RK_QUAD, RK_BAT, RK_GHOST };",
            "enum RigSlot { RS_HEAD, RS_TORSO, RS_UARM, RS_FARM, RS_THIGH, RS_SHIN, RS_WEAPON, RS_SHIELD, RS_WING, RS_TAIL, RS_COUNT };",
            "enum RigWeapon { RW_NONE, RW_BLADE, RW_POLE, RW_BOW, RW_STAFF, RW_THROW };",
            "struct RigPart { const HDSprite* spr; float px, py, ex, ey; };",
            "// grip: the weapon's angle off the forearm; sgrip: the shield's; shoulder: how far down the torso the arms hang from;",
            "// sw, hw: the shoulders' and hips' half-spread across the body, in sprite pixels",
            "struct RigSpec { int kind; RigPart part[RS_COUNT]; float grip, sgrip, shoulder, sw, hw; int weapon; };", ""]
    for name, s in SPRITES.items():
        pal = []
        for row in s.px:
            for p in row:
                if p and p not in pal: pal.append(p)
        assert len(pal) <= len(ALPHABET), name
        out.append(f"static const Color HDP_{name}[] = {{" + ", ".join(f"{{{p[0]}, {p[1]}, {p[2]}, 255}}" for p in pal) + "};")
        out.append(f"static const char* const HDR_{name}[] = {{")
        for row in s.px:
            out.append('    "' + "".join('.' if p is None else ALPHABET[pal.index(p)] for p in row) + '",')
        out.append("};")
        out.append(f"static const HDSprite HD_{name} = {{{s.w}, {s.h}, HDP_{name}, HDR_{name}}};")
        out.append("")
    for name, r in RIGS.items():
        cells = []
        for slot in SLOTS:
            pt = r['parts'].get(slot)
            cells.append(f"{{&HD_RIG_{name}_{slot}, {pt.piv[0]:.2f}f, {pt.piv[1]:.2f}f, {pt.end[0]:.2f}f, {pt.end[1]:.2f}f}}" if pt else "{nullptr, 0, 0, 0, 0}")
        out.append(f"static const RigSpec RIG_{name} = {{{r['kind']}, {{{', '.join(cells)}}}, {r['grip']:.3f}f, {r['sgrip']:.3f}f, {r['shoulder']:.3f}f, {r['sw']:.2f}f, {r['hw']:.2f}f, {r['wclass']}}};")
    return "\n".join(out)


if __name__ == '__main__':
    if sys.argv[1] == 'preview': preview(sys.argv[2])
    elif sys.argv[1] == 'emit': print(emit())

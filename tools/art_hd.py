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


# ---------------------------------------------------------------- Norse warriors (the castle guard)

def warrior(kind, frame):
    W, H = 34, 54
    c = Canvas(W, H)
    hair = GINGER if kind != 'sword' else BLOND
    tunic = {'axe': BLUE, 'spear': STEEL, 'sword': BROWNCLOTH}[kind]
    cx = 15
    # weapon behind the body: the spear stands tall
    if kind == 'spear':
        c.capsule(6, 6, 6, 53, 0.7, WOOD, 'wood')
        c.poly([(6, 0), (7.6, 4), (6.8, 7.5), (5.2, 7.5), (4.4, 4)], STEEL, None, 0.6)
    # cloak behind
    if kind == 'spear':
        c.poly([(8, 17), (23, 17), (26, 40), (21, 43), (12, 42), (5, 40)], RED, 'cloth')
    # legs
    if frame == 0:
        legs = [((13, 34), (12, 41), (11.5, 46.5)), ((18, 34), (19, 41), (19.5, 46.5))]
    else:
        legs = [((13, 34), (10, 40.5), (7.5, 46)), ((18, 34), (21.5, 40.5), (23, 46))]
    for i, (hp, kn, an) in enumerate(legs):
        leg(c, hp, kn, an, TROUSER, WRAP)
        boot(c, an[0], an[1], 1 if i == 1 or frame == 0 else -1)
    # body
    c.poly([(8.5, 17), (22.5, 17), (22.5, 30), (24, 38), (7, 38), (8.5, 30)], tunic, 'mail' if kind == 'spear' else 'cloth')
    if kind == 'spear':  # a padded skirt under the mail
        c.poly([(8, 33), (23, 33), (24, 38.5), (7, 38.5)], BLUE, 'cloth')
    if kind == 'axe':
        c.ellipse(cx + 0.5, 18, 9.5, 3.6, FUR, 'fur')  # fur mantle
    if kind == 'spear':
        c.ellipse(cx + 0.5, 18, 8.5, 3, RED, 'cloth')  # the cloak over the shoulders
        c.ellipse(cx + 3, 19, 1.3, 1.3, GOLD)  # its brooch
    if kind == 'sword':
        c.poly([(8.5, 17), (13, 17), (12, 31), (8.5, 31)], FUR, 'fur', 0.5)  # a fur-lined jerkin edge
    if kind != 'spear':
        c.line(9, 19, 21, 31, LEATHER[1])  # baldric
        c.line(10, 19, 22, 31, LEATHER[2])
    c.line(8, 31, 23, 31, LEATHER[1])  # belt
    c.line(8, 32, 23, 32, LEATHER[2])
    c.put(18, 31, GOLD[2]); c.put(18, 32, GOLD[1])
    # weapon arm (far side)
    c.capsule(10, 19, 8, 26, 2.3, tunic if kind != 'spear' else STEEL, 'mail' if kind == 'spear' else 'cloth')
    c.capsule(8, 26, 8 if kind != 'spear' else 7, 31, 2.0, tunic if kind != 'spear' else STEEL, 'mail' if kind == 'spear' else 'cloth')
    if kind == 'axe':
        c.capsule(8.5, 30, 5, 41, 0.75, WOOD, 'wood')
        axe_head(c, 5, 41, 1.0)
    if kind == 'sword':
        c.capsule(8, 33, 2.5, 45, 0.9, STEEL)
        c.line(8, 33, 2, 45, STEEL[4])
        c.line(6, 31, 10, 34, GOLD[2])  # crossguard
    c.ellipse(8, 31.5, 1.8, 1.8, SKIN)  # fist
    # head: hair, face, beard, helm
    c.capsule(11.5, 9, 10.5, 15 if kind != 'sword' else 19, 2.2, hair, 'fur')
    c.ellipse(cx + 1.5, 11, 5, 5.5, SKIN)
    c.poly([(12, 12.5), (21, 12.5), (21, 16), (18, 20), (14, 20), (12, 16)], hair, 'fur', 0.6)
    c.line(17, 14, 21, 14, hair[1])  # moustache
    c.put(19, 11, (30, 22, 20))  # eye
    nasal_helm(c, cx + 1, 7.5, 5.8)
    # shield (near side)
    paints = {
        'axe': lambda nx, ny: RED,
        'spear': lambda nx, ny: RED if (nx > 0) == (ny > 0) else CREAM,
        'sword': lambda nx, ny: BLUE if (math.atan2(ny, nx) + math.pi) % (math.pi / 2) < math.pi / 4 else CREAM,
    }
    round_shield(c, 25, 26, 8, 9.5, paints[kind])
    c.outline()
    return c


# ---------------------------------------------------------------- castle knights

def knight(kind, frame):
    W, H = 38, 58
    c = Canvas(W, H)
    cx = 17
    plate = STEEL if kind == 'sword' else BLUESTEEL
    if kind == 'halberd':  # the halberd, slanting up behind the near shoulder
        c.capsule(5, 56, 31, 4, 0.8, WOOD, 'wood')
        c.poly([(28, 6), (33, 3), (35, 9), (31, 12)], STEEL, None, 0.5)  # blade
        c.poly([(30, 2), (32, -2), (33, 4)], STEEL)  # spike
        c.line(27, 9, 25, 7, STEEL[2])  # back hook
    if frame == 0:
        legs = [((14, 36), (13.5, 44), (13, 51)), ((20, 36), (21, 44), (21.5, 51))]
    else:
        legs = [((14, 36), (11, 43.5), (8.5, 50.5)), ((20, 36), (23.5, 43.5), (25, 50.5))]
    for i, (hp, kn, an) in enumerate(legs):
        c.capsule(*hp, *kn, 2.8, STEEL, 'mail')
        c.capsule(*kn, *an, 2.4, plate)
        c.ellipse(kn[0], kn[1], 2.4, 2.0, plate)  # poleyn
        boot(c, an[0], an[1], 1 if i == 1 or frame == 0 else -1, plate, None)
    # mail body, plate breast, tabard
    c.poly([(9, 18), (25, 18), (26, 37), (8, 37)], STEEL, 'mail')
    if kind == 'sword':
        c.poly([(11, 19), (23, 19), (24, 44), (19, 46), (15, 46), (10, 44)], BROWNCLOTH, 'cloth', 0.6)
        # a pale keep on the tabard
        for (x, y) in [(15, 25), (17, 25), (19, 25), (15, 26), (16, 26), (17, 26), (18, 26), (19, 26)] + [(xx, yy) for xx in range(15, 20) for yy in range(27, 31)]:
            c.put(x, y, CREAM[2])
        c.put(17, 29, (40, 34, 30)); c.put(17, 30, (40, 34, 30))
    else:
        c.poly([(11, 19), (23, 19), (24, 46), (19, 48), (15, 48), (10, 46)], BLUE, 'cloth', 0.6)
        for (x, y) in [(16, 24), (17, 24), (16, 25), (17, 25), (18, 25), (15, 26), (16, 26), (17, 26), (18, 26), (16, 27), (18, 27), (15, 28), (19, 28), (17, 23), (18, 22)]:
            c.put(x, y, CREAM[3])  # a rampant lion, roughly
        for y in range(19, 47, 1):
            c.put(11 if y < 44 else 10, y, GOLD[1])  # gilt edging
    c.line(9, 36, 25, 36, LEATHER[1]); c.line(9, 37, 25, 37, LEATHER[2])
    c.put(17, 36, GOLD[2])
    # pauldrons and arms
    c.capsule(10, 20, 9, 28, 2.6, plate)
    c.ellipse(10, 20, 3.6, 3.0, plate)
    if kind == 'sword':
        # both hands on the hilt of a greatsword planted before him
        c.capsule(9, 28, 22, 30, 2.1, plate)
        c.ellipse(24, 20, 3.6, 3.0, plate)
        c.capsule(24, 21, 24, 29, 2.3, plate)
        c.capsule(27, 33, 27, 56, 1.1, STEEL)
        c.line(27, 34, 27, 55, STEEL[4])
        c.line(23, 32, 31, 32, GOLD[2]); c.line(23, 33, 31, 33, GOLD[1])
        c.capsule(27, 26, 27, 31, 0.8, LEATHER)
        c.ellipse(27, 25, 1.4, 1.4, GOLD)
        c.ellipse(25.5, 29.5, 2.3, 2.0, plate)
    else:
        c.capsule(9, 28, 12, 34, 2.1, plate)
        c.ellipse(24, 20, 3.6, 3.0, plate)
        c.capsule(24, 21, 26, 27, 2.3, plate)
        c.ellipse(26, 26, 2.0, 2.0, plate)  # gauntlet on the haft
        c.ellipse(12, 34, 2.0, 2.0, plate)
    # head
    if kind == 'sword':  # a great helm: flat top, eye slit, breaths
        c.poly([(11.5, 4), (22.5, 4), (23, 15), (21, 17), (13, 17), (11, 15)], STEEL, None, 0.9)
        c.line(14, 9, 22, 9, (20, 20, 24)); c.line(14, 10, 22, 10, (40, 40, 46))
        c.line(18, 5, 18, 16, STEEL[3])
        for (x, y) in [(20, 13), (21, 14), (20, 15)]: c.put(x, y, (24, 24, 28))
    else:  # hood over a sallet: just the shadowed visor shows
        c.ellipse(17, 10, 7.5, 8.5, GREYCLOTH, 'cloth')
        c.poly([(10, 14), (24, 14), (26, 22), (8, 22)], GREYCLOTH, 'cloth')
        c.ellipse(19, 11, 4.2, 4.6, BLUESTEEL)
        c.line(17, 10, 22, 10, (16, 18, 26))
        c.line(19, 6, 19, 15, BLUESTEEL[3])
    c.outline()
    return c


# ---------------------------------------------------------------- the Black Knight: a dark king

def dark_king():
    W, H = 58, 74
    c = Canvas(W, H)
    cx = 27
    # a tattered cape
    cape = [(14, 20), (40, 20), (46, 64), (43, 70), (40, 64), (37, 71), (33, 65), (29, 72), (25, 65), (21, 71), (17, 64), (13, 70), (9, 63)]
    c.poly(cape, DRED, 'cloth', 0.6)
    # legs
    for i, (hp, kn, an) in enumerate([((22, 46), (21, 56), (20, 66)), ((31, 46), (33, 56), (34, 66))]):
        c.capsule(*hp, *kn, 3.6, DARKSTEEL)
        c.capsule(*kn, *an, 3.1, DARKSTEEL)
        c.ellipse(kn[0], kn[1], 3.3, 2.8, DARKSTEEL)
        c.put(int(kn[0]), int(kn[1] - 1), STEEL[2])
        boot(c, an[0], an[1], 1, DARKSTEEL, None)
    # body
    c.poly([(16, 22), (38, 22), (37, 40), (39, 50), (15, 50), (17, 40)], DARKSTEEL, None, 0.9)
    c.poly([(18, 40), (36, 40), (38, 50), (16, 50)], DARKSTEEL, 'mail')
    for y in (27, 32, 37): c.line(19, y, 35, y, DARKSTEEL[0])  # plate lames
    c.line(16, 41, 38, 41, LEATHER[0]); c.line(16, 42, 38, 42, LEATHER[1])
    c.ellipse(27, 41.5, 1.6, 1.4, BONE)  # a skull buckle
    # the axe, held low across the body
    c.capsule(10, 34, 42, 58, 1.0, WOOD, 'wood')
    c.poly([(38, 52), (45, 50), (51, 56), (49, 64), (42, 62)], STEEL, None, 0.6)
    c.line(45, 50, 51, 56, STEEL[4]); c.line(51, 56, 49, 64, STEEL[4])
    c.poly([(6, 31), (10, 32), (9, 36)], STEEL)  # the butt spike
    # far arm and its huge spiked pauldron, a skull riding it
    c.capsule(16, 26, 13, 36, 3.4, DARKSTEEL)
    c.ellipse(13, 36, 2.8, 2.6, DARKSTEEL)
    c.ellipse(15, 22, 6, 5, DARKSTEEL)
    for k in range(3): c.poly([(11 + k * 3, 19), (12.5 + k * 3, 12 - k), (14 + k * 3, 19)], DARKSTEEL)
    c.ellipse(14, 20, 3.2, 3.0, BONE)
    c.put(13, 20, (20, 16, 16)); c.put(15, 20, (20, 16, 16)); c.put(14, 22, (40, 34, 30))
    # helm: a skull face under a spiked crown
    c.ellipse(28, 13, 6.5, 7.5, DARKSTEEL)
    c.ellipse(29.5, 15, 4.6, 5.2, BONE)
    c.put(28, 14, EMBER[1]); c.put(31, 14, EMBER[1]); c.put(28, 15, EMBER[0]); c.put(31, 15, EMBER[0])
    c.line(28, 19, 32, 19, (30, 26, 22)); c.put(29, 20, BONE[2]); c.put(31, 20, BONE[2])
    for k in range(5): c.poly([(22 + k * 3, 8), (23.5 + k * 3, 1 + (k % 2) * 2), (25 + k * 3, 8)], DARKSTEEL)
    c.line(22, 8, 35, 8, GOLD[1])
    # near pauldron
    c.ellipse(39, 23, 6, 5, DARKSTEEL)
    for k in range(2): c.poly([(38 + k * 3, 19), (40 + k * 3, 13 + k), (41.5 + k * 3, 19)], DARKSTEEL)
    # the kite shield, black with a red dragon
    c.poly([(36, 24), (52, 24), (52, 38), (44, 52), (36, 38)], DARKSTEEL, None, 0.5)
    for y in range(25, 50):
        for x in range(37, 52):
            if c.get(x, y) and (x in (37, 51) or y == 25): c.put(x, y, STEEL[1])
    dragon = ["....rr....", "...r..r...", "..rr.rrr..", ".rrrrr.r..", "rr.rrr....", "...rrrr...", "..r.rr.r..", ".....r....", "....r.....", "...rr....."]
    for j, row in enumerate(dragon):
        for i, ch in enumerate(row):
            if ch == 'r': c.put(39 + i, 28 + j, RED[2] if (i + j) % 3 else RED[1])
    c.outline()
    return c



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
for k in ('axe', 'spear', 'sword'):
    SPRITES[f'WARRIOR_{k.upper()}_A'] = warrior(k, 0)
    SPRITES[f'WARRIOR_{k.upper()}_B'] = warrior(k, 1)
for k in ('sword', 'halberd'):
    SPRITES[f'KNIGHT_{k.upper()}_A'] = knight(k, 0)
    SPRITES[f'KNIGHT_{k.upper()}_B'] = knight(k, 1)
SPRITES['DARK_KING'] = dark_king()
SPRITES['CHEST'] = chest(False)
SPRITES['CHEST_OPEN'] = chest(True)

ALPHABET = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz!#$%&()*+,-/:;<=>?@[]^_`{|}~"


def preview(path, scale=6):
    from PIL import Image
    pad = 4
    W = sum(s.w + pad for s in SPRITES.values()) + pad
    H = max(s.h for s in SPRITES.values()) + pad * 2
    im = Image.new('RGB', (W, H), (24, 22, 30))
    ox = pad
    for s in SPRITES.values():
        oy = H - pad - s.h
        for y in range(s.h):
            for x in range(s.w):
                if s.px[y][x]: im.putpixel((ox + x, oy + y), s.px[y][x])
        ox += s.w + pad
    im.resize((W * scale, H * scale), Image.NEAREST).save(path)


def emit():
    out = ["// Generated by tools/art_hd.py - edit there, then: python tools/art_hd.py emit > sprites_hd.h",
           "#pragma once", "#include <raylib.h>", "",
           "// Detailed character art at half a world cell per pixel. Each sprite carries its own palette; a row",
           "// character is an index into HD_ALPHABET, '.' is transparent.",
           "struct HDSprite { int w, h; const Color* pal; const char* const* rows; };",
           f'static const char HD_ALPHABET[] = "{ALPHABET}";', ""]
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
    return "\n".join(out)


if __name__ == '__main__':
    if sys.argv[1] == 'preview': preview(sys.argv[2])
    elif sys.argv[1] == 'emit': print(emit())

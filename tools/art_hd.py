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

    def outline(self, col=OUTLINE):
        add = []
        for y in range(self.h):
            for x in range(self.w):
                if self.px[y][x] is None and any(self.get(x + a, y + b) not in (None, col) for a, b in ((1, 0), (-1, 0), (0, 1), (0, -1))):
                    add.append((x, y))
        for x, y in add: self.px[y][x] = col


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


# ---------------------------------------------------------------- the dart trap
# A wyrm's head carved in weathered stone, jutting from a mortared mounting slab set into the wall, with an
# iron bore in its jaws and a faint ember in its eye. Faces right; the slab's left 6 px sit inside the wall.
# `broken`: smashed off at the neck, leaving the slab and a jagged stump.
TRAPSTONE = [(40, 42, 40), (66, 70, 64), (98, 102, 92), (134, 138, 124), (170, 172, 156)]
SLAB = [(36, 34, 34), (58, 56, 54), (84, 80, 76), (112, 106, 98)]
BORE = [(14, 12, 12), (34, 32, 34), (70, 68, 74), (112, 110, 118)]


def dart_trap(broken):
    W, H = 26, 24
    c = Canvas(W, H)
    for y in range(1, 23): # the slab, in two mortared courses, bevelled at the face
        for x in range(0, 9):
            nx = 0.9 if x == 8 else (-0.6 if x == 0 else 0)
            ny = -0.8 if y == 1 else (0.8 if y == 22 else 0)
            c.put(x, y, c.tone(SLAB, nx, ny, 0.6, x, y, 'flat') if (y % 11 != 0 and hsh(x, y, 9) > 0.04) else SLAB[0])
    for y in (5, 18): c.put(4, y, BORE[1]); c.put(5, y, BORE[2]) # iron pins
    if broken:
        stump = [(8, 7), (12, 6), (13, 8), (11, 10), (14, 12), (12, 14), (13, 16), (8, 17)]
        c.poly(stump, TRAPSTONE, bulge=0.6)
        for y in range(7, 17): # raw, pale stone where it snapped
            x = max(xx for xx in range(8, 15) if c.get(xx, y) is not None)
            c.put(x, y, TRAPSTONE[4] if hsh(x, y, 11) > 0.4 else TRAPSTONE[3])
        c.put(10, 11, BORE[0]); c.put(10, 12, BORE[0]); c.put(11, 12, BORE[1]) # the bore's broken end
        c.outline(SLAB[0])
        return c
    c.ellipse(13, 10, 6.5, 6, TRAPSTONE) # the skull
    c.capsule(14, 13, 22, 13.5, 3.2, TRAPSTONE) # the snout
    c.capsule(10, 4.5, 4, 1.5, 1.6, TRAPSTONE) # a horn swept back
    c.capsule(15, 17.5, 21, 17.5, 1.8, TRAPSTONE) # the lower jaw, hanging open
    for x in range(14, 24): # the gape: dark, with the iron bore running out of the throat
        for y in range(15, 17):
            c.put(x, y, BORE[0])
    for x in range(15, 25):
        c.put(x, 15, BORE[2] if x < 24 else BORE[3]); c.put(x, 16, BORE[1])
    c.put(24, 16, BORE[0])
    for x in (16, 18, 20): c.put(x, 14, TRAPSTONE[4]); c.put(x + 1, 17, TRAPSTONE[4]) # teeth
    c.put(23, 12, TRAPSTONE[0]) # nostril
    for y in range(8, 11): # a deep-set eye, with a coal still glowing in it
        for x in range(14, 18):
            c.put(x, y, TRAPSTONE[0])
    c.put(15, 9, EMBER[1]); c.put(16, 9, EMBER[2]); c.put(16, 10, EMBER[0])
    c.put(14, 7, TRAPSTONE[4]); c.put(15, 7, TRAPSTONE[4]); c.put(16, 7, TRAPSTONE[3]) # the brow ridge
    for i in range(14): # a carved scroll on the cheek
        a = i / 13 * 1.6 * math.pi
        r = 1.0 + i * 0.16
        x, y = int(round(11 + math.cos(a) * r)), int(round(12 + math.sin(a) * r))
        c.put(x, y, TRAPSTONE[1]); c.put(x + 1, y + 1, TRAPSTONE[3])
    for (x, y) in ((9, 6), (12, 17), (18, 11), (20, 12)): # cracks and lichen
        c.put(x, y, TRAPSTONE[0])
    for (x, y) in ((11, 5), (12, 5), (19, 11)):
        c.put(x, y, (86, 104, 62))
    c.outline(SLAB[0])
    return c


SPRITES = {}
SPRITES['CHEST'] = chest(False)
SPRITES['CHEST_OPEN'] = chest(True)
SPRITES['DART_TRAP'] = dart_trap(False)
SPRITES['DART_TRAP_BROKEN'] = dart_trap(True)

ALPHABET = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz!#$%&()*+,-/:;<=>?@[]^_`{|}~"


def preview(path, scale=6):
    """Every sprite in a row."""
    from PIL import Image
    pad = 4
    rows = [list(SPRITES.values())]
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
           "// character is an index into HD_ALPHABET, '.' is transparent. Alpha 254 marks a colour recoloured to the armour's",
           "// metal, 253 one recoloured to its element's glow (the player's rigs); both draw opaque.",
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
            "struct RigSpec { int kind; RigPart part[RS_COUNT]; float grip, sgrip, shoulder, sw, hw; int weapon; int shieldFar = 0; }; // shieldFar: the shield on the far arm, the weapon in the near hand", ""]
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

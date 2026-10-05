# Every rigged creature, painted fresh in a Noita-like style: no outline; chunky, readable silhouettes; each
# shape a cel-shaded volume (a flat face, a lit rim towards the upper left, a shadowed rim away from it);
# shapes lying on top of others cast a one-tone shadow onto them; the silhouette's own edge pixels darken on
# the shadow side. Palettes are five-tone ramps whose shadows go cool and whose lights go warm.
#
# A part is built in design pixels relative to its pivot (the joint it hangs from) and baked to a canvas.
# Limbs hang down (+y) from the pivot to their bone end; heads rise from the neck (-y) to the crown; weapons
# run down from the grip to the tip; a bow's bone runs up. Part(s=2) paints the same design twice as big.
# Imported by art_hd.py, which emits RIGS into sprites_hd.h. Slots and RigSpec fields: see art_hd.emit.
import math

LIGHT = (-0.5, -0.72, 0.6)
_n = math.sqrt(sum(v * v for v in LIGHT)); LIGHT = tuple(v / _n for v in LIGHT)


def _mix(a, b, t): return tuple(int(round(a[i] + (b[i] - a[i]) * t)) for i in range(3))


def ramp(hexcol):
    """Five tones from a mid colour: deep cool shadow, shadow, base, warm light, highlight."""
    b = tuple(int(hexcol[i:i + 2], 16) for i in (1, 3, 5))
    return [_mix(tuple(v * 0.4 for v in b), (26, 20, 64), 0.24), _mix(tuple(v * 0.68 for v in b), (44, 40, 92), 0.12), b,
            _mix(b, (255, 238, 196), 0.2), _mix(b, (255, 246, 220), 0.42)]


def _clamp(v, a, b): return a if v < a else b if v > b else v


# ---------------------------------------------------------------- signed distances (negative inside)

def sd_circle(cx, cy, r): return lambda x, y: math.hypot(x - cx, y - cy) - r


def sd_ellipse(cx, cy, rx, ry, rot=0.0):
    c, s = math.cos(rot), math.sin(rot)
    def f(x, y):
        dx, dy = x - cx, y - cy
        u, v = dx * c + dy * s, -dx * s + dy * c
        return (math.hypot(u / rx, v / ry) - 1) * min(rx, ry)
    return f


def sd_cap(x0, y0, x1, y1, r0, r1):
    dx, dy = x1 - x0, y1 - y0
    ll = dx * dx + dy * dy or 1e-9
    def f(x, y):
        t = _clamp(((x - x0) * dx + (y - y0) * dy) / ll, 0, 1)
        return math.hypot(x - x0 - dx * t, y - y0 - dy * t) - (r0 + (r1 - r0) * t)
    return f


def sd_poly(pts):
    n = len(pts)
    def f(x, y):
        d = (x - pts[0][0]) ** 2 + (y - pts[0][1]) ** 2
        s = 1
        j = n - 1
        for i in range(n):
            ex, ey = pts[j][0] - pts[i][0], pts[j][1] - pts[i][1]
            wx, wy = x - pts[i][0], y - pts[i][1]
            t = _clamp((wx * ex + wy * ey) / (ex * ex + ey * ey or 1e-9), 0, 1)
            bx, by = wx - ex * t, wy - ey * t
            d = min(d, bx * bx + by * by)
            c1, c2, c3 = y >= pts[i][1], y < pts[j][1], ex * wy > ey * wx
            if (c1 and c2 and c3) or (not c1 and not c2 and not c3): s = -s
            j = i
        return s * math.sqrt(d)
    return f


# ---------------------------------------------------------------- a part

class Canvas:
    def __init__(self, w, h):
        self.w, self.h = w, h
        self.px = [[None] * w for _ in range(h)]


class Shape:
    def __init__(self, sdf, box, mat, bev, bias, tex, grp):
        self.sdf, self.box, self.mat, self.bev, self.bias, self.tex = sdf, box, mat, bev, bias, tex
        self.grp = grp if grp is not None else id(mat)


class Part:
    def __init__(self, end, s=1.0):
        self.shapes, self.marks, self.end_l, self.s, self._c = [], [], end, s, None

    def add(self, sdf, box, mat, bev=2.5, bias=0.0, tex=None, grp=None):
        self.shapes.append(Shape(sdf, box, mat, bev, bias, tex, grp))
        return self

    def circle(self, x, y, r, mat, **k): return self.add(sd_circle(x, y, r), (x - r, y - r, x + r, y + r), mat, k.pop('bev', r), **k)
    def ell(self, x, y, rx, ry, mat, rot=0.0, **k):
        m = max(rx, ry)
        return self.add(sd_ellipse(x, y, rx, ry, rot), (x - m, y - m, x + m, y + m), mat, k.pop('bev', min(rx, ry)), **k)
    def cap(self, x0, y0, x1, y1, r0, r1, mat, **k):
        m = max(r0, r1)
        return self.add(sd_cap(x0, y0, x1, y1, r0, r1), (min(x0, x1) - m, min(y0, y1) - m, max(x0, x1) + m, max(y0, y1) + m), mat, k.pop('bev', m), **k)
    def poly(self, pts, mat, **k):
        return self.add(sd_poly(pts), (min(p[0] for p in pts), min(p[1] for p in pts), max(p[0] for p in pts), max(p[1] for p in pts)), mat, **k)

    # pixel marks on top of the shading: c is an RGB tuple or (ramp, tone)
    def dot(self, x, y, c): self.marks.append((math.floor(x), math.floor(y), c)); return self
    def line(self, x0, y0, x1, y1, c):
        n = int(max(abs(x1 - x0), abs(y1 - y0)) * 3) + 1
        seen = set()
        for i in range(n + 1):
            t = i / n
            q = (math.floor(x0 + (x1 - x0) * t), math.floor(y0 + (y1 - y0) * t))
            if q not in seen: seen.add(q); self.marks.append((q[0], q[1], c))
        return self

    @property
    def c(self):
        if self._c is None: self._bake()
        return self._c

    def _bake(self):
        s = self.s
        xs = [b[0] for b in (sh.box for sh in self.shapes)] + [m[0] for m in self.marks] + [0]
        ys = [b[1] for b in (sh.box for sh in self.shapes)] + [m[1] for m in self.marks] + [0]
        xe = [b[2] for b in (sh.box for sh in self.shapes)] + [m[0] + 1 for m in self.marks]
        ye = [b[3] for b in (sh.box for sh in self.shapes)] + [m[1] + 1 for m in self.marks]
        ox, oy = math.floor(min(xs) * s) - 2, math.floor(min(ys) * s) - 2
        W, H = math.ceil(max(xe) * s) - ox + 3, math.ceil(max(ye) * s) - oy + 3
        own = [[None] * W for _ in range(H)]
        tone = [[0] * W for _ in range(H)]
        for j in range(H):
            for i in range(W):
                x, y = (i + ox + 0.5) / s, (j + oy + 0.5) / s
                for k in range(len(self.shapes) - 1, -1, -1):
                    sh = self.shapes[k]
                    if not (sh.box[0] - 1 <= x <= sh.box[2] + 1 and sh.box[1] - 1 <= y <= sh.box[3] + 1): continue
                    d = sh.sdf(x, y)
                    if d < 0:
                        own[j][i] = k
                        e = 1 - _clamp(-d / max(0.3, sh.bev), 0, 1)
                        gx, gy = sh.sdf(x + 0.25, y) - sh.sdf(x - 0.25, y), sh.sdf(x, y + 0.25) - sh.sdf(x, y - 0.25)
                        gl = math.hypot(gx, gy) or 1
                        nx, ny, nz = gx / gl * e, gy / gl * e, math.sqrt(max(0, 1 - e * e))
                        v = nx * LIGHT[0] + ny * LIGHT[1] + nz * LIGHT[2] - LIGHT[2]
                        t = 2 + (v * 6.0 if v > 0 else v * 3.4) + sh.bias + (sh.tex(x, y) if sh.tex else 0)
                        tone[j][i] = t
                        break
        cv = Canvas(W, H)
        for j in range(H):
            for i in range(W):
                k = own[j][i]
                if k is None: continue
                sh = self.shapes[k]
                t = tone[j][i]
                for a, b in ((-1, -1), (0, -1), (-1, 0)):  # a shape lying on this one casts a shadow down and right
                    q = own[j + b][i + a] if 0 <= j + b < H and 0 <= i + a < W else None
                    if q is not None and q > k and self.shapes[q].grp != sh.grp: t -= 1.0; break
                empty = lambda a, b: not (0 <= j + b < H and 0 <= i + a < W) or own[j + b][i + a] is None
                if empty(0, 1) or empty(1, 0): t -= 1.0  # the silhouette's own shadowed edge, no outline
                cv.px[j][i] = sh.mat[int(_clamp(round(t), 0, len(sh.mat) - 1))]
        for mx, my, c in self.marks:
            col = c[0][c[1]] if isinstance(c[0], list) else c
            for j in range(math.floor(my * s) - oy, math.floor((my + 1) * s) - oy):
                for i in range(math.floor(mx * s) - ox, math.floor((mx + 1) * s) - ox):
                    if 0 <= j < H and 0 <= i < W: cv.px[j][i] = col
        self._c = cv
        self.piv = (-ox, -oy)
        self.end = (self.end_l[0] * s - ox, self.end_l[1] * s - oy)


# ---------------------------------------------------------------- palette

SKIN = ramp('#c88a64'); SKIN2 = ramp('#b77a58')
GOB = ramp('#6c8f3c'); TROLLG = ramp('#748a58'); IMP = ramp('#c4442c'); DRAUG = ramp('#6c8898'); ROT = ramp('#86945a')
STEEL = ramp('#8e96a4'); DSTEEL = ramp('#545864'); BLACK = ramp('#34343e'); RUST = ramp('#8e5a36'); GOLD = ramp('#d4a640')
RED = ramp('#a63228'); DRED = ramp('#6e1c22'); CREAM = ramp('#ddd0a8'); GAMB = ramp('#b49a64'); OCHRE = ramp('#a88a46')
LEATHER = ramp('#7c5434'); DLEATHER = ramp('#58402e'); TROUSER = ramp('#5c4c40'); WOOD = ramp('#8c6238'); BROWN = ramp('#6e5a44')
BONE = ramp('#d6cca8'); PURPLE = ramp('#5e3c80'); ICE = ramp('#5c9ed0'); PALE = ramp('#bcc0d0'); STONE = ramp('#827e78')
MOSS = ramp('#4e7040'); WOLF = ramp('#72727a'); WOLFD = ramp('#4a4a52'); HORSE = ramp('#2e4c60'); WEED = ramp('#3e7c48')
BAT = ramp('#5e3e44'); WING = ramp('#74404a'); GINGER = ramp('#b4642c'); GREY = ramp('#a0a0a4'); BLUE = ramp('#3e5c84')
FUR = ramp('#8a7258'); ROBE = ramp('#7c1e24'); NAVY = ramp('#2c3450')
EYE = (22, 16, 20); WHITE = (240, 236, 224)
GLOW_CYAN, GLOW_RED, GLOW_GREEN, GLOW_EMBER, GLOW_VIOLET, GLOW_YELLOW = (160, 255, 240), (255, 80, 50), (160, 255, 110), (255, 170, 70), (210, 140, 255), (255, 226, 90)


def mail(x, y): return -0.7 if (y % 2 < 1 and (x + y // 2) % 3 < 2) else 0.1          # rows of rings
def quilt(x, y): return -0.8 if (x % 4 < 0.6) else 0.0                                   # quilted seams
def fur(x, y): return -0.6 if ((x * 7 + (y // 2) * 13) % 5 < 1) else 0.0
def grain(x, y): return -0.5 if ((y + x // 2) % 4 < 1) else 0.0
def folds(x, y): return -0.7 if (x % 5 < 0.8) else 0.0
def rags(x, y): return -0.9 if ((x * 5 + y * 3) % 11 < 1.5) else 0.0


# ---------------------------------------------------------------- body parts

def upper_arm(L, r, mat, s=1, pad=None, tex=None):
    p = Part((0, L), s)
    p.cap(0, 0, 0, L, r, r * 0.84, mat, tex=tex)
    if pad: p.ell(0.3, 0.6, r + 1.9, r + 1.0, pad, bev=2)
    return p


def fore_arm(L, r, mat, hand, s=1, cuff=None, claws=None, tex=None, bony=False):
    p = Part((0, L), s)
    if bony:
        p.cap(0, 0, 0, L - 1, r * 0.55, r * 0.5, BONE)
        p.ell(0, 0.3, r * 0.8, r * 0.7, BONE)
        for k in (-1, 0, 1): p.line(k * 0.9, L - 1, k * 1.1 + 0.6, L + 2.5, (BONE, 3 if k < 1 else 1))
        return p
    p.cap(0, 0, 0, L - 0.5, r * 0.92, r * 0.74, mat, tex=tex)
    if cuff: p.cap(0, L - 3, 0, L - 1.2, r * 0.9, r * 0.9, cuff, bev=1.2)
    p.ell(0.6, L + r * 0.45, r * 1.0, r * 1.12, hand, bev=r * 0.9)   # a big Noita mitt of a hand
    p.circle(r * 0.95, L - 0.2, r * 0.42, hand)                     # thumb
    if claws:
        for k in (-1, 0, 1): p.line(0.6 + k * r * 0.6, L + r * 1.3, 1.4 + k * r * 0.7, L + r * 1.3 + 2.2, claws)
    return p


def thigh(L, r, mat, s=1, tex=None, knee=None):
    p = Part((0, L), s)
    p.cap(0, 0, 0, L, r * 1.06, r * 0.84, mat, tex=tex)
    if knee: p.ell(0.4, L, r * 0.95, r * 0.8, knee, bev=1.5)
    return p


def shin(L, r, mat, boot, s=1, toe=4.0, bare=False, cuff=None, tex=None, bony=False):
    p = Part((0, L), s)
    if bony:
        p.cap(0, 0, 0, L - 1.5, r * 0.5, r * 0.45, BONE)
        p.ell(0, 0.3, r * 0.75, r * 0.7, BONE)
        p.poly([(-1.2, L - 2.2), (1.2, L - 2.2), (toe + 1.5, L - 0.6), (toe + 1.5, L), (-1.5, L)], BONE, bev=1)
        return p
    ank = L - 4
    p.cap(0, 0, 0, ank, r * 0.86, r * 0.66, mat, tex=tex)
    w = r * 0.85
    p.poly([(-w, ank - 2), (w, ank - 2), (w + 0.4, L - 3.6), (w + toe * 0.7, L - 3), (w + toe, L - 1.6), (w + toe, L), (-w - 0.6, L)], boot, bev=1.8)
    if bare:
        for k in range(3): p.dot(w + toe - 0.5 - k * 1.5, L - 1, (boot, 0))
    if cuff: p.cap(-w - 0.3, ank - 1.6, w + 0.3, ank - 1.6, 1.2, 1.2, cuff, bev=1.2)
    return p


def torso(T, tw, mat, s=1, tex=None, belly=0.0, hunch=0.0, chest=None):
    """A broad trunk from the neck (pivot) down to the pelvis; `hunch` pushes the shoulders forward."""
    p = Part((0, T), s)
    h = tw / 2
    p.poly([(-h + 1 + hunch, 0), (h - 1 + hunch, -0.5), (h + 0.6 + hunch * 0.7, 3), (h + belly * 0.4, T * 0.62), (h * 0.84, T + 0.8),
            (-h * 0.84, T + 0.8), (-h - 0.2, T * 0.55), (-h - 0.5 + hunch * 0.5, 3)], mat, bev=h * 0.7, tex=tex)
    if belly: p.ell(h * 0.3, T * 0.62, h * 0.8 + belly * 0.5, T * 0.32 + belly * 0.3, mat, bev=h * 0.7, tex=tex)
    if chest: p.ell(h * 0.25 + hunch * 0.5, T * 0.26, h * 0.82, T * 0.24, chest, bev=3, tex=tex)
    return p


def belt(p, T, tw, mat, buckle=GOLD):
    p.cap(-tw * 0.42, T - 1.2, tw * 0.44, T - 1.2, 1.3, 1.3, mat, bev=1.2)
    p.dot(tw * 0.18, T - 2, (buckle, 3)); p.dot(tw * 0.18, T - 1, (buckle, 2))


# ---- weapons and shields

def blade(L, w=1.6, steel=STEEL, guard=GOLD, hilt=4, s=1):
    p = Part((0, L), s)
    p.cap(0, -hilt, 0, 1, 0.9, 0.9, DLEATHER, bev=0.8)
    p.circle(0, -hilt - 0.5, 1.3, guard)
    p.cap(-3.4, 1.6, 3.4, 1.6, 0.9, 0.9, guard, bev=0.9)
    p.poly([(-w, 2.4), (w, 2.4), (w * 0.8, L - 3), (0, L), (-w * 0.8, L - 3)], steel, bev=w)
    p.line(-0.2, 3.2, -0.2, L - 3, (steel, 4))
    return p


def hafted(L, head, butt=3, r=0.9, wood=WOOD, s=1):
    p = Part((0, L), s)
    p.cap(0, -butt, 0, L - 1, r, r, wood, bev=r, tex=grain)
    head(p, L)
    return p


def spear_tip(p, L):
    p.poly([(0, L + 7), (2, L + 1.5), (0.8, L - 2), (-0.8, L - 2), (-2, L + 1.5)], STEEL, bev=1.4)
    p.cap(0, L - 2.5, 0, L - 1, 1.3, 1.3, DSTEEL, bev=1)
    p.line(0, L - 1, 0, L + 5, (STEEL, 4))


def fork_tip(p, L):
    p.cap(-3.4, L - 1, 3.4, L - 1, 0.9, 0.9, RUST, bev=0.8)
    for k in (-3.2, 0, 3.2): p.cap(k, L - 1, k * 1.1, L + 7, 0.7, 0.45, RUST, bev=0.6)


def axe_tip(sz=1.0, metal=STEEL):
    def f(p, L):
        y = L - 2.5
        p.poly([(0.6, y - 2.5 * sz), (-3.5 * sz, y - 4.5 * sz), (-7 * sz, y - 2.5 * sz), (-7.6 * sz, y + 2.5 * sz), (-4.5 * sz, y + 5.5 * sz), (0.6, y + 2.5 * sz)], metal, bev=2 * sz)
        p.line(-7.2 * sz, y - 2.2 * sz, -7.6 * sz, y + 2.2 * sz, (metal, 4))
    return f


def club_tip(p, L):
    p.ell(0, L - 2.5, 4, 6.5, WOOD, bev=3.5, tex=grain)
    for dx, dy in [(-4, -6), (4, -5), (-4.6, 0), (4.6, 1), (-3.4, 4), (3.6, 4.5), (0, -9)]:
        p.cap(dx * 0.8, L - 2.5 + dy * 0.8, dx * 1.2, L - 2.5 + dy * 1.12, 0.7, 0.3, BONE, bev=0.6)


def orb_tip(glow, skull=False):
    def f(p, L):
        p.cap(-2.2, L - 4, -2.2, L + 1, 0.7, 0.5, WOOD, bev=0.6); p.cap(2.2, L - 4, 2.2, L + 1, 0.7, 0.5, WOOD, bev=0.6)
        if skull:
            p.ell(0, L + 2.6, 3.4, 3.6, BONE, bev=3)
            p.dot(-1.4, L + 2.4, glow); p.dot(0.8, L + 2.4, glow); p.line(-1, L + 4.8, 1, L + 4.8, (BONE, 0))
        else:
            g = [_mix(glow, (0, 0, 0), 0.6), _mix(glow, (0, 0, 0), 0.35), glow, _mix(glow, (255, 255, 255), 0.4), (255, 255, 240)]
            p.circle(0, L + 2.4, 2.8, g)
    return f


def bow(L, s=1):
    p = Part((0, -L), s)
    for k in range(-L, L):
        t = k / L
        p.cap(3.4 * (1 - t * t), k, 3.4 * (1 - (t + 1 / L) ** 2), k + 1, 0.85, 0.85, WOOD, bev=0.8, grp=1)
    p.line(0, -L + 1, 0, L - 1, (CREAM, 2))
    p.ell(3.6, 0, 1.3, 2.0, LEATHER, bev=1)
    return p


def bomb(s=1):
    p = Part((0, 4), s)
    p.circle(0, 4, 3.8, DSTEEL)
    p.dot(-1.5, 2.5, (DSTEEL, 4))
    p.line(0.5, 0.4, 2, -1.6, (CREAM, 1)); p.dot(2, -2.4, GLOW_EMBER); p.dot(2.6, -3, (255, 240, 170))
    return p


def lantern(glow, s=1):
    p = Part((0, 8), s)
    p.line(0, 0, 0, 2.5, (DSTEEL, 1))
    p.poly([(-3, 3), (3, 3), (3.4, 11), (-3.4, 11)], DSTEEL, bev=1)
    g = [_mix(glow, (0, 0, 0), 0.5), _mix(glow, (0, 0, 0), 0.25), glow, _mix(glow, (255, 255, 255), 0.4), (255, 255, 255)]
    p.poly([(-1.8, 4.6), (1.8, 4.6), (2, 9.6), (-2, 9.6)], g, bev=1.5, bias=0.4)
    p.cap(-3.8, 2.6, 3.8, 2.6, 0.9, 0.9, DSTEEL, bev=0.8); p.cap(-4, 11.4, 4, 11.4, 1, 1, DSTEEL, bev=0.8)
    return p


def heater(r, face, mark, s=1):
    p = Part((0, -r), s)
    p.poly([(-r, -r * 1.05), (r, -r * 1.05), (r, r * 0.25), (0, r * 1.25), (-r, r * 0.25)], face, bev=r * 0.7)
    p.cap(-r + 0.5, -r * 1.05 + 0.4, r - 0.5, -r * 1.05 + 0.4, 0.7, 0.7, STEEL, bev=0.6)
    cw = max(1.0, r / 4.5)
    for y in range(int(-r * 0.75), int(r * 0.8)):
        for x in range(int(-cw), int(cw) + 1): p.dot(x, y, (mark, 3 if x <= 0 else 2))
    for x in range(int(-r * 0.6), int(r * 0.6) + 1):
        for y in range(int(-r * 0.32 - cw), int(-r * 0.32 + cw) + 1): p.dot(x, y, (mark, 3 if y < -r * 0.32 else 2))
    return p


def roundshield(r, face, face2=None, s=1, boss=STEEL):
    p = Part((0, -r), s)
    p.circle(0, 0, r, DSTEEL, bev=1.4)
    p.circle(0, 0, r - 1.1, face, bev=r * 0.8, tex=grain)
    if face2:  # quartered
        for y in range(-int(r), int(r) + 1):
            for x in range(-int(r), int(r) + 1):
                if (x + 0.5) ** 2 + (y + 0.5) ** 2 < (r - 1.3) ** 2 and (x < 0) == (y < 0): p.dot(x, y, (face2, 3 if x + y < -r * 0.6 else 2))
    p.circle(0.3, 0, 2.2, boss, bev=2.2)
    return p


def wing(L, mem, bone, s=1):
    """A membrane wing from its root (pivot) out to its tip (+x), the skin hanging below the finger bones."""
    p = Part((L, 1), s)
    tips = [(L, 1), (L * 0.8, L * 0.46), (L * 0.5, L * 0.64), (L * 0.2, L * 0.56)]
    p.poly([(0, -0.5)] + [(L + 0.5, 0.5), (L * 0.86, L * 0.36), (L * 0.66, L * 0.42), (L * 0.52, L * 0.6), (L * 0.36, L * 0.48), (L * 0.18, L * 0.58), (0, L * 0.3)], mem, bev=2.5, tex=folds)
    for tx, ty in tips[:3]: p.cap(0, 0, tx, ty, 1.0, 0.4, bone, bev=0.8)
    return p


# ---------------------------------------------------------------- heads (pivot = neck, bone up to the crown, face to the right)

def head(H, s=1):
    return Part((0, -H), s)


def eyes(p, x, y, col=EYE, gap=3.2, w=1):
    for i in range(w): p.dot(x + i, y, col); p.dot(x - gap + i, y + 0.3, col)


# ---------------------------------------------------------------- the creatures

RIGS = {}


def rig(name, kind, parts, grip=1.3, sgrip=2.5, shoulder=0.13, wclass='RW_NONE', tw=16):
    RIGS[name] = dict(kind=kind, parts=parts, grip=grip, sgrip=sgrip, shoulder=shoulder, wclass=wclass, sw=tw / 2 - 1.6, hw=tw * 0.2)


def human_limbs(s=1, arm=ramp('#000000'), hand=SKIN, leg=TROUSER, boot=LEATHER, ar=2.7, lr=3.1, ua=8, fa=8, th=9.5, sh=10.5,
                pad=None, armtex=None, foretex=None, fore=None, legtex=None, shinmat=None, shintex=None, knee=None, cuff=None, bootcuff=None,
                bare=False, claws=None, toe=4.0):
    return {'UARM': upper_arm(ua, ar, arm, s, pad, armtex),
            'FARM': fore_arm(fa, ar * 0.94, fore or arm, hand, s, cuff, claws, foretex),
            'THIGH': thigh(th, lr, leg, s, legtex, knee),
            'SHIN': shin(sh, lr * 0.92, shinmat or leg, boot, s, toe, bare, bootcuff, shintex)}


# --- Castle Dunmoor's guard: a quilted ochre gambeson under a red tabard, a broad kettle hat shading the face
def guard():
    T, tw = 15, 16
    t = torso(T, tw, GAMB, tex=quilt, chest=GAMB)
    t.poly([(-3.6, 0.5), (4.6, 0.5), (5.2, T + 5), (-3.2, T + 5)], RED, bev=2.5, tex=folds)   # the tabard, hanging past the belt
    belt(t, T, tw, LEATHER)
    t.line(0.9, 3, 0.9, 11, (CREAM, 3)); t.line(-1.4, 6, 3.4, 6, (CREAM, 3))                  # Dunmoor's white cross
    t.poly([(-tw / 2, T - 1), (tw / 2, T - 1), (tw / 2 + 1, T + 3.5), (-tw / 2 - 1, T + 3.5)], GAMB, bev=2, tex=quilt)
    h = head(14)
    h.cap(-0.6, -1, -0.6, 2, 2.6, 2.6, SKIN, bev=2)
    h.ell(0.8, -6.5, 5.6, 6.0, SKIN, bev=4)                       # the face
    h.ell(5.8, -5.2, 1.7, 2.2, SKIN2, bev=1.5)                    # a big nose
    h.cap(1.5, -2.4, 6.4, -2.6, 1.3, 1.1, GINGER, bev=1, tex=fur) # moustache
    h.ell(0.2, -10, 6.2, 4.4, STEEL, bev=3)                       # the hat's crown
    h.cap(-8.2, -8.4, 9.2, -8.0, 1.2, 1.2, STEEL, bev=1.1)        # its broad brim
    h.line(-7, -7.4, 8, -7.1, (STEEL, 0))                         # the brim's shadow on the face
    eyes(h, 3.6, -6.2)
    h.ell(-3.5, -4.5, 2.6, 3.6, STEEL, bev=2, tex=mail)           # a mail coif round the back
    limbs = human_limbs(arm=GAMB, armtex=quilt, fore=GAMB, foretex=quilt, hand=LEATHER, leg=TROUSER, boot=LEATHER, cuff=LEATHER, bootcuff=FUR)
    rig('GUARD', 'RK_BIPED', dict(HEAD=h, TORSO=t, WEAPON=hafted(28, spear_tip, butt=10), SHIELD=heater(6.5, RED, CREAM), **limbs),
        2.55, 2.5, 0.13, 'RW_POLE', tw)


# --- its knights: a flat-topped great helm with a red plume, plate, a red surcoat, a longsword
def knight():
    T, tw, s = 15, 17, 1.05
    t = torso(T, tw, STEEL, s, chest=STEEL)
    t.poly([(-4, 2.5), (5, 2), (5.8, T + 7), (-3.8, T + 7)], RED, bev=2.2, tex=folds)
    t.line(1, 4.5, 1, 12, (CREAM, 3)); t.line(-1.6, 7, 3.6, 7, (CREAM, 3))
    belt(t, T, tw, DLEATHER)
    h = head(15, s)
    h.cap(-0.4, -1, -0.4, 2.5, 3, 3, STEEL, bev=2, tex=mail)
    h.poly([(-5.6, -13), (5.4, -13.4), (6.6, -6), (6.2, 0), (-5.6, 0)], STEEL, bev=3.5)
    h.cap(0.5, -7.4, 6.6, -7.4, 0.55, 0.55, (16, 16, 22), bev=0.1)         # the eye slit
    h.line(0.6, -6.6, 6.4, -6.6, (STEEL, 4))
    for i in range(3): h.dot(4.2, -4.4 + i * 1.4, (DSTEEL, 0))             # breaths
    h.line(2.4, -13, 2.4, -0.5, (STEEL, 4))                                 # the ridge
    for i in range(8): h.cap(-3.5 - i * 0.6, -13 - i * 0.3, -5.2 - i * 0.9, -17.5 + i * 0.6, 1.2, 0.6, RED, bev=1)  # the plume
    limbs = human_limbs(s, arm=STEEL, pad=STEEL, fore=STEEL, hand=DSTEEL, leg=STEEL, legtex=mail, knee=STEEL, shinmat=STEEL, boot=DSTEEL)
    rig('KNIGHT', 'RK_BIPED', dict(HEAD=h, TORSO=t, WEAPON=blade(18, 1.6, s=s), SHIELD=heater(7, RED, CREAM, s), **limbs), 1.35, 2.5, 0.13, 'RW_BLADE', tw * s)


# --- skeletons: a big grinning skull, a cage of ribs, a spine and pelvis, thin knobbly bones
def skull(h, glow=None, hood=None, crown=False):
    h.ell(0.4, -7.6, 5.8, 6.0, BONE, bev=4)
    h.poly([(-1, -3.6), (5.6, -3.4), (5.2, -0.6), (0, -0.4)], BONE, bev=1.5, grp=2)   # the jaw
    h.ell(3.4, -7.2, 1.6, 1.8, [(16, 12, 14)] * 5, bev=0.5)            # sockets
    h.ell(-0.6, -7.0, 1.3, 1.7, [(16, 12, 14)] * 5, bev=0.5)
    if glow: h.dot(3.4, -7.4, glow); h.dot(-0.6, -7.2, glow)
    h.dot(5.6, -5, (20, 16, 16)); h.dot(5.2, -5, (BONE, 1))
    for i in range(4): h.dot(1 + i * 1.2, -2.2, (BONE, 0))
    if hood:
        h.ell(-1.5, -7.6, 6.6, 7.4, hood, bev=3, tex=folds, grp=3)
        h.poly([(-7.5, -6), (-1.5, -2), (-1, 1), (-7, 1)], hood, bev=2, tex=folds, grp=3)
        h.shapes.insert(0, h.shapes.pop())
        h.shapes.insert(0, h.shapes.pop())
    if crown:
        for i in range(5):
            x = -5 + i * 2.6
            h.poly([(x - 1, -12.6), (x, -16.5 - (i % 2) * 2), (x + 1, -12.6)], GOLD, bev=0.8)
        h.cap(-5.6, -12.4, 6, -12.4, 1, 1, GOLD, bev=1)
        h.dot(0.4, -13, GLOW_GREEN)


def ribcage(T, tw, rag=None, s=1):
    t = Part((0, T), s)
    t.cap(-0.6, 0, -0.6, T, 1.1, 1.1, BONE, bev=1)                       # spine
    for i in range(4):
        y, half = 2.5 + i * T * 0.15, tw / 2 * (0.95 - i * 0.13)
        t.cap(-0.5, y, half, y + 1.8 + i * 0.3, 0.75, 0.6, BONE, bev=0.7, grp=10 + i)
        t.cap(-0.5, y + 0.3, -half * 0.9, y + 2, 0.7, 0.5, BONE, bev=0.6, bias=-0.8, grp=20 + i)
    t.ell(-0.2, T - 0.4, tw * 0.36, 2.4, BONE, bev=2, grp=30)            # pelvis
    if rag:
        t.poly([(-tw / 2, -0.5), (tw * 0.3, -0.5), (tw * 0.2, T * 0.5), (-tw * 0.1, T * 0.7), (-tw * 0.55, T * 0.55)], rag, bev=2, tex=rags, grp=40)
    return t


def skeletons():
    for name, s, rag, hood, glow, wpn, wc, grip, shd in (
            ('SKELETON', 0.95, None, None, None, blade(14, 1.3, RUST, RUST, s=0.95), 'RW_BLADE', 1.35, roundshield(6, WOOD, DRED, 0.95, RUST)),
            ('ARCHER', 0.95, PURPLE, PURPLE, GLOW_VIOLET, bow(11, 0.95), 'RW_BOW', 1.6, None)):
        h = head(13, s); skull(h, glow, hood)
        tw = 11
        limbs = {'UARM': Part((0, 8), s).cap(0, 0, 0, 8, 0.75, 0.65, BONE, bev=0.7).ell(0, 0.4, 1.4, 1.2, BONE, bev=1).ell(0, 8, 1.1, 1, BONE, bev=1),
                 'FARM': fore_arm(8, 2.6, BONE, BONE, s, bony=True),
                 'THIGH': Part((0, 9.5), s).cap(0, 0, 0, 9.5, 0.9, 0.75, BONE, bev=0.8).ell(0, 9.4, 1.3, 1.1, BONE, bev=1),
                 'SHIN': shin(10.5, 2.8, BONE, BONE, s, toe=3.2, bony=True)}
        parts = dict(HEAD=h, TORSO=ribcage(15, tw, rag, s), WEAPON=wpn, **limbs)
        if shd: parts['SHIELD'] = shd
        rig(name, 'RK_BIPED', parts, grip, 2.5, 0.13, wc, tw * s)


# --- goblins: Noita's hiisi cousins. Big head, long drooping ears, a nose like a parsnip, bare feet
def goblin_head(h, bomber=False):
    h.cap(-0.4, -1, -0.4, 1.5, 2.4, 2.4, GOB, bev=2)
    h.poly([(-2, -7.5), (-11.5, -11.5), (-10, -9.5), (-3, -4.5)], GOB, bev=1.6, grp=2)    # the long ear, behind
    h.ell(0.6, -7, 5.8, 5.6, GOB, bev=4)
    h.cap(4.4, -6.8, 8.8, -4.4, 2.0, 1.2, GOB, bev=1.6, grp=3)                         # the nose
    h.cap(1.5, -2.6, 5.6, -3.0, 0.5, 0.5, [(50, 20, 18)] * 5, bev=0.1)                   # a wide mouth
    h.dot(4.6, -2.5, WHITE)                                                             # a tooth
    h.dot(3.2, -8.2, GLOW_YELLOW); h.dot(4.2, -8.2, EYE); h.dot(-0.2, -8, EYE)
    h.line(1.8, -9.8, 4.8, -9.4, (GOB, 0))                                              # a scowl
    if bomber:
        h.ell(0.2, -10.4, 6.2, 3.4, RED, bev=2, tex=folds)                             # a red head-rag
        h.cap(-5.5, -9.6, -9.5, -6.5, 1.2, 0.7, RED, bev=1)
        h.ell(3.6, -9.4, 2.4, 1.8, BRASSR, bev=1.2)                                     # goggles pushed up
        h.dot(3.6, -9.6, (200, 230, 240))


BRASSR = ramp('#b08a3c')


def goblins():
    s = 0.7
    for name, bomber in (('GOBLIN', False), ('BOMBER', True)):
        T, tw = 13, 15
        t = torso(T, tw, GOB, s, belly=2.5, hunch=1.5)
        t.poly([(-6, 1), (2, 0.5), (3.5, T - 1), (-6.5, T - 1)], BROWN if bomber else LEATHER, bev=2, tex=rags)  # a ragged vest
        t.poly([(-6, T - 2), (6, T - 2), (5, T + 5), (2, T + 3.5), (-1, T + 6), (-5, T + 4)], LEATHER, bev=2, tex=fur)  # loincloth
        belt(t, T, tw, DLEATHER, BRASSR)
        if bomber:
            t.cap(-6, 1, 5, T - 3, 0.8, 0.8, LEATHER, bev=0.6)
            t.ell(-6.2, T - 1.5, 3, 2.8, LEATHER, bev=2)
            t.circle(-6.6, T - 5, 1.8, DSTEEL); t.circle(-4.4, T - 5.6, 1.6, DSTEEL)
        h = head(12, 0.78); goblin_head(h, bomber)
        limbs = human_limbs(s, arm=GOB, fore=GOB, hand=GOB, leg=LEATHER, shinmat=GOB, boot=GOB, bare=True, ar=2.8, lr=3.0, ua=8, fa=8, th=8, sh=9, toe=4.5)
        wpn = bomb(s) if bomber else hafted(10, axe_tip(0.75, RUST), butt=2, s=s)
        rig(name, 'RK_BIPED', dict(HEAD=h, TORSO=t, WEAPON=wpn, **limbs), 0.0 if bomber else 1.5, 2.5, 0.16, 'RW_THROW' if bomber else 'RW_BLADE', tw * s)


# --- the cultist: a deep red robe and a hood swallowing the face, two embers for eyes, a fire-orb staff
def hooded(h, hood, face, glow, ember_mouth=False):
    h.cap(-0.4, -1, -0.4, 1.5, 2.6, 2.6, hood, bev=2)
    h.ell(-0.6, -7.2, 6.6, 7.2, hood, bev=4, tex=folds)
    h.poly([(-7, -6), (0, -1.6), (0.5, 1.6), (-7.5, 1.6)], hood, bev=2.5, tex=folds)
    h.ell(3.2, -6.4, 3.2, 4.4, face, bev=1.5, bias=-0.4)                   # the face, deep in shadow
    h.line(0.4, -10.5, 6.2, -10.2, (hood, 4))                             # the hood's lit lip
    if glow: h.dot(2.6, -7, glow); h.dot(4.8, -7, glow)


def cultist():
    T, tw = 15, 16
    t = torso(T, tw, ROBE, tex=folds)
    t.poly([(-tw * 0.44, T * 0.6), (tw * 0.44, T * 0.6), (tw * 0.5 + 3.5, T + 18), (-tw * 0.5 - 4, T + 18)], ROBE, bev=4, tex=folds)
    t.line(tw * 0.5 + 3.4, T + 17.5, -tw * 0.5 - 4, T + 17.5, (GOLD, 2)); t.line(2, 1, tw * 0.5 + 2.6, T + 17, (GOLD, 3))
    belt(t, T, tw, BLACK, GOLD)
    for i in range(3): t.dot(-3 + i * 2, T + 1 + i * 1.5, (GOLD, 3))   # a hanging sigil cord
    h = head(14); hooded(h, ROBE, [(24, 10, 12)] * 2 + [(40, 18, 18)] * 3, GLOW_EMBER)
    limbs = human_limbs(arm=ROBE, armtex=folds, fore=ROBE, hand=SKIN, leg=ROBE, legtex=folds, shinmat=ROBE, shintex=folds, boot=BLACK, lr=3.8)
    rig('CULTIST', 'RK_BIPED', dict(HEAD=h, TORSO=t, WEAPON=hafted(26, orb_tip(GLOW_EMBER), butt=8), **limbs), 2.7, 2.5, 0.13, 'RW_STAFF', tw)


# --- the fire imp: a pot-bellied little devil, horns, a grin, leathery wings
def imp():
    s = 0.6
    T, tw = 13, 15
    t = torso(T, tw, IMP, s, belly=3, hunch=1)
    h = head(13, 0.7)
    h.cap(-0.4, -1, -0.4, 1.5, 2.4, 2.4, IMP, bev=2)
    for d in (-1, 1): h.cap(d * 2.5 - 0.5, -10.5, d * 2 - 3.5, -16, 1.6, 0.4, BONE, bev=1.2, grp=5 + d)
    h.ell(0.6, -6.8, 6, 5.8, IMP, bev=4)
    h.cap(1, -3, 6, -3.6, 0.6, 0.6, [(40, 8, 6)] * 5, bev=0.1)
    for i in range(3): h.dot(2 + i * 1.4, -3.2, WHITE)
    eyes(h, 3.6, -7.6, GLOW_YELLOW, 3.4, 2)
    limbs = human_limbs(s, arm=IMP, fore=IMP, hand=IMP, claws=(BONE, 3), leg=IMP, shinmat=IMP, boot=IMP, bare=True)
    rig('IMP', 'RK_BIPED', dict(HEAD=h, TORSO=t, WING=wing(14, WING, IMP), **limbs), 1.3, 2.5, 0.13, 'RW_NONE', tw * s)


# --- ghosts: no legs, a robe that trails into tatters; rigPose floats the hem a unit off the ground
def ghost_robe(T, tw, mat, L):
    t = Part((0, T))
    pts = [(-tw / 2 + 1, 0), (tw / 2 - 1, -0.5), (tw / 2 + 1.5, T * 0.7), (tw / 2 + 2, L * 0.75)]
    n = 7
    for i in range(n + 1):
        x = tw / 2 + 2 - (tw + 6) * i / n
        pts.append((x, L - (4 if i % 2 else 0) - (i * 37 % 5) * 0.5))
    pts.append((-tw / 2 - 2, T * 0.5))
    t.poly(pts, mat, bev=4, tex=folds)
    return t


def ghosts():
    T, tw = 14, 15
    t = ghost_robe(T, tw, ICE, T + 16)
    h = head(14); hooded(h, ICE, [(6, 10, 18)] * 5, None)
    h.dot(2.4, -7, GLOW_CYAN); h.dot(4.8, -7, GLOW_CYAN); h.dot(2.4, -6, (90, 200, 220)); h.dot(4.8, -6, (90, 200, 220))
    limbs = {'UARM': upper_arm(8, 2.5, ICE, tex=folds), 'FARM': fore_arm(8, 2.4, ICE, PALE)}
    rig('WRAITH', 'RK_GHOST', dict(HEAD=h, TORSO=t, WEAPON=lantern(GLOW_CYAN), **limbs), -1.0, 2.5, 0.13, 'RW_STAFF', tw)

    t = ghost_robe(T, tw, PALE, T + 17)
    h = head(14)
    for i in range(10):  # long white hair, streaming back
        y0 = -12 + i * 1.5
        h.cap(-1, y0, -9 - (i * 7 % 4), y0 + 7 + i * 0.7, 1.2, 0.5, PALE, bev=1, tex=folds, grp=50)
    h.ell(1.4, -7, 4.8, 6, PALE, bev=3.5, bias=0.3)
    h.ell(3.6, -8, 1.2, 1.8, [(10, 10, 18)] * 5, bev=0.1); h.ell(0.4, -7.8, 1.0, 1.6, [(10, 10, 18)] * 5, bev=0.1)
    h.ell(3.4, -3.4, 1.3, 2.0, [(10, 10, 18)] * 5, bev=0.1)   # the mouth, open in a wail
    limbs = {'UARM': upper_arm(8, 2.4, PALE, tex=folds), 'FARM': fore_arm(8, 2.2, PALE, PALE)}
    rig('BANSHEE', 'RK_GHOST', dict(HEAD=h, TORSO=t, **limbs), 1.3, 2.5, 0.13, 'RW_NONE', tw)


# --- the rock golem: stacked boulders, moss on its shoulders, a rune burning in its chest
def golem():
    T, tw = 19, 20
    t = Part((0, T))
    t.ell(1, 5, 10.5, 7, STONE, bev=5, grp=1)
    t.ell(0, 13, 8.5, 6.5, STONE, bev=5, grp=2)
    t.ell(-0.5, T, 7, 3.6, STONE, bev=3, grp=3)
    t.ell(-3, -0.5, 6, 2.2, MOSS, bev=1.5, tex=fur)
    rn = [(30, 90, 90), (60, 170, 160), GLOW_CYAN, (210, 255, 250), (255, 255, 255)]
    t.line(2, 3, 4, 6, (rn, 2)); t.line(4, 6, 2, 9, (rn, 2)); t.line(3, 6, 6, 6, (rn, 3))
    h = head(11)
    h.poly([(-5, -10), (5, -10.5), (6.4, -3), (5, 0), (-5, 0)], STONE, bev=3.5)
    h.cap(1.5, -5.6, 5.8, -5.6, 0.6, 0.6, rn, bev=0.1, bias=0)
    h.line(1.5, -5.4, 5.8, -5.4, GLOW_CYAN)
    h.ell(-2, -10.3, 3.5, 1.4, MOSS, bev=1, tex=fur)
    ua = Part((0, 10)); ua.ell(0, 1, 4.6, 4.2, STONE, bev=3, grp=1); ua.ell(0, 7, 3.8, 4, STONE, bev=3, grp=2)
    fa = Part((0, 10)); fa.ell(0, 3, 3.8, 4, STONE, bev=3, grp=1); fa.ell(0.5, 10.5, 5, 4.4, STONE, bev=3.5, grp=2)
    th = Part((0, 10)); th.ell(0, 3, 4.4, 4.6, STONE, bev=3, grp=1); th.ell(0, 8.5, 3.8, 3.4, STONE, bev=3, grp=2)
    sh = Part((0, 11)); sh.ell(0, 3, 3.8, 4, STONE, bev=3, grp=1); sh.poly([(-4.5, 6), (4, 6), (7, 9), (7, 11), (-4.6, 11)], STONE, bev=2.5, grp=2)
    rig('GOLEM', 'RK_BIPED', dict(HEAD=h, TORSO=t, UARM=ua, FARM=fa, THIGH=th, SHIN=sh), 1.3, 2.5, 0.12, 'RW_NONE', tw)


# --- the redcap: a squat old murderer, beard to his belt, a cap dyed in blood, iron boots, a hatchet
def redcap():
    s = 0.7
    T, tw = 13, 15
    t = torso(T, tw, BROWN, s, belly=2, tex=folds)
    belt(t, T, tw, DLEATHER)
    t.poly([(-6, T - 2), (6, T - 2), (7, T + 3), (-7, T + 3)], BROWN, bev=2, tex=folds)
    h = head(14, 0.72)
    h.cap(-0.4, -1, -0.4, 1.5, 2.4, 2.4, SKIN, bev=2)
    h.ell(0.6, -6.4, 5.4, 5.6, SKIN, bev=4)
    h.ell(5.4, -6, 1.8, 1.8, SKIN2, bev=1.5)
    h.poly([(-1.5, -5), (6.5, -4.6), (5, 3), (1.5, 6), (-2, 1)], GREY, bev=2.5, tex=fur)   # the beard
    h.poly([(-6, -8.5), (6.2, -9.5), (2, -13.5), (-6, -19), (-10, -17), (-5.5, -12)], RED, bev=3, tex=folds)  # the cap
    h.cap(-6.4, -8.6, 6.6, -9.4, 1.1, 1.1, RED, bev=1, bias=-0.6)
    eyes(h, 3.4, -7, GLOW_RED, 3.2)
    limbs = human_limbs(s, arm=BROWN, armtex=folds, fore=BROWN, hand=SKIN, leg=TROUSER, boot=DSTEEL, ar=2.8, lr=3.0, th=8, sh=9, toe=4.5)
    rig('REDCAP', 'RK_BIPED', dict(HEAD=h, TORSO=t, WEAPON=hafted(14, axe_tip(0.85), s=s), **limbs), 1.5, 2.5, 0.15, 'RW_BLADE', tw * s)


# --- the draugr: a drowned Norse dead man, blue-grey, rusted helm, braided beard, rotting mail, eyes like ice
def draugr():
    T, tw = 15, 17
    t = torso(T, tw, RUST, tex=mail, chest=RUST)
    t.poly([(-tw / 2, T - 1), (tw / 2, T - 1), (tw / 2 + 1, T + 5), (-tw / 2 - 1, T + 5)], RUST, bev=2, tex=mail)
    t.poly([(-3, 1), (4, 1), (5, T + 3), (1, T + 6), (-3.5, T + 3)], NAVY, bev=2, tex=rags)
    belt(t, T, tw, DLEATHER)
    h = head(14)
    h.cap(-0.4, -1, -0.4, 1.5, 2.6, 2.6, DRAUG, bev=2)
    h.ell(0.8, -6.6, 5.6, 6, DRAUG, bev=4)
    h.ell(-0.1, -9.6, 6.2, 4.4, RUST, bev=3)                         # the helm
    h.cap(-6, -7.6, 6.8, -7.6, 1, 1, RUST, bev=1)
    h.cap(5.4, -8.6, 5.6, -4, 0.9, 0.7, RUST, bev=0.8)                # nasal
    h.poly([(0, -3.5), (6.6, -3.6), (4.6, 3), (2.6, 7), (0.5, 2)], GREY, bev=2, tex=fur)  # beard
    h.dot(2.6, 6.5, (GOLD, 3))
    h.dot(3.4, -6.4, GLOW_CYAN); h.dot(4.3, -6.4, (90, 200, 230)); h.dot(0, -6.2, GLOW_CYAN)
    limbs = human_limbs(arm=RUST, armtex=mail, fore=DRAUG, hand=DRAUG, leg=NAVY, legtex=rags, shinmat=DRAUG, boot=DLEATHER, cuff=LEATHER)
    rig('DRAUGR', 'RK_BIPED', dict(HEAD=h, TORSO=t, WEAPON=hafted(16, axe_tip(1.0, RUST)), SHIELD=roundshield(7, BLUE, CREAM, 1, RUST), **limbs),
        1.5, 2.5, 0.13, 'RW_BLADE', tw)


# --- the troll: hunched, huge-armed, pot-bellied, an underbite with tusks, a bone-studded club
def troll():
    s = 1.2
    T, tw = 18, 19
    t = torso(T, tw, TROLLG, s, belly=4, hunch=3, chest=TROLLG)
    t.ell(-4, 2, 6, 4, TROLLG, bev=3)                               # the hump of its back
    t.poly([(-tw / 2, T - 1.5), (tw / 2, T - 1.5), (tw / 2, T + 6), (1, T + 4), (-tw / 2, T + 6.5)], FUR, bev=2, tex=fur)
    belt(t, T, tw, LEATHER, BONE)
    h = head(13, s)
    h.cap(-1, -1, 0.5, 1.5, 3.2, 3.2, TROLLG, bev=2)
    h.ell(1, -6.2, 5.6, 5.4, TROLLG, bev=4)
    h.ell(3.4, -8.4, 4.4, 1.8, TROLLG, bev=1.5, grp=2)               # a shelf of brow
    h.ell(6.2, -5.4, 2.2, 2.6, TROLLG, bev=2, grp=3)                 # the nose
    h.ell(3, -2, 4.6, 2.6, TROLLG, bev=2, grp=4)                     # the jaw, thrust out
    h.cap(4.6, -2.8, 5.2, -5.4, 0.7, 0.4, CREAM, bev=0.5); h.cap(1.6, -2.6, 2, -4.6, 0.6, 0.35, CREAM, bev=0.5)
    h.dot(3.4, -7.2, GLOW_YELLOW); h.dot(0.4, -7, EYE)
    for i in range(5): h.line(-3 + i * 1.3, -10.8, -4.6 + i, -13.6, (FUR, 1 + i % 2))
    limbs = human_limbs(s, arm=TROLLG, fore=TROLLG, hand=TROLLG, cuff=LEATHER, leg=TROLLG, shinmat=TROLLG, boot=TROLLG, bare=True,
                        ar=3.6, lr=3.6, ua=10, fa=10.5, th=9, sh=10, toe=5)
    rig('TROLL', 'RK_BIPED', dict(HEAD=h, TORSO=t, WEAPON=hafted(16, club_tip, r=1.5, s=s), **limbs), 1.6, 2.5, 0.16, 'RW_BLADE', tw * s)


# --- the risen levy: a dead farmhand from the battlefield, hooded, rotting through a torn tunic, a pitchfork
def risen():
    T, tw = 15, 15
    t = torso(T, tw, BROWN, tex=rags, chest=BROWN)
    for i, (x, y) in enumerate([(-3, 4), (3, 8), (-1, 11), (4, 3)]): t.ell(x, y, 1.5, 1.1, ROT, bev=1, grp=60 + i)
    t.poly([(-tw / 2, T - 1), (tw / 2, T - 1), (tw / 2 + 1, T + 4), (3, T + 3), (0, T + 6), (-tw / 2 - 1, T + 4)], BROWN, bev=2, tex=rags)
    belt(t, T, tw, ramp('#8a8070'), ROT)
    h = head(14); hooded(h, BROWN, ROT, GLOW_RED)
    h.line(3, -3.6, 5.8, -3.8, (DRED, 0)); h.dot(5.4, -8.6, (DRED, 2))
    limbs = human_limbs(arm=BROWN, armtex=rags, fore=ROT, hand=ROT, leg=TROUSER, legtex=rags, shinmat=ROT, boot=ramp('#8a8070'))
    rig('RISEN', 'RK_BIPED', dict(HEAD=h, TORSO=t, WEAPON=hafted(26, fork_tip, butt=6), **limbs), 2.5, 2.5, 0.13, 'RW_POLE', tw)


# --- the Black Knight: black plate edged in gold, a tattered red cape, an antlered helm with burning eyes
def black_knight():
    s = 2.1
    T, tw = 15, 17
    t = Part((0, T), s)
    t.poly([(-tw / 2 - 1, 0), (tw / 2 - 1, 0), (tw / 2 + 2, T + 6), (tw / 2 - 2, T + 14), (2, T + 11), (-3, T + 15), (-7, T + 12), (-tw / 2 - 3, T + 14)], DRED, bev=3, tex=folds, grp=1)  # the cape
    tt = torso(T, tw, BLACK, chest=BLACK)
    t.shapes += tt.shapes
    for y in (T * 0.42, T * 0.6, T * 0.78): t.line(-tw * 0.4, y, tw * 0.44, y, (GOLD, 1))
    t.ell(1, 4.5, 2.2, 2, BONE, bev=1.5)
    t.dot(0.2, 4.2, EYE); t.dot(1.8, 4.2, EYE)
    t.poly([(-tw / 2, T - 1), (tw / 2, T - 1), (tw / 2 + 1, T + 5), (-tw / 2 - 1, T + 5)], BLACK, bev=2)
    belt(t, T, tw, DLEATHER)
    h = head(15, s)
    for d in (-1, 1):   # antlers
        bx = d * 2.2
        h.cap(bx, -12, bx - 2 - d, -18, 1.3, 1, BONE, bev=1, grp=5)
        h.cap(bx - 2 - d, -18, bx - 6, -23, 1, 0.6, BONE, bev=0.8, grp=5)
        h.cap(bx - 1.5, -17, bx + 3 * d + 1, -21, 0.9, 0.5, BONE, bev=0.7, grp=5)
        h.cap(bx - 4, -21, bx - 4, -25, 0.7, 0.4, BONE, bev=0.6, grp=5)
    h.cap(-0.4, -1, -0.4, 2.5, 3, 3, BLACK, bev=2)
    h.poly([(-5.6, -13), (5.4, -13.4), (6.8, -5.5), (5.4, 0.5), (-5.6, 0.5)], BLACK, bev=3.5)
    h.line(-5.4, -12.4, 5.2, -12.6, (GOLD, 2))
    h.cap(0.6, -7, 6.6, -7, 0.6, 0.6, [(6, 6, 8)] * 5, bev=0.1)
    h.dot(3.2, -7.4, GLOW_RED); h.dot(5, -7.4, GLOW_RED)
    h.line(2.4, -12.5, 2.4, 0, (BLACK, 4))
    limbs = human_limbs(s, arm=BLACK, pad=BLACK, fore=BLACK, hand=BLACK, leg=BLACK, knee=BLACK, shinmat=BLACK, boot=BLACK)
    rig('BLACKKNIGHT', 'RK_BIPED', dict(HEAD=h, TORSO=t, WEAPON=blade(21, 2.0, STEEL, DSTEEL, 5, s), **limbs), 1.3, 2.5, 0.13, 'RW_BLADE', tw * s)


# --- the Lich King: a crowned skull, rich purple robes over bone, a staff crowned with a skull
def lich():
    s = 1.9
    T, tw = 15, 16
    t = torso(T, tw, PURPLE, s, tex=folds, chest=PURPLE)
    t.poly([(-tw * 0.44, T * 0.6), (tw * 0.44, T * 0.6), (tw * 0.5 + 4, T + 22), (-tw * 0.5 - 4.5, T + 22)], PURPLE, bev=4, tex=folds)
    t.line(tw * 0.5 + 3.8, T + 21.5, -tw * 0.5 - 4.4, T + 21.5, (GOLD, 3)); t.line(2, 1, tw * 0.5 + 3, T + 21, (GOLD, 2))
    t.ell(0.5, 2, tw * 0.55, 3, GOLD, bev=2, bias=-0.5)          # a gilt collar
    belt(t, T, tw, GOLD, GOLD)
    h = head(13, s); skull(h, GLOW_GREEN, crown=True)
    limbs = {'UARM': upper_arm(8, 2.7, PURPLE, s, tex=folds), 'FARM': fore_arm(8, 2.6, PURPLE, BONE, s, bony=False),
             'THIGH': thigh(9.5, 3.9, PURPLE, s, folds), 'SHIN': shin(10.5, 3.6, PURPLE, BLACK, s, tex=folds)}
    rig('LICH', 'RK_BIPED', dict(HEAD=h, TORSO=t, WEAPON=hafted(24, orb_tip(GLOW_GREEN, True), butt=6, r=1.1, s=s), **limbs), 2.7, 2.5, 0.13, 'RW_STAFF', tw * s)


# --- four-footed: the dire wolf and the kelpie; and the bat
def quad_leg(L, r, mat, paw, toe=2.5, hoof=False):
    p = Part((0, L))
    p.cap(0, 0, 0, L - 2, r, r * 0.6, mat, tex=fur if mat is WOLF else None)
    if hoof: p.poly([(-r * 0.9, L - 3), (r * 0.9, L - 3), (r * 1.2, L), (-r, L)], paw, bev=1)
    else: p.ell(toe * 0.4, L - 1, r * 0.8 + toe * 0.5, 1.4, paw, bev=1)
    return p


def wolf():
    B = 19
    t = Part((-B, 0))
    t.ell(-B * 0.5, 0.5, B * 0.55 + 2, 5.2, WOLF, bev=4, tex=fur)
    t.ell(-1.5, -0.5, 6.2, 6.6, WOLF, bev=4, tex=fur)                    # the deep chest
    t.ell(-B + 1, -0.5, 5, 5.4, WOLF, bev=4, tex=fur)                    # the haunch
    t.ell(-B * 0.45, -4.5, B * 0.55, 2.4, WOLFD, bev=2, tex=fur)          # the dark saddle
    for i in range(7): t.cap(-i * 1.6 + 1, -5.5, -i * 1.6 - 1, -8.6 - (i * 3 % 2), 1, 0.4, WOLFD, bev=0.8, grp=7)  # hackles
    t.ell(-B * 0.45, 4, B * 0.4, 1.8, CREAM, bev=1.5, bias=-0.6, tex=fur)  # pale belly
    hd = Part((12 * 0.93, -12 * 0.37))
    hd.ell(3, 0, 5, 4.6, WOLF, bev=3.5, tex=fur)
    hd.poly([(4.5, -3.5), (13.5, -0.5), (13, 1.8), (5, 3.5)], WOLF, bev=2, tex=fur)  # the muzzle
    hd.poly([(0, -2.5), (2, -8.5), (4.6, -3)], WOLFD, bev=1.2)                     # ear
    hd.cap(6.5, 2.2, 12.8, 1.4, 0.55, 0.55, [(70, 10, 14)] * 5, bev=0.1)             # the snarl
    for i in range(3): hd.dot(7.5 + i * 1.6, 1.2, WHITE)
    hd.dot(6, -1.6, GLOW_YELLOW); hd.dot(13.4, -0.6, EYE)
    tail = Part((-11 * 0.86, -11 * 0.5))
    tail.cap(0, 0, -11 * 0.86, -11 * 0.5, 2.2, 1.3, WOLF, bev=1.8, tex=fur)
    tail.ell(-11 * 0.86, -11 * 0.5, 2, 1.5, WOLFD, bev=1.4, tex=fur)
    RIGS['WOLF'] = dict(kind='RK_QUAD', grip=0, sgrip=0, shoulder=0, wclass='RW_NONE', sw=0, hw=0, parts={
        'TORSO': t, 'HEAD': hd, 'UARM': upper_arm(6, 2.2, WOLF, tex=fur), 'FARM': quad_leg(7, 1.7, WOLF, WOLFD),
        'THIGH': upper_arm(6, 2.8, WOLF, tex=fur), 'SHIN': quad_leg(7, 1.7, WOLF, WOLFD), 'TAIL': tail})


def kelpie():
    B = 25
    t = Part((-B, 0))
    t.ell(-B * 0.5, 0, B * 0.55 + 2, 6.6, HORSE, bev=5)
    t.ell(-1.5, -0.5, 7.6, 7.8, HORSE, bev=5)
    t.ell(-B + 1, -0.5, 6.6, 7, HORSE, bev=5)
    for i in range(14): t.cap(-i * 1.5 + 2, -6.6, -i * 1.5 - 1, -10 - (i * 5 % 3), 1.1, 0.5, WEED, bev=0.8, grp=7)
    for i in range(5): t.dot(-B * 0.2 - i * 3, 2 + (i % 2), (WEED, 3))   # weed clinging to its flank
    L = 18
    hd = Part((L * 0.93, -L * 0.37))
    hd.cap(0, 0, 6, -9, 4, 3, HORSE, bev=3)                                  # the neck
    hd.cap(5, -9.5, 16, -5.5, 3.2, 2.2, HORSE, bev=2.4)                      # the long face
    hd.poly([(3.6, -11), (4.6, -15.5), (6.6, -11.4)], HORSE, bev=1)
    hd.dot(8, -9.4, GLOW_CYAN); hd.dot(17, -5.4, EYE)
    for i in range(10): hd.cap(-1 + i * 0.6, -1 - i * 1.1, -4 + i * 0.6, 3 - i * 0.8, 1, 0.5, WEED, bev=0.8, grp=8)
    tail = Part((-15 * 0.86, -15 * 0.5))
    for k in range(3): tail.cap(0, 0, -15 * 0.86 - k, -15 * 0.5 + k * 2.5, 1.6 - k * 0.3, 0.6, WEED, bev=1, grp=20 + k)
    RIGS['KELPIE'] = dict(kind='RK_QUAD', grip=0, sgrip=0, shoulder=0, wclass='RW_NONE', sw=0, hw=0, parts={
        'TORSO': t, 'HEAD': hd, 'UARM': upper_arm(8, 2.7, HORSE), 'FARM': quad_leg(9, 2.0, HORSE, DSTEEL, hoof=True),
        'THIGH': upper_arm(8, 3.2, HORSE), 'SHIN': quad_leg(9, 2.0, HORSE, DSTEEL, hoof=True), 'TAIL': tail})


def bat():
    L = 10
    b = Part((0, L))
    b.ell(0, L * 0.55, 3.8, 5, BAT, bev=3, tex=fur)
    b.ell(0.8, 1.4, 3.4, 3.2, BAT, bev=2.5, tex=fur, grp=2)
    for d in (-1, 1): b.poly([(d * 1.8 - 1.2, -0.5), (d * 2.8, -5.5), (d * 2.4 + 1.4, -0.5)], BAT, bev=1, grp=3)
    b.dot(2.6, 1, GLOW_RED); b.dot(-0.6, 1.2, GLOW_RED); b.dot(2, 3.4, WHITE); b.dot(0.6, 3.4, WHITE)
    RIGS['BAT'] = dict(kind='RK_BAT', grip=0, sgrip=0, shoulder=0.2, wclass='RW_NONE', sw=0, hw=0, parts={'TORSO': b, 'UARM': wing(15, WING, BAT)})


# ---------------------------------------------------------------- the player: a Norse wanderer, one rig per armour set
# METAL is painted neutral grey and recoloured in the game to the armour's metal; RUNE takes its element's glow.
# Their colours are flagged in TINT (emitted as alpha 254 / 253, which rig.cpp reads back as opaque).
METAL = ramp('#9c9c9c'); RUNE = ramp('#9c9c9c')
RUNE[:] = [(60, 60, 60), (110, 110, 110), (190, 190, 190), (230, 230, 230), (255, 255, 255)]
TINT = {c: 254 for c in METAL}
TINT.update({c: 253 for c in RUNE})
WOOL = ramp('#5a6a3e'); HAIR = ramp('#9c5a30'); WRAPS = ramp('#9a8a6c'); PSKIN = ramp('#e0a882'); PSKIN2 = ramp('#c88e6a')
def lamellar(x, y): return -0.8 if y % 3 < 0.7 else (-0.5 if (x + (y // 3) * 2) % 4 < 0.6 else 0.0)
def scales(x, y): return -0.8 if ((y % 3 < 0.8) and ((x + (y // 3) % 2 * 1.5) % 3 < 1.6)) else 0.0


def player_head(kind):
    h = head(13)
    h.cap(-0.6, -1, -0.6, 1.5, 2.4, 2.4, PSKIN, bev=2)
    h.ell(-3.4, -5, 2.2, 5.4, HAIR, bev=1.6, tex=fur)                       # hair falling behind
    h.cap(-4.6, -2, -5.4, 4.5, 1.2, 0.9, HAIR, bev=1, tex=fur, grp=7)        # a braid
    h.ell(0.6, -6.6, 5.3, 5.6, PSKIN, bev=4)
    h.ell(5.2, -5.6, 1.4, 1.9, PSKIN2, bev=1.2)                              # nose
    h.poly([(-1.2, -4.4), (5.8, -3.6), (4.8, 1.2), (2.4, 3.6), (-0.8, 0.8)], HAIR, bev=2, tex=fur, grp=8)  # a full beard
    h.line(2.2, -3.2, 5.6, -3.2, (HAIR, 0))                                  # the mouth line
    if kind == 'bare':
        h.ell(-0.6, -10, 5.4, 2.8, HAIR, bev=2, tex=fur, grp=9)
        eyes(h, 3.4, -6.6, (60, 100, 150))
    elif kind == 'spangen':  # a riveted cap and a nasal
        h.ell(0.2, -9.4, 5.8, 4.4, METAL, bev=3)
        h.cap(-5.6, -7.8, 6.2, -7.8, 1, 1, METAL, bev=1, bias=-0.4)
        h.line(0.6, -13.4, 0.6, -8.4, (METAL, 4))
        h.cap(5.0, -8.6, 5.1, -4.2, 0.8, 0.6, METAL, bev=0.8)
        for x in (-3.5, 2, 4.5): h.dot(x, -8, (METAL, 4))
        eyes(h, 3.4, -6.4, (60, 100, 150))
    elif kind == 'gjermundbu':  # a rounded helm, spectacle guards, mail hanging behind
        h.ell(-3.6, -4.4, 2.8, 4.6, METAL, bev=1.6, tex=mail, grp=10)
        h.ell(0.2, -9.2, 6, 4.8, METAL, bev=3.5)
        h.cap(-5.8, -7.6, 6.2, -7.6, 1, 1, METAL, bev=1, bias=-0.4)
        h.ell(3.6, -6.2, 1.9, 1.6, METAL, bev=0.7, grp=11)
        h.dot(3.6, -6.4, (12, 10, 14)); h.dot(0.6, -6.2, (12, 10, 14))
    else:  # rune / horned: a closed helm, eyes burning with the armour's element, mail at the back
        h.ell(-3.6, -3.6, 3, 4.8, METAL, bev=1.6, tex=mail, grp=10)
        h.poly([(-5.2, -12.4), (4.6, -12.6), (6.2, -6.6), (5.6, -1.6), (-5.2, -2)], METAL, bev=3.2)
        h.cap(0.6, -6.8, 6, -6.8, 0.6, 0.6, [(10, 8, 12)] * 5, bev=0.1)
        h.dot(3, -7.2, (RUNE, 3)); h.dot(4.6, -7.2, (RUNE, 3))
        h.line(2, -12.4, 2, -2, (METAL, 4))
        if kind == 'horned':
            for d in (-1, 1): h.cap(d * 2.6 - 1, -11, d * 6.5 - 3, -17.5, 1.6, 0.5, BONE, bev=1.2, grp=12 + d)
    return h


def player(name, kind, body, body_tex, arm, arm_tex, fore, hand, legs, shinmat, boot, hem=None, studs=False, rune=False, trim=None, pad=None):
    T, tw = 14, 15
    t = torso(T, tw, body, tex=body_tex, chest=body)
    t.poly([(-tw / 2 + 0.4, T - 1), (tw / 2 - 0.4, T - 1), (tw / 2 + 1, T + 4.5), (-tw / 2 - 1, T + 4.5)], hem or body, bev=2, tex=body_tex if not hem else folds)
    belt(t, T, tw, DLEATHER, GOLD)
    if studs:
        for y in (3.5, 7, 10.5):
            for x in (-4, -1, 2, 5): t.dot(x + (y % 2), y, (METAL, 4))
    if trim:
        t.line(-tw / 2 + 1, 3.2, tw / 2 - 0.5, 3.2, (trim, 3)); t.line(-tw / 2 + 1, 4.2, tw / 2 - 0.5, 4.2, (trim, 1))
    if rune:  # Algiz, glowing on the breast
        t.line(1, 4.5, 1, 10, (RUNE, 4)); t.line(1, 6.5, -1, 4.5, (RUNE, 4)); t.line(1, 6.5, 3, 4.5, (RUNE, 4))
    t.ell(0.4, 0.6, tw * 0.5, 2.0, FUR, bev=1.5, tex=fur)               # the fur collar under the cape's pin
    limbs = human_limbs(arm=arm, armtex=arm_tex, fore=fore, foretex=None, hand=hand, leg=legs, shinmat=shinmat, shintex=quilt if shinmat is WRAPS else None,
                        boot=boot, ar=2.5, lr=2.9, ua=8, fa=7.5, th=9, sh=10, pad=pad, cuff=DLEATHER if fore is not METAL else None)
    RIGS[name] = dict(kind='RK_BIPED', parts=dict(HEAD=player_head(kind), TORSO=t, **limbs), grip=0, sgrip=0, shoulder=0.13, wclass='RW_NONE', sw=tw / 2 - 1.6, hw=tw * 0.2)


def players():
    player('PLAYER_WOOL', 'bare', WOOL, folds, WOOL, folds, LEATHER, PSKIN, TROUSER, WRAPS, LEATHER, trim=GOLD)
    player('PLAYER_LEATHER', 'spangen', LEATHER, None, LEATHER, None, DLEATHER, PSKIN, TROUSER, WRAPS, DLEATHER, hem=WOOL, studs=True)
    player('PLAYER_MAIL', 'gjermundbu', METAL, mail, METAL, mail, DLEATHER, LEATHER, TROUSER, WRAPS, DLEATHER, hem=BLUE, pad=METAL)
    player('PLAYER_LAMELLAR', 'rune', METAL, lamellar, METAL, lamellar, METAL, LEATHER, TROUSER, METAL, DLEATHER, hem=NAVY, rune=True, pad=METAL)
    player('PLAYER_SCALE', 'horned', METAL, scales, METAL, scales, METAL, BLACK, BLACK, METAL, BLACK, trim=GOLD, rune=True, pad=METAL)


for _f in (players, guard, knight, skeletons, goblins, cultist, imp, ghosts, golem, redcap, draugr, troll, risen, black_knight, lich, wolf, kelpie, bat): _f()
for _r in RIGS.values():
    for _p in _r['parts'].values(): _p.c  # bake (sets piv and end)

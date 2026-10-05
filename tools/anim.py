# Frame-by-frame character sprites. Every frame of every animation is painted whole: the figure is posed
# on a skeleton, then its shapes (signed-distance primitives) are rasterised straight onto the pixel grid
# and shaded as one picture - lit from the upper left, banded into a few tones, a dark contour wherever
# one part passes in front of another, a one-tone shadow cast down-right, and a dark outline around the
# silhouette. Nothing is rotated afterwards, so the pixels stay crisp in every pose.
#
# Every shape carries a part label (head, torso, arm...), so the same painter also yields loose body parts
# for the ragdoll a creature becomes when it dies.
#
#   python tools/anim.py preview <dir>     sheets (4x) and animated GIFs of every creature
import math, sys
import numpy as np

LIGHT = np.array([-0.62, -0.72, 0.55]); LIGHT /= np.linalg.norm(LIGHT)


def hexc(h): return tuple(int(h[i:i + 2], 16) for i in (1, 3, 5))
def mix(a, b, t): return tuple(int(round(a[i] + (b[i] - a[i]) * t)) for i in range(3))
def scale(c, k): return tuple(max(0, min(255, int(round(v * k)))) for v in c)


def ramp(h, warm=1.0):
    """Five tones from a base colour: contour, shadow, base, light, highlight. Shadows drift to cool violet,
    lights to warm cream, the way painted pixel art does."""
    b = hexc(h)
    return [mix(scale(b, 0.42), (30, 18, 46), 0.3), mix(scale(b, 0.66), (40, 30, 80), 0.18), b,
            mix(scale(b, 1.18), (255, 236, 190), 0.16 * warm), mix(scale(b, 1.35), (255, 248, 224), 0.38 * warm)]


class Mat:
    def __init__(self, h, tex=None, warm=1.0, shine=0.0, glow=False, tag=0):
        self.r = ramp(h, warm) if isinstance(h, str) else h
        self.tex, self.shine, self.glow, self.tag = tex, shine, glow, tag  # tag 1: recoloured as the armour's metal


# ---------------------------------------------------------------- signed distance fields over the canvas grid

def dir_(a): return np.array([math.sin(a), math.cos(a)])  # angle 0 points down, positive turns forward (right)


class Grid:
    def __init__(self, w, h):
        self.w, self.h = w, h
        self.X, self.Y = np.meshgrid(np.arange(w) + 0.5, np.arange(h) + 0.5)

    def circle(self, c, r): return np.hypot(self.X - c[0], self.Y - c[1]) - r

    def ellipse(self, c, rx, ry, rot=0.0):
        cs, sn = math.cos(rot), math.sin(rot)
        dx, dy = self.X - c[0], self.Y - c[1]
        u, v = dx * cs + dy * sn, -dx * sn + dy * cs
        return (np.hypot(u / rx, v / ry) - 1) * min(rx, ry)

    def cap(self, a, b, r0, r1=None):
        r1 = r0 if r1 is None else r1
        ax, ay, bx, by = a[0], a[1], b[0], b[1]
        dx, dy = bx - ax, by - ay
        ll = dx * dx + dy * dy or 1e-9
        t = np.clip(((self.X - ax) * dx + (self.Y - ay) * dy) / ll, 0, 1)
        return np.hypot(self.X - ax - dx * t, self.Y - ay - dy * t) - (r0 + (r1 - r0) * t)

    def poly(self, pts):
        X, Y = self.X, self.Y
        d = np.full(X.shape, 1e9)
        inside = np.zeros(X.shape, bool)
        n = len(pts)
        for i in range(n):
            (x0, y0), (x1, y1) = pts[i], pts[(i + 1) % n]
            ex, ey = x1 - x0, y1 - y0
            wx, wy = X - x0, Y - y0
            t = np.clip((wx * ex + wy * ey) / (ex * ex + ey * ey or 1e-9), 0, 1)
            d = np.minimum(d, np.hypot(wx - ex * t, wy - ey * t))
            cross = ((y0 > Y) != (y1 > Y)) & (X < (x1 - x0) * (Y - y0) / ((y1 - y0) or 1e-9) + x0)
            inside ^= cross
        return np.where(inside, -d, d)

    def box(self, c, hw, hh, rot=0.0, rnd=0.0):
        cs, sn = math.cos(rot), math.sin(rot)
        dx, dy = self.X - c[0], self.Y - c[1]
        u, v = np.abs(dx * cs + dy * sn) - hw + rnd, np.abs(-dx * sn + dy * cs) - hh + rnd
        return np.hypot(np.maximum(u, 0), np.maximum(v, 0)) + np.minimum(np.maximum(u, v), 0) - rnd


# ---------------------------------------------------------------- a figure: shapes back to front, then marks

class Shape:
    __slots__ = ('d', 'mat', 'part', 'bev', 'grp', 'dark', 'org', 'flat', 'drip', 'tone', 'layer')  # the last three: tools/figures.py


class Fig:
    def __init__(self, w, h):
        self.g = Grid(w, h)
        self.shapes, self.marks = [], []
        self.part = 'torso'   # the label new shapes get
        self.dark = 0         # tones darker (the far side of the body)
        self.grp = None       # shapes in one group don't contour or shade each other

    def add(self, d, mat, bev=2.0, org=(0, 0), grp=None, flat=False, dark=None):
        s = Shape()
        s.d, s.mat, s.part, s.bev, s.flat = d, mat, self.part, bev, flat
        s.grp = grp if grp is not None else (self.grp if self.grp is not None else len(self.shapes))
        s.dark = self.dark if dark is None else dark
        s.org = org
        self.shapes.append(s)
        return s

    # shorthands; `org` anchors textures to the part so they move with it
    def circle(self, c, r, mat, bev=None, **k): return self.add(self.g.circle(c, r), mat, bev or r, org=c, **k)
    def ell(self, c, rx, ry, mat, rot=0.0, bev=None, **k): return self.add(self.g.ellipse(c, rx, ry, rot), mat, bev or min(rx, ry), org=c, **k)
    def cap(self, a, b, r0, r1, mat, bev=None, **k): return self.add(self.g.cap(a, b, r0, r1), mat, bev or max(r0, r1), org=a, **k)
    def poly(self, pts, mat, bev=2.0, **k): return self.add(self.g.poly(pts), mat, bev, org=pts[0], **k)
    def box(self, c, hw, hh, mat, rot=0.0, rnd=0.5, bev=None, **k): return self.add(self.g.box(c, hw, hh, rot, rnd), mat, bev or min(hw, hh), org=c, **k)

    def dot(self, p, col, part=None): self.marks.append((int(math.floor(p[0])), int(math.floor(p[1])), col, part or self.part)); return self

    def line(self, a, b, col, part=None):
        n = int(max(abs(b[0] - a[0]), abs(b[1] - a[1])) * 2) + 1
        seen = set()
        for i in range(n + 1):
            t = i / n
            q = (math.floor(a[0] + (b[0] - a[0]) * t), math.floor(a[1] + (b[1] - a[1]) * t))
            if q not in seen: seen.add(q); self.marks.append((q[0], q[1], col, part or self.part))
        return self

    def render(self, only=None, outline=False):
        """-> (rgb[h][w] or None, part[h][w] or None). `only`: a set of part labels to keep."""
        g = self.g
        H, W = g.h, g.w
        own = np.full((H, W), -1)
        for k, s in enumerate(self.shapes):
            if only and s.part not in only: continue
            own[s.d < 0] = k
        tone = np.zeros((H, W))
        for k, s in enumerate(self.shapes):
            m = own == k
            if not m.any(): continue
            gy, gx = np.gradient(s.d)
            gl = np.hypot(gx, gy) + 1e-6
            e = 1 - np.clip(-s.d / max(0.4, s.bev), 0, 1)  # 0 deep inside, 1 at the edge
            e = e * e
            nz = np.sqrt(np.clip(1 - e * e, 0, 1))
            l = (gx / gl * e) * LIGHT[0] + (gy / gl * e) * LIGHT[1] + nz * LIGHT[2]
            t = 2 + (l - LIGHT[2]) * 4.2
            if s.flat: t = 2 + (l - LIGHT[2]) * 1.6
            if s.mat.tex:
                lx, ly = np.floor(g.X - s.org[0]).astype(int), np.floor(g.Y - s.org[1]).astype(int)
                t = t + s.mat.tex(lx, ly)
            tone[m] = t[m] - s.dark
        grp = np.array([s.grp for s in self.shapes] + [-1])
        G = grp[own]
        out = [[None] * W for _ in range(H)]
        part = [[None] * W for _ in range(H)]

        def at(i, j): return own[j][i] if 0 <= i < W and 0 <= j < H else -1
        for j in range(H):
            for i in range(W):
                k = own[j][i]
                if k < 0: continue
                s = self.shapes[k]
                t = tone[j][i]
                # a part lying in front casts a shadow down and right; where its edge passes over this, a contour
                for a, b in ((-1, -1), (-1, 0), (0, -1)):
                    q = at(i + a, j + b)
                    if q > k and grp[q] != s.grp: t -= 1.0; break
                for a, b in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    q = at(i + a, j + b)
                    if q > k and grp[q] != s.grp: t = min(t, 1.0); break
                if at(i + 1, j) < 0 or at(i, j + 1) < 0: t -= 0.6  # the silhouette's shadowed edge
                ti = int(max(0 if t <= 0.2 else 1, min(4, round(t))))
                if s.mat.glow: ti = max(ti, 2)
                out[j][i] = (s.mat.r[ti], s.mat.tag)
                part[j][i] = s.part
        for x, y, c, p in self.marks:
            if only and p not in only: continue
            if 0 <= x < W and 0 <= y < H: out[y][x] = (c, 0); part[y][x] = p
        if outline:  # a dark line round the silhouette, tinted by what it wraps
            src = [row[:] for row in out]
            for j in range(H):
                for i in range(W):
                    if src[j][i] is not None: continue
                    for a, b in ((0, 1), (1, 0), (-1, 0), (0, -1)):
                        if 0 <= i + a < W and 0 <= j + b < H and src[j + b][i + a] is not None:
                            c = src[j + b][i + a][0]
                            out[j][i] = (OUTLINE, 0)
                            part[j][i] = part[j + b][i + a]
                            break
        return out, part


# ---------------------------------------------------------------- textures (tone offsets in part-local pixels)

def t_mail(x, y): return np.where((y % 2 == 0) & ((x + (y // 2)) % 2 == 0), -0.55, 0.15)
def t_cloth(x, y): return np.where(x % 4 == 0, -0.35, 0.0)
def t_quilt(x, y): return np.where((y % 4 == 0), -0.45, 0.0)
def t_fur(x, y): return np.where(((x * 7 + (y // 2) * 5) % 6) == 0, -0.7, np.where(((x * 3 + y) % 7) == 0, 0.4, 0.0))
def t_grain(x, y): return np.where(((y + x // 3) % 4) == 0, -0.5, 0.0)
def t_plate(x, y): return np.where((x % 7 == 3), 0.5, 0.0)
def t_rags(x, y): return np.where(((x * 5 + y * 3) % 11) < 1, -0.8, 0.0)
def t_wrap(x, y): return np.where(((y + x // 2) % 3) == 0, -0.6, 0.1)
def t_scale(x, y): return np.where(((y % 3) == 0) | (((x + 2 * ((y // 3) % 2)) % 4) == 0) & ((y % 3) == 1), -0.6, 0.1)


# ---------------------------------------------------------------- the skeleton
# A side-on figure turned a little towards us: the near arm and leg are in front of the body, the far ones
# behind it, drawn darker. All lengths in sprite pixels (half a world unit each).

class Body:
    def __init__(self, thigh, shin, torso, ua, fa, head, sw=1.6, hw=1.2, foot=4.0, sh=0.16):
        self.th, self.sh, self.to, self.ua, self.fa, self.hd = thigh, shin, torso, ua, fa, head
        self.sw, self.hw, self.foot, self.shp = sw, hw, foot, sh


POSE0 = dict(x=0.0, y=0.0, lean=0.0, nod=0.0, tN=0.0, kN=0.1, tF=0.0, kF=0.1, aN=0.1, eN=0.3, aF=-0.1, eF=0.3,
             w=0.0, wF=None, rot=0.0, cloth=0.0, flat=None, footN=None, footF=None, shield=None)


def pose(**k):
    p = dict(POSE0)
    p.update(k)
    return p


def lerp_pose(a, b, t):
    out = {}
    for k in set(a) | set(b):
        va, vb = a.get(k, b.get(k)), b.get(k, a.get(k))
        out[k] = va + (vb - va) * t if isinstance(va, float) and isinstance(vb, float) else (vb if t >= 0.5 else va)
    return out


def ease(t): return t * t * (3 - 2 * t)


def solve(B, P, cx, gy):
    """Joint positions for pose P, feet on the ground line gy (unless the pose lifts them), centred on cx."""
    th, sh = B.th, B.sh
    def leg(t, k): return th * math.cos(t) + sh * math.cos(t - k)
    hgt = max(leg(P['tN'], P['kN']), leg(P['tF'], P['kF']))
    pel = np.array([cx + P['x'], gy - hgt - 1.0 + P['y']])
    J = {'pel': pel}
    up = dir_(math.pi - P['lean'])
    J['neck'] = pel + up * B.to
    J['chest'] = pel + up * B.to * 0.62
    J['head'] = J['neck'] + dir_(math.pi - P['lean'] * 0.5 - P['nod']) * (B.hd * 0.5 + 0.8)
    J['up'] = up
    J['lean'] = P['lean']
    sho = J['neck'] + (pel - J['neck']) * B.shp
    fwd = np.array([up[1] * -1, up[0]]) * -1  # perpendicular to the spine, pointing forward
    fwd = np.array([math.cos(P['lean']), math.sin(P['lean'])])
    J['shN'], J['shF'] = sho + fwd * B.sw, sho - fwd * B.sw * 0.6
    J['hipN'], J['hipF'] = pel + fwd * B.hw, pel - fwd * B.hw * 0.6
    for s in 'NF':
        J['kn' + s] = J['hip' + s] + dir_(P['t' + s]) * th
        J['ft' + s] = J['kn' + s] + dir_(P['t' + s] - P['k' + s]) * sh
        J['el' + s] = J['sh' + s] + dir_(P['a' + s]) * B.ua
        J['ha' + s] = J['el' + s] + dir_(P['a' + s] + P['e' + s]) * B.fa
        J['fa' + s] = P['a' + s] + P['e' + s]  # the forearm's angle
        # the foot: flat on the ground when it's down, toes trailing when it's lifted behind
        shinA = P['t' + s] - P['k' + s]
        fo = P.get('foot' + s)
        if fo is None: fo = math.pi / 2 if J['ft' + s][1] > gy - 2.5 else math.pi / 2 + shinA * 0.9 - 0.15
        J['toe' + s] = fo
    J['w'] = J['faN'] + P['w']
    J['gy'] = gy
    return J


# ---------------------------------------------------------------- shared painters

def limb(f, a, b, r0, r1, mat, bev=None):
    return f.cap(a, b, r0, r1, mat, bev=bev)


def boot(f, ankle, ang, L, mat, h=3.2, cuff=None):
    """A boot from the ankle, the toe pointing along `ang` (pi/2 = forward and level)."""
    d = dir_(ang)
    up = np.array([d[1], -d[0]])  # 90 degrees from the toe, towards the shin
    heel = ankle - d * 1.6
    toe = ankle + d * L
    pts = [heel + up * h, ankle + up * h + d * 0.8, toe + up * 1.4, toe + d * 0.6, toe - up * 0.2, heel - up * 0.2 - d * 0.3]
    f.poly([tuple(p) for p in pts], mat, bev=1.6)
    if cuff: f.cap(ankle + up * (h - 0.2) - d * 1.4, ankle + up * (h - 0.2) + d * 1.4, 1.0, 1.0, cuff, bev=1)


def hand(f, p, ang, r, mat, fist=True):
    d = dir_(ang)
    f.circle(p + d * (r * 0.4), r, mat, bev=r)
    f.circle(p + d * (r * 0.2) + np.array([r * 0.7, 0]), r * 0.45, mat, bev=r * 0.5)


def P2(v): return (float(v[0]), float(v[1]))


# ---------------------------------------------------------------- creatures

CREATURES = {}


def creature(fn):
    CREATURES[fn.__name__] = fn
    return fn


EYE = (24, 14, 22)
OUTLINE = (17, 11, 20)


# ---- animation sets for a humanoid with a weapon
def walk_cycle(n=8, stride=0.62, knee=1.05, bob=1.0, arm=0.55, lean=0.08, extra=None):
    out = []
    for i in range(n):
        t = i / n * 2 * math.pi
        sw = math.sin(t)
        kN = 0.15 + knee * max(0.0, math.cos(t)) ** 1.5
        kF = 0.15 + knee * max(0.0, -math.cos(t)) ** 1.5
        p = pose(tN=stride * sw, kN=kN, tF=-stride * sw, kF=kF, y=-abs(math.cos(t)) * bob + bob * 0.5, lean=lean,
                 aN=-arm * sw, eN=0.35 + 0.25 * max(0, -sw), aF=arm * sw, eF=0.35 + 0.25 * max(0, sw), cloth=sw)
        if extra: extra(p, t)
        out.append(p)
    return out


def idle_cycle(n=6, base=None, breathe=0.6):
    base = base or pose()
    out = []
    for i in range(n):
        t = i / n * 2 * math.pi
        p = dict(base)
        p['y'] = base['y'] + (0.5 - 0.5 * math.cos(t)) * breathe
        p['aN'] = base['aN'] + 0.04 * math.sin(t)
        p['aF'] = base['aF'] - 0.04 * math.sin(t)
        p['nod'] = base['nod'] + 0.03 * math.sin(t)
        p['cloth'] = 0.25 * math.sin(t)
        out.append(p)
    return out


def keys(frames, n):
    """Tween a list of (pose, weight) keyframes into n frames."""
    out = []
    for i in range(n):
        u = i / (n - 1) * (len(frames) - 1)
        a = int(min(u, len(frames) - 2))
        out.append(lerp_pose(frames[a], frames[a + 1], ease(u - a)))
    return out


def local(J, o, a, b):
    """A point `a` pixels forward and `b` down the body from `o` (following the lean)."""
    fwd = np.array([math.cos(J['lean']), math.sin(J['lean'])])
    return o + fwd * a - J['up'] * b


def hair_cut(g, H, fx, fy):
    """Where hair may grow on a head at H: above the brow or behind the ear."""
    return np.minimum(g.Y - (H[1] + fy), g.X - (H[0] + fx))


# ---- the hero
HERO = Body(thigh=8.4, shin=8.2, torso=12.0, ua=6.8, fa=6.4, head=11.5, sw=3.0, hw=1.6, foot=3.6)


def hero_look(kind='wool'):
    """The five armour sets over one outfit: linen shirt, vest, belt, dark breeches, turned-down boots. Big flat areas
    in clearly different values, so arms, body and legs read apart at a glance. Tag 1 colours are painted grey and
    take the armour metal's colour in game; tag 2, its rune glow."""
    METAL = lambda tex=None: Mat('#9c9c9c', tex=tex, tag=1)
    L = dict(skin=Mat('#e8a882'), hair=Mat('#9a4a24'), shirt=Mat('#c9c6c4'), vest=Mat('#7a4e30'), belt=Mat('#3e2a20'),
             buckle=Mat('#c8ccd4'), trouser=Mat('#4a4656'), boot=Mat('#5e3c26'), cuff=Mat('#8a603e'), eyes=(28, 34, 52),
             helm=None, sleeve=None, body='vest')
    if kind == 'leather':
        L.update(vest=Mat('#6a4a36', tex=t_quilt), buckle=METAL(), body='jerkin')
    elif kind == 'mail':
        L.update(vest=METAL(t_mail), body='byrnie', helm='nasal', buckle=Mat('#c8a050'))
    elif kind == 'lamellar':
        L.update(vest=METAL(t_scale), body='byrnie', helm='aventail', buckle=Mat('#ffffff', tag=2), trouser=Mat('#3a3640'))
    elif kind == 'scale':
        L.update(vest=METAL(t_scale), body='byrnie', helm='great', buckle=Mat('#d6b450'), trouser=Mat('#2e2a34'), cuff=METAL())
    L['sleeve'] = L['shirt']
    L['metal'] = METAL()
    return L


@creature
def hero(fig, J, P, L=None):
    L = L or hero_look()
    g = fig.g
    H = J['head']
    n, p = J['neck'], J['pel']

    def arm(s):
        fig.part = 'uarm' + s
        limb(fig, J['sh' + s], J['el' + s], 2.8, 2.4, L['sleeve'])
        fig.part = 'farm' + s
        limb(fig, J['el' + s], J['ha' + s] - (J['ha' + s] - J['el' + s]) * 0.15, 2.4, 2.2, L['sleeve'])
        hand(fig, J['ha' + s], J['fa' + s], 2.1, L['skin'])

    def leg(s):
        fig.part = 'thigh' + s
        limb(fig, J['hip' + s], J['kn' + s], 3.3, 2.7, L['trouser'])
        fig.part = 'shin' + s
        limb(fig, J['kn' + s], J['ft' + s] + (J['kn' + s] - J['ft' + s]) * 0.4, 2.6, 2.3, L['trouser'])
        ank, up = J['ft' + s], J['kn' + s] - J['ft' + s]
        up = up / (np.linalg.norm(up) + 1e-9)
        fig.cap(ank + up * 1.5, ank + up * 5.2, 2.5, 2.7, L['boot'], bev=2)  # the boot's leg
        boot(fig, ank, J['toe' + s], 4.6, L['boot'], h=3.4)
        fig.cap(ank + up * 5.6 - np.array([up[1], -up[0]]) * 2.6, ank + up * 5.6 + np.array([up[1], -up[0]]) * 2.6, 1.3, 1.3, L['cuff'], bev=1.2)

    # the far side, a tone darker
    fig.dark = 1
    arm('F')
    leg('F')
    fig.dark = 0
    leg('N')
    # the body: shirt, then the vest over it, open at the neck; the belt and its buckle
    fig.part = 'torso'
    chest = [P2(local(J, n, 3.2, -0.4)), P2(local(J, n, 6.4, 3.2)), P2(local(J, p, 5.4, -3.6)), P2(local(J, p, 4.8, 1.6)),
             P2(local(J, p, -4.6, 1.6)), P2(local(J, p, -5.0, -4)), P2(local(J, n, -5.4, 3.0)), P2(local(J, n, -3.0, -0.6))]
    fig.poly(chest, L['shirt'], bev=4.0)
    long = L['body'] != 'vest'
    hem = 4.6 if long else 2.6
    fig.grp = 'vest'
    fig.poly([P2(local(J, n, 1.8, 1.6)), P2(local(J, n, 4.0, 4.6)), P2(local(J, p, 5.6, -3.2)), P2(local(J, p, 5.2, hem)),
              P2(local(J, p, -4.8, hem)), P2(local(J, p, -5.2, -4)), P2(local(J, n, -5.6, 3.0)), P2(local(J, n, -3.2, -0.4)), P2(local(J, n, 0.4, 0.2))],
             L['vest'], bev=3.4)
    fig.grp = None
    fig.cap(local(J, p, -5.0, -0.6), local(J, p, 5.4, -0.6), 1.3, 1.3, L['belt'], bev=1.0)
    fig.box(P2(local(J, p, 3.8, -0.6)), 1.3, 1.4, L['buckle'], bev=1.0)
    # the head: a clear face, eyes and brows you can read, a full but tidy beard, hair falling to the collar
    fig.part = 'head'
    fig.cap(local(J, n, 0.6, 1), H + np.array([0.4, 3.6]), 2.3, 2.3, L['skin'])
    fig.ell(H + np.array([-2.6, 1.6]), 3.6, 5.4, L['hair'], bev=2.4)  # the hair at the back, to the collar
    fig.ell(H + np.array([0.9, 0.4]), 4.8, 5.4, L['skin'], bev=2.8)
    fig.circle(H + np.array([5.5, 1.2]), 1.0, L['skin'], bev=1)  # nose
    fig.add(np.maximum(g.ellipse(H + np.array([-0.2, -1.0]), 5.6, 5.0), hair_cut(g, H, -0.6, -2.0)), L['hair'], bev=2.6, org=P2(H))
    fig.add(np.maximum(np.maximum(g.ellipse(H + np.array([2.4, 3.8]), 3.8, 3.2), -(g.Y - (H[1] + 2.4))), -(g.X - (H[0] - 1.0))),
            L['hair'], bev=2.0, org=P2(H))  # beard
    fig.line(H + np.array([2.0, -1.2]), H + np.array([3.2, -1.2]), L['hair'].r[1])  # brows
    fig.dot(H + np.array([4.6, -1.2]), L['hair'].r[1])
    fig.dot(H + np.array([2.4, -0.2]), (236, 232, 226)); fig.dot(H + np.array([3.4, -0.2]), L['eyes'])  # the near eye
    fig.dot(H + np.array([4.8, -0.2]), L['eyes'])
    fig.line(H + np.array([3.8, 2.6]), H + np.array([5.4, 2.6]), L['hair'].r[1])  # moustache
    fig.dot(H + np.array([4.8, 3.6]), L['skin'].r[1])  # mouth
    fig.dot(H + np.array([-0.2, 1.0]), L['skin'].r[1])  # ear
    if L['helm']:
        M = L['metal']
        fig.add(np.maximum(g.ellipse(H + np.array([0.0, -0.8]), 5.8, 5.8), g.Y - (H[1] - 0.8)), M, bev=3.2, org=P2(H))  # the bowl
        fig.cap(H + np.array([-5.6, -0.8]), H + np.array([5.6, -0.8]), 1.0, 1.0, M, bev=1)
        if L['helm'] == 'nasal': fig.cap(H + np.array([4.8, -0.8]), H + np.array([5.0, 2.0]), 0.8, 0.7, M, bev=0.8)
        if L['helm'] == 'aventail':  # mail hanging from the helm round the back of the neck, a rune on the brow
            fig.add(np.maximum(np.maximum(g.ellipse(H + np.array([-1.2, 1.6]), 5.0, 5.2), -(g.Y - H[1])), g.X - (H[0] + 1.0)),
                    Mat('#9c9c9c', tex=t_mail, tag=1), bev=2, org=P2(H))
            fig.box(P2(H + np.array([0.6, -3.0])), 0.6, 1.2, Mat('#ffffff', tag=2, glow=True), bev=0.6)
        if L['helm'] == 'great':
            fig.add(np.maximum(g.ellipse(H + np.array([0.6, 0.6]), 5.6, 6.0), -(g.Y - (H[1] - 1.0))), M, bev=3, org=P2(H))
            fig.line(H + np.array([2.4, 0.2]), H + np.array([6.0, 0.2]), (30, 24, 34))  # the eye slit
            fig.cap(H + np.array([0.6, -6.6]), H + np.array([0.6, 4.0]), 0.7, 0.7, L['buckle'], bev=0.7)  # a gilt crest
    if not P.get('noArmN'): arm('N')


def hero_anims():
    A = {}
    stand = pose(tN=0.12, kN=0.12, tF=-0.1, kF=0.1, aN=0.05, eN=0.2, aF=-0.08, eF=0.25)
    A['idle'] = idle_cycle(6, stand, 0.7)
    A['walk'] = walk_cycle(8, stride=0.78, knee=1.5, bob=1.6, arm=0.85, lean=0.2)
    A['jump'] = [pose(tN=1.0, kN=1.6, tF=-0.3, kF=0.6, aN=-0.9, eN=0.6, aF=1.0, eF=0.6, lean=0.1, y=-2.0)]
    A['fall'] = [pose(tN=0.45, kN=0.4, tF=-0.5, kF=0.3, aN=-2.2, eN=0.3, aF=-1.8, eF=0.3, y=-2.0, cloth=-1.0)]
    crouch = pose(tN=1.3, kN=2.1, tF=0.9, kF=2.0, lean=0.35, aN=0.6, eN=0.6, aF=0.4, eF=0.6)
    A['crouch'] = idle_cycle(4, crouch, 0.4)
    Q = lambda n: [i / n * 2 * math.pi for i in range(n)]
    A['crouchwalk'] = [dict(crouch, tN=1.3 + 0.35 * math.sin(q), tF=0.9 - 0.35 * math.sin(q), aN=0.6 - 0.3 * math.sin(q),
                            y=-0.5 * abs(math.sin(q))) for q in Q(6)]
    A['roll'] = [pose(tN=1.6, kN=2.5, tF=1.4, kF=2.4, lean=0.9, aN=1.4, eN=1.4, aF=1.3, eF=1.4, rot=q, ty=-10.0) for q in Q(6)]
    # Dead Cells style: a hard dip into the tuck, a fast tight spin, a skid back up. The rig's legs are solved from a
    # pelvis height so the lunges below keep both feet on the ground.
    def st(H, tN, tF, **k):
        th, sh = HERO.th, HERO.sh
        def knee(t):
            v = max(-1.0, min(1.0, (H - th * math.cos(t)) / sh))
            return t + math.acos(v)
        return pose(tN=tN, kN=knee(tN), tF=tF, kF=knee(tF), **k)
    tuck = dict(aN=1.4, eN=1.4, aF=1.3, eF=1.4)
    A['rollin'] = [st(11.0, 0.9, 0.5, lean=0.75, x=1.0, **tuck), pose(tN=1.5, kN=2.4, tF=1.3, kF=2.3, lean=0.85, rot=0.0, ty=-8.0, **tuck)]
    A['rollout'] = [st(9.5, 0.9, -0.5, lean=0.6, x=1.5, aN=0.9, eN=0.6, aF=-0.9, eF=0.4, cloth=1.0), st(13.5, 0.4, -0.1, lean=0.25, x=0.5, aN=0.3, eN=0.4, aF=-0.3, eF=0.3)]
    # the swing: coil, launch, the committed lunge, follow-through, settle. rig.cpp spends the first two on the wind-up,
    # the third on the blow and the last two on the recovery. The near arm is its own layer, so only the body moves here.
    A['slash'] = [st(14.5, 0.2, -0.25, lean=-0.2, x=-1.5, aF=0.5, eF=0.5, nod=-0.05),
                  st(12.0, 0.5, -0.4, lean=0.15, x=0.5, aF=-0.3, eF=0.4),
                  st(11.5, 0.85, -0.65, lean=0.5, x=3.0, aF=-1.0, eF=0.3, nod=0.1, cloth=1.0),
                  st(12.0, 0.8, -0.6, lean=0.45, x=3.0, aF=-1.2, eF=0.3, cloth=-0.6),
                  st(13.5, 0.4, -0.2, lean=0.2, x=1.5, aF=-0.3, eF=0.3)]
    A['chop'] = [st(16.0, 0.1, -0.1, lean=-0.3, x=-0.5, y=-0.5, aF=1.8, eF=0.5, nod=-0.1),
                 st(14.5, 0.25, -0.35, lean=-0.5, x=-2.0, aF=2.0, eF=0.4, nod=-0.15),
                 st(12.5, 0.7, -0.45, lean=0.9, x=3.5, aF=-0.6, eF=0.5, nod=0.2, cloth=1.0),
                 st(12.0, 0.7, -0.45, lean=0.95, x=4.0, aF=-0.7, eF=0.5, nod=0.2, cloth=-0.5),
                 st(13.0, 0.4, -0.2, lean=0.4, x=2.0, aF=-0.2, eF=0.3)]
    A['thrust'] = [st(14.0, 0.15, -0.5, lean=-0.12, x=-2.5, aF=0.6, eF=0.5),
                   st(13.0, 0.3, -0.5, lean=-0.05, x=-1.5, aF=0.3, eF=0.4),
                   st(10.5, 1.0, -0.8, lean=0.55, x=5.0, aF=-1.1, eF=0.3, nod=0.1, cloth=1.0),
                   st(10.8, 0.95, -0.75, lean=0.5, x=4.5, aF=-1.1, eF=0.3, cloth=-0.5),
                   st(13.0, 0.4, -0.2, lean=0.2, x=2.0, aF=-0.3, eF=0.3)]
    # prone: the whole figure laid along the ground, head up, crawling on its elbows
    A['crawl'] = [pose(tN=0.1 + 0.25 * math.sin(q), kN=0.2, tF=-0.1 - 0.25 * math.sin(q), kF=0.3, aN=1.9 + 0.5 * math.sin(q), eN=-1.6,
                       aF=1.9 - 0.5 * math.sin(q), eF=-1.6, nod=-0.9, rot=math.pi / 2, ty=-8.0) for q in Q(6)]
    A['climb'] = [pose(tN=0.9 + 0.4 * math.sin(q), kN=1.6, tF=0.3 - 0.4 * math.sin(q), kF=1.2, aN=2.7 + 0.35 * math.sin(q), eN=0.3,
                       aF=2.5 - 0.35 * math.sin(q), eF=0.3, lean=-0.05) for q in Q(4)]
    A['wall'] = [pose(tN=0.9, kN=1.4, tF=0.4, kF=1.0, aN=2.6, eN=0.3, aF=1.6, eF=0.8, lean=0.15)]
    # swimming: the whole body turned nearly flat, a crawl stroke (the arms sweeping round in turn) over a flutter kick;
    # treading water, when you're hanging in it, upright with the arms sculling and the legs pedalling
    A['swim'] = [pose(tN=0.25 + 0.3 * math.sin(2 * q), kN=0.35 + 0.3 * abs(math.sin(2 * q)), tF=0.25 - 0.3 * math.sin(2 * q), kF=0.35 + 0.3 * abs(math.cos(2 * q)),
                      aN=2.3 + 1.5 * math.sin(q), eN=0.4 + 0.5 * max(0.0, math.cos(q)), aF=2.3 - 1.5 * math.sin(q), eF=0.4 + 0.5 * max(0.0, -math.cos(q)),
                      nod=-0.35, rot=1.25, ty=-10.0, cloth=math.sin(q)) for q in Q(8)]
    A['tread'] = [pose(tN=0.5 + 0.35 * math.sin(q), kN=1.0 + 0.4 * math.cos(q), tF=0.5 - 0.35 * math.sin(q), kF=1.0 - 0.4 * math.cos(q),
                       aN=1.0 + 0.6 * math.sin(q), eN=0.9, aF=1.0 - 0.6 * math.sin(q), eF=0.9, lean=0.1, y=0.6 * math.sin(q * 0.5), cloth=math.sin(q)) for q in Q(8)]
    A['hang'] = [pose(tN=0.35, kN=0.3, tF=-0.3, kF=0.2, aN=2.9, eN=0.1, aF=2.7, eF=0.2)]
    return A


# ---------------------------------------------------------------- weapons and gear (absolute angles: 0 down, pi/2 forward, pi up)

STEEL = Mat('#9aa2b0', shine=1)
RUSTY = Mat('#8a6a52')
ASH = Mat('#8c6a42', tex=t_grain)
BONEM = Mat('#d8ceb0')
GOLDM = Mat('#d0a046')


def spear(f, hand, ang, fore=30, back=10, shaft=ASH, head=STEEL):
    d = dir_(ang)
    tip = hand + d * fore
    f.cap(hand - d * back, tip, 0.9, 0.9, shaft, bev=0.9)
    side = np.array([d[1], -d[0]])
    f.poly([P2(tip - d * 0.5 + side * 1.6), P2(tip + d * 6.5), P2(tip - d * 0.5 - side * 1.6), P2(tip - d * 2)], head, bev=1.4)
    f.cap(tip - d * 1.5, tip - d * 0.2, 1.2, 1.2, head, bev=1)


def sword(f, hand, ang, L=15, blade=STEEL, hilt=Mat('#5a3a22'), guard=GOLDM, w=1.3):
    d = dir_(ang)
    side = np.array([d[1], -d[0]])
    f.cap(hand - d * 2.4, hand + d * 1.0, 0.8, 0.8, hilt, bev=0.8)
    f.circle(hand - d * 3.0, 1.0, guard)
    f.poly([P2(hand + d * 1.8 + side * w), P2(hand + d * (L - 2) + side * w * 0.9), P2(hand + d * L), P2(hand + d * (L - 2) - side * w * 0.9),
            P2(hand + d * 1.8 - side * w)], blade, bev=1.2)
    f.cap(hand + d * 1.4 + side * 2.6, hand + d * 1.4 - side * 2.6, 0.8, 0.8, guard, bev=0.8)


def smear(f, centre, r0, r1, a_from, a_to, col):
    """The blur of a fast blow: a crescent along the arc it swept, solid at the leading edge, thinning and
    breaking up towards the tail. Angles as dir_, measured in [-0.5, 2pi - 0.5) so a swing over the top is one range."""
    g = f.g
    dx, dy = g.X - centre[0], g.Y - centre[1]
    rr = np.hypot(dx, dy)
    a = np.arctan2(dx, dy)
    a = np.where(a < -0.5, a + 2 * math.pi, a)
    t = (a - a_from) / (a_to - a_from)
    inside = (t > 0) & (t < 1) & (rr < r1) & (rr > r1 - (r1 - r0) * t)
    for j, i in zip(*np.nonzero(inside)):
        if t[j, i] < 0.55 and (i + j) % 2: continue
        f.marks.append((int(i), int(j), col, 'fx'))


def streak(f, a, b, col, offs=(-2, 2)):
    """Speed lines alongside a thrust."""
    for o in offs:
        d = (b - a) / (np.linalg.norm(b - a) + 1e-9)
        side = np.array([d[1], -d[0]]) * o
        n = int(np.linalg.norm(b - a))
        for k in range(n):
            if k % 3 == 2 or k < n * 0.3 and k % 2: continue
            q = a + d * k + side
            f.marks.append((int(q[0]), int(q[1]), col, 'fx'))


# ---- the castle guard: kettle helm over a mail coif, quilted gambeson under a Dunmoor tabard, spear and shield
GUARD = Body(thigh=8.8, shin=8.6, torso=12.6, ua=7.0, fa=6.6, head=10.5, sw=1.8, hw=1.4)


@creature
def guard(fig, J, P, L=None):
    g = fig.g
    H, n, p = J['head'], J['neck'], J['pel']
    gamb, tab, trim, mail, skin = Mat('#b49a68', tex=t_quilt), Mat('#9c2c26'), Mat('#e8dcc0'), Mat('#8a8e98', tex=t_mail), Mat('#d89c78')
    trou, boot_, leather = Mat('#4c4448'), Mat('#4a3022'), Mat('#6a4428')
    aw = P.get('wa', J['w'])
    fig.dark = 1
    fig.part = 'uarm' + 'F'
    limb(fig, J['shF'], J['elF'], 2.7, 2.3, gamb)
    fig.part = 'farm' + 'F'
    limb(fig, J['elF'], J['haF'], 2.3, 2.0, leather)
    fig.part = 'thigh' + 'F'
    limb(fig, J['hipF'], J['knF'], 3.1, 2.6, trou)
    fig.part = 'shin' + 'F'
    limb(fig, J['knF'], J['ftF'], 2.5, 2.0, trou)
    boot(fig, J['ftF'], J['toeF'], 4.2, boot_, h=4.4)
    fig.dark = 0
    fig.part = 'thigh' + 'N'
    limb(fig, J['hipN'], J['knN'], 3.2, 2.7, trou)
    fig.part = 'shin' + 'N'
    limb(fig, J['knN'], J['ftN'], 2.6, 2.1, trou)
    boot(fig, J['ftN'], J['toeN'], 4.2, boot_, h=4.4)
    fig.part = 'torso'
    kf = max(J['knN'][0], J['knF'][0]); kb = min(J['knN'][0], J['knF'][0])
    hem = max(J['knN'][1], J['knF'][1]) - 0.5
    sway = P['cloth'] * 0.6
    fig.poly([P2(local(J, n, 3.2, -0.6)), P2(local(J, n, 5.6, 3.0)), P2(local(J, p, 5.0, -3.0)), (max(kf + 2.4, p[0] + 4.8) + sway, hem),
              (min(kb - 2.4, p[0] - 4.6) + sway, hem), P2(local(J, p, -4.6, -3)), P2(local(J, n, -4.8, 3.0)), P2(local(J, n, -2.6, -0.8))],
             gamb, bev=4.0)
    # the tabard: a red panel down the front with a pale cross
    fig.grp = 'tab'
    tb = [P2(local(J, n, 0.6, 0.6)), P2(local(J, n, 5.2, 2.6)), P2(local(J, p, 4.8, -3.0)), (max(kf + 1.4, p[0] + 4.0) + sway, hem - 1.6),
          (p[0] - 1.2 + sway, hem - 1.2), P2(local(J, p, -1.2, -2))]
    fig.poly(tb, tab, bev=2.6)
    c = local(J, n, 2.8, 5.6)
    fig.box(P2(c), 0.7, 2.8, trim, rot=-J['lean'], bev=0.7)
    fig.box(P2(c + J['up'] * 0.8), 2.2, 0.7, trim, rot=-J['lean'], bev=0.7)
    fig.grp = None
    fig.cap(local(J, p, -4.6, -1.6), local(J, p, 5.0, -1.6), 1.2, 1.2, leather, bev=1.1)
    fig.box(P2(local(J, p, 3.6, -1.6)), 1.0, 1.2, STEEL, bev=1)
    # the head: mail coif round the face, a broad-brimmed kettle helm
    fig.part = 'head'
    fig.ell(P2(H + np.array([0.2, 1.6])), 5.4, 6.4, mail, bev=3)
    fig.cap(P2(local(J, n, -4.2, 0.6)), P2(local(J, n, 4.4, 0.8)), 2.0, 2.0, mail, bev=2)
    fig.ell(P2(H + np.array([2.2, 1.2])), 3.4, 3.8, skin, bev=2)
    fig.ell(P2(H + np.array([0.4, -2.0])), 5.0, 4.0, STEEL, bev=3.2)
    fig.add(np.maximum(g.ellipse(H + np.array([0.4, -1.4]), 8.2, 2.0), -(g.Y - (H[1] - 2.2))), STEEL, bev=1.4, org=P2(H))  # the brim
    fig.dot(H + np.array([3.2, 1.0]), EYE); fig.dot(H + np.array([5.0, 1.0]), EYE)
    fig.dot(H + np.array([5.4, 2.6]), skin.r[1])
    fig.line(H + np.array([2.6, 3.8]), H + np.array([4.6, 3.8]), skin.r[0])
    # the shield on the far arm, held across the body
    fig.part = 'shield'
    sc = J['haF'] + np.array([1.6, -2.0])
    sh_d = g.poly([(sc[0] - 4.4, sc[1] - 6), (sc[0] + 4.4, sc[1] - 6), (sc[0] + 4.2, sc[1] + 1), (sc[0], sc[1] + 8.5), (sc[0] - 4.2, sc[1] + 1)])
    fig.add(sh_d, Mat('#e0d6bc'), bev=3, org=P2(sc))
    fig.grp = 'emb'
    fig.box(P2(sc + np.array([0, 0.4])), 0.9, 5.4, tab, bev=0.8)
    fig.box(P2(sc + np.array([0, -1.6])), 3.4, 0.9, tab, bev=0.8)
    fig.grp = None
    fig.add(np.abs(sh_d + 0.5) - 0.55, STEEL, bev=0.6, org=P2(sc))  # the rim
    # the near arm, the spear
    fig.part = 'weapon'
    spear(fig, J['haN'], aw)
    fig.part = 'uarm' + 'N'
    limb(fig, J['shN'], J['elN'], 2.9, 2.4, gamb)
    fig.part = 'farm' + 'N'
    limb(fig, J['elN'], J['haN'], 2.4, 2.1, leather)
    hand(fig, J['haN'], J['faN'], 2.0, skin)
    if P.get('fx'): streak(fig, J['haN'] - dir_(aw) * 6, J['haN'] + dir_(aw) * 22, (236, 232, 220))


def guard_anims():
    A = {}
    stand = pose(tN=0.12, kN=0.12, tF=-0.12, kF=0.1, aN=0.35, eN=-0.9, aF=0.6, eF=1.2, wa=math.pi - 0.08)
    A['idle'] = idle_cycle(6, stand, 0.6)
    def wk(p, t): p.update(aN=0.4 - 0.15 * math.sin(t), eN=-0.9, aF=0.6, eF=1.2, wa=math.pi - 0.45)
    A['walk'] = walk_cycle(8, stride=0.55, knee=1.0, bob=1.0, arm=0.3, lean=0.06, extra=wk)
    guard_ = pose(tN=0.45, kN=0.5, tF=-0.35, kF=0.25, aN=0.2, eN=1.5, aF=0.8, eF=1.0, wa=math.pi / 2 - 0.05, lean=0.05)
    drawn = pose(tN=0.5, kN=0.6, tF=-0.45, kF=0.3, aN=-0.8, eN=2.2, aF=0.9, eF=1.0, wa=math.pi / 2 + 0.05, lean=-0.15, x=-2.0)
    thrust = pose(tN=0.85, kN=0.7, tF=-0.6, kF=0.2, aN=1.45, eN=0.1, aF=0.6, eF=1.0, wa=math.pi / 2 - 0.02, lean=0.32, x=3.0, cloth=1.0)
    A['windup'] = keys([guard_, drawn], 3)
    A['strike'] = [dict(thrust, fx=1), thrust]
    A['recover'] = keys([thrust, guard_, stand], 3)
    hit = pose(tN=-0.1, kN=0.4, tF=-0.5, kF=0.3, aN=0.2, eN=-0.6, aF=0.9, eF=1.2, wa=math.pi - 0.6, lean=-0.35, nod=-0.3, x=-1.5)
    A['hurt'] = [hit, lerp_pose(hit, stand, 0.5)]
    return A


# ---- the skeleton: bare bones, a rusted sword and a cracked round shield
SKEL = Body(thigh=8.6, shin=8.6, torso=11.8, ua=6.8, fa=6.6, head=10, sw=1.6, hw=1.2)


@creature
def skeleton(fig, J, P, L=None):
    g = fig.g
    H, n, p = J['head'], J['neck'], J['pel']
    bone, rag, wood = BONEM, Mat('#5a4c3c', tex=t_rags), Mat('#6e4a2c', tex=t_grain)
    aw = P.get('wa', J['w'])

    def arm(s):
        fig.part = 'uarm' + s
        limb(fig, J['sh' + s], J['el' + s], 1.3, 1.1, bone)
        fig.circle(J['el' + s], 1.4, bone)
        fig.part = 'farm' + s
        limb(fig, J['el' + s], J['ha' + s], 1.1, 0.9, bone)
        fig.part = 'uarm' + s
        fig.circle(J['sh' + s], 1.9, bone)
        fig.part = 'farm' + s
        hand(fig, J['ha' + s], J['fa' + s], 1.3, bone)

    def leg(s):
        fig.part = 'thigh' + s
        limb(fig, J['hip' + s], J['kn' + s], 1.5, 1.2, bone)
        fig.circle(J['kn' + s], 1.6, bone)
        fig.part = 'shin' + s
        limb(fig, J['kn' + s], J['ft' + s], 1.2, 1.0, bone)
        boot(fig, J['ft' + s], J['toe' + s], 3.8, bone, h=1.8)

    fig.dark = 1
    arm('F')
    leg('F')
    fig.dark = 0
    leg('N')
    fig.part = 'torso'
    fig.cap(P2(n), P2(p), 1.1, 1.1, bone)  # the spine
    for k in range(4):  # ribs curling forward
        a = local(J, n, -2.4, 2.0 + k * 1.7)
        b = local(J, n, 3.6 - k * 0.3, 3.4 + k * 1.8)
        fig.cap(P2(a), P2(b), 0.75, 0.6, bone, bev=0.8, grp='ribs')
    fig.ell(P2(local(J, p, 0.4, 0)), 3.6, 2.2, bone, rot=-J['lean'], bev=1.6)  # the pelvis
    kf = max(J['knN'][0], J['knF'][0])
    fig.poly([P2(local(J, p, -3.6, -1)), P2(local(J, p, 3.8, -1)), (kf + 1.0, J['knN'][1] - 3), (p[0] - 0.5, J['knN'][1] - 1), (p[0] - 3, J['knN'][1] - 4)], rag, bev=1.6)
    fig.cap(P2(local(J, n, -3.0, 0.6)), P2(local(J, n, 3.6, 0.8)), 1.0, 1.0, bone)  # the collarbone
    fig.part = 'head'
    fig.cap(P2(n), P2(H + np.array([0, 3])), 0.9, 0.9, bone)
    fig.ell(P2(H + np.array([0.2, -0.6])), 4.6, 4.4, bone, bev=3.0)
    fig.box(P2(H + np.array([2.4, 3.4])), 2.6, 1.6, bone, rnd=0.8, bev=1.4)  # the jaw
    fig.ell(P2(H + np.array([2.6, 0.2])), 1.4, 1.5, Mat('#2a1c22'), bev=0.5)  # the sockets
    fig.ell(P2(H + np.array([5.0, 0.2])), 0.9, 1.4, Mat('#2a1c22'), bev=0.5)
    fig.dot(H + np.array([2.6, 0.2]), (255, 96, 60))
    fig.dot(H + np.array([5.4, 2.0]), (40, 26, 32))
    for k in range(4): fig.dot(H + np.array([1.4 + k * 1.1, 2.6 + (k % 2) * 0.0]), (60, 44, 46))  # the teeth
    fig.part = 'shield'
    sc = J['haF'] + np.array([2.4, -1.0])
    sd = g.circle(sc, 5.6)
    fig.add(sd, wood, bev=3, org=P2(sc))
    fig.add(np.abs(sd + 0.5) - 0.6, RUSTY, bev=0.6, org=P2(sc))
    fig.circle(P2(sc), 1.5, RUSTY)
    fig.line(sc + np.array([-1.2, -5]), sc + np.array([-0.4, -1.6]), (40, 26, 20))
    fig.part = 'weapon'
    sword(fig, J['haN'], aw, L=15, blade=RUSTY, guard=RUSTY)
    arm('N')
    if P.get('fx'): smear(fig, J['shN'], 11, 22, P['fx'][0], P['fx'][1], (236, 226, 200))


def skeleton_anims():
    A = {}
    stand = pose(tN=0.1, kN=0.2, tF=-0.12, kF=0.15, aN=0.3, eN=0.5, aF=0.5, eF=1.1, wa=0.9, lean=0.1, nod=0.05)
    A['idle'] = idle_cycle(6, stand, 0.8)
    def wk(p, t): p.update(aF=0.5, eF=1.1, wa=1.0 + 0.1 * math.sin(t), nod=0.08 * math.sin(t * 2))
    A['walk'] = walk_cycle(8, stride=0.5, knee=0.9, bob=1.2, arm=0.45, lean=0.14, extra=wk)
    up = pose(tN=0.3, kN=0.4, tF=-0.3, kF=0.2, aN=-2.7, eN=-0.3, aF=0.7, eF=1.0, wa=math.pi + 0.6, lean=-0.18, x=-1.0)
    down = pose(tN=0.7, kN=0.6, tF=-0.5, kF=0.2, aN=1.25, eN=0.2, aF=0.5, eF=1.0, wa=1.1, lean=0.35, x=2.5, cloth=1.0)
    A['windup'] = keys([stand, up], 3)
    A['strike'] = [dict(lerp_pose(up, down, 0.55), fx=(3.6, 2.2)), dict(down, fx=(3.0, 1.2))]
    A['recover'] = keys([down, stand], 3)
    hit = pose(tN=-0.1, kN=0.4, tF=-0.4, kF=0.3, aN=-0.3, eN=0.6, aF=0.9, eF=1.0, wa=0.5, lean=-0.4, nod=-0.35, x=-1.5)
    A['hurt'] = [hit, lerp_pose(hit, stand, 0.5)]
    return A


# ================================================================ the bestiary
# Every enemy, painted fresh. Shared pieces first: limbs, bodies, gear, then the attack styles that turn a
# standing pose into wind-up / strike / recover (melee), or draw / release (ranged and casting).

def arm_(fig, J, s, up, lo, hand_m, r=(2.6, 2.2, 2.2, 1.9), hr=2.0, claws=None):
    fig.part = 'uarm' + s
    limb(fig, J['sh' + s], J['el' + s], r[0], r[1], up)
    fig.part = 'farm' + s
    limb(fig, J['el' + s], J['ha' + s] - (J['ha' + s] - J['el' + s]) * 0.12, r[2], r[3], lo)
    hand(fig, J['ha' + s], J['fa' + s], hr, hand_m)
    if claws:
        d = dir_(J['fa' + s])
        for k in (-1, 0, 1):
            a = J['ha' + s] + d * (hr * 0.9) + np.array([d[1], -d[0]]) * k * hr * 0.6
            fig.line(a, a + d * 2.2 + np.array([0.6, 0]), claws)


def leg_(fig, J, s, up, lo, foot_m, r=(3.0, 2.5, 2.4, 2.0), toe=4.2, fh=3.8, toes=None):
    fig.part = 'thigh' + s
    limb(fig, J['hip' + s], J['kn' + s], r[0], r[1], up)
    fig.part = 'shin' + s
    limb(fig, J['kn' + s], J['ft' + s], r[2], r[3], lo)
    boot(fig, J['ft' + s], J['toe' + s], toe, foot_m, h=fh)
    if toes:
        d = dir_(J['toe' + s])
        for k in range(3): fig.dot(J['ft' + s] + d * (toe - 0.4 - k * 1.4) - np.array([d[1], -d[0]]) * 0.4, toes)


def torso_(fig, J, m, front=5.0, back=-4.6, low=1.4, bev=4.0, hunch=0.0):
    n, p = J['neck'], J['pel']
    return fig.poly([P2(local(J, n, front * 0.55, -0.4)), P2(local(J, n, front, 3.0)), P2(local(J, p, front * 0.92, -3.6)),
                     P2(local(J, p, front * 0.85, low)), P2(local(J, p, back * 0.9, low)), P2(local(J, p, back, -4)),
                     P2(local(J, n, back - hunch, 3.0)), P2(local(J, n, back * 0.55 - hunch, -0.6))], m, bev=bev)


def skirt_(fig, J, P, m, drop=2.2, flare=2.6, front=4.4, back=-4.2, trim=None):
    p = J['pel']
    kf = max(J['knN'][0], J['knF'][0]); kb = min(J['knN'][0], J['knF'][0])
    hem = max(J['knN'][1], J['knF'][1]) - 2.6 + drop
    sw = P['cloth'] * 0.7
    sk = [P2(local(J, p, front, -0.6)), (max(kf + flare, p[0] + front + 0.4) + sw, hem - 0.6), (max(kf + flare - 1, p[0] + front - 0.6) + sw, hem + 0.7),
          (min(kb - flare + 1, p[0] + back + 0.8) + sw, hem + 0.9), (min(kb - flare, p[0] + back - 0.2) + sw, hem - 0.2), P2(local(J, p, back, -0.6))]
    fig.poly(sk, m, bev=3.0)
    if trim:
        fig.grp = 'trim'
        for a, b in zip(sk[1:4], sk[2:5]): fig.cap(a, b, 0.8, 0.8, trim, bev=0.8)
        fig.grp = None


def robe_(fig, J, P, m, front=4.6, back=-4.6, flare=3.0, trim=None, ragged=0.0, top=-0.6):
    """A robe from the waist to the ground, swinging with the stride."""
    p, gy = J['pel'], J['gy']
    ff = max(J['ftN'][0], J['ftF'][0], p[0] + front) + flare * 0.6; fb = min(J['ftN'][0], J['ftF'][0], p[0] + back) - flare * 0.6
    sw = P['cloth'] * 1.0
    hem = gy - 1.4
    pts = [P2(local(J, p, front, top)), (ff + sw, hem - 0.8), (ff - 1.2 + sw, hem + 0.6)]
    n = 6
    for i in range(1, n):  # a hem with a little wave (or rags)
        x = ff - 1.2 + (fb - ff + 2.4) * i / n
        pts.append((x + sw, hem + 0.6 - (ragged * (i % 2) * 1.8) - 0.4 * math.sin(i * 1.7 + P['cloth'])))
    pts += [(fb + 1.2 + sw, hem + 0.6), (fb + sw, hem - 0.8), P2(local(J, p, back, top))]
    fig.poly(pts, m, bev=3.4)
    if trim: fig.cap((ff - 1.0 + sw, hem), (fb + 1.0 + sw, hem), 0.8, 0.8, trim, bev=0.8)
    return pts


def wing_(fig, root, ang, L, mem, bone, back, spread=1.1):
    """A bat-wing: bones fanning from `root`, membrane scalloped between them back to `back`."""
    tip = root + dir_(ang) * L
    f1 = root + dir_(ang + spread * 0.5) * L * 0.82
    f2 = root + dir_(ang + spread) * L * 0.6
    inset = lambda a, b: (a + b) * 0.5 + (root - (a + b) * 0.5) * 0.22
    fig.poly([P2(root), P2(tip), P2(inset(tip, f1)), P2(f1), P2(inset(f1, f2)), P2(f2), P2(inset(f2, back)), P2(back)], mem, bev=2.0, flat=True)
    if bone:
        for e in (tip, f1, f2): fig.cap(root, e, 0.8, 0.45, bone, bev=0.6)


# ---- gear
def axe_(f, hand, ang, L=13, head=STEEL, haft=ASH, back=3):
    d = dir_(ang); side = np.array([d[1], -d[0]])
    f.cap(hand - d * back, hand + d * L, 0.8, 0.8, haft, bev=0.8)
    t = hand + d * (L - 1.5)
    f.poly([P2(t - d * 2.6), P2(t + side * 4.6 - d * 3.6), P2(t + side * 5.0 + d * 2.0), P2(t + d * 1.6)], head, bev=1.5)  # a bearded blade


def club_(f, hand, ang, L=18, r0=1.2, r1=3.6, m=None, studs=None):
    d = dir_(ang)
    f.cap(hand - d * 2.5, hand + d * L, r0, r1, m or Mat('#7a5634', tex=t_grain), bev=r1)
    if studs:
        side = np.array([d[1], -d[0]])
        for k in (0.55, 0.75, 0.92): f.dot(hand + d * L * k + side * (r0 + (r1 - r0) * k) * 0.8, studs)


def staff_(f, hand, ang, L=24, wood=None, orb=None, top='orb', below=8):
    d = dir_(ang)
    f.cap(hand - d * below, hand + d * L, 0.9, 0.9, wood or Mat('#5a3e2a', tex=t_grain), bev=0.9)
    t = hand + d * (L + 1.6)
    if top == 'skull':
        f.circle(t, 2.4, BONEM)
        f.dot(t + np.array([0.6, -0.2]), (30, 20, 26)); f.dot(t + np.array([-0.8, -0.2]), (30, 20, 26))
        t = t + d * 3.4
    if orb:
        f.circle(t, 2.2, orb)
        f.dot(t + np.array([-0.8, -0.8]), orb.r[4])


def bow_(f, hand, ang, draw_hand=None, L=11, wood=None, arrow=True):
    """A bow held out along `ang`, its limbs across it; the string runs to `draw_hand` when drawn."""
    d = dir_(ang); side = np.array([d[1], -d[0]])
    wood = wood or Mat('#7a5232')
    top, bot = hand + side * L - d * 2.2, hand - side * L - d * 2.2
    mid1, mid2 = hand + side * L * 0.55 + d * 0.6, hand - side * L * 0.55 + d * 0.6
    for a, b in ((top, mid1), (mid1, hand + d * 0.8), (hand + d * 0.8, mid2), (mid2, bot)): f.cap(a, b, 0.75, 0.75, wood, bev=0.7)
    pull = draw_hand if draw_hand is not None else (top + bot) * 0.5
    f.line(top, pull, (210, 204, 186)); f.line(pull, bot, (210, 204, 186))
    if arrow and draw_hand is not None:
        f.line(draw_hand, hand + d * 3.5, (150, 110, 70))
        f.dot(hand + d * 3.8, (200, 204, 212))


def bomb_(f, c, r=2.8, lit=True):
    f.circle(c, r, Mat('#3a3640'))
    f.box(P2(c + np.array([0.2, -r - 0.4])), 0.9, 0.8, Mat('#7a6a54'), bev=0.6)
    if lit:
        f.line(c + np.array([0.4, -r - 1.0]), c + np.array([1.6, -r - 2.6]), (150, 120, 80))
        f.dot(c + np.array([1.8, -r - 3.0]), (255, 220, 120)); f.dot(c + np.array([2.6, -r - 3.4]), (255, 140, 60))


# ---- attack styles: (stand) -> dict of clips
def held(stand, *keys):
    def ex(p, t):
        for k in keys: p[k] = stand[k]
        if 'aN' in keys: p['aN'] = stand['aN'] - 0.12 * math.sin(t)
    return ex


def style_swing(stand, reach=15):
    up = dict(stand, tN=0.3, kN=0.4, tF=-0.3, kF=0.2, aN=-2.7, eN=-0.3, wa=math.pi + 0.6, lean=stand['lean'] - 0.2, x=-1.0)
    down = dict(stand, tN=0.7, kN=0.6, tF=-0.5, kF=0.2, aN=1.25, eN=0.2, wa=1.1, lean=stand['lean'] + 0.3, x=2.5, cloth=1.0)
    return dict(windup=keys([stand, up], 3), strike=[dict(lerp_pose(up, down, 0.55), fx=(3.6, 2.2)), dict(down, fx=(3.0, 1.2))],
                recover=keys([down, stand], 3))


def style_smash(stand):
    """Both arms over the head, then down with the whole body behind it."""
    up = dict(stand, tN=0.25, kN=0.3, tF=-0.35, kF=0.2, aN=-2.9, eN=-0.2, aF=-2.7, eF=-0.3, wa=math.pi + 0.35, lean=stand['lean'] - 0.3, x=-1.5, nod=-0.2)
    down = dict(stand, tN=0.8, kN=0.9, tF=-0.6, kF=0.3, aN=1.0, eN=0.25, aF=0.9, eF=0.3, wa=0.9, lean=stand['lean'] + 0.45, x=3.0, cloth=1.0, y=1.5)
    return dict(windup=keys([stand, up], 3), strike=[dict(lerp_pose(up, down, 0.55), fx=(3.5, 2.0)), dict(down, fx=(3.0, 1.0))],
                recover=keys([down, down, stand], 4))


def style_thrust(stand):
    guard_ = dict(stand, tN=0.45, kN=0.5, tF=-0.35, kF=0.25, aN=0.2, eN=1.5, wa=math.pi / 2 - 0.05, lean=stand['lean'] + 0.05)
    drawn = dict(stand, tN=0.5, kN=0.6, tF=-0.45, kF=0.3, aN=-0.8, eN=2.2, wa=math.pi / 2 + 0.05, lean=stand['lean'] - 0.15, x=-2.0)
    thrust = dict(stand, tN=0.85, kN=0.7, tF=-0.6, kF=0.2, aN=1.45, eN=0.1, wa=math.pi / 2 - 0.02, lean=stand['lean'] + 0.32, x=3.0, cloth=1.0)
    return dict(windup=keys([guard_, drawn], 3), strike=[dict(thrust, fx='thrust'), thrust], recover=keys([thrust, guard_, stand], 3))


def style_claw(stand):
    """A swipe with both hands (no weapon)."""
    up = dict(stand, aN=-2.4, eN=0.6, aF=-2.0, eF=0.5, lean=stand['lean'] - 0.2, x=-1.0)
    down = dict(stand, aN=1.4, eN=0.4, aF=1.1, eF=0.3, lean=stand['lean'] + 0.4, x=2.5, tN=0.7, kN=0.6, tF=-0.5)
    return dict(windup=keys([stand, up], 3), strike=[dict(lerp_pose(up, down, 0.5), fx=(3.4, 2.0)), dict(down, fx=(2.8, 1.2))],
                recover=keys([down, stand], 3))


def style_cast(stand, raise_=2.3):
    """Ranged: the draw (windup, played as the cooldown runs out) and the release (cast) of a spell or a throw."""
    charge = dict(stand, aN=-0.3, eN=1.9, wa=math.pi - 0.15, lean=stand['lean'] - 0.1, charge=1.0)
    loose = dict(stand, aN=raise_, eN=0.15, wa=math.pi * 0.62, lean=stand['lean'] + 0.15, x=1.0, cloth=0.6, cast=1.0)
    return dict(windup=keys([stand, charge], 3), cast=[loose, loose, lerp_pose(loose, stand, 0.6)])


def style_hurt(stand):
    hit = dict(stand, tN=-0.1, kN=0.4, tF=-0.5, kF=0.3, lean=stand['lean'] - 0.35, nod=stand['nod'] - 0.3, x=-1.5, aN=stand['aN'] - 0.5, aF=stand['aF'] + 0.4)
    return [hit, lerp_pose(hit, stand, 0.5)]


def biped_set(stand, attack, walk=None, hold=('aN', 'eN', 'wa'), breathe=0.7, extra=None):
    w = dict(stride=0.55, knee=1.0, bob=1.0, arm=0.45, lean=stand['lean'] + 0.06)
    w.update(walk or {})
    A = dict(idle=idle_cycle(6, stand, breathe), walk=walk_cycle(8, extra=held(stand, *hold), **w))
    A.update(attack)
    A['hurt'] = style_hurt(stand)
    if extra: A.update(extra)
    return A


def weapon_fx(fig, J, P, aw, reach):
    if not P.get('fx'): return
    if P['fx'] == 'thrust': streak(fig, J['haN'] - dir_(aw) * 6, J['haN'] + dir_(aw) * reach * 0.8, (236, 232, 220))
    else: smear(fig, J['shN'], reach * 0.55, reach + 6, P['fx'][0], P['fx'][1], (236, 228, 206))


# ---------------------------------------------------------------- goblins
GOB = Body(thigh=5.6, shin=5.4, torso=8.4, ua=5.4, fa=5.0, head=10.0, sw=1.8, hw=1.3)
GSKIN = Mat('#6f9a3e')


def goblin_head(fig, J, P, skin, bomber=False):
    g, H = fig.g, J['head']
    fig.part = 'head'
    fig.poly([P2(H + np.array([-2.2, -1.4])), P2(H + np.array([-9.0, -4.4 + P['cloth'] * 0.4])), P2(H + np.array([-6.4, -1.0])), P2(H + np.array([-2.4, 1.6]))], skin, bev=1.6)  # the ear
    fig.ell(H + np.array([0.8, 0.2]), 4.6, 4.2, skin, bev=2.6)
    fig.ell(H + np.array([4.8, 1.2]), 2.4, 1.3, skin, rot=0.25, bev=1.2)  # a long nose
    fig.dot(H + np.array([2.0, -0.8]), (240, 214, 90)); fig.dot(H + np.array([2.8, -0.8]), (30, 20, 16))
    fig.dot(H + np.array([4.4, -0.8]), (30, 20, 16))
    fig.line(H + np.array([1.8, -1.8]), H + np.array([3.2, -1.4]), skin.r[0])  # a scowl
    fig.line(H + np.array([2.0, 2.8]), H + np.array([4.2, 2.6]), (60, 30, 30))
    fig.dot(H + np.array([3.4, 3.2]), (236, 228, 200))  # a tooth
    if bomber:  # a leather cap with goggles pushed up
        fig.add(np.maximum(g.ellipse(H + np.array([0.2, -1.4]), 5.0, 4.0), g.Y - (H[1] - 1.6)), Mat('#6a4428'), bev=2.4, org=P2(H))
        fig.circle(H + np.array([2.2, -2.6]), 1.5, Mat('#9aa2a8')); fig.circle(H + np.array([4.2, -2.4]), 1.3, Mat('#9aa2a8'))
        fig.dot(H + np.array([2.0, -3.0]), (220, 240, 250))
    else:
        for k in range(3): fig.line(H + np.array([-1.6 + k * 1.4, -3.8]), H + np.array([-2.6 + k * 1.2, -5.6]), (40, 50, 26))  # tufts


@creature
def goblin(fig, J, P, L=None):
    rag, belt = Mat('#7a6446', tex=t_rags), Mat('#4a3020')
    aw = P.get('wa', J['w'])
    fig.dark = 1
    arm_(fig, J, 'F', GSKIN, GSKIN, GSKIN, r=(1.9, 1.6, 1.6, 1.4), hr=1.7)
    leg_(fig, J, 'F', rag, GSKIN, GSKIN, r=(2.4, 2.0, 1.7, 1.4), toe=3.6, fh=2.4)
    fig.dark = 0
    leg_(fig, J, 'N', rag, GSKIN, GSKIN, r=(2.5, 2.1, 1.8, 1.5), toe=3.6, fh=2.4, toes=GSKIN.r[1])
    fig.part = 'torso'
    torso_(fig, J, rag, front=4.2, back=-3.8, hunch=1.0, bev=3)
    skirt_(fig, J, P, rag, drop=-1.0, flare=1.8, front=3.8, back=-3.6)
    fig.cap(local(J, J['pel'], -3.8, -1.2), local(J, J['pel'], 4.0, -1.2), 0.9, 0.9, belt, bev=0.9)
    goblin_head(fig, J, P, GSKIN)
    fig.part = 'weapon'
    sword(fig, J['haN'], aw, L=10, blade=RUSTY, guard=Mat('#5a3a22'), w=1.4)
    arm_(fig, J, 'N', GSKIN, GSKIN, GSKIN, r=(2.0, 1.7, 1.7, 1.5), hr=1.8)
    weapon_fx(fig, J, P, aw, 12)


def goblin_anims():
    stand = pose(tN=0.25, kN=0.5, tF=-0.05, kF=0.4, aN=0.4, eN=0.6, aF=0.3, eF=0.5, wa=1.0, lean=0.3, nod=-0.1)
    return biped_set(stand, style_swing(stand), walk=dict(stride=0.6, knee=1.2, bob=1.3, lean=0.38))


@creature
def bomber(fig, J, P, L=None):
    vest, belt = Mat('#6a4a30', tex=t_quilt), Mat('#3e2a1c')
    fig.dark = 1
    arm_(fig, J, 'F', GSKIN, GSKIN, GSKIN, r=(1.9, 1.6, 1.6, 1.4), hr=1.7)
    leg_(fig, J, 'F', Mat('#5a5040'), GSKIN, Mat('#4a3020'), r=(2.4, 2.0, 1.7, 1.4), toe=3.6, fh=3.0)
    fig.dark = 0
    leg_(fig, J, 'N', Mat('#5a5040'), GSKIN, Mat('#4a3020'), r=(2.5, 2.1, 1.8, 1.5), toe=3.6, fh=3.0)
    fig.part = 'torso'
    torso_(fig, J, vest, front=4.4, back=-4.0, hunch=1.0, bev=3)
    n, p = J['neck'], J['pel']
    fig.cap(local(J, n, 2.8, 0.6), local(J, p, -3.4, -1.0), 1.0, 1.0, belt, bev=0.9)  # the bandolier, bombs on it
    for k in (0.3, 0.6): bomb_(fig, local(J, n, 2.8 - 6.2 * k, 0.6 + 10.0 * k) + np.array([0, 0.4]), r=1.5, lit=False)
    fig.cap(local(J, p, -4.0, -1.0), local(J, p, 4.2, -1.0), 1.0, 1.0, belt, bev=0.9)
    goblin_head(fig, J, P, GSKIN, bomber=True)
    fig.part = 'weapon'
    if not P.get('cast'): bomb_(fig, J['haN'] + dir_(J['faN']) * 1.6, r=2.8)
    arm_(fig, J, 'N', GSKIN, GSKIN, GSKIN, r=(2.0, 1.7, 1.7, 1.5), hr=1.8)


def bomber_anims():
    stand = pose(tN=0.25, kN=0.5, tF=-0.05, kF=0.4, aN=0.5, eN=1.2, aF=0.3, eF=0.5, lean=0.25, nod=-0.1)
    wind = dict(stand, aN=-2.4, eN=-0.6, lean=0.05, x=-1.0, tN=0.4, kN=0.5)
    loose = dict(stand, aN=1.7, eN=0.1, lean=0.45, x=1.5, cast=1.0, tN=0.7, kN=0.6, tF=-0.4)
    return biped_set(stand, dict(windup=keys([stand, wind], 3), cast=[loose, loose, lerp_pose(loose, stand, 0.6)]),
                     walk=dict(stride=0.6, knee=1.2, bob=1.3, lean=0.33), hold=('aN', 'eN'))


REDB = Body(thigh=5.8, shin=5.6, torso=8.6, ua=5.4, fa=5.0, head=9.6, sw=1.6, hw=1.2)


@creature
def redcap(fig, J, P, L=None):
    """A redcap: a wizened little murderer, long grey beard, a pointed cap dyed in blood, iron boots and an iron pike."""
    g, H, n, p = fig.g, J['head'], J['neck'], J['pel']
    skin, coat, iron, cap = Mat('#c89c80'), Mat('#6a5a48', tex=t_rags), Mat('#6e7078'), Mat('#b4241e')
    aw = P.get('wa', J['w'])
    fig.dark = 1
    arm_(fig, J, 'F', coat, coat, skin, r=(1.9, 1.6, 1.6, 1.4), hr=1.6)
    leg_(fig, J, 'F', coat, iron, iron, r=(2.2, 1.8, 2.2, 2.4), toe=4.4, fh=4.2)
    fig.dark = 0
    leg_(fig, J, 'N', coat, iron, iron, r=(2.3, 1.9, 2.3, 2.5), toe=4.4, fh=4.2)
    fig.part = 'torso'
    torso_(fig, J, coat, front=4.0, back=-3.8, hunch=1.2, bev=3)
    skirt_(fig, J, P, coat, drop=0.0, flare=1.8, front=3.8, back=-3.6)
    fig.cap(local(J, p, -3.8, -1.0), local(J, p, 4.0, -1.0), 0.9, 0.9, Mat('#3a2a1e'), bev=0.9)
    fig.part = 'head'
    fig.ell(H + np.array([0.8, 0.6]), 4.0, 4.4, skin, bev=2.4)
    fig.ell(H + np.array([4.4, 1.4]), 1.6, 1.4, skin, bev=1)  # a hooked nose
    fig.add(np.maximum(np.maximum(g.ellipse(H + np.array([2.0, 5.0]), 3.0, 5.0), -(g.Y - (H[1] + 2.2))), -(g.X - (H[0] - 1.2))), Mat('#c8c4bc'), bev=1.6, org=P2(H))  # the beard
    fig.dot(H + np.array([2.4, -0.2]), (255, 70, 50)); fig.dot(H + np.array([4.2, -0.2]), (220, 50, 40))
    fig.line(H + np.array([1.8, -1.2]), H + np.array([3.4, -0.8]), (90, 80, 76))
    tip = H + np.array([-7.0 - P['cloth'] * 0.8, -4.0])
    fig.poly([P2(H + np.array([-4.4, -1.0])), P2(H + np.array([3.6, -2.0])), P2(H + np.array([1.0, -6.0])), P2(tip)], cap, bev=2)  # the cap, flopping back
    fig.dot(H + np.array([2.6, -1.4]), (120, 10, 10)); fig.dot(H + np.array([-1.0, -0.6]), (120, 10, 10))  # drips
    fig.part = 'weapon'
    spear(fig, J['haN'], aw, fore=18, back=6, shaft=Mat('#4a4c54'), head=iron)
    arm_(fig, J, 'N', coat, coat, skin, r=(2.0, 1.7, 1.7, 1.5), hr=1.7)
    weapon_fx(fig, J, P, aw, 22)


def redcap_anims():
    stand = pose(tN=0.25, kN=0.5, tF=-0.05, kF=0.4, aN=0.4, eN=-0.7, aF=0.3, eF=0.6, wa=math.pi - 0.35, lean=0.3, nod=0.0)
    return biped_set(stand, style_thrust(stand), walk=dict(stride=0.62, knee=1.2, bob=1.2, lean=0.38))


# ---------------------------------------------------------------- the undead
def skel_body(fig, J, P, bone=BONEM, rag=None, eye=(255, 96, 60), hood=None):
    """A skeleton's far limbs, near leg, ribs, pelvis and skull (its near arm goes on last, over its gear)."""
    g, H, n, p = fig.g, J['head'], J['neck'], J['pel']
    fig.dark = 1
    skel_arm(fig, J, 'F', bone)
    skel_leg(fig, J, 'F', bone)
    fig.dark = 0
    skel_leg(fig, J, 'N', bone)
    fig.part = 'torso'
    fig.cap(P2(n), P2(p), 1.1, 1.1, bone)
    for k in range(4):
        fig.cap(P2(local(J, n, -2.4, 2.0 + k * 1.7)), P2(local(J, n, 3.6 - k * 0.3, 3.4 + k * 1.8)), 0.75, 0.6, bone, bev=0.8, grp='ribs')
    fig.ell(P2(local(J, p, 0.4, 0)), 3.6, 2.2, bone, rot=-J['lean'], bev=1.6)
    if rag:
        kf = max(J['knN'][0], J['knF'][0])
        fig.poly([P2(local(J, p, -3.6, -1)), P2(local(J, p, 3.8, -1)), (kf + 1.0, J['knN'][1] - 3), (p[0] - 0.5, J['knN'][1] - 1), (p[0] - 3, J['knN'][1] - 4)], rag, bev=1.6)
    fig.cap(P2(local(J, n, -3.0, 0.6)), P2(local(J, n, 3.6, 0.8)), 1.0, 1.0, bone)
    fig.part = 'head'
    fig.cap(P2(n), P2(H + np.array([0, 3])), 0.9, 0.9, bone)
    if hood: fig.ell(P2(H + np.array([-0.6, 0.0])), 5.6, 5.8, hood, bev=3)
    fig.ell(P2(H + np.array([0.2, -0.6])), 4.6, 4.4, bone, bev=3.0)
    fig.box(P2(H + np.array([2.4, 3.4])), 2.6, 1.6, bone, rnd=0.8, bev=1.4)
    fig.ell(P2(H + np.array([2.6, 0.2])), 1.4, 1.5, Mat('#2a1c22'), bev=0.5)
    fig.ell(P2(H + np.array([5.0, 0.2])), 0.9, 1.4, Mat('#2a1c22'), bev=0.5)
    fig.dot(H + np.array([2.6, 0.2]), eye)
    fig.dot(H + np.array([5.4, 2.0]), (40, 26, 32))
    for k in range(4): fig.dot(H + np.array([1.4 + k * 1.1, 2.6]), (60, 44, 46))
    if hood:
        fig.add(np.maximum(g.ellipse(H + np.array([-0.4, -1.4]), 6.0, 5.0), g.Y - (H[1] - 0.6)), hood, bev=2.6, org=P2(H))  # the hood's crown


def skel_arm(fig, J, s, bone=BONEM):
    fig.part = 'uarm' + s
    limb(fig, J['sh' + s], J['el' + s], 1.3, 1.1, bone)
    fig.circle(J['el' + s], 1.4, bone)
    fig.circle(J['sh' + s], 1.9, bone)
    fig.part = 'farm' + s
    limb(fig, J['el' + s], J['ha' + s], 1.1, 0.9, bone)
    hand(fig, J['ha' + s], J['fa' + s], 1.3, bone)


def skel_leg(fig, J, s, bone=BONEM):
    fig.part = 'thigh' + s
    limb(fig, J['hip' + s], J['kn' + s], 1.5, 1.2, bone)
    fig.circle(J['kn' + s], 1.6, bone)
    fig.part = 'shin' + s
    limb(fig, J['kn' + s], J['ft' + s], 1.2, 1.0, bone)
    boot(fig, J['ft' + s], J['toe' + s], 3.8, bone, h=1.8)


@creature
def archer(fig, J, P, L=None):
    hood = Mat('#4a3a5c', tex=t_rags)
    skel_body(fig, J, P, rag=Mat('#4a3a5c', tex=t_rags), hood=hood)
    fig.part = 'weapon'
    bow_(fig, J['haN'], J['faN'] + 0.0, J['haF'] if P.get('charge') else None, arrow=bool(P.get('charge')))
    skel_arm(fig, J, 'N')


def archer_anims():
    stand = pose(tN=0.1, kN=0.2, tF=-0.12, kF=0.15, aN=0.5, eN=0.3, aF=0.3, eF=0.6, lean=0.08)
    aim = dict(stand, aN=1.5, eN=0.05, aF=1.35, eF=-1.9, lean=-0.02, tN=0.35, kN=0.3, tF=-0.35, charge=1.0)
    loose = dict(aim, aF=1.0, eF=-2.6, charge=0.0, cast=1.0)
    A = biped_set(stand, dict(windup=keys([stand, aim], 3), cast=[loose, loose, lerp_pose(loose, stand, 0.6)]), hold=('aN', 'eN'))
    return A


@creature
def risen(fig, J, P, L=None):
    """The dead levy of the battlefield: grey-green flesh in a rotten gambeson, a dented helm, a broken spear."""
    g, H, n, p = fig.g, J['head'], J['neck'], J['pel']
    rot, gamb, cloth, helm = Mat('#8a9468'), Mat('#7c7058', tex=t_quilt), Mat('#5a3a30', tex=t_rags), Mat('#8a786a')
    aw = P.get('wa', J['w'])
    fig.dark = 1
    arm_(fig, J, 'F', gamb, rot, rot, r=(2.5, 2.1, 1.8, 1.5), hr=1.8)
    leg_(fig, J, 'F', Mat('#4a4040'), Mat('#4a4040', tex=t_rags), Mat('#3e2a20'), r=(2.9, 2.4, 2.2, 1.8), fh=4.0)
    fig.dark = 0
    leg_(fig, J, 'N', Mat('#4a4040'), Mat('#4a4040', tex=t_rags), Mat('#3e2a20'), r=(3.0, 2.5, 2.3, 1.9), fh=4.0)
    fig.part = 'torso'
    torso_(fig, J, gamb, front=5.0, back=-4.4, hunch=0.8)
    skirt_(fig, J, P, gamb, drop=1.0, flare=2.2)
    fig.poly([P2(local(J, n, 1.0, 1.0)), P2(local(J, n, 4.8, 3.0)), P2(local(J, p, 4.2, 3.0)), P2(local(J, p, 1.6, 5.0)), P2(local(J, p, -0.6, 1.0))], cloth, bev=2)  # the torn tabard
    fig.part = 'head'
    fig.cap(local(J, n, 0.6, 1), H + np.array([0.4, 3.4]), 1.8, 1.8, rot)
    fig.ell(H + np.array([0.8, 0.6]), 4.2, 4.8, rot, bev=2.4)
    fig.box(P2(H + np.array([2.6, 3.6])), 2.0, 1.2, rot, rnd=0.6, bev=1)
    fig.dot(H + np.array([2.6, 0.2]), (230, 236, 170)); fig.dot(H + np.array([4.8, 0.2]), (200, 210, 150))
    fig.line(H + np.array([2.2, 3.0]), H + np.array([4.6, 3.0]), (40, 30, 30))
    fig.ell(H + np.array([0.4, -2.0]), 4.6, 3.6, helm, bev=3)
    fig.add(np.maximum(g.ellipse(H + np.array([0.6, -1.6]), 7.6, 1.8), -(g.Y - (H[1] - 1.8))), helm, bev=1.2, org=P2(H))
    fig.line(H + np.array([-1.0, -4.6]), H + np.array([0.6, -3.6]), helm.r[0])  # a dent
    fig.part = 'weapon'
    spear(fig, J['haN'], aw, fore=24, back=8, shaft=ASH, head=RUSTY)
    arm_(fig, J, 'N', gamb, rot, rot, r=(2.6, 2.2, 1.9, 1.6), hr=1.9)
    weapon_fx(fig, J, P, aw, 30)


def risen_anims():
    stand = pose(tN=0.15, kN=0.3, tF=-0.15, kF=0.2, aN=0.35, eN=-0.9, aF=0.4, eF=0.3, wa=math.pi - 0.3, lean=0.22, nod=0.25)
    return biped_set(stand, style_thrust(stand), walk=dict(stride=0.42, knee=0.8, bob=1.4, lean=0.3, arm=0.3))


@creature
def draugr(fig, J, P, L=None):
    """A drowned Norse dead: blue-grey skin, rusted mail, a spangenhelm, a bearded axe and a faded round shield."""
    g, H, n, p = fig.g, J['head'], J['neck'], J['pel']
    skin, mail, helm, cloak = Mat('#7d8f96'), Mat('#7a6a5c', tex=t_mail), Mat('#7a7068'), Mat('#3a4a4e', tex=t_rags)
    aw = P.get('wa', J['w'])
    fig.part = 'torso'
    fig.poly([P2(local(J, n, -2.0, 0.0)), P2(local(J, n, -5.6, 2.4)), (J['knF'][0] - 5 - P['cloth'], J['knF'][1] + 2), (J['knF'][0] - 1.0, J['knF'][1] + 3.0), P2(local(J, p, -1.0, 0))], cloak, bev=2)  # a torn cloak
    fig.dark = 1
    arm_(fig, J, 'F', mail, skin, skin, r=(2.7, 2.3, 2.0, 1.7), hr=2.0)
    leg_(fig, J, 'F', Mat('#3e3a3a'), Mat('#4a4846', tex=t_wrap), Mat('#3a2a22'), fh=4.0)
    fig.dark = 0
    leg_(fig, J, 'N', Mat('#3e3a3a'), Mat('#4a4846', tex=t_wrap), Mat('#3a2a22'), fh=4.0)
    fig.part = 'torso'
    torso_(fig, J, mail, front=5.2, back=-4.6, hunch=0.6)
    skirt_(fig, J, P, mail, drop=0.0, flare=2.2)
    fig.cap(local(J, p, -4.6, -1.0), local(J, p, 5.0, -1.0), 1.2, 1.2, Mat('#3a2a20'), bev=1)
    fig.part = 'head'
    fig.cap(local(J, n, 0.6, 1), H + np.array([0.4, 3.4]), 2.0, 2.0, skin)
    fig.ell(H + np.array([0.8, 0.6]), 4.4, 5.0, skin, bev=2.4)
    fig.add(np.maximum(np.maximum(g.ellipse(H + np.array([2.0, 4.2]), 3.6, 3.6), -(g.Y - (H[1] + 2.4))), -(g.X - (H[0] - 1.0))), Mat('#8a9a98'), bev=1.6, org=P2(H))  # a grey beard
    fig.dot(H + np.array([2.6, 0.0]), (200, 240, 255)); fig.dot(H + np.array([4.8, 0.0]), (170, 220, 240))
    fig.add(np.maximum(g.ellipse(H + np.array([0.2, -0.8]), 5.4, 5.4), g.Y - (H[1] - 1.0)), helm, bev=3, org=P2(H))
    fig.cap(H + np.array([-5.2, -1.0]), H + np.array([5.4, -1.0]), 0.9, 0.9, RUSTY, bev=0.9)
    fig.cap(H + np.array([0.2, -6.0]), H + np.array([0.2, -1.0]), 0.6, 0.6, RUSTY, bev=0.6)
    fig.cap(H + np.array([4.8, -1.0]), H + np.array([5.0, 1.8]), 0.7, 0.6, helm, bev=0.7)
    fig.part = 'shield'
    sc = J['haF'] + np.array([2.4, -1.4])
    sd = g.circle(sc, 6.0)
    fig.add(sd, Mat('#5a6a5e', tex=t_grain), bev=3, org=P2(sc))
    fig.add(np.maximum(sd, -np.abs(g.X - sc[0]) + 1.2), Mat('#8a4a3a'), bev=1, org=P2(sc))
    fig.add(np.abs(sd + 0.5) - 0.6, RUSTY, bev=0.6, org=P2(sc))
    fig.circle(P2(sc), 1.4, RUSTY)
    fig.part = 'weapon'
    axe_(fig, J['haN'], aw, L=14, head=Mat('#8a8e96'))
    arm_(fig, J, 'N', mail, skin, skin, r=(2.8, 2.4, 2.1, 1.8), hr=2.1)
    weapon_fx(fig, J, P, aw, 15)


def draugr_anims():
    stand = pose(tN=0.15, kN=0.25, tF=-0.15, kF=0.15, aN=0.35, eN=0.5, aF=0.55, eF=1.1, wa=1.0, lean=0.15, nod=0.1)
    return biped_set(stand, style_swing(stand), walk=dict(stride=0.45, knee=0.8, bob=1.1, lean=0.22), hold=('aN', 'eN', 'wa', 'aF', 'eF'))


# ---------------------------------------------------------------- knights and cultists
@creature
def knight(fig, J, P, L=None):
    """A knight of Dunmoor in plate: great helm with a red plume, navy surcoat with a gold cross, sword and heater."""
    g, H, n, p = fig.g, J['head'], J['neck'], J['pel']
    plate, sur, gold = Mat('#a8b0bc', tex=t_plate), Mat('#2c3a66'), Mat('#d0a046')
    aw = P.get('wa', J['w'])
    fig.dark = 1
    arm_(fig, J, 'F', plate, plate, plate, r=(2.9, 2.4, 2.2, 1.9), hr=2.1)
    leg_(fig, J, 'F', plate, plate, plate, r=(3.2, 2.6, 2.5, 2.1), fh=3.6, toe=4.6)
    fig.dark = 0
    leg_(fig, J, 'N', plate, plate, plate, r=(3.3, 2.7, 2.6, 2.2), fh=3.6, toe=4.6)
    fig.part = 'torso'
    torso_(fig, J, plate, front=5.4, back=-4.8)
    skirt_(fig, J, P, sur, drop=0.6, flare=2.4, front=4.8, back=-4.4)
    fig.poly([P2(local(J, n, 0.8, 1.6)), P2(local(J, n, 5.0, 3.2)), P2(local(J, p, 4.8, -3.0)), P2(local(J, p, 4.6, 1.0)), P2(local(J, p, -4.2, 1.0)), P2(local(J, p, -4.6, -3)), P2(local(J, n, -4.6, 3.2))], sur, bev=3)
    c = local(J, n, 2.6, 6.0)
    fig.grp = 'cross'
    fig.box(P2(c), 0.8, 3.0, gold, rot=-J['lean'], bev=0.7); fig.box(P2(c - J['up'] * 0.8), 2.4, 0.8, gold, rot=-J['lean'], bev=0.7)
    fig.grp = None
    fig.cap(local(J, p, -4.6, -1.0), local(J, p, 5.0, -1.0), 1.2, 1.2, Mat('#3a2a20'), bev=1)
    fig.circle(local(J, n, -0.6, 1.4), 3.4, plate)  # the pauldron
    fig.part = 'head'
    fig.box(P2(H + np.array([0.6, 0.2])), 4.6, 5.4, Mat('#b4bcc8'), rnd=1.6, bev=3)  # the great helm
    fig.line(H + np.array([1.6, -0.6]), H + np.array([5.4, -0.6]), (24, 22, 30))
    fig.line(H + np.array([3.0, 1.6]), H + np.array([5.4, 1.6]), (60, 62, 74))
    fig.cap(H + np.array([0.0, -5.2]), H + np.array([-6.0 - P['cloth'], -3.0]), 1.8, 1.0, Mat('#b02a24'), bev=1.4)  # the plume
    fig.part = 'shield'
    sc = J['haF'] + np.array([1.8, -2.0])
    sh_d = g.poly([(sc[0] - 4.6, sc[1] - 6), (sc[0] + 4.6, sc[1] - 6), (sc[0] + 4.4, sc[1] + 1), (sc[0], sc[1] + 8.8), (sc[0] - 4.4, sc[1] + 1)])
    fig.add(sh_d, sur, bev=3, org=P2(sc))
    fig.grp = 'emb'
    fig.box(P2(sc + np.array([0, 0.4])), 0.9, 5.6, gold, bev=0.8); fig.box(P2(sc + np.array([0, -1.6])), 3.4, 0.9, gold, bev=0.8)
    fig.grp = None
    fig.add(np.abs(sh_d + 0.5) - 0.55, gold, bev=0.6, org=P2(sc))
    fig.part = 'weapon'
    sword(fig, J['haN'], aw, L=18)
    arm_(fig, J, 'N', plate, plate, plate, r=(3.0, 2.5, 2.3, 2.0), hr=2.2)
    weapon_fx(fig, J, P, aw, 18)


def knight_anims():
    stand = pose(tN=0.15, kN=0.2, tF=-0.15, kF=0.15, aN=0.3, eN=0.6, aF=0.6, eF=1.2, wa=1.15, lean=0.06)
    return biped_set(stand, style_swing(stand), walk=dict(stride=0.45, knee=0.9, bob=0.9, lean=0.1), hold=('aN', 'eN', 'wa', 'aF', 'eF'))


@creature
def cultist(fig, J, P, L=None):
    """A Dunmoor cultist: a crimson hooded robe trimmed in gold, a pale face lost in shadow, a staff of fire."""
    g, H, n, p = fig.g, J['head'], J['neck'], J['pel']
    robe, gold, skin, fire = Mat('#8a2228'), Mat('#c8963c'), Mat('#d8b4a0'), Mat('#ff8a3a', glow=True)
    aw = P.get('wa', J['w'])
    fig.dark = 1
    arm_(fig, J, 'F', robe, robe, skin, r=(2.4, 2.4, 2.4, 2.8), hr=1.6)
    leg_(fig, J, 'F', robe, robe, Mat('#3a2620'), r=(2.6, 2.2, 2.0, 1.8), fh=2.6)
    fig.dark = 0
    leg_(fig, J, 'N', robe, robe, Mat('#3a2620'), r=(2.7, 2.3, 2.1, 1.9), fh=2.6)
    fig.part = 'torso'
    torso_(fig, J, robe, front=4.8, back=-4.6, low=2.0)
    robe_(fig, J, P, robe, trim=gold)
    fig.cap(local(J, n, 2.4, 1.0), local(J, p, 3.8, 1.0), 0.8, 0.8, gold, bev=0.8)  # the front band
    fig.cap(local(J, p, -4.4, -1.2), local(J, p, 4.8, -1.2), 0.9, 0.9, Mat('#c8b48a'), bev=0.8)  # a rope belt
    fig.part = 'head'
    fig.ell(H + np.array([-0.4, 0.6]), 5.6, 6.0, robe, bev=3.2)  # the hood
    fig.add(np.maximum(g.ellipse(H + np.array([2.4, 1.4]), 3.0, 3.8), -(g.X - (H[0] + 0.6))), Mat('#2a1418'), bev=1, org=P2(H))
    fig.dot(H + np.array([2.8, 1.0]), (255, 170, 80)); fig.dot(H + np.array([4.4, 1.0]), (255, 150, 60))
    fig.dot(H + np.array([3.6, 3.4]), skin.r[1])
    fig.part = 'weapon'
    staff_(fig, J['haN'], aw, L=22, orb=fire)
    if P.get('charge'): fig.circle(J['haN'] + dir_(aw) * 23.6, 3.4, Mat('#ffc070', glow=True))
    arm_(fig, J, 'N', robe, robe, skin, r=(2.5, 2.5, 2.5, 2.9), hr=1.7)


def cultist_anims():
    stand = pose(tN=0.1, kN=0.15, tF=-0.1, kF=0.1, aN=0.35, eN=-0.9, aF=0.5, eF=1.4, wa=math.pi - 0.2, lean=0.08, nod=0.1)
    return biped_set(stand, style_cast(stand), walk=dict(stride=0.4, knee=0.6, bob=0.6, lean=0.1, arm=0.2))


# ---------------------------------------------------------------- the big ones
TROLLB = Body(thigh=9.0, shin=8.4, torso=16.0, ua=10.0, fa=10.0, head=11.0, sw=4.2, hw=3.2, sh=0.12)


@creature
def troll(fig, J, P, L=None):
    """A hill troll: hunched, hulking, grey-green, a nose like a turnip, moss for hair and a tree for a club."""
    g, H, n, p = fig.g, J['head'], J['neck'], J['pel']
    skin, fur, moss = Mat('#7b8b5a'), Mat('#6a5038', tex=t_fur), Mat('#4e6e34', tex=t_fur)
    aw = P.get('wa', J['w'])
    fig.dark = 1
    arm_(fig, J, 'F', skin, skin, skin, r=(4.0, 3.4, 3.6, 3.0), hr=3.4)
    leg_(fig, J, 'F', skin, skin, skin, r=(4.4, 3.6, 3.4, 3.0), toe=5.4, fh=3.0)
    fig.dark = 0
    leg_(fig, J, 'N', skin, skin, skin, r=(4.6, 3.8, 3.5, 3.1), toe=5.4, fh=3.0, toes=skin.r[1])
    fig.part = 'torso'
    torso_(fig, J, skin, front=7.4, back=-6.8, hunch=2.4, bev=6, low=1.0)
    fig.ell(P2(local(J, p, 2.6, -4.2)), 6.0, 5.0, skin, bev=4)  # the belly
    skirt_(fig, J, P, fur, drop=-1.6, flare=3.0, front=6.6, back=-6.4)
    fig.cap(local(J, p, -6.6, -1.4), local(J, p, 7.0, -1.4), 1.6, 1.6, Mat('#4a3424'), bev=1.2)
    fig.part = 'head'
    fig.ell(H + np.array([0.6, 0.6]), 5.4, 5.0, skin, bev=3)
    fig.ell(H + np.array([5.6, 2.2]), 3.0, 2.2, skin, rot=0.4, bev=2)  # the nose
    fig.add(np.maximum(g.ellipse(H + np.array([-0.6, -1.6]), 5.8, 4.0), g.Y - (H[1] - 0.6)), moss, bev=2, org=P2(H))
    fig.dot(H + np.array([2.6, -0.4]), (250, 220, 120)); fig.dot(H + np.array([3.2, -0.4]), (30, 20, 16))
    fig.line(H + np.array([1.8, -1.4]), H + np.array([3.8, -1.0]), skin.r[0])
    fig.line(H + np.array([1.6, 4.0]), H + np.array([4.6, 4.2]), (50, 30, 30))
    fig.dot(H + np.array([2.0, 3.4]), (236, 228, 200)); fig.dot(H + np.array([4.2, 3.6]), (236, 228, 200))  # tusks
    fig.part = 'weapon'
    club_(fig, J['haN'], aw, L=24, r0=1.6, r1=4.6, studs=(120, 120, 128))
    arm_(fig, J, 'N', skin, skin, skin, r=(4.2, 3.6, 3.8, 3.2), hr=3.6)
    weapon_fx(fig, J, P, aw, 24)


def troll_anims():
    stand = pose(tN=0.25, kN=0.45, tF=-0.15, kF=0.3, aN=0.45, eN=0.4, aF=0.35, eF=0.5, wa=0.75, lean=0.38, nod=-0.2)
    thr = style_cast(stand, raise_=2.0)
    A = biped_set(stand, style_smash(stand), walk=dict(stride=0.42, knee=0.9, bob=1.8, lean=0.45, arm=0.3))
    A['cast'] = thr['cast']
    return A


GOLEMB = Body(thigh=6.0, shin=6.4, torso=14.0, ua=8.0, fa=8.5, head=7.0, sw=5.4, hw=3.4, sh=0.06)


@creature
def golem(fig, J, P, L=None):
    """A rock golem: slabs of stone held together by green rune-light, moss on its shoulders, fists like boulders."""
    g, H, n, p = fig.g, J['head'], J['neck'], J['pel']
    stone, dark, moss = Mat('#86827a', tex=lambda x, y: np.where(((x * 3 + y * 5) % 13) == 0, -0.8, 0.0)), Mat('#6a665e'), Mat('#4e7a3a', tex=t_fur)
    rune = (150, 255, 140)

    def slab_arm(s):
        fig.part = 'uarm' + s
        fig.cap(J['sh' + s], J['el' + s], 3.6, 3.2, stone, bev=3)
        fig.part = 'farm' + s
        fig.cap(J['el' + s], J['ha' + s], 3.6, 4.4, stone, bev=3)
        fig.circle(J['ha' + s] + dir_(J['fa' + s]) * 2.0, 5.0, dark, bev=4)
        fig.dot(J['el' + s] + dir_(J['fa' + s]) * 3.0, rune)

    def slab_leg(s):
        fig.part = 'thigh' + s
        fig.cap(J['hip' + s], J['kn' + s], 4.0, 3.8, dark, bev=3)
        fig.part = 'shin' + s
        fig.cap(J['kn' + s], J['ft' + s], 3.8, 4.6, stone, bev=3)
        boot(fig, J['ft' + s], J['toe' + s], 5.4, dark, h=3.4)

    fig.dark = 1
    slab_arm('F'); slab_leg('F')
    fig.dark = 0
    slab_leg('N')
    fig.part = 'torso'
    torso_(fig, J, stone, front=8.4, back=-7.6, bev=5, low=1.0, hunch=1.0)
    fig.ell(P2(local(J, J['neck'], -1.0, 0.0)), 8.0, 3.4, moss, bev=2)  # moss on the shoulders
    for a, b in (((2.0, 3.0), (4.0, 8.0)), ((4.0, 8.0), (1.0, 12.0)), ((-2.0, 6.0), (0.0, 10.0))):  # glowing cracks
        fig.line(local(J, n, *a), local(J, n, *b), rune)
    fig.part = 'head'
    fig.box(P2(H + np.array([1.0, 1.4])), 3.8, 3.4, stone, rnd=1.0, bev=2.4)
    fig.line(H + np.array([2.2, 0.8]), H + np.array([4.2, 0.8]), rune)
    slab_arm('N')


def golem_anims():
    stand = pose(tN=0.15, kN=0.2, tF=-0.15, kF=0.15, aN=0.25, eN=0.3, aF=0.2, eF=0.3, lean=0.15, nod=0.0)
    A = biped_set(stand, style_smash(stand), walk=dict(stride=0.35, knee=0.6, bob=1.2, lean=0.2, arm=0.25), hold=())
    A['cast'] = style_cast(stand, raise_=2.0)['cast']
    return A


# ---------------------------------------------------------------- fliers and spirits
IMPB = Body(thigh=4.4, shin=4.4, torso=7.0, ua=4.4, fa=4.2, head=8.0, sw=1.4, hw=1.0)


@creature
def imp(fig, J, P, L=None):
    """A fire imp: red hide, curled horns, leathery wings and a spade-tipped tail; fire in its claws."""
    g, H, n, p = fig.g, J['head'], J['neck'], J['pel']
    hide, wingm, horn, fire = Mat('#c2402a'), Mat('#7a2a26'), Mat('#e0d0b0'), Mat('#ffb050', glow=True)
    wa = P.get('wing', -2.0)
    fig.part = 'wingF'
    wing_(fig, J['shF'], wa - 0.3, 13, wingm, hide, local(J, p, -1.0, -1.0))
    fig.dark = 1
    arm_(fig, J, 'F', hide, hide, hide, r=(1.5, 1.3, 1.3, 1.1), hr=1.4, claws=(250, 230, 200))
    leg_(fig, J, 'F', hide, hide, hide, r=(1.8, 1.5, 1.3, 1.1), toe=2.6, fh=1.6)
    fig.dark = 0
    leg_(fig, J, 'N', hide, hide, hide, r=(1.9, 1.6, 1.4, 1.2), toe=2.6, fh=1.6)
    fig.part = 'torso'
    t0 = p + np.array([-2.0, 0.6])
    t1 = t0 + np.array([-4.0, 3.0 + P['cloth']]); t2 = t1 + np.array([-3.0, -3.0])
    fig.cap(t0, t1, 0.9, 0.7, hide, bev=0.8); fig.cap(t1, t2, 0.7, 0.6, hide, bev=0.6)
    fig.poly([P2(t2 + np.array([-1.8, 0.4])), P2(t2 + np.array([0.6, -2.2])), P2(t2 + np.array([0.8, 1.4]))], hide, bev=1)
    torso_(fig, J, hide, front=3.4, back=-3.0, bev=2.6)
    fig.part = 'head'
    fig.ell(H + np.array([0.6, 0.4]), 3.8, 3.6, hide, bev=2.2)
    fig.cap(H + np.array([-0.4, -2.6]), H + np.array([-3.0, -5.4]), 1.0, 0.4, horn, bev=0.8)
    fig.cap(H + np.array([1.8, -2.8]), H + np.array([1.0, -5.8]), 1.0, 0.4, horn, bev=0.8)
    fig.dot(H + np.array([2.0, -0.4]), (255, 230, 90)); fig.dot(H + np.array([3.6, -0.4]), (255, 200, 60))
    fig.line(H + np.array([1.6, 1.8]), H + np.array([3.8, 2.2]), (60, 14, 14))
    fig.part = 'wing'
    wing_(fig, J['shN'], wa, 15, wingm, hide, local(J, p, -0.6, -1.0))
    arm_(fig, J, 'N', hide, hide, hide, r=(1.6, 1.4, 1.4, 1.2), hr=1.5, claws=(250, 230, 200))
    if P.get('charge') or P.get('cast'):
        c = J['haN'] + dir_(J['faN']) * 2.4
        fig.circle(c, 2.6 if P.get('charge') else 1.6, fire)


def flap(stand, n=6, lo=-2.6, hi=-0.6, extra=None):
    out = []
    for i in range(n):
        t = i / n * 2 * math.pi
        p = dict(stand, wing=lo + (hi - lo) * (0.5 - 0.5 * math.cos(t)), y=stand['y'] - 1.2 * math.sin(t), cloth=math.sin(t))
        if extra: extra(p, t)
        out.append(p)
    return out


def imp_anims():
    stand = pose(tN=0.4, kN=0.9, tF=0.1, kF=0.7, aN=0.6, eN=0.9, aF=0.4, eF=0.9, lean=0.15, y=-2.0)
    A = dict(idle=flap(stand), walk=flap(dict(stand, lean=0.35)))
    cast = style_cast(stand, raise_=1.6)
    A['windup'] = [dict(p, wing=-2.2) for p in cast['windup']]
    A['cast'] = [dict(p, wing=-1.0) for p in cast['cast']]
    A['hurt'] = [dict(p, wing=-1.6) for p in style_hurt(stand)]
    return A


GHOSTB = Body(thigh=7.0, shin=7.0, torso=11.0, ua=6.4, fa=6.2, head=9.5, sw=1.8, hw=1.2)


def ghost_tail(fig, J, P, m, length=16, width=5.4):
    """A spirit's robe trailing into wisps where its legs would be."""
    p = J['pel']
    sw = P['cloth']
    pts = [P2(local(J, p, width, -1.0))]
    for i, (dx, dy) in enumerate(((width + 0.6, 5), (width - 0.6 + sw, 10), (2.0 + sw * 1.6, length), (-1.0 + sw * 1.2, length - 4), (-3.0 + sw * 2.0, length + 1),
                                  (-5.0 + sw * 1.6, length - 5), (-width - 1.6 + sw, 8))):
        pts.append((p[0] + dx, p[1] + dy))
    pts.append(P2(local(J, p, -width, -1.0)))
    fig.poly(pts, m, bev=3.4)


@creature
def wraith(fig, J, P, L=None):
    """A frost wraith: a hooded, trailing shroud of pale ice-blue, bony hands, a lantern of cold light."""
    g, H, n, p = fig.g, J['head'], J['neck'], J['pel']
    shroud, bone, cold = Mat('#8cb0c0', tex=t_rags), Mat('#c8d4d8'), Mat('#a0f0ff', glow=True)
    fig.dark = 1
    arm_(fig, J, 'F', shroud, shroud, bone, r=(2.2, 2.2, 2.2, 2.6), hr=1.4)
    fig.part = 'weapon'
    lc = J['haF'] + np.array([0.0, 4.0])
    fig.line(J['haF'], lc - np.array([0, 2.6]), (60, 60, 70))
    fig.box(P2(lc), 1.8, 2.4, Mat('#5a5a64'), bev=1); fig.box(P2(lc), 1.0, 1.5, cold, bev=1)
    fig.dark = 0
    fig.part = 'torso'
    ghost_tail(fig, J, P, shroud)
    torso_(fig, J, shroud, front=4.8, back=-4.6, low=1.0, hunch=1.0)
    fig.part = 'head'
    fig.ell(H + np.array([-0.6, 0.6]), 5.6, 6.2, shroud, bev=3.2)
    fig.add(np.maximum(g.ellipse(H + np.array([2.4, 1.4]), 3.0, 4.0), -(g.X - (H[0] + 0.6))), Mat('#141a24'), bev=1, org=P2(H))
    fig.dot(H + np.array([2.8, 1.0]), (200, 255, 255)); fig.dot(H + np.array([4.4, 1.0]), (160, 240, 255))
    arm_(fig, J, 'N', shroud, shroud, bone, r=(2.3, 2.3, 2.3, 2.7), hr=1.5)
    if P.get('charge') or P.get('cast'):
        c = J['haN'] + dir_(J['faN']) * 2.6
        fig.poly([P2(c + np.array([0, -3.4])), P2(c + np.array([1.6, 0])), P2(c + np.array([0, 3.4])), P2(c + np.array([-1.6, 0]))], cold, bev=1)


def ghost_anims(stand):
    idle = []
    for i in range(6):
        t = i / 6 * 2 * math.pi
        idle.append(dict(stand, y=stand['y'] - 1.5 * math.sin(t), cloth=math.sin(t), aN=stand['aN'] + 0.08 * math.sin(t), aF=stand['aF'] - 0.08 * math.sin(t)))
    A = dict(idle=idle, walk=[dict(p, lean=stand['lean'] + 0.2) for p in idle])
    A.update(style_cast(stand, raise_=1.7))
    A['hurt'] = style_hurt(stand)
    return A


def wraith_anims():
    return ghost_anims(pose(aN=0.7, eN=0.6, aF=0.8, eF=0.7, lean=0.2, nod=0.1, y=-6.0, tN=0.0, kN=0.0, tF=0.0, kF=0.0))


@creature
def banshee(fig, J, P, L=None):
    """A banshee: a gaunt white spirit, hair streaming behind her, a tattered shroud; she keens with her mouth wide."""
    g, H, n, p = fig.g, J['head'], J['neck'], J['pel']
    dress, skin, hair = Mat('#c4c8d0', tex=t_rags), Mat('#d8dce4'), Mat('#eef0f4')
    wail = P.get('cast') or P.get('charge')
    fig.part = 'head'
    sw = P['cloth']
    fig.poly([P2(H + np.array([-1.0, -4.6])), P2(H + np.array([-9.0 - sw, -2.0])), P2(H + np.array([-12.0 - sw * 1.6, 6.0])), P2(H + np.array([-8.0 - sw, 12.0])), P2(H + np.array([-3.0, 6.0]))], hair, bev=2.4)
    fig.dark = 1
    arm_(fig, J, 'F', skin, skin, skin, r=(1.4, 1.2, 1.2, 1.0), hr=1.3)
    fig.dark = 0
    fig.part = 'torso'
    ghost_tail(fig, J, P, dress, width=4.8)
    torso_(fig, J, dress, front=4.0, back=-3.8, low=1.0)
    fig.part = 'head'
    fig.cap(local(J, n, 0.6, 1), H + np.array([0.4, 3.2]), 1.5, 1.5, skin)
    fig.ell(H + np.array([0.8, 0.4]), 3.8, 4.8, skin, bev=2.4)
    fig.add(np.maximum(g.ellipse(H + np.array([-0.2, -1.4]), 4.8, 4.4), hair_cut(g, H, 0.4, -1.8)), hair, bev=2, org=P2(H))
    fig.dot(H + np.array([2.2, 0.0]), (40, 50, 70)); fig.dot(H + np.array([4.0, 0.0]), (40, 50, 70))
    if wail: fig.ell(H + np.array([3.0, 3.0]), 1.0, 1.6 if P.get('cast') else 1.0, Mat('#202430'), bev=0.5)
    else: fig.line(H + np.array([2.4, 2.8]), H + np.array([3.8, 2.8]), (120, 120, 140))
    arm_(fig, J, 'N', skin, skin, skin, r=(1.5, 1.3, 1.3, 1.1), hr=1.4)


def banshee_anims():
    stand = pose(aN=0.4, eN=0.4, aF=0.5, eF=0.5, lean=0.15, nod=0.0, y=-6.0)
    A = ghost_anims(stand)
    scream = dict(stand, aN=-1.0, eN=-0.3, aF=-0.6, eF=-0.3, nod=-0.4, lean=-0.15, cast=1.0)
    A['windup'] = keys([stand, dict(stand, aN=1.0, eN=1.4, aF=1.0, eF=1.4, nod=0.3, lean=0.35, charge=1.0)], 3)
    A['cast'] = [scream, scream, lerp_pose(scream, stand, 0.6)]
    return A


# ---- the bat
def solve_bat(B, P, cx, gy):
    c = np.array([cx + P['x'], gy - 9.0 + P['y']])
    J = {'gy': gy, 'lean': 0.0, 'up': np.array([0.0, -1.0])}
    J['neck'] = c + np.array([2.2, -2.6]); J['pel'] = c + np.array([-1.6, 3.6])
    J['head'] = J['neck'] + np.array([1.4, -1.6])
    J['shN'] = J['neck'] + np.array([-0.4, 1.4]); J['shF'] = J['shN'] + np.array([-0.8, -0.4])
    w = P.get('wing', -2.0)
    J['elN'] = J['shN'] + dir_(w) * 17; J['elF'] = J['shF'] + dir_(w + 0.25) * 15
    for s in 'NF': J['ha' + s] = J['el' + s]; J['hip' + s] = J['pel']; J['kn' + s] = J['pel']; J['ft' + s] = J['pel']
    J['headb'], J['headt'] = J['neck'], J['head']
    J['wb'] = J['wt'] = J['pel']
    return J


@creature
def bat(fig, J, P, L=None):
    fur, mem, bone = Mat('#7e5e62'), Mat('#9a6a74'), Mat('#5e3e44')
    w = P.get('wing', -2.0)
    fig.part = 'uarmF'
    fig.dark = 1
    wing_(fig, J['shF'], w + 0.25, 15, mem, None, J['pel'] + np.array([-0.4, -0.6]))
    fig.dark = 0
    fig.part = 'torso'
    fig.ell(P2((J['neck'] + J['pel']) * 0.5), 3.4, 5.0, fur, rot=-0.6, bev=3)
    for s in (-1, 1): fig.cap(J['pel'], J['pel'] + np.array([-1.0 + s * 0.8, 3.2]), 0.6, 0.5, bone, bev=0.5)  # little hind claws
    H = J['head']
    fig.ell(H, 2.8, 2.6, fur, bev=2)
    fig.poly([P2(H + np.array([-1.6, -1.6])), P2(H + np.array([-2.6, -5.4])), P2(H + np.array([0.0, -2.2]))], mem, bev=1)
    fig.poly([P2(H + np.array([0.4, -2.0])), P2(H + np.array([1.2, -5.6])), P2(H + np.array([2.0, -1.6]))], mem, bev=1)
    fig.dot(H + np.array([1.4, -0.4]), (255, 70, 60))
    fig.dot(H + np.array([2.6, 1.6]), (240, 236, 224))
    fig.part = 'uarmN'
    wing_(fig, J['shN'], w, 17, mem, None, J['pel'] + np.array([0.6, -0.4]))


def bat_anims():
    stand = pose(y=0.0)
    A = dict(idle=flap(stand, lo=-2.9, hi=-0.3), walk=flap(stand, lo=-2.9, hi=-0.3))
    A['windup'] = [dict(stand, wing=-3.0, y=-1.0), dict(stand, wing=-3.2, y=-1.5), dict(stand, wing=-3.3, y=-2.0)]
    A['strike'] = [dict(stand, wing=-1.2, x=1.0), dict(stand, wing=-1.0, x=2.0)]
    A['recover'] = flap(stand, n=3, lo=-2.9, hi=-0.3)
    A['hurt'] = [dict(stand, wing=-0.4, y=1.0, x=-1.0), dict(stand, wing=-1.6)]
    return A


# ---- the slime
@creature
def slime(fig, J, P, L=None):
    g = fig.g
    goo, core = Mat('#6cd04a'), Mat('#4a9a34')
    sx, sy = P.get('sx', 1.0), P.get('sy', 1.0)
    rx, ry = 13.0 * sx, 10.5 * sy
    c = np.array([J['pel'][0] + P['x'], J['gy'] - ry - 0.5 + P['y']])
    fig.part = 'torso'
    d = np.maximum(g.ellipse(c, rx, ry), g.Y - (J['gy'] - 0.2 + P['y']))  # flat where it sits
    fig.add(d, goo, bev=6, org=P2(c))
    fig.circle(c + np.array([-3.0 * sx, 2.6 * sy]), 2.4, core, bev=1.2)  # what it swallowed: a skull, a bone
    fig.dot(c + np.array([-3.6 * sx, 2.2 * sy]), (60, 90, 40)); fig.dot(c + np.array([-2.2 * sx, 2.2 * sy]), (60, 90, 40))
    fig.cap(c + np.array([3.0 * sx, 4.4 * sy]), c + np.array([7.0 * sx, 3.0 * sy]), 0.8, 0.8, core, bev=0.6)
    for ex in (2.6, 6.6):
        e = c + np.array([ex * sx, -3.0 * sy])
        fig.circle(e, 1.8, Mat('#f0f4e8'), bev=1)
        fig.dot(e + np.array([0.6, 0.2]), (20, 30, 20))
    fig.dot(c + np.array([-6.0 * sx, -5.6 * sy]), (230, 255, 210)); fig.dot(c + np.array([-5.0 * sx, -6.4 * sy]), (230, 255, 210))


def slime_anims():
    A = {}
    A['idle'] = [pose(sx=1 + 0.06 * math.sin(i / 6 * 2 * math.pi), sy=1 - 0.06 * math.sin(i / 6 * 2 * math.pi)) for i in range(6)]
    A['walk'] = A['idle']
    A['windup'] = [pose(sx=1.1, sy=0.88), pose(sx=1.22, sy=0.76), pose(sx=1.28, sy=0.7)]
    A['strike'] = [pose(sx=0.82, sy=1.22), pose(sx=0.9, sy=1.1)]
    A['recover'] = [pose(sx=1.25, sy=0.75), pose(sx=0.95, sy=1.05), pose(sx=1.0, sy=1.0)]
    A['hurt'] = [pose(sx=1.3, sy=0.7, x=-1.0), pose(sx=1.1, sy=0.9)]
    return A


# ---- four-legged: wolf, kelpie
class Quad:
    def __init__(self, body, ua, fa, th, sh, neck, head, tail, neck_ang=0.72, head_ang=0.55):
        self.body, self.ua, self.fa, self.th, self.sh, self.neck, self.head, self.tail = body, ua, fa, th, sh, neck, head, tail
        self.neck_ang, self.head_ang = neck_ang * math.pi, head_ang * math.pi


def solve_quad(B, P, cx, gy):
    """Shoulders ahead, hips behind; front legs (aN/aF, knee eN/eF folding back), hind legs (tN/tF, hock kN/kF)."""
    def front(a, e): return B.ua * math.cos(a) + B.fa * math.cos(a - e)
    def hind(t, k): return B.th * math.cos(t + 0.3) + B.sh * math.cos(t - 0.45 - k)
    hf = max(front(P['aN'], P['eN']), front(P['aF'], P['eF']))
    hh = max(hind(P['tN'], P['kN']), hind(P['tF'], P['kF']))
    S = np.array([cx + P['x'] + B.body / 2, gy - hf - 1.0 + P['y']])
    Hp = np.array([cx + P['x'] - B.body / 2, gy - hh - 1.0 + P['y']])
    J = {'gy': gy, 'neck': S, 'pel': Hp, 'lean': 0.0, 'up': np.array([0.0, -1.0])}
    J['shN'], J['shF'] = S + np.array([0.6, 0.6]), S + np.array([-0.8, 0.0])
    J['hipN'], J['hipF'] = Hp + np.array([0.6, 0.6]), Hp + np.array([-0.4, 0.0])
    for s in 'NF':
        J['el' + s] = J['sh' + s] + dir_(P['a' + s]) * B.ua
        J['ha' + s] = J['el' + s] + dir_(P['a' + s] - P['e' + s]) * B.fa
        J['fa' + s] = P['a' + s] - P['e' + s]
        J['kn' + s] = J['hip' + s] + dir_(P['t' + s] + 0.3) * B.th
        J['ft' + s] = J['kn' + s] + dir_(P['t' + s] - 0.45 - P['k' + s]) * B.sh
    J['headb'] = S + dir_(B.neck_ang - P['nod']) * B.neck  # the neck reaching forward and up
    J['head'] = J['headb'] + dir_(B.head_ang - P['nod'] * 1.4) * (B.head * 0.5)
    J['headt'] = J['headb'] + dir_(B.head_ang - P['nod'] * 1.4) * B.head
    J['wb'] = Hp + np.array([-1.0, -0.6])
    J['wt'] = J['wb'] + dir_(P.get('tail', -2.2)) * B.tail
    return J


WOLFB = Quad(body=14.0, ua=5.4, fa=5.0, th=5.8, sh=5.4, neck=4.6, head=9.6, tail=11.0, neck_ang=0.7, head_ang=0.5)


@creature
def wolf(fig, J, P, L=None):
    g = fig.g
    fur, belly, dark = Mat('#86848c'), Mat('#c0bcb4'), Mat('#5e5c66')

    def leg(s, front):
        a, b, c = ('sh', 'el', 'ha') if front else ('hip', 'kn', 'ft')
        fig.part = ('uarm' if front else 'thigh') + s
        limb(fig, J[a + s], J[b + s], 3.0 if front else 3.8, 2.0, fur)
        fig.part = ('farm' if front else 'shin') + s
        limb(fig, J[b + s], J[c + s], 1.7, 1.4, fur)
        fig.ell(J[c + s] + np.array([1.0, -0.4]), 2.0, 1.1, dark, bev=0.8)

    fig.dark = 1
    leg('F', True); leg('F', False)
    fig.dark = 0
    fig.part = 'tail'
    fig.cap(J['wb'], J['wt'], 2.2, 1.8, fur, bev=1.8)
    fig.circle(J['wt'], 1.6, dark)
    fig.part = 'torso'
    S, Hp = J['neck'], J['pel']
    fig.grp = 'body'
    fig.cap(Hp, S, 4.6, 5.6, fur, bev=4.4)
    fig.ell(S + np.array([0.4, 1.6]), 4.8, 4.6, fur, bev=3.6)  # the deep chest
    fig.add(np.maximum(g.cap(Hp + np.array([1.4, 1.8]), S + np.array([0.6, 2.6]), 2.6, 3.4), -(g.Y - (Hp[1] + 1.6))), belly, bev=2, org=P2(S))
    fig.cap(S + np.array([0.6, -0.4]), J['headb'], 4.4, 3.4, fur, bev=3)  # the ruff of its neck
    fig.grp = None
    leg('N', False); leg('N', True)
    fig.part = 'head'
    hb = J['headb']
    d = (J['headt'] - hb) / (np.linalg.norm(J['headt'] - hb) + 1e-9)
    fig.ell(hb + d * 2.0, 3.8, 3.4, fur, bev=2.4)
    fig.cap(hb + d * 3.0, J['headt'], 2.4, 1.5, fur, bev=1.6)  # the muzzle
    if P.get('bite'): fig.cap(hb + d * 3.0 + np.array([0, 2.0]), J['headt'] + np.array([-0.8, 3.0]), 1.2, 0.9, Mat('#8a4a4a'), bev=0.6)
    fig.dot(J['headt'] + d * 0.6 + np.array([0, -0.4]), (24, 20, 24))
    fig.poly([P2(hb + np.array([-1.4, -2.0])), P2(hb + np.array([-0.4, -6.4])), P2(hb + np.array([1.4, -2.4]))], fur, bev=1.2)  # an ear
    fig.dot(hb + d * 3.4 + np.array([0.0, -1.6]), (250, 214, 90))


def quad_walk(n, stride=0.5, knee=0.9, bob=1.0, extra=None):
    out = []
    for i in range(n):
        t = i / n * 2 * math.pi
        p = pose(aN=stride * math.sin(t), eN=0.1 + knee * max(0.0, math.cos(t)), aF=stride * math.sin(t + math.pi), eF=0.1 + knee * max(0.0, math.cos(t + math.pi)),
                 tN=stride * math.sin(t + math.pi), kN=0.1 + knee * max(0.0, -math.cos(t)), tF=stride * math.sin(t), kF=0.1 + knee * max(0.0, -math.cos(t + math.pi)),
                 y=-abs(math.cos(t)) * bob + bob * 0.5, tail=-2.2 + 0.2 * math.sin(t * 2), nod=0.05 * math.sin(t * 2))
        if extra: extra(p, t)
        out.append(p)
    return out


def quad_anims(gallop=0.55):
    stand = pose(aN=0.05, eN=0.1, aF=-0.05, eF=0.1, tN=-0.05, kN=0.1, tF=0.05, kF=0.1)
    idle = [dict(stand, y=0.4 * math.sin(i / 6 * 2 * math.pi), tail=-2.2 + 0.25 * math.sin(i / 6 * 2 * math.pi), nod=0.04 * math.sin(i / 6 * 2 * math.pi)) for i in range(6)]
    crouch = dict(stand, aN=0.5, eN=1.4, aF=0.4, eF=1.3, tN=0.5, kN=1.0, tF=0.4, kF=1.0, nod=-0.35, y=0.0, bite=1.0)
    leap = dict(stand, aN=1.4, eN=0.1, aF=1.2, eF=0.2, tN=-1.0, kN=0.0, tF=-0.9, kF=0.1, nod=-0.1, tail=-1.6, bite=1.0, y=-3.0)
    return dict(idle=idle, walk=quad_walk(6, stride=gallop, knee=1.0, bob=1.2), windup=keys([stand, crouch], 3), strike=[leap, leap],
                recover=keys([leap, crouch, stand], 3), hurt=[dict(stand, nod=0.5, x=-1.5, tail=-1.2), dict(stand, nod=0.2)])


KELPB = Quad(body=20.0, ua=6.8, fa=6.4, th=7.0, sh=6.8, neck=9.0, head=12.0, tail=12.0, neck_ang=0.8, head_ang=0.36)


@creature
def kelpie(fig, J, P, L=None):
    """A kelpie: a water-horse of dark teal hide, weed for a mane and tail, eyes like drowned lamps."""
    g = fig.g
    hide, weed, hoof = Mat('#2f5a66'), Mat('#3e8a4e', tex=t_fur), Mat('#1e2a30')

    def leg(s, front):
        a, b, c = ('sh', 'el', 'ha') if front else ('hip', 'kn', 'ft')
        fig.part = ('uarm' if front else 'thigh') + s
        limb(fig, J[a + s], J[b + s], 3.0 if front else 4.0, 1.8, hide)
        fig.part = ('farm' if front else 'shin') + s
        limb(fig, J[b + s], J[c + s], 1.5, 1.3, hide)
        fig.box(P2(J[c + s] + np.array([0.2, -0.6])), 1.6, 1.1, hoof, bev=0.8)

    fig.dark = 1
    leg('F', True); leg('F', False)
    fig.dark = 0
    fig.part = 'tail'
    fig.cap(J['wb'], J['wt'], 2.0, 1.4, weed, bev=1.6)
    fig.cap((J['wb'] + J['wt']) * 0.5, J['wt'] + np.array([-1.0, 3.0]), 1.4, 0.8, weed, bev=1)
    fig.part = 'torso'
    S, Hp = J['neck'], J['pel']
    fig.grp = 'body'
    fig.cap(Hp, S, 5.6, 6.0, hide, bev=5)
    fig.cap(S + np.array([0.0, -1.0]), J['headb'], 4.2, 3.0, hide, bev=3)
    fig.grp = None
    for k in range(4):  # the mane, weed hanging down the neck
        a = S + (J['headb'] - S) * (0.2 + k * 0.25) + np.array([-1.6, -2.6])
        fig.cap(a, a + np.array([-2.2 - P['cloth'], 4.0]), 1.3, 0.6, weed, bev=0.8)
    leg('N', False); leg('N', True)
    fig.part = 'head'
    hb = J['headb']
    d = (J['headt'] - hb) / (np.linalg.norm(J['headt'] - hb) + 1e-9)
    fig.cap(hb, J['headt'], 3.2, 2.0, hide, bev=2.2)
    fig.poly([P2(hb + np.array([-1.0, -1.8])), P2(hb + np.array([-0.4, -5.0])), P2(hb + np.array([1.2, -2.2]))], hide, bev=1)
    fig.dot(hb + d * 3.0 + np.array([0.4, -1.4]), (150, 255, 230))
    fig.dot(J['headt'] + np.array([0.0, 0.6]), (20, 30, 34))


# ---------------------------------------------------------------- the bosses
BKB = Body(thigh=11.0, shin=10.6, torso=17.0, ua=9.4, fa=9.0, head=12.0, sw=4.4, hw=2.8)


@creature
def blackknight(fig, J, P, L=None):
    """The Black Knight: blackened spiked plate, an antlered helm with a red slit, a torn crimson cape, a great black sword."""
    g, H, n, p = fig.g, J['head'], J['neck'], J['pel']
    plate, edge, cape, red = Mat('#3a3a46', tex=t_plate), Mat('#5a5a6a'), Mat('#6a1a20', tex=t_rags), (255, 60, 50)
    aw = P.get('wa', J['w'])
    fig.part = 'torso'
    sw = P['cloth']
    fig.poly([P2(local(J, n, -1.0, -0.4)), P2(local(J, n, -7.0, 1.0)), (p[0] - 12.0 - sw * 2, J['gy'] - 6.0), (p[0] - 7.0 - sw, J['gy'] - 3.0),
              (p[0] - 3.0 - sw, J['gy'] - 7.0), P2(local(J, p, -1.0, -2.0))], cape, bev=3)
    fig.dark = 1
    arm_(fig, J, 'F', plate, plate, plate, r=(4.6, 3.8, 3.6, 3.0), hr=3.2)
    leg_(fig, J, 'F', plate, plate, plate, r=(5.2, 4.2, 4.0, 3.2), toe=6.4, fh=5.0)
    fig.dark = 0
    leg_(fig, J, 'N', plate, plate, plate, r=(5.4, 4.4, 4.1, 3.3), toe=6.4, fh=5.0)
    fig.part = 'torso'
    torso_(fig, J, plate, front=9.0, back=-8.0, bev=6)
    skirt_(fig, J, P, plate, drop=-0.6, flare=3.0, front=6.6, back=-6.2, trim=edge)
    fig.cap(local(J, p, -6.4, -1.2), local(J, p, 6.8, -1.2), 1.6, 1.6, Mat('#2a2228'), bev=1.2)
    for k in range(3): fig.cap(local(J, n, -2.4 + k * 2.6, 0.8), local(J, n, -3.4 + k * 2.6, -3.6), 1.0, 0.3, edge, bev=0.8)  # spikes
    fig.circle(local(J, n, 0.4, 2.4), 6.0, plate)  # the pauldron
    fig.part = 'head'
    fig.box(P2(H + np.array([0.6, 0.6])), 6.0, 6.8, plate, rnd=2.2, bev=3.6)
    fig.line(H + np.array([1.6, -0.4]), H + np.array([6.0, -0.4]), red)
    for s in (-1, 1):  # antlers
        b = H + np.array([-1.0 + s * 1.6, -5.4])
        t1 = b + np.array([-4.0 + s * 2.6, -8.0]); t2 = t1 + np.array([-4.0 + s * 1.4, -4.0])
        m = Mat('#6a5a48')
        fig.cap(b, t1, 1.0, 0.7, m, bev=0.8); fig.cap(t1, t2, 0.7, 0.4, m, bev=0.6)
        fig.cap(t1 + (b - t1) * 0.4, t1 + (b - t1) * 0.4 + np.array([2.0 + s, -3.0]), 0.6, 0.3, m, bev=0.5)
    fig.part = 'weapon'
    sword(fig, J['haN'], aw, L=34, blade=Mat('#4a4a58'), guard=Mat('#7a2a2a'), w=2.2)
    d = dir_(aw)
    for k in (8, 14, 20): fig.dot(J['haN'] + d * k, (255, 80, 60))  # runes down the blade
    arm_(fig, J, 'N', plate, plate, plate, r=(4.8, 4.0, 3.8, 3.2), hr=3.4)
    weapon_fx(fig, J, P, aw, 34)


def blackknight_anims():
    stand = pose(tN=0.2, kN=0.3, tF=-0.2, kF=0.2, aN=0.35, eN=0.7, aF=0.6, eF=1.0, wa=1.25, lean=0.08)
    A = biped_set(stand, style_swing(stand), walk=dict(stride=0.45, knee=0.9, bob=1.2, lean=0.14))
    low = dict(stand, aN=0.9, eN=0.6, wa=math.pi / 2 - 0.15, lean=0.4, tN=0.6, kN=0.8, tF=-0.4, kF=0.4)
    run = walk_cycle(6, stride=0.75, knee=1.4, bob=1.6, arm=0.3, lean=0.45, extra=held(low, 'aN', 'eN', 'wa'))
    A['charge'] = [low] + run
    return A


LICHB = Body(thigh=9.0, shin=9.0, torso=16.0, ua=9.4, fa=9.0, head=12.0, sw=2.6, hw=1.8)


@creature
def lich(fig, J, P, L=None):
    """The Lich King: a crowned skull with green fire in its sockets, ribs bare between rich violet robes, a skull-staff."""
    g, H, n, p = fig.g, J['head'], J['neck'], J['pel']
    robe, gold, bone, green = Mat('#4a2a66', tex=t_rags), Mat('#d4a640'), BONEM, Mat('#8affa0', glow=True)
    aw = P.get('wa', J['w'])
    fig.dark = 1
    arm_(fig, J, 'F', robe, robe, bone, r=(3.0, 2.8, 2.8, 3.2), hr=1.8, claws=(220, 210, 180))
    fig.dark = 0
    fig.part = 'torso'
    ghost_tail(fig, J, P, robe, length=22, width=7.4)
    torso_(fig, J, robe, front=6.4, back=-6.2, low=1.0, hunch=1.0, bev=4.4)
    fig.poly([P2(local(J, n, 0.6, 1.0)), P2(local(J, n, 5.6, 3.0)), P2(local(J, p, 4.6, -4.0)), P2(local(J, p, 0.6, -4.0))], Mat('#1e1426'), bev=1)  # the open chest
    for k in range(4): fig.cap(P2(local(J, n, 1.0, 3.0 + k * 2.0)), P2(local(J, n, 5.0 - k * 0.3, 4.0 + k * 2.0)), 0.7, 0.6, bone, bev=0.7, grp='ribs')
    fig.cap(local(J, n, 0.6, 0.6), local(J, p, 4.4, 1.0), 1.0, 1.0, gold, bev=0.9)
    fig.cap(local(J, n, -6.0, 0.4), local(J, n, 5.6, 0.6), 1.6, 1.6, gold, bev=1.2)  # a gold collar
    fig.part = 'head'
    fig.ell(H + np.array([0.2, -0.4]), 5.4, 5.2, bone, bev=3.4)
    fig.box(P2(H + np.array([2.6, 4.0])), 3.0, 1.8, bone, rnd=0.8, bev=1.4)
    fig.ell(H + np.array([2.6, 0.2]), 1.6, 1.7, Mat('#1a1420'), bev=0.5); fig.ell(H + np.array([5.2, 0.2]), 1.0, 1.6, Mat('#1a1420'), bev=0.5)
    fig.dot(H + np.array([2.6, 0.2]), green.r[3]); fig.dot(H + np.array([5.4, 0.2]), green.r[3])
    for k in range(5): fig.dot(H + np.array([1.2 + k * 1.1, 3.2]), (60, 44, 46))
    crown = [(-5.0, -3.6), (-4.6, -8.6), (-2.6, -5.6), (-0.6, -9.6), (1.4, -5.6), (3.6, -9.0), (5.0, -3.6)]
    fig.poly([P2(H + np.array(c)) for c in crown], gold, bev=1.6)
    fig.dot(H + np.array([-0.6, -6.0]), (120, 255, 150))
    fig.part = 'weapon'
    staff_(fig, J['haN'], aw, L=30, wood=Mat('#3a2a3a'), orb=green, top='skull', below=12)
    if P.get('charge'): fig.circle(J['haN'] + dir_(aw) * 35.0, 4.0, Mat('#c0ffd0', glow=True))
    arm_(fig, J, 'N', robe, robe, bone, r=(3.1, 2.9, 2.9, 3.3), hr=1.9, claws=(220, 210, 180))


def lich_anims():
    stand = pose(aN=0.35, eN=-0.9, aF=0.7, eF=0.9, wa=math.pi - 0.15, lean=0.12, y=-8.0)
    return ghost_anims(stand)


# ---------------------------------------------------------------- baking

class Spec:
    def __init__(self, name, painter, body, anims, w, h, gy, look=None, cx=None, rest=None, wlen=0, wclass='RW_NONE', kind='biped'):
        self.name, self.painter, self.body, self.anims, self.w, self.h, self.gy, self.look = name, painter, body, anims, w, h, gy, look
        self.kind = kind  # biped, quad, bat, or none (no corpse: it bursts)
        self.cx = w / 2 if cx is None else cx
        self.rest, self.wlen, self.wclass = rest, wlen, wclass  # the pose its corpse parts are cut from; weapon reach; weapon kind


def bake_frame(S, P, only=None):
    fig = getattr(S, 'fig', Fig)(S.w, S.h)  # tools/figures.py swaps in its own painter
    rot = P.get('rot') or 0.0
    J = {"quad": solve_quad, "bat": solve_bat}.get(S.kind, solve)(S.body, P, S.cx, S.gy)
    if rot:  # turned whole (a roll, lying prone): the grid turns about the body's middle, so the shapes rasterise crisply
        c = (J['pel'] + J['neck']) * 0.5
        tx, ty = S.cx, S.gy + P.get('ty', -10.0)
        cs, sn = math.cos(rot), math.sin(rot)
        dX, dY = fig.g.X - tx, fig.g.Y - ty
        fig.g.X, fig.g.Y = c[0] + dX * cs + dY * sn, c[1] - dX * sn + dY * cs
    S.painter(fig, J, P, S.look) if S.look else S.painter(fig, J, P)
    if rot:
        fig.marks = [(int(math.floor(tx + (x + 0.5 - c[0]) * cs - (y + 0.5 - c[1]) * sn)), int(math.floor(ty + (x + 0.5 - c[0]) * sn + (y + 0.5 - c[1]) * cs)), col, p)
                     for x, y, col, p in fig.marks]
    return fig.render(only), J


REST = pose(aN=0.0, eN=0.0, aF=0.0, eF=0.0, tN=0.0, kN=0.0, tF=0.0, kF=0.0, wa=0.0)
KNIGHTB = Body(thigh=9.2, shin=9.0, torso=13.2, ua=7.2, fa=6.8, head=10.5, sw=2.0, hw=1.5)
SPECS = {
    'hero': lambda: Spec('hero', hero, HERO, hero_anims(), 64, 60, 54),
    'guard': lambda: Spec('guard', guard, GUARD, guard_anims(), 120, 80, 72, cx=44, wlen=36, wclass='RW_POLE', rest=REST),
    'skeleton': lambda: Spec('skeleton', skeleton, SKEL, skeleton_anims(), 80, 72, 64, cx=36, wlen=15, wclass='RW_BLADE', rest=REST),
    'goblin': lambda: Spec('goblin', goblin, GOB, goblin_anims(), 72, 60, 52, cx=32, wlen=10, wclass='RW_BLADE', rest=REST),
    'bomber': lambda: Spec('bomber', bomber, GOB, bomber_anims(), 72, 60, 52, cx=32, wclass='RW_THROW', rest=REST),
    'redcap': lambda: Spec('redcap', redcap, REDB, redcap_anims(), 80, 60, 52, cx=32, wlen=20, wclass='RW_POLE', rest=REST),
    'archer': lambda: Spec('archer', archer, SKEL, archer_anims(), 80, 72, 64, cx=36, wclass='RW_BOW', rest=REST),
    'risen': lambda: Spec('risen', risen, GUARD, risen_anims(), 110, 80, 72, cx=44, wlen=26, wclass='RW_POLE', rest=REST),
    'draugr': lambda: Spec('draugr', draugr, GUARD, draugr_anims(), 96, 80, 72, cx=44, wlen=13, wclass='RW_BLADE', rest=REST),
    'knight': lambda: Spec('knight', knight, KNIGHTB, knight_anims(), 100, 84, 76, cx=44, wlen=18, wclass='RW_BLADE', rest=REST),
    'cultist': lambda: Spec('cultist', cultist, GUARD, cultist_anims(), 100, 90, 80, cx=44, wlen=22, wclass='RW_STAFF', rest=REST),
    'troll': lambda: Spec('troll', troll, TROLLB, troll_anims(), 150, 110, 100, cx=64, wlen=24, wclass='RW_BLADE', rest=REST),
    'golem': lambda: Spec('golem', golem, GOLEMB, golem_anims(), 130, 100, 88, cx=60, rest=REST),
    'imp': lambda: Spec('imp', imp, IMPB, imp_anims(), 80, 72, 62, cx=40, rest=REST),
    'wraith': lambda: Spec('wraith', wraith, GHOSTB, wraith_anims(), 90, 84, 74, cx=44, rest=REST),
    'banshee': lambda: Spec('banshee', banshee, GHOSTB, banshee_anims(), 90, 84, 74, cx=48, rest=REST),
    'bat': lambda: Spec('bat', bat, None, bat_anims(), 80, 64, 50, cx=40, kind='bat', rest=pose()),
    'slime': lambda: Spec('slime', slime, GOB, slime_anims(), 72, 50, 44, cx=36, kind='none'),
    'wolf': lambda: Spec('wolf', wolf, WOLFB, quad_anims(0.55), 96, 64, 56, cx=48, kind='quad', rest=pose()),
    'kelpie': lambda: Spec('kelpie', kelpie, KELPB, quad_anims(0.6), 130, 90, 80, cx=62, kind='quad', rest=pose()),
    'blackknight': lambda: Spec('blackknight', blackknight, BKB, blackknight_anims(), 190, 140, 128, cx=80, wlen=34, wclass='RW_BLADE', rest=REST),
    'lich': lambda: Spec('lich', lich, LICHB, lich_anims(), 160, 150, 136, cx=70, wlen=30, wclass='RW_STAFF', rest=REST),
}


def to_image(px, w, h):
    from PIL import Image
    im = Image.new('RGBA', (w, h), (0, 0, 0, 0))
    for j in range(h):
        for i in range(w):
            if px[j][i]: im.putpixel((i, j), px[j][i][0] + (255,))
    return im


def preview(outdir, names=None):
    from PIL import Image
    import os
    os.makedirs(outdir, exist_ok=True)
    Z = 4
    for name, mk in SPECS.items():
        if names and name not in names: continue
        S = mk()
        rows, gif = [], []
        for an, frames in S.anims.items():
            imgs = [to_image(bake_frame(S, P)[0][0], S.w, S.h) for P in frames]
            rows.append((an, imgs))
            for _ in range(2 if len(imgs) < 4 else 1):
                for im in imgs: gif += [im] * (2 if an in ('run', 'walk', 'roll') else 3)
        cols = max(len(r[1]) for r in rows)
        sheet = Image.new('RGBA', (cols * S.w * Z, len(rows) * S.h * Z), (46, 44, 56, 255))
        for r, (an, imgs) in enumerate(rows):
            for c, im in enumerate(imgs):
                bg = Image.new('RGBA', (S.w, S.h), (46, 44, 56, 255) if (r + c) % 2 else (54, 52, 64, 255))
                bg.alpha_composite(im)
                sheet.paste(bg.resize((S.w * Z, S.h * Z), Image.NEAREST), (c * S.w * Z, r * S.h * Z))
        sheet.save(f'{outdir}/{name}_sheet.png')
        fr = []
        for im in gif:
            bg = Image.new('RGBA', (S.w, S.h), (40, 38, 50, 255)); bg.alpha_composite(im)
            fr.append(bg.resize((S.w * Z, S.h * Z), Image.NEAREST).convert('P', palette=Image.ADAPTIVE))
        fr[0].save(f'{outdir}/{name}.gif', save_all=True, append_images=fr[1:], duration=50, loop=0)
        print(name, 'ok')


# ---------------------------------------------------------------- emitting sprites_anim.h

CLIPS = ['idle', 'walk', 'windup', 'strike', 'recover', 'hurt', 'jump', 'fall', 'crouch', 'crouchwalk', 'roll', 'crawl', 'climb', 'wall', 'swim', 'hang', 'cast', 'charge']
JOINTS = ['neck', 'pel', 'headb', 'headt', 'shF', 'elF', 'haF', 'shN', 'elN', 'haN', 'hipF', 'knF', 'ftF', 'hipN', 'knN', 'ftN', 'wb', 'wt']  # = rig.cpp J_*
ALPHABET = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz!#$%&()*+,-/:;<=>?@[]^_`{|}~"  # = HD_ALPHABET
ARM = {'uarmN', 'farmN'}
ALL = {'torso', 'head', 'uarmF', 'farmF', 'thighF', 'shinF', 'thighN', 'shinN', 'uarmN', 'farmN', 'weapon', 'shield', 'fx'}


def joints_of(S, J, P):
    pts = dict(J)
    if 'headb' not in pts:
        pts['headb'] = J['neck'] + J['up'] * 1.0
        pts['headt'] = J['head'] * 2 - pts['headb']
    if 'wb' not in pts:
        pts['wb'] = J['haN']
        pts['wt'] = J['haN'] + dir_(P.get('wa', J.get('w', 0.0))) * S.wlen
    return [pts.get(k, J['pel']) for k in JOINTS]


def palette_of(grids):
    pal = []
    for gr in grids:
        for row in gr:
            for p in row:
                if p is not None and p not in pal: pal.append(p)
    assert len(pal) <= len(ALPHABET), f'{len(pal)} colours'
    return pal


def pal_c(name, pal):
    al = {0: 255, 1: 254, 2: 253}
    return f"static const Color {name}[] = {{" + ", ".join(f"{{{c[0]}, {c[1]}, {c[2]}, {al[t]}}}" for c, t in pal) + "};"


def crop(grids):
    xs, ys = [], []
    for gr in grids:
        for j, row in enumerate(gr):
            for i, p in enumerate(row):
                if p is not None: xs.append(i); ys.append(j)
    return min(xs), min(ys), max(xs) + 1, max(ys) + 1


def emit_sheet(out, name, S, frames, ax, ay):
    """frames: list of (clip index, pixel grid, joints)."""
    grids = [f[1] for f in frames]
    x0, y0, x1, y1 = crop(grids)
    pal = palette_of(grids)
    out.append(pal_c(f'AP_{name}', pal))
    out.append(f'static const char AX_{name}[] =')
    for gr in grids:
        for j in range(y0, y1):
            out.append('    "' + ''.join('.' if gr[j][i] is None else ALPHABET[pal.index(gr[j][i])] for i in range(x0, x1)) + '"')
    out[-1] += ';'
    js = []
    for f in frames:
        for p in f[2]: js += [p[0] - ax, p[1] - ay]
    out.append(f'static const float AJ_{name}[] = {{' + ', '.join(f'{v:.1f}f' for v in js) + '};')
    clip = [[0, 0] for _ in CLIPS]
    for k, f in enumerate(frames):
        c = clip[f[0]]
        if c[1] == 0: c[0] = k
        c[1] += 1
    cl = ', '.join(f'{{{a}, {b}}}' for a, b in clip)
    out.append(f'static const AnimSheet ANIM_{name} = {{{x1 - x0}, {y1 - y0}, {ax - x0:.1f}f, {ay - y0:.1f}f, {len(frames)}, AP_{name}, AX_{name}, {{{cl}}}, AJ_{name}}};')
    out.append('')


def emit_parts(out, name, S):
    """The creature's body parts, cut from its rest pose, as a RigSpec for its corpse (rig.cpp)."""
    P = S.rest
    if S.kind == 'bat':
        slots = {'TORSO': ({'torso'}, 'neck', 'pel'), 'UARM': ({'uarmN'}, 'shN', 'elN')}
    else:
        slots = {'HEAD': ({'head'}, 'headb', 'headt'), 'TORSO': ({'torso'}, 'neck', 'pel'), 'UARM': ({'uarmN'}, 'shN', 'elN'),
                 'FARM': ({'farmN'}, 'elN', 'haN'), 'THIGH': ({'thighN'}, 'hipN', 'knN'), 'SHIN': ({'shinN'}, 'knN', 'ftN'),
                 'WEAPON': ({'weapon'}, 'wb', 'wt'), 'SHIELD': ({'shield'}, 'haF', None), 'WING': ({'wing'}, 'shN', None), 'TAIL': ({'tail'}, 'wb', 'wt')}
    cells, shield = [], False
    for slot in ('HEAD', 'TORSO', 'UARM', 'FARM', 'THIGH', 'SHIN', 'WEAPON', 'SHIELD', 'WING', 'TAIL'):
        if slot not in slots: cells.append('{nullptr, 0, 0, 0, 0}'); continue
        only, a, b = slots[slot]
        (gr, _), J = bake_frame(S, P, only)
        if not any(p is not None for row in gr for p in row): cells.append('{nullptr, 0, 0, 0, 0}'); continue
        shield |= slot == 'SHIELD'
        pts = dict(zip(JOINTS, joints_of(S, J, P)))
        x0, y0, x1, y1 = crop([gr])
        pal = palette_of([gr])
        nm = f'{name}_{slot}'
        out.append(pal_c(f'HDP_A_{nm}', pal))
        out.append(f'static const char* const HDR_A_{nm}[] = {{')
        for j in range(y0, y1): out.append('    "' + ''.join('.' if gr[j][i] is None else ALPHABET[pal.index(gr[j][i])] for i in range(x0, x1)) + '",')
        out.append('};')
        out.append(f'static const HDSprite HD_A_{nm} = {{{x1 - x0}, {y1 - y0}, HDP_A_{nm}, HDR_A_{nm}}};')
        pa = pts[a]
        if b: pb = pts[b]
        elif slot == 'WING': pb = pa + dir_(P.get('wing', -2.0)) * 15
        else: pb = pa + dir_(J['faF']) * 4
        cells.append(f'{{&HD_A_{nm}, {pa[0] - x0:.2f}f, {pa[1] - y0:.2f}f, {pb[0] - x0:.2f}f, {pb[1] - y0:.2f}f}}')
    B = S.body
    kind = {'quad': 'RK_QUAD', 'bat': 'RK_BAT'}.get(S.kind, 'RK_BIPED')
    shp, sw, hw = (B.shp, B.sw * 2, B.hw * 2) if isinstance(B, Body) else (0.0, 0.0, 0.0)
    out.append(f'static const RigSpec RIG_A_{name} = {{{kind}, {{{", ".join(cells)}}}, 0.0f, 0.0f, {shp:.3f}f, {sw:.2f}f, {hw:.2f}f, {S.wclass}, {int(shield)}}};')
    out.append('')


def emit_hero(out, S):
    """Per armour set: the body, the near arm alone, and that arm held out at 16 angles."""
    for look in ('wool', 'leather', 'mail', 'lamellar', 'scale'):
        S.look = hero_look(look)
        body, arm = [], []
        for an, frames in S.anims.items():
            for P in frames:
                (gb, _), J = bake_frame(S, P, ALL - ARM)
                (ga, _), _ = bake_frame(S, P, ARM)
                js = joints_of(S, J, P)
                body.append((CLIPS.index(an), gb, js)); arm.append((CLIPS.index(an), ga, js))
        emit_sheet(out, f'HERO_{look.upper()}', S, body, S.cx, S.gy)
        emit_sheet(out, f'HERO_{look.upper()}_ARM', S, arm, S.cx, S.gy)
        # the arm alone, held straight out at 16 angles (0 = down, turning forward), anchored at the shoulder
        aims = []
        P0 = pose()
        for k in range(16):
            P = dict(P0, aN=k / 16 * 2 * math.pi, eN=0.0)
            (ga, _), J = bake_frame(S, P, ARM)
            aims.append((0, ga, [J['shN']] * len(JOINTS)))
        sh = bake_frame(S, P0, ARM)[1]['shN']
        emit_sheet(out, f'HERO_{look.upper()}_AIM', S, aims, sh[0], sh[1])


def emit():
    out = ['// Generated by tools/anim.py - edit there, then: python tools/anim.py emit > sprites_anim.h',
           '#pragma once', '#include "sprites_hd.h"', '',
           '// Frame-by-frame character animation. A sheet holds every frame of a creature, stacked top to bottom, each fw x fh',
           '// pixels (half a world unit each), drawn facing right; (ax, ay) is the point between its feet on the ground. A pixel',
           '// is an index into HD_ALPHABET (\'.\' clear) and its palette, alpha 254/253 as in sprites_hd.h. joints: per frame, the',
           '// rig joints (rig.cpp J_*) in pixels from the anchor - where wounds sit, and where its corpse starts from.',
           'enum AnimClip { ' + ', '.join('AC_' + c.upper() for c in CLIPS) + ', AC_COUNT };',
           'struct AnimSheet { int fw, fh; float ax, ay; int frames; const Color* pal; const char* px; unsigned char clip[AC_COUNT][2]; const float* joints; };', '']
    for name, mk in SPECS.items():
        S = mk()
        if name == 'hero':
            emit_hero(out, S)
            continue
        frames = []
        for an, fr in S.anims.items():
            for P in fr:
                (gr, _), J = bake_frame(S, P)
                frames.append((CLIPS.index(an), gr, joints_of(S, J, P)))
        if S.kind != 'none': emit_parts(out, name.upper(), S)
        emit_sheet(out, name.upper(), S, frames, S.cx, S.gy)
    return '\n'.join(out) + '\n'


if __name__ == '__main__':
    if sys.argv[1] == 'preview': preview(sys.argv[2], sys.argv[3:])
    elif sys.argv[1] == 'emit': sys.stdout.write(emit())

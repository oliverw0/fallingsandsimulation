# The Viking axeman, Dead Cells style (.claude/skills/dead-cells-rig/SKILL.md). The body now comes from Blender (viking3d.py);
# the capsule build below is the fallback when Blender is missing. Canvas.render is still the shader for both.
# Lit 3D-ish primitives (tapered capsules, flat polygons) on a skeleton are rasterised with a z-buffer, then shaded from
# a 5-step ramp per material: banded light from the upper left, a cool rim on the back edge, a dark outline, dark lines
# where a nearer part crosses a farther one. No anti-aliasing: every pixel is a ramp colour. Everything character-specific
# (ramps, bone lengths, sizes) is data below; the renderer knows nothing about Vikings.
#
#   python tools/viking.py preview <dir>        sheets + GIFs at 3x   (scale 1.0 = the reference art, 65 px tall)
import math, sys, os
import numpy as np
from PIL import Image, ImageDraw

# ---------------------------------------------------------------- ramps (shadow -> highlight)
def C(*a): return tuple(a)
RAMPS = {
    'tunic':      [C(46, 24, 56), C(128, 66, 36), C(206, 132, 38), C(246, 190, 70), C(255, 236, 160)],
    'tunic_trim': [C(30, 16, 40), C(84, 40, 36), C(140, 74, 34), C(180, 104, 40), C(214, 140, 60)],
    'steel':      [C(22, 22, 40), C(56, 60, 84), C(102, 108, 132), C(152, 160, 180), C(210, 218, 232)],
    'helm':       [C(50, 50, 58), C(76, 76, 86), C(108, 108, 118), C(152, 152, 162), C(206, 206, 216)],   # his own steel: fixed colours
    'tunic_w':    [C(44, 28, 50), C(104, 68, 40), C(168, 124, 52), C(208, 168, 92), C(236, 212, 150)],       # the tunic, faded by weather
    'dirt':       [C(24, 18, 22), C(56, 42, 38), C(86, 66, 52), C(114, 90, 68), C(142, 116, 88)],             # mud and soot worked into cloth and leather
    'armour':     [C(24, 24, 30), C(66, 66, 76), C(112, 112, 124), C(160, 160, 172), C(214, 214, 226)],       # worn armour: takes the armour metal's colour in game (tag 3)
    'belt':       [C(28, 14, 16), C(60, 32, 26), C(92, 52, 36), C(122, 76, 50), C(152, 102, 66)],
    'pants':      [C(12, 8, 22), C(30, 22, 46), C(50, 38, 70), C(74, 58, 94), C(104, 88, 122)],
    'wrap':       [C(34, 24, 36), C(86, 70, 70), C(140, 122, 108), C(186, 170, 148), C(222, 210, 186)],
    'boot':       [C(10, 6, 14), C(28, 18, 24), C(50, 32, 32), C(76, 50, 44), C(108, 76, 60)],
    'skin':       [C(70, 36, 36), C(130, 80, 60), C(184, 124, 90), C(214, 156, 116), C(232, 184, 140)],
    'beard':      [C(44, 20, 22), C(120, 54, 24), C(186, 98, 36), C(226, 148, 60), C(252, 200, 110)],
    'fur':        [C(18, 12, 18), C(44, 30, 30), C(80, 56, 46), C(118, 88, 68), C(158, 126, 98)],
    'wood':       [C(28, 14, 16), C(70, 36, 28), C(112, 62, 40), C(150, 92, 56), C(186, 126, 78)],
    'glove':      [C(14, 8, 20), C(38, 24, 46), C(62, 42, 72), C(92, 66, 102), C(124, 96, 132)],
    'mustard':    [C(70, 38, 28), C(134, 82, 26), C(192, 132, 30), C(222, 168, 52), C(240, 202, 106)],   # the hero's tunic (user's reference art)
    'trim':       [C(38, 36, 30), C(78, 72, 52), C(120, 108, 76), C(150, 138, 102), C(178, 166, 130)],         # grey-green collar and cuffs
    'hair':       [C(28, 14, 14), C(66, 36, 24), C(104, 62, 36), C(142, 94, 56), C(178, 130, 84)],
    'trews':      [C(28, 28, 30), C(54, 54, 48), C(86, 84, 68), C(110, 106, 84), C(134, 128, 102)],           # grey-olive wool
    'leather':    [C(24, 14, 16), C(52, 30, 28), C(82, 50, 38), C(110, 72, 50), C(138, 96, 66)],            # tall riding boots, bracers
    'gem':        [C(36, 36, 48), C(104, 104, 120), C(168, 168, 184), C(222, 222, 236), C(255, 255, 255)],
}
MATS = list(RAMPS)
MI = {m: i for i, m in enumerate(MATS)}
TAGS = {'steel': 1, 'gem': 2, 'armour': 3}              # tag 1 takes the weapon's metal colour in game, tag 2 the staff gem's
RIM = (110, 236, 255)
OUTLINE_MODE = 'tint'                       # 'tint': the outline is the edge colour darkened (not black); 'black': the old hard outline
OUTLINE_KEEP = 0.58                         # how much of the edge colour a tinted outline keeps
DEPTH_LINE = 0.28                           # how far the inner depth lines go toward the outline colour
SMOOTH = 0                                  # passes of normal smoothing across touching parts: shading flows over joints instead of restarting on every capsule (foes use it)
RIM_ON = False                              # the cool back-light is off (user: no blue tint)
OUTLINE = (14, 8, 22)
EYE = (255, 150, 40); EYE_CORE = (255, 236, 170)
LIGHT = np.array([-0.55, -0.65, 0.55]); LIGHT /= np.linalg.norm(LIGHT)
GLINT = np.array([-0.3, -0.45, 0.84]); GLINT /= np.linalg.norm(GLINT)


def dirv(deg): a = math.radians(deg); return np.array([math.cos(a), math.sin(a)])
def V(x, y): return np.array([float(x), float(y)])


# ---------------------------------------------------------------- the renderer
class Canvas:
    def __init__(s, w, h, ground, scale=1.0):
        s.w, s.h, s.ground, s.S = w, h, ground, scale
        s.z = np.full((h, w), -1e9)
        s.n = np.zeros((h, w, 3))
        s.mat = np.full((h, w), -1, int)
        s.aux = np.zeros((h, w), int)
        s.obj = np.full((h, w), -1, int)
        s.dark = np.zeros((h, w), int)
        s.glint = np.zeros((h, w), bool)
        s.eye = {}
        s.nobj = 0
        s.X, s.Y = np.meshgrid(np.arange(w) + 0.5, np.arange(h) + 0.5)

    def capsule(s, A, B, rA, rB, zA, zB, mat, tex=None, caps=True, clip=None, dark=0, obj=None, local=False):
        """A tapered capsule. Spheres are capsules with A = B. tex(t, nrm, px, py) -> (aux offset, material override or None).
        local=True hands tex and clip the part's own coordinates (u along the bone from A, v across it, in reference units)
        instead of canvas pixels, so their noise and cuts ride on the limb rather than crawling over it as it moves."""
        if obj is None: s.nobj += 1; obj = s.nobj
        A, B = V(*A), V(*B)
        ab = B - A
        L2 = float(ab @ ab)
        R = max(rA, rB) + 1
        x0, x1 = int(max(0, math.floor(min(A[0], B[0]) - R))), int(min(s.w, math.ceil(max(A[0], B[0]) + R)))
        y0, y1 = int(max(0, math.floor(min(A[1], B[1]) - R))), int(min(s.h, math.ceil(max(A[1], B[1]) + R)))
        if x1 <= x0 or y1 <= y0: return
        X, Y = s.X[y0:y1, x0:x1], s.Y[y0:y1, x0:x1]
        if L2 < 1e-9: traw = np.zeros_like(X)
        else: traw = ((X - A[0]) * ab[0] + (Y - A[1]) * ab[1]) / L2
        t = np.clip(traw, 0, 1)
        r = rA + (rB - rA) * t
        cx, cy = A[0] + ab[0] * t, A[1] + ab[1] * t
        dx, dy = X - cx, Y - cy
        d = np.hypot(dx, dy)
        inside = d <= r
        if not caps: inside &= (traw >= 0) & (traw <= 1)
        TX, TY = X, Y
        if local:
            px_, py_ = X - A[0], Y - A[1]
            if L2 < 1e-9: TX, TY = px_ / s.S, py_ / s.S
            else:
                ax, ay = ab / math.sqrt(L2)
                TX, TY = (px_ * ax + py_ * ay) / s.S, (py_ * ax - px_ * ay) / s.S
        if clip is not None: inside &= clip(TX, TY)
        if not inside.any(): return
        rr = np.maximum(r, 1e-6)
        nx, ny = dx / rr, dy / rr
        nz = np.sqrt(np.clip(1 - (d / rr) ** 2, 0, 1))
        depth = zA + (zB - zA) * t + nz * r
        aux = np.zeros(X.shape, int); mo = None
        if tex:
            aux, mo = tex(t, (nx, ny, nz), TX, TY)
            aux = np.broadcast_to(aux, X.shape).astype(int)
        mat_ = MI[mat] if mo is None else np.where(mo >= 0, mo, MI[mat])
        sl = (slice(y0, y1), slice(x0, x1))
        sub = _View(s, sl)
        sub.write(inside, depth, np.stack([nx, ny, nz], -1), mat_, aux, dark, obj)

    def poly(s, pts, z, mat, normal=(0.0, 0.0, 1.0), aux_fn=None, dark=0, obj=None, tex=None, zfn=None, nfn=None):
        if obj is None: s.nobj += 1; obj = s.nobj
        im = Image.new('L', (s.w, s.h), 0)
        ImageDraw.Draw(im).polygon([(float(p[0]), float(p[1])) for p in pts], fill=1)
        m = np.array(im, bool)
        if not m.any(): return
        nrm = np.zeros((s.h, s.w, 3)); nrm[...] = normal
        if nfn is not None: nrm = nfn(s.X, s.Y)
        aux = np.zeros((s.h, s.w), int); mo = None
        if aux_fn is not None:
            a = aux_fn(s.X, s.Y)
            if isinstance(a, tuple): aux, mo = a[0], a[1]
            else: aux = a
        if tex: aux, mo = tex(None, (nrm[..., 0], nrm[..., 1], nrm[..., 2]), s.X, s.Y)
        mat_ = MI[mat] if mo is None else np.where(mo >= 0, mo, MI[mat])
        depth = np.full((s.h, s.w), float(z)) if zfn is None else zfn(s.X, s.Y)
        _View(s, (slice(0, s.h), slice(0, s.w))).write(m, depth, nrm, mat_, np.broadcast_to(aux, (s.h, s.w)).astype(int), dark, obj)

    def smoothed_normals(s, passes):
        """The normal buffer blended with its neighbours where the surface is continuous (close in depth): a limb's shading runs on through
        the knee, the arm into the shoulder, the head into the neck - no more beads strung on a string."""
        h, w = s.h, s.w
        has = s.mat >= 0
        n = s.n.copy()
        zthr = 5.0 * s.S
        pad = lambda a, v: np.pad(a, [(1, 1), (1, 1)] + [(0, 0)] * (a.ndim - 2), constant_values=v)
        zp, hp = pad(np.where(has, s.z, -1e9), -1e9), pad(has, False)
        for _ in range(passes):
            np_ = pad(n, 0.0)
            acc, wsum = n * 2.0, np.full((h, w), 2.0)
            for dy, dx in ((0, 1), (0, -1), (1, 0), (-1, 0), (1, 1), (-1, -1), (1, -1), (-1, 1)):
                sl = (slice(1 + dy, 1 + dy + h), slice(1 + dx, 1 + dx + w))
                wt = (hp[sl] & has & (np.abs(zp[sl] - s.z) < zthr)) * (1.0 if dy == 0 or dx == 0 else 0.6)
                acc += wt[..., None] * np_[sl]
                wsum += wt
            n = acc / wsum[..., None]
            n /= np.maximum(np.linalg.norm(n, axis=-1, keepdims=True), 1e-6)
            n = np.where(has[..., None], n, 0.0)
        return n

    # ---- shading
    def render(s, outline=True, rim_side=1, floor_clip=True):
        """-> (rgb[h][w][3], tag[h][w], opaque mask). rim_side: +1 puts the rim on right-facing edges, -1 on left-facing."""
        h, w = s.h, s.w
        has = s.mat >= 0
        n = s.smoothed_normals(SMOOTH) if SMOOTH else s.n
        I = 0.18 + 0.82 * np.maximum(0, (n * LIGHT).sum(-1))
        band = np.clip(np.floor(I * 4.6).astype(int) + s.aux - s.dark, 0, 4)
        rgb = np.zeros((h, w, 3), int)
        tag = np.zeros((h, w), int)
        for mi, name in enumerate(MATS):
            m = has & (s.mat == mi)
            if not m.any(): continue
            ramp = np.array(RAMPS[name])
            rgb[m] = ramp[band[m]]
            if name in TAGS: tag[m] = TAGS[name]
            if name in ('steel', 'helm', 'armour'):
                g = m & ((n * GLINT).sum(-1) > 0.975)
                rgb[g] = (236, 244, 255)
        # rim light on the back edge
        if RIM_ON:
            rimm = has & (rim_side * n[..., 0] > 0.62) & (n[..., 2] < 0.55)
            rgb[rimm] = (0.35 * rgb[rimm] + 0.65 * np.array(RIM)).astype(int)
            tag[rimm] = 0
        # inner depth lines: where a much nearer part of another object borders this pixel
        thr = 5.0 * s.S
        dl = np.zeros((h, w), bool)
        for dy, dx in ((0, 1), (0, -1), (1, 0), (-1, 0)):
            sy0, sy1 = max(0, -dy), min(h, h - dy); sx0, sx1 = max(0, -dx), min(w, w - dx)
            a = (slice(sy0, sy1), slice(sx0, sx1)); b = (slice(sy0 + dy, sy1 + dy), slice(sx0 + dx, sx1 + dx))
            near = has[b] & has[a] & (s.obj[b] != s.obj[a]) & (s.z[b] - s.z[a] > thr)
            dl[a] |= near
        rgb[dl] = (np.round(((1 - DEPTH_LINE) * rgb[dl] + DEPTH_LINE * np.array(OUTLINE)) / 10) * 10).astype(int)    # (snapped to a coarse grid: few extra colours)
        # the eye is a mark painted over the shading
        for (x, y), c in s.eye.items():
            if 0 <= x < w and 0 <= y < h and has[y, x]: rgb[y, x] = c; tag[y, x] = 0
        opaque = has.copy()
        if floor_clip:
            opaque[int(s.ground) + 2:] = False
        if outline:
            nb = np.zeros((h, w), bool)
            nb[1:] |= opaque[:-1]; nb[:-1] |= opaque[1:]; nb[:, 1:] |= opaque[:, :-1]; nb[:, :-1] |= opaque[:, 1:]
            edge = nb & ~opaque
            if floor_clip: edge[int(s.ground) + 2:] = False
            if OUTLINE_MODE == 'black':
                rgb[edge] = OUTLINE
            else:   # selective outlining: each outline pixel is its neighbours' colour, darkened - a shadow edge, not a black line
                acc = np.zeros((h, w, 3)); cnt = np.zeros((h, w))
                src = np.where(opaque[..., None], rgb, 0).astype(float)
                for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    sh = np.zeros_like(src); sm = np.zeros((h, w))
                    ys, yd = (slice(0, h - 1), slice(1, h)) if dy == 1 else ((slice(1, h), slice(0, h - 1)) if dy == -1 else (slice(0, h), slice(0, h)))
                    xs, xd = (slice(0, w - 1), slice(1, w)) if dx == 1 else ((slice(1, w), slice(0, w - 1)) if dx == -1 else (slice(0, w), slice(0, w)))
                    sh[yd, xd] = src[ys, xs]; sm[yd, xd] = opaque[ys, xs]
                    acc += sh; cnt += sm
                mean = acc / np.maximum(cnt, 1)[..., None]
                col = OUTLINE_KEEP * 0.75 * mean + (1 - OUTLINE_KEEP * 0.75) * 0.28 * np.array(OUTLINE) + 6
                rgb[edge] = (np.round(col[edge] / 10) * 10).astype(int)
            tag[edge] = 0
            opaque = opaque | edge
        return rgb, tag, opaque


class _View:
    """Writes a sub-rectangle of a Canvas's buffers."""
    def __init__(v, c, sl): v.c, v.sl = c, sl
    def write(v, m, depth, n, mat, aux, dark, obj):
        c, sl = v.c, v.sl
        z = c.z[sl]
        m = m & (depth > z)
        if not m.any(): return
        z[m] = depth[m]
        nn = c.n[sl]; nn[m] = n[m]
        mm = c.mat[sl]; mm[m] = mat if np.isscalar(mat) else np.broadcast_to(mat, m.shape)[m]
        aa = c.aux[sl]; aa[m] = aux if np.isscalar(aux) else np.broadcast_to(aux, m.shape)[m]
        c.dark[sl][m] = dark
        c.obj[sl][m] = obj


# ---------------------------------------------------------------- drawing in the character's local space
class Draw:
    """Local space: x forward (+ = the way he faces), y down, ground at y = 0, in units of the reference art. `f` mirrors it;
    `S` scales it to the sprite size, z included; `rot` turns the whole figure (radians, clockwise) about `pivot` - the geometry
    turns and is then shaded, so the light stays fixed."""
    def __init__(d, cv, ox, f=1, S=1.0, rot=0.0, pivot=(0.0, -30.0)):
        d.cv, d.ox, d.f, d.S, d.rot, d.pivot = cv, ox, f, S, rot, V(*pivot)
        d.cs, d.sn = math.cos(rot), math.sin(rot)

    def P(d, p):
        q = V(*p) - d.pivot
        x, y = d.pivot[0] + q[0] * d.cs - q[1] * d.sn, d.pivot[1] + q[0] * d.sn + q[1] * d.cs
        return (d.ox + d.f * x * d.S, d.cv.ground + y * d.S)

    def inv(d, X, Y):
        """Canvas coordinates -> local coordinates (arrays)."""
        x, y = (X - d.ox) / (d.S * d.f), (Y - d.cv.ground) / d.S
        qx, qy = x - d.pivot[0], y - d.pivot[1]
        return d.pivot[0] + qx * d.cs + qy * d.sn, d.pivot[1] - qx * d.sn + qy * d.cs

    def cap(d, A, B, rA, rB, zA, zB, mat, **k):
        S = d.S
        d.cv.capsule(d.P(A), d.P(B), rA * S, rB * S, zA * S, zB * S, mat, **k)

    def ball(d, A, r, z, mat, **k): d.cap(A, A, r, r, z, z, mat, **k)

    def poly(d, pts, z, mat, nfn=None, **k):
        if nfn is not None:
            def f(X, Y):
                n = nfn(*d.inv(X, Y))
                nx, ny = n[..., 0] * d.cs - n[..., 1] * d.sn, n[..., 0] * d.sn + n[..., 1] * d.cs
                return np.stack([nx * d.f, ny, n[..., 2]], -1)
            k['nfn'] = f
        d.cv.poly([d.P(p) for p in pts], z * d.S, mat, **k)


def two_bone(R, T, l1, l2, sign):
    """The middle joint of a two-bone chain from R reaching for T (clamped to what it can reach)."""
    R, T = V(*R), V(*T)
    v = T - R
    dist = float(np.hypot(*v)) + 1e-9
    dist = min(max(dist, abs(l1 - l2) + 1e-3), l1 + l2 - 1e-3)
    a = math.atan2(v[1], v[0]) + sign * math.acos(max(-1, min(1, (l1 * l1 + dist * dist - l2 * l2) / (2 * l1 * dist))))
    return R + l1 * np.array([math.cos(a), math.sin(a)])

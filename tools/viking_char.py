# The Viking axeman as data + a build function: pose -> primitives (tools/viking.py renders them).
# Local space: x forward, y down, ground at y = 0, reference-art units (the sprite scale is applied by Draw).
import math, os, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from viking import *

BONE = dict(torso=23, thigh=15, shin=14, uarm=12, farm=11)
HIP_REST = 28.0   # the hip rests this far above the ground


# ---------------------------------------------------------------- texture callbacks: (t, normal, X, Y) -> (tone offset, material override)
def tex_fur(t, n, X, Y):
    h = ((X * 7.0 + Y * 13.0) % 5) / 5.0
    cl = ((np.floor(X / 2) * 3 + np.floor(Y / 2) * 5) % 4) / 4.0
    return np.where(cl > 0.7, 1, np.where(h < 0.25, -1, 0)), None


def tex_wraps(t, n, X, Y):
    """Pale linen leg wraps over the shin, the trouser showing above and below."""
    band = ((t * 9).astype(int) % 2 == 0)
    wrap = (t > 0.08) & (t < 0.92)
    return np.where(wrap & band, 1, 0), np.where(wrap, MI['wrap'], -1)


def cuff(frac0, frac1, mat):
    def tex(t, n, X, Y):
        m = (t >= frac0) & (t <= frac1)
        return np.zeros_like(X, int), np.where(m, MI[mat], -1)
    return tex


def tex_hem(t, n, X, Y):
    return np.where(t > 0.86, -2, np.where(t > 0.8, -1, 0)), np.where(t > 0.88, MI['tunic_trim'], -1)


def tex_belt_band(t, n, X, Y):
    return np.zeros_like(X, int), np.where((t > 0.04) & (t < 0.16), MI['belt'], -1)


def tex_braid(t, n, X, Y):
    return np.where((np.floor(t * 8) % 2) == 0, 1, 0), None


def binding(t0, t1):
    def tex(t, n, X, Y):
        g = (t > t0) & (t < t1)
        return np.where(g & (((X + Y) % 2) < 1), 0, np.where(g, -1, 0)), np.where(g, MI['belt'], -1)
    return tex


def hnoise(X, Y, seed=0):
    """A cheap deterministic per-pixel hash in 0..1 (arrays in, arrays out)."""
    n = np.sin(X * 12.9898 + Y * 78.233 + seed * 37.719) * 43758.5453
    return n - np.floor(n)


def blobs(X, Y, seed, cell=2):
    """Clumped noise: the same hash over cell x cell blocks."""
    return hnoise(np.floor(X / cell), np.floor(Y / cell), seed)


def pose(**k):
    p = dict(hip=(0.0, -HIP_REST), lean=-88.0, head=-86.0,
             fn=(7.0, 0.0), ff=(-8.0, 0.0),                # ankle targets, near and far (y 0 = the ground)
             hn=(10.0, -34.0), hf=(-1.0, -29.0),           # wrist targets
             held=None,                                    # a held weapon: dict(type, g, ang, zg, zh)
             skirt=0.0, beard=0.0, eye=False, kn=-1, ke=1, rot=0.0, pivot=(0.0, -30.0), toe=(0.0, 0.0),
             nobody=False, far_arm_z=-9.0)
    p.update(k)
    return p


# The bearded axe head (the user's outline), as (v out along the cutting side, u along the haft; 0 = the haft's tip): a long flat
# top with a poll behind the haft, a spike at the far top corner, a cutting edge running down parallel to the haft with a little
# barb, and a long hooked beard hanging below.
AXE_HEAD = [(-3.1, 0.0), (9.1, 0.0), (11.0, 1.4), (14.4, 2.8), (14.4, -2.8), (15.3, -2.8), (15.3, -4.5), (14.4, -4.5), (14.4, -11.5),
            (13.0, -13.6), (11.6, -15.3), (10.0, -17.2), (9.0, -16.4), (9.8, -8.0), (8.6, -6.8), (7.0, -5.7), (1.8, -5.7), (-3.1, -4.5)]


# ---------------------------------------------------------------- weapons (grip g, direction angle in degrees, 0 = forward, -90 = up)
def draw_held(d, H, k=1.0):
    """A weapon in the pose H = dict(type, g, ang, zg, zh). `k` thickens it (radii and blade widths) for icons: 1.0 is the weapon as
    the Viking holds it, ~1.7 is chunky enough to read as an item on the ground or in the hotbar."""
    t = H['type']
    g, a = V(*H['g']), dirv(H['ang'])
    s = np.array([-a[1], a[0]])               # +90 degrees from the direction: the cutting edge / the underside
    zg, zh = H.get('zg', 12.0), H.get('zh', 12.0)
    pt = lambda base, u, v: base + a * u + s * (v * k)
    if t == 'axe':
        fo = H.get('fore', 1.0)       # a horizontal sweep seen side-on: the weapon turns toward or away from the viewer and shortens
        L = 48 * fo
        head = g + a * L
        hk = 1 + 0.55 * (k - 1)
        d.cap(g - a * 4, head - a * 1.0, 1.9 * k, 1.9 * k, zg, zh, 'wood', tex=binding(0.0, 0.2))
        d.ball(g - a * 4.8, 2.4 * k, zg, 'steel')
        pts = [head + a * (u * hk * fo) + s * (v * hk) for v, u in AXE_HEAD]
        d.poly(pts, zh, 'steel', nfn=lambda lx, ly: _tilt(head, a, s, lx, ly, 6.0, 0.5, 0.5), aux_fn=lambda X, Y: _edge_aux(d, head, a, s, X, Y, 12.0 * hk))
    elif t == 'sword':
        # a Norse sword, weathered: a broad parallel-edged blade with a fuller and a rounded point, nicked along the edge and
        # pitted; a short straight guard, a leather-wrapped grip and a lobed pommel
        fo = H.get('fore', 1.0)       # a level sweep seen side-on: the blade turns toward / away from the viewer and shortens
        d.cap(g - a * 4.5 * fo, g + a * 1.4 * fo, 1.3 * k, 1.3 * k, zg, zg, 'belt', tex=binding(-1, 2))
        d.ball(g - a * 5.6 * fo, 1.9 * k, zg, 'steel')
        d.ball(g - a * 5.6 * fo + s * 1.7 * k, 1.1 * k, zg - 0.5, 'steel')
        d.ball(g - a * 5.6 * fo - s * 1.7 * k, 1.1 * k, zg - 0.5, 'steel')
        d.cap(g + a * 1.8 * fo + s * 4.6 * k, g + a * 1.8 * fo - s * 4.6 * k, 1.0 * k, 1.0 * k, zg, zg, 'steel')
        pt = lambda base, u, v: base + a * (u * fo) + s * (v * k)
        w = 1.7
        top = [(2, w), (12, w), (12.7, w - 0.5), (13.4, w), (22, w), (22.6, w - 0.6), (23.3, w), (32, w), (32.5, w - 0.5), (33.2, w), (39, w), (43, w * 0.7), (46, w * 0.3), (47, 0)]
        bot = [(46, -w * 0.3), (43, -w * 0.7), (39, -w), (27, -w), (27.6, -w + 0.5), (28.2, -w), (14, -w), (2, -w)]
        d.poly([pt(g, u, vv) for u, vv in top + bot], zh, 'steel', aux_fn=lambda X, Y: _blade_faces(d, g, a, s, X, Y, 0.5, True))
    elif t == 'dagger':
        # a Norse seax: a broad single-edged blade, the spine dead straight then dropping to a clipped point,
        # a short oval guard, a plain grip and a round bronze pommel
        d.cap(g - a * 4.5, g + a * 1.2, 1.2 * k, 1.2 * k, zg, zg, 'belt', tex=binding(-1, 2))
        d.ball(g - a * 5.4, 1.8 * k, zg, 'steel')
        d.cap(g + a * 1.6 + s * 2.4 * k, g + a * 1.6 - s * 1.8 * k, 1.0 * k, 1.0 * k, zg, zg, 'steel')
        sp, ed = -2.0, 2.4      # the spine's side and the edge's side of the blade's centre line
        blade = [(2, sp), (16, sp), (18.5, sp + 0.7), (22, 0.1), (20, 1.3), (16, ed - 0.3), (13, ed), (2, ed)]
        d.poly([pt(g, u, vv) for u, vv in blade], zh, 'steel', aux_fn=lambda X, Y: _blade_faces(d, g, a, s, X, Y, 0.0))
    elif t == 'spear':
        d.cap(g - a * 24, g + a * 34, 1.4 * k, 1.4 * k, zg, zh, 'wood', tex=binding(0.38, 0.5))
        tip = g + a * 34
        d.cap(tip, tip + a * 3, 1.8 * k, 1.4 * k, zh, zh, 'steel')
        d.poly([pt(tip, 2, 2.4), pt(tip, 8, 3.4), pt(tip, 15, 0), pt(tip, 8, -3.4), pt(tip, 2, -2.4)], zh, 'steel', aux_fn=lambda X, Y: _blade_faces(d, tip, a, s, X, Y))
    elif t == 'mace':
        fo = H.get('fore', 1.0)
        head = g + a * 29 * fo
        d.cap(g - a * 4, head, 1.7 * k, 1.7 * k, zg, zh, 'wood', tex=binding(0.0, 0.2))
        d.ball(g - a * 4.8, 2.2 * k, zg, 'steel')
        hk = 1 + 0.35 * (k - 1)
        d.ball(head + a * 3 * fo, 5.6 * hk, zh, 'steel')
        for q in range(6):
            th = q * math.pi / 3
            p = head + a * 3 * fo + (a * 5.4 * math.cos(th) * fo + s * 5.4 * math.sin(th)) * hk
            d.ball(p, 1.8 * hk, zh + 2 * math.cos(th + 1), 'steel')
        d.cap(head + a * 7.5 * hk * fo, head + a * 11 * hk * fo, 1.5 * hk, 0.3, zh, zh, 'steel')
    elif t == 'pan':
        hk = 1 + 0.3 * (k - 1)
        d.cap(g - a * 3, g + a * 17, 1.4 * k, 1.6 * k, zg, zh, 'belt', tex=binding(-1, 0.3))
        c = g + a * 25
        ring = [c + (a * 8.5 * math.cos(q) + s * 8.0 * math.sin(q)) * hk for q in np.linspace(0, 2 * math.pi, 18, endpoint=False)]
        d.poly(ring, zh, 'steel', aux_fn=lambda X, Y: _pan_aux(d, c, a, s, X, Y))
    elif t in ('crossbow', 'hook'):
        big = t == 'crossbow'
        L = 20 if big else 9
        d.cap(g - a * (9 if big else 3), g + a * L, 2.0 * k, 1.6 * k, zg, zh, 'wood', tex=binding(0.34, 0.5) if big else None)
        if big:
            w = g + a * (L - 5)
            bow = [pt(w, 5, 11.5), pt(w, 2, 9), pt(w, -0.5, 4), pt(w, -1, 0), pt(w, -0.5, -4), pt(w, 2, -9), pt(w, 5, -11.5),
                   pt(w, 3.6, -11.8), pt(w, 1.2, -9.4), pt(w, 1, -4.6), pt(w, 1.8, 0), pt(w, 1, 4.6), pt(w, 1.2, 9.4), pt(w, 3.6, 11.8)]
            d.poly(bow, zh, 'steel')
            d.cap(pt(w, 4.6, 11), pt(w, -3.5, 0), 0.4 * k, 0.4 * k, zh + 1, zh + 1, 'wrap')
            d.cap(pt(w, 4.6, -11), pt(w, -3.5, 0), 0.4 * k, 0.4 * k, zh + 1, zh + 1, 'wrap')
            d.cap(g - a * 2, g + a * (L + 4), 0.9 * k, 0.5 * k, zh + 1.5, zh + 1.5, 'wood')
            d.cap(g + a * (L + 3), g + a * (L + 6), 1.2 * k, 0.2, zh + 1.5, zh + 1.5, 'steel')
        else:
            d.cap(g + a * 2, g + a * 9, 2.4 * k, 1.2 * k, zh, zh, 'steel')
            d.cap(g + a * 8, g + a * 12, 1.8 * k, 0.6, zh, zh, 'steel')
    elif t == 'staff':
        top = g + a * 44
        d.cap(g - a * 18, top, 1.5 * k, 1.3 * k, zg, zh, 'wood', tex=binding(0.26, 0.34))
        d.cap(top - a * 3, top + a * 4, 2.6 * k, 1.2 * k, zh, zh, 'steel')
        d.ball(top + a * 3.5, 3.6 * k, zh + 1, 'gem')
        d.ball(top + a * 3.5 + V(-1, -1), 1.0 * k, zh + 6, 'gem', tex=lambda t_, n, X, Y: (np.full_like(X, 4, int), None))


def _tilt(head, a, s, lx, ly, vmid, k_u, k_v):
    """A gently domed surface: the normal leans toward the cutting edge and the tip, so the blade shades across its width."""
    u = (lx - head[0]) * a[0] + (ly - head[1]) * a[1]
    v = (lx - head[0]) * s[0] + (ly - head[1]) * s[1]
    nu, nv = np.clip(u / 14.0, -1, 1) * 0.3, np.clip((v - vmid) / 9.0, -1, 1) * 0.7
    nx, ny = nu * a[0] + nv * s[0], nu * a[1] + nv * s[1]
    nz = np.sqrt(np.clip(1 - nx * nx - ny * ny, 0.05, 1))
    return np.stack([nx, ny, nz], -1)


def _local(d, X, Y): return d.inv(X, Y)


def _edge_aux(d, head, a, s, X, Y, vedge):
    lx, ly = d.inv(X, Y)
    v = (lx - head[0]) * s[0] + (ly - head[1]) * s[1]
    return np.where(v > vedge, 2, np.where(v > vedge - 2.5, 1, np.where(v < 2.5, -1, 0)))


def _blade_faces(d, g, a, s, X, Y, fuller=0.5, pitted=False):
    lx, ly = d.inv(X, Y)
    v = (lx - g[0]) * s[0] + (ly - g[1]) * s[1]
    f = np.where(np.abs(v) < fuller, -1, np.where(v > 0, 1, 0))
    if pitted: f = np.where(hnoise(np.floor(X), np.floor(Y), 3) > 0.9, f - 1, f)     # pits and rust-bloom
    return f


def _pan_aux(d, c, a, s, X, Y):
    lx, ly = d.inv(X, Y)
    u, v = (lx - c[0]) * a[0] + (ly - c[1]) * a[1], (lx - c[0]) * s[0] + (ly - c[1]) * s[1]
    r = np.hypot(u / 8.5, v / 8.0)
    return np.where(r > 0.8, 1, np.where(r > 0.62, -1, 0))


# ---------------------------------------------------------------- the Viking
LOOKS = ('gambeson', 'mail', 'lamellar', 'plate')   # P['look']: what he wears over the tunic (the armour's metal tints it in game)


def armour_tex(look):
    """Texture callback for worn armour: (t, normal, X, Y) -> (tone offset, material override)."""
    def tex(t, n, X, Y):
        xi, yi = np.floor(X), np.floor(Y)
        if look == 1:   # mail: a checker of rings, row on row, the odd bright link
            return np.where(((xi + yi) % 2) == 0, 0, -1) + np.where(hnoise(xi, yi, 5) > 0.93, 1, 0), None
        if look == 2:   # lamellar: rows of small overlapping plates, seams staggered
            r = yi % 3
            seam = (((xi + (np.floor(yi / 3) % 2) * 2) % 4) == 0)
            return np.where(r == 0, 1, np.where(r == 2, -1, 0)) + np.where(seam, -1, 0), None
        r = yi % 8      # plate: broad bands with a bright edge, riveted
        return np.where(r == 0, -2, np.where(r == 1, 1, 0)) + np.where((xi % 6 == 0) & (r == 4), 2, 0), None
    return tex


def build(d, P):
    """Draw the Viking for pose P into Draw d (the z-buffer sorts it). He is weathered: faded, mud-stained, patched, scarred."""
    look = P.get('look', 0)
    hip = V(*P['hip'])
    lean = P['lean']
    C_ = hip + 23 * dirv(lean)
    N = C_ + 3 * dirv(lean)
    hd = N + 10.2 * dirv(P['head']) + V(1.2, 0)
    sh = {'n': C_ + V(0.5, 2.5), 'f': C_ + V(-2.5, 2.5)}
    hips = {'n': hip + V(1.0, 0), 'f': hip + V(-1.0, 0)}
    legs = {s: (hips[s], two_bone(hips[s], P['f' + s], BONE['thigh'], BONE['shin'], P['kn']), V(*P['f' + s])) for s in 'nf'}
    arms = {s: (sh[s], two_bone(sh[s], P['h' + s], BONE['uarm'], BONE['farm'], P['ke']), V(*P['h' + s])) for s in 'nf'}
    arm_tex = armour_tex(look)

    def dirt(X, Y, seed, thresh, mo=None):
        """Mud and soot: clumps where the noise beats `thresh` (an array or a number)."""
        m = blobs(X, Y, seed, 2) > thresh
        return np.where(m, MI['dirt'], -1 if mo is None else mo)

    # ---- the cloth: faded, with grime thickest toward the hem, quilting worn pale, a mended patch on the flank
    def tex_torso(t, n, X, Y):
        aux, mo = tex_belt_band(t, n, X, Y)
        if look:
            a2, _ = arm_tex(t, n, X, Y)
            return a2, mo
        xi, yi = np.floor(X), np.floor(Y)
        aux = aux + np.where(((xi + yi) % 5) == 0, -1, 0)
        mo = np.where(mo >= 0, mo, dirt(X, Y, 11, 0.8 + 0.6 * t))
        lx, ly = d.inv(X, Y)
        patch = (lx > -3.2) & (lx < 3.0) & (ly > hip[1] - 17) & (ly < hip[1] - 10.5)
        edge = patch & ((lx < -2.4) | (lx > 2.2) | (ly < hip[1] - 16.2) | (ly > hip[1] - 11.3))
        mo = np.where(patch, MI['wrap'], mo)
        aux = aux + np.where(patch & ~edge, -1, 0) + np.where(edge, -2, 0) + np.where(patch & ((xi + yi) % 3 == 0), -1, 0)   # a stitched patch
        return aux, mo

    def tex_skirt(t, n, X, Y):
        if look:
            a2, mo = arm_tex(t, n, X, Y)
            return a2, np.where(t > 0.9, MI['tunic_trim'], -1)
        aux, mo = tex_hem(t, n, X, Y)
        return aux, np.where(mo >= 0, mo, dirt(X, Y, 12, 0.92 - 0.34 * t))

    def tex_sleeve(t, n, X, Y):
        if look: return arm_tex(t, n, X, Y)
        return np.where(hnoise(np.floor(X), np.floor(Y), 13) > 0.94, -1, 0), dirt(X, Y, 14, 0.9)

    def tex_shin(t, n, X, Y):
        aux, mo = tex_wraps(t, n, X, Y)
        return aux, np.where(blobs(X, Y, 21, 2) > 0.7 + 0.5 * (1 - t), MI['dirt'], mo)   # the wraps muddied toward the boot

    def tex_boot(t, n, X, Y):
        return np.zeros_like(X, int), dirt(X, Y, 22, 0.76)

    def tex_mantle(t, n, X, Y):
        a2, mo = tex_fur(t, n, X, Y)
        return a2 - np.where(blobs(X, Y, 23, 3) > 0.78, 1, 0), mo

    def leg(s, z, dark):
        h_, k, a = legs[s]
        d.cap(h_, k, 5.2, 4.2, z, z, 'pants', dark=dark)
        d.cap(k, a, 4.2, 3.4, z, z, 'pants', dark=dark, tex=tex_shin)
        if look:   # armour over the thigh (chausses / cuisses), so the leg is covered by it, not stuck on top of it
            d.cap(h_, h_ + (k - h_) * 0.66, 5.9, 5.3, z + 1.0, z + 1.0, 'armour', dark=dark, tex=arm_tex)
        td = V(*P['toe']) if P['toe'] != (0.0, 0.0) else V(6.5, 1.0)
        d.cap(a + V(-1, -1), a + V(0, 1.0), 3.8, 3.4, z, z, 'boot', dark=dark, tex=tex_boot)
        d.cap(a + V(0, 1.0), a + td, 3.6, 2.6, z, z, 'boot', dark=dark, tex=tex_boot)
        d.cap(a + V(-1, -4), a + V(0, -1), 4.0, 3.8, z, z, 'boot', dark=dark, tex=tex_boot)

    def arm(s, z, dark):
        a, e, w = arms[s]
        d.cap(a, e, 4.6, 3.8, z, z, 'armour' if look else 'tunic_w', dark=dark, tex=tex_sleeve)
        d.cap(e, w, 3.8, 3.2, z, z, 'glove', dark=dark, tex=cuff(0.55, 0.85, 'belt'))
        fw = (w - e) / (np.hypot(*(w - e)) + 1e-9)
        d.ball(w + fw * 1.0, 3.7, z, 'glove', dark=dark)
        if look >= 2:   # a pauldron over the shoulder
            d.ball(a + V(0.4, -0.6), 5.4 if look == 3 else 4.8, z + 2.5, 'armour', dark=dark, tex=arm_tex)

    leg('f', -6, 1)
    arm('f', P['far_arm_z'], 1)
    if P['held']: draw_held(d, P['held'])
    s0 = hip + V(0, 2)
    s1 = s0 + 15.5 * dirv(90 + P['skirt'])
    ragged = (lambda X, Y: d.inv(X, Y)[1] <= s1[1] - 2.8 * hnoise(np.floor(d.inv(X, Y)[0] * 0.8), 7, 1)) if not look else None   # a torn, uneven hem
    d.cap(s0, s1, 9, 12, -4, -4, 'armour' if look else 'tunic_w', caps=False, tex=tex_skirt, clip=ragged)
    d.cap(hip, C_, 8.4, 10.2, 0, 0, 'armour' if look else 'tunic_w', tex=tex_torso)
    d.ball(hip + 0.1 * (C_ - hip) + V(4.0, 0.8), 1.5, 9, 'helm')
    d.ball(C_ + V(-4.2, 3.0), 7.8, 2.5, 'fur', tex=tex_mantle)
    d.cap(N + V(0.2, 2.0), hd + V(0.4, 2.0), 3.3, 3.0, 3, 3, 'skin')
    if look >= 2: d.cap(N + V(0.4, 1.4), N + V(0.4, 4.4), 4.6, 5.0, 5, 5, 'armour', tex=arm_tex)   # a gorget
    d.ball(hd + V(2.5, 2.5), 6.4, 4, 'skin')
    bs, bw = hd + V(3.5, 4.5), P['beard']
    def tex_beard(t, n, X, Y):
        a2, _ = tex_fur(t, n, X, Y)
        return a2, np.where(blobs(X, Y, 41, 1) > 0.84, MI['wrap'], -1)   # grey coming through the red
    d.cap(bs, bs + V(0.8 + bw, 8.0), 5.6, 2.2, 6, 6, 'beard', tex=tex_beard)
    d.cap(hd + V(4.0, 4.4), hd + V(8.4, 4.2), 1.9, 1.4, 8, 8, 'beard')
    d.cap(bs + V(0.5, 4.5), bs + V(1.4 + bw, 13.0), 1.4, 1.1, 8, 8, 'beard', tex=tex_braid)
    hy = hd[1] + 3.0
    peak = hd + V(0.9, -9.0)
    mid = hd + V(0.4, -4.6)
    def tex_helm(t, n, X, Y):
        xi, yi = np.floor(X), np.floor(Y)
        ly = d.inv(X, Y)[1]
        aux = np.where(ly > hd[1] + 0.2, -2, 0)
        aux = aux - np.where(blobs(X, Y, 31, 3) > 0.88, 1, 0) - np.where(hnoise(xi, yi, 32) > 0.97, 1, 0) + np.where(hnoise(xi, yi, 33) > 0.985, 2, 0)   # dents, scratches, a bright scuff
        return aux, np.where(blobs(X, Y, 34, 1) > 0.95, MI['belt'], -1)    # a few rust flecks
    d.cap(hd + V(0, -0.6), mid, 8.8, 7.0, 7, 7, 'helm', clip=lambda X, Y: d.inv(X, Y)[1] <= hy, tex=tex_helm)     # a rounded ogive dome...
    d.cap(mid, peak, 7.0, 2.4, 7, 7, 'helm', tex=tex_helm)                                                        # ...rising to a point
    for xo in (-6.2, -0.4, 5.6):         # the iron-and-bronze bands, brim to peak (thin, so the steel shows between them)
        d.cap(hd + V(xo, -0.2), peak + V(xo * 0.1, 0.9), 0.6, 0.45, 20, 20, 'belt')
    d.cap(hd + V(-8.6, 0.3), hd + V(8.6, 0.3), 1.1, 1.1, 20, 20, 'belt')                                            # the brim band
    d.ball(peak + V(0, 0.2), 1.1, 21, 'helm')                                                                        # the peak rivet
    d.cap(hd + V(8.6, -0.6), hd + V(9.4, 6.2), 1.2, 0.9, 14, 14, 'helm')                                            # the nasal guard, down over the nose
    d.cap(hd + V(1.4, 1.2), hd + V(3.2, 6.6), 2.7, 1.8, 13, 13, 'helm', tex=lambda t, n, X, Y: (np.where(hnoise(np.floor(X), np.floor(Y), 36) > 0.95, -1, 0), None))   # a cheek-plate
    ex, ey = d.P(hd + V(6.2, 2.2))
    d.cv.eye[(int(ex), int(ey))] = EYE_CORE
    d.cv.eye[(int(ex) - 1, int(ey))] = EYE
    for q in range(4):   # an old scar down the cheek
        sx, sy = d.P(hd + V(1.4 + q * 0.8, 1.2 + q * 1.3))
        d.cv.eye[(int(sx), int(sy))] = (176, 98, 92)
    leg('n', 6, 0)
    arm('n', 10, 0)


def render_pose(P, scale=1.0, w=150, h=150, gx=75, ground=132):
    """-> rgb, tag, opaque for one pose at the given scale. He faces right; the game flips the picture for left."""
    cv = Canvas(w, h, ground, scale)
    d = Draw(cv, gx, 1, scale, rot=math.radians(P['rot']), pivot=P['pivot'])
    build(d, P)
    return cv.render(rim_side=1)

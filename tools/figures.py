# Noita-style character art: the hero and the first stage's creatures, painted pixel by pixel on the native grid.
# The skeleton, poses, clips and the sprites_anim.h writer come from tools/anim.py; this file swaps in a different
# painter and replaces those creatures' specs, so it is now the one that emits the header:
#   python tools/figures.py emit > sprites_anim.h
#   python tools/figures.py preview <dir> [names]   native transparent sheets + 6x GIFs on black
#
# Style rules (from the brief):
# - no outlines: shapes read by colour against black
# - no anti-aliasing, gradients or glow; muted ramps of 5 tones, reused across materials
# - lit from the upper left: hard materials (metal, leather, fur, wood, bone, skin) get a 1 px lighter edge top and
#   left and a 1 px darker edge right and bottom; cloth gets the dark edge only, plus a few 1 px fold lines
# - chainmail is a checker of two neighbouring tones, the only dithering
# - cloth hems are ragged: each column hangs 0-7 px, like drips
# - faces are a near-black shadow under the hood or helm with 2-3 px of pale cyan eyes
# - chunky, slightly squat, oversized gear; three-quarter profile facing right; far limbs a tone darker
# Edges are worked out per (layer, material) region of the finished figure, and a whole limb is one layer, so an
# arm or leg reads as one piece, not a chain of segments.
import math, sys, os
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import anim
from anim import Fig, Body, Spec, pose, keys, lerp_pose, idle_cycle, walk_cycle, local, dir_, P2, hexc, REST, CLIPS


def R(*h): return [hexc(x) for x in h]


# ---------------------------------------------------------------- palette (dark -> light)
STEEL = R('#262931', '#3f434c', '#5c6372', '#798291', '#a3aeb6')
BROWN = R('#241b18', '#3b2c26', '#574338', '#75604e', '#927c66')  # fur, leather, wood
WINE = R('#2a141d', '#44202b', '#632c38', '#803a42', '#96484b')
BLUE = R('#22262f', '#353c4a', '#465064', '#59667c', '#6f7d92')
BONE = R('#4f4534', '#7d7059', '#a59a81', '#c7c2ad', '#dedaca')
SKIN = R('#4a3530', '#6e4f45', '#916b5a', '#a8806b', '#bb957d')  # the brief's two, stretched to five
EYES = R('#6fa3a8', '#9fd0cf', '#c9eeea')
FACE = hexc('#0e0e12')
# the stage's creatures, in the same muted key
OLIVE = R('#1f2318', '#30361f', '#454d2b', '#5c6539', '#737c4a')    # goblin hide
ASHFUR = R('#211f1d', '#35322e', '#4c4843', '#66615a', '#817b72')   # wolf
GOO = R('#18241a', '#253a26', '#355236', '#476c45', '#5f8a55')      # slime
BLOOD = R('#2a0c0e', '#4a1214', '#6a1a1a', '#862420', '#9a2e24')
METAL = R('#2b2b2b', '#444444', '#636363', '#868686', '#acacac')    # painted grey, recoloured to the armour's metal in game
OCHRE = R('#3a2a16', '#5e4522', '#82622e', '#a07c3a', '#b8944a')  # the hero's robe (the user's own design, in this key)
NIGHT = R('#17121b', '#231a28', '#2f2436', '#3d3045', '#4c3d55')  # his breeches and boots
SAFFRON = R('#4a3410', '#7a5818', '#b08a22', '#d6ab36', '#f0cc60')  # the hero's torn tunic: faded yellow, sun-bleached at the folds
RUSTHAIR = R('#3a1e10', '#64361a', '#8f5426', '#b57a3a', '#d89c58')   # his red-fair beard
GREYWOOL = R('#1f1c1a', '#322e2b', '#48423d', '#5f574f', '#78706a')   # trousers
RUNE = R('#5a5a5a', '#8a8a8a', '#c0c0c0', '#e0e0e0', '#ffffff')


class NMat:
    """A material: its ramp, how it's shaded (hard, mail, cloth, fur, flat), and its tag (1 metal recolour, 2 glow)."""
    def __init__(self, ramp, kind='hard', tag=0, base=2):
        self.r, self.kind, self.tag, self.base = ramp, kind, tag, base
        self.tex, self.glow, self.shine = None, False, 0  # what anim.Fig expects of a Mat


def hsh(x, y, s=0):
    h = (int(x) * 374761393 + int(y) * 668265263 + int(s) * 2246822519) & 0xffffffff
    h = ((h ^ (h >> 13)) * 1274126177) & 0xffffffff
    return ((h ^ (h >> 16)) & 0xffffff) / 16777216.0


LAYER = {'uarmN': 'armN', 'farmN': 'armN', 'uarmF': 'armF', 'farmF': 'armF', 'thighN': 'legN', 'shinN': 'legN', 'thighF': 'legF', 'shinF': 'legF'}


class NFig(Fig):
    def __init__(self, w, h):
        super().__init__(w, h)
        self.layer = None  # overrides the part's layer

    def add(self, d, mat, bev=2.0, org=(0, 0), grp=None, flat=False, dark=None, drip=None, layer=None, tone=None):
        s = super().add(d, mat, bev, org, grp, flat, dark)
        s.drip, s.tone = drip, tone
        s.layer = layer or self.layer or LAYER.get(self.part, self.part)
        return s

    def render(self, only=None, outline=False):
        g = self.g
        H, W = g.h, g.w
        own = np.full((H, W), -1)
        masks = []
        for k, s in enumerate(self.shapes):
            m = s.d < 0
            if s.drip is not None and m.any():  # a ragged hem: each column hangs 0-7 px below the cloth
                m = m.copy()
                ox = int(round(s.org[0]))
                for x in np.nonzero(m.any(axis=0))[0]:
                    b = np.nonzero(m[:, x])[0].max()
                    seed, most = s.drip if isinstance(s.drip, tuple) else (s.drip, 7)
                    n = int(hsh(x - ox, 7, seed) ** 1.7 * (most + 1))
                    m[b + 1:min(H, b + 1 + n), x] = True
            masks.append(m)
            if only and s.part not in only: continue
            own[m] = k
        regs = {}
        reg = np.array([regs.setdefault((s.layer, id(s.mat), s.dark), len(regs)) for s in self.shapes] + [-1])
        Rg = reg[own]
        pad = np.pad(Rg, 1, constant_values=-2)
        up, dn, lf, rt = pad[:-2, 1:-1], pad[2:, 1:-1], pad[1:-1, :-2], pad[1:-1, 2:]
        light = (up != Rg) | (lf != Rg)
        shade = (dn != Rg) | (rt != Rg)
        X, Y = np.meshgrid(np.arange(W), np.arange(H))
        out = [[None] * W for _ in range(H)]
        part = [[None] * W for _ in range(H)]
        tone = np.zeros((H, W), int)
        for k, s in enumerate(self.shapes):
            m = own == k
            if not m.any(): continue
            kind, base = s.mat.kind, (s.tone if s.tone is not None else s.mat.base) - s.dark
            ox, oy = int(round(s.org[0])), int(round(s.org[1]))
            t = np.full((H, W), base)
            if kind == 'mail': t = t - (((X - ox) + (Y - oy)) % 2)
            if kind == 'fur':  # sparse dark strokes, three pixels long, lying along the body
                hv = np.vectorize(lambda x, y: hsh(x, y, 3))((X - ox) // 3, Y - oy)
                t = t - (hv < 0.12)
            if kind == 'hair': t = t - (((X - ox) % 2 == 0) & (Y > oy + 1))  # strands
            if kind in ('hard', 'mail', 'fur', 'hair'):
                t = np.where(light & ~shade, base + 1, t)
                t = np.where(shade & ~light, base - 1, t)
            elif kind == 'cloth':
                ys, xs = np.nonzero(m)
                y0, y1 = ys.min(), ys.max()
                for x in range(xs.min() + 1, xs.max()):  # a few fold lines hanging from the waist down
                    if hsh(x - ox, 1, 11) > 0.2 or hsh(x - 1 - ox, 1, 11) <= 0.2: continue
                    top = y0 + int((y1 - y0) * (0.3 + 0.35 * hsh(x - ox, 2, 11)))
                    col = m[:, x] & (np.arange(H) >= top)
                    t[col, x] = base - 1
                t = np.where(shade, base - 1, t)
            tone[m] = np.clip(t[m], 0, len(s.mat.r) - 1)
        for j in range(H):
            for i in range(W):
                k = own[j][i]
                if k < 0: continue
                s = self.shapes[k]
                out[j][i] = (tuple(s.mat.r[tone[j][i]]), s.mat.tag)
                part[j][i] = s.part
        for x, y, c, p in self.marks:
            if only and p not in only: continue
            if 0 <= x < W and 0 <= y < H: out[y][x] = (tuple(c[:3]), c[3] if len(c) > 3 else 0); part[y][x] = p
        return out, part


# ---------------------------------------------------------------- shared painters
def limb(f, a, b, r0, r1, mat, **k): return f.cap(P2(a), P2(b), r0, r1, mat, **k)


def boot(f, ankle, ang, L, mat, h=3.0):
    """A chunky boot from the ankle, the toe along `ang` (pi/2 = forward, level)."""
    d = dir_(ang)
    up = np.array([d[1], -d[0]])
    heel = ankle - d * 1.8
    toe = ankle + d * L
    f.poly([P2(heel + up * h), P2(ankle + up * h + d * 1.0), P2(toe + up * 1.6), P2(toe + d * 0.8), P2(toe - up * 0.4), P2(heel - up * 0.4 - d * 0.4)], mat)


def face(f, H, fwd=1.0, w=3.6, h=4.2, eyes=2, dy=0.0):
    """The near-black shadow under a hood or helm, with pale cyan eyes."""
    c = H + np.array([1.6 * fwd, 0.8 + dy])
    f.ell(P2(c), w, h, NMat([FACE] * 5, 'flat'), layer='face')
    e = (int(math.floor(c[0] + 0.8)), int(math.floor(c[1] - 1.0)))
    f.marks.append((e[0], e[1], EYES[2], f.part))
    if eyes > 1: f.marks.append((e[0] + 2, e[1], EYES[1], f.part))
    if eyes > 2: f.marks.append((e[0] + 1, e[1] + 1, EYES[0], f.part))


def beard(f, H, mat, L=7.5, w=3.2):
    """A great beard spilling from under the face shadow down the chest, ragged at its end."""
    top = H + np.array([2.4, 3.2])
    f.poly([P2(top + np.array([-2.6, -0.6])), P2(top + np.array([w, -0.6])), P2(top + np.array([w + 0.4, L * 0.6])),
            P2(top + np.array([0.6, L])), P2(top + np.array([-2.0, L * 0.7]))], mat, drip=(5, 2), layer='beard')


# ---------------------------------------------------------------- the hero
HERO = Body(thigh=8.2, shin=8.0, torso=12.0, ua=6.8, fa=6.4, head=11.0, sw=3.0, hw=1.8, foot=3.6)


def hero_look(kind='wool'):
    """The five armour sets. Tag 1 (METAL) takes the armour metal's colour in game, tag 2 the rune glow."""
    # a rugged Norse warrior: a torn saffron tunic cut off at the elbows, a fur mantle, a baldric and a broad belt,
    # leather bracers on bare scarred forearms, wrapped shins over grey wool, heavy boots, a nasal spangenhelm, a red braided beard
    L = dict(kind=kind, tunic=NMat(SAFFRON, 'cloth'), sleeve=NMat(SAFFRON, 'cloth'), trouser=NMat(GREYWOOL, 'cloth', base=2),
             boot=NMat(BROWN, base=2), belt=NMat(BROWN, base=1), buckle=NMat(BONE), hand=NMat(SKIN, base=2), hood=None,
             beard=NMat(RUSTHAIR, 'hair', base=2), helm='spangen', over=None, robe=6.5,
             bare=NMat(SKIN, base=2), bracer=NMat(BROWN, base=2), mantle=NMat(BROWN, 'fur', base=2), wrap=NMat(BONE, 'cloth', base=1),
             baldric=NMat(BROWN, base=1))
    if kind == 'leather':
        L.update(over=NMat(BROWN))
    elif kind == 'mail':
        L.update(over=NMat(METAL, 'mail', tag=1), sleeve=NMat(METAL, 'mail', tag=1), hand=NMat(BROWN, base=1), helm='nasal', hood=None, bare=None, mantle=None)
    elif kind == 'lamellar':
        L.update(over=NMat(METAL, 'hard', tag=1), sleeve=NMat(METAL, 'mail', tag=1), hand=NMat(BROWN, base=1), helm='aventail', hood=None, bare=None, mantle=None,
                 trouser=NMat(BLUE, 'cloth', base=1))
    elif kind == 'scale':
        L.update(over=NMat(METAL, 'hard', tag=1), sleeve=NMat(METAL, 'mail', tag=1), hand=NMat(METAL, tag=1), helm='great', hood=None, bare=None, mantle=None,
                 trouser=NMat(BLUE, 'cloth', base=1))
    L['metal'] = NMat(METAL, tag=1)
    return L


def hero(fig, J, P, L=None):
    L = L or hero_look()
    g, H, n, p = fig.g, J['head'], J['neck'], J['pel']

    def arm(s):
        fig.part = 'uarm' + s
        limb(fig, J['sh' + s], J['el' + s], 3.2, 2.8, L['sleeve'], drip=(4, 2) if L['bare'] is not None else None)  # a sleeve torn off at the elbow
        fig.part = 'farm' + s
        if L['bare'] is not None:
            limb(fig, J['el' + s], J['ha' + s], 2.5, 2.2, L['bare'])  # bare, scarred forearm
            e, h = J['el' + s], J['ha' + s]
            limb(fig, e + (h - e) * 0.42, e + (h - e) * 0.92, 2.7, 2.5, L['bracer'], layer='bracer' + s)  # a leather bracer
        else:
            limb(fig, J['el' + s], J['ha' + s], 2.6, 2.3, L['sleeve'])
        fig.circle(P2(J['ha' + s] + dir_(J['fa' + s]) * 0.8), 2.3, L['hand'], layer='hand' + s)

    def leg(s):
        fig.part = 'thigh' + s
        limb(fig, J['hip' + s], J['kn' + s], 3.6, 3.0, L['trouser'])
        fig.part = 'shin' + s
        limb(fig, J['kn' + s], J['ft' + s], 3.0, 2.8, L['trouser'])
        ank, up = J['ft' + s], J['kn' + s] - J['ft' + s]
        if L.get('wrap') is not None:  # leg wraps over the shin
            k_, f_ = J['kn' + s], J['ft' + s]
            fig.cap(P2(k_ + (f_ - k_) * 0.2), P2(k_ + (f_ - k_) * 0.8), 3.2, 3.2, L['wrap'], layer='wrap' + s)
            for q in (0.35, 0.55, 0.75):
                m_ = k_ + (f_ - k_) * q
                fig.cap(P2(m_ + np.array([-3.0, 0.0])), P2(m_ + np.array([3.0, 0.0])), 0.5, 0.5, L['wrap'], layer='wrap' + s, tone=1)
        up = up / (np.linalg.norm(up) + 1e-9)
        fig.layer = 'boot' + s
        limb(fig, ank, ank + up * 2.4, 3.0, 3.1, L['boot'])
        boot(fig, ank, J['toe' + s], 4.6, L['boot'], h=3.2)
        fig.cap(P2(ank + up * 3.2 - np.array([up[1], -up[0]]) * 2.6), P2(ank + up * 3.2 + np.array([up[1], -up[0]]) * 2.6), 0.9, 0.9, L['boot'], tone=3)  # the turned-down cuff
        fig.layer = None

    fig.dark = 1
    arm('F')
    leg('F')
    fig.dark = 0
    leg('N')
    fig.part = 'torso'
    body = [P2(local(J, n, 3.6, -0.6)), P2(local(J, n, 7.2, 3.0)), P2(local(J, p, 6.6, -3.0)), P2(local(J, p, 6.0, 1.4)),
            P2(local(J, p, -5.6, 1.4)), P2(local(J, p, -6.0, -4)), P2(local(J, n, -6.2, 3.0)), P2(local(J, n, -3.4, -0.8))]
    fig.poly(body, L['tunic'])
    sw = P['cloth'] * 0.8
    rl = L.get('robe', 5.2)
    hem = [P2(local(J, p, 6.2, -1.0)), (p[0] + 7.4 + sw, p[1] + rl), (p[0] - 6.6 + sw, p[1] + rl), P2(local(J, p, -6.0, -1.0))]
    fig.poly(hem, L['tunic'], drip=(1, 5))  # the tunic's skirt, torn to ribbons
    fig.poly([P2(local(J, p, 1.0, -1.0)), (p[0] + 3.2 + sw, p[1] + rl + 3.5), (p[0] - 1.4 + sw, p[1] + rl + 2.0), P2(local(J, p, -2.4, -1.0))], L['tunic'], drip=(9, 5), layer='flap')  # a long torn flap
    if L.get('baldric') is not None and L['over'] is None:  # a leather baldric across the chest
        fig.cap(P2(local(J, n, -4.6, 2.6)), P2(local(J, p, 5.8, -1.6)), 1.1, 1.1, L['baldric'], layer='baldric', tone=1)
    if L['over'] is not None:  # a jerkin, byrnie or plate over it, to the hips
        fig.poly([P2(local(J, n, 2.8, 0.4)), P2(local(J, n, 6.8, 3.4)), P2(local(J, p, 6.2, -3.0)), P2(local(J, p, 6.0, 2.6)),
                  P2(local(J, p, -5.4, 2.6)), P2(local(J, p, -5.8, -4)), P2(local(J, n, -6.0, 3.0)), P2(local(J, n, -3.6, -0.4))],
                 L['over'], layer='over')
    fig.cap(P2(local(J, p, -5.4, -0.8)), P2(local(J, p, 6.0, -0.8)), 1.4, 1.4, L['belt'], layer='belt')
    fig.box(P2(local(J, p, 4.2, -0.8)), 1.4, 1.5, L['buckle'], layer='buckle')
    if L.get('mantle') is not None:  # a shaggy fur mantle over the shoulders
        fig.ell(P2(local(J, n, 0.4, 2.4)), 7.6, 3.5, L['mantle'], layer='mantle', drip=(9, 3))
    fig.part = 'head'
    if L['helm'] is None:  # a deep hood, the face lost in its shadow
        fig.ell(P2(H + np.array([-0.4, 0.4])), 6.4, 6.8, L['hood'])
        fig.poly([P2(H + np.array([-6.0, 1.0])), P2(H + np.array([-9.0 - P['cloth'] * 0.6, 6.0])), P2(H + np.array([-3.0, 7.0]))], L['hood'], drip=(3, 3))  # its tail
        fig.poly([P2(local(J, n, -5.0, 0.6)), P2(local(J, n, 5.6, 1.4)), P2(local(J, n, 4.6, 4.4)), P2(local(J, n, -5.4, 3.8))], L['hood'], drip=(2, 2))  # the cowl on the shoulders
        face(fig, H, w=3.4, h=4.4)
        beard(fig, H, L['beard'])
    else:
        M = L['metal']
        if L['helm'] == 'aventail':  # mail hanging from the helm round the neck
            fig.ell(P2(H + np.array([-0.8, 3.0])), 5.8, 5.0, NMat(METAL, 'mail', tag=1), layer='aventail')
        face(fig, H, w=3.4, h=4.4, dy=0.6)
        beard(fig, H, L['beard'], L=10.0, w=3.8)
        bt = H + np.array([3.4, 5.5])
        fig.cap(P2(bt), P2(bt + np.array([0.6, 8.0])), 0.8, 0.7, L['beard'], layer='braid', tone=1)  # a braid down the middle
        fig.cap(P2(bt + np.array([0.6, 8.0])), P2(bt + np.array([0.7, 9.6])), 0.9, 0.9, L['belt'], layer='braid', tone=3)  # tied off
        if L['helm'] == 'great':
            fig.add(np.maximum(g.ellipse(H + np.array([0.4, 0.2]), 6.0, 6.6), -(g.Y - (H[1] - 2.0))), M, org=P2(H), layer='helm')
            fig.box(P2(H + np.array([3.6, 0.0])), 2.4, 0.7, NMat([FACE] * 5, 'flat'), layer='slit')
            fig.marks.append((int(H[0] + 3), int(H[1]), EYES[2], 'head')); fig.marks.append((int(H[0] + 5), int(H[1]), EYES[1], 'head'))
        cone = [P2(H + np.array([-5.6, 0.0])), P2(H + np.array([-5.2, -4.4])), P2(H + np.array([-2.2, -7.6])), P2(H + np.array([0.4, -9.2])),
                P2(H + np.array([3.2, -7.4])), P2(H + np.array([5.6, -4.0])), P2(H + np.array([6.0, 0.0]))]
        fig.poly(cone, M, layer='helm')  # a pointed spangenhelm
        fig.cap(P2(H + np.array([-5.6, -0.4])), P2(H + np.array([6.0, -0.4])), 1.0, 1.0, M, layer='rim', tone=3)
        fig.cap(P2(H + np.array([0.4, -8.6])), P2(H + np.array([0.6, -0.8])), 0.6, 0.6, M, layer='rim', tone=3)  # its riveted band
        if L['helm'] in ('nasal', 'aventail', 'spangen'): fig.cap(P2(H + np.array([5.2, -0.6])), P2(H + np.array([5.4, 3.2])), 0.9, 0.8, M, layer='nasal')
        if L['helm'] == 'aventail':
            for k in range(3): fig.marks.append((int(H[0] + 2), int(H[1] - 3 - k), RUNE[4 - k] + (2,), 'head'))  # a rune on the brow, in the glow colour
    if not P.get('noArmN'): arm('N')


# ---------------------------------------------------------------- gear (absolute angles: 0 down, pi/2 forward, pi up)
STEELM, RUST, WOOD = NMat(STEEL), NMat(STEEL, base=1), NMat(BROWN)
FX = STEEL[4]  # the streak of a fast blow


def blade(f, hand, ang, L=12, w=1.6, metal=STEELM, hilt=None):
    d = dir_(ang); side = np.array([d[1], -d[0]])
    f.layer = 'weapon'
    f.cap(P2(hand - d * 2.4), P2(hand + d * 1.0), 0.9, 0.9, hilt or WOOD)
    f.poly([P2(hand + d * 1.6 + side * w), P2(hand + d * (L - 2.5) + side * w), P2(hand + d * L), P2(hand + d * (L - 2.5) - side * w * 0.8),
            P2(hand + d * 1.6 - side * w * 0.8)], metal)
    f.cap(P2(hand + d * 1.4 + side * 2.4), P2(hand + d * 1.4 - side * 2.4), 0.8, 0.8, hilt or WOOD, tone=3)
    f.layer = None


def spear(f, hand, ang, fore=24, back=8, shaft=WOOD, head=STEELM):
    d = dir_(ang); side = np.array([d[1], -d[0]])
    tip = hand + d * fore
    f.layer = 'weapon'
    f.cap(P2(hand - d * back), P2(tip), 0.9, 0.9, shaft)
    f.poly([P2(tip - d * 0.5 + side * 1.8), P2(tip + d * 6.5), P2(tip - d * 0.5 - side * 1.8), P2(tip - d * 2.2)], head)
    f.layer = None


def fx(fig, J, P, aw, reach):
    if not P.get('fx'): return
    if P['fx'] == 'thrust': anim.streak(fig, J['haN'] - dir_(aw) * 6, J['haN'] + dir_(aw) * reach * 0.8, FX)
    else: anim.smear(fig, J['shN'], reach * 0.55, reach + 6, P['fx'][0], P['fx'][1], FX)


def arm_(fig, J, s, sleeve, hand_m, r=(2.2, 1.9, 1.9, 1.7), hr=1.9, fore=None):
    fig.part = 'uarm' + s
    limb(fig, J['sh' + s], J['el' + s], r[0], r[1], sleeve)
    fig.part = 'farm' + s
    limb(fig, J['el' + s], J['ha' + s], r[2], r[3], fore or sleeve)
    fig.circle(P2(J['ha' + s] + dir_(J['fa' + s]) * 0.6), hr, hand_m, layer='hand' + s)


def leg_(fig, J, s, thigh, shin, foot, r=(2.8, 2.4, 2.3, 2.0), toe=4.0, fh=2.8):
    fig.part = 'thigh' + s
    limb(fig, J['hip' + s], J['kn' + s], r[0], r[1], thigh)
    fig.part = 'shin' + s
    limb(fig, J['kn' + s], J['ft' + s], r[2], r[3], shin)
    fig.layer = 'foot' + s
    boot(fig, J['ft' + s], J['toe' + s], toe, foot, h=fh)
    fig.layer = None


def torso_(fig, J, m, front=4.6, back=-4.2, low=1.4, hunch=0.0, **k):
    n, p = J['neck'], J['pel']
    return fig.poly([P2(local(J, n, front * 0.55, -0.4)), P2(local(J, n, front, 3.0)), P2(local(J, p, front * 0.92, -3.6)),
                     P2(local(J, p, front * 0.85, low)), P2(local(J, p, back * 0.9, low)), P2(local(J, p, back, -4)),
                     P2(local(J, n, back - hunch, 3.0)), P2(local(J, n, back * 0.55 - hunch, -0.6))], m, **k)


def skirt_(fig, J, P, m, length=5.0, front=4.4, back=-4.2, drip=(1, 5)):
    p = J['pel']
    sw = P['cloth'] * 0.8
    fig.poly([P2(local(J, p, front, -1.0)), (p[0] + front + 1.0 + sw, p[1] + length), (p[0] + back - 0.6 + sw, p[1] + length), P2(local(J, p, back, -1.0))], m, drip=drip)


# ---------------------------------------------------------------- goblins
GOB = anim.GOB
HIDE = NMat(OLIVE)


def goblin_head(fig, J, P, bomber=False):
    H = J['head']
    fig.part = 'head'
    sway = P['cloth'] * 0.5
    fig.poly([P2(H + np.array([-2.0, -0.6])), P2(H + np.array([-9.4, -4.0 + sway])), P2(H + np.array([-7.0, -0.4])), P2(H + np.array([-2.2, 2.2]))], HIDE, layer='ear')
    if bomber:  # a leather cap, goggles pushed up on it
        fig.ell(P2(H + np.array([0.6, 0.6])), 4.8, 4.6, HIDE)
        fig.add(np.maximum(fig.g.ellipse(H + np.array([0.2, -1.0]), 5.2, 4.2), fig.g.Y - (H[1] - 1.0)), NMat(BROWN), org=P2(H), layer='cap')
        fig.circle(P2(H + np.array([2.0, -2.2])), 1.6, STEELM, layer='goggle'); fig.circle(P2(H + np.array([4.4, -2.0])), 1.4, STEELM, layer='goggle2')
        face(fig, H + np.array([0.4, 0.4]), w=2.8, h=2.6)
    else:  # a ragged hood
        fig.ell(P2(H + np.array([-0.2, 0.0])), 5.2, 5.4, NMat(BROWN, 'cloth', base=1))
        fig.poly([P2(H + np.array([-4.6, 1.0])), P2(H + np.array([-7.4 - sway, 5.0])), P2(H + np.array([-2.0, 5.4]))], NMat(BROWN, 'cloth', base=1), drip=(4, 3))
        face(fig, H, w=2.8, h=3.4)
    fig.cap(P2(H + np.array([3.4, 0.8])), P2(H + np.array([7.4, 2.6])), 1.3, 0.8, HIDE, layer='nose')  # a long nose out of the shadow


def goblin(fig, J, P, L=None):
    rag = NMat(BROWN, 'cloth')
    aw = P.get('wa', J['w'])
    fig.dark = 1
    arm_(fig, J, 'F', HIDE, HIDE, r=(1.9, 1.6, 1.6, 1.4), hr=1.7)
    leg_(fig, J, 'F', HIDE, HIDE, HIDE, r=(2.3, 1.9, 1.7, 1.4), toe=3.8, fh=2.2)
    fig.dark = 0
    leg_(fig, J, 'N', HIDE, HIDE, HIDE, r=(2.4, 2.0, 1.8, 1.5), toe=3.8, fh=2.2)
    fig.part = 'torso'
    torso_(fig, J, rag, front=4.2, back=-3.8, hunch=1.2)
    skirt_(fig, J, P, rag, length=3.6, front=3.8, back=-3.6, drip=(2, 4))
    fig.cap(P2(local(J, J['pel'], -3.8, -1.2)), P2(local(J, J['pel'], 4.0, -1.2)), 1.0, 1.0, NMat(BROWN, base=1), layer='belt')
    goblin_head(fig, J, P)
    fig.part = 'weapon'
    blade(fig, J['haN'], aw, L=10, metal=RUST)
    arm_(fig, J, 'N', HIDE, HIDE, r=(2.0, 1.7, 1.7, 1.5), hr=1.8)
    fx(fig, J, P, aw, 12)


def bomb(fig, c, r=2.8, lit=True):
    fig.circle(P2(c), r, NMat(STEEL, base=1), layer='bomb%d' % int(c[0] * 7 + c[1]))
    if lit:
        fig.line(c + np.array([0.4, -r - 0.4]), c + np.array([1.6, -r - 2.4]), BROWN[3])
        fig.dot(c + np.array([1.8, -r - 2.8]), WINE[4])


def bomber(fig, J, P, L=None):
    vest, trews = NMat(BROWN), NMat(BROWN, 'cloth', base=1)
    fig.dark = 1
    arm_(fig, J, 'F', HIDE, HIDE, r=(1.9, 1.6, 1.6, 1.4), hr=1.7)
    leg_(fig, J, 'F', trews, trews, NMat(BROWN, base=1), r=(2.3, 1.9, 1.7, 1.4), toe=3.8, fh=2.8)
    fig.dark = 0
    leg_(fig, J, 'N', trews, trews, NMat(BROWN, base=1), r=(2.4, 2.0, 1.8, 1.5), toe=3.8, fh=2.8)
    fig.part = 'torso'
    torso_(fig, J, vest, front=4.4, back=-4.0, hunch=1.0)
    n, p = J['neck'], J['pel']
    fig.cap(P2(local(J, n, 2.8, 0.6)), P2(local(J, p, -3.4, -1.0)), 1.0, 1.0, NMat(BROWN, base=1), layer='strap')
    for k in (0.3, 0.62): bomb(fig, local(J, n, 2.8 - 6.2 * k, 0.6 + 10.0 * k), r=1.5, lit=False)
    fig.cap(P2(local(J, p, -4.0, -1.0)), P2(local(J, p, 4.2, -1.0)), 1.0, 1.0, NMat(BROWN, base=1), layer='belt')
    goblin_head(fig, J, P, bomber=True)
    fig.part = 'weapon'
    if not P.get('cast'): bomb(fig, J['haN'] + dir_(J['faN']) * 1.8, r=2.8)
    arm_(fig, J, 'N', HIDE, HIDE, r=(2.0, 1.7, 1.7, 1.5), hr=1.8)


def redcap(fig, J, P, L=None):
    """A wizened little murderer: a cap dyed in blood, a long grey beard, an iron pike and iron-shod boots."""
    H = J['head']
    coat, iron, cap = NMat(BROWN, 'cloth'), NMat(STEEL, base=1), NMat(WINE, 'cloth', base=3)
    aw = P.get('wa', J['w'])
    fig.dark = 1
    arm_(fig, J, 'F', coat, NMat(SKIN), r=(1.9, 1.6, 1.6, 1.4), hr=1.6)
    leg_(fig, J, 'F', coat, iron, iron, r=(2.2, 1.8, 2.2, 2.3), toe=4.2, fh=3.8)
    fig.dark = 0
    leg_(fig, J, 'N', coat, iron, iron, r=(2.3, 1.9, 2.3, 2.4), toe=4.2, fh=3.8)
    fig.part = 'torso'
    torso_(fig, J, coat, front=4.0, back=-3.8, hunch=1.4)
    skirt_(fig, J, P, coat, length=4.4, front=3.8, back=-3.8, drip=(3, 5))
    fig.cap(P2(local(J, J['pel'], -3.8, -1.0)), P2(local(J, J['pel'], 4.0, -1.0)), 0.9, 0.9, NMat(BROWN, base=1), layer='belt')
    fig.part = 'head'
    fig.ell(P2(H + np.array([0.6, 0.8])), 4.0, 4.2, NMat(SKIN))
    face(fig, H + np.array([0.2, 0.6]), w=2.8, h=2.8)
    beard(fig, H + np.array([-0.4, -0.6]), NMat(BONE, 'hair'), L=9.0, w=2.6)
    tip = H + np.array([-8.0 - P['cloth'] * 0.8, -3.0])
    fig.poly([P2(H + np.array([-4.6, -0.6])), P2(H + np.array([4.2, -1.6])), P2(H + np.array([1.4, -6.4])), P2(tip)], cap, layer='cap')  # flopping back
    for d in ((2.4, -0.8), (-1.0, 0.0), (0.6, 0.4)): fig.dot(H + np.array(d), BLOOD[2])  # still dripping
    fig.part = 'weapon'
    spear(fig, J['haN'], aw, fore=18, back=6, shaft=NMat(STEEL, base=1), head=STEELM)
    arm_(fig, J, 'N', coat, NMat(SKIN), r=(2.0, 1.7, 1.7, 1.5), hr=1.7)
    fx(fig, J, P, aw, 22)


# ---------------------------------------------------------------- the dead of the battlefield
def risen(fig, J, P, L=None):
    """The dead levy: a dented kettle helm over a shadowed face, rotten mail under a torn, bloodied tabard, a spear."""
    g, H, n, p = fig.g, J['head'], J['neck'], J['pel']
    flesh, mail, tabard, trews = NMat(OLIVE, base=3), NMat(STEEL, 'mail', base=2), NMat(WINE, 'cloth'), NMat(BLUE, 'cloth', base=1)
    aw = P.get('wa', J['w'])
    fig.dark = 1
    arm_(fig, J, 'F', mail, flesh, r=(2.5, 2.1, 1.9, 1.6), hr=1.8)
    leg_(fig, J, 'F', trews, trews, NMat(BROWN), r=(2.9, 2.4, 2.2, 1.9), fh=3.4)
    fig.dark = 0
    leg_(fig, J, 'N', trews, trews, NMat(BROWN), r=(3.0, 2.5, 2.3, 2.0), fh=3.4)
    fig.part = 'torso'
    torso_(fig, J, mail, front=5.0, back=-4.6, hunch=0.8)
    skirt_(fig, J, P, mail, length=4.0, front=5.0, back=-4.6, drip=None)
    fig.poly([P2(local(J, n, 0.4, 0.8)), P2(local(J, n, 5.2, 2.6)), P2(local(J, p, 5.0, -2.0)), P2(local(J, p, 3.0, 7.0)),
              P2(local(J, p, -2.6, 6.0)), P2(local(J, p, -3.2, -2.0))], tabard, drip=(5, 6), layer='tabard')
    for d in ((2.0, 4.0), (3.0, 5.0), (2.4, 6.0), (0.4, 8.0), (1.4, 9.0)): fig.dot(local(J, n, *d), BLOOD[1 + int(d[1]) % 2])  # old blood
    fig.cap(P2(local(J, p, -4.6, -1.0)), P2(local(J, p, 5.0, -1.0)), 1.1, 1.1, NMat(BROWN, base=1), layer='belt')
    fig.part = 'head'
    fig.ell(P2(H + np.array([0.6, 1.0])), 4.4, 4.8, flesh)
    face(fig, H, w=3.2, h=3.6, dy=0.4)
    fig.ell(P2(H + np.array([0.4, -1.8])), 5.0, 3.8, STEELM, layer='helm')
    fig.add(np.maximum(g.ellipse(H + np.array([0.6, -1.4]), 8.0, 1.9), -(g.Y - (H[1] - 1.6))), STEELM, org=P2(H), layer='brim')
    fig.line(H + np.array([-1.2, -4.6]), H + np.array([0.4, -3.4]), STEEL[1])  # a dent
    fig.part = 'weapon'
    spear(fig, J['haN'], aw, fore=24, back=8, shaft=WOOD, head=RUST)
    arm_(fig, J, 'N', mail, flesh, r=(2.6, 2.2, 2.0, 1.7), hr=1.9)
    fx(fig, J, P, aw, 30)


def skel_limbs(fig, J, s, bone):
    fig.part = 'uarm' + s
    limb(fig, J['sh' + s], J['el' + s], 1.4, 1.2, bone)
    fig.part = 'farm' + s
    limb(fig, J['el' + s], J['ha' + s], 1.2, 1.0, bone)
    fig.circle(P2(J['ha' + s] + dir_(J['fa' + s]) * 0.6), 1.4, bone, layer='hand' + s)


def skel_leg(fig, J, s, bone):
    fig.part = 'thigh' + s
    limb(fig, J['hip' + s], J['kn' + s], 1.6, 1.3, bone)
    fig.part = 'shin' + s
    limb(fig, J['kn' + s], J['ft' + s], 1.3, 1.1, bone)
    fig.layer = 'foot' + s
    boot(fig, J['ft' + s], J['toe' + s], 3.6, bone, h=1.6)
    fig.layer = None


def skeleton(fig, J, P, L=None):
    H, n, p = J['head'], J['neck'], J['pel']
    bone, rag = NMat(BONE), NMat(WINE, 'cloth', base=1)
    aw = P.get('wa', J['w'])
    fig.dark = 1
    skel_limbs(fig, J, 'F', bone)
    skel_leg(fig, J, 'F', bone)
    fig.dark = 0
    skel_leg(fig, J, 'N', bone)
    fig.part = 'torso'
    fig.cap(P2(n), P2(p), 1.2, 1.2, bone, layer='spine')
    for k in range(4):
        fig.cap(P2(local(J, n, -2.6, 2.0 + k * 1.8)), P2(local(J, n, 3.8 - k * 0.4, 3.4 + k * 1.9)), 0.8, 0.7, bone, layer='rib%d' % k)
    fig.ell(P2(local(J, p, 0.4, 0)), 3.6, 2.2, bone, rot=-J['lean'], layer='pelvis')
    skirt_(fig, J, P, rag, length=4.6, front=3.6, back=-3.6, drip=(6, 6))
    fig.cap(P2(local(J, n, -3.2, 0.6)), P2(local(J, n, 3.8, 0.8)), 1.1, 1.1, bone, layer='collar')
    fig.part = 'head'
    fig.ell(P2(H + np.array([0.2, -0.6])), 4.8, 4.6, bone)
    fig.box(P2(H + np.array([2.6, 3.4])), 2.6, 1.6, bone, layer='jaw')
    fig.ell(P2(H + np.array([2.6, 0.2])), 1.6, 1.6, NMat([FACE] * 5, 'flat'), layer='eye1')
    fig.ell(P2(H + np.array([5.0, 0.2])), 1.0, 1.4, NMat([FACE] * 5, 'flat'), layer='eye2')
    fig.dot(H + np.array([2.6, 0.2]), EYES[2]); fig.dot(H + np.array([5.0, 0.2]), EYES[0])
    for k in range(4): fig.dot(H + np.array([1.4 + k * 1.1, 2.6]), BONE[0])
    fig.part = 'weapon'
    blade(fig, J['haN'], aw, L=14, metal=RUST)
    skel_limbs(fig, J, 'N', bone)
    fx(fig, J, P, aw, 15)


# ---------------------------------------------------------------- beasts
def wolf(fig, J, P, L=None):
    fur, belly, paw = NMat(ASHFUR, 'fur'), NMat(ASHFUR, 'fur', base=3), NMat(ASHFUR, base=1)

    def leg(s, front):
        a, b, c = ('sh', 'el', 'ha') if front else ('hip', 'kn', 'ft')
        fig.part = ('uarm' if front else 'thigh') + s
        limb(fig, J[a + s], J[b + s], 3.0 if front else 3.8, 2.0, fur, layer='body' if s == 'N' else None)  # haunch and shoulder melt into the body
        fig.part = ('farm' if front else 'shin') + s
        limb(fig, J[b + s], J[c + s], 1.7, 1.4, fur)
        fig.ell(P2(J[c + s] + np.array([1.0, -0.4])), 2.0, 1.1, paw, layer='paw' + s + str(front))

    fig.dark = 1
    leg('F', True); leg('F', False)
    fig.dark = 0
    fig.part = 'tail'
    fig.cap(P2(J['wb']), P2(J['wt']), 2.4, 1.6, fur)
    S, Hp = J['neck'], J['pel']
    fig.part = 'torso'
    fig.layer = 'body'
    fig.cap(P2(Hp), P2(S), 4.6, 5.6, fur)
    fig.ell(P2(S + np.array([0.4, 1.6])), 4.8, 4.6, fur)
    fig.cap(P2(S + np.array([0.6, -0.4])), P2(J['headb']), 4.6, 3.4, fur)  # the ruff
    fig.layer = None
    leg('N', False); leg('N', True)
    fig.part = 'head'
    hb = J['headb']
    d = (J['headt'] - hb) / (np.linalg.norm(J['headt'] - hb) + 1e-9)
    fig.poly([P2(hb + np.array([-1.6, -2.0])), P2(hb + np.array([-0.6, -6.6])), P2(hb + np.array([1.4, -2.4]))], fur, layer='ear')
    fig.ell(P2(hb + d * 2.0), 3.8, 3.4, fur)
    fig.cap(P2(hb + d * 3.0), P2(J['headt']), 2.4, 1.6, fur)
    if P.get('bite'):
        fig.cap(P2(hb + d * 3.0 + np.array([0, 2.2])), P2(J['headt'] + np.array([-0.8, 3.2])), 1.2, 0.9, NMat(WINE), layer='jaw')
        for k in (0.4, 0.7): fig.dot(hb + d * 3.0 + (J['headt'] - hb - d * 3.0) * k + np.array([0, 1.4]), BONE[4])
    fig.dot(J['headt'] + d * 0.6 + np.array([0, -0.6]), FACE)  # its nose
    e = hb + d * 3.0 + np.array([0.0, -1.6])
    fig.dot(e, FACE); fig.dot(e + np.array([1, 0]), EYES[1])


def bat(fig, J, P, L=None):
    fur, mem, bone = NMat(BROWN, 'fur', base=2), NMat(WINE, 'cloth', base=2), WINE[0]
    w = P.get('wing', -2.0)

    def wing(root, ang, L, back):
        tip = root + dir_(ang) * L
        f1 = root + dir_(ang + 0.55) * L * 0.82
        f2 = root + dir_(ang + 1.1) * L * 0.6
        inset = lambda a, b: (a + b) * 0.5 + (root - (a + b) * 0.5) * 0.22
        fig.poly([P2(root), P2(tip), P2(inset(tip, f1)), P2(f1), P2(inset(f1, f2)), P2(f2), P2(inset(f2, back)), P2(back)], mem)
        for e in (tip, f1, f2): fig.line(root, e, bone)

    fig.part = 'uarmF'
    fig.dark = 1
    wing(J['shF'], w + 0.25, 15, J['pel'] + np.array([-0.4, -0.6]))
    fig.dark = 0
    fig.part = 'torso'
    fig.ell(P2((J['neck'] + J['pel']) * 0.5), 3.4, 5.0, fur, rot=-0.6)
    for s in (-1, 1): fig.line(J['pel'], J['pel'] + np.array([-1.0 + s * 0.8, 3.2]), BROWN[1])
    H = J['head']
    fig.ell(P2(H), 2.8, 2.6, fur, layer='head')
    fig.poly([P2(H + np.array([-1.6, -1.6])), P2(H + np.array([-2.6, -5.4])), P2(H + np.array([0.0, -2.2]))], mem, layer='ear1')
    fig.poly([P2(H + np.array([0.4, -2.0])), P2(H + np.array([1.2, -5.6])), P2(H + np.array([2.0, -1.6]))], mem, layer='ear2')
    fig.dot(H + np.array([1.4, -0.4]), EYES[2])
    fig.dot(H + np.array([2.4, 1.6]), BONE[4])
    fig.part = 'uarmN'
    wing(J['shN'], w, 17, J['pel'] + np.array([0.6, -0.4]))


def slime(fig, J, P, L=None):
    g = fig.g
    goo = NMat(GOO)
    sx, sy = P.get('sx', 1.0), P.get('sy', 1.0)
    rx, ry = 13.0 * sx, 10.5 * sy
    c = np.array([J['pel'][0] + P['x'], J['gy'] - ry - 0.5 + P['y']])
    fig.part = 'torso'
    fig.add(np.maximum(g.ellipse(c, rx, ry), g.Y - (J['gy'] - 0.2 + P['y'])), goo, org=P2(c))
    fig.ell(P2(c + np.array([-1.0, 2.0])), rx * 0.62, ry * 0.5, NMat(GOO, base=1), layer='core')  # its murky middle
    fig.circle(P2(c + np.array([-3.4 * sx, 2.6 * sy])), 2.4, NMat(BONE, base=1), layer='skull')  # what it swallowed
    fig.dot(c + np.array([-4.0 * sx, 2.4 * sy]), FACE); fig.dot(c + np.array([-2.6 * sx, 2.4 * sy]), FACE)
    fig.cap(P2(c + np.array([3.0 * sx, 4.4 * sy])), P2(c + np.array([7.0 * sx, 3.0 * sy])), 0.8, 0.8, NMat(BONE, base=1), layer='bone')
    for ex in (3.0, 6.4):
        e = c + np.array([ex * sx, -3.4 * sy])
        fig.dot(e, FACE); fig.dot(e + np.array([1, 0]), FACE); fig.dot(e + np.array([1, -1]), EYES[2])



# ---------------------------------------------------------------- the deep sea and the desert
SEA = R('#0e1c20', '#163036', '#20464c', '#2c6264', '#3c807a')       # a sea serpent's scales
BELLY = R('#33443a', '#4c6252', '#68826c', '#8aa48a', '#acc4a8')
FIN = R('#1c1424', '#2e2238', '#46324e', '#604668', '#7c5c84')
CHITIN = R('#22140c', '#3e2414', '#62381c', '#8a5428', '#b0783a')    # a giant scorpion's plates
SANDC = R('#4a3a26', '#6e5a3c', '#927a54', '#b49c70', '#cebb8e')     # sun-bleached wraps


def serpent(fig, J, P, L=None):
    """A sea serpent: a long ribbon of scales swimming in waves that grow towards the tail, a crest along its back,
    a pale belly, and a long head whose jaws gape when it strikes."""
    gy, hx = J['gy'], J['pel'][0] + P['x'] + 26 + P.get('lunge', 0.0)
    ph, op = P.get('ph', 0.0), P.get('open', 0.0)
    N = 44
    pts, rad = [], []
    for i in range(N + 1):
        t = i / N  # 0 tail .. 1 neck
        x = hx - 22 - (1 - t) * 118
        amp = 9.0 * (1 - t) ** 0.8 + 1.2
        y = gy - 13 + amp * math.sin(2 * math.pi * 1.6 * t - ph) + P['y']
        pts.append(np.array([x, y]))
        rad.append(1.4 + 4.6 * math.sin(math.pi * min(1.0, t * 1.1 + 0.02)) ** 0.6)
    scales, belly, fin = NMat(SEA), NMat(BELLY, base=2), NMat(FIN)
    fig.part = 'torso'
    for i in range(0, N, 4):  # the crest, a sail of spines along the back
        a, b = pts[i], pts[min(N, i + 3)]
        if i < 4 or i > N - 6: continue
        up = -2.0 - rad[i] * 1.4
        fig.poly([P2(a + np.array([0, -rad[i] + 1])), P2((a + b) * 0.5 + np.array([-1.0, up])), P2(b + np.array([0, -rad[i] + 1]))], fin, layer='crest')
    for i in range(N):
        fig.cap(P2(pts[i]), P2(pts[i + 1]), rad[i], rad[i + 1], scales, layer='body')
    for i in range(2, N):
        fig.cap(P2(pts[i] + np.array([0, rad[i] * 0.55])), P2(pts[i + 1] + np.array([0, rad[i + 1] * 0.55])), rad[i] * 0.42, rad[i + 1] * 0.42, belly, layer='belly')
    fig.poly([P2(pts[0] + np.array([2, 0])), P2(pts[0] + np.array([-7, -5])), P2(pts[0] + np.array([-5, 0])), P2(pts[0] + np.array([-7, 5]))], fin, layer='tailfin')
    # the head: a long skull, the lower jaw swinging open
    fig.part = 'head'
    n = pts[N]
    H = np.array([hx - 12, n[1] - 1])
    fig.cap(P2(n), P2(H), rad[N], 5.0, scales, layer='neck')
    jaw = 0.15 + 0.75 * op
    fig.ell(P2(H + np.array([2.0, -0.6])), 8.0, 4.6, scales, layer='skull')
    fig.cap(P2(H + np.array([6.0, -1.0])), P2(H + np.array([16.0, -2.0 - op * 2])), 3.4, 1.8, scales, layer='snout')
    jd = np.array([math.cos(jaw), math.sin(jaw)])
    if op > 0.05:  # the mouth, dark, its teeth catching the light
        fig.poly([P2(H + np.array([2.0, 0.6])), P2(H + np.array([16.0, -1.0 - op * 2])), P2(H + np.array([2, 1.6]) + jd * 13)], NMat([FACE] * 5, 'flat'), layer='maw')
        for k in range(4): fig.dot(H + np.array([5.0 + k * 2.6, -0.2 - op * 0.5 * k]), BONE[4])
    fig.cap(P2(H + np.array([0.0, 2.2])), P2(H + np.array([2, 2.2]) + jd * 13), 2.4, 1.3, NMat(BELLY, base=1), layer='jaw')
    fig.poly([P2(H + np.array([-6, -3])), P2(H + np.array([-14, -9])), P2(H + np.array([-2, -4.5]))], fin, layer='frill')  # a frill behind the skull
    fig.dot(H + np.array([4.0, -2.4]), EYES[2]); fig.dot(H + np.array([5.0, -2.4]), EYES[1])


def serpent_anims():
    A = {}
    Q = lambda n: [i / n * 2 * math.pi for i in range(n)]
    A['idle'] = [pose(ph=q) for q in Q(8)]
    A['walk'] = [pose(ph=q) for q in Q(8)]
    A['windup'] = [pose(ph=0.6, lunge=-3.0, open=0.3), pose(ph=0.9, lunge=-6.0, open=0.6), pose(ph=1.1, lunge=-7.0, open=0.8)]
    A['strike'] = [pose(ph=1.6, lunge=6.0, open=1.0), pose(ph=2.0, lunge=9.0, open=0.9)]
    A['recover'] = [pose(ph=2.4, lunge=4.0, open=0.4), pose(ph=2.8, lunge=1.0, open=0.1), pose(ph=3.2)]
    A['hurt'] = [pose(ph=0.3, lunge=-5.0, open=0.5, y=-1.0), pose(ph=0.6, lunge=-2.0, open=0.2)]
    return A


def scorpion(fig, J, P, L=None):
    """A giant scorpion: a low plated body on eight legs, great pincers held forward, and a jointed tail curled over its
    back that lashes forward with the sting. Drawn at K times its sketch size."""
    K = 1.5
    gy, cx = J['gy'], J['pel'][0] + P['x']
    ph, curl, clw, by = P.get('ph', 0.0), P.get('curl', 0.0), P.get('claw', 0.3), P['y']
    plate = NMat(CHITIN)
    body = np.array([cx + 2 * K, gy - 11 * K + by])
    o = lambda x, y, at=None: (body if at is None else at) + np.array([x, y]) * K

    def leg(ax, k, far):
        a = o(ax, 3.0)
        sw = math.sin(ph + k * 1.7 + (math.pi if far else 0)) * 3.2 * K
        lift = max(0.0, math.cos(ph + k * 1.7 + (math.pi if far else 0))) * 2.0 * K
        foot = np.array([a[0] + (1.5 - k) * 7.0 * K + sw, gy - lift])  # the front pairs reach forward, the back pairs back
        knee = a + (foot - a) * 0.4 + np.array([(1.5 - k) * 2.5, -9.0]) * K  # knees high and splayed
        fig.part = ('thigh' if k < 2 else 'shin') + ('F' if far else 'N')
        fig.cap(P2(a), P2(knee), 1.8 * K, 1.4 * K, plate, layer='leg%d%d' % (k, far))
        fig.cap(P2(knee), P2(foot), 1.4 * K, 0.8 * K, plate, layer='leg%d%d' % (k, far))

    def pincer(off, far):
        sh = o(13.0, 1.0 + off)
        el = o(7.0, -5.0 - clw * 2, sh)
        cl = o(8.0, 2.0, el)
        fig.part = 'uarm' + ('F' if far else 'N')
        fig.cap(P2(sh), P2(el), 2.2 * K, 1.8 * K, plate, layer='arm%d' % far)
        fig.cap(P2(el), P2(cl), 1.8 * K, 2.6 * K, plate, layer='arm%d' % far)
        fig.ell(P2(o(2.5, 0.0, cl)), 5.0 * K, 3.2 * K, plate, layer='claw%d' % far)
        fig.cap(P2(o(5.0, -1.4, cl)), P2(o(11.0, -3.0 - clw * 4, cl)), 1.5 * K, 0.6 * K, plate, layer='finger%d' % far)
        fig.cap(P2(o(5.0, 1.4, cl)), P2(o(10.0, 2.5 + clw * 2, cl)), 1.2 * K, 0.5 * K, plate, layer='thumb%d' % far)

    fig.dark = 1
    for k in range(4): leg(10 - k * 7, k, True)
    pincer(-1.5, True)
    fig.dark = 0
    fig.part = 'torso'  # a head plate and four rings of abdomen tapering back
    fig.ell(P2(o(7.0, -1.0)), 10.0 * K, 5.6 * K, plate, layer='prosoma')
    for i in range(4):
        fig.ell(P2(o(-3.0 - i * 6.0, -1.0 + i * 0.4)), (6.2 - i * 0.6) * K, (5.4 - i * 0.5) * K, plate, layer='seg%d' % i)
    # the tail: five joints curling up over the back. `curl` < 0 cocks it tight; > 0 swings the whole tail forward
    # and straightens it, so the sting stabs out over the head
    fig.part = 'tail'
    pts = [o(-24.0, -2.0)]
    ang, inc = -2.2 + curl * 0.45, 0.55 - curl * 0.13
    for i in range(5):
        ang += inc
        pts.append(pts[-1] + np.array([math.cos(ang), math.sin(ang)]) * (7.0 - i * 0.6) * K)
    for i in range(5):
        fig.cap(P2(pts[i]), P2(pts[i + 1]), (3.4 - i * 0.35) * K, (3.0 - i * 0.35) * K, plate, layer='tail%d' % (i % 2))
    d = np.array([math.cos(ang + 0.6), math.sin(ang + 0.6)])
    fig.ell(P2(pts[-1] + d * 2.0 * K), 3.2 * K, 2.6 * K, NMat(CHITIN, base=3), layer='bulb')
    fig.cap(P2(pts[-1] + d * 3.5 * K), P2(pts[-1] + d * 8.5 * K + np.array([0, K])), 1.2 * K, 0.3 * K, NMat(BONE, base=3), layer='sting')
    for k in range(4): leg(10 - k * 7, k, False)
    pincer(1.5, False)
    fig.part = 'head'
    for e in ((12.0, -5.0), (14.0, -5.4)): fig.dot(o(*e), EYES[2])


def scorpion_anims():
    A = {}
    Q = lambda n: [i / n * 2 * math.pi for i in range(n)]
    A['idle'] = [pose(ph=0.0, claw=0.3 + 0.15 * math.sin(q), curl=0.15 * math.sin(q)) for q in Q(6)]
    A['walk'] = [pose(ph=q, claw=0.3, curl=0.1 * math.sin(q * 2), y=-0.5 * abs(math.sin(q * 2))) for q in Q(8)]
    A['windup'] = [pose(claw=0.8, curl=-0.6), pose(claw=1.0, curl=-1.2), pose(claw=1.0, curl=-1.5, y=0.5)]
    A['strike'] = [pose(claw=0.4, curl=2.4), pose(claw=0.2, curl=3.2)]
    A['recover'] = [pose(claw=0.4, curl=1.8), pose(claw=0.4, curl=0.8), pose(claw=0.3, curl=0.2)]
    A['hurt'] = [pose(claw=0.9, curl=-0.8, x=-1.5), pose(claw=0.6, curl=-0.3)]
    return A


def glaive(f, hand, ang, fore=22, back=10):
    """The raiders' staff: a long iron-shod shaft ending in a flared, hooked blade."""
    d = dir_(ang); side = np.array([d[1], -d[0]])
    tip = hand + d * fore
    f.layer = 'weapon'
    f.cap(P2(hand - d * back), P2(tip), 0.9, 0.9, WOOD)
    f.cap(P2(hand - d * back), P2(hand - d * (back - 2.5)), 1.3, 1.3, STEELM)  # the iron foot
    f.poly([P2(tip - d * 1.0 + side * 1.0), P2(tip + d * 3.0 + side * 4.5), P2(tip + d * 7.5 + side * 2.0), P2(tip + d * 6.0),
            P2(tip + d * 7.5 - side * 3.0), P2(tip + d * 3.0 - side * 2.0), P2(tip - d * 1.0 - side * 1.0)], STEELM)
    f.cap(P2(tip + d * 1.0 - side * 1.0), P2(tip - d * 2.0 - side * 4.0), 0.8, 0.5, STEELM, tone=3)  # the hook
    f.layer = None


def raider(fig, J, P, L=None):
    """A sand raider: wrapped head to foot in bleached cloth, goggles and a breathing grille where a face should be, a
    tattered mantle, a bandolier of pouches, and a hooked glaive."""
    H, n, p = J['head'], J['neck'], J['pel']
    wrap, wrapD, boot, strap = NMat(SANDC, 'cloth'), NMat(SANDC, 'cloth', base=1), NMat(BROWN, base=1), NMat(BROWN)
    aw = P.get('wa', J['w'])
    fig.dark = 1
    arm_(fig, J, 'F', wrap, wrapD, r=(2.4, 2.0, 1.9, 1.7), hr=1.8)
    leg_(fig, J, 'F', wrap, wrap, boot, r=(2.8, 2.3, 2.2, 1.9), fh=3.0)
    fig.dark = 0
    leg_(fig, J, 'N', wrap, wrap, boot, r=(2.9, 2.4, 2.3, 2.0), fh=3.0)
    fig.part = 'torso'
    torso_(fig, J, wrap, front=5.0, back=-4.6, hunch=1.4)
    skirt_(fig, J, P, wrap, length=8.0, front=5.0, back=-4.8, drip=(7, 6))  # the robe, torn to the shins
    fig.poly([P2(local(J, n, -5.6, -0.4)), P2(local(J, n, 5.0, 0.4)), P2(local(J, n, 4.0, 6.0)), P2(local(J, n, -7.0 - P['cloth'], 9.0))],
             NMat(SANDC, 'cloth', base=1), drip=(9, 5), layer='mantle')
    fig.cap(P2(local(J, n, 3.6, 0.8)), P2(local(J, p, -4.0, -1.0)), 1.1, 1.1, strap, layer='bandolier')
    for k in (0.3, 0.55, 0.8): fig.box(P2(local(J, n, 3.6 - 7.6 * k, 0.8 + 9.0 * k)), 1.2, 1.1, NMat(BROWN, base=2), layer='pouch%d' % int(k * 10))
    fig.cap(P2(local(J, p, -4.6, -1.0)), P2(local(J, p, 5.0, -1.0)), 1.0, 1.0, strap, layer='belt')
    fig.part = 'head'
    fig.ell(P2(H + np.array([0.4, 0.4])), 5.0, 5.6, wrap)  # the head-wrap
    fig.poly([P2(H + np.array([-4.4, -2.0])), P2(H + np.array([-9.0 - P['cloth'], 3.0])), P2(H + np.array([-3.0, 4.0]))], wrap, drip=(4, 3), layer='tail')
    face(fig, H + np.array([0.6, 0.6]), w=3.0, h=3.4, eyes=0)
    for e in (np.array([3.4, -0.6]), np.array([6.0, -0.4])):  # goggles: dark lenses in brass rims, a hard glint
        fig.circle(P2(H + e), 1.5, NMat(BONE, base=1), layer='rim%d' % int(e[0]))
        fig.dot(H + e, (24, 22, 28)); fig.dot(H + e + np.array([-0.6, -0.6]), EYES[2])
    for k in range(2): fig.cap(P2(H + np.array([4.0 + k * 1.6, 2.4])), P2(H + np.array([4.4 + k * 1.6, 5.4])), 0.6, 0.5, STEELM, layer='grille%d' % k)  # the breathing grille
    fig.part = 'weapon'
    glaive(fig, J['haN'], aw)
    arm_(fig, J, 'N', wrap, wrapD, r=(2.5, 2.1, 2.0, 1.8), hr=1.9)
    fx(fig, J, P, aw, 30)


# ---------------------------------------------------------------- villagers
# Hearthwick's folk, several of each: men, women, children, each look its own sheet (idle and walk, frame by frame). Coats
# and dresses are tag-1 greys (recoloured per villager in game, like armour metal); everything else is fixed per look.
FOLKM = Body(thigh=7.4, shin=7.2, torso=10.6, ua=6.0, fa=5.6, head=9.6, sw=2.4, hw=1.6)
FOLKF = Body(thigh=7.0, shin=6.8, torso=10.0, ua=5.6, fa=5.2, head=9.4, sw=2.0, hw=1.6)
FOLKC = Body(thigh=4.8, shin=4.6, torso=6.8, ua=4.2, fa=3.8, head=8.8, sw=1.6, hw=1.2)
COATG = R('#5a5a5a', '#7c7c7c', '#9c9c9c', '#bcbcbc', '#dcdcdc')   # tone 2 = the villager's coat colour exactly
HAIRB = R('#2a1a10', '#45291a', '#65401f', '#855a2b', '#a67639')   # brown
HAIRG = R('#6a5320', '#8f7230', '#b8964a', '#d4b765', '#e8d28a')   # fair
HAIRW = R('#4a4844', '#76736c', '#9e9a90', '#c4c0b4', '#e2dfd4')   # white and grey
HAIRK = R('#0d0b0c', '#1a1517', '#2a2224', '#3d3234', '#52454a')   # black
HAIRR = R('#3a1408', '#65240f', '#8f3a18', '#b85624', '#d87a38')   # red
LINEN = R('#6b6558', '#8f8878', '#b3ac9a', '#cfc9b8', '#e6e2d4')
LEATHER = R('#1c1410', '#2e2118', '#46331f', '#604830', '#7a5e40')
SHAWL = R('#2a1f2c', '#43313f', '#5e4455', '#7a5a6a', '#95707e')   # a plum wool shawl
MOSS = R('#1f2418', '#2e3820', '#43502c', '#586839', '#6f8049')   # a green cap


def coat(base=2): return NMat(COATG, 'cloth', tag=1, base=base)
def hr(L, base=2): return NMat(L['hair'], 'hair', base=base)


def folk_head(fig, J, kind, L):
    H = J['head']
    fig.part = 'head'
    sk = NMat(SKIN, base=3)
    s = 0.86 if kind == 'c' else 1.0
    fig.ell(P2(H + np.array([0.4, 0.6])), 4.7 * s, 5.1 * s, sk)
    fig.ell(P2(H + np.array([4.8 * s, 1.6])), 1.3, 1.1, sk, layer='nose')
    ex, ey = int(math.floor(H[0] + 2.8)), int(math.floor(H[1] + 0.2))
    fig.marks.append((ex, ey, (26, 18, 20), 'head')); fig.marks.append((ex + 2, ey, (26, 18, 20), 'head'))
    fig.marks.append((ex + 1, ey + 3, (120, 62, 56), 'head'))  # a mouth
    hs = L['hs']
    if hs == 'mop': fig.ell(P2(H + np.array([-0.8, -2.8])), 5.4, 3.8, hr(L), layer='hair'); fig.cap(P2(H + np.array([-4.6, -1.0])), P2(H + np.array([-4.8, 3.4])), 1.3, 1.0, hr(L, 1), layer='hair')
    elif hs == 'thin':  # a grey fringe round a bare crown
        fig.ell(P2(H + np.array([-2.4, -2.0])), 3.2, 3.4, hr(L), layer='hair')
        fig.cap(P2(H + np.array([-4.2, -1.4])), P2(H + np.array([-4.4, 3.0])), 1.2, 1.0, hr(L, 1), layer='hair')
    elif hs == 'crop': fig.ell(P2(H + np.array([-0.4, -3.2])), 5.2, 2.8, hr(L), layer='hair'); fig.cap(P2(H + np.array([-4.2, -1.8])), P2(H + np.array([-4.4, 1.6])), 1.0, 0.9, hr(L, 1), layer='hair')
    elif hs == 'braid':
        fig.ell(P2(H + np.array([-0.8, -2.4])), 5.4, 3.6, hr(L, 3), layer='hair')
        fig.cap(P2(H + np.array([-4.4, -1.0])), P2(H + np.array([-5.0, 3.0])), 1.6, 1.4, hr(L), layer='hair')
        fig.cap(P2(H + np.array([-4.6, 3.0])), P2(H + np.array([-6.0, 12.0])), 1.5, 1.1, hr(L), layer='braid')
        fig.cap(P2(H + np.array([-6.0, 12.0])), P2(H + np.array([-6.1, 13.4])), 1.0, 1.0, coat(1), layer='braid', tone=3)
    elif hs == 'bun':  # drawn up into a bun at the back
        fig.ell(P2(H + np.array([-0.8, -2.4])), 5.4, 3.6, hr(L, 3), layer='hair')
        fig.cap(P2(H + np.array([-4.4, -1.0])), P2(H + np.array([-4.8, 2.0])), 1.4, 1.2, hr(L), layer='hair')
        fig.circle(P2(H + np.array([-5.6, -2.2])), 2.5, hr(L, 2), layer='bun')
    elif hs == 'loose':  # down to the shoulders
        fig.ell(P2(H + np.array([-0.8, -2.4])), 5.4, 3.6, hr(L, 3), layer='hair')
        fig.poly([P2(H + np.array([-4.4, -2.0])), P2(H + np.array([-1.0, -0.4])), P2(H + np.array([-2.0, 7.6])), P2(H + np.array([-7.4, 9.0])), P2(H + np.array([-6.0, 1.0]))], hr(L, 2), drip=(6, 2), layer='hair')
    elif hs == 'scarf':  # a kerchief knotted at the back
        fig.ell(P2(H + np.array([-0.8, -2.2])), 5.5, 3.8, coat(3), layer='scarf')
        fig.poly([P2(H + np.array([-4.6, -2.2])), P2(H + np.array([-8.4, 1.0])), P2(H + np.array([-6.6, 3.6])), P2(H + np.array([-4.2, 1.4]))], coat(2), layer='scarf', drip=(3, 2))
        fig.cap(P2(H + np.array([-4.0, -0.8])), P2(H + np.array([-4.2, 1.8])), 1.0, 0.9, hr(L, 2), layer='hair')
    elif hs == 'tuft':
        fig.ell(P2(H + np.array([-0.6, -2.6])), 5.0, 3.2, hr(L, 3), layer='hair'); fig.cap(P2(H + np.array([0.0, -5.0])), P2(H + np.array([1.4, -7.4])), 0.9, 0.5, hr(L, 3), layer='hair')
    elif hs == 'pigtails':
        fig.ell(P2(H + np.array([-0.6, -2.6])), 5.0, 3.0, hr(L, 3), layer='hair')
        for dx in (-4.2, -1.0): fig.cap(P2(H + np.array([dx - 1.0, 0.6])), P2(H + np.array([dx - 2.2, 6.4])), 1.5, 1.0, hr(L, 2), layer='tail%d' % int(dx))
    elif hs == 'cap':  # a knitted cap pulled down, hair poking out
        fig.ell(P2(H + np.array([-0.4, -2.6])), 5.4, 3.6, coat(3), layer='cap')
        fig.cap(P2(H + np.array([-5.0, -1.2])), P2(H + np.array([5.0, -1.2])), 1.1, 1.1, coat(1), layer='cap', tone=1)
        fig.cap(P2(H + np.array([-4.2, -0.6])), P2(H + np.array([-4.4, 2.6])), 1.2, 1.0, hr(L, 2), layer='hair')
    b = L.get('beard')
    if b == 'short': beard(fig, H + np.array([-0.2, 0.4]), hr(L, 2), L=5.5, w=3.0)
    elif b == 'long': beard(fig, H + np.array([-0.2, 0.4]), hr(L, 2), L=11.0, w=3.2)
    elif b == 'full': beard(fig, H + np.array([-0.4, 0.2]), hr(L, 2), L=8.0, w=3.8)
    if L.get('hat') == 'flat':  # a flat wool cap
        fig.poly([P2(H + np.array([-5.4, -2.6])), P2(H + np.array([-3.0, -6.4])), P2(H + np.array([2.4, -6.6])), P2(H + np.array([6.6, -3.4])), P2(H + np.array([6.8, -2.2])), P2(H + np.array([-5.2, -2.0]))], NMat(LEATHER, 'cloth', base=2), layer='hat')


def folk_man(fig, J, P, L=None):
    shirt, trews, bt, skin = coat(2), NMat(L['trews'], 'cloth', base=2), NMat(BROWN, base=1), NMat(SKIN, base=2)
    outer = L.get('outer')
    n, p = J['neck'], J['pel']
    fore = skin if L.get('bare') else None
    fig.dark = 1
    arm_(fig, J, 'F', shirt, skin, r=(2.4, 2.1, 2.0, 1.8), hr=1.8, fore=fore)
    leg_(fig, J, 'F', trews, trews, bt, r=(3.0, 2.5, 2.4, 2.1), toe=4.0, fh=2.8)
    fig.dark = 0
    leg_(fig, J, 'N', trews, trews, bt, r=(3.1, 2.6, 2.5, 2.2), toe=4.0, fh=2.8)
    fig.part = 'torso'
    torso_(fig, J, shirt, front=4.8, back=-4.4)
    if outer == 'jerkin':
        fig.poly([P2(local(J, n, 3.2, 0.8)), P2(local(J, n, 4.8, 3.2)), P2(local(J, p, 4.8, -2.0)), P2(local(J, p, 4.4, 1.6)), P2(local(J, p, -4.0, 1.6)),
                  P2(local(J, p, -4.2, -4)), P2(local(J, n, -4.2, 3.2)), P2(local(J, n, -2.0, 0.6))], NMat(BROWN, 'cloth', base=2), layer='vest')
    elif outer == 'apron':  # a leather smith's apron, bib to the knee
        sw = P['cloth'] * 0.6
        fig.poly([P2(local(J, n, 3.6, 1.0)), P2(local(J, n, 5.0, 3.6)), (p[0] + 5.4 + sw, p[1] + 8.0), (p[0] - 0.4 + sw, p[1] + 8.0), P2(local(J, p, -0.2, -1.0)), P2(local(J, n, 0.2, 2.0))], NMat(LEATHER, base=2), layer='apron')
    elif outer == 'robe':  # a long woollen tunic to the knee
        skirt_(fig, J, P, shirt, length=6.5, front=5.2, back=-4.8, drip=(5, 2))
    if outer != 'robe': skirt_(fig, J, P, NMat(BROWN, 'cloth', base=2) if outer == 'jerkin' else shirt, length=3.0, front=4.6, back=-4.4, drip=(2, 2))
    fig.cap(P2(local(J, p, -4.4, -0.8)), P2(local(J, p, 4.8, -0.8)), 1.0, 1.0, NMat(BROWN, base=1), layer='belt')
    fig.box(P2(local(J, p, 3.4, -0.8)), 1.2, 1.2, NMat(BONE), layer='buckle')
    folk_head(fig, J, 'm', L)
    arm_(fig, J, 'N', shirt, skin, r=(2.5, 2.2, 2.1, 1.9), hr=1.9, fore=fore)


def folk_woman(fig, J, P, L=None):
    dress, skin, bt, hose = coat(2), NMat(SKIN, base=2), NMat(BROWN, base=1), NMat(BROWN, 'cloth', base=1)
    n, p = J['neck'], J['pel']
    fig.dark = 1
    arm_(fig, J, 'F', dress, skin, r=(2.2, 1.9, 1.8, 1.6), hr=1.7)
    leg_(fig, J, 'F', dress, hose, bt, r=(2.8, 2.3, 2.2, 1.9), toe=3.6, fh=2.6)
    fig.dark = 0
    leg_(fig, J, 'N', dress, hose, bt, r=(2.9, 2.4, 2.3, 2.0), toe=3.6, fh=2.6)
    fig.part = 'torso'
    torso_(fig, J, dress, front=4.4, back=-4.0)
    skirt_(fig, J, P, dress, length=L.get('hem', 9.0), front=5.4, back=-5.2, drip=(3, 1))  # the dress to the shins
    sw = P['cloth'] * 0.8
    if L.get('apron'):
        apron = NMat(LINEN, 'cloth', base=2)
        fig.poly([P2(local(J, p, 4.6, -3.0)), (p[0] + 5.8 + sw, p[1] + 7.6), (p[0] - 0.2 + sw, p[1] + 7.6), P2(local(J, p, -0.4, -3.0))], apron, layer='apron', drip=(5, 1))
        fig.poly([P2(local(J, n, 2.4, 2.4)), P2(local(J, n, 3.6, 3.4)), P2(local(J, p, 4.4, -3.0)), P2(local(J, p, 0.2, -3.0))], apron, layer='apron')
    fig.cap(P2(local(J, p, -4.4, -2.4)), P2(local(J, p, 4.6, -2.4)), 0.9, 0.9, NMat(BROWN, base=2), layer='belt')
    if L.get('shawl'):  # a wool shawl over the shoulders, fringed
        fig.poly([P2(local(J, n, -5.0, 0.4)), P2(local(J, n, 5.2, 1.0)), P2(local(J, n, 4.4, 5.6)), P2(local(J, n, -0.4, 9.0)), P2(local(J, n, -5.4, 6.4))], NMat(SHAWL, 'cloth', base=2), layer='shawl', drip=(8, 2))
    folk_head(fig, J, 'f', L)
    arm_(fig, J, 'N', dress, skin, r=(2.3, 2.0, 1.9, 1.7), hr=1.8)


def folk_child(fig, J, P, L=None):
    tunic, trews, bt, skin = coat(2), NMat(GREYWOOL, 'cloth', base=1), NMat(BROWN, base=1), NMat(SKIN, base=2)
    p = J['pel']
    fig.dark = 1
    arm_(fig, J, 'F', tunic, skin, r=(1.9, 1.6, 1.5, 1.4), hr=1.5)
    leg_(fig, J, 'F', trews, trews, bt, r=(2.2, 1.9, 1.8, 1.6), toe=3.2, fh=2.2)
    fig.dark = 0
    leg_(fig, J, 'N', trews, trews, bt, r=(2.3, 2.0, 1.9, 1.7), toe=3.2, fh=2.2)
    fig.part = 'torso'
    torso_(fig, J, tunic, front=3.8, back=-3.5)
    skirt_(fig, J, P, tunic, length=L.get('hem', 3.4), front=3.8, back=-3.6, drip=(4, 2))
    fig.cap(P2(local(J, p, -3.6, -0.8)), P2(local(J, p, 3.8, -0.8)), 0.8, 0.8, NMat(BROWN, base=1), layer='belt')
    folk_head(fig, J, 'c', L)
    arm_(fig, J, 'N', tunic, skin, r=(2.0, 1.7, 1.6, 1.5), hr=1.6)


def folk_anims():
    stand = pose(tN=0.05, kN=0.12, tF=-0.05, kF=0.12, aN=0.12, eN=0.25, aF=-0.1, eF=0.3)
    return {'idle': anim.idle_cycle(6, stand, 0.6), 'walk': anim.walk_cycle(8, stride=0.5, knee=0.9, bob=0.8, arm=0.4, lean=0.04)}


FOLK_LOOKS = {
    'folk_m0': (folk_man, FOLKM, dict(hair=HAIRB, hs='mop', beard='short', outer='jerkin', trews=GREYWOOL)),            # a farmer
    'folk_m1': (folk_man, FOLKM, dict(hair=HAIRW, hs='thin', beard='long', outer='robe', trews=GREYWOOL, hat=None)),      # an old man in a long tunic
    'folk_m2': (folk_man, FOLKM, dict(hair=HAIRR, hs='crop', beard=None, outer=None, trews=BROWN, bare=True, hat='flat')),  # a young hand, sleeves rolled, flat cap
    'folk_m3': (folk_man, FOLKM, dict(hair=HAIRK, hs='crop', beard='full', outer='apron', trews=GREYWOOL, bare=True)),    # a smith in a leather apron
    'folk_f0': (folk_woman, FOLKF, dict(hair=HAIRG, hs='braid', apron=True)),                                            # a braid, an apron
    'folk_f1': (folk_woman, FOLKF, dict(hair=HAIRK, hs='scarf', apron=False, shawl=True)),                               # a kerchief and a shawl
    'folk_f2': (folk_woman, FOLKF, dict(hair=HAIRW, hs='bun', apron=True, shawl=True, hem=10.0)),                        # grey hair in a bun
    'folk_f3': (folk_woman, FOLKF, dict(hair=HAIRR, hs='loose', apron=False)),                                           # red hair worn loose
    'folk_c0': (folk_child, FOLKC, dict(hair=HAIRB, hs='tuft')),
    'folk_c1': (folk_child, FOLKC, dict(hair=HAIRG, hs='pigtails', hem=5.0)),
    'folk_c2': (folk_child, FOLKC, dict(hair=HAIRK, hs='cap')),
}


def folk_spec(name):
    fn, body, look = FOLK_LOOKS[name]
    S = nspec(name, fn, body, folk_anims(), 48 if name[5] != 'c' else 40, 54 if name[5] != 'c' else 44, 48 if name[5] != 'c' else 38, cx=24 if name[5] != 'c' else 20, kind='none')
    S.look = look
    return S


# ---------------------------------------------------------------- specs
def nspec(name, painter, body, anims, w, h, gy, **k):
    S = Spec(name, painter, body, anims, w, h, gy, **k)
    S.fig = NFig
    return S


SPECS = {
    'hero': lambda: nspec('hero', hero, HERO, anim.hero_anims(), 64, 60, 54),
    'skeleton': lambda: nspec('skeleton', skeleton, anim.SKEL, anim.skeleton_anims(), 80, 72, 64, cx=36, wlen=15, wclass='RW_BLADE', rest=REST),
    'goblin': lambda: nspec('goblin', goblin, GOB, anim.goblin_anims(), 72, 60, 52, cx=32, wlen=10, wclass='RW_BLADE', rest=REST),
    'bomber': lambda: nspec('bomber', bomber, GOB, anim.bomber_anims(), 72, 60, 52, cx=32, wclass='RW_THROW', rest=REST),
    'redcap': lambda: nspec('redcap', redcap, anim.REDB, anim.redcap_anims(), 80, 60, 52, cx=32, wlen=20, wclass='RW_POLE', rest=REST),
    'risen': lambda: nspec('risen', risen, anim.GUARD, anim.risen_anims(), 110, 80, 72, cx=44, wlen=26, wclass='RW_POLE', rest=REST),
    'bat': lambda: nspec('bat', bat, None, anim.bat_anims(), 80, 64, 50, cx=40, kind='bat', rest=pose()),
    'slime': lambda: nspec('slime', slime, GOB, anim.slime_anims(), 72, 50, 44, cx=36, kind='none'),
    'wolf': lambda: nspec('wolf', wolf, anim.WOLFB, anim.quad_anims(0.55), 96, 64, 56, cx=48, kind='quad', rest=pose()),
    'serpent': lambda: nspec('serpent', serpent, GOB, serpent_anims(), 156, 56, 50, cx=112, kind='none'),
    'scorpion': lambda: nspec('scorpion', scorpion, GOB, scorpion_anims(), 170, 84, 78, cx=84, kind='none'),
    'raider': lambda: nspec('raider', raider, anim.GUARD, anim.risen_anims(), 110, 80, 72, cx=44, wlen=26, wclass='RW_POLE', rest=REST),
}

for _n in FOLK_LOOKS: SPECS[_n] = (lambda n=_n: folk_spec(n))


def install():
    """Replace anim.py's specs (and its hero painter and looks) with these."""
    anim.SPECS.update(SPECS)
    anim.hero_look = hero_look


def to_image(px, w, h):
    from PIL import Image
    im = Image.new('RGBA', (w, h), (0, 0, 0, 0))
    for j in range(h):
        for i in range(w):
            if px[j][i]: im.putpixel((i, j), tuple(px[j][i][0][:3]) + (255,))
    return im


def preview(outdir, names=None, looks=('wool',)):
    """Per creature: <name>_sheet.png (native size, transparent; a row per clip) and <name>.gif (6x on black)."""
    from PIL import Image
    os.makedirs(outdir, exist_ok=True)
    Z = 6
    for name, mk in SPECS.items():
        if names and name not in names: continue
        for look in (looks if name == 'hero' else (None,)):
            S = mk()
            if look: S.look = hero_look(look)
            tag = name + ('_' + look if look else '')
            rows, gif = [], []
            for an, frames in S.anims.items():
                imgs = [to_image(anim.bake_frame(S, P)[0][0], S.w, S.h) for P in frames]
                rows.append(imgs)
                if an in ('idle', 'walk', 'windup', 'strike', 'recover', 'cast', 'hurt'):
                    for im in imgs: gif += [im] * (1 if an == 'walk' else 2)
            cols = max(len(r) for r in rows)
            sheet = Image.new('RGBA', (cols * S.w, len(rows) * S.h), (0, 0, 0, 0))
            for r, imgs in enumerate(rows):
                for c, im in enumerate(imgs): sheet.paste(im, (c * S.w, r * S.h))
            sheet.save(f'{outdir}/{tag}_sheet.png')
            big = Image.new('RGB', (sheet.width, sheet.height), (0, 0, 0)); big.paste(sheet, (0, 0), sheet)
            big.resize((sheet.width * 4, sheet.height * 4), Image.NEAREST).save(f'{outdir}/{tag}_sheet_x4.png')
            fr = []
            for im in gif:
                bg = Image.new('RGB', (S.w, S.h), (0, 0, 0)); bg.paste(im, (0, 0), im)
                fr.append(bg.resize((S.w * Z, S.h * Z), Image.NEAREST))
            fr[0].save(f'{outdir}/{tag}.gif', save_all=True, append_images=fr[1:], duration=90, loop=0)
            print(tag, f'frame {S.w}x{S.h}, feet at ({S.cx:.0f}, {S.gy})')


if __name__ == '__main__':
    install()
    if sys.argv[1] == 'preview': preview(sys.argv[2], sys.argv[3:] or None, looks=('wool', 'leather', 'mail', 'lamellar', 'scale'))
    elif sys.argv[1] == 'emit': sys.stdout.write(anim.emit().replace('Generated by tools/anim.py', 'Generated by tools/figures.py (with tools/anim.py)'))

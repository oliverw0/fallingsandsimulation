# The white dire wolf, built the same way as the Viking (tools/viking.py): tapered capsules on a skeleton, z-buffered and banded,
# but shaggy - a ruff of fur round the neck, feathered legs, a plumed tail, strands of tone running along the coat and spiky
# tufts breaking every edge.  Local space: x forward, y down, ground y = 0, units = sprite pixels at S = 0.88.
#   python tools/wolf3d.py preview <dir>        sheet (4x)
import math, os, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import viking as VK
from viking import Canvas, Draw, V, dirv, two_bone, C

S = 0.78
FW, FH, GX, GND = 84, 64, 40, 54


def reg(name, ramp):
    VK.RAMPS[name] = ramp
    VK.MATS.append(name)
    VK.MI[name] = len(VK.MATS) - 1


reg('wfur', [C(44, 40, 58), C(104, 100, 116), C(160, 160, 160), C(212, 212, 200), C(250, 250, 234)])   # the user's whites: violet shadows, warm cream lights
reg('wnose', [C(24, 14, 16), C(62, 38, 30), C(110, 70, 52), C(150, 104, 80), C(190, 140, 110)])
reg('wpaw', [C(30, 26, 36), C(70, 62, 72), C(120, 112, 116), C(176, 170, 168), C(230, 226, 214)])
EYE = (205, 52, 64)


def rnd(i, k=0):
    h = (i * 73856093) ^ (k * 19349663) ^ 0x5bd1e995
    h = (h ^ (h >> 13)) * 1274126177
    return ((h ^ (h >> 16)) & 0xFFFF) / 65536.0


def tex_fur(t, n, X, Y):
    """Strands of tone running back along the coat, the odd lighter hair on top."""
    ph = (X * 2.0 + Y * 3.0 + (np.floor(Y / 3) * 5)) % 7
    st = np.where(ph < 1.0, -1, np.where((X * 3 + Y) % 11 < 1.0, 1, 0))
    return st, None


class W:
    """Draws the wolf's parts, each tagged so one can be rendered alone (the corpse's limbs)."""
    def __init__(w, d, only=None):
        w.d, w.only, w.part = d, only, 'torso'

    def on(w): return w.only is None or w.part in w.only
    def cap(w, *a, **k):
        if w.on(): w.d.cap(*a, **k)
    def ball(w, *a, **k):
        if w.on(): w.d.ball(*a, **k)
    def poly(w, *a, **k):
        if w.on(): w.d.poly(*a, **k)

    def tufts(w, A, B, rA, rB, n, size, z, seed, sweep=(-0.5, 0.5), dark=0, sides=(1, -1), mat='wfur'):
        """Spiky tufts of fur along the capsule A-B: short tapered strands raked back and outward on both sides."""
        A, B = V(*A), V(*B)
        ab = B - A
        L = float(np.hypot(*ab)) + 1e-9
        u = ab / L
        nrm = np.array([-u[1], u[0]])
        for i in range(n):
            t = (i + 0.5) / n + (rnd(i, seed) - 0.5) * 0.5 / n
            r = rA + (rB - rA) * t
            for sd in sides:
                j = rnd(i * 7 + (sd > 0), seed + 3)
                base = A + ab * t + nrm * sd * r * (0.75 + 0.25 * rnd(i, seed + 9))
                dirv_ = nrm * sd * (0.6 + 0.4 * j) + V(sweep[0], sweep[1] * (0.5 + j)) * 1.0
                dirv_ = dirv_ / (np.hypot(*dirv_) + 1e-9)
                ln = size * (0.55 + 0.5 * rnd(i * 3 + 1, seed + 5))
                zz = z + (2.5 if sd < 0 else -1.5) + (rnd(i, seed + 11) - 0.5) * 2
                w.cap(base, base + dirv_ * ln, size * 0.62, size * 0.3, zz, zz, mat, tex=tex_fur, dark=dark)   # a clump, not a spike


def wolf_joints(P):
    """The skeleton for a pose: positions of the points the parts hang from."""
    lean = P.get('lean', 0.0)
    bob = P.get('bob', 0.0)
    pel = V(-12 + P.get('rear', 0.0), -26 + bob + P.get('low_rear', 0.0))
    nek = V(11 + P.get('fore', 0.0), -28 + bob + lean)
    hb = nek + V(8, -5 + P.get('head', 0.0))
    nose = hb + V(16, 3.2 + P.get('head', 0.0) * 0.4)
    J = dict(pel=pel, neck=nek, hb=hb, nose=nose)
    # the front legs: shoulder, elbow (behind), paw
    for s, sd in (('N', 1.0), ('F', -1.0)):
        sh = nek + V(-2.0 + sd * 1.5, 3.0)
        fx, fl = P['f' + s.lower()] if False else P.get('fn' if s == 'N' else 'ff', (14.0, 0.0))
        paw = V(fx, -fl)
        el = two_bone(sh, paw + V(0, -3), 12.0, 12.0, 1)
        J['sh' + s], J['el' + s], J['ha' + s] = sh, el, paw
    # the hind legs: hip, stifle (forward), hock (back), paw
    for s, sd in (('N', 1.0), ('F', -1.0)):
        hip = pel + V(2.0 + sd * 1.5, 2.0)
        hx, hl = P.get('hn' if s == 'N' else 'hf', (-14.0, 0.0))
        paw = V(hx, -hl)
        hock = two_bone(hip, V(hx - 3.0, -9.5 - hl), 11.0, 11.0, -1)
        J['hip' + s], J['kn' + s], J['hock' + s], J['ft' + s] = hip, hock, hock, paw
        J['stifle' + s] = hip + (hock - hip) * 0.5 + V(5.0, -1.0)
    return J


def build(d, P, only=None):
    w = WolfDrawer(d, only)
    return w.build(P)


class WolfDrawer(W):
    def build(w, P):
        J = wolf_joints(P)
        pel, nek, hb, nose = J['pel'], J['neck'], J['hb'], J['nose']
        wag = P.get('wag', 0.0)
        # ---- the far legs first (a tone darker)
        for s in ('F',):
            w.leg_front(J, s, -7, 1)
            w.leg_hind(J, s, -7, 1)
        # ---- the tail: a bushy plume swept up and back
        w.part = 'tail'
        tb = pel + V(-5, -2)
        pts = [tb, tb + V(-7, -1 + wag * 0.4), tb + V(-13, -4 + wag), tb + V(-17, -9 + wag * 1.2), tb + V(-18.5 + wag * 0.4, -14 + wag * 1.2)]
        rad = [4.4, 5.8, 6.2, 5.6, 3.4]
        for i in range(len(pts) - 1):
            w.cap(pts[i], pts[i + 1], rad[i], rad[i + 1], -1, -1, 'wfur', tex=tex_fur)
            w.tufts(pts[i], pts[i + 1], rad[i], rad[i + 1], 4, 3.4, -1, 100 + i, sweep=(-0.7, -0.3))
        w.cap(pts[-1], pts[-1] + V(0.4 + wag * 0.3, -3.5), 3.4, 0.6, -1, -1, 'wfur', tex=tex_fur)
        # ---- the body: a deep chest tapering to the loins, a belly fringe and a back that bristles
        w.part = 'torso'
        w.cap(pel, nek, 5.8, 7.0, 0, 0, 'wfur', tex=tex_fur)
        w.ball(nek + V(1.0, 1.5), 7.6, 1, 'wfur', tex=tex_fur)                                  # the broad chest
        w.ball(pel + V(1, 0), 6.2, 0, 'wfur', tex=tex_fur)                                      # the haunch
        w.tufts(pel + V(0, -4), nek + V(0, -5), 2.0, 2.0, 11, 3.8, 2, 11, sweep=(-0.9, -0.5), sides=(1,))  # the bristling back
        w.tufts(pel + V(0, 3), nek + V(2, 4), 2.0, 2.0, 8, 3.0, 2, 12, sweep=(-0.4, 0.8), sides=(1,))      # the belly's fringe, hanging
        w.tufts(pel + V(2, 0), pel + V(-2, 3), 5.0, 5.0, 5, 3.6, 2, 13, sweep=(-0.9, 0.2))
        # ---- the neck's ruff: a great shaggy collar round the shoulders and throat
        w.part = 'head'
        w.cap(nek, hb, 7.0, 5.6, 1, 2, 'wfur', tex=tex_fur)
        w.tufts(nek + V(0, -3), hb + V(0, -2), 5.0, 4.0, 7, 4.6, 3, 21, sweep=(-0.8, -0.2), sides=(1,))
        w.tufts(nek + V(2, 4), hb + V(1, 3), 5.5, 4.5, 7, 5.0, 3, 22, sweep=(-0.4, 0.9), sides=(1,))
        w.tufts(nek + V(1, 1), nek + V(3, 6), 5.0, 5.0, 6, 4.2, 4, 23, sweep=(0.3, 0.9))
        # ---- the head: skull, muzzle, jaw, nose, ears
        sk = hb + V(4.5, -0.5)
        w.ball(sk, 6.4, 4, 'wfur', tex=tex_fur)
        jaw = float(P.get('jaw', 0.0))
        mz0, mz1 = sk + V(2.0, 0.6), nose
        w.cap(mz0, mz1, 4.8, 3.0, 5, 5, 'wfur', tex=tex_fur)                                      # the muzzle
        w.cap(mz0 + V(0, 2.0), mz1 + V(-1.0, 2.0 + jaw), 3.2, 2.0, 4, 4, 'wfur', tex=tex_fur)       # the lower jaw (drops when it bites)
        w.ball(nose + V(0.8, -0.8), 2.0, 8, 'wnose')
        w.tufts(sk + V(-1, 3), sk + V(3, 5), 3.0, 3.0, 4, 3.4, 6, 31, sweep=(-0.5, 0.9), sides=(1,))   # cheek fur
        for sd, z, dk in ((1, 7, 0), (-1, -5, 1)):
            e0 = sk + V(-0.8 + sd * -2.0, -4.6)
            e1 = e0 + V(0.8 + sd * 1.0, -7.5)
            w.cap(e0, e1, 3.6, 1.5, z, z, 'wfur', tex=tex_fur, dark=dk)                          # a short, broad, rounded ear                           # an ear: pointed, tufted
            w.tufts(e0, e1, 3.2, 1.6, 3, 2.4, z, 40 + sd, sweep=(-0.3, -0.5), sides=(1,), dark=dk)
        ex, ey = w.d.P(sk + V(2.6, -1.4))
        if w.only is None or 'head' in w.only:
            w.d.cv.eye[(int(ex), int(ey))] = EYE
        # ---- the near legs
        w.leg_front(J, 'N', 8, 0)
        w.leg_hind(J, 'N', 8, 0)
        return J

    def leg_front(w, J, s, z, dark):
        w.part = 'uarm' if s == 'N' else 'farL'
        sh, el, paw = J['sh' + s], J['el' + s], J['ha' + s]
        w.cap(sh, el, 5.0, 3.6, z, z, 'wfur', tex=tex_fur, dark=dark)
        w.tufts(sh, el, 4.2, 3.0, 3, 3.0, z, 51 + (s == 'N'), sweep=(-0.9, 0.5), dark=dark)
        w.part = 'farm' if s == 'N' else 'farL'
        w.cap(el, paw + V(0, -1.6), 3.5, 2.6, z, z, 'wfur', tex=tex_fur, dark=dark)
        w.tufts(el, paw + V(0, -1.6), 3.0, 2.0, 3, 2.6, z, 61 + (s == 'N'), sweep=(-0.7, 0.8), dark=dark)
        w.cap(paw + V(-0.5, -1.8), paw + V(4.0, -0.9), 3.0, 2.6, z, z, 'wpaw', dark=dark)

    def leg_hind(w, J, s, z, dark):
        hip, hock, paw = J['hip' + s], J['kn' + s], J['ft' + s]
        stifle = J['stifle' + s]
        w.part = 'thigh' if s == 'N' else 'farL'
        w.ball(hip + V(0.5, 1.5), 5.8, z, 'wfur', tex=tex_fur, dark=dark)
        w.cap(hip + V(0.5, 1.5), stifle, 5.6, 4.0, z, z, 'wfur', tex=tex_fur, dark=dark)
        w.tufts(hip, stifle, 5.0, 3.6, 4, 3.8, z, 71 + (s == 'N'), sweep=(-0.9, 0.4), dark=dark)
        w.part = 'shin' if s == 'N' else 'farL'
        w.cap(stifle, hock, 4.0, 3.0, z, z, 'wfur', tex=tex_fur, dark=dark)
        w.cap(hock, paw + V(0, -1.6), 2.9, 2.2, z, z, 'wfur', tex=tex_fur, dark=dark)
        w.tufts(hock, paw + V(0, -1.6), 2.6, 1.8, 3, 3.0, z, 81 + (s == 'N'), sweep=(-0.8, 0.6), dark=dark)
        w.cap(paw + V(-0.5, -1.8), paw + V(3.8, -0.9), 3.0, 2.6, z, z, 'wpaw', dark=dark)


def render(P, only=None, scale=S):
    cv = Canvas(FW, FH, GND, scale)
    d = Draw(cv, GX, 1, scale)
    J = WolfDrawer(d, only).build(P)
    rgb, tag, op = cv.render(rim_side=1)
    return rgb, tag, op, J, d


# ---------------------------------------------------------------- poses and clips
def pose(**k):
    p = dict(bob=0.0, lean=0.0, wag=0.0, head=0.0, jaw=0.0, rear=0.0, fore=0.0, low_rear=0.0,
             fn=(19.0, 0.0), ff=(7.0, 0.0), hn=(-17.0, 0.0), hf=(-4.0, 0.0))
    p.update(k)
    return p


def idle(n=4):
    out = []
    for i in range(n):
        t = i / n * 2 * math.pi
        out.append(pose(bob=0.6 * (0.5 - 0.5 * math.cos(t)), wag=1.2 * math.sin(t), head=0.6 * math.sin(t - 0.7)))
    return out


def walk(n=8):
    out = []
    for i in range(n):
        ph = i / n * 2 * math.pi
        s, c = math.sin(ph), math.cos(ph)
        lift = lambda v: 4.0 * max(0.0, v)
        # a trot: diagonal pairs swing together (near fore with far hind)
        out.append(pose(bob=-abs(s) * 1.4 + 0.5, wag=2.0 * math.sin(ph * 2), head=1.0 * s,
                        fn=(19.0 + 6.0 * s, lift(c)), hf=(-4.0 + 6.0 * s, lift(c)),
                        ff=(7.0 - 6.0 * s, lift(-c)), hn=(-17.0 - 6.0 * s, lift(-c))))
    return out


def windup():
    return [pose(bob=2.0, low_rear=2.0, lean=3.0, head=3.0, fore=-2.0, rear=-1.0, fn=(18.0, 0.0), ff=(10.0, 0.0), hn=(-17.0, 0.0), hf=(-8.0, 0.0), wag=-1.0),
            pose(bob=3.5, low_rear=3.0, lean=5.0, head=4.0, fore=-3.0, rear=-2.0, fn=(19.0, 0.0), ff=(11.0, 0.0), hn=(-18.0, 0.0), hf=(-9.0, 0.0), wag=-2.0)]


def strike():
    return [pose(bob=-4.0, lean=-2.0, head=-1.0, jaw=3.0, fn=(21.0, 5.0), ff=(17.0, 6.0), hn=(-20.0, 4.0), hf=(-16.0, 5.0), wag=3.0),
            pose(bob=-7.0, lean=-3.0, head=-2.0, jaw=5.0, fn=(24.0, 8.0), ff=(20.0, 9.0), hn=(-23.0, 6.0), hf=(-19.0, 7.0), wag=4.0),
            pose(bob=-3.0, lean=1.0, head=0.0, jaw=2.0, fn=(19.0, 3.0), ff=(15.0, 4.0), hn=(-17.0, 2.0), hf=(-13.0, 3.0), wag=2.0)]


def recover():
    return [pose(bob=2.0, lean=1.0, fn=(16.0, 0.0), ff=(11.0, 0.0), hn=(-14.0, 0.0), hf=(-9.0, 0.0)), pose(bob=0.8)]


def hurt():
    return [pose(bob=1.0, lean=3.0, head=4.0, jaw=2.0, fore=-2.0, wag=-2.0, fn=(12.0, 0.0), ff=(8.0, 0.0)), pose(bob=0.4, lean=1.5, head=2.0, wag=-1.0)]


def land():
    return [pose(bob=2.5, low_rear=1.5, lean=2.0, fn=(16.0, 0.0), ff=(11.0, 0.0), hn=(-14.0, 0.0), hf=(-9.0, 0.0))]


CLIPSET = [('idle', idle()), ('walk', walk()), ('windup', windup()), ('strike', strike()), ('recover', recover()), ('hurt', hurt()), ('land', land())]


# ---------------------------------------------------------------- output
JOINT_NAMES = ['neck', 'pel', 'headb', 'headt', 'shF', 'elF', 'haF', 'shN', 'elN', 'haN', 'hipF', 'knF', 'ftF', 'hipN', 'knN', 'ftN', 'wb', 'wt']


def joints_list(J, P, d):
    wb = J['pel'] + V(-5, -2)
    wt = wb + V(-18.5, -14 + P.get('wag', 0.0) * 1.2)
    pts = dict(neck=J['neck'], pel=J['pel'], headb=J['hb'], headt=J['nose'], shF=J['shF'], elF=J['elF'], haF=J['haF'], shN=J['shN'], elN=J['elN'],
               haN=J['haN'], hipF=J['hipF'], knF=J['knF'], ftF=J['ftF'], hipN=J['hipN'], knN=J['knN'], ftN=J['ftN'], wb=wb, wt=wt)
    return [d.P(pts[k]) for k in JOINT_NAMES]


def grid_of(rgb, tag, op):
    h, w = op.shape
    g = [[None] * w for _ in range(h)]
    for y in range(h):
        for x in range(w):
            if op[y, x]: g[y][x] = (tuple(int(v) for v in rgb[y, x]), int(tag[y, x]))
    return g


def emit(out):
    import anim
    import userart
    frames = []
    for clip, poses in CLIPSET:
        for P in poses:
            rgb, tag, op, J, d = render(P)
            frames.append((anim.CLIPS.index(clip), grid_of(rgb, tag, op), joints_list(J, P, d)))
    # the corpse's parts, cut from the standing drawing
    P0 = pose()
    slots = {'HEAD': ({'head'}, 'headb', 'headt'), 'TORSO': ({'torso'}, 'neck', 'pel'), 'UARM': ({'uarm'}, 'shN', 'elN'), 'FARM': ({'farm'}, 'elN', 'haN'),
             'THIGH': ({'thigh'}, 'hipN', 'knN'), 'SHIN': ({'shin'}, 'knN', 'ftN'), 'TAIL': ({'tail'}, 'wb', 'wt')}
    rgb0, tag0, op0, J0, d0 = render(P0)
    pts = dict(zip(JOINT_NAMES, joints_list(J0, P0, d0)))
    cells = []
    for slot in ('HEAD', 'TORSO', 'UARM', 'FARM', 'THIGH', 'SHIN', 'WEAPON', 'SHIELD', 'WING', 'TAIL'):
        if slot not in slots: cells.append('{nullptr, 0, 0, 0, 0}'); continue
        only, a, b = slots[slot]
        rgb, tag, op, _, _ = render(P0, only)
        if not op.any(): cells.append('{nullptr, 0, 0, 0, 0}'); continue
        g = grid_of(rgb, tag, op)
        x0, y0, x1, y1 = anim.crop([g])
        pal = anim.palette_of([g])
        nm = f'WOLF_{slot}'
        out.append(anim.pal_c(f'HDP_A_{nm}', pal))
        out.append(f'static const char* const HDR_A_{nm}[] = {{')
        for j in range(y0, y1): out.append('    "' + ''.join('.' if g[j][i] is None else anim.ALPHABET[pal.index(g[j][i])] for i in range(x0, x1)) + '",')
        out.append('};')
        out.append(f'static const HDSprite HD_A_{nm} = {{{x1 - x0}, {y1 - y0}, HDP_A_{nm}, HDR_A_{nm}}};')
        pa, pb = pts[a], pts[b]
        cells.append(f'{{&HD_A_{nm}, {pa[0] - x0:.2f}f, {pa[1] - y0:.2f}f, {pb[0] - x0:.2f}f, {pb[1] - y0:.2f}f}}')
    out.append(f'static const RigSpec RIG_A_WOLF = {{RK_QUAD, {{{", ".join(cells)}}}, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, RW_NONE, 0}};')
    out.append('')
    anim.emit_sheet(out, 'WOLF', None, frames, GX, GND)


def preview(outdir):
    from PIL import Image
    os.makedirs(outdir, exist_ok=True)
    cols = 8
    cells = []
    for clip, poses in CLIPSET:
        for P in poses:
            rgb, tag, op, J, d = render(P)
            im = Image.new('RGB', (FW, FH), (24, 18, 36))
            px = im.load()
            for y in range(FH):
                for x in range(FW):
                    if op[y, x]: px[x, y] = tuple(int(v) for v in rgb[y, x])
            cells.append(im)
    rows = (len(cells) + cols - 1) // cols
    sheet = Image.new('RGB', (cols * FW, rows * FH), (24, 18, 36))
    for k, im in enumerate(cells): sheet.paste(im, ((k % cols) * FW, (k // cols) * FH))
    sheet.resize((sheet.width * 3, sheet.height * 3), Image.NEAREST).save(f'{outdir}/wolf_sheet.png')
    big = cells[0].resize((FW * 8, FH * 8), Image.NEAREST)
    big.save(f'{outdir}/wolf_idle_big.png')


if __name__ == '__main__':
    if sys.argv[1] == 'preview': preview(sys.argv[2])

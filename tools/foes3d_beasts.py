# The non-humanoid starter foes in the 3D-primitive style: giant bat, acid slime, giant scorpion, sea serpent.
# Same interface as tools/foes3d.py's biped: build_<name>(w, P) -> joints dict; poses/clips as dicts; canvas data in foes3d_run.py.
import math, os, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import viking as VK
from viking import V, dirv, two_bone
import foes3d as F
from foes3d import FoeW, hnoise, blobs

MI = VK.MI


# ================================================================ giant bat
def build_bat(w, P):
    d = w.d
    bob = P.get('bob', 0.0)
    body = V(0, -14 + bob)
    head = body + V(5.0, -3.5 + P.get('head', 0.0))
    # far wing first (a tone darker), then the body, then the near wing
    def wing(sd, z, dk):
        dv = lambda a: dirv(180 - a)       # the wings sweep back from the shoulder (the bat faces right)
        w.part = 'uarm' if sd > 0 else 'farL'
        k = 1.0 if sd > 0 else 0.86
        ang = P['wing'] + (0 if sd > 0 else 16)                    # degrees: -90 straight up ... +70 down and back
        fold = P.get('fold', 0)
        S = body + V(-0.6 - (sd < 0) * 1.4, -1.8 - (sd < 0) * 0.6)
        Wr = S + dv(ang) * 8.5 * k                                # the wrist
        offs, lens = (-12, 18, 48, 78), (11.5, 12.5, 11.0, 8.5)
        tips = [Wr + dv(ang + o - fold * 0.4) * l * k for o, l in zip(offs, lens)]
        back = S + dv(ang + 112 - fold * 0.2) * 7.0 * k             # where the membrane meets the body
        mem = [S, Wr, tips[0]]
        for i in range(3):                                          # scalloped between each pair of finger tips
            mid = (tips[i] + tips[i + 1]) * 0.5
            mem += [mid * 0.78 + Wr * 0.22 + V(0, 0.8), tips[i + 1]]
        mem += [(tips[3] + back) * 0.5 * 0.82 + Wr * 0.18, back]
        w.poly(mem, z, 'membrane', dark=dk, aux_fn=lambda X, Y: np.where(hnoise(np.floor(X / 2), np.floor(Y / 2), 4) > 0.9, -1, 0))
        w.cap(S, Wr, 1.2, 0.9, z + 0.7, z + 0.7, 'batfur', dark=dk)
        for t_ in tips: w.cap(Wr, t_, 0.7, 0.25, z + 0.7, z + 0.7, 'batfur', dark=dk)
        return S, tips[0]
    rF, tF = wing(-1, -5, 1)
    w.part = 'torso'
    w.ball(body, 4.2, 0, 'batfur', tex=lambda t, n, X, Y: (np.where(blobs(X, Y, 61, 2) > 0.78, 1, 0) - np.where(blobs(X, Y, 62, 2) > 0.85, 1, 0), None))
    w.cap(body + V(-2.0, 1.5), body + V(-6.0, 3.5), 2.6, 1.0, -1, -1, 'batfur')          # the feet / tail end
    w.cap(body + V(1.0, 2.0), body + V(1.2, 6.0), 0.8, 0.6, 2, 2, 'belly')               # a clutching foot
    w.part = 'head'
    w.ball(head, 3.1, 3, 'batfur')
    w.cap(head + V(2.0, 0.6), head + V(5.0, 1.4), 1.8, 1.0, 4, 4, 'batfur')               # the snout
    for sd, z, dk in ((1, 5, 0), (-1, -2, 1)):
        e0 = head + V(-0.4 - (sd < 0) * 1.2, -2.2)
        w.cap(e0, e0 + V(-1.4 + sd * 0.6, -5.4), 1.7, 0.3, z, z, 'batfur', dark=dk)         # big pointed ears
    if w.on():
        ex, ey = d.P(head + V(1.6, -0.6))
        d.cv.eye[(int(ex), int(ey))] = F.EYE_R
        fx, fy = d.P(head + V(4.2, 2.0))
        d.cv.eye[(int(fx), int(fy))] = (240, 236, 220)                                       # a fang
    rN, tN = wing(1, 1.5, 0)
    return dict(body=body, head=head, rN=rN, tN=tN, rF=rF, tF=tF)


def bat_poses():
    out = {}
    out['idle'] = [dict(wing=wg, bob=b, fold=0) for wg, b in ((-70, 1.0), (-35, 0.4), (5, -0.4), (45, -1.0), (15, -0.2), (-40, 0.6))]
    out['windup'] = [dict(wing=-80, bob=1.5, head=1.0, fold=-10), dict(wing=-88, bob=2.2, head=1.5, fold=-20)]
    out['strike'] = [dict(wing=30, bob=-0.5, head=-2.0, fold=40), dict(wing=55, bob=-1.5, head=-3.0, fold=60), dict(wing=40, bob=-0.8, head=-2.0, fold=45)]
    out['recover'] = [dict(wing=-20, bob=0.4), dict(wing=-55, bob=0.8)]
    out['hurt'] = [dict(wing=-10, bob=-1.0, head=2.0, fold=30), dict(wing=-40, bob=-0.4, head=1.0)]
    out['land'] = [dict(wing=-30, bob=0.5)]
    return out


# ================================================================ acid slime
def build_slime(w, P):
    d = w.d
    sq = P.get('sq', 1.0)                      # 1 = at rest; <1 squashed flat and wide, >1 stretched tall
    lift = P.get('lift', 0.0)
    ry = 8.4 * sq
    rx = 11.5 / math.sqrt(sq)
    cy = -ry - lift
    j = P.get('lean', 0.0)
    w.part = 'torso'
    # a puddle's edge, then the dome of the body: a stadium (a wide capsule) shaded like a blob
    if lift < 0.5:
        w.cap(V(-rx - 1.0, -1.4), V(rx + 1.0 + j, -1.4), 1.4, 1.4, -2, -2, 'slimemat', dark=1)
    w.cap(V(-rx + ry + 1.0, cy), V(rx - ry + 1.0 + j, cy), ry, ry, 0, 0, 'slimemat',
          tex=lambda t, n, X, Y: (np.where(blobs(X, Y, 71, 2) > 0.88, 1, 0), None))
    # the nucleus: a darker core with bubbles
    w.cap(V(-3.0 + j * 0.4, cy + 1.6), V(1.5 + j * 0.4, cy + 1.2), ry * 0.5, ry * 0.45, 6, 6, 'slimemat', tex=lambda t, n, X, Y: (np.full_like(X, -1, int), None))
    for bx, by, br in ((-5.4, -0.4, 1.3), (4.6, 1.6, 1.0), (-1.0, -3.4, 0.9), (6.5, -3.0, 0.8)):
        w.ball(V(bx + j * 0.4, cy + by * sq), br, 8, 'slimemat', tex=lambda t, n, X, Y: (np.full_like(X, 2, int), None))
    # a bright glint, top left
    w.ball(V(-rx * 0.45, cy - ry * 0.5), 1.6, 12, 'slimemat', tex=lambda t, n, X, Y: (np.full_like(X, 4, int), None))
    # the face: two eyes and a wide mouth, set toward the front
    if w.on():
        for ex, ey in ((rx * 0.18, cy - 1.0), (rx * 0.5, cy - 0.6)):
            px, py = d.P(V(ex + j * 0.5, ey))
            d.cv.eye[(int(px), int(py))] = (236, 244, 220)
            d.cv.eye[(int(px) + 1, int(py))] = (20, 40, 20)
            d.cv.eye[(int(px), int(py) - 1)] = (236, 244, 220)
            d.cv.eye[(int(px) + 1, int(py) - 1)] = (20, 40, 20)
        for k in range(5):
            px, py = d.P(V(rx * 0.12 + k * 1.2 + j * 0.5, cy + 2.4 + (0.4 if k in (0, 4) else 0.0)))
            d.cv.eye[(int(px), int(py))] = (24, 54, 24)
    return dict(c=V(0, cy), rx=rx, ry=ry)


def slime_poses():
    out = {}
    out['idle'] = [dict(sq=s) for s in (1.0, 1.04, 1.0, 0.95)]
    out['walk'] = [dict(sq=0.8, lift=0, lean=-1), dict(sq=1.25, lift=2, lean=1), dict(sq=1.3, lift=8, lean=2), dict(sq=1.15, lift=10, lean=2),
                   dict(sq=0.85, lift=2, lean=1), dict(sq=0.72, lift=0, lean=0)]
    out['windup'] = [dict(sq=0.8, lean=-2), dict(sq=0.62, lean=-3)]
    out['strike'] = [dict(sq=1.3, lift=3, lean=3), dict(sq=1.45, lift=7, lean=5), dict(sq=1.2, lift=3, lean=4)]
    out['recover'] = [dict(sq=0.8, lean=1), dict(sq=0.95)]
    out['hurt'] = [dict(sq=0.6, lean=-2), dict(sq=0.85, lean=-1)]
    out['land'] = [dict(sq=0.72)]
    return out


# ================================================================ giant scorpion
def build_scorpion(w, P):
    d = w.d
    bob = P.get('bob', 0.0)
    ph = P.get('phase', 0.0)
    base = V(0, -13 + bob)
    abd = base + V(-12, -1)
    thx = base + V(0, 0)
    hdc = base + V(11, 1.5)
    # far legs (a tone darker), then the body, then the claws, the tail, the near legs
    def legs(sd, z, dk):
        for k in range(4):
            rx = -9 + k * 6.5
            root = base + V(rx, 2.5)
            sw = math.sin(ph + k * 1.7 + (0 if sd > 0 else math.pi)) * P.get('stride', 3.0)
            lift = max(0.0, math.cos(ph + k * 1.7 + (0 if sd > 0 else math.pi))) * P.get('stride', 3.0) * 0.8
            foot = V(rx + 4 + sw + (k - 1.5) * 2.4, -lift)
            knee = two_bone(root, foot, 9.0, 9.5, -1) if True else root
            w.cap(root, knee, 1.7, 1.3, z, z, 'chitin', dark=dk)
            w.cap(knee, foot, 1.3, 0.6, z, z, 'chitin', dark=dk)
    w.part = 'farL'
    legs(-1, -6, 1)
    w.part = 'torso'
    tex_c = lambda t, n, X, Y: ((np.where((np.floor(X) % 5) == 0, -1, 0) + np.where(blobs(X, Y, 81, 2) > 0.88, 1, 0)), None)
    w.ball(abd, 7.0, 0, 'chitin', tex=tex_c)                                              # the abdomen
    w.ball(abd + V(5.5, 0.5), 6.0, 1, 'chitin', tex=tex_c)
    w.ball(thx + V(1.5, 0), 6.6, 2, 'chitin', tex=tex_c)                                  # the carapace
    w.cap(thx + V(-2, -3.2), thx + V(7, -3.6), 1.6, 1.2, 6, 6, 'chitin', tex=lambda t, n, X, Y: (np.full_like(X, 1, int), None))      # a ridge down the back
    w.ball(hdc, 4.6, 3, 'chitin', tex=tex_c)
    if w.on():
        for ex, ey in ((hdc[0] + 2.4, hdc[1] - 1.8), (hdc[0] + 1.0, hdc[1] - 2.8)):
            px, py = d.P(V(ex, ey)); d.cv.eye[(int(px), int(py))] = F.EYE_R
    # the claws: an arm out front, the pincer open or shut
    w.part = 'uarm'
    cl = P.get('claw', 18.0)
    for sd, z, dk in ((1, 7, 0), (-1, -4, 1)):
        sh = hdc + V(3.0, 2.2 + (sd < 0) * -0.6)
        el = sh + V(7.0, -1.2 + P.get('arm', 0.0))
        w.cap(sh, el, 2.0, 1.6, z, z, 'chitin', dark=dk)
        w.ball(el + V(2.5, 0), 3.2, z, 'chitin', dark=dk)
        w.cap(el + V(2.5, -1.0), el + V(2.5, -1.0) + dirv(-cl) * 7.5, 2.0, 0.5, z + 0.5, z + 0.5, 'chitin', dark=dk)
        w.cap(el + V(2.5, 1.0), el + V(2.5, 1.0) + dirv(cl) * 7.0, 1.7, 0.4, z + 0.5, z + 0.5, 'chitin', dark=dk)
    # the tail: segments arching up over the back, the stinger at the end
    w.part = 'tail'
    tail = P.get('tail', 0.0)       # degrees of how far forward the tail is thrown (0 = resting arch)
    pts = [abd + V(-5.0, -3.0)]
    ang = -150.0 + tail * 0.9
    for k in range(6):
        ang += 22.0 - tail * 0.2
        pts.append(pts[-1] + dirv(ang) * 6.3)
    for k in range(len(pts) - 1):
        r0 = 3.2 - k * 0.22
        w.cap(pts[k], pts[k + 1], r0, r0 - 0.25, 2, 2, 'chitin', tex=tex_c)
    sting = pts[-1] + dirv(ang + 40) * 4.0
    w.cap(pts[-1], sting, 2.0, 0.3, 3, 3, 'chitin', tex=lambda t, n, X, Y: (np.full_like(X, 2, int), None))
    w.part = 'torso'
    legs(1, 6, 0)
    return dict(base=base, abd=abd, hdc=hdc, tail=pts[-1])


def scorpion_poses():
    out = {}
    out['idle'] = [dict(bob=0.0, tail=0, claw=18), dict(bob=0.4, tail=3, claw=14), dict(bob=0.2, tail=5, claw=20), dict(bob=0.4, tail=3, claw=15)]
    out['walk'] = [dict(phase=i / 8 * 2 * math.pi, bob=0.6 * abs(math.sin(i / 8 * 2 * math.pi)), tail=2 * math.sin(i / 8 * 2 * math.pi), claw=16, stride=3.2) for i in range(8)]
    out['windup'] = [dict(tail=-14, claw=34, arm=-2.0, bob=1.0), dict(tail=-26, claw=40, arm=-3.0, bob=1.6)]
    out['strike'] = [dict(tail=30, claw=26, arm=0.5, bob=-0.4), dict(tail=60, claw=14, arm=1.0, bob=-0.8), dict(tail=46, claw=18, arm=0.5, bob=-0.4)]
    out['recover'] = [dict(tail=14, claw=18, bob=0.4), dict(tail=4, claw=18)]
    out['hurt'] = [dict(tail=-10, claw=34, bob=1.2), dict(tail=-4, claw=26, bob=0.5)]
    out['land'] = [dict(bob=1.6, tail=4, claw=18)]
    return out


# ================================================================ sea serpent
def build_serpent(w, P):
    d = w.d
    ph = P.get('phase', 0.0)
    amp = P.get('amp', 3.2)
    lead = P.get('lead', 0.0)                    # how far the head is thrown forward
    head_lift = P.get('head', 0.0)
    N = 22
    pts, rad = [], []
    for i in range(N + 1):
        u = i / N                                # 0 at the head ... 1 at the tail
        x = lead - u * 96.0 * P.get('len', 1.0)
        y = -8.5 - amp * math.sin(ph + u * 9.0) * (0.35 + u) - P.get('rise', 0.0) * math.exp(-u * 7.0)
        pts.append(V(x, y))
        rad.append(5.0 if u < 0.06 else 5.6 * (1 - u * 0.15) * (1.0 if u < 0.7 else 1 - (u - 0.7) * 2.6) + 0.3)
    w.part = 'torso'
    tex = lambda t, n, X, Y: (np.where(((np.floor(X) + np.floor(Y)) % 4) == 0, -1, 0) - np.where(blobs(X, Y, 91, 2) > 0.9, 1, 0),
                              np.where(n[1] > 0.45, MI['belly'], -1))
    for i in range(N, 0, -1):
        w.cap(pts[i], pts[i - 1], max(0.4, rad[i]), max(0.4, rad[i - 1]), 0, 0, 'scale', tex=tex)
    # a crest of fins along the back
    for i in range(2, N - 4, 2):
        a, b = pts[i], pts[i + 1]
        mid = (a + b) * 0.5 + V(0, -rad[i] + 0.4)
        w.poly([mid + V(1.8, 0.8), mid + V(-1.0, -4.5), mid + V(-2.6, 0.8)], 1, 'scale', aux_fn=lambda X, Y: np.where(hnoise(np.floor(X), np.floor(Y), 5) > 0.7, 1, 0))
    # the head: a long skull, jaws that part, a yellow eye and fangs
    w.part = 'head'
    h0 = pts[0] + V(3.0, -head_lift * 0.4)
    w.ball(h0, 5.6, 3, 'scale', tex=tex)
    jaw = P.get('jaw', 0.0)
    w.cap(h0 + V(2.0, -1.0), h0 + V(10.5, -0.6 - jaw * 0.3), 3.6, 2.2, 4, 4, 'scale', tex=tex)
    w.cap(h0 + V(2.0, 1.4), h0 + V(10.0, 1.8 + jaw), 2.6, 1.6, 4, 4, 'belly')
    w.ball(h0 + V(11.0, -0.8 - jaw * 0.3), 1.6, 6, 'scale')
    if w.on():
        ex, ey = d.P(h0 + V(3.0, -2.6))
        d.cv.eye[(int(ex), int(ey))] = F.EYE_Y
        d.cv.eye[(int(ex) + 1, int(ey))] = (20, 20, 10)
        for k in range(3):
            fx, fy = d.P(h0 + V(5.0 + k * 2.2, 2.4 + jaw * 0.6))
            d.cv.eye[(int(fx), int(fy))] = (244, 240, 224)
    return dict(head=h0, pts=pts)


def serpent_poses():
    out = {}
    out['idle'] = [dict(phase=i / 6 * 2 * math.pi, amp=2.6) for i in range(6)]
    out['walk'] = [dict(phase=i / 8 * 2 * math.pi * 2, amp=3.6) for i in range(8)]
    out['windup'] = [dict(phase=0.5, amp=4.6, lead=-8, head=6, rise=6, jaw=1.0, len=0.96), dict(phase=0.9, amp=5.4, lead=-14, head=9, rise=10, jaw=2.0, len=0.92)]
    out['strike'] = [dict(phase=1.4, amp=2.0, lead=6, head=0, jaw=4.0), dict(phase=1.6, amp=1.2, lead=14, head=-2, jaw=6.0, len=1.04), dict(phase=1.8, amp=2.0, lead=8, jaw=3.0)]
    out['recover'] = [dict(phase=2.2, amp=3.0, lead=2, jaw=1.5), dict(phase=2.8, amp=2.6)]
    out['hurt'] = [dict(phase=3.4, amp=5.0, lead=-6, head=4, jaw=3.0), dict(phase=3.8, amp=4.0, lead=-2, jaw=1.0)]
    out['land'] = [dict(phase=0.0, amp=3.0)]
    return out

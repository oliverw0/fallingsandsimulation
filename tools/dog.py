# Indi, the user's English setter, who wanders Hearthwick. Painted with tools/figures.py's painter on anim.py's
# four-legged skeleton, like the wolf; emitted to its own header so sprites_anim.h never needs re-emitting:
#   python tools/dog.py emit > sprites_dog.h
#   python tools/dog.py preview <dir>
# From her photos: a white coat ticked all over in blue-grey (blue belton), darker clouds over the back and haunch,
# a dark patch round the eye, long wavy ears gone grey-black, a black nose, soft brown eyes, a red collar with a
# green tag, feathering under the belly, behind the legs and in a long flag of a tail.
import math, sys, os
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import anim, figures
from anim import Quad, pose, keys, dir_, P2
from figures import R, NMat, hsh, limb, nspec, FACE

COAT = R('#55565e', '#83848a', '#b4b3ad', '#d6d4ca', '#ecebe0')
TICK = R('#1b1c22', '#292b33', '#3b3e48', '#525662', '#6a6e7a')  # blue-grey ticking, the ears
NOSE = R('#0e0d10', '#1a181b', '#2a2629', '#3e383a', '#544b4c')
TONGUE = R('#5a2630', '#86404a', '#b05a64', '#c87880', '#dc9aa0')
COLLAR = R('#3a0e12', '#5e161c', '#8a2028', '#a82e34', '#c04440')
TAG = R('#3e5a10', '#5e8418', '#86b022', '#a6d03a', '#c4e860')
EYE = (58, 34, 24)

SETTER = Quad(body=15.5, ua=5.8, fa=5.6, th=6.2, sh=6.0, neck=5.2, head=9.0, tail=11.0, neck_ang=0.74, head_ang=0.47)


def ticks(fig, a, b, n, r, seed, spread):
    """Flecks and clouds of ticking scattered along a to b (in the part's own frame, so they ride with it)."""
    a, b = np.array(a, float), np.array(b, float)
    ax = b - a
    nrm = np.array([-ax[1], ax[0]]) / (np.linalg.norm(ax) + 1e-9)
    for k in range(n):
        u, v, s = hsh(k, 1, seed), hsh(k, 2, seed) * 2 - 1, hsh(k, 3, seed)
        c = a + ax * u + nrm * v * spread
        fig.ell(P2(c), r * (0.5 + 0.8 * s), r * (0.5 + 0.7 * s), NMat(TICK, 'flat', base=2 + (s > 0.6)), layer='tick')


def setter(fig, J, P, L=None):
    coat, under, ear = NMat(COAT, 'fur', base=3), NMat(COAT, 'fur', base=4), NMat(TICK, 'hair', base=3)
    S, Hp = J['neck'], J['pel']

    def leg(s, front):
        a, b, c = ('sh', 'el', 'ha') if front else ('hip', 'kn', 'ft')
        fig.part = ('uarm' if front else 'thigh') + s
        limb(fig, J[a + s], J[b + s], 3.0 if front else 4.0, 2.0, coat, layer='body' if s == 'N' else None)
        fig.part = ('farm' if front else 'shin') + s
        limb(fig, J[b + s], J[c + s], 1.6, 1.3, coat)
        # feathering behind the leg, hanging in a ragged fringe
        back = J[b + s] + np.array([-1.4, 0.6 if front else -1.0])
        fig.cap(P2(back), P2(J[b + s] + np.array([-1.0, 2.4])), 1.0, 0.7, under, drip=(7 + front, 3), layer='feath' + s)
        fig.ell(P2(J[c + s] + np.array([1.0, -0.4])), 1.9, 1.0, coat, layer='paw' + s + str(front))
        ticks(fig, J[a + s], J[c + s], 3, 0.6, 31 + front * 2 + (s == 'N'), 1.0)

    fig.dark = 1
    leg('F', True); leg('F', False)
    fig.dark = 0
    # the flag: carried level, feathered long underneath
    fig.part = 'tail'
    wb, wt = J['wb'], J['wt']
    fig.cap(P2(wb), P2(wt), 2.0, 1.0, coat)
    fig.cap(P2(wb + (wt - wb) * 0.2 + np.array([0, 1.0])), P2(wt + np.array([0.4, 1.2])), 1.6, 0.6, under, drip=(5, 4), layer='flag')
    ticks(fig, wb, wb + (wt - wb) * 0.5, 3, 0.7, 41, 0.6)
    fig.part = 'torso'
    fig.layer = 'body'
    fig.cap(P2(Hp), P2(S), 4.6, 5.2, coat)
    fig.ell(P2(S + np.array([0.6, 1.8])), 4.6, 4.4, coat)  # the deep chest
    fig.cap(P2(S + np.array([0.6, -0.4])), P2(J['headb']), 3.8, 3.0, coat)  # the neck
    fig.layer = None
    fig.cap(P2(Hp + np.array([1.5, 3.0])), P2(S + np.array([-0.5, 4.4])), 1.6, 2.0, under, drip=(3, 3), layer='belly')  # the fringe
    # blue belton: heavy over the back and haunch, a light peppering below
    ticks(fig, Hp + np.array([-1.0, -2.6]), S + np.array([-1.0, -3.0]), 11, 1.0, 11, 1.4)
    ticks(fig, Hp + np.array([0, 0.5]), S + np.array([0, 1.5]), 10, 0.6, 12, 2.6)
    ticks(fig, S + np.array([0.5, -1.0]), J['headb'], 4, 0.6, 13, 1.6)
    # the collar, and its tag
    n0, n1 = S + (J['headb'] - S) * 0.62, J['headb'] - S
    side = np.array([n1[1], -n1[0]]) / (np.linalg.norm(n1) + 1e-9)
    fig.cap(P2(n0 + side * 3.0), P2(n0 - side * 3.4), 1.0, 1.0, NMat(COLLAR), layer='collar')
    tag = n0 - side * 3.6 + np.array([0.4, 1.4 + 0.3 * math.sin(P.get('wag', 0.0))])
    fig.ell(P2(tag), 1.0, 1.1, NMat(TAG), layer='tagd')
    leg('N', False); leg('N', True)

    fig.part = 'head'
    hb, ht = J['headb'], J['headt']
    d = (ht - hb) / (np.linalg.norm(ht - hb) + 1e-9)
    up = np.array([d[1], -d[0]])  # the top of the head
    fig.ell(P2(hb + d * 2.0 + up * 0.4), 3.6, 3.3, coat)          # the skull, a domed brow
    fig.cap(P2(hb + d * 3.4 + up * 0.2), P2(ht), 2.6, 2.1, coat)   # the long square muzzle
    fig.cap(P2(hb + d * 4.4 - up * 1.4), P2(ht - d * 0.8 - up * 2.0), 1.5, 1.4, coat, layer='flews')  # the hanging lip
    if P.get('pant'):
        fig.cap(P2(hb + d * 4.0 - up * 2.8), P2(ht - d * 2.2 - up * 3.2), 0.7, 0.8, NMat(TONGUE), layer='tongue')
    fig.ell(P2(ht - d * 0.2 + up * 0.5), 1.2, 1.1, NMat(NOSE), layer='nose')
    ticks(fig, hb + d * 4.5, ht - d * 1.2, 4, 0.45, 21, 1.4)
    e = hb + d * 3.4 + up * 1.1
    fig.ell(P2(e + np.array([-0.2, 0.0])), 1.6, 1.3, NMat(TICK, 'flat', base=1), layer='patch')  # the dark eye patch
    fig.dot(e, EYE); fig.dot(e + np.array([1, 0]), FACE)
    # the ear: set level with the eye, long and wavy, grey-black with a few white locks
    swing = P.get('ear', 0.0)
    eb = hb + d * 0.4 + up * 1.6
    et = eb - up * 7.0 + np.array([-1.2 + swing, 0.0])
    fig.cap(P2(eb), P2(et), 2.0, 3.0, ear, drip=(9, 3), layer='ear')
    fig.cap(P2(eb + (et - eb) * 0.4 + np.array([0.6, 0])), P2(et + np.array([0.8, 0.5])), 0.6, 0.6, NMat(COAT, 'hair', base=2), layer='lock')


# ---- poses: a setter stands tall, trots with a level back, sniffs the ground, flops down
def anims():
    stand = pose(aN=0.05, eN=0.1, aF=-0.05, eF=0.1, tN=-0.12, kN=0.15, tF=0.0, kF=0.15, tail=-1.75)
    idle = [dict(stand, y=0.3 * math.sin(i / 8 * 2 * math.pi), tail=-1.75 + 0.35 * math.sin(i / 8 * 6 * math.pi), wag=i * 1.5,
                 nod=-0.05 + 0.03 * math.sin(i / 8 * 2 * math.pi), pant=1.0, ear=0.3 * math.sin(i / 8 * 2 * math.pi)) for i in range(8)]
    walk = anim.quad_walk(8, stride=0.5, knee=0.95, bob=0.8, extra=lambda p, t: p.update(tail=-1.75 + 0.15 * math.sin(t * 2), ear=0.6 * math.sin(t), wag=t, pant=1.0))
    sniff = [dict(stand, aN=0.25, eN=0.5, aF=0.15, eF=0.45, nod=0.75 + 0.08 * (i % 2), y=0.6, tail=-1.9 + 0.25 * math.sin(i * 2.1), ear=0.6) for i in range(6)]
    lie = [dict(stand, aN=1.55, eN=0.0, aF=1.5, eF=0.0, tN=1.3, kN=2.3, tF=1.25, kF=2.3, nod=0.2 + 0.03 * math.sin(i / 6 * 2 * math.pi),
                y=-3.0 + 0.3 * math.sin(i / 6 * 2 * math.pi), tail=-1.6, ear=0.4) for i in range(6)]
    return dict(idle=idle, walk=walk, crouch=lie, cast=sniff)


def spec(): return nspec('indi', setter, SETTER, anims(), 96, 64, 56, cx=48, kind='quad')


def emit():
    S = spec()
    frames = []
    for an, fr in S.anims.items():
        for P in fr:
            (gr, _), J = anim.bake_frame(S, P)
            frames.append((anim.CLIPS.index(an), gr, anim.joints_of(S, J, P)))
    out = ['// Generated by tools/dog.py - edit there, then: python tools/dog.py emit > sprites_dog.h', '#pragma once', '#include "sprites_anim.h"', '']
    anim.emit_sheet(out, 'INDI', S, frames, S.cx, S.gy)
    return '\n'.join(out) + '\n'


if __name__ == '__main__':
    if sys.argv[1] == 'emit': sys.stdout.write(emit())
    elif sys.argv[1] == 'preview':
        figures.SPECS = {'indi': spec}
        figures.preview(sys.argv[2])

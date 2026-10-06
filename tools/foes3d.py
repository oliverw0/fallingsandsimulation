# The starter-level foes in the 3D-primitive style (see docs/RIG3D.md): goblin, bomber, redcap, sand raider, risen levy
# (one parametrised biped), plus the bat, acid slime, giant scorpion and sea serpent (tools/foes3d_beasts.py).
# Each foe is data (sizes, ramps, headgear, weapon) + pose/clip functions; the renderer is tools/viking.py.
# Emitted through tools/userart.py into sprites_anim.h as ANIM_<NAME> + RIG_A_<NAME> (corpse parts).
#   python tools/foes3d.py preview <dir>
import math, os, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import viking as VK
from viking import Canvas, Draw, V, dirv, two_bone, C
from viking_char import draw_held, binding, hnoise, blobs, tex_fur

# ---------------------------------------------------------------- ramps
def reg(name, ramp):
    if name in VK.MI: return
    VK.RAMPS[name] = ramp
    VK.MATS.append(name)
    VK.MI[name] = len(VK.MATS) - 1

reg('gob',    [C(20, 34, 22), C(48, 86, 40), C(88, 140, 60), C(130, 184, 84), C(176, 220, 120)])
reg('rag',    [C(26, 18, 16), C(62, 44, 34), C(104, 76, 52), C(142, 108, 74), C(180, 142, 100)])
reg('redcloth', [C(40, 10, 16), C(92, 24, 28), C(150, 40, 40), C(196, 64, 56), C(232, 110, 90)])
reg('sandcloth', [C(52, 38, 24), C(112, 84, 52), C(176, 138, 84), C(214, 180, 118), C(240, 212, 160)])
reg('deadskin', [C(30, 28, 34), C(78, 74, 80), C(126, 124, 126), C(166, 164, 160), C(204, 200, 190)])
reg('bombmat', [C(8, 8, 12), C(22, 22, 28), C(42, 42, 50), C(70, 70, 82), C(120, 120, 134)])
reg('tan',    [C(60, 38, 28), C(128, 84, 56), C(186, 130, 86), C(222, 170, 118), C(246, 208, 160)])
reg('chitin', [C(30, 22, 12), C(86, 66, 30), C(142, 116, 48), C(190, 166, 76), C(226, 208, 120)])
reg('scale',  [C(10, 28, 36), C(24, 68, 84), C(44, 112, 124), C(84, 160, 160), C(150, 206, 196)])
reg('belly',  [C(60, 56, 50), C(130, 120, 100), C(180, 168, 136), C(214, 204, 170), C(240, 230, 200)])
reg('slimemat', [C(18, 60, 28), C(40, 128, 50), C(84, 200, 80), C(150, 244, 120), C(220, 255, 190)])
reg('batfur', [C(18, 12, 16), C(46, 34, 36), C(86, 66, 62), C(128, 100, 92), C(172, 140, 126)])
reg('membrane', [C(36, 16, 28), C(84, 40, 56), C(128, 68, 84), C(170, 100, 112), C(210, 140, 150)])
reg('gamb',   [C(54, 40, 34), C(118, 94, 62), C(172, 146, 96), C(206, 184, 128), C(232, 214, 166)])
reg('navy',   [C(10, 14, 32), C(26, 38, 82), C(44, 62, 122), C(70, 92, 160), C(110, 132, 196)])
reg('gold',   [C(52, 30, 14), C(120, 80, 28), C(188, 142, 52), C(226, 190, 90), C(250, 232, 150)])
reg('crimson', [C(30, 8, 20), C(76, 18, 32), C(130, 32, 44), C(176, 54, 60), C(214, 92, 90)])
reg('pale',   [C(50, 36, 48), C(120, 92, 100), C(184, 150, 150), C(222, 190, 180), C(244, 220, 206)])
reg('bone',   [C(34, 28, 36), C(96, 86, 82), C(158, 148, 130), C(204, 196, 170), C(238, 232, 208)])
reg('hoodp',  [C(18, 12, 28), C(44, 32, 62), C(72, 56, 98), C(104, 84, 132), C(142, 118, 170)])
reg('shadow', [C(4, 2, 8), C(10, 6, 16), C(18, 10, 26), C(30, 18, 40), C(46, 30, 58)])
reg('ember',  [C(150, 40, 16), C(230, 96, 24), C(255, 150, 40), C(255, 206, 96), C(255, 244, 190)])
reg('furgold', [C(52, 40, 28), C(112, 88, 48), C(170, 138, 70), C(212, 178, 98), C(240, 214, 150)])
reg('slate',  [C(12, 14, 20), C(34, 40, 48), C(62, 70, 78), C(94, 102, 108), C(132, 138, 142)])
reg('bonew', [C(36, 28, 22), C(100, 82, 58), C(160, 134, 92), C(206, 180, 132), C(236, 214, 164)])     # old bone: stained warm ivory
reg('drflesh', [C(30, 26, 28), C(78, 66, 58), C(124, 106, 90), C(164, 144, 122), C(200, 180, 154)])  # a draugr's dried, leathery skin
reg('horn', [C(60, 44, 24), C(120, 88, 44), C(170, 130, 70), C(204, 168, 100), C(230, 204, 140)])
reg('robeP', [C(16, 10, 34), C(44, 30, 88), C(74, 52, 134), C(106, 80, 170), C(146, 116, 202)])     # the Magus's robes: deep violet
reg('robeB', [C(8, 12, 38), C(22, 34, 88), C(38, 58, 130), C(58, 84, 170), C(96, 124, 210)])      # and midnight blue
reg('hide', [C(28, 20, 16), C(68, 48, 34), C(108, 78, 54), C(146, 108, 76), C(184, 144, 104)])
EYE_Y = (250, 220, 70)
EYE_R = (220, 50, 40)
PTEX = lambda t, n, X, Y: (np.where((np.floor(Y) % 5) == 0, -1, np.where((np.floor(Y) % 5) == 1, 1, 0)) + np.where(blobs(X, Y, 15, 2) > 0.9, -1, 0), None)   # plate bands


# ---------------------------------------------------------------- the biped's data
# sizes in sprite pixels (S = 1): torso, thigh, shin, upper arm, forearm, head radius; skin/cloth ramps; gear
TYPES = {
    'goblin': dict(torso=8.0, thigh=5.4, shin=5.4, ua=4.8, fa=4.4, hr=4.6, tr=(3.8, 4.6), skin='gob', cloth='rag', cloth2='rag', pants=None,
                   head='hood', ears=True, weapon='dagger', nose=1.6, eye=EYE_Y, lean=-74, ws=0.52, boots=False, skirt=3.0),
    'bomber': dict(torso=8.0, thigh=5.4, shin=5.4, ua=4.8, fa=4.4, hr=4.6, tr=(3.9, 4.7), skin='gob', cloth='tan', cloth2='rag', pants=None,
                   head='cap', ears=True, weapon='bomb', nose=1.6, eye=EYE_Y, lean=-76, ws=0.55, boots=False, skirt=2.4),
    'redcap': dict(torso=9.0, thigh=6.0, shin=6.0, ua=5.2, fa=4.8, hr=4.4, tr=(4.4, 5.2), skin='tan', cloth='rag', cloth2='redcloth', pants='rag',
                   head='redcap', ears=False, weapon='spear', nose=1.4, eye=EYE_R, lean=-80, ws=0.62, boots=True, skirt=0.0, beard=True),
    'raider': dict(torso=13.0, thigh=9.0, shin=9.0, ua=7.0, fa=6.4, hr=5.0, tr=(5.4, 6.4), skin='tan', cloth='sandcloth', cloth2='redcloth', pants='sandcloth',
                   head='wrap', ears=False, weapon='spear', nose=1.6, eye=(40, 24, 18), lean=-82, ws=0.64, boots=True, skirt=0.0),
    'risen': dict(torso=13.0, thigh=9.0, shin=9.0, ua=7.0, fa=6.4, hr=5.0, tr=(5.4, 6.4), skin='deadskin', cloth='redcloth', cloth2='rag', pants='rag',
                  head='kettle', ears=False, weapon='spear', nose=1.4, eye=EYE_R, lean=-80, ws=0.64, boots=True, skirt=4.0, mail=True),
    # the castle's own: a guard (kettle helm, gambeson, red tabard, spear and shield), a plate knight (great helm, plume, surcoat, sword and
    # shield), a crimson cultist (cowl, long robe, staff of fire) and a skeletal archer (hood, rags, bow)
    'guard': dict(torso=14.0, thigh=9.2, shin=9.0, ua=7.2, fa=6.6, hr=5.0, tr=(5.8, 6.8), skin='tan', cloth='helm', cloth2='redcloth', pants='pants',
                  head='kettle2', ears=False, weapon='sword', nose=1.5, eye=(40, 24, 18), lean=-80, ws=0.9, boots=True, skirt=8.0, mail=True, twohand=True,
                  tabard=('redcloth', 'wrap'), baldric=True, pauldron='helm'),
    'knight': dict(torso=14.0, thigh=9.5, shin=9.2, ua=7.4, fa=6.8, hr=5.2, tr=(5.8, 6.8), skin='tan', cloth='helm', cloth2='rag', pants='rag',
                   head='spang', ears=False, weapon='axe', nose=1.3, eye=(40, 24, 18), lean=-84, ws=0.8, boots=True, bootmat='boot', skirt=9.0, mail=True,
                   tabard=('navy', 'navy'), tabard_low=True, mantle=True, baldric=True,
                   shield=dict(shape='round', mat='rag', rim='helm', emblem='helm', hand=(5.5, 6.0))),
    'cultist': dict(torso=13.6, thigh=9.2, shin=9.2, ua=7.0, fa=6.4, hr=4.4, tr=(3.6, 4.4), skin='slate', cloth='slate', cloth2='slate', pants='boot',
                    head='voidhood', ears=False, weapon='staff', nose=1.0, eye=(220, 90, 50), lean=-82, ws=0.62, boots=True, bootmat='boot', skirt=6.0,
                    lr=0.84, belts=True, lining=True, pauldron='slate', bracer=True, staffmat='pants'),
    'archer': dict(torso=11.0, thigh=8.6, shin=8.6, ua=6.8, fa=6.6, hr=4.4, tr=(2.0, 2.4), skin='bonew', cloth='hoodp', cloth2='hoodp', pants=None, skel=True,
                   head='skullhood', ears=False, weapon='bow', nose=0.8, eye=(255, 96, 60), lean=-70, ws=0.62, boots=False, skirt=5.0, bone=True, lr=0.5),
    # the crypt's dead: three more skeletons - a sword and rusted cap, a spearman, a helmed one with shield and axe - and the Skeletal Magus,
    # twice the player's height and bulky, who casts without a weapon
    'skeleton': dict(torso=11.4, thigh=8.6, shin=8.6, ua=6.8, fa=6.6, hr=4.4, tr=(2.0, 2.4), skin='bonew', cloth='bone', cloth2='rag', pants=None, skel=True,
                     head='skullcap', ears=False, weapon='sword', nose=0.8, eye=(255, 96, 60), lean=-68, ws=0.55, boots=False, skirt=4.0, bone=True, lr=0.5),
    'skelspear': dict(torso=11.4, thigh=8.8, shin=8.8, ua=6.8, fa=6.6, hr=4.4, tr=(2.0, 2.4), skin='bonew', cloth='bone', cloth2='tan', pants=None, skel=True,
                      head='skull', ears=False, weapon='spear', nose=0.8, eye=(120, 220, 255), lean=-68, ws=0.62, boots=False, skirt=3.0, bone=True, lr=0.5),
    'skelshield': dict(torso=12.4, thigh=9.0, shin=9.0, ua=7.2, fa=6.8, hr=4.6, tr=(2.6, 3.0), skin='bonew', cloth='bone', cloth2='rag', pants=None, skel=True,
                       head='skullhelm', ears=False, weapon='axe', nose=0.8, eye=(255, 200, 60), lean=-72, ws=0.5, boots=False, skirt=5.0, bone=True, lr=0.6,
                       pauldron='helm', shield=dict(shape='round', mat='rag', rim='helm', emblem='helm', hand=(5.5, 6.0))),
    # a draugr: gaunt, leathery, hunched, in a horned iron helm with its eyes burning blue, a layered pauldron, bracers and a heavy rusted sword
    'draugr': dict(torso=13.2, thigh=9.2, shin=9.2, ua=7.2, fa=6.8, hr=4.8, tr=(4.0, 4.8), skin='drflesh', cloth='drflesh', cloth2='rag', pants='hide',
                   head='horned', ears=False, weapon='sword', nose=1.2, eye=(120, 225, 255), lean=-66, ws=0.78, boots=True, bootmat='boot', skirt=5.0, lr=0.78,
                   twohand=True, baldric=True, bracer=True, pauldron='helm', layers=True),
    'bonemage': dict(torso=19.5, thigh=13.5, shin=13.5, ua=10.5, fa=9.8, hr=7.1, tr=(6.0, 7.5), skin='bonew', cloth='bone', cloth2='robeP', pants=None, skel=True,
                     head='skullhood2', ears=False, weapon=None, nose=1.6, eye=(214, 96, 255), lean=-78, ws=1.0, boots=False, skirt=22.5, bone=True, robe=True,
                     cloak=True, mantle2=True, sash=True, sleeve=True, pscale=True),
}


class FoeW:
    """Draws a foe's primitives, each tagged with a part so one part can be rendered alone (the corpse's limbs)."""
    def __init__(w, d, only=None):
        w.d, w.only, w.part = d, only, 'torso'

    def on(w): return w.only is None or w.part in w.only
    def cap(w, *a, **k):
        if w.on(): w.d.cap(*a, **k)
    def ball(w, *a, **k):
        if w.on(): w.d.ball(*a, **k)
    def poly(w, *a, **k):
        if w.on(): w.d.poly(*a, **k)



def chain(w, P0, P1, P2, P3, knots, z, mat, dark=0, tex=None, obj=None, step=1.5):
    """A smooth loft from P1 to P2 (a Catmull-Rom curve, P0 and P3 give the tangents): many short overlapping capsules whose radius follows
    `knots` = [(fraction along, radius)], so a limb or torso flows - muscle swell, narrow wrist - instead of being a stick with a ball at each end."""
    P0, P1, P2, P3 = V(*P0), V(*P1), V(*P2), V(*P3)
    L = float(np.hypot(*(P2 - P1)))
    n = max(2, int(math.ceil(L / step)))
    fr = [f for f, r in knots]; rs = [r for f, r in knots]

    def at(u):
        u2, u3 = u * u, u * u * u
        return 0.5 * ((2 * P1) + (-P0 + P2) * u + (2 * P0 - 5 * P1 + 4 * P2 - P3) * u2 + (-P0 + 3 * P1 - 3 * P2 + P3) * u3)

    def rad(u):   # a smooth (eased) blend between the knots
        r = float(np.interp(u, fr, rs))
        return r
    us = [i / n for i in range(n + 1)]
    pts = [at(u) for u in us]
    rr = [rad(u) for u in us]
    rr = [rr[0]] + [(rr[i - 1] + 2 * rr[i] + rr[i + 1]) / 4.0 for i in range(1, n)] + [rr[-1]]
    for i in range(n):
        u0, u1 = us[i], us[i + 1]
        t2 = None
        if tex is not None:
            t2 = (lambda tt, nn, X, Y, _t=tex, _a=u0, _b=u1: _t(_a + (_b - _a) * tt if np.ndim(tt) else _a, nn, X, Y))
        w.cap(pts[i], pts[i + 1], rr[i], rr[i + 1], z, z, mat, dark=dark, tex=t2, obj=obj)


# ---------------------------------------------------------------- skeletons: real bones, hung on the same skeleton
def _unit(v): return v / (np.hypot(*v) + 1e-9)
def _rot(v, a): return V(v[0] * math.cos(a) - v[1] * math.sin(a), v[0] * math.sin(a) + v[1] * math.cos(a))
HEAD_TEX = lambda t, n, X, Y: (np.where(blobs(X, Y, 61, 1) > 0.88, -1, 0) + 1, None)
BONE_TEX = lambda t, n, X, Y: (np.where(blobs(X, Y, 61, 1) > 0.84, -1, 0) + np.where(hnoise(np.floor(X), np.floor(Y), 62) > 0.95, 1, 0), None)   # stains and a pale fleck


def skel_leg(w, T, J, s, z, dark, oid):
    k = T['torso'] / 11.4
    m = T['skin']
    hp, kn, an = J['legs'][s]
    bc = lambda a, b, ra, rb: w.cap(a, b, ra * k, rb * k, z, z, m, dark=dark, obj=oid, tex=BONE_TEX)
    bb = lambda c, r, dz=0: w.ball(c, r * k, z + dz, m, dark=dark, obj=oid)
    w.part = 'thigh' if s == 'n' else 'farL'
    bc(hp, kn, 1.2, 1.0)                           # the femur
    bb(hp + V(-0.4 * k, -0.6 * k), 1.6)            # its head and trochanter
    bb(kn, 1.5)                                    # the knuckled knee
    bb(kn + V(0.9 * k, -0.2 * k), 1.1, 1)          # the kneecap
    w.part = 'shin' if s == 'n' else 'farL'
    bc(kn, an, 0.95, 0.75)                         # the tibia
    bc(kn + V(-1.0 * k, 0.2 * k), an + V(-0.9 * k, -0.2 * k), 0.55, 0.5)   # and the fibula beside it
    bb(an, 1.3)
    bb(an + V(-1.0 * k, 0.3 * k), 1.1)             # the heel
    bc(an + V(0.4 * k, 0.3 * k), an + V(4.4 * k, 0.6 * k), 0.85, 0.6)      # the foot's long bones
    for dy, ln in ((0.0, 2.4), (0.6, 2.8), (0.2, 2.0)):
        w.cap(an + V(4.4 * k, 0.5 * k + dy * k * 0.3), an + V((4.4 + ln) * k, 0.45 * k + dy * k * 0.5), 0.36 * k, 0.28 * k, z, z, m, dark=dark, obj=oid)   # toes
    if T.get('plates'):   # heavy iron: a knee cop, greaves down the shin, a plated thigh guard
        lr = T.get('lr', 1.0)
        w.ball(kn + V(0.6, -0.4), 3.7 * lr * 0.8, z + 2, 'helm', dark=dark, obj=oid, tex=PTEX)
        w.cap(kn + (an - kn) * 0.28, an + (kn - an) * 0.14, 2.9 * lr * 0.72, 2.3 * lr * 0.72, z + 1, z + 1, 'helm', dark=dark, obj=oid, tex=PTEX)
        w.cap(hp + (kn - hp) * 0.1, hp + (kn - hp) * 0.55, 3.4 * lr * 0.72, 3.1 * lr * 0.72, z + 1, z + 1, 'helm', dark=dark, obj=oid, tex=PTEX)


def skel_arm(w, T, J, s, z, dark, oid):
    k = T['torso'] / 11.4
    m = T['skin']
    sh, el, ha = J['arms'][s]
    bc = lambda a, b, ra, rb: w.cap(a, b, ra * k, rb * k, z, z, m, dark=dark, obj=oid, tex=BONE_TEX)
    bb = lambda c, r, dz=0: w.ball(c, r * k, z + dz, m, dark=dark, obj=oid)
    w.part = 'uarm' if s == 'n' else 'farL'
    bb(sh, 1.5)
    bc(sh, el, 1.0, 0.8)                           # the humerus
    if T.get('sleeve'):   # wide hanging sleeves of robe: the upper arm is inside, the forearm and hand come out of the cuff
        w.cap(sh + V(0.0, -0.3 * k), el + (ha - el) * 0.12, 3.5 * k, 2.9 * k, z + 2, z + 2, 'robeP', dark=dark, obj=oid, tex=lambda t, n, X, Y: (np.where(blobs(X, Y, 71, 2) > 0.8, -1, 0) + np.where(t > 0.85, -1, 0), None))
        w.cap(el + (ha - el) * 0.06, el + (ha - el) * 0.34, 3.0 * k, 3.5 * k, z + 2, z + 2, 'robeB', dark=dark, obj=oid, tex=lambda t, n, X, Y: (np.where(t > 0.86, -1, 0), np.where(t > 0.9, VK.MI['gold'], -1)))
    w.part = 'farm' if s == 'n' else 'farL'
    bb(el, 1.2)
    u = _unit(ha - el); nrm = V(-u[1], u[0])
    bc(el + nrm * 0.55 * k, ha + nrm * 0.4 * k, 0.58, 0.46)      # radius and ulna, two thin bones side by side
    bc(el - nrm * 0.55 * k, ha - nrm * 0.4 * k, 0.5, 0.42)
    bb(ha, 1.0)
    pe = ha + u * 1.9 * k
    bc(ha, pe, 0.95, 0.8)                          # the palm's bones
    for q in range(4):                             # four fingers, each of two bones curling in
        a = (q - 1.5) * 0.3
        d1 = _rot(u, a + 0.15); p1 = pe + d1 * 1.5 * k; p2 = p1 + _rot(d1, 0.7) * 1.2 * k
        w.cap(pe, p1, 0.36 * k, 0.3 * k, z, z, m, dark=dark, obj=oid)
        w.cap(p1, p2, 0.3 * k, 0.24 * k, z, z, m, dark=dark, obj=oid)
    w.cap(ha + u * 0.6 * k, ha + u * 0.6 * k + _rot(u, -0.9) * 2.0 * k, 0.4 * k, 0.3 * k, z, z, m, dark=dark, obj=oid)   # the thumb
    if T.get('plates'):   # an elbow cop and a riveted bracer over the forearm
        lr = T.get('lr', 1.0)
        w.ball(el + V(-0.4, -0.6), 3.4 * lr * 0.8, z + 2, 'helm', dark=dark, obj=oid, tex=PTEX)
        w.cap(el + (ha - el) * 0.3, el + (ha - el) * 0.92, 2.8 * lr * 0.7, 2.4 * lr * 0.7, z + 1, z + 1, 'helm', dark=dark, obj=oid, tex=PTEX)
    if T.get('pauldron'): w.ball(sh + V(0.4, -0.2), 3.9 * T.get('lr', 1.0) * 0.8, z + 2.5, T['pauldron'], dark=dark, obj=oid, tex=PTEX)
    if T.get('spikes') and s == 'n': w.cap(sh + V(0.5, -2.0), sh + V(-1.5, -8.5 * k * 0.8), 2.0 * k * 0.6, 0.4, z + 3, z + 3, 'bone', dark=dark, obj=oid)


def skel_torso(w, T, J):
    """The spine with its small spined vertebrae, curved ribs round a dark hollow, a sternum, collar bones, a shoulder blade and a pelvis."""
    k = T['torso'] / 11.4
    m = T['skin']
    hip, Cc = J['hip'], J['C']
    rd = 4.3 * k
    tex = BONE_TEX
    sp = lambda t: hip + (Cc - hip) * t + V(-(rd * 0.62 + 1.3 * k * math.sin(math.pi * t)), 0)
    w.cap(hip + (Cc - hip) * 0.24, Cc, rd * 0.8, rd * 0.7, -3, -3, 'shadow')                    # the hollow behind the ribs
    for i in range(12):                                                                       # the spine
        t = i / 11.0
        w.ball(sp(t), 0.98 * k, 1, m, tex=tex)
        w.cap(sp(t), sp(t) + V(-1.3 * k, -0.5 * k), 0.4 * k, 0.28 * k, 1, 1, m)               # a spinous process
        if i: w.cap(sp(t - 1 / 11.0), sp(t), 0.5 * k, 0.5 * k, 0, 0, m)
    for q in range(5):                                                                        # the ribs, spaced so the dark shows between them, drooping toward the waist
        f = 0.32 + 0.15 * q
        b = hip + (Cc - hip) * f
        a0 = sp(f)
        depth = rd * (1.0 - 0.14 * abs(q - 2.5) / 3.5)
        front = b + V(depth, 1.5 * k + 0.7 * k * q)
        mid = a0 + (front - a0) * 0.5 + V(0.5 * k, -1.0 * k)
        w.cap(a0, mid, 0.52 * k, 0.52 * k, 4, 5, m, tex=tex)
        w.cap(mid, front, 0.52 * k, 0.42 * k, 5, 5, m, tex=tex)
    w.cap(hip + (Cc - hip) * 0.86 + V(rd * 0.95, 2.6 * k), hip + (Cc - hip) * 0.42 + V(rd * 0.9, 4.4 * k), 0.8 * k, 0.7 * k, 6, 6, m, tex=tex)   # the sternum
    w.cap(Cc + V(rd * 0.8, 0.9 * k), J['sh']['n'] + V(0.2, 0.0), 0.68 * k, 0.6 * k, 6, 7, m)                                                    # the collar bone
    w.poly([Cc + V(-rd * 0.8, 0.3 * k), Cc + V(-rd * 0.3, -0.5 * k), Cc + V(-rd * 0.15, 3.4 * k), Cc + V(-rd * 0.7, 4.6 * k)], 3.0, m, normal=(0, 0, 1.0))    # the shoulder blade
    w.poly([hip + V(-2.6 * k, -0.4 * k), hip + V(-1.2 * k, -2.9 * k), hip + V(2.4 * k, -2.1 * k), hip + V(3.0 * k, 1.2 * k), hip + V(1.4 * k, 3.5 * k), hip + V(-0.2 * k, 2.3 * k)], 2.5, m, normal=(0.1, 0, 1.0))  # the pelvis
    w.ball(hip + V(-2.0 * k, 0.7 * k), 1.2 * k, 3, m)
    w.cap(hip + V(1.6 * k, 2.3 * k), hip + V(2.9 * k, 1.5 * k), 0.7 * k, 0.6 * k, 3, 3, m)


def skel_head(w, d, T, J, P):
    """A skull with real features: a domed cranium, brow, sunken glowing eye, cheekbone, nasal hole, teeth, and a mandible hanging a little open."""
    hd, Nn = J['head'], J['N']
    hr = T['hr']; k = hr / 4.4
    m = T['skin']
    for q in range(3):
        a = Nn + (hd - Nn) * (q / 3.0)
        w.ball(a + V(-0.3 * k, 0.4), 0.9 * k, 3, m)                                       # the neck's vertebrae
    w.ball(hd + V(0.6 * k, -0.2 * k), hr * 0.98, 4, m, tex=HEAD_TEX)                         # the cranium
    w.cap(hd + V(hr * 0.15, -hr * 0.42), hd + V(hr * 0.98, -hr * 0.28), 0.8 * k, 0.7 * k, 7, 7, m)   # the brow ridge
    w.ball(hd + V(hr * 0.52, -hr * 0.12), hr * 0.3, 8, 'shadow')                             # the eye socket
    w.cap(hd + V(hr * 0.46, hr * 0.28), hd + V(hr * 0.86, hr * 0.46), 0.65 * k, 0.55 * k, 7, 7, m, tex=HEAD_TEX)  # the cheekbone
    w.ball(hd + V(hr * 1.0, hr * 0.28), hr * 0.15, 8, 'shadow')                              # the nasal hole
    jx, jy = hd + V(hr * 0.05, hr * 0.65), hd + V(hr * 0.58, hr * (1.18 + 0.12 * P.get('wag', 0.0) * 0)) + V(0, 0.5 * k)
    w.cap(hd + V(hr * 0.12, hr * 0.5), jy, 0.8 * k, 0.8 * k, 6, 6, m, tex=HEAD_TEX)                        # the mandible
    w.cap(jy, hd + V(hr * 1.0, hr * 1.1), 0.75 * k, 0.55 * k, 6, 6, m, tex=HEAD_TEX)
    for q in range(5):                                                                       # teeth, upper and lower
        tx = hr * (0.5 + 0.1 * q)
        w.cap(hd + V(tx, hr * 0.5), hd + V(tx, hr * 0.5 + 1.3 * k), 0.38 * k, 0.3 * k, 9, 9, 'bonew' if m != 'bonew' else 'bone')
        w.cap(hd + V(tx, hr * 1.0), hd + V(tx, hr * 1.0 - 1.1 * k), 0.38 * k, 0.3 * k, 9, 9, 'bonew' if m != 'bonew' else 'bone')
    ex, ey = d.P(hd + V(hr * 0.5, -hr * 0.1))
    if w.on():
        for dx_ in (0, 1): d.cv.eye[(int(ex) + dx_, int(ey))] = T['eye']
        if k > 1.4:
            for dx_ in (0, 1): d.cv.eye[(int(ex) + dx_, int(ey) + 1)] = T['eye']


def bone(T): return (T['thigh'], T['shin'], T['ua'], T['fa'])


def stand_height(T): return (T['thigh'] + T['shin']) * 0.96


def bpose(T, **k):
    p = dict(bob=0.0, lean=T['lean'], head=T['lean'] - 2, fn=(5.0, 0.0), ff=(-4.0, 0.0), hn=(2.6, 9.6), hf=(1.0, 9.6), held=None, hold=None,
             skirt=0.0, kn=-1, ke=1, wag=0.0)
    p.update(k)
    return p


def joints_of(T, P):
    h = stand_height(T) - P['bob']
    hip = V(0.0, -h)
    lean = P['lean']
    Cc = hip + T['torso'] * dirv(lean)
    Nn = Cc + 2.0 * dirv(lean)
    hd = Nn + (T['hr'] * 1.05) * dirv(P['head']) + V(0.8, 0)
    sh = {'n': Cc + V(0.6, 1.8), 'f': Cc + V(-1.8, 1.8)}
    hips = {'n': hip + V(0.8, 0), 'f': hip + V(-0.8, 0)}
    th, sn, ua, fa = bone(T)
    tgt = {s: V(P['f' + s][0], -P['f' + s][1]) for s in 'nf'}     # ankle targets: (x forward, lift above the ground)
    legs = {s: (hips[s], two_bone(hips[s], tgt[s], th, sn, P['kn']), tgt[s]) for s in 'nf'}
    arms = {s: (sh[s], two_bone(sh[s], sh[s] + V(*P['h' + s]), ua, fa, P['ke']), sh[s] + V(*P['h' + s])) for s in 'nf'}
    return dict(hip=hip, C=Cc, N=Nn, head=hd, sh=sh, hips=hips, legs=legs, arms=arms)


def build_biped(w, P, T):
    J = joints_of(T, P)
    d = w.d
    hip, Cc, Nn, hd = J['hip'], J['C'], J['N'], J['head']
    skin, cloth, cloth2 = T['skin'], T['cloth'], T['cloth2']
    pants = T['pants']
    lr = T.get('lr', 1.0)
    bs = T.get('bs', 1.0)   # the size of a skeleton's bone details (knobbed joints, ribs, fists), bigger for the big ones
    robe, plate = T.get('robe'), T.get('plate')
    qtex = lambda t, n, X, Y: (np.where(((np.floor(X) + np.floor(Y)) % 3) == 0, -1, 0), None)      # quilted padding
    ptex = lambda t, n, X, Y: (np.where((np.floor(Y) % 5) == 0, -1, np.where((np.floor(Y) % 5) == 1, 1, 0)) + np.where(blobs(X, Y, 15, 2) > 0.9, -1, 0), None)   # plate: bands
    limbtex = ptex if plate else None

    def newobj():
        d.cv.nobj += 1
        return d.cv.nobj

    def leg(s, z, dark):
        hp, kn, an = J['legs'][s]
        oid = newobj()
        if robe:   # a long robe hides the legs: only the toes peek out
            w.part = 'farL' if s == 'f' else 'shin'
            w.cap(an + V(-0.4, -0.6), an + V(3.2, 0.2), 1.9, 1.5, z, z, 'boot', dark=dark, obj=oid)
            return
        if T.get('skel'):
            skel_leg(w, T, J, s, z, dark, oid)
            return
        pm = pants or skin
        w.part = 'thigh' if s == 'n' else 'farL'
        chain(w, hp - (kn - hp), hp, kn, an, [(0, 3.1 * lr), (0.4, 3.0 * lr), (1, 2.45 * lr)], z, pm, dark, limbtex, oid)
        w.part = 'shin' if s == 'n' else 'farL'
        chain(w, hp, kn, an, an + (an - kn), [(0, 2.45 * lr), (0.3, 2.65 * lr), (1, 1.95 * lr)], z, pm, dark, limbtex, oid)
        if T.get('bone'): w.ball(kn, 1.5 * bs, z, pm, dark=dark, obj=oid)       # a bony knee
        if T.get('plates'):   # heavy iron: a knee cop, greaves down the shin, a plated thigh guard
            w.ball(kn + V(0.6, -0.4), 3.7 * lr * 0.8, z + 2, 'helm', dark=dark, obj=oid, tex=ptex)
            w.cap(kn + (an - kn) * 0.28, an + (kn - an) * 0.14, 2.9 * lr * 0.72, 2.3 * lr * 0.72, z + 1, z + 1, 'helm', dark=dark, obj=oid, tex=ptex)
            w.cap(hp + (kn - hp) * 0.1, hp + (kn - hp) * 0.55, 3.4 * lr * 0.72, 3.1 * lr * 0.72, z + 1, z + 1, 'helm', dark=dark, obj=oid, tex=ptex)
        if T['boots']:
            bm = T.get('bootmat', 'boot')
            chain(w, an + V(0, -6), an + V(0, -3.2), an + V(0.3, 0.2), an + V(4, 0.4), [(0, 2.45), (1, 2.7)], z, bm, dark, limbtex, oid)
            w.cap(an + V(0.3, 0.2), an + V(4.4, 0.4), 2.7, 1.8, z, z, bm, dark=dark, obj=oid)
        else:
            k_ = (0.8 if lr < 1 else 1) * max(1.0, bs * 0.7)
            w.cap(an + V(-0.2, -0.6), an + V(3.8, 0.3), 2.0 * k_, 1.4 * k_, z, z, skin, dark=dark, obj=oid)      # a bare, clawed foot

    def arm(s, z, dark):
        sh, el, ha = J['arms'][s]
        oid = newobj()
        if T.get('skel'):
            skel_arm(w, T, J, s, z, dark, oid)
            return
        um = cloth if T.get('mail') is None else 'helm'
        fm = cloth if plate else skin
        hm = 'glove' if (T.get('bracer') or T.get('mail')) else skin
        utex = limbtex or (qtex if T.get('quilt') else None)
        ftex = limbtex or ((lambda t, n, X, Y: (np.zeros_like(X, int), np.where((t > 0.2) & (t < 0.8), VK.MI['helm'], VK.MI['glove']))) if T.get('bracer') or T.get('mail') else None)
        w.part = 'uarm' if s == 'n' else 'farL'
        chain(w, sh - (el - sh), sh, el, ha, [(0, 2.9 * lr), (0.5, 2.8 * lr), (1, 2.35 * lr)], z, um, dark, utex, oid)
        w.part = 'farm' if s == 'n' else 'farL'
        chain(w, sh, el, ha, ha + (ha - el), [(0, 2.35 * lr), (0.35, 2.5 * lr), (1, 1.85 * lr)], z, fm if not T.get('quilt') else 'belt', dark, ftex, oid)
        u = (ha - el) / (np.hypot(*(ha - el)) + 1e-9)
        hk = 0.62 * bs if T.get('bone') else 1.0
        w.cap(ha - u * 0.6, ha + u * 2.6, 2.05 * hk, 1.6 * hk, z, z, hm, dark=dark, obj=oid)         # a tapering fist, not a ball
        if T.get('bone'): w.ball(el, 1.5 * bs, z, skin, dark=dark, obj=oid)
        if T.get('layers') and s == 'n':   # a layered iron pauldron: three overlapping plates stepping down the shoulder, a rivet in each
            for i_, (dx_, dy_, r_) in enumerate(((0.0, -0.8, 4.6), (0.6, 1.4, 4.2), (1.2, 3.4, 3.8))):
                w.ball(sh + V(dx_, dy_), r_ * 0.75, z + 3 + i_, 'helm', dark=dark, obj=oid, tex=ptex)
                w.cap(sh + V(dx_ - 1.6, dy_ + 0.5), sh + V(dx_ + 1.6, dy_ + 0.5), 0.5, 0.5, z + 6 + i_, z + 6 + i_, 'belt', dark=dark, obj=oid)
        if T.get('plates'):   # an elbow cop and a riveted bracer over the forearm
            w.ball(el + V(-0.4, -0.6), 3.4 * lr * 0.8, z + 2, 'helm', dark=dark, obj=oid, tex=ptex)
            w.cap(el + (ha - el) * 0.3, el + (ha - el) * 0.92, 2.8 * lr * 0.7, 2.4 * lr * 0.7, z + 1, z + 1, 'helm', dark=dark, obj=oid, tex=ptex)
        if T.get('spikes') and s == 'n': w.cap(sh + V(0.5, -2.0), sh + V(-1.5, -8.5 * bs * 0.8), 2.0 * bs * 0.6, 0.4, z + 3, z + 3, 'bone', dark=dark, obj=oid)    # a spike off the shoulder
        if T.get('pauldron'): w.ball(sh + V(0.4, -0.2), 3.9 * lr, z + 2.5, T['pauldron'], dark=dark, tex=qtex if T['pauldron'] == 'slate' else ptex)

    leg('f', -5, 1)
    arm('f', -7, 1)
    if T.get('shield'): draw_shield(w, T['shield'], J['arms']['f'][2])
    # the weapon, between the hands in depth
    w.part = 'weapon'
    if P['held'] and w.only is None:
        draw_foe_weapon(d, P['held'], T)
    w.part = 'torso'
    # the body: a tunic of rags, hem torn into points; a belt
    sk = P['skirt'] + 0.0
    if T['skirt'] > 0:
        s0 = hip + V(0, 1)
        if robe:   # a long crimson robe, hem trimmed in gold, swinging as he walks
            w.cap(s0, s0 + (T['skirt'] + 3.0) * dirv(90 + sk), T['tr'][0] + 1.4, T['tr'][0] + 3.6, 2, 2, cloth2, caps=False,
                  tex=lambda t, n, X, Y: (np.where(t > 0.86, 0, np.where(((np.floor(X) + np.floor(Y * 0.5)) % 5) == 0, -1, 0)),
                                          np.where((t > 0.86) & (t < 0.95), VK.MI['gold'], -1)))
        else:
            w.cap(s0, s0 + (T['skirt'] + 3.0) * dirv(90 + sk), T['tr'][0] + 1.2 * bs, T['tr'][0] + 2.4 * bs, -3, -3, cloth2, caps=False,
                  tex=lambda t, n, X, Y: (np.where(t > 0.8, -1, 0), np.where(blobs(X, Y, 3, 2) > 0.7, VK.MI['dirt'], -1)))
    tr0, tr1 = T['tr']
    torso_tex = lambda t, n, X, Y: (np.where(((np.floor(X) + np.floor(Y)) % 4) == 0, -1, 0) if T.get('mail') else
                                    (ptex(t, n, X, Y)[0] if plate else (qtex(t, n, X, Y)[0] if T.get('quilt') else np.zeros_like(X, int))),
                                    np.where((t > 0.05) & (t < 0.16), VK.MI['belt'], np.where(blobs(X, Y, 5, 2) > 0.9, VK.MI['dirt'] if not (plate or T.get('bone')) else -1, -1)))
    if T.get('cloak'):   # a tattered cloak hanging down the back in three layers, each hem torn into points
        kk = T['torso'] / 11.4
        for i_, (mat_, wide_, len_) in enumerate((('robeB', 1.0, 1.0), ('robeP', 0.8, 0.9), ('robeB', 0.6, 0.78))):
            a0 = Cc + V(-2.5 * kk, 1.4)
            a1 = hip + V(-(7.0 + i_ * 1.8) * kk - 0.8 * math.sin(P.get('skirt', 0) * 0.05 + i_), T['skirt'] * len_ + 6.0)
            w.cap(a0, a1, 3.4 * kk * wide_, 7.2 * kk * wide_, -5 - i_, -5 - i_, mat_, caps=False,
                  clip=lambda X, Y, _h=a1[1], _i=i_: d.inv(X, Y)[1] <= _h - 3.0 * ((np.floor(d.inv(X, Y)[0] * 0.8) + _i) % 3),
                  tex=lambda t, n, X, Y: (np.where(blobs(X, Y, 75, 2) > 0.84, -1, 0) + np.where(t > 0.9, -1, 0), None))
    if T.get('skel'):
        skel_torso(w, T, J)
    if T.get('mantle2'):   # a mantle over the shoulders, gold at its edge, and a waist sash with a runed strip hanging down the front
        kk = T['torso'] / 11.4
        w.ball(Cc + V(-1.2 * kk, 1.2 * kk), 5.4 * kk, 8, 'robeB', clip=lambda X, Y: d.inv(X, Y)[1] <= Cc[1] + 2.2 * kk,
               tex=lambda t, n, X, Y: (np.where(blobs(X, Y, 73, 2) > 0.8, -1, 0), None))
        w.cap(Cc + V(-5.6 * kk, 3.0 * kk), Cc + V(4.0 * kk, 3.8 * kk), 0.7 * kk, 0.7 * kk, 12, 12, 'gold')
    if T.get('sash'):
        kk = T['torso'] / 11.4
        w.cap(hip + V(-4.6 * kk, 0.6 * kk), hip + V(4.8 * kk, 1.0 * kk), 2.4 * kk, 2.4 * kk, 8, 8, 'robeB', tex=lambda t, n, X, Y: (np.where(t > 0.9, -1, 0), None))
        w.cap(hip + V(4.4 * kk, 1.0 * kk), hip + V(5.2 * kk + P.get('skirt', 0) * 0.03, 15.0 * kk), 2.2 * kk, 1.9 * kk, 9, 9, 'robeP', caps=False,
              tex=lambda t, n, X, Y: (np.zeros_like(X, int), np.where(((np.floor(Y) % 5) == 2) & ((np.floor(X) % 3) != 0) & (t > 0.06) & (t < 0.94), VK.MI['gold'], -1)))
    elif T.get('bone') and bs >= 1.5:   # a big skeleton: no solid body, an open ribcage - spine down the back, ribs curving round a dark hollow, a sternum
        w.cap(hip, Cc, tr0 * 0.78, tr1 * 0.78, -3, -3, 'shadow')
        up = (Cc - hip) / (np.hypot(*(Cc - hip)) + 1e-9)
        for t_ in np.linspace(0.04, 1.0, 9):
            vb = hip + (Cc - hip) * t_ + V(-tr0 * 0.62, 0)
            w.ball(vb, 1.9 * bs * 0.55, 3, 'bone')                                        # a vertebra
        for q in range(6):
            f = 0.30 + 0.12 * q
            b = hip + (Cc - hip) * f
            rad = (tr0 + (tr1 - tr0) * f) * (1.0 - 0.07 * abs(q - 3))
            a0 = b + V(-tr0 * 0.6, 0.2); a1 = b + V(rad * 0.55, 1.6); a2 = b + V(rad * 1.0, 2.6)
            w.cap(a0, a1, 0.5 * bs, 0.5 * bs, 4, 7, 'bone')
            w.cap(a1, a2, 0.5 * bs, 0.38 * bs, 7, 6, 'bone')
        w.cap(hip + (Cc - hip) * 0.30 + V(tr0 * 0.95, 1.6), Cc + V(tr1 * 0.6, 1.0), 0.55 * bs, 0.55 * bs, 6, 6, 'bone')      # the sternum
        w.cap(Cc + V(-tr1 * 0.3, 0.4), Cc + V(tr1 * 0.9, 1.4), 0.55 * bs, 0.45 * bs, 7, 7, 'bone')                              # a collar bone
        w.cap(hip + V(-tr0 * 0.5, 1.0), hip + V(tr0 * 0.7, 1.4), 2.6 * bs * 0.8, 2.4 * bs * 0.8, 2, 2, 'bone')                             # the pelvis
    elif T.get('bone'):
        w.cap(hip, Cc, tr0, tr1, 0, 0, cloth, tex=torso_tex)
    else:   # hips, a waist, a deep chest, the shoulders: one flowing form
        chain(w, hip - (Cc - hip), hip, Cc, Cc + (Cc - hip), [(0, tr0 * 0.98), (0.3, tr0 * 0.9), (0.72, tr1 * 1.0), (1, tr1 * 0.9)], 0, cloth, 0, torso_tex)
    if T.get('bone') and bs < 1.5 and not T.get('skel'):   # ribs and a pelvis on the spine
        w.ball(hip + V(0.6, 0.2), 3.4 * bs, 1, 'bone')
        for q in range(4 if bs < 1.5 else 6):
            b = hip + (Cc - hip) * ((0.36 + 0.16 * q) if bs < 1.5 else (0.28 + 0.11 * q))
            w.cap(b + V(-2.6 * bs, 0.2), b + V(3.8 * bs - 0.3 * q, 0.9), 1.0 * bs, 0.8 * bs, 1, 1, 'bone')
    if T.get('baldric'):   # a leather baldric across the chest, a buckle at the belt
        w.cap(Cc + V(-3.0, 0.4), hip + V(4.6, -0.6), 1.1, 1.1, 6.4, 6.4, 'belt')
        w.ball(hip + (Cc - hip) * 0.1 + V(4.8, 0.0), 1.5, 8, 'gold')
    if T.get('belts'):      # crossed belts hung with flasks, a skull and a scrap of blue cloth
        w.cap(Cc + V(-3.4, 1.0), hip + V(4.0, 0.0), 1.0, 1.0, 5.6, 5.6, 'belt')
        w.cap(hip + V(-4.4, -1.6), hip + V(5.0, 0.0), 1.2, 1.2, 5.8, 5.8, 'belt')
        for q, (px_, py_, mat_) in enumerate(((3.6, 0.8, 'crimson'), (1.6, 1.4, 'ember'), (-0.4, 1.0, 'crimson'))):
            w.ball(hip + V(px_, py_ + 1.4), 1.3, 8, mat_, tex=(lambda t, n, X, Y: (np.full_like(X, 1, int), None)) if mat_ == 'ember' else None)
        w.ball(hip + V(-2.6, 1.2), 1.4, 8, 'bone')
        w.cap(hip + V(-3.4, 0.6), hip + V(-3.8 + P.get('skirt', 0) * 0.05, 7.0), 1.0, 0.8, 6.5, 6.5, 'navy')
    if T.get('lining'):     # the robe's red lining, glimpsed down the open front
        w.cap(hip + V(3.6, 1.6), hip + V(4.8, 4.4 + T['skirt']), 1.0, 1.6, 4, 4, 'crimson', caps=False)
    if T.get('mantle'):     # a great mantle of golden fur across the shoulders
        w.ball(Cc + V(-1.6, 0.4), 7.4, 3.0, 'furgold', tex=lambda t, n, X, Y: (tex_fur(t, n, X, Y)[0], None))
        w.ball(Cc + V(2.4, 1.2), 4.6, 4.0, 'furgold', tex=lambda t, n, X, Y: (tex_fur(t, n, X, Y)[0], None))
    if T.get('tabard') and T.get('tabard_low'):   # a blue tunic panel hanging down the front, over the dark skirt
        slope = (Cc[0] - hip[0]) / (Cc[1] - hip[1] - 1e-9)
        w.cap(hip + V(0.6, 0.0), hip + V(1.2, T['skirt'] + 4.0), T['tr'][0] - 0.4, T['tr'][0] + 0.2, 1.5, 1.5, T['tabard'][0], caps=False,
              clip=lambda X, Y: d.inv(X, Y)[0] >= hip[0] + 0.4,
              tex=lambda t, n, X, Y: (np.where(t > 0.9, -1, 0), None))
    elif T.get('tabard'):   # a surcoat over the front of the body, a cross on the chest
        tm, em = T['tabard']
        slope = (Cc[0] - hip[0]) / (Cc[1] - hip[1] - 1e-9)
        w.cap(hip + V(0, 1.5), Cc + (Cc - hip) * 0.02, T['tr'][0] + 0.7, T['tr'][1] + 0.7, 1, 1, tm, caps=False,
              clip=lambda X, Y: d.inv(X, Y)[0] >= hip[0] + (d.inv(X, Y)[1] - hip[1]) * slope - 0.6,
              tex=lambda t, n, X, Y: (np.where((t > 0.05) & (t < 0.14), -1, 0), np.where((t > 0.05) & (t < 0.14), VK.MI['belt'], -1)))
        cc = hip + (Cc - hip) * 0.6 + V(T['tr'][0] * 0.42, 0)
        w.poly([cc + V(-0.7, -3.4), cc + V(0.7, -3.4), cc + V(0.7, 3.2), cc + V(-0.7, 3.2)], 7.0, em, normal=(0.3, 0, 0.95))
        w.poly([cc + V(-2.0, -1.0), cc + V(2.0, -1.0), cc + V(2.0, 0.4), cc + V(-2.0, 0.4)], 7.0, em, normal=(0.3, 0, 0.95))
    if T.get('mail'):   # a mail shirt over the rags
        w.cap(hip + (Cc - hip) * 0.2, Cc, T['tr'][0] + 0.7, T['tr'][1] + 0.7, 1, 1, 'helm',
              tex=lambda t, n, X, Y: (np.where(((np.floor(X) % 2 == 0) & (np.floor(Y) % 2 == 0)), 0, -1), None))
    # the head
    w.part = 'head'
    hr = T['hr']
    if T.get('skel'):
        skel_head(w, d, T, J, P)
    else:
        w.cap(Nn + V(0, 0.5), hd, 2.2, 2.2, 3, 3, skin)
        w.ball(hd + V(0.8, 0.8), hr, 4, skin)                                   # the skull / face
        w.ball(hd + V(hr * 0.95, 1.1), T['nose'], 6, skin)                      # the nose
    if T['ears']:
        for sd, z, dk in ((1, 10, 0), (-1, -3, 1)):
            e0 = hd + V(-2.4 + (sd < 0) * -1.0, -1.2)
            w.cap(e0, e0 + V(-6.2 - (sd < 0), -3.6 + (sd < 0) * 1.0), 2.0, 0.5, z, z, skin, dark=dk)     # long pointed ears swept back
    ex, ey = d.P(hd + V(hr * 0.62, -0.6))
    if w.on() and not T.get('skel'): d.cv.eye[(int(ex), int(ey))] = T['eye']; d.cv.eye[(int(ex), int(ey) + 1)] = T['eye'] if T['eye'] == EYE_R else T['eye']
    if T.get('beard'):
        w.cap(hd + V(2.0, 2.4), hd + V(2.6, 6.8), 3.2, 1.2, 6, 6, 'wrap', tex=lambda t, n, X, Y: (np.where(hnoise(np.floor(X), np.floor(Y), 7) > 0.7, 1, 0), None))
    # headgear
    hg = T['head']
    if hg == 'hood':     # a ragged hood, the face a dark opening
        w.ball(hd + V(-0.6, -0.4), hr + 1.0, 7, cloth, clip=lambda X, Y: d.inv(X, Y)[0] <= hd[0] + 1.6,
               tex=lambda t, n, X, Y: (np.where(blobs(X, Y, 8, 2) > 0.8, -1, 0), None))
        w.cap(hd + V(-2.2, 1.2), hd + V(-5.8, 4.4), 3.0, 1.4, 5, 5, cloth)
    elif hg == 'cap':    # a dented iron cap
        w.ball(hd + V(0, -0.8), hr + 0.7, 7, 'helm', clip=lambda X, Y: d.inv(X, Y)[1] <= hd[1] - 0.2,
               tex=lambda t, n, X, Y: (np.where(blobs(X, Y, 9, 2) > 0.85, -1, 0), None))
        w.cap(hd + V(-hr, -0.2), hd + V(hr + 0.6, -0.2), 1.0, 1.0, 8, 8, 'helm')
        w.cap(hd + V(hr * 0.4, 0.4), hd + V(hr * 0.9, 0.4), 1.3, 1.3, 9, 9, 'bombmat')              # goggles: a dark band over the eyes
    elif hg == 'redcap':
        w.ball(hd + V(0, -0.8), hr + 0.4, 7, 'redcloth', clip=lambda X, Y: d.inv(X, Y)[1] <= hd[1] - 0.4)
        w.cap(hd + V(-0.6, -2.0), hd + V(-3.0, -11.0 + P.get('wag', 0)), 3.4, 0.6, 7, 7, 'redcloth',
              tex=lambda t, n, X, Y: (np.where(blobs(X, Y, 10, 2) > 0.86, -1, 0), None))             # a tall, drooping, blood-dyed cap
    elif hg == 'wrap':   # a desert head-wrap and veil
        w.ball(hd + V(-0.2, -0.6), hr + 1.1, 7, 'wrap', clip=lambda X, Y: d.inv(X, Y)[1] <= hd[1] + 1.2,
               tex=lambda t, n, X, Y: (np.where(((np.floor(X) + np.floor(Y)) % 4) == 0, -1, 0), None))
        w.cap(hd + V(-1.8, 1.0), hd + V(-6.0 + P.get('wag', 0) * 0.3, 5.5), 2.6, 1.0, 5, 5, 'wrap')   # the tail of the wrap
        w.cap(hd + V(0.6, 2.4), hd + V(hr * 0.8, 2.4), 2.0, 1.8, 8, 8, 'redcloth')                    # a veil across the mouth
    elif hg == 'kettle': # a kettle hat over a mail coif
        w.ball(hd + V(-0.4, -0.2), hr + 0.8, 6, 'helm', clip=lambda X, Y: d.inv(X, Y)[1] <= hd[1] + 3.0,
               tex=lambda t, n, X, Y: (np.where(blobs(X, Y, 11, 2) > 0.82, -1, 0), np.where(blobs(X, Y, 12, 1) > 0.94, VK.MI['belt'], -1)))
        w.cap(hd + V(-hr - 3.4, -1.8), hd + V(hr + 3.8, -1.8), 1.3, 1.3, 8, 8, 'helm')                # the brim
    elif hg == 'kettle2':   # a mail coif round the face, a broad-brimmed kettle helm over it
        w.ball(hd + V(-0.6, 1.0), hr + 0.7, 5, 'helm', clip=lambda X, Y: d.inv(X, Y)[0] <= hd[0] + 1.6,
               tex=lambda t, n, X, Y: (np.where(((np.floor(X) % 2 == 0) & (np.floor(Y) % 2 == 0)), -1, 0), None))
        w.ball(hd + V(-0.2, -1.0), hr + 1.0, 7, 'helm', clip=lambda X, Y: d.inv(X, Y)[1] <= hd[1] - 0.8,
               tex=lambda t, n, X, Y: (np.where(blobs(X, Y, 11, 2) > 0.84, -1, 0), None))
        w.cap(hd + V(-hr - 3.2, -0.8), hd + V(hr + 3.6, -0.8), 1.2, 1.2, 8, 8, 'helm')
    elif hg == 'greathelm':   # a flat-topped great helm: an eye slit, breaths down the front, a red plume
        def slit(t, n, X, Y):
            lx, ly = d.inv(X, Y)
            sl = (np.abs(ly - (hd[1] - 0.6)) < 0.6) & (lx > hd[0] + 0.4)
            br = (np.abs(lx - (hd[0] + hr * 0.5)) < 0.45) & (ly > hd[1] + 1.2)
            vb = (np.abs(lx - (hd[0] + hr + 0.6)) < 0.4) & (ly > hd[1] - 4)
            return np.where(vb, 1, 0) + np.where(blobs(X, Y, 16, 2) > 0.92, -1, 0), np.where(sl | (br & (((np.floor(Y)) % 2) == 0)), VK.MI['shadow'], -1)
        w.cap(hd + V(-0.4, -3.8), hd + V(0.4, 4.4), hr + 1.2, hr + 0.9, 7, 7, 'helm', tex=slit)
        w.cap(hd + V(-hr - 0.6, -5.2), hd + V(hr + 0.8, -5.2), 1.3, 1.3, 8, 8, 'helm')
        w.cap(hd + V(-0.4, -5.8), hd + V(-4.6 - P.get('wag', 0) * 0.6, -4.0 + P.get('wag', 0) * 0.3), 2.0, 1.0, 4, 4, T['plume'])
        w.cap(hd + V(-0.4, -5.8), hd + V(-7.4 - P.get('wag', 0), -0.6 + P.get('wag', 0) * 0.4), 1.6, 0.5, 4, 4, T['plume'])
    elif hg == 'spang':      # a Norse spangenhelm: bronze-banded dome with a peak, a nasal, eye holes; a mail aventail over beard and chest
        w.ball(hd + V(0.0, -0.8), hr + 1.1, 7, 'helm', clip=lambda X, Y: d.inv(X, Y)[1] <= hd[1] + 0.6,
               tex=lambda t, n, X, Y: (np.where(blobs(X, Y, 31, 2) > 0.86, -1, 0), None))
        w.cap(hd + V(0.4, -hr), hd + V(1.0, -hr - 5.0), 1.5, 0.2, 8, 8, 'helm')                       # the spike
        for xo in (-4.4, -0.2, 3.8):
            w.cap(hd + V(xo, -0.6), hd + V(xo * 0.15 + 0.6, -hr - 1.6), 0.6, 0.4, 20, 20, 'gold')      # the brass bands
        w.cap(hd + V(-hr - 0.8, 0.4), hd + V(hr + 1.2, 0.4), 0.8, 0.8, 21, 21, 'gold')                 # the brim band
        w.cap(hd + V(hr * 0.95, 0.0), hd + V(hr * 1.0 + 0.4, 4.4), 0.9, 0.7, 14, 14, 'helm')            # the nasal
        w.ball(hd + V(2.2, 1.6), hr * 0.62, 6, 'shadow', clip=lambda X, Y: d.inv(X, Y)[1] <= hd[1] + 2.4)  # the dark under the brim
        w.ball(hd + V(0.8, 3.6), hr * 0.95, 8, 'helm', clip=lambda X, Y: d.inv(X, Y)[1] >= hd[1] + 2.2,
               tex=lambda t, n, X, Y: (np.where(((np.floor(X) % 2 == 0) & (np.floor(Y) % 2 == 0)), -1, 0), None))   # the mail aventail
        w.cap(Nn + V(1.0, 0.6), Nn + V(1.6, 9.0), 5.0, 4.6, 7, 7, 'helm',
              tex=lambda t, n, X, Y: (np.where(((np.floor(X) % 2 == 0) & (np.floor(Y) % 2 == 0)), -1, 0) + np.where(t > 0.85, -1, 0), None))   # the mail bib over the chest
        for ex_ in (2.2, 4.0):
            ex, ey = d.P(hd + V(ex_, 0.6))
            if w.on(): d.cv.eye[(int(ex), int(ey))] = (255, 190, 90)
    elif hg == 'voidhood':   # a deep grey hood: the face is a black void
        w.cap(hd + V(-0.8, 1.6), hd + V(-1.0, -1.0), hr + 1.5, hr + 1.8, 7, 7, 'slate', dark=1, clip=lambda X, Y: d.inv(X, Y)[0] <= hd[0] + 2.0,
              tex=lambda t, n, X, Y: (np.where(blobs(X, Y, 17, 2) > 0.86, -1, 0), None))
        w.cap(hd + V(-1.0, -1.0), hd + V(-3.6, -8.4 + P.get('wag', 0) * 0.3), hr + 1.7, 0.8, 7, 7, 'slate', dark=1)
        w.cap(hd + V(0.0, -4.4), hd + V(3.8, -1.4), 2.4, 2.4, 8, 8, 'slate', dark=1, clip=lambda X, Y: d.inv(X, Y)[1] <= hd[1] - 2.0)   # the cowl's peak over the brow
        w.ball(hd + V(2.6, 0.8), hr * 0.8, 9, 'shadow')                                                  # the void
        w.cap(hd + V(3.0, 1.0), hd + V(2.6, 5.0), 2.4, 2.4, 9, 9, 'shadow')
        for ex_ in (3.4, 5.2):
            ex, ey = d.P(hd + V(ex_, 0.6))
            if w.on(): d.cv.eye[(int(ex), int(ey))] = (170, 70, 46)
    elif hg in ('skull', 'skullcap', 'skullhelm', 'skullcrown'):   # the dead: the skull itself is built by skel_head; this is what is left on it
        k = 1.0 if hg != 'skullcrown' else 1.8
        if hg == 'skullcap':   # a dented rusted iron cap
            w.ball(hd + V(0.0, -1.2), hr + 0.6, 7, 'helm', clip=lambda X, Y: d.inv(X, Y)[1] <= hd[1] - 1.0, tex=lambda t, n, X, Y: (np.where(blobs(X, Y, 19, 2) > 0.8, -1, 0), np.where(blobs(X, Y, 20, 1) > 0.85, VK.MI['belt'], -1)))
        elif hg == 'skullhelm':   # an iron helm with a nasal and cheek-plate, a mail aventail hanging behind
            w.ball(hd + V(0.0, -1.0), hr + 0.9, 7, 'helm', clip=lambda X, Y: d.inv(X, Y)[1] <= hd[1] + 0.4, tex=lambda t, n, X, Y: (np.where(blobs(X, Y, 21, 2) > 0.8, -1, 0), None))
            w.cap(hd + V(hr * 0.9, -1.0), hd + V(hr * 1.0, 2.8), 0.8, 0.7, 14, 14, 'helm')
            w.cap(hd + V(-hr * 0.7, 0.4), hd + V(-hr * 0.9, 5.0), 2.2, 1.6, 4, 4, 'helm', tex=lambda t, n, X, Y: (np.where(((np.floor(X) % 2 == 0) & (np.floor(Y) % 2 == 0)), -1, 0), None))
        elif hg == 'skullcrown':   # a great skull crowned with spikes and swept-back horns, a ragged hood behind it, eyes ablaze
            for sd, (hx, hy, tx, ty) in enumerate(((-2.0, -hr + 1.5, -9.0, -hr - 9.0), (3.0, -hr + 1.5, 7.0, -hr - 11.0))):
                w.cap(hd + V(hx, hy), hd + V(tx * 0.55, ty * 0.5 + hy * 0.5), 2.0, 1.5, 6, 6, 'bone')
                w.cap(hd + V(tx * 0.55, ty * 0.5 + hy * 0.5), hd + V(tx, ty), 1.5, 0.3, 6, 6, 'bone')
            for q in range(5): w.cap(hd + V(-5.0 + q * 2.6, -hr + 0.6), hd + V(-5.0 + q * 2.6 + (q - 2) * 0.4, -hr - 3.0 - (q % 2) * 1.6), 0.9, 0.2, 7, 7, 'gold')       # the crown's spikes
            w.cap(hd + V(-4.0, -hr + 1.0), hd + V(5.0, -hr + 1.0), 1.1, 1.1, 8, 8, 'gold')
            w.ball(hd + V(-3.4, 1.0), hr + 1.2, 5, 'hoodp', clip=lambda X, Y: d.inv(X, Y)[0] <= hd[0] - 2.4,
                   tex=lambda t, n, X, Y: (np.where(blobs(X, Y, 22, 2) > 0.8, -1, 0), None))
            for ex_ in (3.2, 4.4):
                ex, ey = d.P(hd + V(ex_, -0.6))
                if w.on():
                    for dx_ in (0, 1):
                        for dy_ in (0, 1): d.cv.eye[(int(ex) + dx_, int(ey) + dy_)] = T['eye']
    elif hg == 'horned':   # a draugr's horned helm: dented iron dome over the whole face, eye slits burning blue, a nasal ridge, a ram's horn curling out each side
        w.ball(hd + V(0.4, -0.2), hr + 1.1, 7, 'helm', tex=lambda t, n, X, Y: (np.where(blobs(X, Y, 23, 2) > 0.8, -1, 0) + np.where(hnoise(np.floor(X), np.floor(Y), 24) > 0.96, 1, 0), np.where(blobs(X, Y, 25, 1) > 0.9, VK.MI['belt'], -1)))
        w.cap(hd + V(hr * 0.55, -hr * 0.9), hd + V(hr * 1.05, hr * 0.7), 0.9, 0.8, 14, 14, 'helm')      # the nasal ridge
        w.cap(hd + V(hr * 0.1, -hr * 0.12), hd + V(hr * 1.1, -hr * 0.12), 0.8, 0.8, 12, 12, 'shadow')   # the eye slit
        w.cap(hd + V(-hr * 0.2, hr * 0.45), hd + V(hr * 0.7, hr * 0.85), 1.6, 1.1, 8, 8, 'helm')      # a cheek guard
        for ex_ in (hr * 0.45, hr * 0.8):
            ex, ey = d.P(hd + V(ex_, -hr * 0.14))
            if w.on():
                for dx_ in (0, 1): d.cv.eye[(int(ex) + dx_, int(ey))] = T['eye']
        for z_, dk_, mir in ((10, 0, 1.0), (-3, 1, 0.8)):          # the horns: out from the temple, back, up and round to a point
            base = hd + V(-hr * 0.1, -hr * 0.1)
            pts = [V(0, 0), V(-2.6, -0.4), V(-4.8, -2.6), V(-5.4, -5.8), V(-3.6, -8.4), V(-1.0, -9.0)] if mir == 1.0 else [V(0, 0), V(-2.2, -0.6), V(-4.0, -2.6), V(-4.4, -5.4)]
            for i in range(len(pts) - 1):
                r0, r1 = 2.1 - i * 0.36, 2.1 - (i + 1) * 0.36
                w.cap(base + pts[i], base + pts[i + 1], max(0.35, r0), max(0.3, r1), z_, z_, 'horn', dark=dk_)
    elif hg == 'cowl':      # a pointed crimson cowl: the face a black hollow with two embers in it
        w.ball(hd + V(-0.6, -0.4), hr + 1.7, 7, 'crimson', clip=lambda X, Y: d.inv(X, Y)[0] <= hd[0] + 1.4,
               tex=lambda t, n, X, Y: (np.where(blobs(X, Y, 17, 2) > 0.85, -1, 0), None))
        w.cap(hd + V(-0.4, -3.4), hd + V(-3.4, -8.6 + P.get('wag', 0) * 0.3), 3.2, 0.6, 7, 7, 'crimson')
        w.cap(hd + V(-0.6, -4.6), hd + V(2.2, -1.6), 2.0, 2.0, 8, 8, 'crimson', clip=lambda X, Y: d.inv(X, Y)[1] <= hd[1] - 2.2)       # the cowl's peak over the brow
        w.ball(hd + V(2.6, 0.8), hr * 0.62, 6, 'shadow')
        w.cap(Nn + V(-1.0, 0.0), Nn + V(1.6, 1.4), 3.2, 3.0, 6, 6, 'gold')              # a gold collar
        for ex_ in (1.6, 3.8):
            ex, ey = d.P(hd + V(ex_, 0.0))
            if w.on(): d.cv.eye[(int(ex), int(ey))] = (255, 170, 80)
    elif hg == 'skullhood2':   # a deep purple hood with a gold edge over the skull, the face a grin in its shadow
        k = hr / 4.4
        w.ball(hd + V(-2.4 * k, -0.9 * k), hr * 1.12, 7, 'robeP', clip=lambda X, Y: (d.inv(X, Y)[0] <= hd[0] - hr * 0.2) | (d.inv(X, Y)[1] <= hd[1] - hr * 0.75),
               tex=lambda t, n, X, Y: (np.where(blobs(X, Y, 74, 2) > 0.82, -1, 0), None))
        w.cap(hd + V(-hr * 0.2, -hr * 0.95), hd + V(hr * 1.15, -hr * 0.62), hr * 0.38, hr * 0.28, 9, 9, 'robeP')              # the peak over the brow
        w.cap(hd + V(-hr * 0.2, -hr * 0.95), hd + V(hr * 1.15, -hr * 0.62), 0.55 * k, 0.5 * k, 12, 12, 'gold')                   # the gold edge along the peak
        w.cap(hd + V(hr * 1.15, -hr * 0.62), hd + V(hr * 1.15 + 0.2, -hr * 0.05), 0.6 * k, 0.5 * k, 12, 12, 'gold')
        w.cap(Nn + V(-1.0 * k, 0.5), Nn + V(-4.5 * k, 9.0 * k), 4.2 * k, 3.2 * k, -2, -2, 'robeP')                               # the hood's tail down the back
    elif hg == 'skullhood':  # a tattered hood over the skull
        w.ball(hd + V(-0.6, -0.4), hr + 1.4, 7, 'hoodp', clip=lambda X, Y: (d.inv(X, Y)[0] <= hd[0] + 0.2) | (d.inv(X, Y)[1] <= hd[1] - 2.6),
               tex=lambda t, n, X, Y: (np.where(blobs(X, Y, 18, 2) > 0.8, -1, 0), None))
        w.cap(hd + V(-2.0, 1.0), hd + V(-6.0, 5.0 + P.get('wag', 0) * 0.4), 2.8, 1.0, 5, 5, 'hoodp')
        w.part = 'torso'
        w.cap(Nn + V(-1.0, 0.6), Nn + V(-3.4, 10.0), 3.6, 2.4, -2, -2, 'hoodp',
              tex=lambda t, n, X, Y: (np.where(t > 0.8, -1, 0), None))        # shoulder rags
    leg('n', 5, 0)
    arm('n', 8, 0)
    return J


def draw_shield(w, Sh, hand):
    """A heater shield on the far arm, held in front of the body: a rim, a painted face and a cross."""
    if not (w.only is None or 'shield' in w.only): return
    saved, w.part = w.part, 'shield'
    if Sh.get('shape') == 'round':   # a big round shield held out at the side: dark boards, an iron rim and boss
        c = hand + V(2.4, 0.6)
        R = 8.6
        ring = lambda k: [c + V(math.cos(q) * R * k, math.sin(q) * R * k) for q in np.linspace(0, 2 * math.pi, 22, endpoint=False)]
        dome_r = lambda lx, ly: np.stack([np.clip((lx - c[0]) / R, -1, 1) * 0.5, np.clip((ly - c[1]) / R, -1, 1) * 0.5, np.full_like(lx, 0.8)], -1)
        w.poly(ring(1.0), 7.0, Sh['rim'], nfn=dome_r)
        w.poly(ring(0.86), 7.6, Sh['mat'], nfn=dome_r,
               aux_fn=lambda X, Y: np.where(((np.floor(Y) - np.floor(c[1])) % 4) == 0, -1, 0) + np.where(blobs(X, Y, 41, 2) > 0.85, -1, 0))
        w.ball(c, 2.4, 9.5, Sh['emblem'])
        w.part = saved
        return
    c = hand + V(1.8, -1.6)
    pts = lambda k: [c + V(x * k, y * k + (0.4 if k < 1 else 0)) for x, y in ((-4.6, -6.0), (4.6, -6.0), (4.4, 1.0), (0.0, 8.8), (-4.4, 1.0))]
    dome = lambda lx, ly: np.stack([np.clip((lx - c[0]) / 6.0, -1, 1) * 0.35, np.clip((ly - c[1]) / 9.0, -1, 1) * 0.3, np.full_like(lx, 0.88)], -1)
    w.poly(pts(1.0), 7.0, Sh['rim'], nfn=dome)
    w.poly(pts(0.78), 7.6, Sh['mat'], nfn=dome)
    w.poly([c + V(-0.6, -5.2), c + V(0.6, -5.2), c + V(0.6, 6.4), c + V(-0.6, 6.4)], 8.2, Sh['emblem'], normal=(0, 0, 1.0))
    w.poly([c + V(-3.2, -2.6), c + V(3.2, -2.6), c + V(3.2, -1.4), c + V(-3.2, -1.4)], 8.2, Sh['emblem'], normal=(0, 0, 1.0))
    w.part = saved


def draw_foe_weapon(d, H, T):
    """The foe's weapon: H = dict(type, g (px, absolute), ang, flip). Uses the player weapons' models at the foe's scale."""
    t = H['type']
    ws = T['ws']
    cv = d.cv
    dw = Draw(cv, d.ox, 1, ws)       # the weapon's own scale
    g = V(*H['g']) / ws              # grip in weapon-model units (local x forward, y down, ground 0)
    ang = H['ang']
    if t == 'bomb':
        c = V(*H['g']) + dirv(ang) * 1.5
        d.ball(c, 3.6, 8, 'bombmat')
        d.ball(c + V(-1.0, -1.1), 0.9, 12, 'bombmat', tex=lambda tt, n, X, Y: (np.full_like(X, 4, int), None))
        d.cap(c + V(0.4, -3.0), c + V(1.8, -5.0 - H.get('fuse', 0)), 0.6, 0.4, 8, 8, 'belt')
        fx, fy = d.P(c + V(1.8, -5.6 - H.get('fuse', 0)))
        cv.eye[(int(fx), int(fy))] = (255, 200, 60)
        cv.eye[(int(fx), int(fy) - 1)] = (255, 120, 40)
    elif t == 'staff':   # a cultist's staff: a gnarled shaft, a steel claw and an orb of fire (it swells as he gathers it)
        a = dirv(ang)
        gg = V(*H['g'])
        top = gg + a * 27.0
        d.cap(gg - a * 9.0, top, 1.1, 1.0, 7, 7, T.get('staffmat', 'wood'), tex=binding(0.34, 0.46))
        d.cap(top - a * 1.5, top + a * 2.0, 1.9, 1.0, 8, 8, 'steel')
        r = 2.6 + 1.4 * H.get('charge', 0.0)
        c = top + a * (2.6 + r * 0.5)
        d.ball(c, r, 9, 'ember', tex=lambda tt, n, X, Y: (np.full_like(X, 1, int), None))
        d.ball(c + V(-r * 0.3, -r * 0.3), r * 0.4, 12, 'ember', tex=lambda tt, n, X, Y: (np.full_like(X, 3, int), None))
        for q in range(3):   # licks of flame
            th = q * 2.1 + H.get('charge', 0.0) * 1.7
            fx, fy = d.P(c + V(math.cos(th) * (r + 1.0), math.sin(th) * (r + 1.0)))
            cv.eye[(int(fx), int(fy))] = (255, 190, 80)
    elif t == 'bow':   # a skeleton's bow: a wooden stave, a string drawn back to the far hand, an arrow nocked when it is drawn
        gg = V(*H['g'])
        tipf = lambda sg: gg + V(-4.4 * sg * sg, 12.5 * sg)
        pts_ = [tipf(k / 6.0) for k in range(-6, 7)]
        for i in range(len(pts_) - 1):
            rr = 1.5 - 0.7 * abs((i - 6 + 0.5) / 6.0)
            d.cap(pts_[i], pts_[i + 1], rr, rr, 9, 9, 'wood')
        nock = V(*H['nock']) if H.get('draw') else tipf(0) + V(-4.4 * 0 - 4.6, 0)
        for tp in (tipf(-1), tipf(1)):
            d.cap(tp, nock, 0.38, 0.38, 8, 8, 'wrap')
        if H.get('draw'):
            fwd = V(1.0, 0.0)
            d.cap(nock - fwd * 1.5, nock + fwd * 23.0, 0.5, 0.5, 9, 9, 'wood')
            d.cap(nock + fwd * 22.0, nock + fwd * 25.5, 1.1, 0.2, 9, 9, 'steel')
            d.cap(nock - fwd * 1.5, nock - fwd * 4.0, 1.1, 1.1, 9, 9, 'redcloth')
    else:
        draw_held(dw, dict(type=t, g=tuple(g), ang=ang, zg=H.get('zg', 6.0) / ws, zh=H.get('zh', 6.0) / ws), 1.3)


# ---------------------------------------------------------------- poses and clips
def hand_to(T, P, ang, reach=1.0, side='n'):
    """A hand target relative to the shoulder: `reach` of the arm's length along angle ang (degrees)."""
    L = (T['ua'] + T['fa']) * 0.98 * reach
    return tuple(L * dirv(ang))


def spear_grip(T, P, ang, push=0.0):
    """Two-handed spear: the grip hand (far) near the chest, the near hand forward on the shaft."""
    J = joints_of(T, P)
    return J


def clip_biped(T, kind):
    """-> dict clip name -> list of poses. kind: 'stab' (dagger), 'throw' (bomb), 'thrust' (spear)."""
    out = {}
    base = dict(held=None)
    WTYPE = {'stab': 'dagger', 'throw': 'bomb', 'thrust': 'spear', 'swing': T.get('weapon') if T.get('weapon') in ('sword',) else 'sword', 'chop': 'axe', 'cast': 'staff', 'shoot': 'bow', 'bonecast': None}

    def held_for(ph, P):
        J = joints_of(T, P)
        near = J['arms']['n'][2]
        return near

    def mk(**k): return bpose(T, **k)

    def with_weapon(P, ang, ahead=0.0, fuse=0, lift=0.0, charge=0.0, draw=False):
        """Hang the weapon in the near hand at angle ang."""
        J = joints_of(T, P)
        g = J['arms']['n'][2]
        P = dict(P)
        if kind == 'thrust' and not T.get('shield'):   # two-handed: the far hand on the shaft behind the near one
            gf = g - dirv(ang) * 6.0
            P['hf'] = tuple(gf - J['sh']['f'])
        if T.get('twohand'):   # a greatsword in both fists: the far hand behind the near one on the grip
            gf = g - dirv(ang) * 5.0
            P['hf'] = tuple(gf - J['sh']['f'])
        if T.get('shield'):   # the far arm holds the shield up in front of him
            hx, hy = T['shield']['hand']
            P['hf'] = (hx + (P['hf'][0] - 1.0) * 0.0 + ahead * 0.0, hy - 0.4 * P.get('bob', 0.0) - lift)
        P['held'] = None if WTYPE[kind] is None else dict(type=WTYPE[kind], g=tuple(g), ang=ang, fuse=fuse, charge=charge, draw=draw, nock=tuple(J['sh']['f'] + V(*P['hf'])))
        return P

    # ---- idle: breathing, the weapon held low
    out['idle'] = []
    for i in range(4):
        t = i / 4 * 2 * math.pi
        P = mk(bob=0.5 * (0.5 - 0.5 * math.cos(t)), head=T['lean'] - 2 + math.sin(t), hn=(4.0, 10.5 + 0.5 * math.sin(t)), hf=(0.5, 10.5 - 0.5 * math.sin(t)), wag=math.sin(t))
        if kind == 'shoot': P['hn'] = (8.0, 2.5 + 0.4 * math.sin(t)); P['hf'] = (1.5, 9.5)
        out['idle'].append(with_weapon(P, {'stab': -40, 'throw': -60, 'thrust': -70, 'swing': -62, 'chop': 42, 'cast': -86, 'shoot': 0, 'bonecast': 0}[kind]))
    # ---- walk: a stride, the weapon carried
    out['walk'] = []
    for i in range(8):
        ph = i / 8 * 2 * math.pi
        s, c = math.sin(ph), math.cos(ph)
        st = 5.0 if T['torso'] < 10 else (7.5 if T['torso'] < 18 else 8.5)
        P = mk(bob=0.8 * abs(s), lean=T['lean'] + 2, fn=(st * s, 2.4 * max(0, c)), ff=(-st * s, 2.4 * max(0, -c)),
               hn=(4.0 + 1.5 * s, 10.0), hf=(1.0 - 3.0 * s, 10.0 + c), wag=2 * s, skirt=-6 * abs(s))
        if kind == 'shoot': P['hn'] = (8.0, 2.8); P['hf'] = (1.0 - 3.0 * s, 10.0 + c)
        out['walk'].append(with_weapon(P, {'stab': -35, 'throw': -55, 'thrust': -68, 'swing': -56, 'chop': 36, 'cast': -84, 'shoot': 0, 'bonecast': 0}[kind]))
    # ---- windup / strike / recover by weapon
    if kind == 'stab':
        out['windup'] = [with_weapon(mk(bob=1.0, lean=T['lean'] - 6, head=T['lean'] - 8, hn=(-4.0, 6.0), hf=(1.0, 9.0), fn=(6.0, 0.0), ff=(-5.0, 0.0)), -150),
                         with_weapon(mk(bob=1.6, lean=T['lean'] - 10, head=T['lean'] - 10, hn=(-6.0, 3.0), hf=(0.0, 8.0), fn=(7.0, 0.0), ff=(-6.0, 0.0)), -165)]
        out['strike'] = [with_weapon(mk(bob=0.4, lean=T['lean'] + 6, head=T['lean'] + 4, hn=(8.5, 2.0), hf=(-2.0, 9.0), fn=(9.0, 0.0), ff=(-7.0, 0.0)), -5),
                         with_weapon(mk(bob=0.2, lean=T['lean'] + 10, head=T['lean'] + 8, hn=(10.0, 3.0), hf=(-3.0, 9.0), fn=(10.0, 0.0), ff=(-8.0, 0.0)), 5),
                         with_weapon(mk(bob=0.6, lean=T['lean'] + 6, hn=(8.0, 5.0), hf=(-2.0, 9.0), fn=(8.0, 0.0), ff=(-6.0, 0.0)), 15)]
        out['recover'] = [with_weapon(mk(bob=0.8, lean=T['lean'] + 2, hn=(6.0, 8.0), hf=(0.0, 10.0)), -20), with_weapon(mk(), -40)]
    elif kind == 'swing':   # a sword raised behind the head, then brought down in a hard cut
        out['windup'] = [with_weapon(mk(bob=1.0, lean=T['lean'] - 6, head=T['lean'] - 8, hn=(-1.0, -4.0), hf=(1.0, 9.0), fn=(6.0, 0.0), ff=(-5.0, 0.0)), -118),
                         with_weapon(mk(bob=1.6, lean=T['lean'] - 12, head=T['lean'] - 12, hn=(-4.0, -9.0), hf=(0.0, 8.0), fn=(7.0, 0.0), ff=(-6.0, 0.0)), -150)]
        out['strike'] = [with_weapon(mk(bob=0.4, lean=T['lean'] + 6, head=T['lean'] + 4, hn=(7.0, -6.0), hf=(-2.0, 9.0), fn=(9.0, 0.0), ff=(-7.0, 0.0)), -62),
                         with_weapon(mk(bob=0.2, lean=T['lean'] + 12, head=T['lean'] + 8, hn=(10.0, 3.0), hf=(-3.0, 9.0), fn=(11.0, 0.0), ff=(-8.0, 0.0)), 8),
                         with_weapon(mk(bob=0.8, lean=T['lean'] + 10, hn=(9.0, 6.0), hf=(-2.0, 9.0), fn=(10.0, 0.0), ff=(-7.0, 0.0)), 58)]
        out['recover'] = [with_weapon(mk(bob=0.8, lean=T['lean'] + 2, hn=(6.0, 6.0), hf=(0.0, 10.0)), 20), with_weapon(mk(), -30)]
    elif kind == 'chop':   # an axe swung up behind the head, then brought over and down
        out['windup'] = [with_weapon(mk(bob=1.0, lean=T['lean'] - 6, head=T['lean'] - 8, hn=(-1.0, -3.0), hf=(1.0, 9.0), fn=(6.0, 0.0), ff=(-5.0, 0.0)), -110),
                         with_weapon(mk(bob=1.8, lean=T['lean'] - 14, head=T['lean'] - 14, hn=(-3.0, -10.0), hf=(0.0, 8.0), fn=(7.0, 0.0), ff=(-6.0, 0.0)), -146)]
        out['strike'] = [with_weapon(mk(bob=0.4, lean=T['lean'] + 6, head=T['lean'] + 4, hn=(6.0, -9.0), hf=(-2.0, 9.0), fn=(9.0, 0.0), ff=(-7.0, 0.0)), -84),
                         with_weapon(mk(bob=0.2, lean=T['lean'] + 14, head=T['lean'] + 10, hn=(10.0, -1.0), hf=(-3.0, 9.0), fn=(11.0, 0.0), ff=(-8.0, 0.0)), -6),
                         with_weapon(mk(bob=1.6, lean=T['lean'] + 16, hn=(10.0, 6.0), hf=(-2.0, 9.0), fn=(12.0, 0.0), ff=(-8.0, 0.0)), 56)]
        out['recover'] = [with_weapon(mk(bob=1.0, lean=T['lean'] + 6, hn=(7.0, 7.0), hf=(0.0, 10.0)), 52), with_weapon(mk(), 40)]
    elif kind == 'cast':    # a staff gathered overhead as the orb swells, then thrust out to let the fire go
        out['windup'] = [with_weapon(mk(bob=0.8, lean=T['lean'] - 4, head=T['lean'] - 6, hn=(5.0, 0.0), hf=(4.0, 3.0), fn=(6.0, 0.0), ff=(-5.0, 0.0)), -82, charge=0.5),
                         with_weapon(mk(bob=1.4, lean=T['lean'] - 10, head=T['lean'] - 12, hn=(6.0, -7.0), hf=(7.0, -3.0), fn=(7.0, 0.0), ff=(-6.0, 0.0)), -76, charge=1.0)]
        out['cast'] = [with_weapon(mk(bob=0.2, lean=T['lean'] + 8, head=T['lean'] + 6, hn=(11.0, 1.0), hf=(4.0, 3.0), fn=(9.0, 0.0), ff=(-7.0, 0.0)), -28, charge=1.0),
                       with_weapon(mk(bob=0.2, lean=T['lean'] + 10, head=T['lean'] + 8, hn=(12.0, 2.0), hf=(3.0, 4.0), fn=(10.0, 0.0), ff=(-8.0, 0.0)), -22, charge=0.6),
                       with_weapon(mk(bob=0.6, lean=T['lean'] + 4, hn=(9.0, 5.0), hf=(1.0, 8.0), fn=(8.0, 0.0), ff=(-6.0, 0.0)), -50, charge=0.2)]
        out['strike'] = list(out['cast'])
        out['recover'] = [with_weapon(mk(bob=0.8, lean=T['lean'] + 2, hn=(6.0, 6.0), hf=(0.0, 10.0)), -70), with_weapon(mk(), -86)]
    elif kind == 'shoot':   # the bow raised and the string drawn to the cheek, then loosed
        out['windup'] = [with_weapon(mk(bob=0.6, lean=T['lean'] - 2, head=T['lean'] - 4, hn=(9.0, 3.0), hf=(3.0, 4.0), fn=(6.0, 0.0), ff=(-5.0, 0.0)), 0, draw=True),
                         with_weapon(mk(bob=1.0, lean=T['lean'] - 4, head=T['lean'] - 2, hn=(12.0, 0.0), hf=(-3.0, 0.0), fn=(7.0, 0.0), ff=(-6.0, 0.0)), 0, draw=True)]
        out['cast'] = [with_weapon(mk(bob=0.4, lean=T['lean'] + 2, head=T['lean'], hn=(12.0, 0.0), hf=(-1.0, 1.0), fn=(8.0, 0.0), ff=(-6.0, 0.0)), 0),
                       with_weapon(mk(bob=0.4, lean=T['lean'] + 4, hn=(11.0, 1.0), hf=(-3.0, 4.0), fn=(8.0, 0.0), ff=(-6.0, 0.0)), 0),
                       with_weapon(mk(bob=0.6, lean=T['lean'] + 2, hn=(9.0, 4.0), hf=(0.0, 7.0), fn=(8.0, 0.0), ff=(-6.0, 0.0)), 0)]
        out['strike'] = list(out['cast'])
        out['recover'] = [with_weapon(mk(bob=0.8, hn=(6.0, 6.0), hf=(0.0, 9.0)), 0), with_weapon(mk(), 0)]
    elif kind == 'bonecast':   # arms drawn slowly up over the head as a rune is cut in light (drawn live), then both swept forward as the spell is thrown
        W = [(5.0, 2.0, 2.0, 3.0, -4, -8, 0.6), (4.0, -1.0, 1.5, -0.5, -6, -11, 0.8), (3.0, -4.0, 1.0, -4.0, -8, -14, 1.0), (2.5, -7.0, 0.5, -7.0, -10, -16, 1.3), (2.0, -9.5, 0.0, -9.5, -12, -18, 1.6)]
        out['windup'] = [with_weapon(mk(bob=b_, lean=T['lean'] + l_, head=T['lean'] + h_, hn=(a_, b0), hf=(c_, d_), fn=(6.0 + 0.4 * i_, 0.0), ff=(-5.0 - 0.4 * i_, 0.0)), 0)
                         for i_, (a_, b0, c_, d_, l_, h_, b_) in enumerate(W)]
        C5 = [(8.0, -4.0, 5.0, -3.0, 4, 2, 0.5), (11.0, -2.0, 8.0, -1.0, 10, 6, 0.2), (13.0, 1.0, 10.0, 2.0, 15, 10, 0.2), (13.0, 2.0, 10.0, 3.0, 14, 9, 0.3), (9.0, 5.0, 6.0, 6.0, 8, 4, 0.8)]
        out['cast'] = [with_weapon(mk(bob=b_, lean=T['lean'] + l_, head=T['lean'] + h_, hn=(a_, b0), hf=(c_, d_), fn=(9.0 + 1.0 * min(i_, 2), 0.0), ff=(-7.0 - 0.8 * min(i_, 2), 0.0)), 0)
                       for i_, (a_, b0, c_, d_, l_, h_, b_) in enumerate(C5)]
        out['strike'] = list(out['cast'])
        out['recover'] = [with_weapon(mk(bob=0.8, lean=T['lean'] + 3, hn=(6.0, 7.0), hf=(3.0, 8.0)), 0), with_weapon(mk(), 0)]
    elif kind == 'throw':
        out['windup'] = [with_weapon(mk(bob=0.8, lean=T['lean'] - 6, head=T['lean'] - 8, hn=(-1.0, -4.0), hf=(1.0, 9.0), fn=(6.0, 0.0), ff=(-5.0, 0.0)), -100, fuse=1),
                         with_weapon(mk(bob=1.4, lean=T['lean'] - 12, head=T['lean'] - 12, hn=(-4.0, -9.0), hf=(0.0, 8.0), fn=(7.0, 0.0), ff=(-6.0, 0.0)), -130, fuse=2)]
        out['strike'] = [with_weapon(mk(bob=0.2, lean=T['lean'] + 6, hn=(7.0, -7.0), hf=(-2.0, 9.0), fn=(9.0, 0.0), ff=(-7.0, 0.0)), -40),
                         mk(bob=0.2, lean=T['lean'] + 10, head=T['lean'] + 8, hn=(10.0, -1.0), hf=(-3.0, 9.0), fn=(10.0, 0.0), ff=(-8.0, 0.0)),    # the bomb is gone
                         mk(bob=0.6, lean=T['lean'] + 6, hn=(8.0, 3.0), hf=(-2.0, 9.0), fn=(8.0, 0.0), ff=(-6.0, 0.0))]
        out['recover'] = [mk(bob=0.8, lean=T['lean'] + 2, hn=(6.0, 8.0), hf=(0.0, 10.0)), mk()]
    else:   # thrust: the spear drawn back, then driven forward
        out['windup'] = [with_weapon(mk(bob=1.0, lean=T['lean'] - 6, head=T['lean'] - 8, hn=(2.0, 6.0), hf=(-1.0, 8.0), fn=(6.0, 0.0), ff=(-5.0, 0.0)), -15),
                         with_weapon(mk(bob=1.6, lean=T['lean'] - 10, head=T['lean'] - 10, hn=(0.0, 5.0), hf=(-2.0, 7.0), fn=(7.0, 0.0), ff=(-6.0, 0.0)), -8)]
        out['strike'] = [with_weapon(mk(bob=0.4, lean=T['lean'] + 6, hn=(9.0, 6.0), hf=(1.0, 8.0), fn=(9.0, 0.0), ff=(-7.0, 0.0)), 0),
                         with_weapon(mk(bob=0.2, lean=T['lean'] + 10, head=T['lean'] + 8, hn=(11.0, 6.0), hf=(3.0, 8.0), fn=(11.0, 0.0), ff=(-8.0, 0.0)), 0),
                         with_weapon(mk(bob=0.6, lean=T['lean'] + 6, hn=(8.0, 6.5), hf=(1.0, 8.0), fn=(8.0, 0.0), ff=(-6.0, 0.0)), -3)]
        out['recover'] = [with_weapon(mk(bob=0.8, lean=T['lean'] + 2, hn=(5.0, 7.0), hf=(-1.0, 9.0)), -25), with_weapon(mk(), -50)]
    out['hurt'] = [with_weapon(mk(bob=0.6, lean=T['lean'] - 10, head=T['lean'] - 16, hn=(-3.0, 6.0), hf=(-5.0, 7.0), fn=(5.0, 0.0), ff=(-6.0, 0.0), skirt=8.0), -80),
                   with_weapon(mk(bob=0.2, lean=T['lean'] - 5, head=T['lean'] - 8, hn=(1.0, 8.0), hf=(-2.0, 9.0)), -60)]
    out['land'] = [with_weapon(mk(bob=2.0, lean=T['lean'] + 2, fn=(6.0, 0.0), ff=(-5.0, 0.0), hn=(3.0, 9.0), hf=(0.0, 9.0)), -50)]
    if T.get('pscale'):   # the poses are authored for a raider-sized body: a big one's arms reach, strides and crouches scale up with it
        sa, sl = (T['ua'] + T['fa']) / 13.4, T['thigh'] / 9.0
        for clip in out.values():
            for P in clip:
                P['hn'] = (P['hn'][0] * sa, P['hn'][1] * sa); P['hf'] = (P['hf'][0] * sa, P['hf'][1] * sa)
                P['fn'] = (P['fn'][0] * sl, P['fn'][1] * sl); P['ff'] = (P['ff'][0] * sl, P['ff'][1] * sl); P['bob'] = P['bob'] * sl
    return out

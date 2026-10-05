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
from viking_char import draw_held, binding, hnoise, blobs

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
EYE_Y = (250, 220, 70)
EYE_R = (220, 50, 40)


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

    def leg(s, z, dark):
        hp, kn, an = J['legs'][s]
        w.part = 'thigh' if s == 'n' else 'farL'
        w.cap(hp, kn, 3.0, 2.5, z, z, pants or skin, dark=dark)
        w.part = 'shin' if s == 'n' else 'farL'
        w.cap(kn, an, 2.5, 2.2, z, z, pants or skin, dark=dark)
        if T['boots']:
            w.cap(an + V(0, -2.4), an + V(0, 0.4), 2.7, 2.6, z, z, 'boot', dark=dark)
            w.cap(an + V(0, 0.2), an + V(4.2, 0.2), 2.6, 1.9, z, z, 'boot', dark=dark)
        else:
            w.cap(an + V(0, -0.8), an + V(3.6, 0.2), 2.0, 1.6, z, z, skin, dark=dark)      # a bare, clawed foot

    def arm(s, z, dark):
        sh, el, ha = J['arms'][s]
        w.part = 'uarm' if s == 'n' else 'farL'
        w.cap(sh, el, 2.8, 2.4, z, z, cloth if T.get('mail') is None else 'helm', dark=dark)
        w.part = 'farm' if s == 'n' else 'farL'
        w.cap(el, ha, 2.4, 2.0, z, z, skin, dark=dark)
        w.ball(ha + (ha - el) / (np.hypot(*(ha - el)) + 1e-9) * 0.8, 2.3, z, skin, dark=dark)

    leg('f', -5, 1)
    arm('f', -7, 1)
    # the weapon, between the hands in depth
    w.part = 'weapon'
    if P['held'] and w.only is None:
        draw_foe_weapon(d, P['held'], T)
    w.part = 'torso'
    # the body: a tunic of rags, hem torn into points; a belt
    sk = P['skirt'] + 0.0
    if T['skirt'] > 0:
        s0 = hip + V(0, 1)
        w.cap(s0, s0 + (T['skirt'] + 3.0) * dirv(90 + sk), T['tr'][0] + 1.2, T['tr'][0] + 2.4, -3, -3, cloth2, caps=False,
              tex=lambda t, n, X, Y: (np.where(t > 0.8, -1, 0), np.where(blobs(X, Y, 3, 2) > 0.7, VK.MI['dirt'], -1)))
    w.cap(hip, Cc, T['tr'][0], T['tr'][1], 0, 0, cloth,
          tex=lambda t, n, X, Y: (np.where(((np.floor(X) + np.floor(Y)) % 4) == 0, -1, 0) if T.get('mail') else np.zeros_like(X, int),
                                  np.where((t > 0.05) & (t < 0.16), VK.MI['belt'], np.where(blobs(X, Y, 5, 2) > 0.9, VK.MI['dirt'], -1))))
    if T.get('mail'):   # a mail shirt over the rags
        w.cap(hip + (Cc - hip) * 0.2, Cc, T['tr'][0] + 0.7, T['tr'][1] + 0.7, 1, 1, 'helm',
              tex=lambda t, n, X, Y: (np.where(((np.floor(X) + np.floor(Y)) % 2) == 0, 0, -1), None))
    # the head
    w.part = 'head'
    w.cap(Nn + V(0, 0.5), hd, 2.2, 2.2, 3, 3, skin)
    hr = T['hr']
    w.ball(hd + V(0.8, 0.8), hr, 4, skin)                                   # the skull / face
    w.ball(hd + V(hr * 0.95, 1.1), T['nose'], 6, skin)                      # the nose
    if T['ears']:
        for sd, z, dk in ((1, 10, 0), (-1, -3, 1)):
            e0 = hd + V(-2.4 + (sd < 0) * -1.0, -1.2)
            w.cap(e0, e0 + V(-6.2 - (sd < 0), -3.6 + (sd < 0) * 1.0), 2.0, 0.5, z, z, skin, dark=dk)     # long pointed ears swept back
    ex, ey = d.P(hd + V(hr * 0.62, -0.6))
    if w.on(): d.cv.eye[(int(ex), int(ey))] = T['eye']; d.cv.eye[(int(ex), int(ey) + 1)] = T['eye'] if T['eye'] == EYE_R else T['eye']
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
    leg('n', 5, 0)
    arm('n', 8, 0)
    return J


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

    def held_for(ph, P):
        J = joints_of(T, P)
        near = J['arms']['n'][2]
        return near

    def mk(**k): return bpose(T, **k)

    def with_weapon(P, ang, ahead=0.0, fuse=0, lift=0.0):
        """Hang the weapon in the near hand at angle ang."""
        J = joints_of(T, P)
        g = J['arms']['n'][2]
        P = dict(P)
        if kind == 'thrust':   # two-handed: the far hand on the shaft behind the near one
            gf = g - dirv(ang) * 6.0
            P['hf'] = tuple(gf - J['sh']['f'])
        P['held'] = dict(type={'stab': 'dagger', 'throw': 'bomb', 'thrust': 'spear'}[kind], g=tuple(g), ang=ang, fuse=fuse)
        return P

    # ---- idle: breathing, the weapon held low
    out['idle'] = []
    for i in range(4):
        t = i / 4 * 2 * math.pi
        P = mk(bob=0.5 * (0.5 - 0.5 * math.cos(t)), head=T['lean'] - 2 + math.sin(t), hn=(4.0, 10.5 + 0.5 * math.sin(t)), hf=(0.5, 10.5 - 0.5 * math.sin(t)), wag=math.sin(t))
        out['idle'].append(with_weapon(P, {'stab': -40, 'throw': -60, 'thrust': -70}[kind]))
    # ---- walk: a stride, the weapon carried
    out['walk'] = []
    for i in range(8):
        ph = i / 8 * 2 * math.pi
        s, c = math.sin(ph), math.cos(ph)
        st = 5.0 if T['torso'] < 10 else 7.5
        P = mk(bob=0.8 * abs(s), lean=T['lean'] + 2, fn=(st * s, 2.4 * max(0, c)), ff=(-st * s, 2.4 * max(0, -c)),
               hn=(4.0 + 1.5 * s, 10.0), hf=(1.0 - 3.0 * s, 10.0 + c), wag=2 * s, skirt=-6 * abs(s))
        out['walk'].append(with_weapon(P, {'stab': -35, 'throw': -55, 'thrust': -68}[kind]))
    # ---- windup / strike / recover by weapon
    if kind == 'stab':
        out['windup'] = [with_weapon(mk(bob=1.0, lean=T['lean'] - 6, head=T['lean'] - 8, hn=(-4.0, 6.0), hf=(1.0, 9.0), fn=(6.0, 0.0), ff=(-5.0, 0.0)), -150),
                         with_weapon(mk(bob=1.6, lean=T['lean'] - 10, head=T['lean'] - 10, hn=(-6.0, 3.0), hf=(0.0, 8.0), fn=(7.0, 0.0), ff=(-6.0, 0.0)), -165)]
        out['strike'] = [with_weapon(mk(bob=0.4, lean=T['lean'] + 6, head=T['lean'] + 4, hn=(8.5, 2.0), hf=(-2.0, 9.0), fn=(9.0, 0.0), ff=(-7.0, 0.0)), -5),
                         with_weapon(mk(bob=0.2, lean=T['lean'] + 10, head=T['lean'] + 8, hn=(10.0, 3.0), hf=(-3.0, 9.0), fn=(10.0, 0.0), ff=(-8.0, 0.0)), 5),
                         with_weapon(mk(bob=0.6, lean=T['lean'] + 6, hn=(8.0, 5.0), hf=(-2.0, 9.0), fn=(8.0, 0.0), ff=(-6.0, 0.0)), 15)]
        out['recover'] = [with_weapon(mk(bob=0.8, lean=T['lean'] + 2, hn=(6.0, 8.0), hf=(0.0, 10.0)), -20), with_weapon(mk(), -40)]
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
    return out

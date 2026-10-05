# Animation clips for the Viking: lists of poses with per-frame durations (ms). Idle and run are functions of phase; the
# attacks are hand-authored keyframe tables (the axe's is the brief's). Angles are unwrapped degrees, never wrapped before
# interpolating: a swing's angle only ever climbs (or only ever falls). 0 = forward, -90 = up, +90 = down.
import math, os, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from viking_char import *

MELEE = ['dagger', 'sword', 'axe', 'spear', 'mace', 'pan']
RANGED = ['crossbow', 'staff']
WEAPONS = MELEE + RANGED


def chest_of(hip, lean): return V(*hip) + 23 * dirv(lean)
def tup(v): return (float(v[0]), float(v[1]))


# how each weapon is held: grip offset from the chest at rest, the resting angle, and how far up the shaft the second hand
# goes (None: one-handed, the far hand left free)
HOLD = {
    'axe':      dict(g=(8, 12), ang=-137, h2=None, run=-137, zg=12.0, zh=-6.0),
    'mace':     dict(g=(8, 12), ang=-125, h2=None, run=-120, zg=12.0, zh=-4.0),
    'sword':    dict(g=(10, 17), ang=-58, h2=None, run=-36, zg=9.0, zh=9.0),
    'dagger':   dict(g=(10, 19), ang=-50, h2=None, run=-30, zg=9.0, zh=9.0),
    'pan':      dict(g=(10, 18), ang=-66, h2=None, run=-50, zg=9.0, zh=9.0),
    'spear':    dict(g=(9, 21), ang=-72, h2=15, run=-52, zg=12.0, zh=12.0),
    'crossbow': dict(g=(8, 11), ang=8, h2=11, run=22, zg=12.0, zh=12.0),
    'staff':    dict(g=(9, 16), ang=-86, h2=None, run=-80, zg=9.0, zh=9.0),
}


def held_at(w, grip, ang, hip_note=None):
    H = HOLD[w]
    g = V(*grip)
    a = dirv(ang)
    held = dict(type=w, g=tup(g), ang=ang, zg=H['zg'], zh=H['zh'])
    if H['h2']:
        return held, tup(g + a * H['h2']), tup(g)         # two-handed: the near hand up the shaft, the far on the grip
    return held, tup(g), None                              # one-handed: the near hand grips it


def ready(w, hip, lean, run=False, bob=0.0, wob=0.0):
    H = HOLD[w]
    c = chest_of(hip, lean)
    g = c + V(*H['g']) + V(0, bob)
    held, hn, hf = held_at(w, g, (H['run'] if run else H['ang']) + wob)
    return held, hn, hf


# ---------------------------------------------------------------- idle and run
def idle(w, n=8):
    out = []
    for i in range(n):
        t = i / n * 2 * math.pi
        br = 0.5 - 0.5 * math.cos(t)
        hip = (0.0, -HIP_REST + 0.5 * br)
        lean = -88.0 + 1.2 * math.sin(t)
        held, hn, hf = ready(w, hip, lean, bob=-0.8 * math.sin(t), wob=1.6 * math.sin(t - 0.6))
        out.append(pose(hip=hip, lean=lean, head=-86.0 + math.sin(t - 0.8), fn=(7.0, 0.0), ff=(-8.0, 0.0), hn=hn,
                        hf=hf or (-1.0, -29.0 + 0.5 * br), held=held, skirt=2.0 * math.sin(t - 0.4), beard=0.6 * math.sin(t - 1.0)))
    return out, [150] * n


def run(w, n=8):
    out = []
    for i in range(n):
        ph = i / n * 2 * math.pi
        sn, cs = math.sin(ph), math.cos(ph)
        hip = (1.8 * math.sin(ph + 1.2), -HIP_REST + 3.0 * abs(math.sin(ph)) + 2.0)    # the hips shift with each stride
        lean = -70.0 + 3.6 * math.sin(ph + 0.6) + 1.2 * math.sin(2 * ph)                   # and the torso rocks over the legs
        fx = lambda s, c: (22.0 * s, -13.0 * max(0.0, c))  # a long stride: the forward swing lifts the foot
        held, hn, hf = ready(w, hip, lean, run=True, bob=0.8 * math.sin(2 * ph), wob=3.0 * math.sin(2 * ph))
        out.append(pose(hip=hip, lean=lean, head=-80.0 + 1.5 * math.sin(2 * ph), fn=fx(sn, cs), ff=fx(-sn, -cs), hn=hn,
                        hf=hf or (2 - 9.0 * sn, -30.0 + 3.0 * cs), held=held, skirt=-12.0 - 8.0 * abs(sn), beard=-2.0))
    return out, [70] * n


# ---------------------------------------------------------------- attacks: (angle, grip offset from the chest, lean, crouch, feet (near, far), ms)
AXE_CHOP = [(-137, (8, 12), -84, 0, (7, -8), 80), (192, (-6, -6), -98, 2, (8, -9), 70), (208, (-5, -17), -106, 3, (10, -10), 60),
            (202, (-4, -19), -109, 3, (10, -10), 150), (266, (2, -21), -96, 1, (12, -10), 35), (322, (10, -12), -80, 1, (14, -10), 30),
            (372, (14, -1), -68, 3, (15, -11), 30), (404, (14, 4), -60, 5, (15, -12), 120), (404, (14, 4), -61, 5, (15, -12), 70),
            (402, (13, 4), -63, 4, (15, -12), 70), (380, (11, 3), -72, 3, (13, -11), 70), (300, (6, -6), -80, 2, (10, -9), 80),
            (-137, (8, 12), -84, 1, (7, -8), 90)]
MACE_SLAM = [(-125, (8, 12), -84, 0, (7, -8), 80), (200, (-5, -8), -98, 2, (8, -9), 80), (214, (-5, -19), -108, 3, (10, -10), 70),
             (208, (-4, -21), -112, 4, (10, -10), 150), (270, (3, -22), -98, 2, (12, -10), 40), (330, (12, -12), -80, 2, (14, -10), 32),
             (380, (16, 0), -66, 4, (15, -11), 30), (404, (16, 6), -58, 6, (15, -12), 130), (404, (16, 6), -59, 6, (15, -12), 80),
             (400, (14, 4), -64, 4, (14, -12), 70), (360, (10, -2), -74, 3, (12, -11), 70), (-125, (8, 12), -84, 1, (7, -8), 90)]
PAN_BONK = [(-66, (10, 10), -84, 0, (7, -8), 70), (200, (-2, -10), -98, 2, (8, -9), 70), (212, (-4, -18), -106, 3, (10, -10), 120),
            (268, (4, -18), -96, 1, (12, -10), 35), (336, (12, -8), -80, 2, (14, -10), 30), (392, (16, 2), -66, 4, (15, -11), 100),
            (390, (15, 2), -68, 4, (15, -11), 70), (340, (10, 0), -76, 3, (12, -10), 70), (-66, (10, 10), -84, 1, (7, -8), 80)]
SWORD_SLASH = [(-58, (10, 9), -86, 0, (7, -8), 70), (-130, (3, -6), -95, 1, (8, -9), 60), (-160, (-4, -14), -100, 2, (9, -10), 90),
               (-92, (12, -12), -92, 1, (12, -10), 32), (-24, (18, -3), -80, 2, (14, -10), 30), (40, (22, 8), -70, 3, (15, -11), 30),
               (60, (22, 12), -68, 3, (15, -11), 90), (24, (16, 8), -76, 2, (12, -10), 70), (-58, (10, 9), -86, 0, (7, -8), 80)]
SWORD_SWEEP = [(-58, (10, 9), -86, 0, (7, -8), 70), (-174, (-2, 4), -92, 2, (8, -9), 60), (-186, (-8, 6), -94, 3, (9, -10), 80),
               (-140, (4, 6), -88, 2, (12, -10), 32), (-80, (14, 7), -82, 2, (14, -10), 30), (-30, (20, 8), -76, 3, (15, -11), 30),
               (-4, (22, 9), -72, 3, (15, -11), 80), (-30, (16, 8), -78, 2, (12, -10), 70), (-58, (10, 9), -86, 0, (7, -8), 80)]
DAGGER_STAB = [(-50, (10, 11), -86, 0, (7, -8), 50), (-24, (2, 8), -92, 2, (8, -9), 50), (-6, (-2, 6), -96, 3, (9, -10), 60),
               (-2, (20, 6), -78, 3, (14, -10), 30), (0, (24, 6), -74, 3, (15, -11), 80), (-12, (14, 8), -82, 2, (11, -9), 60), (-50, (10, 11), -86, 0, (7, -8), 60)]
DAGGER_SLASH = [(-50, (10, 11), -86, 0, (7, -8), 50), (-150, (4, 0), -92, 2, (8, -9), 50), (-170, (0, -4), -96, 3, (9, -10), 60),
                (-70, (14, 0), -84, 2, (13, -10), 30), (20, (20, 8), -74, 3, (15, -11), 70), (-10, (14, 8), -82, 2, (11, -9), 60), (-50, (10, 11), -86, 0, (7, -8), 60)]
SPEAR_THRUST = [(-72, (9, 14), -86, 0, (7, -8), 70), (-20, (0, 10), -92, 2, (8, -9), 70), (-8, (-6, 8), -98, 3, (9, -10), 90),
                (-2, (14, 6), -82, 3, (13, -10), 30), (0, (22, 6), -74, 3, (16, -11), 100), (-4, (18, 8), -78, 3, (14, -10), 70),
                (-20, (12, 10), -84, 2, (10, -9), 70), (-72, (9, 14), -86, 0, (7, -8), 80)]
SPEAR_LUNGE = [(-72, (9, 14), -86, 0, (7, -8), 70), (-12, (-4, 8), -100, 4, (6, -12), 100), (-4, (-8, 6), -102, 5, (5, -13), 100),
               (-2, (16, 4), -76, 4, (16, -9), 28), (0, (26, 5), -66, 5, (20, -8), 110), (-2, (22, 6), -70, 5, (19, -8), 70),
               (-18, (12, 10), -84, 2, (11, -9), 70), (-72, (9, 14), -86, 0, (7, -8), 80)]



def side_swings(rest, hold):
    """The heavy weapons' first two blows of the chain: a diagonal cut from high behind down across the front (the left swing), then the
    backhand, rising from low behind up and across (the right swing). The third blow is the overhead chop/slam."""
    r = (rest, hold, -84, 0, (7, -8), 80)
    a = [r, (-165, (-2, -6), -94, 2, (8, -9), 70), (-172, (-6, -14), -100, 3, (9, -10), 110), (-100, (8, -14), -92, 2, (12, -10), 32),
         (-35, (18, -4), -80, 3, (14, -10), 30), (35, (22, 8), -70, 4, (15, -11), 30), (60, (22, 12), -66, 5, (15, -12), 110),
         (40, (16, 8), -74, 3, (13, -11), 70), (rest, hold, -84, 1, (7, -8), 90)]
    b = [r, (140, (6, 8), -92, 2, (8, -9), 70), (156, (0, 12), -98, 4, (9, -10), 110), (112, (8, 14), -90, 3, (12, -10), 32),
         (50, (18, 10), -80, 3, (14, -10), 30), (-15, (22, 0), -70, 3, (15, -11), 30), (-60, (20, -8), -66, 4, (15, -11), 110),
         (-30, (14, 0), -76, 2, (13, -11), 70), (rest, hold, -84, 1, (7, -8), 90)]
    return a, b


AXE_L, AXE_R = side_swings(-137, (8, 12))
MACE_L, MACE_R = side_swings(-125, (8, 12))
ATTACKS = {'axe': [AXE_L, AXE_R, AXE_CHOP], 'mace': [MACE_L, MACE_R, MACE_SLAM], 'pan': [PAN_BONK, PAN_BONK], 'sword': [SWORD_SWEEP, SWORD_SLASH],
           'dagger': [DAGGER_STAB, DAGGER_SLASH], 'spear': [SPEAR_THRUST, SPEAR_LUNGE]}
# the frames in which the blow is live (the first and last are the ready stance)
STRIKE = {'axe': (4, 7), 'mace': (4, 7), 'pan': (3, 5), 'sword': (3, 6), 'dagger': (3, 4), 'spear': (3, 4)}
THRUSTERS = ('spear', 'dagger')
DROP = {'sword': 7, 'dagger': 8, 'pan': 7, 'spear': 9, 'mace': 3}   # the grip's drop from the table's chest-relative offset: blows come from the belly, not the shoulder


def attack(w, variant=0):
    table = ATTACKS[w][variant]
    s0, s1 = STRIKE[w]
    if w in ('axe', 'mace') and variant < 2: s0, s1 = 3, 6     # the side swings are shorter than the overhead blow
    if w == 'sword' and variant == 0: s0, s1 = 3, 6
    if w == 'dagger' and variant == 1: s0, s1 = 3, 4
    out, ms = [], []
    prev = None
    for k, (ang, g, lean, crouch, feet, dur) in enumerate(table):
        hip = (0.0, -HIP_REST + crouch)
        c = chest_of(hip, lean)
        g = (g[0], g[1] + DROP.get(w, 0))
        held, hn, hf = held_at(w, c + V(*g), ang)
        rest = k in (0, len(table) - 1)
        if HOLD[w]['h2'] is None and w in ('axe', 'mace'):
            # a two-handed swing: the far hand on the grip, the near hand 9 up the haft
            if not rest: hn, hf = tup(V(*held['g']) + dirv(ang) * 9.0), held['g']
            else: hn, hf = held['g'], None
        if hf is None: hf = (-1.0, -29.0) if rest or w in ('sword', 'dagger', 'pan') else (-2.0, -26.0)
        if w in ('sword', 'dagger', 'pan') and not rest:
            hf = (-3.0 + 0.2 * g[0], -26.0 + 0.4 * g[1])        # the free arm swings out for balance
        if not rest and w in ('axe', 'mace'): held['zh'] = 12.0
        if not rest and w in ('pan',): held['zh'] = 9.0
        P = pose(hip=hip, lean=lean, head=lean + 3.0, fn=(feet[0], 0.0), ff=(feet[1], 0.0), hn=hn, hf=hf, held=held,
                 eye=(w == 'axe' and k == 3), skirt=(-10.0 if s0 <= k <= s1 else 0.0), beard=(-3.0 if s0 <= k <= s1 else 0.0))
        if s0 <= k <= s1 and prev is not None: P['smear_from'] = prev
        out.append(P); ms.append(dur)
        prev = held
    return out, ms


# ---------------------------------------------------------------- aiming (crossbow, staff, the grapple): one pose per angle
AIM_ANGLES = list(range(-90, 91, 15))


def aim(w, recoil=False):
    out = []
    for th in AIM_ANGLES:
        lean = -88.0 - th * 0.10
        hip = (0.0, -HIP_REST + (1.0 if w == 'crossbow' else 0.0))
        c = chest_of(hip, lean)
        shn = c + V(0.5, 2.5)
        a = dirv(th)
        if w == 'crossbow':
            back = 3.5 if recoil else 0.0
            g = shn + a * (9.0 - back) + V(0, 1.0)
            held, hn, hf = held_at(w, g, th)
            hn, hf = tup(g), tup(g + a * 13.0)              # the trigger hand near, the far hand under the fore-end
            hf = hf
            P = pose(hip=hip, lean=lean - (2.0 if recoil else 0.0), head=th * 0.3 - 86.0, hn=hn, hf=hf, held=held, fn=(8.0, 0.0), ff=(-8.0, 0.0), skirt=(-4.0 if recoil else 0.0))
            P['far_arm_z'] = 9.0
        elif w == 'staff':
            g = shn + a * 11.0
            held, hn, hf = held_at(w, g, th)
            hn, hf = tup(g), (-1.0, -29.0)
            P = pose(hip=hip, lean=lean, head=th * 0.3 - 86.0, hn=hn, hf=hf, held=held, fn=(8.0, 0.0), ff=(-8.0, 0.0), skirt=(-3.0 if recoil else 0.0))
        else:   # the grapple: the near arm out toward the hook, a short launcher in the fist
            g = shn + a * 12.0
            held = dict(type='hook', g=tup(g), ang=th, zg=12.0, zh=12.0)
            P = pose(hip=hip, lean=lean, head=th * 0.3 - 86.0, hn=tup(g), hf=(-1.0, -29.0), held=held, fn=(8.0, 0.0), ff=(-8.0, 0.0))
        out.append(P)
    return out, [100] * len(out)


# ---------------------------------------------------------------- the body moves (nothing in the hands: the game draws the weapon stowed)
def crouch(n=4):
    out = []
    for i in range(n):
        t = i / n * 2 * math.pi
        out.append(pose(hip=(0.0, -17.0 + 0.5 * (0.5 - 0.5 * math.cos(t))), lean=-72.0, head=-82.0, fn=(11.0, 0.0), ff=(-7.0, 0.0),
                        hn=(13.0, -22.0), hf=(9.0, -21.0), skirt=3.0))
    return out, [140] * n


def crouchwalk(n=6):
    out = []
    for i in range(n):
        ph = i / n * 2 * math.pi
        s, c = math.sin(ph), math.cos(ph)
        out.append(pose(hip=(0.0, -17.0 + 0.8 * abs(s)), lean=-70.0, head=-80.0, fn=(11.0 + 8.0 * s, -5.0 * max(0.0, c)), ff=(-7.0 - 8.0 * s, -5.0 * max(0.0, -c)),
                        hn=(13.0 - 3.0 * s, -22.0), hf=(9.0 + 3.0 * s, -21.0), skirt=-4.0))
    return out, [90] * n


def crawl(n=6):
    out = []
    for i in range(n):
        ph = i / n * 2 * math.pi
        s, c = math.sin(ph), math.cos(ph)
        out.append(pose(hip=(-14.0, -9.0), lean=-8.0, head=-46.0, fn=(-31.0 + 3.0 * s, -3.0 - 2.0 * max(0.0, c)), ff=(-29.0 - 3.0 * s, -3.0 - 2.0 * max(0.0, -c)),
                        hn=(18.0 + 6.0 * s, -6.0 - 2.0 * max(0.0, c)), hf=(17.0 - 6.0 * s, -6.0 - 2.0 * max(0.0, -c)), kn=-1, skirt=8.0, beard=2.0,
                        toe=(-3.0, 3.0)))
    return out, [100] * n


def _tuck(rot, hip_y=-20.0):
    return pose(hip=(0.0, hip_y), lean=-62.0, head=-60.0, fn=(10.0, -13.0), ff=(8.0, -9.0), hn=(15.0, -22.0), hf=(12.0, -24.0), rot=rot, pivot=(2.0, hip_y - 8.0), skirt=0.0)


def roll():
    # the dive: crouch and spring, arms thrown out ahead and the body stretched long, then curling into the tuck as the shoulder meets the ground
    rin = [pose(hip=(0.0, -20.0), lean=-60.0, head=-62.0, fn=(12.0, 0.0), ff=(-8.0, 0.0), hn=(18.0, -22.0), hf=(14.0, -20.0), skirt=-4.0),
           pose(hip=(4.0, -30.0), lean=-26.0, head=-28.0, fn=(-12.0, -14.0), ff=(-18.0, -10.0), hn=(30.0, -34.0), hf=(27.0, -30.0), skirt=-14.0, beard=-4.0),
           pose(hip=(6.0, -27.0), lean=-30.0, head=-36.0, fn=(-6.0, -13.0), ff=(-11.0, -9.0), hn=(32.0, -26.0), hf=(28.0, -22.0), skirt=-12.0, beard=-4.0),
           _tuck(0.0, -20.0)]
    rl = [_tuck(math.degrees(2 * math.pi * k / 8) + 20.0) for k in range(8)]
    rout = [pose(hip=(2.0, -24.0), lean=-66.0, head=-72.0, fn=(14.0, 0.0), ff=(-3.0, 0.0), hn=(8.0, -16.0), hf=(0.0, -20.0), skirt=-10.0),
            pose(hip=(1.0, -26.0), lean=-80.0, head=-84.0, fn=(10.0, 0.0), ff=(-5.0, 0.0), hn=(9.0, -26.0), hf=(-1.0, -26.0), skirt=-6.0),
            pose(hip=(0.0, -28.0), lean=-86.0, head=-86.0, fn=(8.0, 0.0), ff=(-7.0, 0.0), hn=(10.0, -34.0), hf=(-1.0, -29.0), skirt=0.0)]
    return (rin, [50, 60, 50, 40]), (rl, [40] * 8), (rout, [60, 60, 70])


def jump():
    return [pose(hip=(0.0, -31.0), lean=-90.0, head=-92.0, fn=(8.0, -9.0), ff=(-6.0, -13.0), hn=(7.0, -52.0), hf=(1.0, -50.0), skirt=-6.0, kn=-1)], [100]


def fall():
    return [pose(hip=(0.0, -32.0), lean=-92.0, head=-92.0, fn=(5.0, -2.0), ff=(-6.0, -7.0), hn=(16.0, -54.0), hf=(-10.0, -54.0), skirt=14.0, beard=2.0)], [100]


def land():
    return [pose(hip=(0.0, -16.0), lean=-70.0, head=-80.0, fn=(12.0, 0.0), ff=(-8.0, 0.0), hn=(13.0, -18.0), hf=(6.0, -18.0), skirt=8.0),
            pose(hip=(0.0, -22.0), lean=-80.0, head=-84.0, fn=(10.0, 0.0), ff=(-8.0, 0.0), hn=(11.0, -26.0), hf=(4.0, -26.0), skirt=4.0)], [70, 70]


def climb(n=6):
    out = []
    for i in range(n):
        ph = i / n * 2 * math.pi
        s, c = math.sin(ph), math.cos(ph)
        out.append(pose(hip=(-3.0, -28.0 - 2.0 * abs(s)), lean=-92.0, head=-92.0, fn=(4.0, -13.0 + 9.0 * s), ff=(2.0, -13.0 - 9.0 * s),
                        hn=(9.0, -58.0 + 10.0 * s), hf=(7.0, -58.0 - 10.0 * s), kn=-1, skirt=3.0, far_arm_z=-3.0))
    return out, [90] * n


def wall():
    return [pose(hip=(-4.0, -27.0), lean=-98.0, head=-96.0, fn=(5.0, -3.0), ff=(2.0, -9.0), hn=(14.0, -46.0), hf=(10.0, -50.0), skirt=6.0),
            pose(hip=(-4.0, -27.0), lean=-99.0, head=-96.0, fn=(5.0, -4.0), ff=(2.0, -8.0), hn=(14.0, -47.0), hf=(10.0, -49.0), skirt=7.0)], [120, 120]


def hang(n=4):
    out = []
    for i in range(n):
        t = i / n * 2 * math.pi
        out.append(pose(hip=(1.0, -29.0), lean=-90.0 + 2.0 * math.sin(t), head=-90.0, fn=(4.0 + math.sin(t), -2.0), ff=(-4.0 - math.sin(t), -3.0),
                        hn=(8.0, -78.0), hf=(6.0, -80.0), skirt=2.0 * math.sin(t), kn=-1, far_arm_z=-3.0))
    return out, [140] * n


def swim(n=6):
    out = []
    for i in range(n):
        ph = i / n * 2 * math.pi
        s, c = math.sin(ph), math.cos(ph)
        piv = (0.0, -26.0)
        # the figure is built upright and turned prone: his "up" is the way he swims. A crawl stroke: the arms circle, the legs flutter
        out.append(pose(hip=(0.0, -26.0), lean=-90.0, head=-92.0, rot=76.0, pivot=piv,
                        hn=(8.0 + 14.0 * c, -48.0 + 14.0 * s), hf=(6.0 - 14.0 * c, -48.0 - 14.0 * s),
                        fn=(5.0, -3.0 + 4.0 * s), ff=(-5.0, -3.0 - 4.0 * s), skirt=-6.0, beard=-3.0))
    return out, [90] * n


def tread(n=4):
    out = []
    for i in range(n):
        ph = i / n * 2 * math.pi
        s = math.sin(ph)
        out.append(pose(hip=(0.0, -30.0), lean=-90.0, head=-90.0, fn=(8.0, -8.0 + 3.0 * s), ff=(-6.0, -8.0 - 3.0 * s),
                        hn=(13.0, -30.0 - 3.0 * s), hf=(-8.0, -30.0 + 3.0 * s), skirt=3.0))
    return out, [120] * n


def hurt():
    return [pose(hip=(-2.0, -27.0), lean=-98.0, head=-104.0, fn=(8.0, 0.0), ff=(-9.0, 0.0), hn=(-4.0, -40.0), hf=(-9.0, -34.0), skirt=10.0, beard=3.0),
            pose(hip=(-1.0, -28.0), lean=-94.0, head=-98.0, fn=(8.0, 0.0), ff=(-8.0, 0.0), hn=(2.0, -38.0), hf=(-6.0, -32.0), skirt=6.0)], [90, 90]


def body_clips():
    """Clips with nothing in the hands. -> {name: (poses, ms)}"""
    rin, rl, rout = roll()
    return {'jump': jump(), 'fall': fall(), 'land': land(), 'crouch': crouch(), 'crouchwalk': crouchwalk(), 'crawl': crawl(),
            'rollin': rin, 'roll': rl, 'rollout': rout, 'climb': climb(), 'wall': wall(), 'hang': hang(), 'swim': swim(), 'tread': tread(), 'hurt': hurt()}


def weapon_clips(w):
    """Clips with a weapon in hand. -> {name: (poses, ms)}"""
    c = {'idle': idle(w), 'run': run(w)}
    if w in MELEE:
        c['atk0'] = attack(w, 0)
        c['atk1'] = attack(w, 1)
        if len(ATTACKS[w]) > 2: c['atk2'] = attack(w, 2)
    else:
        c['aim'] = aim(w)
        c['fire'] = aim(w, recoil=True)
    return c

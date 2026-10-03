# Paints every weapon on the 45-degree diagonal in the style of the hand-drawn reference sword: clean
# stair-stepped edges, muted greys with a lit bevel and a shadowed edge, a checkered guard, dark leather
# bound raggedly round the grips, gold pommels and caps, the same wood on every haft.
# t runs along the weapon (pixels, toward the tip), s across it (negative = the upper-left, lit side).
#   python tools/weapons.py preview out.png   draws them all at 8x (needs Pillow); magenta marks each grip
#   python tools/weapons.py                   prints each one's rows and grip pivot for the WART table in rig.cpp
import math, sys

S2 = math.sqrt(2)
PAL = {  # preview colours only; in game the metal letters are tinted by the weapon's metal
    'A': (96, 96, 100), 'C': (109, 109, 114), 'D': (122, 122, 128), 'E': (54, 54, 57), 'F': (74, 74, 78),
    'a': (58, 34, 27), 'c': (40, 24, 19), 'e': (78, 48, 36),
    'f': (187, 159, 73), 'i': (140, 112, 48), 'j': (230, 204, 118),
    'k': (92, 62, 44), 'l': (62, 40, 28), 'n': (120, 84, 58),
    'q': (196, 170, 96), 'r': (34, 32, 32), 'G': (120, 200, 255), 'u': (220, 245, 255),
}


def grip(t, s, t0, t1):
    """Dark leather bound round in a spiral, its edges ragged."""
    lim = 1.75 + (0.5 if int(math.floor(t * 0.9)) % 2 == 0 else 0)
    if abs(s) > lim: return None
    if abs(s) > lim - 0.7: return 'c'
    return 'e' if int(math.floor(t - s)) % 3 == 0 else 'a'


def knot(t, s, tc, r):
    """A gold pommel or cap, lit on its upper side."""
    if math.hypot(t - tc, s) > r: return None
    return 'j' if s < -r * 0.35 else ('f' if s < r * 0.4 else 'i')


def haft(t, s, hw=1.55):
    if abs(s) > hw: return None
    return 'n' if s < -hw * 0.35 else ('l' if s > hw * 0.45 else 'k')


def metal(s, lo, hi):
    """Across a blade from its lit edge (lo) to its shadowed edge (hi): edge, bevel, body, shadow."""
    if s < lo + 0.75: return 'D'
    if s > hi - 0.75: return 'E'
    return 'C' if s < (lo + hi) / 2 - 0.2 else 'A'


def sword(t, s):
    if t < -12.5: return knot(t, s, -14, 2.2)
    if t < -1.6: return grip(t, s, -12.5, -1.6)
    if t < 0.9:  # crossguard, checkered like the reference
        if abs(s) > 6.4: return None
        if abs(s) > 5.4: return 'f'
        return 'D' if (int(math.floor(s + 10)) + int(math.floor(t + 10))) % 2 else 'r'
    L = 24.0
    hw = 2.9 if t < L - 6 else 2.9 * max(0, (L - t) / 6)
    if t > L or abs(s) > hw: return None
    if abs(s + 0.2) < 0.55 and 1.5 < t < L - 7 and -hw + 0.75 <= s <= hw - 0.75: return 'F'  # the fuller
    return metal(s, -hw, hw)


def dagger(t, s):  # after the Damascus dagger: disc guard, ringed wooden grip, brass band, round pommel
    if t < -11.6:
        d = math.hypot(t + 13.3, s)
        if d > 2.6: return None
        return 'n' if s < -0.5 else ('l' if s > 0.9 else 'k')
    if t < -10.0: return 'q' if abs(s) < 1.9 else None
    if t < -1.1:
        lim = 1.45 + 0.55 * (0.5 + 0.5 * math.cos((t + 10) * 1.6))
        return haft(t, s, lim)
    if t < 0.6:
        if abs(s) > 4.4: return None
        if abs(s) > 3.6: return 'E'
        return 'q' if int(math.floor(s * 1.4 + 9)) % 2 else 'r'
    L = 16.0
    hw = 2.9 * (1 - t / L) + 0.3
    if t > L or abs(s) > hw: return None
    if -hw + 0.7 <= s <= hw - 0.7:
        ripple = math.sin(t * 1.9 + math.sin(s * 3.1 + t * 0.7) * 2.2)
        if ripple > 0.45: return 'C'
        if ripple < -0.55: return 'F'
    return metal(s, -hw, hw)


def axe(t, s):  # the bearded axe: head on the lit side, its long edge sweeping back toward the hand
    if t < -14.5: return knot(t, s, -15.6, 2.0)
    if t < -5: return grip(t, s, -14.5, -5)
    if -13.5 <= s <= -1.2:
        edge = -13.5 + 0.035 * (t - 15.5) ** 2  # the cutting edge bows out in a crescent
        lo = 15.5 - max(0.0, -s - 3.5) * 0.95  # the hollow under the beard
        hi = 24 - max(0.0, -s - 7) * 0.5
        if lo <= t <= hi and s >= edge:
            if s < edge + 1.0: return 'D'  # the cutting edge
            if s < edge + 2.1: return 'C'  # its bevel
            if s > -2.3 or t > hi - 0.9 or t < lo + 0.8: return 'E'
            return 'F' if (t - s * 0.6) % 6 < 0.7 else 'A'
    if t > 24.5: return None
    return haft(t, s, 1.8)


def pan(t, s):  # the frying pan: an iron disc on a short neck, the handle bound in leather
    if t < -12.5: return knot(t, s, -13.5, 1.5)
    if t < -3.5: return grip(t, s, -12.5, -3.5)
    d = math.hypot(t - 11.5, s)
    if d <= 10.5:
        lit = (t - 11.5) * 0.3 - s > 0.8  # the upper-left of the rim faces the light
        if d > 9.6: return 'D' if lit else 'E'
        if d > 8.3: return 'C' if lit else 'A'
        if d > 7.2: return 'r' if lit else 'C'  # the inner wall: in shadow on the lit side
        return 'C' if (abs(d - 4.5) < 0.55 and t - 11.5 - s < -1.5) else 'A'
    if t < 1.5: return 'E' if abs(s) < 1.1 else None  # the iron neck
    return None


def spear(t, s):
    if t < -13: return knot(t, s, -14, 1.4)
    if -6 < t < 2: return grip(t, s, -6, 2)
    if t < 25.5: return haft(t, s, 0.95)
    if t < 27.6: return ('f' if s < 0.6 else 'i') if abs(s) < 1.5 else None  # socket rings
    if t < 28: return 'E' if abs(s) < 1.0 else None
    L = 13.0
    x = (t - 28) / L
    if x > 1: return None
    hw = 3.4 * math.sin(math.pi * min(0.5, x * 0.9)) ** 0.7 * (1 if x < 0.55 else (1 - x) / 0.45)
    if abs(s) > hw: return None
    if abs(s) < 0.35 and hw > 0.9: return 'C'  # the midrib
    return metal(s, -hw, hw)


def hammer(t, s):  # a war hammer: a heavy block with a brass band, on a bound haft
    if t < -13: return knot(t, s, -14, 1.7)
    if t < -4: return grip(t, s, -13, -4)
    if 13.5 <= t <= 23 and abs(s) <= 7:
        if 17.3 < t < 19.2: return 'q' if int(math.floor(s + 10)) % 2 else 'r'  # brass band, studded
        if s < -6.1: return 'D'
        if s > 6.0: return 'E'
        if t > 22.1: return 'C'
        if t < 14.4: return 'E'
        return 'C' if s < -3.5 else 'A'
    if t > 23: return None
    return haft(t, s)


def staff(t, s):  # carved wood with gold bands, a gem held in a claw at the head
    if t < -17: return knot(t, s, -18, 1.4)
    if 25.5 <= t <= 31.5:
        d = math.hypot(t - 28.3, s)
        if d <= 2.1: return 'u' if (s < -0.6 and t > 28.6) else 'G'
        if abs(abs(s) - 2.6 + (t - 25.5) * 0.12) < 0.7 and t < 31: return 'l' if s > 0 else 'n'  # the claw
        return None
    if t > 25.5: return None
    for b in (-9, 0, 9, 24):
        if abs(t - b) < 0.8 and abs(s) < 1.45: return 'f' if s < 0.4 else 'i'
    if -6 < t < -1.5: return grip(t, s, -6, -1.5)
    return haft(t, s, 1.0)


WEAPONS = {  # name: (painter, t range, grip t)
    'dagger': (dagger, (-16.0, 16.2), -6),
    'sword': (sword, (-16.3, 24.2), -7),
    'axe': (axe, (-17.7, 24.6), -9),
    'staff': (staff, (-19.5, 31.6), -4),
    'spear': (spear, (-15.5, 41.2), -2),
    'hammer': (hammer, (-16.3, 23.1), -8),
    'pan': (pan, (-15.1, 22.2), -8),
}


def build(name):
    fn, (t0, t1), tg = WEAPONS[name]
    m = 6  # margin, for the width across
    n = int(math.ceil((t1 - t0) / S2)) + 2 * m
    ox, oy = m - t0 / S2, n - m + t0 / S2
    rows = []
    for y in range(n):
        r = ''
        for x in range(n):
            px, py = x + 0.5 - ox, y + 0.5 - oy
            r += fn((px - py) / S2, (px + py) / S2) or '.'
        rows.append(r)
    # trim the empty margin, keeping the grip pivot in step
    top = next(i for i, r in enumerate(rows) if r.strip('.'))
    bot = max(i for i, r in enumerate(rows) if r.strip('.'))
    left = min(len(r) - len(r.lstrip('.')) for r in rows if r.strip('.'))
    right = max(len(r.rstrip('.')) for r in rows)
    rows = [r[left:right] for r in rows[top:bot + 1]]
    gx, gy = ox + tg / S2, oy - tg / S2
    return rows, int(gx) - left, int(gy) - top


if __name__ == '__main__':
    from PIL import Image
    built = {k: build(k) for k in WEAPONS}
    if len(sys.argv) > 1 and sys.argv[1] == 'preview':
        W = sum(len(r[0][0]) + 3 for r in built.values()) + 3
        H = max(len(r[0]) for r in built.values()) + 6
        im = Image.new('RGB', (W, H), (60, 60, 64))
        x0 = 3
        for rows, gx, gy in built.values():
            for y, row in enumerate(rows):
                for x, ch in enumerate(row):
                    if ch in PAL: im.putpixel((x0 + x, 3 + y), PAL[ch])
            im.putpixel((x0 + gx, 3 + gy), (255, 0, 255))
            x0 += len(rows[0]) + 3
        im.resize((W * 8, H * 8), Image.NEAREST).save(sys.argv[2])
    else:
        for k, (rows, gx, gy) in built.items():
            print(f'{k} {gx} {gy}')
            print('\n'.join(rows))
            print()

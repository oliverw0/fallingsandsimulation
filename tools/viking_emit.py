# Bakes every Viking clip at game scale and writes sprites_viking.h:  python tools/viking_emit.py > sprites_viking.h
# One global palette (colour, alpha, tag); each sheet is a stack of frames, run-length coded; tags: 1 = takes the weapon's
# metal colour in game, 2 = the staff gem's, 3 = the worn armour's metal, 4 = a strike smear (its alpha is the colour's own).
# Four looks (gambeson, mail, lamellar, plate) are baked: VK_LOOKS[look][set].
import math, os, sys
import numpy as np
from PIL import Image, ImageDraw
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from viking_clips import *
import viking_clips as VC
import shutil
import viking3d as V3
USE_3D = shutil.which('blender') is not None   # the body comes from the Blender model (viking3d.py); without Blender, the 2.5D build()

SCALE = 0.62
FW, FH, GX, GND = 112, 100, 48, 86
CLIP_NAMES = ['idle', 'run', 'atk0', 'atk1', 'atk2', 'atk3', 'aim', 'fire', 'jump', 'fall', 'land', 'crouch', 'crouchwalk', 'crawl', 'rollin', 'roll', 'rollout',
              'climb', 'wall', 'hang', 'swim', 'tread', 'hurt', 'hookaim']
SMEAR = [(250, 252, 220, 240), (120, 236, 255, 215), (56, 150, 255, 180), (40, 72, 196, 140)]   # by how old that part of the sweep is


# ---------------------------------------------------------------- the weapons' cutting parts, for the smear
def held_polys(H):
    t = H['type']
    g, a = V(*H['g']), dirv(H['ang'])
    s = np.array([-a[1], a[0]])
    pt = lambda base, u, v: base + a * u + s * v
    if t == 'axe':
        fo = H.get('fore', 1.0)
        head = g + a * 48 * fo
        return [[head + a * u * fo + s * v for v, u in AXE_HEAD]]
    if t in ('sword', 'dagger'):
        L = 46 if t == 'sword' else 21
        if t == 'sword':
            fo = H.get('fore', 1.0)
            pt = lambda base, u, v: base + a * (u * fo) + s * v
        return [[pt(g, 2, 2.4), pt(g, L, 2.4), pt(g, L + 2, 0), pt(g, L, -2.4), pt(g, 2, -2.4)]]
    if t == 'spear':
        tip = g + a * 34
        return [[pt(tip, -14, 2.5), pt(tip, 14, 4), pt(tip, 16, 0), pt(tip, 14, -4), pt(tip, -14, -2.5)]]
    if t == 'mace':
        fo = H.get('fore', 1.0)
        c = g + a * 32 * fo
        return [[c + a * 7.5 * fo * math.cos(q) + s * 7.5 * math.sin(q) for q in np.linspace(0, 2 * math.pi, 12, endpoint=False)]]
    if t == 'pan':
        c = g + a * 25
        return [[c + a * 9.5 * math.cos(q) + s * 9.0 * math.sin(q) for q in np.linspace(0, 2 * math.pi, 16, endpoint=False)]]
    return []


def smear_layer(P, prev, d, w, h, steps=14):
    """-> (bucket[h][w] in 0..3 or -1) for the blade swept from `prev` to this frame's held pose."""
    cur = P['held']
    out = np.full((h, w), -1, int)
    if cur is None or prev is None or cur['type'] != prev['type']: return out
    for k in range(steps, 0, -1):               # oldest first, so newer parts overwrite
        u = k / steps
        H = dict(cur)
        gx = prev['g'][0] + (cur['g'][0] - prev['g'][0]) * (1 - u + 0)
        gy = prev['g'][1] + (cur['g'][1] - prev['g'][1]) * (1 - u + 0)
        H['g'] = (gx, gy)
        H['ang'] = prev['ang'] + (cur['ang'] - prev['ang']) * (1 - u + 0)
        H['fore'] = prev.get('fore', 1.0) + (cur.get('fore', 1.0) - prev.get('fore', 1.0)) * (1 - u)
        age = u                                   # 1 = the previous frame's position, 0 = now
        # (the loop runs from old to new: u falls from 1 to 1/steps)
        for poly in held_polys(H):
            im = Image.new('L', (w, h), 0)
            ImageDraw.Draw(im).polygon([d.P(p) for p in poly], fill=1)
            m = np.array(im, bool)
            b = 0 if age < 0.18 else (1 if age < 0.38 else (2 if age < 0.62 else 3))
            if age > 0.6:
                yy, xx = np.mgrid[0:h, 0:w]
                m &= ((xx + yy) % 2 == 0)         # the old part is dithered
            out[m] = b
    return out


# ---------------------------------------------------------------- baking
PAL = {}
PAL_LIST = []


def pal_index(r, g, b, a, tag):
    k = (int(r), int(g), int(b), int(a), int(tag))
    if k not in PAL:
        PAL[k] = len(PAL_LIST); PAL_LIST.append(k)
    return PAL[k]


def bake(P):
    """-> index grid (h x w) of palette indices, 0 = clear."""
    cv = Canvas(FW, FH, GND, SCALE)
    d = Draw(cv, GX, 1, SCALE, rot=math.radians(P['rot']), pivot=P['pivot'])
    if USE_3D:
        V3.body(cv, d, P, P.get('look', 0))
        if P['held']: draw_held(d, P['held'])
    else: build(d, P)
    rgb, tag, op = cv.render(rim_side=1)
    idx = np.zeros((FH, FW), int)
    sm = smear_layer(P, P.get('smear_from'), d, FW, FH) if P.get('smear_from') else None
    for y in range(FH):
        for x in range(FW):
            if op[y, x]: idx[y, x] = 1 + pal_index(*rgb[y, x], 255, tag[y, x])
            elif sm is not None and sm[y, x] >= 0:
                r, g, b, a = SMEAR[sm[y, x]]
                idx[y, x] = 1 + pal_index(r, g, b, a, 4)
    return idx


SET_ORDER = ['none', 'dagger', 'sword', 'axe', 'spear', 'mace', 'pan', 'crossbow', 'staff', 'body']
LOOK_COUNT = 4


def build_sheets():
    """-> {(set name, look): (frames, clip table)}"""
    sheets = {}
    sets = {'none': {'idle': idle_none(), 'run': run_none()}}
    for w in WEAPONS: sets[w] = weapon_clips(w)
    body = body_clips()
    body['hookaim'] = aim('hook')
    sets['body'] = body
    if USE_3D: V3.ensure([P for clips in sets.values() for poses, ms in clips.values() for P in poses], SCALE, FW, FH, GX, GND)
    else: print('viking_emit: no blender on PATH, using the 2.5D body', file=sys.stderr)
    for look in range(LOOK_COUNT):
        for name, clips in sets.items():
            frames, table = [], {}
            for cn in CLIP_NAMES:
                if cn in clips:
                    poses, ms = clips[cn]
                    table[cn] = (len(frames), len(poses))
                    for P in poses: frames.append(bake(dict(P, look=look)))
                else: table[cn] = (0, 0)
            sheets[(name, look)] = (frames, table)
    return sheets


def idle_none(n=8):
    poses, ms = VC.idle('axe', n)
    for P in poses: P['held'] = None; P['hn'] = (10.0, -26.0); P['hf'] = (6.0, -28.0)
    return poses, ms


def run_none(n=8):
    poses, ms = VC.run('axe', n)
    for i, P in enumerate(poses):
        ph = i / n * 2 * math.pi
        P['held'] = None
        P['hn'] = (4 + 9.0 * math.sin(ph), -32.0 + 3.0 * math.cos(ph))
        P['hf'] = (4 - 9.0 * math.sin(ph), -32.0 - 3.0 * math.cos(ph))
    return poses, ms


def rle(idx_frames, x0, y0, x1, y1):
    out, offs = [], []
    for f in idx_frames:
        offs.append(len(out))
        flat = f[y0:y1, x0:x1].reshape(-1)
        i = 0
        while i < len(flat):
            v, n = flat[i], 1
            while i + n < len(flat) and flat[i + n] == v and n < 255: n += 1
            out += [int(v), n]
            i += n
    return out, offs


def emit():
    sheets = build_sheets()
    out = ['// Generated by tools/viking_emit.py (tools/viking*.py) - edit there, then: python tools/viking_emit.py > sprites_viking.h',
           '#pragma once', '#include <raylib.h>', '',
           '// The Viking (Dead Cells-style baked 3D primitives). One shared palette: colour (alpha = its own), tag (1 weapon metal, 2 staff gem,',
           '// 3 worn armour metal, 4 strike smear). A sheet is a stack of frames, each fw x fh, run-length coded as (palette index + 1 or 0 = clear,',
           '// run) unsigned-short pairs; he faces right and the game flips the picture for left. (ax, ay) is the point between his feet on the ground.',
           'enum VkClip { ' + ', '.join('VC_' + c.upper() for c in CLIP_NAMES) + ', VC_COUNT };',
           'enum VkSet { ' + ', '.join('VS_' + s.upper() for s in SET_ORDER) + ', VS_COUNT };',
           'struct VkSheet { int fw, fh, ax, ay, frames; const unsigned short* rle; const int* off; int first[VC_COUNT], count[VC_COUNT]; };', '']
    body = []
    for (name, look), (frames, table) in sheets.items():
        xs, ys = [], []
        for f in frames:
            m = f > 0
            if m.any():
                yy, xx = np.where(m); xs += [xx.min(), xx.max() + 1]; ys += [yy.min(), yy.max() + 1]
        x0, x1, y0, y1 = min(xs), max(xs), min(ys), max(ys)
        data, offs = rle(frames, x0, y0, x1, y1)
        N = f'{name.upper()}_{look}'
        body.append(f'static const unsigned short VKD_{N}[] = {{' + ','.join(map(str, data)) + '};')
        body.append(f'static const int VKO_{N}[] = {{' + ','.join(map(str, offs)) + '};')
        first = ','.join(str(table[c][0]) for c in CLIP_NAMES)
        count = ','.join(str(table[c][1]) for c in CLIP_NAMES)
        body.append(f'static const VkSheet VK_{N} = {{{x1 - x0}, {y1 - y0}, {GX - x0}, {GND - y0}, {len(frames)}, VKD_{N}, VKO_{N}, {{{first}}}, {{{count}}}}};')
        body.append('')
        print(name, look, len(frames), 'frames', x1 - x0, 'x', y1 - y0, file=sys.stderr)
    out.append('static const Color VK_PAL[] = {{0, 0, 0, 0}, ' + ', '.join(f'{{{r}, {g}, {b}, {a}}}' for r, g, b, a, t in PAL_LIST) + '};')
    out.append('static const unsigned char VK_TAG[] = {0, ' + ', '.join(str(t) for r, g, b, a, t in PAL_LIST) + '};')
    out.append('')
    print('palette', len(PAL_LIST) + 1, file=sys.stderr)
    out = out + body
    out.append('static const VkSheet* const VK_LOOKS[' + str(LOOK_COUNT) + '][VS_COUNT] = {')
    for look in range(LOOK_COUNT):
        out.append('    {' + ', '.join(f'&VK_{s.upper()}_{look}' for s in SET_ORDER) + '},')
    out.append('};')
    return '\n'.join(out) + '\n'


if __name__ == '__main__':
    sys.stdout.write(emit())

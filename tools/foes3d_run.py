# Rendering, previewing and emitting the starter foes (tools/foes3d.py + tools/foes3d_beasts.py).
#   python tools/foes3d_run.py preview <dir> [names]      sheets of every clip, 4x
# tools/userart.py emit calls emit_foe(out, name) for each name in FOES.
import math, os, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import viking as VK
from viking import Canvas, Draw, V
import foes3d as F

JOINT_NAMES = ['neck', 'pel', 'headb', 'headt', 'shF', 'elF', 'haF', 'shN', 'elN', 'haN', 'hipF', 'knF', 'ftF', 'hipN', 'knN', 'ftN', 'wb', 'wt']
CLIP_ORDER = ['idle', 'walk', 'windup', 'strike', 'recover', 'hurt', 'land']

# canvas (w, h, gx, ground) per biped
CANVAS = {'goblin': (56, 46, 24, 40), 'bomber': (56, 48, 24, 42), 'redcap': (84, 54, 26, 48), 'raider': (92, 70, 28, 64), 'risen': (92, 70, 28, 64)}
# RigSpec shoulder placement: shoulder drop fraction, shoulder half-width and hip half-width (px)
BODY = {'goblin': (0.2, 3.0, 1.6), 'bomber': (0.2, 3.0, 1.6), 'redcap': (0.2, 3.4, 1.8), 'raider': (0.16, 4.4, 2.4), 'risen': (0.16, 4.4, 2.4)}
KIND = {'goblin': 'stab', 'bomber': 'throw', 'redcap': 'thrust', 'raider': 'thrust', 'risen': 'thrust'}


def clips_of(name):
    T = F.TYPES[name]
    c = F.clip_biped(T, KIND[name])
    return [(k, c[k]) for k in CLIP_ORDER if k in c]


def render_biped(name, P, only=None):
    T = F.TYPES[name]
    fw, fh, gx, gnd = CANVAS[name]
    cv = Canvas(fw, fh, gnd, 1.0)
    d = Draw(cv, gx, 1, 1.0)
    w = F.FoeW(d, only)
    J = F.build_biped(w, P, T)
    rgb, tag, op = cv.render(rim_side=1)
    return rgb, tag, op, J, d


def joints_biped(name, J, P, d):
    T = F.TYPES[name]
    nk = J['N']
    hd = J['head']
    held = P.get('held')
    wb = J['arms']['n'][2]
    pts = dict(neck=nk, pel=J['hip'], headb=nk, headt=hd + V(T['hr'] * 0.8, -0.5),
               shF=J['arms']['f'][0], elF=J['arms']['f'][1], haF=J['arms']['f'][2], shN=J['arms']['n'][0], elN=J['arms']['n'][1], haN=J['arms']['n'][2],
               hipF=J['legs']['f'][0], knF=J['legs']['f'][1], ftF=J['legs']['f'][2], hipN=J['legs']['n'][0], knN=J['legs']['n'][1], ftN=J['legs']['n'][2],
               wb=wb, wt=wb + V(8, 0))
    return [d.P(pts[k]) for k in JOINT_NAMES]


def grid_of(rgb, tag, op):
    h, w = op.shape
    g = [[None] * w for _ in range(h)]
    for y in range(h):
        for x in range(w):
            if op[y, x]: g[y][x] = (tuple(int(v) for v in rgb[y, x]), int(tag[y, x]))
    return g


def reduce_palette(grids, limit=88):
    """The sprite format holds 91 colours a sheet: merge the closest, least-used colours (same tag) until it fits."""
    from collections import Counter
    cnt = Counter()
    for g in grids:
        for row in g:
            for p in row:
                if p is not None: cnt[p] += 1
    alive = list(cnt)
    if len(alive) <= limit: return grids
    mapping = {c: c for c in alive}
    def dist(a, b):
        return sum((x - y) ** 2 for x, y in zip(a[0], b[0])) + (1e9 if a[1] != b[1] else 0)
    while len(alive) > limit:
        best = None
        for i in range(len(alive)):
            for j in range(i + 1, len(alive)):
                dd = dist(alive[i], alive[j]) * min(cnt[alive[i]], cnt[alive[j]]) ** 0.5
                if best is None or dd < best[0]: best = (dd, i, j)
        _, i, j = best
        x, y = alive[i], alive[j]
        keep, drop = (x, y) if cnt[x] >= cnt[y] else (y, x)
        cnt[keep] += cnt[drop]
        alive.remove(drop)
        for k, v in mapping.items():
            if v == drop: mapping[k] = keep
    return [[[mapping[p] if p is not None else None for p in row] for row in g] for g in grids]


def emit_foe(out, name):
    import anim
    T = F.TYPES[name]
    frames = []
    for clip, poses in clips_of(name):
        for P in poses:
            rgb, tag, op, J, d = render_biped(name, P)
            frames.append((anim.CLIPS.index(clip), grid_of(rgb, tag, op), joints_biped(name, J, P, d)))
    red = reduce_palette([f[1] for f in frames])
    frames = [(f[0], g, f[2]) for f, g in zip(frames, red)]
    P0 = dict(F.bpose(T), held=None)
    slots = {'HEAD': ({'head'}, 'headb', 'headt'), 'TORSO': ({'torso'}, 'neck', 'pel'), 'UARM': ({'uarm'}, 'shN', 'elN'), 'FARM': ({'farm'}, 'elN', 'haN'),
             'THIGH': ({'thigh'}, 'hipN', 'knN'), 'SHIN': ({'shin'}, 'knN', 'ftN')}
    rgb0, tag0, op0, J0, d0 = render_biped(name, P0)
    pts = dict(zip(JOINT_NAMES, joints_biped(name, J0, P0, d0)))
    cells = []
    for slot in ('HEAD', 'TORSO', 'UARM', 'FARM', 'THIGH', 'SHIN', 'WEAPON', 'SHIELD', 'WING', 'TAIL'):
        if slot not in slots: cells.append('{nullptr, 0, 0, 0, 0}'); continue
        only, a, b = slots[slot]
        rgb, tag, op, _, _ = render_biped(name, P0, only)
        if not op.any(): cells.append('{nullptr, 0, 0, 0, 0}'); continue
        g = reduce_palette([grid_of(rgb, tag, op)])[0]
        x0, y0, x1, y1 = anim.crop([g])
        pal = anim.palette_of([g])
        nm = f'{name.upper()}_{slot}'
        out.append(anim.pal_c(f'HDP_A_{nm}', pal))
        out.append(f'static const char* const HDR_A_{nm}[] = {{')
        for j in range(y0, y1): out.append('    "' + ''.join('.' if g[j][i] is None else anim.ALPHABET[pal.index(g[j][i])] for i in range(x0, x1)) + '",')
        out.append('};')
        out.append(f'static const HDSprite HD_A_{nm} = {{{x1 - x0}, {y1 - y0}, HDP_A_{nm}, HDR_A_{nm}}};')
        pa, pb = pts[a], pts[b]
        cells.append(f'{{&HD_A_{nm}, {pa[0] - x0:.2f}f, {pa[1] - y0:.2f}f, {pb[0] - x0:.2f}f, {pb[1] - y0:.2f}f}}')
    shp, sw, hw = BODY[name]
    out.append(f'static const RigSpec RIG_A_{name.upper()} = {{RK_BIPED, {{{", ".join(cells)}}}, 0.0f, 0.0f, {shp:.3f}f, {sw * 2:.2f}f, {hw * 2:.2f}f, RW_NONE, 0}};')
    out.append('')
    fw, fh, gx, gnd = CANVAS[name]
    anim.emit_sheet(out, name.upper(), None, frames, gx, gnd)


def preview(outdir, names=None):
    from PIL import Image
    os.makedirs(outdir, exist_ok=True)
    for name in (names or list(CANVAS)):
        cells = []
        for clip, poses in clips_of(name):
            for P in poses:
                rgb, tag, op, J, d = render_biped(name, P)
                im = Image.new('RGB', (op.shape[1], op.shape[0]), (24, 18, 36))
                p = im.load()
                for y in range(op.shape[0]):
                    for x in range(op.shape[1]):
                        if op[y, x]: p[x, y] = tuple(int(v) for v in rgb[y, x])
                cells.append(im)
        fw, fh = cells[0].size
        cols = 9
        rows = (len(cells) + cols - 1) // cols
        sheet = Image.new('RGB', (cols * fw, rows * fh), (24, 18, 36))
        for k, im in enumerate(cells): sheet.paste(im, ((k % cols) * fw, (k // cols) * fh))
        sheet.resize((sheet.width * 3, sheet.height * 3), Image.NEAREST).save(f'{outdir}/{name}_sheet.png')
        print(name, len(cells), 'frames')


# ---------------------------------------------------------------- the beasts (bat, slime, scorpion, serpent)
import foes3d_beasts as B
BEASTS = {
    'bat': dict(build=B.build_bat, poses=B.bat_poses(), canvas=(44, 44, 22, 34), kind='bat'),
    'slime': dict(build=B.build_slime, poses=B.slime_poses(), canvas=(44, 36, 22, 32), kind='none'),
    'scorpion': dict(build=B.build_scorpion, poses=B.scorpion_poses(), canvas=(160, 96, 84, 84), kind='none', scale=1.45),
    'serpent': dict(build=B.build_serpent, poses=B.serpent_poses(), canvas=(156, 50, 130, 42), kind='none'),
}


def render_beast(name, P, only=None):
    b = BEASTS[name]
    fw, fh, gx, gnd = b['canvas']
    sc = b.get('scale', 1.0)
    cv = Canvas(fw, fh, gnd, sc)
    d = Draw(cv, gx, 1, sc)
    w = F.FoeW(d, only)
    J = b['build'](w, P)
    rgb, tag, op = cv.render(rim_side=1)
    return rgb, tag, op, J, d


def joints_beast(name, J, d):
    c = d.P(V(0, -10))
    pts = {k: c for k in JOINT_NAMES}
    if name == 'bat':
        pts.update(neck=d.P(J['head']), pel=d.P(J['body']), headb=d.P(J['head']), headt=d.P(J['head'] + V(5, 1)), shN=d.P(J['rN']), elN=d.P(J['tN']),
                   shF=d.P(J['rF']), elF=d.P(J['tF']))
    return [pts[k] for k in JOINT_NAMES]


def beast_clips(name):
    P = BEASTS[name]['poses']
    return [(k, P[k]) for k in CLIP_ORDER if k in P]


def emit_beast(out, name):
    import anim
    b = BEASTS[name]
    frames = []
    for clip, poses in beast_clips(name):
        for P in poses:
            rgb, tag, op, J, d = render_beast(name, P)
            frames.append((anim.CLIPS.index(clip), grid_of(rgb, tag, op), joints_beast(name, J, d)))
    red = reduce_palette([f[1] for f in frames])
    frames = [(f[0], g, f[2]) for f, g in zip(frames, red)]
    if b['kind'] == 'bat':
        P0 = b['poses']['idle'][2]
        rgb0, tag0, op0, J0, d0 = render_beast(name, P0)
        pts = dict(zip(JOINT_NAMES, joints_beast(name, J0, d0)))
        slots = {'TORSO': ({'torso', 'head'}, 'neck', 'pel'), 'UARM': ({'uarm'}, 'shN', 'elN')}
        cells = []
        for slot in ('HEAD', 'TORSO', 'UARM', 'FARM', 'THIGH', 'SHIN', 'WEAPON', 'SHIELD', 'WING', 'TAIL'):
            if slot not in slots: cells.append('{nullptr, 0, 0, 0, 0}'); continue
            only, a, bb = slots[slot]
            rgb, tag, op, _, _ = render_beast(name, P0, only)
            g = reduce_palette([grid_of(rgb, tag, op)])[0]
            x0, y0, x1, y1 = anim.crop([g])
            pal = anim.palette_of([g])
            nm = f'{name.upper()}_{slot}'
            out.append(anim.pal_c(f'HDP_A_{nm}', pal))
            out.append(f'static const char* const HDR_A_{nm}[] = {{')
            for j in range(y0, y1): out.append('    "' + ''.join('.' if g[j][i] is None else anim.ALPHABET[pal.index(g[j][i])] for i in range(x0, x1)) + '",')
            out.append('};')
            out.append(f'static const HDSprite HD_A_{nm} = {{{x1 - x0}, {y1 - y0}, HDP_A_{nm}, HDR_A_{nm}}};')
            pa, pb = pts[a], pts[bb]
            cells.append(f'{{&HD_A_{nm}, {pa[0] - x0:.2f}f, {pa[1] - y0:.2f}f, {pb[0] - x0:.2f}f, {pb[1] - y0:.2f}f}}')
        out.append(f'static const RigSpec RIG_A_{name.upper()} = {{RK_BAT, {{{", ".join(cells)}}}, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, RW_NONE, 0}};')
        out.append('')
    fw, fh, gx, gnd = b['canvas']
    anim.emit_sheet(out, name.upper(), None, frames, gx, gnd)


def preview_beasts(outdir, names=None):
    from PIL import Image
    os.makedirs(outdir, exist_ok=True)
    for name in (names or list(BEASTS)):
        cells = []
        for clip, poses in beast_clips(name):
            for P in poses:
                rgb, tag, op, J, d = render_beast(name, P)
                im = Image.new('RGB', (op.shape[1], op.shape[0]), (24, 18, 36))
                p = im.load()
                for y in range(op.shape[0]):
                    for x in range(op.shape[1]):
                        if op[y, x]: p[x, y] = tuple(int(v) for v in rgb[y, x])
                cells.append(im)
        fw, fh = cells[0].size
        cols = max(2, 1800 // (fw * 3))
        rows = (len(cells) + cols - 1) // cols
        sheet = Image.new('RGB', (cols * fw, rows * fh), (24, 18, 36))
        for k, im in enumerate(cells): sheet.paste(im, ((k % cols) * fw, (k // cols) * fh))
        sheet.resize((sheet.width * 3, sheet.height * 3), Image.NEAREST).save(f'{outdir}/{name}_sheet.png')
        print(name, len(cells), 'frames')


FOES = list(CANVAS) + list(BEASTS)


def emit_any(out, name):
    if name in BEASTS: emit_beast(out, name)
    else: emit_foe(out, name)


if __name__ == '__main__':
    names = sys.argv[3:] or None
    if sys.argv[1] == 'preview':
        preview([n for n in (names or CANVAS) if n in CANVAS] and sys.argv[2] or sys.argv[2], [n for n in (names or CANVAS) if n in CANVAS] or []) if (not names or any(n in CANVAS for n in names)) else None
        preview_beasts(sys.argv[2], [n for n in (names or BEASTS) if n in BEASTS] if names else None) if (not names or any(n in BEASTS for n in names)) else None

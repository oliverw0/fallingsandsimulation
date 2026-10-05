# The user's own sprites (the hero in the torn ochre robe, the white dire wolf), halved from their drawings and
# animated by moving their own pixels: legs swung by shearing, bobs and squashes by shifting rows, rolls by turning
# the grid. No skeleton while alive - joints are the rest pose, used only for corpses, wounds and held weapons.
#   python tools/userart.py emit > sprites_anim.h      (every other creature still comes from tools/figures.py / anim.py)
#   python tools/userart.py preview <dir>
import math, sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import anim
from anim import CLIPS, JOINTS, ALPHABET, pal_c, crop, emit_sheet, palette_of

# 'land': a landing squash (rig.cpp picks it while the squash eases back); the rest are the hero's committed swings
# and the two ends of the roll (rig.cpp picks them by phase)
for _c in ('land', 'slash', 'chop', 'thrust', 'rollin', 'rollout', 'tread'):
    if _c not in CLIPS: CLIPS.append(_c)


def hexc(h): return tuple(int(h[i:i + 2], 16) for i in (1, 3, 5))


# ---------------------------------------------------------------- the art (one letter per pixel, '.' clear)
HERO_PAL = {'0': '#2c2132', '1': '#68524b', '2': '#80807e', '4': '#a58f63', '5': '#a8a397', '6': '#ccb481', '7': '#f5c35a',
            '8': '#e7c277', 'a': '#f1cd7b', 'b': '#f0f0ea'}
HERO_METAL = set('25')  # the helm: takes the armour's metal colour in game
HERO = '''
...........2225.......
..........255555......
.........25555555.....
.........255552555....
........2555552255....
.........555552255....
.........555522222....
.........555225222....
........6255555552....
........62bb252bb2....
........2aa22225a4....
........26aaa5aa44....
.........644aaaa46....
.........6464aa44.....
......2666a444446.....
.....66aaa644444666...
...7a66aaaaa644aaa66a.
...77727aa6aa44aa6a66.
..7777777aaa6aaa66a66a
.77777777a6aa66a66a66a
.7777747777aa66aa7414a
.777777477a7744777446a
777477447777777477746a
777444177777777747116a
777441177477777744116a
77771117a744777744146a
77474117a777477774446a
77444417777774477144aa
.7777417711111111114aa
.777777111111111111aa0
..77700001111111110000
...7000001111111170000
...000000111117777400.
....0000111117777447..
....0000174777704747..
.....007744777047747..
....7777477777477477..
....77747777047774777.
....7747770477777477..
....77777447777777008.
...40000007700800000..
....8770000000000000..
......000000.000000...
......00000..000000...
......00000..000000...
.....00000...000000...
.....00000...000000...
.....0000000.0000000..
....00000000.000000000
....000000000000000000
.....00000............
'''
WOLF_PAL = {'0': '#08080c', '1': '#5f4636', '2': '#8e3b45', '3': '#967460', '4': '#9e9e97', '5': '#bbbbb0', '6': '#d1d1c5', '7': '#fffff0'}
WOLF = '''
...........................................66....6......
..........................................667...674.....
..........................................674..674......
.........................................6674.6674......
.77......................................6776.6674......
677......................................677777777.7....
777.....................................677777777777....
766............................6.666.....777777720277...
77666...............66.6.......6666766777777777777777713
777676.........77766676676676666766777677777777777777711
776767666....7777767776777667777777777666777777777777776
.7666677667777777666677777777777777777667677777667776667
.776667777777777766677776677777667677777767777776666666.
..7767777777777767666666677776666667777667676776767666..
...77777777777766676667666677777667777776677666676......
......7777.777777776766767776777777677777676666666......
..........777777777777766666777676667777777667776.......
...........7777777777776776666766767777777777776........
...........777777777777777777777777766777777777.........
..............77777777777767677776676676666666..........
..............77777777557677677676766767666556..........
..............77777775555556666557777777755566..........
.............766777776555....66..766777755566...........
..............77677775556........6.7777755666...........
..............766777666666.......767777766666...........
.............6667777666666.......6777777.6666...........
.............6677776666666........677777.66666..........
.............777776666666.........677777..6666..........
............767777.666666.........766777..66666.........
............77777..66666...........67777...66666........
............67777...6666..........6667777..66666........
.............7777...66666..........6676777..6666........
.............777777..6666666..........77777..6666.......
...............77776..666.66...........7766...66666.....
...............77766....66666..........7766....66.......
'''


def load(text, pal, metal=()):
    rows = text.strip('\n').split('\n')
    return {(x, y): (hexc(pal[ch]), 1 if ch in metal else 0) for y, r in enumerate(rows) for x, ch in enumerate(r) if ch != '.'}


# ---------------------------------------------------------------- pixel moves (a layer = {(x, y): colour})
def move(L, dx=0, dy=0): return {(x + dx, y + dy): c for (x, y), c in L.items()}


def pick(L, f): return {p: c for p, c in L.items() if f(*p)}


def shear(L, top, length, dx, lift=0):
    """Swing a hanging part: row `top` stays put, the bottom row moves `dx` across; the whole thing rises `lift`."""
    out = {}
    for (x, y), c in L.items():
        k = max(0, min(1, (y - top) / max(1, length)))
        out[(x + round(dx * k), y - lift)] = c
    return out


def stack(*layers):
    out = {}
    for L in layers: out.update(L)
    return out


def rot(L, quarter, cx, by):
    """Turn a layer a quarter-turn at a time about (cx, centre), keeping its bottom on row `by`."""
    out = dict(L)
    for _ in range(quarter % 4): out = {(-y, x): c for (x, y), c in out.items()}  # 90 degrees clockwise
    if not out: return out
    xs = [p[0] for p in out]; ys = [p[1] for p in out]
    mx, my = (min(xs) + max(xs)) // 2, max(ys)
    return {(x - mx + cx, y - my + by): c for (x, y), c in out.items()}


# ---------------------------------------------------------------- the hero
HW, HH, HOX, HOY = 64, 60, 21, 6           # canvas, and where the drawing's top-left sits on it
HAX, HAY = HOX + 11.5, HOY + 51            # the anchor: between the feet, on the ground


def hero_layers():
    A = move(load(HERO, HERO_PAL, HERO_METAL), HOX, HOY)
    arm = pick(A, lambda x, y: x - HOX >= 18 and 15 <= y - HOY <= 32)
    legs = pick(A, lambda x, y: y - HOY >= 42)
    body = {p: c for p, c in A.items() if p not in arm and p not in legs}
    legL = pick(legs, lambda x, y: x - HOX <= 11)
    legR = {p: c for p, c in legs.items() if p not in legL}
    return body, arm, legL, legR


def hero_frames():
    """(clip, body layer, arm layer) per frame."""
    body, arm, legL, legR = hero_layers()
    top = HOY + 42
    F = []

    def pose(clip, dl=0, dr=0, ll=0, lr=0, bob=0, hem=0, b=body, a=arm):
        legs = stack(shear(legL, top, 8, dl, ll), shear(legR, top, 8, dr, lr))
        bd = move(b, 0, bob)
        if hem: bd = {(x + (hem if y - HOY >= 36 + bob else 0), y): c for (x, y), c in bd.items()}  # the hem swings
        F.append((clip, stack(legs, bd), move(a, 0, bob)))

    pose('idle'); pose('idle', bob=1)
    for dl, dr, ll, lr, bob, hem in ((3, -3, 0, 0, 0, 0), (1, -2, 0, 2, 1, 1), (-1, 0, 0, 3, 1, 1),
                                     (-3, 3, 0, 0, 0, 0), (-2, 1, 2, 0, 1, -1), (0, -1, 3, 0, 1, -1)):
        pose('walk', dl, dr, ll, lr, bob, hem)
    pose('jump', 1, -1, 3, 2, hem=-1)
    pose('fall', -2, 2, 0, 0, hem=1)
    # landing: the robe and all above it sink two rows into the knees
    sq = {(x, y + (2 if y - HOY < 30 else 1)): c for (x, y), c in body.items()}
    pose('land', -1, 1, b=sq, a=move(arm, 0, 2))
    # crouching: everything but the boots sinks eight rows
    cb, ca = move(body, 0, 8), move(arm, 0, 8)
    boots = pick(stack(legL, legR), lambda x, y: y - HOY >= 46)
    F.append(('crouch', stack(boots, cb), ca))
    for d in (1, 0, -1, 0):
        F.append(('crouchwalk', stack(move(pick(legL, lambda x, y: y - HOY >= 46), d, 0), move(pick(legR, lambda x, y: y - HOY >= 46), -d, 0), cb), ca))
    # the roll: crouched, turning a quarter a frame
    for q in range(4):
        F.append(('roll', rot(stack(boots, cb, ca), q, round(HAX), HAY - 1), {}))  # the arm turns with the body
    # crawling: lying along the ground, head forward, the legs working
    for d in (0, 1, 0, -1):
        lying = stack(shear(legL, top, 8, d), shear(legR, top, 8, -d), body, arm)
        F.append(('crawl', rot(lying, 1, round(HAX) + 4, HAY - 1), {}))
    return F


# where the hero's joints sit (drawing pixels): the near shoulder anchors the weapon arm; neck/hip place a stowed weapon
HERO_J = {'neck': (11, 15), 'pel': (11, 30), 'headb': (12, 13), 'headt': (12, 0), 'shF': (4, 17), 'elF': (3, 24), 'haF': (5, 30),
          'shN': (18, 17), 'elN': (19, 24), 'haN': (19, 30), 'hipF': (8, 40), 'knF': (8, 45), 'ftF': (8, 50),
          'hipN': (15, 40), 'knN': (16, 45), 'ftN': (16, 50), 'wb': (19, 30), 'wt': (19, 20)}


def joints(J, ox, oy): return [(J[k][0] + ox, J[k][1] + oy) for k in JOINTS]


def aim_arm(a):
    """The near arm held straight out at angle a (0 down, turning forward): the sleeve, its crease, the dark glove."""
    sh = (HOX + 18.5, HOY + 17.5)
    d = (math.sin(a), math.cos(a))
    n = (-d[1], d[0])
    out = {}
    for y in range(int(sh[1]) - 16, int(sh[1]) + 17):
        for x in range(int(sh[0]) - 16, int(sh[0]) + 17):
            vx, vy = x + 0.5 - sh[0], y + 0.5 - sh[1]
            t = vx * d[0] + vy * d[1]; s = vx * n[0] + vy * n[1]
            if -1 <= t <= 10.5 and abs(s) <= 2.1:
                lit = -(n[0] * 0.7 + n[1] * 0.7) * (1 if s > 0 else -1) > 0  # the side facing the upper left
                ch = 'a' if lit and abs(s) > 1.0 else ('4' if abs(s) > 1.2 else '7')
                out[(x, y)] = (hexc(HERO_PAL[ch]), 0)
            elif 10.5 < t <= 13.5 and abs(s) <= 1.9:
                out[(x, y)] = (hexc(HERO_PAL['0' if t < 13 else '1']), 0)
    return out


# ---------------------------------------------------------------- the dire wolf
WW, WH, WOX, WOY = 72, 46, 8, 7
WAX, WAY = WOX + 30, WOY + 35


def wolf_layers():
    A = move(load(WOLF, WOLF_PAL), WOX, WOY)
    white = hexc(WOLF_PAL['7'])
    legs = pick(A, lambda x, y: y - WOY >= 20)
    body = {p: c for p, c in A.items() if p not in legs}

    def leg(p):
        x = p[0] - WOX; grey = legs[p][0] != white
        if x < 30: return 'hindF' if grey and x >= 19 else 'hindN'
        return 'frontF' if grey and x >= 41 else 'frontN'
    L = {k: {} for k in ('hindN', 'hindF', 'frontN', 'frontF')}
    for p, c in legs.items(): L[leg(p)][p] = c
    tail = pick(body, lambda x, y: x - WOX <= 10)
    return body, tail, L


def wolf_frames():
    body, tail, L = wolf_layers()
    top, n = WOY + 20, 14
    F = []

    def pose(clip, hn=0, hf=0, fn=0, ff=0, lift=(0, 0, 0, 0), bob=0, wag=0, lean=0):
        trunk = {p: c for p, c in body.items() if p not in tail}
        if lean:  # the head and shoulders dip (or rise) - columns ahead of the shoulders move most
            trunk = {(x, y + (round(lean * (x - WOX - 30) / 20) if x - WOX > 30 else 0)): c for (x, y), c in trunk.items()}
        up = max(0, -bob)  # off the ground: the legs go up with the body
        legs = stack(shear(L['hindF'], top, n, hf, lift[1] + up), shear(L['frontF'], top, n, ff, lift[3] + up),
                     shear(L['hindN'], top, n, hn, lift[0] + up), shear(L['frontN'], top, n, fn, lift[2] + up))
        F.append((clip, stack(legs, move(stack(move(tail, 0, -wag), trunk), 0, bob))))

    pose('idle'); pose('idle', bob=1, wag=1)
    for k in range(6):  # a trot: the diagonal pairs swing together
        s = [3, 2, 0, -3, -2, 0][k]
        up = [0, 1, 2, 0, 1, 2][k]
        pose('walk', hn=s, ff=s, hf=-s, fn=-s, lift=(up if k < 3 else 0, up if k >= 3 else 0, up if k >= 3 else 0, up if k < 3 else 0),
             bob=1 if k in (1, 4) else 0)
    pose('windup', hn=-2, hf=-2, fn=2, ff=2, bob=2, lean=2); pose('windup', hn=-3, hf=-3, fn=3, ff=3, bob=3, lean=3)
    pose('strike', hn=-5, hf=-4, fn=5, ff=6, lift=(1, 1, 3, 3), bob=-2)
    pose('strike', hn=-6, hf=-5, fn=6, ff=7, lift=(2, 2, 4, 4), bob=-3)
    pose('strike', hn=-3, hf=-2, fn=4, ff=4, lift=(1, 1, 2, 2), bob=-1)
    pose('recover', hn=-2, hf=-1, fn=2, ff=1, bob=2, lean=1); pose('recover', bob=1)
    pose('hurt', hn=2, hf=2, fn=-1, ff=-1, bob=1, lean=2); pose('hurt', hn=1, hf=1, lean=1)
    pose('land', hn=-2, hf=-2, fn=2, ff=2, bob=2, lean=1)
    return F


WOLF_J = {'neck': (37, 15), 'pel': (17, 15), 'headb': (42, 11), 'headt': (54, 8), 'shN': (36, 20), 'elN': (36, 27), 'haN': (37, 34),
          'shF': (43, 20), 'elF': (44, 27), 'haF': (46, 34), 'hipN': (16, 20), 'knN': (15, 27), 'ftN': (16, 34),
          'hipF': (23, 20), 'knF': (23, 27), 'ftF': (24, 34), 'wb': (10, 11), 'wt': (1, 5)}


def wolf_parts():
    """Its corpse falls apart into these, cut from the standing drawing: slot -> (pixels, joint a, joint b)."""
    body, tail, L = wolf_layers()
    d = lambda x, y: (x - WOX, y - WOY)
    head = pick(body, lambda x, y: d(x, y)[0] >= 41 and d(x, y)[1] <= 17)
    torso = {p: c for p, c in body.items() if p not in head and p not in tail}
    return {'HEAD': (head, 'headb', 'headt'), 'TORSO': (torso, 'neck', 'pel'), 'TAIL': (tail, 'wb', 'wt'),
            'UARM': (pick(L['frontN'], lambda x, y: d(x, y)[1] < 27), 'shN', 'elN'), 'FARM': (pick(L['frontN'], lambda x, y: d(x, y)[1] >= 27), 'elN', 'haN'),
            'THIGH': (pick(L['hindN'], lambda x, y: d(x, y)[1] < 27), 'hipN', 'knN'), 'SHIN': (pick(L['hindN'], lambda x, y: d(x, y)[1] >= 27), 'knN', 'ftN')}


# ---------------------------------------------------------------- output
def grid(L, w, h):
    g = [[None] * w for _ in range(h)]
    for (x, y), c in L.items():
        if 0 <= x < w and 0 <= y < h: g[y][x] = c
    return g


def emit_parts(out, name, parts, J, ox, oy, kind):
    slots = ('HEAD', 'TORSO', 'UARM', 'FARM', 'THIGH', 'SHIN', 'WEAPON', 'SHIELD', 'WING', 'TAIL')
    cells = []
    for slot in slots:
        if slot not in parts or not parts[slot][0]: cells.append('{nullptr, 0, 0, 0, 0}'); continue
        L, a, b = parts[slot]
        xs = [p[0] for p in L]; ys = [p[1] for p in L]
        x0, y0 = min(xs), min(ys)
        g = grid(move(L, -x0, -y0), max(xs) - x0 + 1, max(ys) - y0 + 1)
        pal = palette_of([g]); nm = f'{name}_{slot}'
        out.append(pal_c(f'HDP_A_{nm}', pal))
        out.append(f'static const char* const HDR_A_{nm}[] = {{')
        for row in g: out.append('    "' + ''.join('.' if p is None else ALPHABET[pal.index(p)] for p in row) + '",')
        out.append('};')
        out.append(f'static const HDSprite HD_A_{nm} = {{{len(g[0])}, {len(g)}, HDP_A_{nm}, HDR_A_{nm}}};')
        pa, pb = J[a], J[b]
        cells.append(f'{{&HD_A_{nm}, {pa[0] + ox - x0 + 0.5:.2f}f, {pa[1] + oy - y0 + 0.5:.2f}f, {pb[0] + ox - x0 + 0.5:.2f}f, {pb[1] + oy - y0 + 0.5:.2f}f}}')
    out.append(f'static const RigSpec RIG_A_{name} = {{{kind}, {{{", ".join(cells)}}}, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, RW_NONE, 0}};')
    out.append('')


def emit_hero(out):
    F = hero_frames()
    js = joints(HERO_J, HOX + 0.5, HOY + 0.5)
    emit_sheet(out, 'HERO_WOOL', None, [(CLIPS.index(c), grid(b, HW, HH), js) for c, b, a in F], HAX, HAY)
    emit_sheet(out, 'HERO_WOOL_ARM', None, [(CLIPS.index(c), grid(a, HW, HH), js) for c, b, a in F], HAX, HAY)
    sh = (HOX + 18.5, HOY + 17.5)
    emit_sheet(out, 'HERO_WOOL_AIM', None, [(0, grid(aim_arm(k / 16 * 2 * math.pi), HW, HH), [sh] * len(JOINTS)) for k in range(16)], sh[0], sh[1])
    for look in ('LEATHER', 'MAIL', 'LAMELLAR', 'SCALE'):  # one robe whatever the armour: only the helm takes its metal
        for part in ('', '_ARM', '_AIM'): out.append(f'static const AnimSheet& ANIM_HERO_{look}{part} = ANIM_HERO_WOOL{part};')
    out.append('')


def emit_wolf(out):
    emit_parts(out, 'WOLF', wolf_parts(), WOLF_J, WOX, WOY, 'RK_QUAD')
    js = joints(WOLF_J, WOX + 0.5, WOY + 0.5)
    emit_sheet(out, 'WOLF', None, [(CLIPS.index(c), grid(L, WW, WH), js) for c, L in wolf_frames()], WAX, WAY)


def emit():
    import figures
    figures.install()
    out = ['// Generated by tools/userart.py (with tools/figures.py, tools/anim.py) - edit there, then: python tools/userart.py emit > sprites_anim.h',
           '#pragma once', '#include "sprites_hd.h"', '',
           '// Frame-by-frame character animation. A sheet holds every frame of a creature, stacked top to bottom, each fw x fh',
           '// pixels (half a world unit each), drawn facing right; (ax, ay) is the point between its feet on the ground. A pixel',
           "// is an index into HD_ALPHABET ('.' clear) and its palette, alpha 254/253 as in sprites_hd.h. joints: per frame, the",
           '// rig joints (rig.cpp J_*) in pixels from the anchor - where wounds sit, and where its corpse starts from.',
           'enum AnimClip { ' + ', '.join('AC_' + c.upper() for c in CLIPS) + ', AC_COUNT };',
           'struct AnimSheet { int fw, fh; float ax, ay; int frames; const Color* pal; const char* px; unsigned char clip[AC_COUNT][2]; const float* joints; };', '']
    for name, mk in anim.SPECS.items():
        if name == 'hero': anim.emit_hero(out, mk()); continue  # the hero is painted in figures.py's style now (user, 2026-10-04)
        import foes3d_run
        if name in foes3d_run.FOES:   # the starter foes are built in 3D (tools/foes3d*.py)
            foes3d_run.emit_any(out, name)
            continue
        if name == 'wolf':   # the dire wolf is now built in 3D, shaggy (tools/wolf3d.py)
            import wolf3d
            wolf3d.emit(out)
            continue
        S = mk()
        frames = []
        for an, fr in S.anims.items():
            for P in fr:
                (gr, _), J = anim.bake_frame(S, P)
                frames.append((CLIPS.index(an), gr, anim.joints_of(S, J, P)))
        if S.kind != 'none': anim.emit_parts(out, name.upper(), S)
        emit_sheet(out, name.upper(), S, frames, S.cx, S.gy)
    return '\n'.join(out) + '\n'


def preview(outdir):
    from PIL import Image
    os.makedirs(outdir, exist_ok=True)
    for name, frames, w, h in (('hero', [(c, stack(b, a)) for c, b, a in hero_frames()], HW, HH), ('wolf', wolf_frames(), WW, WH)):
        rows = {}
        for c, L in frames: rows.setdefault(c, []).append(L)
        Z = 4
        sheet = Image.new('RGB', (max(len(v) for v in rows.values()) * w * Z, len(rows) * h * Z))
        gif = []
        for r, (c, Ls) in enumerate(rows.items()):
            for k, L in enumerate(Ls):
                im = Image.new('RGB', (w, h), (0, 0, 0) if (r + k) % 2 else (14, 14, 18))
                for (x, y), col in L.items():
                    if 0 <= x < w and 0 <= y < h: im.putpixel((x, y), col[0])
                sheet.paste(im.resize((w * Z, h * Z), Image.NEAREST), (k * w * Z, r * h * Z))
                if c in ('idle', 'walk', 'windup', 'strike', 'recover', 'jump', 'fall', 'land'):
                    gif += [im.resize((w * 5, h * 5), Image.NEAREST)] * (2 if c == 'walk' else 4)
        sheet.save(f'{outdir}/{name}_sheet.png')
        gif[0].save(f'{outdir}/{name}.gif', save_all=True, append_images=gif[1:], duration=60, loop=0)
        print(name, 'clips:', {c: len(v) for c, v in rows.items()})


if __name__ == '__main__':
    if sys.argv[1] == 'emit': sys.stdout.write(emit())
    elif sys.argv[1] == 'preview': preview(sys.argv[2])

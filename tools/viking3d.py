# The Viking's body as a real 3D model (Dead Cells' pipeline): tools/viking3d_blender.py builds one continuous skinned mesh in
# Blender, the clips' joints pose its armature, and Blender renders each frame's G-buffer (depth, normal, material, rest-pose
# coordinates) at sprite size with no anti-aliasing. Here that G-buffer is poured into a viking.Canvas, so the palette
# shading, outline, depth lines, weapon and smear are exactly the 2.5D pipeline's. Frames are cached by pose in .cache/viking3d.
#
#   needs: blender on PATH; numpy, Pillow, OpenEXR in the Python that runs the tools
import hashlib, json, math, os, subprocess, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from viking_char import *

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CACHE = os.path.join(ROOT, '.cache', 'viking3d')
SCRIPT = os.path.join(ROOT, 'tools', 'viking3d_blender.py')
MATS3 = ['mustard', 'trim', 'belt', 'trews', 'leather', 'skin', 'hair', 'helm', 'boot', 'socket']
FAR = {6, 7, 8, 12, 13, 14}           # body regions on the far side: shaded a band darker
CAMY = 300.0
VERSION = 8                           # bump when the model changes, so cached frames are re-rendered


def export(P):
    """The pose's joints in local space (x forward, y down) plus depth (+ toward the viewer), the figure's rot applied."""
    hip, lean, C_, N, hd, sh, legs, arms = joints(P)
    J = dict(hip=(hip, 0), chest=(C_, 0), neck=(N, 0), head=(hd, 0))
    for s, S_, dep in (('n', 'N', 1), ('f', 'F', -1)):
        a, e, w = arms[s]
        wz = 10.0 if s == 'n' else P['far_arm_z']
        J['sh' + S_] = (a, 9.5 * dep); J['elb' + S_] = (e, (9.5 * dep + wz) / 2); J['wri' + S_] = (w, wz)
        fw = (w - e) / (np.hypot(*(w - e)) + 1e-9)
        J['fist' + S_] = (w + fw * 2.6, wz)
        h_, k, an = legs[s]
        td = V(*P['toe']) if P['toe'] != (0.0, 0.0) else V(6.5, 1.0)
        J['hip' + S_] = (h_, 5.0 * dep); J['knee' + S_] = (k, 5.0 * dep); J['ank' + S_] = (an, 5.0 * dep); J['toe' + S_] = (an + td, 5.0 * dep)
    # the robe's panels: the front is pushed out by whichever knee leads, the back by whichever trails (angles: 0 forward, 90 down)
    ka = [math.degrees(math.atan2(legs[s][1][1] - hip[1], legs[s][1][0] - hip[0])) for s in 'nf']
    J['skirtF'] = (hip + 16 * dirv(min(90.0, max(5.0, min(ka) - 6))), 0)
    J['skirt'] = (hip + 16 * dirv(max(90.0, min(160.0, max(ka) + 4 - 0.6 * P['skirt']))), 0)
    rot, pv = math.radians(P['rot']), V(*P['pivot'])
    cs, sn = math.cos(rot), math.sin(rot)
    out = {}
    for k_, (p, z) in J.items():
        q = V(*p) - pv
        out[k_] = [round(float(pv[0] + q[0] * cs - q[1] * sn), 3), round(float(pv[1] + q[0] * sn + q[1] * cs), 3), round(float(z), 3)]
    return out


def rest():
    sh_n = joints(pose())[5]
    return export(pose(hn=(float(sh_n['n'][0]) + 2.5, -26.3), hf=(float(sh_n['f'][0]) + 1.0, -26.3), fn=(1.5, 0.5), ff=(-1.5, 0.5)))


def key(P):
    return hashlib.sha1(json.dumps([VERSION, export(P)], sort_keys=True).encode()).hexdigest()[:16]


def cache_dir(S, FW, FH, GX, GND): return os.path.join(CACHE, f'{S:g}_{FW}x{FH}_{GX:g}_{GND:g}')


def ensure(poses, S, FW, FH, GX, GND):
    """Render every pose not yet in the cache (one Blender run)."""
    out = cache_dir(S, FW, FH, GX, GND)
    os.makedirs(out, exist_ok=True)
    jobs, seen = [], set()
    for P in poses:
        k = key(P)
        if k in seen or os.path.exists(os.path.join(out, k + '.exr')): continue
        seen.add(k); jobs.append(dict(key=k, j=export(P)))
    if not jobs: return
    jf = os.path.join(out, 'jobs.json')
    json.dump(dict(S=S, FW=FW, FH=FH, GX=GX, GND=GND, CAMY=CAMY, rest=rest(), jobs=jobs), open(jf, 'w'))
    print(f'viking3d: rendering {len(jobs)} frames in Blender', file=sys.stderr)
    with open(os.path.join(out, 'blender.log'), 'w') as log:
        subprocess.run(['blender', '-b', '--factory-startup', '--python-exit-code', '1', '-P', SCRIPT, '--', jf, out], stdout=log, stderr=subprocess.STDOUT, check=True)


def read(path):
    import OpenEXR
    with OpenEXR.File(path) as f:
        ch = {n: np.asarray(c.pixels, float) for part in f.parts for n, c in part.channels.items()}
    def layer(L):
        g = lambda n: ch[L + '.' + n]
        return dict(z=g('Depth.Z'), n=np.stack([g('Normal.X'), g('Normal.Y'), g('Normal.Z')], -1),
                    idx=np.rint(g('Material Index.X')).astype(int), a=g('Combined')[..., 3], rest=g('rest')[..., :3])
    G, R = layer('ViewLayer'), layer('robe')
    # the robe lies over the legs whatever their depth (it is worn over them); only nearer arms, hands and the torso cover it
    rm = R['a'] > 0.5
    legs = (G['idx'] % 16 >= 9) & (G['idx'] % 16 <= 14)
    take = rm & ((G['a'] <= 0.5) | legs | (R['z'] <= G['z']))
    for k in G: G[k] = np.where(take[..., None] if G[k].ndim == 3 else take, R[k], G[k])
    return G


def body(cv, d, P, look=0):
    """Fill Canvas cv with the 3D body for pose P (its G-buffer must be cached: see ensure)."""
    G = read(os.path.join(cache_dir(cv.S, cv.w, cv.h, d.ox, cv.ground), key(P) + '.exr'))
    m = G['a'] > 0.5
    reg, mi = G['idx'] % 16, G['idx'] // 16
    S = cv.S
    cv.z[m] = ((CAMY - G['z']) * S)[m]
    n = G['n']
    cv.n[m] = np.stack([n[..., 0], -n[..., 2], -n[..., 1]], -1)[m]
    names = np.array(MATS3)[np.clip(mi, 0, len(MATS3) - 1)]
    mat = np.array([MI['boot' if s == 'socket' else s] for s in MATS3])[np.clip(mi, 0, len(MATS3) - 1)]
    aux = np.where(names == 'socket', -4, 0)
    if look:
        cloth = (names == 'mustard') | (names == 'trim')
        X, Y = G['rest'][..., 0] * 60 * S, (1 - G['rest'][..., 2]) * 60 * S
        a2, _ = armour_tex(look)(None, None, X, Y)
        mat = np.where(cloth, MI['armour'], mat)
        aux = np.where(cloth, a2, aux)
    cv.mat[m] = mat[m]; cv.aux[m] = aux[m]
    cv.dark[m] = np.isin(reg, list(FAR)).astype(int)[m]
    cv.obj[m] = (1000 + reg)[m]
    cv.nobj = max(cv.nobj, 2000)
    if P['eye']:
        hd = joints(P)[4]
        ex, ey = d.P(hd + V(6.3, 2.0))
        cv.eye[(int(ex), int(ey))] = EYE_CORE

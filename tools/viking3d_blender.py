# Runs inside Blender (called by tools/viking3d.py):  blender -b --factory-startup -P tools/viking3d_blender.py -- jobs.json outdir
# Builds the Viking as one continuous skinned body (a skin modifier over the skeleton, smoothed) plus the tunic skirt, belt,
# helm, beard and hair, all deformed by one armature. Each job poses the armature from the clip's joints and renders a
# G-buffer at sprite size with no anti-aliasing: depth, normal, material index (material * 16 + body region) and an AOV of
# rest-pose coordinates (for textures pinned to the body). The palette shading happens back in Python (viking.py).
#
# Coordinates: the clips' local space is x forward, y down, depth toward the viewer. Blender: X = x, Z = -y, Y = -depth;
# the orthographic camera looks along +Y.
import bpy, bmesh, json, math, os, sys
from mathutils import Vector, Matrix

args = sys.argv[sys.argv.index('--') + 1:]
JOBS = json.load(open(args[0]))
OUT = args[1]

MATS3 = ['mustard', 'trim', 'belt', 'trews', 'leather', 'skin', 'hair', 'helm', 'boot', 'socket']
REG = dict(spine=0, neck=1, head=2, uarmN=3, farmN=4, handN=5, uarmF=6, farmF=7, handF=8,
           thighN=9, shinN=10, footN=11, thighF=12, shinF=13, footF=14, gear=15)
BONES = dict(spine=('hip', 'chest'), neck=('chest', 'neck'), head=('neck', 'head'), claN=('chest', 'shN'), claF=('chest', 'shF'),
             uarmN=('shN', 'elbN'), farmN=('elbN', 'wriN'), handN=('wriN', 'fistN'), uarmF=('shF', 'elbF'), farmF=('elbF', 'wriF'), handF=('wriF', 'fistF'),
             thighN=('hipN', 'kneeN'), shinN=('kneeN', 'ankN'), footN=('ankN', 'toeN'), thighF=('hipF', 'kneeF'), shinF=('kneeF', 'ankF'), footF=('ankF', 'toeF'),
             skirt=('hip', 'skirt'), skirtF=('hip', 'skirtF'))


def B(p): return Vector((p[0], -p[2], -p[1]))          # clip (x, y down, depth) -> Blender


R = {k: B(v) for k, v in JOBS['rest'].items()}
UP = Vector((0, 0, 1))


# ---------------------------------------------------------------- scene
for o in list(bpy.data.objects): bpy.data.objects.remove(o)
sc = bpy.context.scene
col = bpy.data.collections.new('base'); sc.collection.children.link(col)     # the body and its gear
robe = bpy.data.collections.new('robe'); sc.collection.children.link(robe)   # the tunic's skirt: rendered on its own layer, laid over the legs

def mat_slot(obj, mi, region):
    """The material for (material, region): one Blender material per pair, its pass index encoding both."""
    key = f'{MATS3[mi]}.{region}'
    m = bpy.data.materials.get(key)
    if m is None:
        m = bpy.data.materials.new(key)
        m.pass_index = mi * 16 + region
        nt = m.node_tree
        tc = nt.nodes.new('ShaderNodeTexCoord')
        aov = nt.nodes.new('ShaderNodeOutputAOV'); aov.aov_name = 'rest'
        nt.links.new(tc.outputs['Generated'], aov.inputs['Color'])
    names = [s.material.name if s.material else '' for s in obj.material_slots]
    if key in names: return names.index(key)
    obj.data.materials.append(m)
    return len(obj.data.materials) - 1


# ---------------------------------------------------------------- armature (parentless bones: each one is posed directly)
def frame_of(h, t):
    y = (t - h).normalized()
    ref = Vector((0, 1, 0)) if abs(y.y) < 0.9 else Vector((1, 0, 0))
    z = (ref - y * ref.dot(y)).normalized()
    x = y.cross(z)
    return x, y, z


arm_data = bpy.data.armatures.new('rig')
rig = bpy.data.objects.new('rig', arm_data)
col.objects.link(rig)
bpy.context.view_layer.objects.active = rig
bpy.ops.object.mode_set(mode='EDIT')
for bn, (a, b) in BONES.items():
    eb = arm_data.edit_bones.new(bn)
    eb.head, eb.tail = R[a], R[b]
    eb.align_roll(frame_of(R[a], R[b])[2])
bpy.ops.object.mode_set(mode='OBJECT')
REST_LEN = {bn: (R[b] - R[a]).length for bn, (a, b) in BONES.items()}


def pose(J):
    for bn, (a, b) in BONES.items():
        h, t = J[a], J[b]
        x, y, z = frame_of(h, t)
        k = (t - h).length / REST_LEN[bn]
        M = Matrix(((x.x, y.x * k, z.x, h.x), (x.y, y.y * k, z.y, h.y), (x.z, y.z * k, z.z, h.z), (0, 0, 0, 1)))
        rig.pose.bones[bn].matrix = M


# ---------------------------------------------------------------- the body: one skin-modifier mesh over the skeleton
def seg_t(p, a, b):
    ab = b - a
    t = max(0.0, min(1.0, (p - a).dot(ab) / max(ab.length_squared, 1e-9)))
    return t, (p - (a + ab * t)).length


def make_skin(name, nodes, edges, subdiv=2):
    """nodes: [(position, (rx, ry))] -> an applied, smooth-shaded mesh object."""
    me = bpy.data.meshes.new(name)
    me.from_pydata([p for p, r in nodes], edges, [])
    ob = bpy.data.objects.new(name, me)
    col.objects.link(ob)
    sk = ob.modifiers.new('skin', 'SKIN'); sk.use_smooth_shade = True
    for i, (p, r) in enumerate(nodes): me.skin_vertices[0].data[i].radius = r
    me.skin_vertices[0].data[0].use_root = True
    ss = ob.modifiers.new('sub', 'SUBSURF'); ss.levels = ss.render_levels = subdiv
    dg = bpy.context.evaluated_depsgraph_get()
    me2 = bpy.data.meshes.new_from_object(ob.evaluated_get(dg))
    ob.modifiers.clear(); ob.data = me2
    for p in me2.polygons: p.use_smooth = True
    return ob


hip, chest, neck, head = R['hip'], R['chest'], R['neck'], R['head']
sp = (chest - hip).normalized()
body_nodes, body_edges = [], []
def node(p, r, link=None):
    body_nodes.append((p, r))
    i = len(body_nodes) - 1
    if link is not None: body_edges.append((link, i))
    return i
i_pel = node(hip + sp * 1.0, (9.4, 10.6))
i_bel = node(hip + sp * 9.0, (8.6, 10.0), i_pel)
i_chs = node(hip + sp * 17.0, (9.6, 11.2), i_bel)
i_top = node(chest - sp * 1.0, (7.0, 9.0), i_chs)
i_nk = node(neck + (head - neck) * 0.3, (4.4, 4.2), i_top)
i_hd = node(head + Vector((1.6, 0, -1.4)), (6.0, 6.0), i_nk)
i_jaw = node(head + Vector((4.4, 0, -5.2)), (3.4, 3.4), i_hd)
for s, sgn in (('N', -1), ('F', 1)):
    i_sh = node(R['sh' + s], (4.9, 4.9), i_chs)
    i_el = node(R['elb' + s], (4.1, 4.1), i_sh)
    i_wr = node(R['wri' + s], (3.3, 3.3), i_el)
    node(R['fist' + s], (3.5, 3.5), i_wr)
    i_hp = node(R['hip' + s], (5.6, 5.6), i_pel)
    i_kn = node(R['knee' + s], (5.0, 5.0), i_hp)
    sh_ = R['knee' + s] + (R['ank' + s] - R['knee' + s]) * 0.22
    i_bl = node(sh_, (5.4, 5.4), i_kn)                                             # the trouser bloused over the boot
    i_bt = node(R['knee' + s] + (R['ank' + s] - R['knee' + s]) * 0.36, (4.9, 4.9), i_bl)   # the boot's folded top
    i_an = node(R['ank' + s], (3.9, 3.9), i_bt)
    node(R['toe' + s], (2.7, 2.7), i_an)
body = make_skin('body', body_nodes, body_edges)

# materials and regions per face, from the nearest bone in the rest pose
GROUP = dict(claN='spine', claF='spine')
for poly in body.data.polygons:
    c = Vector(poly.center)
    best = None
    for bn, (a, b) in BONES.items():
        if bn.startswith('skirt'): continue
        t, d = seg_t(c, R[a], R[b])
        if best is None or d < best[0]: best = (d, bn, t)
    d, bn, t = best
    rg = GROUP.get(bn, bn)
    z = c.z
    if rg == 'spine':
        if hip.z - 1.5 <= z <= hip.z + 3.2: m = 'belt'
        elif z < hip.z - 1.5: m = 'trews'
        elif (c - neck).length < 5.6 or (c.y < -1.0 and seg_t(Vector((c.x, 0, c.z)), Vector((neck.x + 2.0, 0, neck.z - 1.5)), Vector((hip.x + 8.5, 0, hip.z + 3.5)))[1] < 1.5):
            m = 'trim'    # the collar, and the wrap's crossover edge running down to the belt
        else: m = 'mustard'
    elif rg == 'neck': m = 'trim' if c.z < neck.z + 3.6 else 'skin'   # a high collar
    elif rg in ('head', 'handN', 'handF'): m = 'skin'
    elif rg.startswith('uarm'): m = 'mustard'
    elif rg.startswith('farm'): m = 'trim' if t > 0.8 else 'mustard'
    elif rg.startswith('thigh'): m = 'trews'
    elif rg.startswith('shin'): m = 'trews' if t < 0.3 else 'leather'
    else: m = 'boot' if z < 0.9 else 'leather'
    poly.material_index = mat_slot(body, MATS3.index(m), REG[rg])

# skin the body to the armature (bone heat), the skirt bone left out
for bn in ('skirt', 'skirtF'): arm_data.bones[bn].use_deform = False
for o in bpy.context.view_layer.objects: o.select_set(False)
body.select_set(True); rig.select_set(True)
bpy.context.view_layer.objects.active = rig
bpy.ops.object.parent_set(type='ARMATURE_AUTO')
for bn in ('skirt', 'skirtF'): arm_data.bones[bn].use_deform = True


# ---------------------------------------------------------------- gear: explicit weights
def gear(name, verts, faces, mat, weights, region=REG['gear'], smooth=True):
    """weights: f(vertex position) -> {bone: weight}"""
    me = bpy.data.meshes.new(name)
    me.from_pydata(verts, [], faces)
    me.validate()
    ob = bpy.data.objects.new(name, me)
    col.objects.link(ob)
    mi = mat_slot(ob, MATS3.index(mat), region)
    for p in me.polygons: p.material_index = mi; p.use_smooth = smooth
    for v in me.vertices:
        for bn, w in weights(Vector(v.co)).items():
            if w <= 0: continue
            g = ob.vertex_groups.get(bn) or ob.vertex_groups.new(name=bn)
            g.add([v.index], w, 'REPLACE')
    ob.parent = rig
    ob.modifiers.new('arm', 'ARMATURE').object = rig
    return ob


def hsh(i, s=0):
    n = math.sin(i * 12.9898 + s * 78.233) * 43758.5453
    return n - math.floor(n)


def ring_mesh(cx, cz, rings, cols, holes=(), split=None):
    """A tube of `rings` = [(z, rx, ry, ragged)] around (cx, cz); returns verts, faces."""
    V_, F_ = [], []
    for ri, (z, rx, ry, rag) in enumerate(rings):
        for ci in range(cols):
            a = ci / cols * 2 * math.pi
            zz = z - (rag * (0.35 + 0.65 * hsh(ci, 5)) * (1.0 if ci % 2 == 0 else 0.45) if rag else 0.0)
            V_.append((cx + rx * math.cos(a), ry * math.sin(a), zz))
    for ri in range(len(rings) - 1):
        for ci in range(cols):
            if (ri, ci) in holes: continue
            if split and ri >= split[0] and ci in split[1]: continue
            a, b = ri * cols + ci, ri * cols + (ci + 1) % cols
            F_.append((a, b, b + cols, a + cols))
    return V_, F_


# the tunic's skirt: from the belt to the knee, flared, torn into tatters, split up the front
cols = 28
zt = hip.z + 1.5
sk_rings = [(zt, 9.8, 11.0, 0), (zt - 4, 10.0, 11.2, 0), (zt - 8, 10.4, 11.6, 0), (zt - 12, 10.8, 11.9, 0), (zt - 14.5, 11.0, 12.1, 0), (zt - 16.5, 11.2, 12.3, 4.2)]   # snug at the hips, flared, ragged below the knee
front = [c for c in range(cols) if abs((c / cols * 2 * math.pi + 0.32 + math.pi) % (2 * math.pi) - math.pi) < 0.2]   # just to the near side of centre front
sv, sf = ring_mesh(hip.x, zt, sk_rings, cols, split=(1, front))
def w_skirt(p):
    h = max(0.0, min(1.0, (zt - p.z) / 16.5))
    a = math.atan2(p.y, p.x - hip.x)
    fr = max(0.0, math.cos(a))
    w = {'skirtF': h * fr, 'skirt': h * max(0.0, -math.cos(a))}   # hangs from the waist; the leading knee pushes the front out, the trailing one the back
    w['spine'] = max(0.0, 1 - sum(w.values()))
    return w
sk = gear('skirt', sv, sf, 'mustard', w_skirt)
col.objects.unlink(sk); robe.objects.link(sk)

# the belt: a wide leather band, a steel buckle, the tail hanging down the front, a pouch on the back
for k, (z0, z1, g) in enumerate(((hip.z + 4.0, hip.z + 1.6, 0.0), (hip.z + 1.0, hip.z - 1.6, 0.5))):
    bv, bf = ring_mesh(hip.x, 0, [(z0, 10.6 + g, 12.0 + g, 0), ((z0 + z1) / 2, 10.9 + g, 12.3 + g, 0), (z1, 11.1 + g, 12.5 + g, 0)], 28)
    gear('belt%d' % k, bv, bf, 'belt', lambda p: {'spine': 1.0})


def box(c, sx, sy, sz):
    v = [(c.x + dx * sx, c.y + dy * sy, c.z + dz * sz) for dx in (-1, 1) for dy in (-1, 1) for dz in (-1, 1)]
    f = [(0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3)]
    return v, f


def uv_sphere(c, r, seg=16, rings=10, keep=lambda p: True, scale=(1, 1, 1)):
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=seg, v_segments=rings, radius=r)
    for v in bm.verts: v.co = Vector((v.co.x * scale[0], v.co.y * scale[1], v.co.z * scale[2])) + c
    dead = [f for f in bm.faces if not keep(f.calc_center_median())]
    bmesh.ops.delete(bm, geom=dead, context='FACES')
    v = [tuple(x.co) for x in bm.verts]
    idx = {x: i for i, x in enumerate(bm.verts)}
    f = [tuple(idx[x] for x in fa.verts) for fa in bm.faces]
    bm.free()
    return v, f


def torus(c, axis, R_, r, seg=14, sides=6):
    q = Vector((0, 0, 1)).rotation_difference(axis.normalized())
    V_, F_ = [], []
    for i in range(seg):
        a = i / seg * 2 * math.pi
        for j in range(sides):
            b = j / sides * 2 * math.pi
            p = Vector(((R_ + r * math.cos(b)) * math.cos(a), (R_ + r * math.cos(b)) * math.sin(a), r * math.sin(b)))
            V_.append(tuple(q @ p + c))
    for i in range(seg):
        for j in range(sides):
            a, b_ = i * sides + j, i * sides + (j + 1) % sides
            c_, d_ = ((i + 1) % seg) * sides + (j + 1) % sides, ((i + 1) % seg) * sides + j
            F_.append((a, b_, c_, d_))
    return V_, F_


gear('buckle', *torus(Vector((hip.x + 11.8, -3.0, hip.z + 2.8)), Vector((1, -0.25, 0)), 1.4, 0.45, 12, 5), 'helm', lambda p: {'spine': 1.0})
gear('tail', *box(Vector((hip.x + 11.8, -4.6, hip.z - 4.4)), 0.5, 1.1, 6.0), 'belt', w_skirt, smooth=False)
gear('tip', *box(Vector((hip.x + 11.8, -4.6, hip.z - 10.8)), 0.55, 1.2, 0.6), 'helm', w_skirt, smooth=False)
gear('pouch', *uv_sphere(Vector((hip.x - 11.2, -3.0, hip.z - 2.6)), 3.0, 10, 6, scale=(0.7, 1.0, 1.1)), 'belt', lambda p: {'spine': 1.0})

# the head's gear, built round the rest-pose head and weighted to the head bone
H = lambda p: {'head': 1.0}
brow = head.z - 0.6
dome_c = head + Vector((0.8, 0, 0.6))
gear('helm', *uv_sphere(dome_c, 8.2, 20, 12, keep=lambda p: p.z >= brow - 0.4), 'helm', H)
gear('ridge', *uv_sphere(dome_c + Vector((0, 0, 0.3)), 8.5, 24, 12, keep=lambda p: p.z >= brow + 1.0, scale=(1, 0.16, 1)), 'helm', H)
EYES = [head + Vector((6.3, y, -2.0)) for y in (-3.0, 3.0)]
def guard(p):   # a band over the eyes and cheeks, with a hole for each eye
    d = (p - dome_c).normalized()
    return brow - 4.2 <= p.z <= brow + 0.2 and p.x > dome_c.x + 0.5 and all(d.dot((e - dome_c).normalized()) < 0.975 for e in EYES)
gear('guard', *uv_sphere(dome_c, 8.5, 28, 16, keep=guard), 'helm', H)
gear('brim', *torus(Vector((dome_c.x, 0, brow)), Vector((0, 0, 1)), 7.8, 1.0, 22, 6), 'helm', H)
for s, y in (('N', -3.0), ('F', 3.0)):
    e = head + Vector((6.3, y, -2.0))
    gear('ring' + s, *torus(e + Vector((1.9, y * 0.25, 0)), Vector((1.0, y * 0.22, 0.15)), 2.1, 0.8, 12, 5), 'helm', H)
    gear('socket' + s, *uv_sphere(e - Vector((0.6, 0, 0)), 1.5, 8, 6), 'socket', H)
gear('nasal', *box(head + Vector((9.0, 0, -3.4)), 0.7, 1.0, 3.8), 'helm', H, smooth=False)
gear('nose', *uv_sphere(head + Vector((7.4, 0, -3.4)), 1.2, 8, 6), 'skin', H)
hair_n = [(head + Vector((-4.6, 0, -0.4)), (4.6, 5.4)), (head + Vector((-5.6, 0, -6.0)), (4.4, 5.4)), (head + Vector((-6.0, 0, -11.0)), (3.0, 4.0))]
hb = make_skin('hair', hair_n, [(0, 1), (1, 2)], 1)
beard_n = [(head + Vector((5.6, 0, -7.4)), (4.0, 4.6)), (head + Vector((0.6, -3.8, -6.0)), (3.2, 3.2)), (head + Vector((0.6, 3.8, -6.0)), (3.2, 3.2)),
           (head + Vector((6.4, 0, -11.0)), (2.8, 3.0)), (head + Vector((7.3, -1.8, -4.4)), (0.9, 0.9)), (head + Vector((7.3, 1.8, -4.4)), (0.9, 0.9))]
bb = make_skin('beard', beard_n, [(0, 1), (0, 2), (0, 3), (1, 4), (2, 5)], 1)
for ob in (hb, bb):
    mi = mat_slot(ob, MATS3.index('hair'), REG['head'])
    for p in ob.data.polygons: p.material_index = mi
    g = ob.vertex_groups.new(name='head'); g.add([v.index for v in ob.data.vertices], 1.0, 'REPLACE')
    ob.parent = rig; ob.modifiers.new('arm', 'ARMATURE').object = rig


# ---------------------------------------------------------------- camera and render settings
S, FW, FH, GX, GND = JOBS['S'], JOBS['FW'], JOBS['FH'], JOBS['GX'], JOBS['GND']
cam_d = bpy.data.cameras.new('cam'); cam_d.type = 'ORTHO'; cam_d.sensor_fit = 'HORIZONTAL'
cam_d.ortho_scale = FW / S; cam_d.clip_start = 1; cam_d.clip_end = 1000
cam = bpy.data.objects.new('cam', cam_d); col.objects.link(cam)
cam.location = ((FW / 2 - GX) / S, -JOBS['CAMY'], (GND - FH / 2) / S)
cam.rotation_euler = (math.pi / 2, 0, 0)
sc.camera = cam
r = sc.render
r.engine = 'CYCLES'; r.resolution_x, r.resolution_y, r.resolution_percentage = FW, FH, 100
r.film_transparent = True
r.image_settings.media_type = 'MULTI_LAYER_IMAGE'; r.image_settings.file_format = 'OPEN_EXR_MULTILAYER'; r.image_settings.color_depth = '32'
r.use_persistent_data = True
cy = sc.cycles
cy.samples = 1; cy.use_denoising = False; cy.pixel_filter_type = 'BOX'; cy.filter_width = 0.01; cy.max_bounces = 0
cy.use_adaptive_sampling = False; cy.seed = 0; cy.device = 'CPU'
vl = sc.view_layers[0]
vr = sc.view_layers.new('robe')
for v, off in ((vl, 'robe'), (vr, 'base')):
    v.layer_collection.children[off].exclude = True
    v.use_pass_z = v.use_pass_normal = v.use_pass_material_index = True
    a = v.aovs.add(); a.name = 'rest'; a.type = 'COLOR'

for job in JOBS['jobs']:
    pose({k: B(v) for k, v in job['j'].items()})
    r.filepath = os.path.join(OUT, job['key'] + '.exr')
    bpy.ops.render.render(write_still=True)

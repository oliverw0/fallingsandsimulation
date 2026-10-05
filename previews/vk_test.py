import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'tools'))
from viking_clips import *
from PIL import Image

BG = (24, 18, 36)


def to_img(rgb, opaque, z=1, bg=BG):
    h, w = opaque.shape
    im = Image.new('RGB', (w, h), bg)
    px = im.load()
    for y in range(h):
        for x in range(w):
            if opaque[y, x]: px[x, y] = tuple(int(v) for v in rgb[y, x])
    return im.resize((w * z, h * z), Image.NEAREST) if z > 1 else im


def frame(P, scale, w, h, gx, ground):
    rgb, tag, op = render_pose(P, scale, w, h, gx, ground)
    return to_img(rgb, op, 1)


def sheet(path, clips, scale=1.0, z=1, w=150, h=150, gx=75, ground=132, maxcols=16):
    rows = []
    for nm, (poses, ms) in clips:
        rows.append([frame(P, scale, w, h, gx, ground) for P in poses])
    cols = min(maxcols, max(len(r) for r in rows))
    nrows = sum((len(r) + cols - 1) // cols for r in rows)
    im = Image.new('RGB', (cols * w, nrows * h), BG)
    y = 0
    for fr in rows:
        for c, f in enumerate(fr): im.paste(f, ((c % cols) * w, (y + c // cols) * h))
        y += (len(fr) + cols - 1) // cols
    if z > 1: im = im.resize((im.width * z, im.height * z), Image.NEAREST)
    im.save(path)


if __name__ == '__main__':
    what = sys.argv[2] if len(sys.argv) > 2 else 'axe'
    scale = float(sys.argv[3]) if len(sys.argv) > 3 else 1.0
    w = h = 150 if scale >= 1 else 100
    gx, ground = (75, 132) if scale >= 1 else (50, 86)
    if what == 'body': clips = list(body_clips().items())
    else: clips = list(weapon_clips(what).items())
    sheet(sys.argv[1], clips, scale, 1, w, h, gx, ground)

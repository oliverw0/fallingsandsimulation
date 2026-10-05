import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'tools'))
import viking_emit as E
from PIL import Image
import numpy as np

BG = (24, 18, 36)


def img_of(idx, tintmetal=None):
    h, w = idx.shape
    im = Image.new('RGB', (w, h), BG)
    px = im.load()
    pal = E.PAL_LIST
    for y in range(h):
        for x in range(w):
            v = idx[y, x]
            if v:
                r, g, b, a, t = pal[v - 1]
                if a < 255:
                    br, bg_, bb = BG
                    k = a / 255.0
                    r, g, b = int(br + (r - br) * k), int(bg_ + (g - bg_) * k), int(bb + (b - bb) * k)
                px[x, y] = (r, g, b)
    return im


def main(out, which, z=3, cols=12, names=None):
    sheets = E.build_sheets()
    frames, table = sheets[which]
    use = []
    for cn in E.CLIP_NAMES:
        if names and cn not in names: continue
        f0, n = table[cn]
        use += list(range(f0, f0 + n))
    rows = (len(use) + cols - 1) // cols
    im = Image.new('RGB', (cols * E.FW, rows * E.FH), BG)
    for k, fi in enumerate(use):
        im.paste(img_of(frames[fi]), ((k % cols) * E.FW, (k // cols) * E.FH))
    # crop to content
    a = np.array(im).astype(int)
    m = (np.abs(a - np.array(BG)).sum(2) > 0)
    ys, xs = np.where(m)
    im = im.crop((0, 0, im.width, im.height)).resize((im.width * z, im.height * z), Image.NEAREST)
    im.save(out)


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2], int(sys.argv[3]) if len(sys.argv) > 3 else 3, int(sys.argv[4]) if len(sys.argv) > 4 else 10,
         sys.argv[5].split(',') if len(sys.argv) > 5 else None)

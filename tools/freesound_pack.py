# Fetches the freesound.org previews used by the game and packs them into sounds_fs.h:   python tools/freesound_pack.py > /dev/null
# (macOS: needs `afconvert` for mp3 -> wav; numpy). All of them are Creative Commons 0 (public domain), see docs/SOUND_CREDITS.md.
# name: (preview url, start s, max length s, target peak, max gain)
import os, subprocess, sys, tempfile, wave
import numpy as np
P = 'https://cdn.freesound.org/previews/'
SOUNDS = {
    'CHEST1': (P + '573/573654_6614920-hq.mp3', 0, 2.4, 0.8, 6), 'CHEST2': (P + '573/573653_6614920-hq.mp3', 0, 1.8, 0.8, 8),
    'CLANK1': (P + '616/616492_702542-hq.mp3', 0, 0.6, 0.8, 1), 'CLANK2': (P + '426/426322_8522109-hq.mp3', 0, 0.8, 0.8, 3),
    'CLANK3': (P + '733/733887_6703998-hq.mp3', 0, 1.0, 0.8, 1),
    'SPLASH1': (P + '867/867460_71257-hq.mp3', 0, 1.0, 0.8, 1), 'SPLASH2': (P + '805/805910_14688336-hq.mp3', 2.3, 1.0, 0.8, 3),
    'CREAK1': (P + '113/113361_1371021-hq.mp3', 0, 2.2, 0.8, 1), 'CREAK2': (P + '113/113362_1371021-hq.mp3', 0, 2.2, 0.8, 1),
    'CREAK3': (P + '456/456814_9159316-hq.mp3', 0, 2.2, 0.8, 1),
    'SWIM1': (P + '316/316598_2291325-hq.mp3', 0.2, 0.9, 0.7, 8), 'SWIM2': (P + '398/398042_7586736-hq.mp3', 0, 0.7, 0.7, 6),
}
out = ['// Recorded sounds from freesound.org (all Creative Commons 0 / public domain, see docs/SOUND_CREDITS.md), converted to mono 22.05 kHz 16-bit,',
       '// trimmed to the sound, peak-normalised and faded out (tools/freesound_pack.py). Used by audio.cpp.', '#pragma once', '']
tmp = tempfile.mkdtemp()
for k, (url, st, ln, pk, mg) in SOUNDS.items():
    mp3, wav = os.path.join(tmp, k + '.mp3'), os.path.join(tmp, k + '.wav')
    subprocess.run(['curl', '-s', '-m', '60', '-A', 'Mozilla/5.0', '-o', mp3, url], check=True)
    subprocess.run(['afconvert', '-f', 'WAVE', '-d', 'LEI16@22050', '-c', '1', mp3, wav], check=True)
    w = wave.open(wav)
    a = np.frombuffer(w.readframes(w.getnframes()), dtype='<i2').astype(float) / 32768
    a = a[int(st * 22050):int((st + ln) * 22050)]
    idx = np.where(abs(a) > 0.02 * abs(a).max())[0]
    a = a[max(0, idx[0] - 100):idx[-1] + 1]
    a = a * min(mg, pk / abs(a).max())
    f = min(len(a), 1500)
    a[-f:] *= np.linspace(1, 0, f)
    a[:40] *= np.linspace(0, 1, 40)
    d = (np.clip(a, -1, 1) * 32000).astype('<i2').tobytes()
    out += [f'#define {k}_FRAME_COUNT {len(a)}', f'#define {k}_SAMPLE_RATE 22050', f'#define {k}_SAMPLE_SIZE 16', f'#define {k}_CHANNELS 1',
            f'static unsigned char {k}_DATA[{len(d)}] = {{' + ', '.join(f'0x{b:x}' for b in d) + '};', '']
open(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'sounds_fs.h'), 'w').write('\n'.join(out))

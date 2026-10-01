// All sound is synthesised at startup, so the game ships without audio files.
// Recipes layer filtered noise, modal (inharmonic) partials and formant-filtered voices,
// then run through a small Freeverb-style reverb. Common sounds get several variants.
#include "game.h"
#include "util.h"
#include <cmath>
#include <vector>
#include <functional>
#include <algorithm>

static const int SR = 44100;
static const int VOICES = 4;     // aliases per variant so sounds can overlap
static const float TAU = 6.2831853f;

struct SfxSlot
{
    std::vector<Sound> base;                // variants
    std::vector<std::vector<Sound>> alias;  // VOICES aliases per variant
    int next = 0;
    int lastFrame = -100;
    int minGap = 0; // frames between plays (stops 30 explosions stacking)
};
static SfxSlot sfx[SFX_COUNT];
static Sound music{};
static bool audioOk = false;
static int frameCounter = 0;

// ---------------------------------------------------------------- DSP building blocks

struct Rng
{
    uint32_t s;
    explicit Rng(uint32_t seed) : s(seed * 2654435761u + 1) {}
    float operator()() { s = s * 1664525u + 1013904223u; return ((s >> 9) & 0xFFFF) / 32768.0f - 1; }
};

// RBJ biquad
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void set(int type, float f, float q)
    {
        f = clampf(f, 20, SR * 0.45f);
        float w = TAU * f / SR, cs = std::cos(w), sn = std::sin(w), al = sn / (2 * q);
        float a0;
        switch (type)
        {
        case 0: // low pass
            b0 = (1 - cs) / 2; b1 = 1 - cs; b2 = (1 - cs) / 2; a0 = 1 + al; a1 = -2 * cs; a2 = 1 - al; break;
        case 1: // high pass
            b0 = (1 + cs) / 2; b1 = -(1 + cs); b2 = (1 + cs) / 2; a0 = 1 + al; a1 = -2 * cs; a2 = 1 - al; break;
        default: // band pass (constant peak gain)
            b0 = al; b1 = 0; b2 = -al; a0 = 1 + al; a1 = -2 * cs; a2 = 1 - al; break;
        }
        b0 /= a0; b1 /= a0; b2 /= a0; a1 /= a0; a2 /= a0;
    }
    float operator()(float x)
    {
        float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};
enum { LP, HP, BP };

static float env(float t, float a, float d) { return t < a ? t / a : std::exp(-(t - a) / d); }
static float softclip(float x) { return std::tanh(x); }

// Freeverb-lite: 4 damped combs into 2 allpasses.
static void reverb(std::vector<float>& buf, float wet, float room)
{
    if (wet <= 0) return;
    const int cl[4] = {1116, 1188, 1277, 1356}, al[2] = {556, 441};
    std::vector<float> comb[4], ap[2];
    float store[4] = {0, 0, 0, 0};
    int ci[4] = {0, 0, 0, 0}, ai[2] = {0, 0};
    for (int i = 0; i < 4; i++) comb[i].assign(cl[i], 0);
    for (int i = 0; i < 2; i++) ap[i].assign(al[i], 0);
    for (auto& s : buf)
    {
        float in = s * 0.25f, out = 0;
        for (int i = 0; i < 4; i++)
        {
            float y = comb[i][ci[i]];
            store[i] = y * 0.8f + store[i] * 0.2f; // damping
            comb[i][ci[i]] = in + store[i] * room;
            ci[i] = (ci[i] + 1) % cl[i];
            out += y;
        }
        for (int i = 0; i < 2; i++)
        {
            float b = ap[i][ai[i]];
            ap[i][ai[i]] = out + b * 0.5f;
            out = b - out;
            ai[i] = (ai[i] + 1) % al[i];
        }
        s = s + out * wet;
    }
}

static Sound bake(std::vector<float>& buf, float gain)
{
    float peak = 0.0001f;
    for (float v : buf) peak = std::max(peak, std::fabs(v));
    float norm = gain / peak;
    int n = (int)buf.size();
    short* data = (short*)MemAlloc(n * sizeof(short));
    for (int i = 0; i < n; i++)
    {
        float fade = std::min(1.0f, (n - i) / 400.0f);
        data[i] = (short)(clampf(buf[i] * norm * fade, -1, 1) * 32000);
    }
    Wave w{(unsigned int)n, (unsigned int)SR, 16, 1, data};
    Sound s = LoadSoundFromWave(w);
    UnloadWave(w);
    return s;
}

// Render `dur` seconds from a generator, add a reverb tail, normalise to `gain`.
static Sound render(float dur, float wet, float room, float gain, std::function<float(float)> gen)
{
    float tail = wet > 0 ? 0.25f + room * 0.9f : 0.02f;
    std::vector<float> buf((size_t)((dur + tail) * SR), 0.0f);
    int n = (int)(dur * SR);
    for (int i = 0; i < n; i++) buf[i] = gen((float)i / SR);
    reverb(buf, wet, room);
    return bake(buf, gain);
}

// Formant voice: a buzzy source through vowel band-passes (for grunts, groans, roars).
struct Voice
{
    Biquad f1, f2, f3;
    float phase = 0;
    Rng r{7};
    Voice(float a, float b, float c, uint32_t seed) : r(seed) { f1.set(BP, a, 6); f2.set(BP, b, 8); f3.set(BP, c, 10); }
    float operator()(float hz, float breath)
    {
        phase += hz / SR;
        if (phase > 1) phase -= 1;
        float src = (phase * 2 - 1) * 0.7f + r() * breath; // saw + breath noise
        return f1(src) * 1.0f + f2(src) * 0.6f + f3(src) * 0.3f;
    }
};

// ---------------------------------------------------------------- recipes

static void build()
{
    auto add = [](int id, int gap, int variants, std::function<Sound(int)> make) {
        sfx[id].minGap = gap;
        for (int v = 0; v < variants; v++) sfx[id].base.push_back(make(v));
    };

    // sword swing: band-passed air sweeping up and back down (doppler-ish), short airy tail
    add(SFX_SWING, 2, 3, [](int v) {
        Rng n(10 + v); Biquad bp;
        float dur = 0.22f + v * 0.02f;
        return render(dur, 0.12f, 0.4f, 0.55f, [=](float t) mutable {
            float x = t / dur;
            bp.set(BP, 350 + 2300 * std::sin(3.14159f * x) * (0.9f + 0.1f * v), 1.4f);
            return bp(n()) * std::sin(3.14159f * std::pow(x, 0.7f));
        });
    });
    // flesh hit: low thump + wet squelch + click transient
    add(SFX_HIT, 2, 3, [](int v) {
        Rng n(20 + v); Biquad sq, ck;
        sq.set(BP, 520 + v * 90, 2.5f);
        ck.set(HP, 3000, 0.7f);
        return render(0.2f, 0.1f, 0.3f, 0.8f, [=](float t) mutable {
            float thump = std::sin(TAU * (95 - 260 * t) * t) * env(t, 0.002f, 0.05f);
            float squelch = sq(n()) * 3.0f * env(t, 0.003f, 0.04f);
            float click = ck(n()) * env(t, 0.0005f, 0.004f);
            return softclip(thump * 1.2f + squelch + click * 0.6f);
        });
    });
    // armour clang: modal synthesis of a struck plate (inharmonic partials, faster decay up high)
    add(SFX_CLANG, 2, 3, [](int v) {
        Rng n(30 + v); Biquad ck;
        ck.set(HP, 2500, 0.7f);
        float f0 = 560 + v * 70;
        const float ratio[5] = {1.0f, 2.76f, 5.40f, 8.93f, 13.34f}, dec[5] = {0.45f, 0.28f, 0.16f, 0.09f, 0.05f};
        return render(0.7f, 0.25f, 0.55f, 0.6f, [=](float t) mutable {
            float s = 0;
            for (int i = 0; i < 5; i++) s += std::sin(TAU * f0 * ratio[i] * t * (1 + 0.002f * std::sin(TAU * 5 * t))) * std::exp(-t / dec[i]) / (1 + i * 0.6f);
            return s * env(t, 0.0008f, 1) + ck(n()) * env(t, 0.0005f, 0.006f) * 0.8f;
        });
    });
    // explosion: crack, roaring low-passed body that darkens, sub boom, crackle; big room
    add(SFX_EXPLODE, 4, 2, [](int v) {
        Rng n(40 + v); Biquad body, crack, sub;
        crack.set(HP, 1800, 0.7f);
        return render(1.4f, 0.35f, 0.82f, 0.95f, [=](float t) mutable {
            body.set(LP, 3200 * std::exp(-t * 3.2f) + 140, 0.8f);
            float b = body(n()) * env(t, 0.004f, 0.42f) * 2.2f;
            float c = crack(n()) * env(t, 0.0005f, 0.012f);
            float boom = std::sin(TAU * (58 - 30 * t) * t) * env(t, 0.006f, 0.5f) * 1.4f;
            float crackle = (n() > 0.985f ? n() * 2 : 0) * env(t, 0.05f, 0.4f);
            return softclip((b + c * 1.5f + boom + crackle) * 1.3f);
        });
    });
    // arcane cast: shimmering detuned chirp with a breathy edge
    add(SFX_CAST, 2, 2, [](int v) {
        Rng n(50 + v); Biquad air;
        air.set(BP, 4000, 1.5f);
        return render(0.3f, 0.3f, 0.6f, 0.4f, [=](float t) mutable {
            float f = 600 + 1800 * (1 - std::exp(-t * 14)) + v * 120;
            float s = std::sin(TAU * f * t) + std::sin(TAU * f * 1.503f * t) * 0.5f + std::sin(TAU * f * 2.01f * t) * 0.25f;
            return (s * 0.6f + air(n()) * 0.8f) * env(t, 0.01f, 0.08f);
        });
    });
    // fire whoosh: rising band of noise with crackling sparks
    add(SFX_FIRE, 3, 2, [](int v) {
        Rng n(60 + v); Biquad bp, lp;
        return render(0.55f, 0.18f, 0.5f, 0.6f, [=](float t) mutable {
            bp.set(BP, 250 + 1400 * t, 0.9f);
            lp.set(LP, 1200, 0.7f);
            float roar = bp(n()) * 2 + lp(n()) * 0.6f;
            float crack = (n() > 0.992f ? 1.0f : 0.0f) * n();
            return softclip((roar + crack * 2) * env(t, 0.06f, 0.18f));
        });
    });
    // lightning: harsh buzzing arc with random crackle bursts
    add(SFX_ZAP, 3, 2, [](int v) {
        Rng n(70 + v); Biquad hp;
        hp.set(HP, 900, 0.7f);
        float ph = 0;
        return render(0.35f, 0.15f, 0.5f, 0.6f, [=](float t) mutable {
            ph += (110 + 60 * n()) / SR;
            float saw = std::fmod(ph, 1.0f) * 2 - 1;
            float burst = (std::sin(TAU * 37 * t + v) > 0.2f) ? 1.0f : 0.25f;
            return softclip(hp(saw * 1.5f + n() * 1.2f) * burst * 2) * env(t, 0.001f, 0.1f);
        });
    });
    // ice: crystalline cluster of high partials
    add(SFX_ICE, 3, 2, [](int v) {
        Rng n(80 + v); Biquad hp;
        hp.set(HP, 5000, 0.7f);
        float partials[6];
        Rng pr(81 + v);
        for (float& p : partials) p = 2200 + (pr() * 0.5f + 0.5f) * 4200;
        return render(0.45f, 0.3f, 0.6f, 0.45f, [=](float t) mutable {
            float s = 0;
            for (int i = 0; i < 6; i++) s += std::sin(TAU * partials[i] * t) * std::exp(-t / (0.06f + i * 0.03f));
            return s * 0.3f + hp(n()) * env(t, 0.001f, 0.02f);
        });
    });
    // crossbow: Karplus-Strong string twang + wooden thunk
    add(SFX_BOW, 2, 2, [](int v) {
        std::vector<float> line(SR / (140 + v * 20));
        Rng n(90 + v);
        for (auto& b : line) b = n();
        size_t idx = 0;
        return render(0.45f, 0.1f, 0.3f, 0.6f, [=](float t) mutable {
            float s = line[idx];
            size_t nx = (idx + 1) % line.size();
            line[idx] = (line[idx] + line[nx]) * 0.496f;
            idx = nx;
            float thunk = std::sin(TAU * (180 - 300 * t) * t) * env(t, 0.001f, 0.03f);
            return s * env(t, 0.001f, 0.18f) + thunk;
        });
    });
    // jump: cloth flutter and a soft scuff
    add(SFX_JUMP, 4, 2, [](int v) {
        Rng n(100 + v); Biquad bp;
        bp.set(BP, 1500 + v * 300, 0.8f);
        return render(0.12f, 0, 0, 0.3f, [=](float t) mutable { return bp(n()) * env(t, 0.01f, 0.04f); });
    });
    // landing: low thud plus grit
    add(SFX_LAND, 4, 2, [](int v) {
        Rng n(110 + v); Biquad lp, grit;
        lp.set(LP, 300, 0.8f);
        grit.set(BP, 2200, 1.2f);
        return render(0.18f, 0.06f, 0.3f, 0.6f, [=](float t) mutable {
            return lp(n()) * 4 * env(t, 0.002f, 0.05f) + std::sin(TAU * 70 * t) * env(t, 0.002f, 0.06f) + grit(n()) * env(t, 0.001f, 0.02f) * 0.6f;
        });
    });
    // footstep: heel then toe, gritty band-passed scuffs
    add(SFX_STEP, 3, 4, [](int v) {
        Rng n(120 + v); Biquad bp, lp;
        bp.set(BP, 700 + v * 160, 1.3f);
        lp.set(LP, 220, 0.8f);
        return render(0.1f, 0.04f, 0.25f, 0.35f, [=](float t) mutable {
            float heel = env(t, 0.001f, 0.012f), toe = t > 0.035f ? env(t - 0.035f, 0.001f, 0.015f) * 0.7f : 0;
            return bp(n()) * (heel + toe) * 1.5f + lp(n()) * heel * 3;
        });
    });
    // hurt: a short pained grunt (formant voice) over a body thump
    add(SFX_HURT, 6, 3, [](int v) {
        Voice vo(650 + v * 40, 1080, 2400, 130 + v);
        return render(0.28f, 0.1f, 0.3f, 0.7f, [=](float t) mutable {
            float hz = 150 - 70 * t + v * 8;
            return vo(hz, 0.3f) * env(t, 0.015f, 0.09f) * 1.5f + std::sin(TAU * 80 * t) * env(t, 0.002f, 0.04f) * 0.5f;
        });
    });
    // creature death: falling groan plus a crunch
    add(SFX_DIE, 3, 3, [](int v) {
        Voice vo(520 - v * 30, 900, 2300, 140 + v);
        Rng n(141 + v); Biquad cr;
        cr.set(BP, 900, 1.5f);
        return render(0.55f, 0.15f, 0.4f, 0.65f, [=](float t) mutable {
            float hz = 120 - 70 * t + v * 15;
            return vo(hz, 0.5f) * env(t, 0.02f, 0.18f) * 1.2f + cr(n()) * env(t, 0.001f, 0.05f) * 2;
        });
    });
    // pickup: soft two-note bell
    add(SFX_PICKUP, 4, 1, [](int) {
        return render(0.5f, 0.3f, 0.6f, 0.4f, [=](float t) {
            float a = std::sin(TAU * 880 * t) + 0.4f * std::sin(TAU * 2640 * t);
            float b = t > 0.09f ? (std::sin(TAU * 1318 * t) + 0.4f * std::sin(TAU * 3954 * t)) * env(t - 0.09f, 0.002f, 0.15f) : 0;
            return a * env(t, 0.002f, 0.12f) + b;
        });
    });
    // ore: pebbles clicking with a tiny metallic ring
    add(SFX_ORE, 3, 3, [](int v) {
        Rng n(150 + v); Biquad hp;
        hp.set(HP, 2500, 0.7f);
        return render(0.12f, 0.08f, 0.3f, 0.3f, [=](float t) mutable {
            float clicks = env(t, 0.0005f, 0.004f) + (t > 0.03f ? env(t - 0.03f, 0.0005f, 0.004f) * 0.6f : 0);
            return hp(n()) * clicks * 2 + std::sin(TAU * (3200 + v * 300) * t) * env(t, 0.001f, 0.03f) * 0.3f;
        });
    });
    // chest: stick-slip wood creak then a clunk
    add(SFX_CHEST, 10, 1, [](int) {
        Rng n(160); Biquad bp, lp;
        bp.set(BP, 420, 4);
        lp.set(LP, 250, 0.8f);
        float ph = 0;
        return render(0.7f, 0.15f, 0.4f, 0.6f, [=](float t) mutable {
            float rate = 40 + 60 * std::sin(TAU * 1.3f * t) * std::sin(TAU * 1.3f * t);
            ph += rate / SR;
            float pulse = std::fmod(ph, 1.0f) < 0.05f ? 1.0f : 0.0f;
            float creak = bp(pulse + n() * 0.05f) * 4 * (t < 0.5f ? 1 : env(t - 0.5f, 0.001f, 0.05f));
            float clunk = t > 0.5f ? (lp(n()) * 3 + std::sin(TAU * 110 * t)) * env(t - 0.5f, 0.001f, 0.06f) : 0;
            return creak + clunk;
        });
    });
    // portal: swelling chorus with a rushing wind
    add(SFX_PORTAL, 30, 1, [](int) {
        Rng n(170); Biquad bp;
        return render(1.6f, 0.4f, 0.85f, 0.6f, [=](float t) mutable {
            bp.set(BP, 300 + 1500 * t / 1.6f, 0.9f);
            float s = 0;
            for (int i = 0; i < 4; i++) s += std::sin(TAU * (220 * (i + 1) * 1.001f + std::sin(TAU * (0.5f + i) * t) * 6) * t) / (i + 1);
            return (s * 0.4f + bp(n()) * 1.5f) * env(t, 0.5f, 0.6f);
        });
    });
    add(SFX_CLICK, 2, 1, [](int) {
        Rng n(180); Biquad bp;
        bp.set(BP, 2500, 2);
        return render(0.04f, 0, 0, 0.3f, [=](float t) mutable { return bp(n()) * env(t, 0.0005f, 0.006f) + std::sin(TAU * 1800 * t) * env(t, 0.001f, 0.008f) * 0.5f; });
    });
    // anvil: hammer on steel (modal, long ring)
    add(SFX_CRAFT, 10, 1, [](int) {
        Rng n(190); Biquad hp;
        hp.set(HP, 2000, 0.7f);
        const float ratio[5] = {1.0f, 2.32f, 4.25f, 6.63f, 9.38f}, dec[5] = {1.0f, 0.6f, 0.35f, 0.2f, 0.12f};
        return render(1.4f, 0.3f, 0.7f, 0.6f, [=](float t) mutable {
            float s = 0;
            for (int i = 0; i < 5; i++) s += std::sin(TAU * 820 * ratio[i] * t) * std::exp(-t / dec[i]) / (1 + i * 0.5f);
            return s + hp(n()) * env(t, 0.0005f, 0.008f) * 1.5f;
        });
    });
    // roll: dirt and cloth rustle
    add(SFX_ROLL, 5, 2, [](int v) {
        Rng n(200 + v); Biquad bp;
        bp.set(BP, 1800, 0.7f);
        return render(0.3f, 0.05f, 0.3f, 0.45f, [=](float t) mutable {
            float am = 0.6f + 0.4f * std::sin(TAU * 18 * t + v);
            return bp(n()) * am * env(t, 0.04f, 0.1f);
        });
    });
    // boss roar: growling formant voice with flutter, huge room
    add(SFX_ROAR, 60, 1, [](int) {
        Voice vo(480, 820, 2100, 210);
        Rng n(211);
        return render(1.4f, 0.45f, 0.85f, 0.9f, [=](float t) mutable {
            float hz = 70 + 25 * std::sin(TAU * 2.5f * t) + n() * 6;
            float flutter = 0.7f + 0.3f * std::sin(TAU * 28 * t);
            return softclip(vo(hz, 0.8f) * flutter * 2.5f) * env(t, 0.12f, 0.55f);
        });
    });
    // potion: gulping bubbles
    add(SFX_POTION, 10, 1, [](int) {
        Biquad bp;
        return render(0.6f, 0.08f, 0.3f, 0.5f, [=](float t) mutable {
            float g = std::fmod(t, 0.16f);
            bp.set(BP, 280 + 900 * g, 6);
            return bp(std::sin(TAU * (180 + 1200 * g) * t)) * env(g, 0.01f, 0.04f) * 3;
        });
    });
    // splash: falling band of noise with droplet chirps
    add(SFX_SPLASH, 8, 2, [](int v) {
        Rng n(220 + v); Biquad bp;
        return render(0.5f, 0.12f, 0.4f, 0.55f, [=](float t) mutable {
            bp.set(BP, 1600 - 1200 * t, 1.0f);
            float drops = 0;
            for (int i = 0; i < 4; i++)
            {
                float st = 0.05f + i * 0.07f + v * 0.01f;
                if (t > st) drops += std::sin(TAU * (900 + 1600 * (t - st) * 20) * (t - st)) * env(t - st, 0.001f, 0.015f);
            }
            return bp(n()) * 2 * env(t, 0.005f, 0.12f) + drops * 0.4f;
        });
    });
    // grapple: metallic tink plus a rope whip
    add(SFX_HOOK, 3, 2, [](int v) {
        Rng n(230 + v); Biquad bp;
        bp.set(BP, 3000, 1);
        return render(0.2f, 0.1f, 0.3f, 0.45f, [=](float t) mutable {
            float tink = (std::sin(TAU * 2600 * t) + std::sin(TAU * 4100 * t) * 0.5f) * env(t, 0.0005f, 0.04f);
            return tink * 0.6f + bp(n()) * env(t, 0.005f, 0.05f);
        });
    });

    // dog bark: a sharp "wuff", pitch snapping down through an open vowel
    add(SFX_BARK, 20, 3, [](int v) {
        Voice vo(700 + v * 60, 1300, 2600, 240 + v);
        return render(0.18f, 0.12f, 0.45f, 0.6f, [=](float t) mutable {
            float hz = 330 - 900 * t + v * 25;
            return softclip(vo(hz, 0.6f) * 2.2f) * env(t, 0.004f, 0.05f);
        });
    });
    // distant howl: a rising then sagging "ooo" with vibrato, big outdoor room
    add(SFX_HOWL, 240, 2, [](int v) {
        Voice vo(420, 820, 2300, 250 + v);
        return render(1.8f, 0.5f, 0.9f, 0.5f, [=](float t) mutable {
            float x = t / 1.8f;
            float hz = 360 + 260 * std::sin(3.14159f * std::pow(x, 0.6f)) + v * 30 + 7 * std::sin(TAU * 5.5f * t);
            return vo(hz, 0.15f) * env(t, 0.25f, 0.45f);
        });
    });
    // wood knock: a hollow thunk on a crate
    add(SFX_KNOCK, 3, 3, [](int v) {
        Rng n(260 + v); Biquad bp, ck;
        bp.set(BP, 260 + v * 40, 5);
        ck.set(BP, 1800, 1.5f);
        return render(0.2f, 0.1f, 0.35f, 0.65f, [=](float t) mutable {
            return bp(n()) * 6 * env(t, 0.001f, 0.05f) + ck(n()) * env(t, 0.0005f, 0.008f) + std::sin(TAU * (150 - 120 * t) * t) * env(t, 0.001f, 0.04f);
        });
    });
    // crate smash: splintering crackle over a low crash, then bits clattering down
    add(SFX_SMASH, 6, 2, [](int v) {
        Rng n(270 + v); Biquad bp, lp;
        lp.set(LP, 400, 0.8f);
        return render(0.7f, 0.18f, 0.45f, 0.85f, [=](float t) mutable {
            bp.set(BP, 1400 + 900 * std::sin(TAU * 7 * t), 2);
            float crack = (n() > 0.93f ? n() * 3 : 0) * env(t, 0.001f, 0.12f);
            float body = lp(n()) * 4 * env(t, 0.002f, 0.08f) + std::sin(TAU * 80 * t) * env(t, 0.002f, 0.06f);
            float clatter = 0;
            for (int i = 0; i < 5; i++)
            {
                float st = 0.18f + i * 0.08f + v * 0.02f;
                if (t > st) clatter += bp(n()) * env(t - st, 0.001f, 0.02f) * (1 - i * 0.15f);
            }
            return softclip(crack + body + bp(n()) * env(t, 0.002f, 0.1f) * 1.5f + clatter * 1.5f);
        });
    });

    for (auto& s : sfx)
        for (auto& b : s.base)
        {
            std::vector<Sound> al;
            for (int i = 0; i < VOICES; i++) al.push_back(LoadSoundAlias(b));
            s.alias.push_back(al);
        }

    // music: dark minor pad (Am - F - C - E) with sparse plucked notes, drenched in reverb
    const float chords[4][3] = {{220.0f, 261.6f, 329.6f}, {174.6f, 220.0f, 261.6f}, {130.8f, 196.0f, 261.6f}, {164.8f, 207.7f, 246.9f}};
    const float scale[6] = {220.0f, 261.6f, 293.7f, 329.6f, 392.0f, 440.0f};
    const float total = 32.0f;
    std::vector<float> buf((size_t)(total * SR), 0.0f);
    Rng mr(300);
    Biquad padLp;
    padLp.set(LP, 900, 0.7f);
    for (size_t i = 0; i < buf.size(); i++)
    {
        float t = (float)i / SR;
        int c = (int)(t / 8.0f) % 4;
        float local = std::fmod(t, 8.0f);
        float amp = std::min(1.0f, local / 2.0f) * std::min(1.0f, (8.0f - local) / 2.0f);
        float v = 0;
        for (int k = 0; k < 3; k++)
        {
            float f = chords[c][k] * 0.5f;
            for (float det : {0.996f, 1.004f}) // two detuned saws per note
                v += (std::fmod(f * det * t, 1.0f) * 2 - 1);
        }
        buf[i] = padLp(v) * 0.05f * amp + std::sin(TAU * 55 * t) * 0.05f;
    }
    for (int note = 0; note < 24; note++) // plucks
    {
        float st = note * 1.33f + (mr() * 0.5f + 0.5f) * 0.6f;
        if (mr() < -0.2f) continue;
        float f = scale[(int)((mr() * 0.5f + 0.5f) * 5.99f)] * (mr() > 0.3f ? 2.0f : 1.0f);
        std::vector<float> line((size_t)(SR / f));
        for (auto& b : line) b = mr();
        size_t idx = 0;
        for (int j = 0; j < SR * 2 && (size_t)(st * SR) + j < buf.size(); j++)
        {
            float s = line[idx];
            size_t nx = (idx + 1) % line.size();
            line[idx] = (line[idx] + line[nx]) * 0.4985f;
            idx = nx;
            buf[(size_t)(st * SR) + j] += s * 0.08f;
        }
    }
    reverb(buf, 0.6f, 0.88f);
    music = bake(buf, 0.6f);
}

void initAudio()
{
    InitAudioDevice();
    audioOk = IsAudioDeviceReady();
    if (!audioOk) return;
    build();
    SetMasterVolume(0.8f);
}

void closeAudio()
{
    if (!audioOk) return;
    for (auto& s : sfx)
    {
        for (auto& v : s.alias)
            for (auto& a : v) UnloadSoundAlias(a);
        for (auto& b : s.base) UnloadSound(b);
    }
    UnloadSound(music);
    CloseAudioDevice();
}

void playSfx(int id, float vol, float pitch, float pan)
{
    if (!audioOk || sfx[id].base.empty()) return;
    SfxSlot& s = sfx[id];
    if (frameCounter - s.lastFrame < s.minGap) return;
    s.lastFrame = frameCounter;
    auto& voices = s.alias[irand((int)s.alias.size())]; // random variant
    Sound& a = voices[s.next % VOICES];
    s.next++;
    SetSoundVolume(a, vol);
    SetSoundPitch(a, pitch * frange(0.95f, 1.05f));
    SetSoundPan(a, pan);
    PlaySound(a);
}

// Volume and pan fall off with distance from the player.
void playAt(int id, float x, float y, float vol, float pitch)
{
    float dx = x - G.p.m.cx(), dy = y - G.p.m.cy();
    float d = std::sqrt(dx * dx + dy * dy);
    float v = vol * clampf(1 - d / 320.0f, 0, 1);
    if (v < 0.02f) return;
    playSfx(id, v, pitch, clampf(0.5f + dx / 500.0f, 0.1f, 0.9f));
}

void updateAudio(bool inGame)
{
    if (!audioOk) return;
    frameCounter++;
    if (!IsSoundPlaying(music))
    {
        SetSoundVolume(music, 0.5f);
        PlaySound(music);
    }
    float pitch = inGame && !G.sanctuary ? 1.0f - G.stage * 0.05f : 1.1f; // each stage sits in its own key
    SetSoundPitch(music, pitch);
}

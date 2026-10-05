// Most sound (and all the music) is synthesised at startup; the recordings (sword swings and drawing, hammer swings, the
// crossbow, the frying pan) are compiled in from sounds.h, so the game still ships without audio files.
// Recipes layer filtered noise, modal (inharmonic) partials and formant-filtered voices,
// then run through a small Freeverb-style reverb. Common sounds get several variants.
#include "game.h"
#include "util.h"
#include "sounds.h"
#include <cmath>
#include <cstring>
#include <cstdlib>
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
static Sound wind{};
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

static Sound bake(std::vector<float>& buf, float gain, bool fadeOut = true)
{
    float peak = 0.0001f;
    for (float v : buf) peak = std::max(peak, std::fabs(v));
    float norm = gain / peak;
    int n = (int)buf.size();
    short* data = (short*)MemAlloc(n * sizeof(short));
    for (int i = 0; i < n; i++)
    {
        float fade = fadeOut ? std::min(1.0f, (n - i) / 400.0f) : 1.0f;
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

    // blade swish: three recorded sword swings, taken in turn at random
    add(SFX_SWING, 2, 3, [](int v) {
        static const Wave swings[3] = {{SWING1_FRAME_COUNT, SWING1_SAMPLE_RATE, SWING1_SAMPLE_SIZE, SWING1_CHANNELS, SWING1_DATA},
                                       {SWING2_FRAME_COUNT, SWING2_SAMPLE_RATE, SWING2_SAMPLE_SIZE, SWING2_CHANNELS, SWING2_DATA},
                                       {SWING3_FRAME_COUNT, SWING3_SAMPLE_RATE, SWING3_SAMPLE_SIZE, SWING3_CHANNELS, SWING3_DATA}};
        return LoadSoundFromWave(swings[v]);
    });
    // a sword drawn from its scabbard (recorded), when you take one in hand
    add(SFX_DRAW, 8, 1, [](int) {
        Wave w{SWORD_DRAW_FRAME_COUNT, SWORD_DRAW_SAMPLE_RATE, SWORD_DRAW_SAMPLE_SIZE, SWORD_DRAW_CHANNELS, SWORD_DRAW_DATA};
        return LoadSoundFromWave(w);
    });
    // heavy swing (axe, hammer): two recorded war-hammer swings
    add(SFX_HEAVY, 3, 2, [](int v) {
        static const Wave swings[2] = {{HAMMER1_FRAME_COUNT, HAMMER1_SAMPLE_RATE, HAMMER1_SAMPLE_SIZE, HAMMER1_CHANNELS, HAMMER1_DATA},
                                       {HAMMER2_FRAME_COUNT, HAMMER2_SAMPLE_RATE, HAMMER2_SAMPLE_SIZE, HAMMER2_CHANNELS, HAMMER2_DATA}};
        return LoadSoundFromWave(swings[v]);
    });
    // poke: a short sharp jab of air, a flick of cloth and a faint metallic hiss off the point
    add(SFX_THRUST, 2, 3, [](int v) {
        Rng n(520 + v); Biquad bp, ck, ring1, ring2;
        ck.set(HP, 2500, 0.7f);
        ring1.set(BP, 3200 + v * 200, 18);
        ring2.set(BP, 4700 + v * 150, 22);
        float dur = 0.14f + v * 0.015f;
        return render(dur, 0.06f, 0.3f, 0.55f, [=](float t) mutable {
            float x = t / dur;
            float a = x < 0.22f ? std::pow(x / 0.22f, 1.5f) : std::exp(-(x - 0.22f) * 11);
            bp.set(BP, 900 + 2600 * a, 1.6f);
            float s = n();
            float hiss = (ring1(s) + ring2(s)) * env(t, 0.004f, 0.05f) * 0.5f;
            return bp(s) * a + ck(s) * env(t, 0.0008f, 0.004f) * 0.5f + hiss;
        });
    });
    // slam: a hammer into the ground - a sub-thump, a dull body thud, a crunch of grit, then stones pattering down
    add(SFX_SLAM, 4, 3, [](int v) {
        Rng n(540 + v), r(560 + v); Biquad body, grit, patter;
        body.set(LP, 280, 0.8f);
        grit.set(BP, 1700 + v * 200, 0.9f);
        patter.set(BP, 2600, 1.2f);
        return render(0.75f, 0.22f, 0.6f, 0.95f, [=](float t) mutable {
            float boom = std::sin(TAU * (62 - 30 * t) * t) * env(t, 0.002f, 0.16f);
            float thud = body(n()) * 4.0f * env(t, 0.001f, 0.06f);
            float crunch = grit(n()) * 2.2f * env(t, 0.004f, 0.09f);
            float density = t > 0.06f ? 0.02f * std::exp(-(t - 0.06f) * 6) : 0;
            float pat = (r() * 0.5f + 0.5f < density ? 1.0f : 0.0f);
            return softclip(boom * 1.4f + thud + crunch + patter(pat * 8.0f) * 0.6f);
        });
    });
    // crossbow: three recorded bolts loosed
    add(SFX_XBOW, 3, 3, [](int v) {
        static const Wave shots[3] = {{XBOW1_FRAME_COUNT, XBOW1_SAMPLE_RATE, XBOW1_SAMPLE_SIZE, XBOW1_CHANNELS, XBOW1_DATA},
                                      {XBOW2_FRAME_COUNT, XBOW2_SAMPLE_RATE, XBOW2_SAMPLE_SIZE, XBOW2_CHANNELS, XBOW2_DATA},
                                      {XBOW3_FRAME_COUNT, XBOW3_SAMPLE_RATE, XBOW3_SAMPLE_SIZE, XBOW3_CHANNELS, XBOW3_DATA}};
        return LoadSoundFromWave(shots[v]);
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
    // frying pan: a light whoosh as it's swung, and a bright metal clang when it lands (all recorded)
    add(SFX_WHOOSH, 2, 1, [](int) {
        Wave w{PAN_WHOOSH_FRAME_COUNT, PAN_WHOOSH_SAMPLE_RATE, PAN_WHOOSH_SAMPLE_SIZE, PAN_WHOOSH_CHANNELS, PAN_WHOOSH_DATA};
        return LoadSoundFromWave(w);
    });
    add(SFX_DING, 3, 3, [](int v) {
        static const Wave hits[3] = {{PAN_HIT1_FRAME_COUNT, PAN_HIT1_SAMPLE_RATE, PAN_HIT1_SAMPLE_SIZE, PAN_HIT1_CHANNELS, PAN_HIT1_DATA},
                                     {PAN_HIT2_FRAME_COUNT, PAN_HIT2_SAMPLE_RATE, PAN_HIT2_SAMPLE_SIZE, PAN_HIT2_CHANNELS, PAN_HIT2_DATA},
                                     {PAN_HIT3_FRAME_COUNT, PAN_HIT3_SAMPLE_RATE, PAN_HIT3_SAMPLE_SIZE, PAN_HIT3_CHANNELS, PAN_HIT3_DATA}};
        return LoadSoundFromWave(hits[v]);
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

    // wind over the dunes: rumbling low noise and a breathy band that swells with each gust, plus a
    // faint whistle. Gusts are periodic over the buffer and the ends are cross-faded, so it loops cleanly.
    {
        const float len = 16.0f, xf = 1.0f;
        std::vector<float> w((size_t)((len + xf) * SR));
        Rng n(400);
        Biquad low, band, whistle;
        low.set(LP, 220, 0.7f);
        float pink = 0;
        for (size_t i = 0; i < w.size(); i++)
        {
            float t = (float)i / SR, ph = TAU * t / len;
            float gust = 0.55f + 0.3f * std::sin(ph * 2 + 0.7f) + 0.2f * std::sin(ph * 5 + 2.1f) + 0.1f * std::sin(ph * 11);
            if (i % 64 == 0) { band.set(BP, 380 + 520 * gust, 0.8f); whistle.set(BP, 900 + 500 * gust, 18); }
            float wn = n();
            pink = pink * 0.97f + wn * 0.03f;
            w[i] = (low(wn) * 1.6f + band(wn) * 0.9f * gust + whistle(wn) * 0.35f * gust * gust + pink * 0.6f) * gust;
        }
        size_t N = (size_t)(len * SR), X = (size_t)(xf * SR);
        for (size_t i = 0; i < X; i++) { float a = (float)i / X; w[i] = w[i] * a + w[N + i] * (1 - a); }
        w.resize(N);
        wind = bake(w, 0.55f, false);
    }
}

// ---------------------------------------------------------------- music
// One looping tune per area, written as notes and played on a small synthetic band (lute, flute, harp, bells,
// brass, choir, drums). Each tune is rendered once at startup, its reverb tail folded back over its start so it
// loops seamlessly, and streamed from memory as a WAV.

enum { TR_VILLAGE, TR_GREEN, TR_BATTLE, TR_DESERT, TR_CASTLE, TR_CRYPT, TR_MINES, TR_FROST, TR_FORGE, TR_CITADEL, TR_HAVEN, TR_SEA, TR_COUNT };
struct Track
{
    Music m{};
    std::vector<unsigned char> wav; // the stream reads straight from this
    float vol = 0;
    bool ok = false, on = false;
};
static Track trk[TR_COUNT];
using Buf = std::vector<float>;
struct Part { float r, a, d; }; // partial: frequency ratio, amplitude, decay time constant (s)

static float mtof(float n) { return 440.0f * std::pow(2.0f, (n - 69) / 12.0f); }

// additive plucked/struck note; rotating oscillators keep it cheap
static void strike(Buf& b, float t0, float f, float amp, const Part* ps, int n, float ring)
{
    size_t i0 = (size_t)(t0 * SR), len = (size_t)(ring * SR);
    for (int k = 0; k < n; k++)
    {
        float fk = f * ps[k].r;
        if (fk > SR * 0.45f) continue;
        float w = TAU * fk / SR, cw = std::cos(w), sw = std::sin(w), c = 1, s = 0;
        float a = amp * ps[k].a, dk = std::exp(-1.0f / (ps[k].d * SR));
        for (size_t i = 0; i < len && i0 + i < b.size(); i++)
        {
            b[i0 + i] += s * a;
            float c2 = c * cw - s * sw;
            s = s * cw + c * sw; c = c2; a *= dk;
        }
    }
}
static const Part LUTE[] = {{1, 1, .7f}, {2, .55f, .4f}, {3, .38f, .25f}, {4, .2f, .17f}, {5, .1f, .12f}, {6, .05f, .09f}};
static const Part OUD[] = {{1, 1, .6f}, {2, .8f, .35f}, {3, .6f, .25f}, {4, .4f, .15f}, {5, .25f, .1f}};
static const Part HARP[] = {{1, 1, 1.2f}, {2, .3f, .6f}, {3, .12f, .3f}, {4, .05f, .2f}};
static const Part HARPSI[] = {{1, 1, .5f}, {2, .7f, .35f}, {3, .5f, .25f}, {4, .4f, .2f}, {5, .3f, .15f}, {6, .2f, .1f}, {7, .12f, .08f}};
static const Part BELL[] = {{1, 1, 1.6f}, {2.76f, .5f, .9f}, {5.4f, .3f, .5f}, {8.93f, .15f, .3f}};
static const Part GLASS[] = {{1, 1, 2.2f}, {2, .35f, 1.2f}, {3, .2f, .8f}, {4.01f, .1f, .5f}, {6.02f, .06f, .3f}};
static const Part METAL[] = {{1, 1, .35f}, {2.32f, .7f, .2f}, {4.25f, .5f, .12f}, {6.63f, .3f, .08f}};
#define STRIKE(b, t, f, amp, inst, ring) strike(b, t, f, amp, inst, (int)(sizeof(inst) / sizeof(Part)), ring)

static void lute(Buf& b, float t, float midi, float amp, float ring = 1.5f) { STRIKE(b, t, mtof(midi), amp, LUTE, ring); }

// sustained breathy flute with vibrato
static void flute(Buf& b, float t0, float f, float dur, float amp)
{
    size_t i0 = (size_t)(t0 * SR), n = (size_t)((dur + 0.1f) * SR);
    Rng r((uint32_t)(f * 10));
    Biquad br;
    br.set(BP, f * 2, 2);
    float ph = 0;
    for (size_t i = 0; i < n && i0 + i < b.size(); i++)
    {
        float t = (float)i / SR;
        ph += f * (1 + 0.006f * std::sin(TAU * 5.3f * t) * std::min(1.0f, t / 0.25f)) / SR;
        float e = std::min(1.0f, t / 0.05f) * (t < dur ? 1.0f : std::exp(-(t - dur) / 0.03f));
        float s = std::sin(TAU * ph) + 0.22f * std::sin(TAU * 2 * ph) + 0.06f * std::sin(TAU * 3 * ph);
        b[i0 + i] += (s + br(r()) * 0.35f) * e * amp;
    }
}

// two detuned saws through a low-pass: pads, drones and brass (swell opens the filter with the envelope)
static void saws(Buf& b, float t0, float f, float dur, float amp, float att, float rel, float cut, float swell = 0)
{
    size_t i0 = (size_t)(t0 * SR), n = (size_t)((dur + rel) * SR);
    Biquad lp;
    float p1 = 0, p2 = 0.37f;
    for (size_t i = 0; i < n && i0 + i < b.size(); i++)
    {
        float t = (float)i / SR, e = std::min(1.0f, t / att) * (t < dur ? 1.0f : std::max(0.0f, 1 - (t - dur) / rel));
        if ((i & 31) == 0) lp.set(LP, cut * (1 + swell * e), 0.7f);
        p1 += f * 0.997f / SR; p2 += f * 1.003f / SR;
        p1 -= std::floor(p1); p2 -= std::floor(p2);
        b[i0 + i] += lp((p1 * 2 - 1) + (p2 * 2 - 1)) * e * amp * 0.5f;
    }
}
static void pad(Buf& b, float t0, float dur, std::initializer_list<int> notes, float amp, float cut)
{
    for (int n : notes) saws(b, t0, mtof((float)n), dur, amp, dur * 0.3f, dur * 0.3f, cut);
}
static void brass(Buf& b, float t, float midi, float dur, float amp) { saws(b, t, mtof(midi), dur, amp, 0.08f, 0.15f, 800, 1.3f); }

// sung vowel: 0 ah, 1 oo, 2 oh
static void choir(Buf& b, float t0, float f, float dur, float amp, int vowel)
{
    static const float F[3][3] = {{700, 1100, 2500}, {350, 750, 2500}, {450, 800, 2600}};
    Voice v(F[vowel][0], F[vowel][1], F[vowel][2], (uint32_t)(f * 7));
    size_t i0 = (size_t)(t0 * SR), n = (size_t)((dur + 1) * SR);
    for (size_t i = 0; i < n && i0 + i < b.size(); i++)
    {
        float t = (float)i / SR, e = std::min(1.0f, t / (dur * 0.4f)) * (t < dur ? 1.0f : std::max(0.0f, 1 - (t - dur)));
        b[i0 + i] += v(f * (1 + 0.004f * std::sin(TAU * 5 * t + f)), 0.12f) * e * amp;
    }
}

// drums: a pitch-dropping skin thump, and a noisy hand-drum / snare slap
static void thump(Buf& b, float t0, float f, float amp, float dec)
{
    size_t i0 = (size_t)(t0 * SR), n = (size_t)(dec * 6 * SR);
    Rng r((uint32_t)(i0 + f));
    float ph = 0;
    for (size_t i = 0; i < n && i0 + i < b.size(); i++)
    {
        float t = (float)i / SR;
        ph += f * (1 + 1.2f * std::exp(-t * 30)) / SR;
        b[i0 + i] += (std::sin(TAU * ph) * std::exp(-t / dec) + r() * std::exp(-t / 0.004f) * 0.2f) * amp;
    }
}
static void skin(Buf& b, float t0, float f, float amp, float dec)
{
    size_t i0 = (size_t)(t0 * SR), n = (size_t)(dec * 5 * SR);
    Rng r((uint32_t)(i0 + f));
    Biquad bp;
    bp.set(BP, f, 1.2f);
    for (size_t i = 0; i < n && i0 + i < b.size(); i++)
    {
        float t = (float)i / SR;
        b[i0 + i] += (bp(r()) * 2 + std::sin(TAU * f * 0.5f * t) * 0.3f) * std::exp(-t / dec) * amp;
    }
}

// "c#5:2 r:1 bb4:3": note name, octave, length in steps (r = rest); `play(midi, start, length)` per note
template <class F> static void tune(const char* s, float t0, float step, F play)
{
    static const int semi[7] = {9, 11, 0, 2, 4, 5, 7}; // a b c d e f g
    float t = t0;
    while (*s)
    {
        while (*s == ' ' || *s == '|') s++;
        if (!*s) break;
        int midi = -1;
        if (*s == 'r') s++;
        else
        {
            int n = semi[*s++ - 'a'];
            if (*s == '#') { n++; s++; } else if (*s == 'b') { n--; s++; }
            midi = 12 * (*s++ - '0' + 1) + n;
        }
        float len = 1;
        if (*s == ':') { char* e; len = std::strtof(s + 1, &e); s = e; }
        if (midi >= 0) play(midi, t, len * step);
        t += len * step;
    }
}

// fingerpicked arpeggio, 8 steps to the bar, one chord per `per` bars
static void arpeggio(Buf& b, float e, const int* roots, const bool* minor, int chords, int per, const Part* inst, int np, float amp)
{
    static const int maj[8] = {0, 7, 12, 16, 19, 16, 12, 7}, mnr[8] = {0, 7, 12, 15, 19, 15, 12, 7};
    for (int c = 0; c < chords; c++)
        for (int k = 0; k < per * 8; k++)
            strike(b, (c * per * 8 + k) * e, mtof((float)roots[c] + (minor[c] ? mnr : maj)[k % 8]), amp * (k % 8 == 0 ? 1.2f : 0.8f), inst, np, 1.8f);
}

// Hearthwick: a Norse bard's jig in D dorian, 6/8. The lute carries the tune once, then a flute takes it up over a frame drum.
static void tVillage(Buf& b)
{
    const float e = 0.18f, bar = 6 * e;
    struct Ch { int root, t[3]; };
    static const Ch Dm{50, {57, 62, 65}}, C{48, {55, 60, 64}}, G{43, {55, 59, 62}}, Am{45, {57, 60, 64}}, F{41, {57, 60, 65}};
    const Ch* prog[16] = {&Dm, &C, &G, &Am, &Dm, &C, &G, &Dm, &F, &C, &G, &Am, &F, &C, &G, &Dm};
    static const char* A = "a4:1 d5:2 d5:1 f5:2  e5:2 c5:1 e5:3  d5:1 b4:2 g4:1 b4:2  a4:3 c5:1 b4:1 a4:1  a4:1 d5:2 d5:1 f5:2  e5:2 g5:1 e5:3  d5:2 b4:1 c5:1 b4:1 a4:1  d5:5 r:1";
    static const char* B = "f5:1 a5:2 a5:1 c6:2  g5:2 e5:1 g5:3  g5:1 b5:2 b5:1 a5:2  a5:3 e5:1 f5:1 e5:1  f5:1 a5:2 a5:1 c6:2  e6:2 c6:1 g5:3  b5:2 a5:1 g5:1 e5:1 d5:1  d5:5 r:1";
    for (int k = 0; k < 32; k++)
    {
        const Ch& c = *prog[k % 16];
        float t = k * bar;
        lute(b, t, (float)c.root, 0.8f, 1.6f);
        lute(b, t + 3 * e, (float)c.root + 7, 0.6f, 1.2f);
        for (int s : {1, 2, 4, 5})
            for (int j = 1; j < 3; j++) lute(b, t + s * e + j * 0.014f, (float)c.t[j], 0.3f, 0.5f);
        if (k >= 16) // second time round: bodhran
        {
            skin(b, t, 120, 0.7f, 0.09f); skin(b, t + 3 * e, 120, 0.5f, 0.08f);
            skin(b, t + 5 * e, 160, 0.25f, 0.05f);
        }
    }
    auto lead = [&](int m, float t, float d) { lute(b, t, (float)m, 1.0f, std::min(1.5f, std::max(0.5f, d * 1.6f))); };
    auto fl = [&](int m, float t, float d) { flute(b, t, mtof((float)m), d * 0.92f, 0.45f); lute(b, t, (float)m, 0.35f, 0.6f); };
    tune(A, 0, e, lead); tune(B, 8 * bar, e, lead);
    tune(A, 16 * bar, e, fl); tune(B, 24 * bar, e, fl);
}

// the Greenmarch: a wary D minor, harp-picked, with a thin distant flute
static void tGreen(Buf& b)
{
    const float e = 0.375f;
    static const int roots[4] = {50, 46, 43, 45};
    static const bool minor[4] = {true, false, true, true};
    arpeggio(b, e, roots, minor, 4, 2, HARP, 4, 0.35f);
    for (int c = 0; c < 4; c++) pad(b, c * 6.0f, 6, {roots[c] - 12, roots[c], roots[c] + 7, roots[c] + 12 + (minor[c] ? 3 : 4)}, 0.35f, 600);
    tune("a4:6 r:2  f4:4 e4:2 d4:2  d5:4 c5:2 bb4:2  a4:6 r:2  bb4:6 r:2  g4:4 a4:4  e5:4 d5:2 c5:2  a4:6 r:2", 0, e,
         [&](int m, float t, float d) { flute(b, t, mtof((float)m), d * 0.9f, 0.2f); });
}

// the battlefield: war drums and a lone horn in D minor
static void tBattle(Buf& b)
{
    const float u = 0.7f, bar = 4 * u;
    for (int k = 0; k < 8; k++)
    {
        float t = k * bar;
        thump(b, t, 62, 1.2f, 0.25f); thump(b, t + 2 * u, 76, 0.9f, 0.2f); thump(b, t + 3.5f * u, 62, 0.6f, 0.2f);
        if (k % 4 == 3) for (int j = 0; j < 12; j++) skin(b, t + 3 * u + j * u / 12, 220, 0.12f + j * 0.03f, 0.05f); // a roll
    }
    for (int p = 0; p < 4; p++) { saws(b, p * 2 * bar, 36.7f, 2 * bar, 0.5f, 1.5f, 1.5f, 300); saws(b, p * 2 * bar, 55, 2 * bar, 0.3f, 1.5f, 1.5f, 400); }
    tune("d4:2 f4:1 a4:1 d5:3 c5:1  bb4:3 a4:1 g4:2 f4:2  e4:2 g4:2 c5:3 b4:1  a4:2 f4:2 d4:4", 0, u, [&](int m, float t, float d) { brass(b, t, (float)m, d * 0.95f, 0.5f); });
}

// the Scorched Reach: an oud winding through E phrygian dominant over a darbuka
static void tDesert(Buf& b)
{
    const float e = 0.3f, bar = 8 * e;
    for (int k = 0; k < 8; k++)
    {
        float t = k * bar;
        for (int p : {0, 4}) thump(b, t + p * e, 110, 0.9f, 0.1f);
        for (int p : {2, 3, 6}) skin(b, t + p * e, 1800, 0.45f, 0.04f);
        STRIKE(b, t, mtof(40), 0.8f, OUD, 1.5f);
        STRIKE(b, t + 4 * e, mtof(47), 0.5f, OUD, 1.2f);
    }
    for (int p = 0; p < 4; p++) { saws(b, p * 2 * bar, mtof(40), 2 * bar, 0.35f, 1, 1, 350); saws(b, p * 2 * bar, mtof(47), 2 * bar, 0.2f, 1, 1, 350); }
    tune("e4:2 f4:1 g#4:1 f4:1 e4:1 f4:2  g#4:2 a4:1 g#4:1 f4:2 e4:2  a4:2 b4:2 c5:2 b4:2  a4:1 g#4:1 f4:2 e4:4"
         "  b4:2 c5:1 b4:1 a4:2 g#4:2  a4:1 b4:1 c5:2 d5:2 c5:2  b4:2 a4:1 g#4:1 f4:2 g#4:2  f4:2 e4:6", 0, e,
         [&](int m, float t, float d) { STRIKE(b, t, mtof((float)m), 0.45f, OUD, std::min(1.4f, std::max(0.4f, d * 1.5f))); });
}

// Dunmoor: a slow court dirge in A minor, harpsichord over a stately pad and timpani
static void tCastle(Buf& b)
{
    static const int ch[4][3] = {{57, 60, 64}, {53, 57, 60}, {48, 55, 64}, {52, 56, 59}}, root[4] = {45, 41, 36, 40};
    for (int c = 0; c < 4; c++)
    {
        float t = c * 8.0f;
        pad(b, t, 8, {ch[c][0], ch[c][1], ch[c][2], root[c]}, 0.3f, 700);
        thump(b, t, 55, 1.1f, 0.3f); thump(b, t + 4, 55, 0.6f, 0.3f);
        STRIKE(b, t, mtof((float)root[c] + 12), 0.45f, BELL, 3);
    }
    tune("e5:3 a5:1 c6:2 b5:1 a5:1  a5:2 f5:2 c6:3 a5:1  g5:2 c6:2 e6:3 d6:1  b5:3 g#5:1 e5:4", 0, 1.0f,
         [&](int m, float t, float d) { STRIKE(b, t, mtof((float)m), 0.2f, HARPSI, 1.0f); });
}

// the Crypts: a low drone, a lonely choir note or two, and bells dropped into the dark
static void tCrypt(Buf& b)
{
    for (int k = 0; k < 4; k++) { saws(b, k * 8.0f, mtof(38), 8, 0.5f, 3, 3, 250); saws(b, k * 8.0f, mtof(45), 8, 0.3f, 3, 3, 250); }
    static const float bt[9] = {2, 5.5f, 9, 12.5f, 17, 20, 23.5f, 27, 29.5f};
    static const int bn[9] = {74, 70, 72, 69, 75, 67, 74, 70, 63};
    for (int i = 0; i < 9; i++) STRIKE(b, bt[i], mtof((float)bn[i]), 0.3f, BELL, 4);
    choir(b, 4, mtof(62), 6, 2.5f, 1); choir(b, 8, mtof(63), 6, 2.0f, 1);
    choir(b, 20, mtof(57), 7, 2.5f, 1); choir(b, 23, mtof(58), 6, 2.0f, 1);
}

// Deepdelve: a miner's plodding E minor, bass lute, anvil clinks and picks, a hollow pipe the second time round
static void tMines(Buf& b)
{
    const float u = 0.5f, bar = 4 * u;
    static const int roots[8] = {40, 40, 40, 40, 36, 36, 38, 38};
    for (int k = 0; k < 16; k++)
    {
        float t = k * bar;
        int r = roots[k % 8];
        lute(b, t, (float)r, 0.9f); lute(b, t + 1.5f * u, (float)r, 0.6f); lute(b, t + 2 * u, (float)r + 7, 0.7f); lute(b, t + 3 * u, (float)r, 0.6f);
        thump(b, t, 55, 0.7f, 0.15f);
        for (int p : {1, 3}) STRIKE(b, t + p * u, 900 + 220 * ((k + p) % 3), 0.35f, METAL, 0.8f);
        for (int p = 0; p < 8; p++) skin(b, t + p * u / 2, 3000, 0.08f, 0.02f);
    }
    for (int p = 0; p < 4; p++) saws(b, p * 4 * bar, mtof(28), 4 * bar, 0.5f, 1, 1, 250);
    tune("e4:3 g4:1 b4:4  a4:2 g4:2 e4:4  g4:3 e4:1 c5:4  b4:4 a4:2 f#4:2", 16 * u, u, [&](int m, float t, float d) { flute(b, t, mtof((float)m), d * 0.9f, 0.15f); });
}

// Frostdeep: glass bells running up shifting chords over a cold pad
static void tFrost(Buf& b)
{
    static const int ch[4][4] = {{69, 72, 76, 83}, {74, 77, 81, 88}, {71, 74, 79, 83}, {69, 72, 76, 83}};
    static const int pat[8] = {0, 2, 1, 3, 2, 3, 1, 2}, low[4] = {45, 38, 40, 45};
    for (int c = 0; c < 4; c++)
    {
        pad(b, c * 8.0f, 8, {low[c] + 12, low[c] + 19, low[c] + 24}, 0.25f, 1000);
        for (int k = 0; k < 16; k++)
            STRIKE(b, c * 8.0f + k * 0.5f, mtof((float)ch[c][pat[k % 8]]), k % 8 == 0 ? 0.4f : 0.22f, GLASS, 2.5f);
        STRIKE(b, c * 8.0f, mtof((float)low[c] + 24), 0.5f, BELL, 4);
    }
}

// the Infernal Forge: pounding anvils and a growling brass riff in C phrygian
static void tForge(Buf& b)
{
    const float u = 0.45f, bar = 4 * u;
    static const int riff[2][8] = {{0, 0, 1, 0, 0, 0, 3, 1}, {0, 0, 0, 1, 0, 5, 3, 1}};
    for (int k = 0; k < 16; k++)
    {
        float t = k * bar;
        for (int p = 0; p < 4; p++) thump(b, t + p * u, 50, 1.0f, 0.18f);
        for (int p : {0, 2}) STRIKE(b, t + p * u, 640, 0.6f, METAL, 1.0f);
        for (int p : {1, 3}) skin(b, t + p * u, 260, 0.6f, 0.07f);
        for (int p = 0; p < 8; p++) brass(b, t + p * u / 2, 36.0f + riff[k % 2][p], u / 2 * 0.9f, 0.7f);
        if (k % 4 == 0) { brass(b, t, 48, 1.6f * u, 0.8f); brass(b, t, 55, 1.6f * u, 0.8f); }
    }
}

// the Lich's Citadel: a choir in D minor under a tolling bell and a church-organ drone
static void tCitadel(Buf& b)
{
    static const int ch[4][3] = {{57, 62, 65}, {58, 62, 65}, {58, 62, 67}, {57, 61, 64}}, root[4] = {38, 34, 31, 33};
    for (int c = 0; c < 4; c++)
    {
        float t = c * 8.0f;
        for (int j = 0; j < 3; j++) choir(b, t, mtof((float)ch[c][j]), 7, 3.0f, j == 1 ? 2 : 0);
        saws(b, t, mtof((float)root[c]), 8, 0.5f, 2, 2, 700); saws(b, t, mtof((float)root[c] + 12), 8, 0.3f, 2, 2, 900);
        STRIKE(b, t, mtof((float)root[c] + 12), 0.9f, BELL, 5);
        thump(b, t, 50, 1.0f, 0.4f);
    }
}

// a waystone: a hushed A minor, slow harp over a warm pad over a warm pad
static void tHaven(Buf& b)
{
    const float e = 0.45f;
    static const int roots[4] = {45, 41, 38, 40};
    static const bool minor[4] = {true, false, true, true};
    arpeggio(b, e, roots, minor, 4, 2, HARP, 4, 0.4f);
    for (int c = 0; c < 4; c++) pad(b, c * 7.2f, 7.2f, {roots[c] + 12, roots[c] + 19, roots[c] + 24 + (minor[c] ? 3 : 4)}, 0.3f, 800);
}

// the open sea: long minor swells and a bell lost in the fog
static void tSea(Buf& b)
{
    static const int ch[4][3] = {{50, 57, 62}, {45, 52, 57}, {46, 53, 58}, {45, 52, 57}};
    for (int c = 0; c < 4; c++)
    {
        pad(b, c * 8.0f, 8, {ch[c][0], ch[c][1], ch[c][2], ch[c][0] - 12}, 0.4f, 500);
        STRIKE(b, c * 8.0f + 3, mtof((float)ch[c][2] + 12), 0.3f, BELL, 4);
    }
    choir(b, 6, mtof(57), 8, 2.0f, 1); choir(b, 22, mtof(53), 8, 2.0f, 1);
}

static void buildMusic()
{
    static const struct { float loop, wet, room, rms; void (*make)(Buf&); } defs[TR_COUNT] = {
        {34.56f, 0.25f, 0.5f, 0.15f, tVillage}, {24.0f, 0.4f, 0.7f, 0.12f, tGreen},   {22.4f, 0.4f, 0.8f, 0.17f, tBattle},
        {19.2f, 0.3f, 0.6f, 0.15f, tDesert},    {32.0f, 0.5f, 0.85f, 0.13f, tCastle}, {32.0f, 0.6f, 0.9f, 0.1f, tCrypt},
        {32.0f, 0.3f, 0.6f, 0.15f, tMines},     {32.0f, 0.6f, 0.9f, 0.11f, tFrost},   {28.8f, 0.2f, 0.5f, 0.17f, tForge},
        {32.0f, 0.6f, 0.9f, 0.15f, tCitadel},   {28.8f, 0.5f, 0.8f, 0.11f, tHaven},   {32.0f, 0.6f, 0.9f, 0.11f, tSea}};
    for (int id = 0; id < TR_COUNT; id++)
    {
        const auto& d = defs[id];
        size_t N = (size_t)(d.loop * SR);
        Buf b(N + (size_t)(3 * SR), 0.0f);
        d.make(b);
        reverb(b, d.wet, d.room);
        for (size_t i = N; i < b.size(); i++) b[i - N] += b[i]; // the tail wraps round onto the start
        b.resize(N);
        double sq = 0;
        for (float v : b) sq += (double)v * v;
        float k = d.rms / std::sqrt((float)(sq / N) + 1e-9f);
        auto& w = trk[id].wav;
        w.resize(44 + N * 2);
        auto put = [&](size_t at, const void* p, size_t n) { std::memcpy(&w[at], p, n); };
        uint32_t u32; uint16_t u16;
        put(0, "RIFF", 4); u32 = 36 + (uint32_t)N * 2; put(4, &u32, 4); put(8, "WAVEfmt ", 8);
        u32 = 16; put(16, &u32, 4); u16 = 1; put(20, &u16, 2); put(22, &u16, 2);
        u32 = SR; put(24, &u32, 4); u32 = SR * 2; put(28, &u32, 4); u16 = 2; put(32, &u16, 2); u16 = 16; put(34, &u16, 2);
        put(36, "data", 4); u32 = (uint32_t)N * 2; put(40, &u32, 4);
        for (size_t i = 0; i < N; i++) { int16_t s = (int16_t)(std::tanh(b[i] * k) * 30000); put(44 + i * 2, &s, 2); }
        trk[id].m = LoadMusicStreamFromMemory(".wav", w.data(), (int)w.size());
        trk[id].ok = trk[id].m.frameCount > 0;
        trk[id].m.looping = true;
    }
}

void initAudio()
{
    InitAudioDevice();
    audioOk = IsAudioDeviceReady();
    if (!audioOk) return;
    build();
    buildMusic();
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
    for (auto& t : trk) if (t.ok) UnloadMusicStream(t.m);
    UnloadSound(wind);
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

// Which tune fits where the player stands (-1: none, the dunes keep only their wind).
static int trackFor(bool inGame)
{
    if (!inGame || G.inVillage) return TR_VILLAGE; // the title screen sings the bard's tune too
    if (G.sandbox || G.sanctuary) return TR_HAVEN;
    switch (regionId())
    {
    case 1: return -1;
    case 2: return TR_SEA;
    case 3: return TR_DESERT;
    }
    static const int byStage[7] = {TR_GREEN, TR_CASTLE, TR_CRYPT, TR_MINES, TR_FROST, TR_FORGE, TR_CITADEL};
    if (G.stage == 0 && G.stormX1 > G.stormX0 && G.p.m.cx() > G.stormX0 - 100) return TR_BATTLE;
    return byStage[std::min(std::max(G.stage, 0), 6)];
}

void updateAudio(bool inGame)
{
    if (!audioOk) return;
    frameCounter++;
    int want = trackFor(inGame);
    bool dunes = inGame && inDunes();
    for (int i = 0; i < TR_COUNT; i++) // the wanted tune fades in as the old one fades out
    {
        Track& t = trk[i];
        if (!t.ok) continue;
        t.vol += ((i == want ? 1.0f : 0.0f) - t.vol) * 0.02f;
        if (t.vol > 0.003f)
        {
            if (!t.on) { PlayMusicStream(t.m); t.on = true; }
            UpdateMusicStream(t.m);
            SetMusicVolume(t.m, t.vol * (inGame && G.underwater ? 0.25f : 0.5f));
        }
        else if (t.on) { StopMusicStream(t.m); t.on = false; }
    }
    static float windVol = 0;
    float windTo = dunes ? 0.75f : (inGame && (G.inVillage || G.duneEnd) ? 0.12f : 0.0f);
    if (inGame && G.underwater) windTo = 0; // no wind under the waves
    windVol += (windTo - windVol) * (inGame && G.underwater ? 0.08f : 0.01f);
    if (!IsSoundPlaying(wind)) PlaySound(wind);
    SetSoundVolume(wind, windVol);
}

#pragma once
#include <cstdint>
#include <cmath>

// Fast xorshift RNG shared by the whole game.
inline uint32_t& rngState() { static uint32_t s = 2463534242u; return s; }
inline uint32_t xr()
{
    uint32_t& s = rngState();
    s ^= s << 13; s ^= s >> 17; s ^= s << 5;
    return s;
}
inline int irand(int n) { return n > 0 ? (int)(xr() % (uint32_t)n) : 0; }  // [0, n)
inline int irange(int a, int b) { return a + irand(b - a + 1); }            // [a, b]
inline float frand() { return (xr() & 0xFFFFFF) / 16777216.0f; }            // [0, 1)
inline float frange(float a, float b) { return a + (b - a) * frand(); }
inline bool chance(int oneIn) { return irand(oneIn) == 0; }
inline float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }

// Value noise + fractal brownian motion, used for terrain and textures.
inline float hash2(int x, int y, int seed)
{
    uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u + (uint32_t)seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return ((h ^ (h >> 16)) & 0xFFFFFF) / 16777216.0f;
}
inline float vnoise(float x, float y, int seed)
{
    int xi = (int)std::floor(x), yi = (int)std::floor(y);
    float fx = x - xi, fy = y - yi;
    fx = fx * fx * (3 - 2 * fx);
    fy = fy * fy * (3 - 2 * fy);
    float a = hash2(xi, yi, seed), b = hash2(xi + 1, yi, seed);
    float c = hash2(xi, yi + 1, seed), d = hash2(xi + 1, yi + 1, seed);
    return a + (b - a) * fx + (c - a) * fy + (a - b - c + d) * fx * fy;
}
inline float fbm(float x, float y, int seed, int oct = 4)
{
    float v = 0, amp = 0.5f, f = 1, norm = 0;
    for (int i = 0; i < oct; i++)
    {
        v += vnoise(x * f, y * f, seed + i * 31) * amp;
        norm += amp;
        f *= 2;
        amp *= 0.5f;
    }
    return v / norm;
}

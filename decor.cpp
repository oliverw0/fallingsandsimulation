// decor.cpp: furniture and hangings for the farmhouses, watchtowers, halls and castle, painted once per variant at
// half a unit per pixel (the grain of the creatures and the terrain). Same house style as the desert's decor:
// no outline, lit from the upper left, a ramp of five tones per material. Drawn as IT_DECOR (rig.cpp:drawDecor).
#include "game.h"
#include "util.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

namespace
{
using Ramp = std::array<Color, 5>;
constexpr Color C(int r, int g, int b) { return Color{(unsigned char)r, (unsigned char)g, (unsigned char)b, 255}; }
const Ramp WOOD = {C(44, 28, 18), C(74, 48, 30), C(106, 72, 44), C(142, 100, 60), C(180, 136, 86)};
const Ramp IRON = {C(26, 26, 32), C(48, 48, 56), C(80, 82, 92), C(120, 124, 136), C(170, 174, 188)};
const Ramp BRASS = {C(70, 44, 14), C(120, 82, 24), C(176, 128, 40), C(224, 176, 70), C(250, 222, 128)};
const Ramp STONE = {C(34, 32, 38), C(58, 56, 64), C(88, 86, 94), C(122, 120, 128), C(160, 158, 166)};
const Ramp CLAY = {C(60, 32, 20), C(100, 56, 34), C(140, 84, 50), C(180, 114, 70), C(214, 150, 100)};
const Ramp LINEN = {C(78, 66, 52), C(122, 108, 88), C(166, 152, 128), C(206, 194, 168), C(238, 228, 204)};
const Ramp STRAW = {C(96, 70, 24), C(146, 108, 40), C(190, 150, 64), C(220, 184, 96), C(240, 214, 134)};
const Ramp CRIM = {C(40, 8, 14), C(80, 16, 24), C(124, 28, 36), C(166, 46, 52), C(208, 84, 80)};
const Ramp NAVY = {C(12, 18, 46), C(24, 38, 90), C(40, 64, 134), C(64, 96, 176), C(112, 142, 214)};
const Ramp FOREST = {C(12, 30, 20), C(24, 60, 36), C(42, 98, 54), C(68, 136, 74), C(116, 174, 108)};
const Ramp OCHRE = {C(52, 32, 10), C(98, 64, 18), C(148, 102, 32), C(198, 148, 56), C(238, 198, 108)};
const Ramp PLUM = {C(28, 10, 34), C(56, 22, 64), C(88, 38, 96), C(126, 64, 134), C(170, 102, 174)};
const Ramp NIGHT = {C(6, 6, 8), C(16, 16, 20), C(30, 30, 36), C(48, 48, 56), C(78, 78, 90)};
const Ramp BLOODR = {C(40, 6, 8), C(70, 10, 12), C(102, 16, 16), C(136, 26, 24), C(170, 42, 36)};
const Ramp CLOTHS[6] = {CRIM, NAVY, FOREST, OCHRE, PLUM, NIGHT};

inline int clampi(int v, int a, int b) { return v < a ? a : (v > b ? b : v); }
inline const Color& tone(const Ramp& r, int t) { return r[clampi(t, 0, 4)]; }

struct Cv
{
    int w, h;
    std::vector<Color> d;
    Cv(int w_, int h_) : w(w_), h(h_), d((size_t)w_ * h_, Color{0, 0, 0, 0}) {}
    bool in(int x, int y) const { return x >= 0 && y >= 0 && x < w && y < h; }
    void px(int x, int y, Color c) { if (in(x, y)) d[(size_t)y * w + x] = c; }
    Color at(int x, int y) const { return in(x, y) ? d[(size_t)y * w + x] : Color{0, 0, 0, 0}; }
    bool on(int x, int y) const { return at(x, y).a != 0; }
    void rect(int x0, int y0, int x1, int y1, Color c) { for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) px(x, y, c); }
    void hline(int x0, int x1, int y, Color c) { for (int x = x0; x <= x1; x++) px(x, y, c); }
    void vline(int x, int y0, int y1, Color c) { for (int y = y0; y <= y1; y++) px(x, y, c); }
    void line(float x0, float y0, float x1, float y1, Color c, int t = 1)
    {
        int n = (int)(std::max(std::fabs(x1 - x0), std::fabs(y1 - y0)) * 2) + 1;
        for (int i = 0; i <= n; i++)
        {
            float u = (float)i / n, x = x0 + (x1 - x0) * u, y = y0 + (y1 - y0) * u;
            for (int dy = 0; dy < t; dy++) for (int dx = 0; dx < t; dx++) px((int)std::floor(x) + dx, (int)std::floor(y) + dy, c);
        }
    }
    void disc(float cx, float cy, float r, Color c) { ell(cx, cy, r, r, c); }
    void ell(float cx, float cy, float rx, float ry, Color c)
    {
        for (int y = (int)(cy - ry - 1); y <= (int)(cy + ry + 1); y++)
            for (int x = (int)(cx - rx - 1); x <= (int)(cx + rx + 1); x++)
            {
                float dx = (x + 0.5f - cx) / rx, dy = (y + 0.5f - cy) / ry;
                if (dx * dx + dy * dy <= 1) px(x, y, c);
            }
    }
    // a round form lit from the upper left: the ramp's tones run light to dark across it
    void ball(float cx, float cy, float rx, float ry, const Ramp& r, int mid = 2)
    {
        for (int y = (int)(cy - ry - 1); y <= (int)(cy + ry + 1); y++)
            for (int x = (int)(cx - rx - 1); x <= (int)(cx + rx + 1); x++)
            {
                float dx = (x + 0.5f - cx) / rx, dy = (y + 0.5f - cy) / ry;
                if (dx * dx + dy * dy > 1) continue;
                float t = (dx + dy) * 0.5f; // -1 lit .. +1 shaded
                px(x, y, tone(r, mid + (t < -0.55f ? 2 : (t < -0.15f ? 1 : (t > 0.55f ? -2 : (t > 0.2f ? -1 : 0))))));
            }
    }
    // a flat slab: base tone, a light top and left edge, a dark bottom and right edge
    void slab(int x0, int y0, int x1, int y1, const Ramp& r, int t = 2)
    {
        rect(x0, y0, x1, y1, tone(r, t));
        hline(x0, x1, y0, tone(r, t + 1));
        vline(x0, y0, y1, tone(r, t + 1));
        hline(x0, x1, y1, tone(r, t - 1));
        vline(x1, y0, y1, tone(r, t - 1));
    }
    Image image() const
    {
        Image im = GenImageColor(w, h, BLANK);
        std::memcpy(im.data, d.data(), d.size() * sizeof(Color));
        return im;
    }
};

float R(int a, int b, int s) { return hash2(a, b, s); }

// A plank of wood: grain runs along it, a knot now and then.
void plank(Cv& c, int x0, int y0, int x1, int y1, bool horizontal, int seed, int t = 2)
{
    c.slab(x0, y0, x1, y1, WOOD, t);
    for (int y = y0 + 1; y < y1; y++)
        for (int x = x0 + 1; x < x1; x++)
        {
            float g = horizontal ? R(x / 5, y, seed) : R(x, y / 5, seed);
            if (g > 0.86f) c.px(x, y, tone(WOOD, t - 1));
            else if (g < 0.1f) c.px(x, y, tone(WOOD, t + 1));
        }
}

// ------------------------------------------------------------------------------------------------ furniture

void dresser(Cv& c, int var)
{ // 34 x 40: two rows of drawers on stubby feet, with a few things on top
    c.slab(1, 14, 32, 17, WOOD, 3);
    for (int y = 18; y <= 36; y++) for (int x = 2; x <= 31; x++) c.px(x, y, tone(WOOD, 1 + (R(x, y / 4, 3 + var) > 0.8f)));
    for (int row = 0; row < 2; row++)
        for (int col = 0; col < 2; col++)
        {
            int x0 = 3 + col * 15, y0 = 19 + row * 9;
            plank(c, x0, y0, x0 + 13, y0 + 7, true, 11 + var + row * 7 + col, 2);
            c.rect(x0 + 2, y0 + 2, x0 + 11, y0 + 5, tone(WOOD, 2));
            c.hline(x0 + 2, x0 + 11, y0 + 2, tone(WOOD, 1)); c.vline(x0 + 2, y0 + 2, y0 + 5, tone(WOOD, 1));
            c.hline(x0 + 2, x0 + 11, y0 + 5, tone(WOOD, 3));
            c.px(x0 + 6, y0 + 3, BRASS[3]); c.px(x0 + 7, y0 + 3, BRASS[3]); c.px(x0 + 6, y0 + 4, BRASS[1]); c.px(x0 + 7, y0 + 4, BRASS[1]); // the pull
            if (row == 0 && col == 1) c.px(x0 + 3, y0 + 3, BRASS[0]);                                                              // a keyhole
        }
    for (int x : {2, 3, 4, 29, 30, 31}) c.rect(x, 37, x, 39, tone(WOOD, x < 10 ? 2 : 1)); // feet
    int set = var % 3;
    if (set != 1) c.ball(7, 9.5f, 4.2f, 4.8f, CLAY, 2), c.rect(6, 3, 8, 5, tone(CLAY, 2)), c.px(5, 3, tone(CLAY, 3)), c.line(11, 7, 11, 11, tone(CLAY, 1)); // a jug
    if (set != 0) { c.ball(16, 11.5f, 4.5f, 2.8f, LINEN, 2); c.hline(13, 19, 9, tone(LINEN, 4)); }                                                      // a crock
    c.rect(27, 8, 28, 13, tone(IRON, 2)); c.rect(26, 13, 29, 13, tone(IRON, 1));                                                                     // a candlestick
    c.rect(27, 5, 28, 8, LINEN[4]);
    c.px(27, 3, C(255, 232, 150)); c.px(27, 4, C(255, 188, 70)); c.px(28, 4, C(230, 120, 40));
}

void table(Cv& c, int /*w*/, int var)
{ // a trestle table laid for a feast with a bench along it; w = the top's length in units
    int W = c.w, x0 = 8, x1 = W - 9;
    c.slab(x0, 10, x1, 12, WOOD, 3);
    c.hline(x0, x1, 13, tone(WOOD, 0)); c.hline(x0 + 1, x1 - 1, 14, tone(WOOD, 0));
    for (int lx : {x0 + 6, x1 - 6}) // X trestles
    {
        c.line((float)lx - 4, 15, (float)lx + 4, 25, tone(WOOD, 2), 2);
        c.line((float)lx + 4, 15, (float)lx - 4, 25, tone(WOOD, 1), 2);
    }
    c.hline(x0 + 8, x1 - 8, 20, tone(WOOD, 1)); c.hline(x0 + 8, x1 - 8, 21, tone(WOOD, 0)); // the stretcher
    plank(c, 0, 19, W - 1, 21, true, 17 + var, 3); // the bench in front
    for (int lx : {2, W - 4}) c.rect(lx, 22, lx + 1, 25, tone(WOOD, 1));
    for (int x = x0 + 4; x < x1 - 5;)
    {
        int k = (int)(R(x, var, 21) * 5);
        if (k == 0) { c.ell(x + 2, 9, 3.5f, 1.2f, LINEN[3]); c.hline(x, x + 4, 10, LINEN[1]); x += 9; }                              // a plate
        else if (k == 1) { c.rect(x, 5, x + 2, 9, tone(IRON, 3)); c.vline(x, 5, 9, IRON[4]); c.hline(x, x + 2, 9, IRON[1]); x += 6; } // a cup
        else if (k == 2) { c.ball(x + 3, 7.5f, 4.5f, 2.6f, STRAW, 2); c.line(x + 1, 6, x + 2, 7, STRAW[1]); c.line(x + 4, 6, x + 5, 7, STRAW[1]); x += 10; } // bread
        else if (k == 3) { c.ball(x + 2.5f, 6.5f, 2.6f, 3.4f, CLAY, 2); c.rect(x + 2, 2, x + 3, 3, tone(CLAY, 2)); x += 7; }       // a jug
        else { c.rect(x, 6, x, 9, LINEN[4]); c.px(x, 4, C(255, 232, 150)); c.px(x, 5, C(255, 170, 60)); c.rect(x - 1, 10, x + 1, 10, IRON[2]); x += 5; } // a candle
    }
}

void picture(Cv& c, int var)
{ // a painting in a gilt frame; var picks the subject
    int W = c.w, H = c.h;
    c.slab(0, 0, W - 1, H - 1, BRASS, 2);
    c.rect(2, 2, W - 3, H - 3, tone(BRASS, 1));
    int x0 = 3, y0 = 3, x1 = W - 4, y1 = H - 4, k = var % 5;
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++)
        {
            float u = (float)(x - x0) / (x1 - x0), v = (float)(y - y0) / (y1 - y0);
            Color col;
            if (k == 0) // a longship under sail at sundown
            {
                col = v < 0.62f ? lerpColor(C(120, 70, 110), C(244, 160, 96), v / 0.62f) : lerpColor(C(30, 50, 90), C(18, 30, 60), (v - 0.62f) * 2);
                if (v > 0.62f && ((x + (int)(v * 20)) % 4 == 0)) col = C(90, 112, 150);
            }
            else if (k == 1) col = v > 0.55f + 0.1f * std::sin(u * 9) ? lerpColor(C(76, 120, 60), C(40, 76, 40), v) : lerpColor(C(150, 180, 210), C(222, 228, 230), v); // hills
            else if (k == 2) col = lerpColor(C(40, 34, 30), C(70, 58, 46), v); // a portrait's dim ground
            else if (k == 3) col = v > 0.7f ? C(30, 56, 36) : lerpColor(C(44, 72, 84), C(120, 140, 130), v);       // a stag in a clearing
            else col = lerpColor(C(48, 20, 20), C(80, 34, 26), v);                                                 // runes on dark red
            c.px(x, y, col);
        }
    auto P = [&](float fu, float fv) { return Vector2{x0 + fu * (x1 - x0), y0 + fv * (y1 - y0)}; };
    if (k == 0)
    {
        Vector2 a = P(0.3f, 0.7f), b = P(0.72f, 0.7f);
        c.line(a.x, a.y, b.x, b.y, C(64, 40, 28), 2);
        c.line(a.x + 2, a.y + 2, b.x - 3, b.y + 2, C(40, 26, 20), 1);
        c.rect((int)P(0.5f, 0.3f).x, (int)P(0.5f, 0.3f).y, (int)P(0.5f, 0.3f).x, (int)a.y, C(60, 44, 32));
        c.rect((int)P(0.42f, 0.34f).x, (int)P(0.42f, 0.34f).y, (int)P(0.6f, 0.6f).x, (int)P(0.6f, 0.6f).y, C(226, 214, 186));
        c.hline((int)P(0.42f, 0.5f).x, (int)P(0.6f, 0.5f).x, (int)P(0.42f, 0.5f).y, C(160, 44, 38));
    }
    else if (k == 2)
    {
        c.ball(P(0.5f, 0.42f).x, P(0.5f, 0.42f).y, (x1 - x0) * 0.2f, (y1 - y0) * 0.3f, LINEN, 2); // a face
        c.rect((int)P(0.3f, 0.2f).x, (int)P(0.3f, 0.2f).y, (int)P(0.7f, 0.28f).x, (int)P(0.7f, 0.28f).y, tone(IRON, 2)); // a helm
        c.ell(P(0.5f, 0.7f).x, P(0.5f, 0.7f).y, (x1 - x0) * 0.22f, (y1 - y0) * 0.22f, C(170, 120, 60)); // a beard
        c.px((int)P(0.42f, 0.4f).x, (int)P(0.42f, 0.4f).y, NIGHT[0]); c.px((int)P(0.58f, 0.4f).x, (int)P(0.58f, 0.4f).y, NIGHT[0]);
        c.rect(x0, y1 - 3, x1, y1, tone(CRIM, 1));
    }
    else if (k == 3)
    {
        c.ball(P(0.5f, 0.6f).x, P(0.5f, 0.6f).y, (x1 - x0) * 0.2f, (y1 - y0) * 0.16f, CLAY, 2); // the body
        c.line(P(0.62f, 0.55f).x, P(0.62f, 0.55f).y, P(0.7f, 0.3f).x, P(0.7f, 0.3f).y, tone(CLAY, 2), 2);
        for (int s : {-1, 1}) c.line(P(0.7f, 0.3f).x, P(0.7f, 0.3f).y, P(0.7f + s * 0.1f, 0.12f).x, P(0.7f + s * 0.1f, 0.12f).y, LINEN[3]);
        for (float l : {0.38f, 0.46f, 0.56f, 0.62f}) c.vline((int)P(l, 0).x, (int)P(l, 0.7f).y, (int)P(l, 0.92f).y, tone(CLAY, 1));
    }
    else if (k == 4)
    {
        for (int i = 0; i < 3; i++)
        {
            float ux = 0.25f + i * 0.25f;
            c.vline((int)P(ux, 0).x, (int)P(ux, 0.2f).y, (int)P(ux, 0.8f).y, tone(BRASS, 3));
            c.line(P(ux, 0.3f).x, P(ux, 0.3f).y, P(ux + 0.1f, 0.5f).x, P(ux + 0.1f, 0.5f).y, tone(BRASS, 3));
            c.line(P(ux, 0.5f).x, P(ux, 0.5f).y, P(ux - 0.1f, 0.7f).x, P(ux - 0.1f, 0.7f).y, tone(BRASS, 3));
        }
    }
    else
    {
        c.disc(P(0.8f, 0.2f).x, P(0.8f, 0.2f).y, 2.2f, C(250, 236, 170)); // the sun over the hills
    }
}

void tool(Cv& c, int var)
{ // a tool on a peg at the top: axe, saw, sickle, hammer, drinking horn
    c.rect(8, 0, 9, 1, tone(WOOD, 1));
    const Color haft = WOOD[3], hd = WOOD[2];
    switch (var % 5)
    {
    case 0: // an axe, head up
        c.vline(8, 2, 28, haft); c.vline(9, 2, 28, hd); c.vline(7, 2, 28, tone(WOOD, 4));
        c.rect(10, 2, 15, 6, IRON[3]); c.rect(10, 7, 14, 9, IRON[3]); c.rect(15, 3, 16, 8, IRON[4]); c.hline(10, 15, 6, IRON[2]); c.rect(7, 4, 9, 5, IRON[1]);
        break;
    case 1: // a saw
        c.rect(6, 2, 10, 8, haft); c.rect(7, 4, 9, 5, NIGHT[1]);
        for (int k = 0; k < 22; k++) { c.px(7, 9 + k, IRON[3]); c.px(8, 9 + k, IRON[2]); c.px(9, 9 + k, IRON[1]); if (k % 2) c.px(10, 9 + k, IRON[1]); }
        break;
    case 2: // a sickle
        c.vline(8, 2, 14, haft); c.vline(9, 2, 14, hd);
        for (int a = 0; a < 26; a++) { float t = a / 25.0f * 3.2f; c.px(8 + (int)std::lround(std::sin(t) * 7), 15 + (int)std::lround((1 - std::cos(t)) * 6), a > 20 ? IRON[1] : IRON[3]); c.px(8 + (int)std::lround(std::sin(t) * 7), 16 + (int)std::lround((1 - std::cos(t)) * 6), IRON[2]); }
        break;
    case 3: // a hammer
        c.vline(8, 2, 24, haft); c.vline(9, 2, 24, hd); c.rect(4, 22, 14, 28, IRON[3]); c.hline(4, 14, 22, IRON[4]); c.hline(4, 14, 28, IRON[1]); c.vline(14, 22, 28, IRON[2]);
        break;
    default: // a drinking horn on its strap
        c.line(3, 2, 8, 10, tone(WOOD, 1), 1); c.line(15, 2, 10, 10, tone(WOOD, 1), 1);
        for (int k = 0; k < 18; k++)
        {
            int w = 1 + (k < 12) + (k < 6), x = 2 + k, y = 9 + (int)(k * k * 0.03f);
            for (int t = 0; t < w + 1; t++) c.px(x, y + t, k < 3 ? LINEN[4] : tone(CLAY, 4 - k / 5 + (t == 0)));
        }
        c.rect(1, 9, 2, 12, BRASS[3]);
        break;
    }
}

void shelf(Cv& c, int var)
{ // a plank shelf on brackets, with bowls, jars, a wheel of cheese and a couple of books
    int W = c.w;
    plank(c, 0, 12, W - 1, 14, true, 31 + var, 3);
    c.hline(1, W - 2, 15, tone(WOOD, 0));
    for (int k = 0; k < 6; k++) { c.hline(2, 2 + k / 2 + 1, 15 + k, tone(WOOD, 2 - k / 3)); c.hline(W - 3 - k / 2 - 1, W - 3, 15 + k, tone(WOOD, 1)); }
    for (int x = 3; x < W - 8;)
    {
        int k = (int)(R(x, var, 33) * 5);
        if (k == 0) { c.ball(x + 3.5f, 10.5f, 3.6f, 2.2f, LINEN, 2); c.hline(x, x + 7, 9, tone(LINEN, 4)); x += 9; }                                      // a bowl
        else if (k == 1) { c.rect(x, 5, x + 4, 11, tone(FOREST, 2)); c.vline(x, 5, 11, tone(FOREST, 4)); c.vline(x + 4, 5, 11, tone(FOREST, 1)); c.rect(x + 1, 3, x + 3, 4, tone(WOOD, 2)); x += 8; } // a jar
        else if (k == 2) { c.ball(x + 4, 9, 4.5f, 2.6f, OCHRE, 2); c.hline(x + 1, x + 7, 10, tone(OCHRE, 0)); c.px(x + 5, 8, tone(OCHRE, 1)); x += 10; }     // a cheese
        else if (k == 3) { for (int b = 0; b < 3; b++) c.slab(x + b * 3, 4 + b % 2, x + b * 3 + 2, 11, b == 1 ? NAVY : CRIM, 2); x += 11; }                  // books
        else { c.ball(x + 2.5f, 9, 2.8f, 3.4f, CLAY, 2); c.rect(x + 2, 4, x + 3, 5, tone(CLAY, 2)); x += 7; }                                              // a jug
    }
}

void spear(Cv& c, int x, int top, int bottom, int lean = 0, int dim = 0)
{ // a spear, its head `lean` cells off the vertical; dim 1 = standing behind the others (darker, so the rack has depth)
    auto X = [&](int y) { return x + (int)std::lround(lean * (float)(bottom - y) / std::max(1, bottom - top)); };
    for (int y = top + 8; y <= bottom; y++) { c.px(X(y), y, tone(WOOD, 3 - dim)); c.px(X(y) + 1, y, tone(WOOD, 2 - dim)); }
    for (int k = 0; k < 9; k++) { int hw = k < 5 ? k / 2 : (8 - k) / 2, xx = X(top + 8 - k); c.hline(xx - hw, xx + 1 + hw, top + 8 - k, tone(IRON, (k == 4 ? 4 : 3) - dim)); c.px(xx + 1 + hw, top + 8 - k, tone(IRON, 2 - dim)); }
    c.px(X(top), top, tone(IRON, 4 - dim)); c.px(X(top), top + 1, tone(IRON, 3 - dim)); // the point
}

void rack(Cv& c, int var)
{ // a weapon rack: spears stood in it, an axe and a sword hung from the rails, a round shield beside them
    int W = c.w, H = c.h;
    for (int sx : {0, W - 3}) plank(c, sx, 6, sx + 2, H - 1, false, 41 + var, 2);
    for (int ry : {H - 30, H - 14}) { plank(c, 0, ry, W - 1, ry + 2, true, 47 + var + ry, 3); c.hline(1, W - 2, ry + 3, tone(WOOD, 0)); }
    int n = std::max(2, (W - 22) / 7);
    for (int pass = 0; pass < 2; pass++) // the back row leans the other way, darker, behind the front
        for (int i = pass; i < n; i += 2) spear(c, 6 + i * 6, i % 2 ? 5 : 1, H - 4, (i % 3 - 1) * 2 + (pass ? 1 : 0), pass == 0 ? 1 : 0);
    int ax = 6 + n * 6 + 1; // an axe leaning on the rail
    c.vline(ax, H - 38, H - 4, WOOD[3]); c.vline(ax + 1, H - 38, H - 4, WOOD[2]);
    c.rect(ax + 2, H - 38, ax + 7, H - 32, IRON[3]); c.rect(ax + 2, H - 33, ax + 5, H - 30, IRON[2]); c.vline(ax + 8, H - 37, H - 33, IRON[4]);
    int sw = ax + 5; // a sword hung from the top rail
    if (sw < W - 20)
    {
        c.vline(sw + 3, H - 28, H - 5, IRON[4]); c.vline(sw + 4, H - 28, H - 5, IRON[2]); c.px(sw + 3, H - 4, IRON[3]);
        c.hline(sw, sw + 7, H - 29, BRASS[3]); c.rect(sw + 3, H - 36, sw + 4, H - 30, tone(WOOD, 1)); c.disc(sw + 3.5f, H - 37, 1.4f, BRASS[2]);
    }
    for (int y = 0; y < 18; y++) for (int x = 0; x < 18; x++) { float dx = x - 8.5f, dy = y - 8.5f; if (dx * dx + dy * dy > 81) continue; } // (the shield is its own decor)
}

void arrows(Cv& c, int var)
{ // a wicker basket of arrows, fletched red and white
    for (int k = 0; k < 7; k++)
    {
        float a = -0.55f + k * 0.18f;
        int x = 11 + (int)(std::sin(a) * 3), y = 6;
        for (int t = 0; t < 18; t++) c.px(11 + (int)std::lround(std::sin(a) * (2 + t * 0.9f)), 22 - t, t > 14 ? IRON[3] : (k % 2 ? WOOD[3] : WOOD[2]));
        int fx = 11 + (int)std::lround(std::sin(a) * 4.4f), fy = 22 - 4;
        c.px(fx, fy, k % 3 ? C(220, 220, 214) : CRIM[3]); c.px(fx + 1, fy + 1, CRIM[3]); c.px(fx - 1, fy + 1, k % 3 ? C(190, 190, 184) : CRIM[2]);
        (void)x; (void)y;
    }
    for (int y = 22; y < 34; y++)
        for (int x = 1; x < 21; x++)
        {
            int inset = (y - 22) / 6;
            if (x < 1 + inset || x > 20 - inset) continue;
            bool weave = ((x + y / 2) % 3 == 0) != (y % 2 == 0);
            c.px(x, y, tone(STRAW, weave ? 2 : 1 + (R(x, y, 55 + var) > 0.8f)));
        }
    c.hline(1, 20, 22, STRAW[3]); c.hline(1, 20, 23, STRAW[0]);
    c.hline(2, 19, 33, STRAW[0]);
}

void bunk(Cv& c, int var)
{ // bunks against the wall: straw mattress, a blanket roll and pillow on each
    int W = c.w, H = c.h;
    bool two = H >= 34;
    for (int px = 0; px < 2; px++) plank(c, px ? W - 3 : 0, 0, px ? W - 1 : 2, H - 1, false, 61 + var + px, 2);
    int levels[2] = {H - 10, 12};
    for (int l = 0; l < (two ? 2 : 1); l++)
    {
        int by = levels[l];
        plank(c, 0, by, W - 1, by + 2, true, 67 + var + l, 3); c.hline(1, W - 2, by + 3, tone(WOOD, 0));
        for (int y = by - 5; y < by; y++)
            for (int x = 3; x < W - 3; x++) c.px(x, y, tone(STRAW, 1 + (y == by - 5) * 2 + (R(x / 2, y, 69 + l) > 0.7f)));
        const Ramp& bl = CLOTHS[(var + l) % 3];
        c.slab(5, by - 8, 14, by - 4, LINEN, 3); c.hline(6, 13, by - 7, tone(LINEN, 4));       // the pillow
        for (int x = 17; x < W - 5; x++) for (int y = by - 7; y < by - 2; y++) c.px(x, y, tone(bl, 2 + (y == by - 7) - (y == by - 3) + (x % 6 == 0 ? -1 : 0))); // the rolled blanket
        c.ball(W - 6, by - 4.5f, 2.6f, 2.6f, bl, 2);
    }
}

void hearth(Cv& c, int H)
{ // a fieldstone fireplace: a broad hearth, a chimney breast running up to the beams, a mantel, a fire in the mouth
    int W = c.w;
    auto stoneBlock = [&](int x0, int y0, int x1, int y1) {
        for (int y = y0; y <= y1; y++)
            for (int x = x0; x <= x1; x++)
            {
                int row = (y - y0) / 8, ry = (y - y0) % 8, off = (int)(R(row, 3, 71) * 9) + (row & 1) * 5, u = (x - x0 + off) % 12, st = (x - x0 + off) / 12;
                if (ry == 7 || u == 11 || ((u == 0 || u == 10) && (ry == 0 || ry == 6))) { c.px(x, y, STONE[0]); continue; }
                int t = 2 + (R(st, row + y0, 73) > 0.6f) - (R(st, row + y0, 74) > 0.8f) + (ry < 2 || u < 2) - (ry > 5 || u > 9);
                c.px(x, y, tone(STONE, t));
            }
    };
    int cx = W / 2;
    stoneBlock(cx - 12, 0, cx + 12, H - 29);          // the chimney breast
    stoneBlock(0, H - 28, W - 1, H - 1);              // the hearth block
    plank(c, 2, H - 31, W - 3, H - 29, true, 77, 4);  // the mantel, a heavy beam
    c.hline(3, W - 4, H - 28, tone(WOOD, 0));
    int x0 = cx - 13, x1 = cx + 13, y0 = H - 22;
    for (int y = y0; y < H - 3; y++)                   // the opening, arched, black inside
        for (int x = x0; x <= x1; x++)
        {
            float dx = (x - cx) / 13.5f, top = 1 - std::sqrt(std::max(0.0f, 1 - dx * dx)) * 0.5f;
            if (y < y0 + (int)(top * 6 - 2)) continue;
            c.px(x, y, (y > H - 8) ? C(34, 20, 14) : C(14, 10, 10));
        }
    for (int y = H - 22; y < H - 8; y++) for (int x = cx - 12; x <= cx + 12; x++) if (c.on(x, y) && y < H - 16 && R(x, y, 79) > 0.55f) c.px(x, y, C(24, 20, 20)); // soot
    for (int k = 0; k < 2; k++) c.line((float)cx - 8, H - 5 - k * 2, (float)cx + 8, H - 9 + k * 3, tone(WOOD, 1 + k), 2); // crossed logs
    for (int f = 0; f < 5; f++) // flames
    {
        int fx = cx - 8 + f * 4, fh = 7 + (int)(R(f, 5, 81) * 5);
        for (int k = 0; k < fh; k++)
        {
            int hw = k < fh / 2 ? 1 : 0;
            Color col = k > fh * 2 / 3 ? C(255, 232, 140) : (k > fh / 3 ? C(255, 170, 50) : C(214, 70, 24));
            for (int d = -hw; d <= hw; d++) c.px(fx + d, H - 9 - k, col);
        }
    }
    for (int x = cx - 10; x <= cx + 10; x++) if (R(x, 4, 83) > 0.5f) c.px(x, H - 5, C(236, 90, 30)); // embers
    for (int s = 0; s < 2; s++) // things on the mantel
    {
        int mx = s ? cx + 14 : cx - 16;
        if (s) { c.ball(mx + 2, H - 36, 2.4f, 3, CLAY, 2); c.rect(mx + 1, H - 41, mx + 2, H - 38, tone(CLAY, 2)); }
        else { c.rect(mx, H - 38, mx + 1, H - 32, tone(IRON, 3)); c.rect(mx, H - 42, mx + 1, H - 39, LINEN[4]); c.px(mx, H - 44, C(255, 232, 150)); c.px(mx, H - 43, C(255, 170, 60)); }
    }
}

// One shield pixel at (dx, dy) from its centre, `R` cells to the rim; alpha 0 outside. Painted boards with seams, a leather-bound
// edge, an iron rim that catches the light on its upper left, and a domed boss with a spike that throws a small shadow on the
// field. Used flat on a wall and, tilted, by the leaning shield.
Color shieldFace(float dx, float dy, float rad, int var, int px, int py)
{
    static const Ramp* P[3][2] = {{&CRIM, &LINEN}, {&NAVY, &OCHRE}, {&FOREST, &LINEN}};
    const Ramp &a = *P[var % 3][0], &b = *P[var % 3][1];
    int design = (var / 3) % 4;
    float d2 = dx * dx + dy * dy, f = std::sqrt(d2) / rad;
    if (f > 1.0f) return Color{0, 0, 0, 0};
    float lit = (-dx - dy) / (2 * rad); // -0.5 .. 0.5, lit from the upper left
    int t = 2 + (lit > 0.16f) - (lit < -0.16f);
    if (f > 0.87f) return tone(IRON, f > 0.95f ? 1 + (lit > 0) : 3 - (lit < -0.1f)); // the rim: a bright upper edge, a dark lower one
    if (f > 0.8f) return tone(WOOD, lit > 0 ? 1 : 0); // leather binding
    if (f < 0.2f) return tone(IRON, f < 0.07f ? 4 : (lit > -0.05f ? 3 : 2)); // the boss and its spike
    if (f < 0.3f) return tone(IRON, lit > 0 ? 3 : 1);                         // the boss's slope
    bool first = design == 0 ? dx < 0 : (design == 1 ? (dx >= 0) == (dy >= 0) : (design == 2 ? std::fabs(dx) < rad * 0.13f || std::fabs(dy) < rad * 0.13f : ((int)std::floor((dx + rad * 3) / (rad * 0.45f))) % 2 == 0));
    Color col = tone(first ? a : b, t);
    float seam = std::fmod(dx + rad * 4, rad * 0.5f);
    if (seam < 0.75f && f < 0.78f) col = tone(first ? a : b, t - 1); // the joins between boards
    else if (rad > 5 && R(px, py / 4, 91) > 0.9f) col = tone(first ? a : b, t - 1); // grain
    if (dx + dy > 0 && f < 0.42f && f >= 0.3f) col = tone(first ? a : b, t - 1); // the boss's shadow on the field
    return col;
}

// A shield seen at an angle: turned about the vertical (yaw: the face narrows and its thickness shows on one side), tipped back (pitch),
// and rolled in the picture. The back disc is drawn first, offset, as the iron-bound edge; then the face on top.
void tiltedShield(Cv& c, int var, float cx, float cy, float rad, float yaw, float pitch, float roll, float thick)
{
    float cy_ = std::max(0.35f, std::cos(yaw)), cp = std::max(0.35f, std::cos(pitch)), cr = std::cos(roll), sr = std::sin(roll);
    float ex = -std::sin(yaw) * thick, ey = std::sin(pitch) * thick; // where the back edge peeks out
    for (int pass = 0; pass < 2; pass++)
        for (int y = 0; y < c.h; y++)
            for (int x = 0; x < c.w; x++)
            {
                float qx = x + 0.5f - cx - (pass == 0 ? ex : 0), qy = y + 0.5f - cy - (pass == 0 ? ey : 0);
                float u = (qx * cr + qy * sr) / cy_, v = (-qx * sr + qy * cr) / cp;
                if (pass == 0)
                {
                    if (u * u + v * v <= rad * rad) c.px(x, y, tone(IRON, (qx + qy) * 0.5f > 0 ? 0 : 1)); // the thickness: dark iron-bound edge
                    continue;
                }
                Color col = shieldFace(u, v, rad, var, x, y);
                if (col.a) c.px(x, y, col);
            }
}

void shield(Cv& c, int var)
{ // a round shield hung on the wall by its strap, turned a little to one side or the other so it has a thickness and a slant
    float side = (var * 37 % 7) < 3 ? -1.0f : 1.0f, yaw = side * (0.45f + 0.04f * (var % 5)), roll = ((var * 13) % 5 - 2) * 0.1f;
    tiltedShield(c, var, 11.0f, 11.0f, 8.8f, yaw, 0.12f, roll, 2.6f);
}

void leanShield(Cv& c, int var)
{ // a shield stood on the floor and leant back against the wall: strongly turned and tipped, its thickness showing, a shadow on the floor
    for (int x = 2; x < c.w - 2; x++)
        for (int y = c.h - 4; y < c.h; y++)
            if (std::pow((x - 14.0f) / 12.0f, 2) + std::pow((y - (c.h - 1.0f)) / 3.0f, 2) < 1) c.px(x, y, Color{10, 8, 12, 120});
    tiltedShield(c, var, 14.0f, c.h - 12.5f, 10.0f, 0.95f, 0.5f, -0.5f, 3.4f);
}

// A rack of longbows, dark with age, in three-quarter view: a backboard of planks, two thick posts with rails that show their top faces,
// bows hung in two rows at different slants (the back row darker and behind), a quiver of fletched arrows hung at the end.
void bowRack(Cv& c, int var)
{
    int W = c.w, H = c.h;
    auto box = [&](int x0, int y0, int x1, int y1, int t) { // a timber with a lit top face, a front, a shaded end
        c.rect(x0, y0 + 2, x1, y1, tone(WOOD, t));
        c.hline(x0, x1, y0 + 2, tone(WOOD, t + 1));
        for (int x = x0 + 1; x <= x1 + 2; x++) { c.px(x, y0, tone(WOOD, t + 1)); c.px(x, y0 + 1, tone(WOOD, t + 1)); }
        c.vline(x1, y0 + 2, y1, tone(WOOD, t - 1)); c.vline(x1 + 1, y0, y1 - 1, tone(WOOD, t - 2));
        for (int x = x0; x < x1; x++) if (R(x, y0 / 3, 71 + var) > 0.85f) c.px(x, y0 + 3, tone(WOOD, t - 1));
    };
    for (int y = 8; y < H - 4; y++) // the backboard: upright planks, dark, with seams and nail heads
        for (int x = 4; x < W - 5; x++)
        {
            int t = ((x - 4) % 7 == 0) ? 0 : (R(x, y / 6, 72) > 0.85f ? 1 : 0);
            c.px(x, y, tone(WOOD, t));
            if ((x - 4) % 7 == 2 && (y == 12 || y == H - 9)) c.px(x, y, tone(IRON, 2));
        }
    auto bow = [&](float bx, float top, float bot, float bulge, float slant, int dim, bool strung) {
        float half = (bot - top) / 2, my = (top + bot) / 2;
        for (int k = 0; k <= (int)(half * 4); k++)
        {
            float t = -1 + k / (half * 2.0f), x = bx + bulge * (1 - t * t) + slant * t, y = my + t * half;
            int xi = (int)std::lround(x), yi = (int)std::lround(y);
            bool grip = std::fabs(t) < 0.16f;
            c.px(xi, yi, grip ? tone(NIGHT, 3) : tone(OCHRE, 3 - 2 * dim)); c.px(xi + 1, yi, grip ? tone(NIGHT, 2) : tone(OCHRE, 1 - dim));
        }
        if (strung) c.line(bx + slant * -1, top, bx + slant, bot, tone(LINEN, 2 - dim), 1);
        c.px((int)std::lround(bx - slant), (int)top, tone(BRASS, 2 - dim)); c.px((int)std::lround(bx + slant), (int)bot, tone(BRASS, 2 - dim)); // horn nocks
    };
    box(0, H - 30, W - 8, H - 24, 2); box(0, H - 14, W - 8, H - 8, 2); // two rails across the front
    int n = std::max(3, (W - 14) / 7);
    for (int i = 0; i < 2; i++) bow(10.0f + i * 8, 10, H - 9, 2.2f, 1.5f * (i ? 1 : -1), 1, false);                   // back row: dim, unstrung, slanting the other way
    for (int i = 0; i < n; i++) bow(8.0f + i * 7, 6 + i % 2 * 2, H - 7, 3.2f + (i % 2), (i % 3 - 1) * 1.6f, 0, i % 2 == 0); // front row
    for (int sx : {0, W - 12}) { c.rect(sx, 4, sx + 4, H - 1, tone(WOOD, 2)); c.vline(sx, 4, H - 1, tone(WOOD, 3)); c.vline(sx + 4, 4, H - 1, tone(WOOD, 1)); c.vline(sx + 5, 5, H - 2, tone(WOOD, 0)); c.hline(sx, sx + 5, 4, tone(WOOD, 3)); }
    int qx = W - 7; // the quiver, hung on the end post: a leather tube lit on its left, arrows fanned out of the top
    for (int y = 16; y < 40; y++) for (int dx = 0; dx < 6; dx++) c.px(qx + dx, y, tone(CLAY, dx < 2 ? 2 : (dx < 4 ? 1 : 0)));
    for (int y : {20, 30}) c.hline(qx, qx + 5, y, tone(WOOD, 0));
    for (int a = 0; a < 5; a++)
    {
        int ax = qx + 1 + a, top = 6 + (a * 3 + var) % 5;
        for (int y = top; y < 18; y++) c.px(ax, y, tone(WOOD, y % 4 ? 3 : 2));
        for (int y = top; y < top + 4; y++) { c.px(ax - 1, y, tone(a % 2 ? LINEN : CRIM, 3)); c.px(ax + 1, y, tone(a % 2 ? LINEN : CRIM, 2)); }
    }
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) if (c.on(x, y)) { Color& p = c.d[(size_t)y * W + x]; p.r = (unsigned char)(p.r * 0.8f); p.g = (unsigned char)(p.g * 0.8f); p.b = (unsigned char)(p.b * 0.84f); } // aged and dark
}

// An archery butt seen from the side, dark with age: a coiled-straw drum turned away from us so its thickness shows, rings on its face,
// arrows standing out of it at the shooter's end, on a timber tripod, with a wall of hay bales behind as the backstop.
void archeryTarget(Cv& c, int var)
{
    const float cx = 20.0f, cy = 21.0f, rad = 15.0f, yaw = -1.0f, thick = 7.0f;
    // the backstop: two hay bales, bound with twine, behind and below
    for (int row = 0; row < 2; row++)
        for (int y = c.h - 9 - row * 9; y < c.h - row * 9; y++)
            for (int x = 24 - row * 3; x < 52 - row * 3 && x < c.w; x++)
            {
                int t = 2 + (y == c.h - 9 - row * 9) - (y == c.h - 1 - row * 9) - ((x == 24 - row * 3 || (x + row) % 9 == 0) ? 1 : 0);
                Color col = tone(STRAW, R(x, y, 51) > 0.8f ? t - 1 : (R(x / 2, y, 52) > 0.7f ? t + 1 : t));
                if ((x + y / 2) % 7 == 0 && y % 4 != 0) col = tone(STRAW, 0); // straws lying across
                if (y == c.h - 5 - row * 9 || y == c.h - 4 - row * 9) col = tone(WOOD, x % 3 ? 1 : 0); // the twine
                c.px(x, y, col);
            }
    for (int leg : {-1, 1}) // the tripod: two legs to the front, one brace behind
        for (int y = (int)cy + 8; y < c.h; y++)
        {
            float t = (y - cy - 8) / (c.h - cy - 8);
            int x = (int)std::lround(cx + 3 + leg * (4 + 9 * t));
            c.px(x, y, tone(WOOD, 2)); c.px(x + 1, y, tone(WOOD, 0));
        }
    for (int y = (int)cy + 8; y < c.h - 2; y++) { int x = (int)std::lround(cx + 12 + (y - cy) * 0.35f); c.px(x, y, tone(WOOD, 1)); c.px(x + 1, y, tone(WOOD, 0)); }
    float cyw = std::cos(yaw), cp = 1.0f;
    float ex = -std::sin(yaw) * thick;
    for (int pass = 0; pass < 2; pass++)
        for (int y = 0; y < c.h; y++)
            for (int x = 0; x < c.w; x++)
            {
                float qx = x + 0.5f - cx - (pass == 0 ? ex : 0), qy = y + 0.5f - cy;
                float u = qx / cyw, v = qy / cp, f = std::sqrt(u * u + v * v) / rad;
                if (f > 1) continue;
                if (pass == 0) { c.px(x, y, tone(STRAW, (y % 3 == 0) ? 0 : ((qx + qy) * 0.5f > 0 ? 0 : 1))); continue; } // coils of straw round the drum
                float lit = (-u - v) / (2 * rad);
                int t = 2 + (lit > 0.2f) - (lit < -0.2f);
                Color col;
                if (f > 0.9f) col = tone(STRAW, 0);                                    // the rim binding
                else if (f > 0.74f) col = tone(NIGHT, std::max(1, t));                 // black outer ring
                else if (f > 0.56f) col = tone(LINEN, t - 1);                          // white
                else if (f > 0.36f) col = tone(CRIM, t - 1);                           // red
                else if (f > 0.16f) col = tone(LINEN, t - 1);                          // white
                else col = tone(OCHRE, t);                                             // the gold
                if (R(x, y, 53) > 0.88f) col = tone(STRAW, 0);                         // chewed by arrows
                c.px(x, y, col);
            }
    auto face = [&](float f, float a, float& sx, float& sy) { float u = std::cos(a) * f * rad, v = std::sin(a) * f * rad; sx = cx + u * cyw; sy = cy + v; };
    int n = 4 + var % 3;
    for (int i = 0; i < n; i++)
    { // arrows: driven into the face, shafts standing out toward the shooter on the left with feathers at the end
        float sx, sy; face(0.1f + 0.78f * R(i, var, 61), R(var, i, 62) * 6.28f, sx, sy);
        float len = 9 + R(i, 2, 63) * 5, sl = (R(i, 3, 64) - 0.5f) * 0.5f;
        for (int k = 0; k < (int)len; k++)
        {
            int x = (int)std::lround(sx - k), y = (int)std::lround(sy + k * sl);
            c.px(x, y, tone(WOOD, k % 5 == 0 ? 2 : 3)); c.px(x, y + 1, tone(WOOD, 1));
            if (k >= (int)len - 4 && k < (int)len - 1) { c.px(x, y - 1, tone(i % 2 ? LINEN : CRIM, 3)); c.px(x, y + 2, tone(i % 2 ? LINEN : CRIM, 2)); } // the fletching
        }
    }
    float sx, sy; face(0.9f, 2.4f, sx, sy); // one has missed and stands in the bale behind
    for (int k = 0; k < 11; k++) { c.px((int)sx + 10 - k, c.h - 14 + k / 6, tone(WOOD, k % 4 ? 2 : 3)); }
    for (int y = 0; y < c.h; y++) for (int x = 0; x < c.w; x++) if (c.on(x, y)) { Color& p = c.d[(size_t)y * c.w + x]; p.r = (unsigned char)(p.r * 0.78f); p.g = (unsigned char)(p.g * 0.78f); p.b = (unsigned char)(p.b * 0.82f); } // aged and dark
}

// A spear driven point-first into the ground: only the butt end of the shaft stands up (10 cells wide, `H` tall, foot at the bottom).
// The cloth on it is live (rig.cpp:drawDecor), tied at the lashing under the butt cap.
void spearPost(Cv& c, int var, int H)
{
    int sx = 4;
    for (int y = 2; y < H - 3; y++) // the shaft, rounded: lit left, shaded right, a knot now and then
    {
        c.px(sx, y, tone(WOOD, R(1, y / 6, 93 + var) > 0.8f ? 2 : 3)); c.px(sx + 1, y, tone(WOOD, 1));
        if (y > 8 && y < H - 8 && R(2, y / 9, 95 + var) > 0.93f) c.px(sx, y, tone(WOOD, 4)); // a worn pale patch where hands have held it
    }
    for (int dx = 0; dx < 2; dx++) { c.px(sx + dx, 0, tone(IRON, dx ? 2 : 4)); c.px(sx + dx, 1, tone(IRON, dx ? 1 : 3)); } // the iron butt cap
    c.px(sx - 1, 1, tone(IRON, 2)); c.px(sx + 2, 1, tone(IRON, 0));
    for (int y = 4; y < 8; y++) for (int dx = -1; dx < 3; dx++) c.px(sx + dx, y, tone(LINEN, (y + dx) % 2 ? 1 : 2)); // the lashing the cloth ties to
    for (int y = H - 6; y < H; y++) // the mound it's driven into: earth, a stone or two, tufts
        for (int x = 0; x < 10; x++)
        {
            float k = 1 - std::fabs(x - 4.5f) / 5.0f;
            if ((H - y) <= k * 6 + 0.5f) c.px(x, y, (x < 4.5f ? tone(CLAY, 1) : tone(CLAY, 0)));
        }
    c.px(2, H - 3, tone(STONE, 3)); c.px(3, H - 3, tone(STONE, 2)); c.px(7, H - 2, tone(STONE, 2));
    c.px(1, H - 3, tone(FOREST, 3)); c.px(8, H - 3, tone(FOREST, 2)); c.px(6, H - 5, tone(FOREST, 3));
}

void post(Cv& c, int /*var*/)
{ // a carved post: zig-zag down its face, a block capital and a plinth. 6 px wide
    int H = c.h;
    for (int y = 4; y < H - 3; y++)
    {
        int zz = (y / 3) % 4; bool cut = (zz == 0 && false);
        (void)cut;
        c.px(0, y, WOOD[4]); c.px(1, y, WOOD[3]); c.px(2, y, WOOD[2]); c.px(3, y, WOOD[2]); c.px(4, y, WOOD[1]); c.px(5, y, WOOD[0]);
        int zx = std::abs((y % 8) - 4) / 2 + 1; // the engraved zig-zag
        c.px(zx, y, WOOD[0]); c.px(zx + 1, y, WOOD[1]);
    }
    c.slab(0, 0, 5, 3, WOOD, 3); c.hline(0, 5, 4, WOOD[0]);
    c.slab(0, H - 3, 5, H - 1, WOOD, 2);
}

void ladder(Cv& c, int var)
{ // rails and rungs, 14 px wide
    int H = c.h;
    for (int y = 0; y < H; y++) { c.px(0, y, WOOD[3]); c.px(1, y, WOOD[2]); c.px(12, y, WOOD[2]); c.px(13, y, WOOD[1]); }
    for (int y = 4; y < H - 1; y += 8) { c.rect(2, y, 11, y + 1, tone(WOOD, 3 + (R(y, var, 89) > 0.5f))); c.hline(2, 11, y + 2, WOOD[0]); }
}

void yard(Cv& c, int var)
{ // firewood stacked log-ends out, a bound bale of hay, or a heap of sacks
    int k = var % 3;
    if (k == 0)
        for (int row = 0; row < 3; row++)
            for (int i = 0; i < 4 - row % 2 - row / 2; i++)
            {
                float cx = 4.5f + i * 6.5f + (row % 2) * 3.2f + row * 0.6f, cy = 22 - 4 - row * 6;
                c.disc(cx, cy, 3.3f, WOOD[1]);
                c.disc(cx - 0.4f, cy - 0.4f, 2.5f, C(176, 134, 86));
                c.disc(cx - 0.4f, cy - 0.4f, 1.2f, C(150, 108, 66));
                c.px((int)cx, (int)cy, WOOD[2]);
            }
    else if (k == 1)
    {
        for (int y = 2; y < 18; y++)
            for (int x = 0; x < 22; x++)
            {
                if ((y < 4 || y > 15) && (x < 2 || x > 19)) continue;
                int t = 2 + (R(x / 2, y, 91 + var) > 0.6f) - (R(x, y / 2, 92) > 0.8f) + (y < 4) - (y > 14) + (x < 2) - (x > 19);
                c.px(x, y, tone(STRAW, t));
            }
        for (int bx : {6, 14}) { c.vline(bx, 2, 17, WOOD[1]); c.vline(bx + 1, 2, 17, WOOD[2]); }
    }
    else
        for (int s = 0; s < 3; s++)
        {
            float cx = 6 + s * 7.5f, cy = 15 - (s == 1) * 6;
            c.ball(cx, cy, 5.2f, 5.6f, LINEN, 2);
            c.hline((int)cx - 2, (int)cx + 2, (int)cy - 5, tone(LINEN, 1)); c.px((int)cx, (int)cy - 6, WOOD[2]);
        }
}

// ------------------------------------------------------------------------------------------------ the castle's cloth, chains and blood

// the hangings' designs, drawn into the field fx0..fx1 x fy0..fy1 of a tapestry
// A raised, stitched stroke along a list of points: a dark shadow dropped down-right, the body in the ramp's mid tone, a lit edge on the upper left.
struct Pt { float x, y, r; };
void stitched(Cv& c, const std::vector<Pt>& pts, const Ramp& ramp, Color shadow, int body = 2)
{
    for (auto& p : pts) c.disc(p.x + 0.9f, p.y + 0.9f, p.r, shadow);
    for (auto& p : pts) c.disc(p.x, p.y, p.r, tone(ramp, body));
    for (auto& p : pts) if (p.r > 1.2f) c.disc(p.x - 0.45f, p.y - 0.45f, p.r * 0.62f, tone(ramp, body + 1));
    for (auto& p : pts) if (p.r > 1.5f) c.px((int)std::floor(p.x - p.r * 0.45f), (int)std::floor(p.y - p.r * 0.45f), tone(ramp, 4));
}

void motif(Cv& c, int design, int fx0, int fy0, int fx1, int fy1, const Ramp& field)
{
    float cx = (fx0 + fx1) * 0.5f, cy = (fy0 + fy1) * 0.5f, w = (float)(fx1 - fx0), h = (float)(fy1 - fy0);
    Color gold = BRASS[3], goldD = BRASS[1], pale = LINEN[3], dark = tone(field, 0);
    switch (design)
    {
    case 0: // the tree of life: a trunk, boughs, roots, a ring of knotwork round it
        c.vline((int)cx, (int)(cy - h * 0.3f), (int)(cy + h * 0.3f), tone(WOOD, 3)); c.vline((int)cx + 1, (int)(cy - h * 0.3f), (int)(cy + h * 0.3f), tone(WOOD, 2));
        for (int s : {-1, 1})
            for (int b = 0; b < 3; b++)
            {
                float by = cy - h * 0.2f + b * h * 0.12f;
                c.line(cx, by, cx + s * w * (0.38f - b * 0.06f), by - h * 0.12f, tone(WOOD, 3), 1);
                c.disc(cx + s * w * (0.38f - b * 0.06f), by - h * 0.14f, 1.8f, tone(FOREST, 3));
                c.line(cx, cy + h * 0.28f, cx + s * w * (0.2f + b * 0.1f), cy + h * 0.4f, tone(WOOD, 2), 1);
            }
        c.disc(cx, cy - h * 0.34f, 3, tone(FOREST, 3));
        break;
    case 1: // a raven with spread wings before a pale disc
        c.ball(cx, cy, std::min(w, h) * 0.38f, std::min(w, h) * 0.38f, LINEN, 3); // a full moon, shaded, with craters and a bright rim
        for (int k = 0; k < 7; k++) { float ang = k * 2.4f, rr = std::min(w, h) * 0.38f * (0.2f + 0.5f * R(k, design, 41)); c.disc(cx + std::cos(ang) * rr, cy + std::sin(ang) * rr, 1.2f + (k % 2), tone(LINEN, 1)); }
        for (int s : {-1, 1}) { c.line(cx, cy, cx + s * w * 0.34f, cy - h * 0.12f, NIGHT[1], 3); c.line(cx, cy + 1, cx + s * w * 0.3f, cy + h * 0.06f, NIGHT[2], 2); }
        c.ell(cx, cy + 1, 2.4f, 3.6f, NIGHT[1]); c.disc(cx, cy - 3, 1.8f, NIGHT[1]); c.px((int)cx + 1, (int)cy - 5, gold); c.px((int)cx + 2, (int)cy - 4, gold);
        break;
    case 2: // a stag's head, antlers spread: raised cream thread with tines
    {
        std::vector<Pt> an;
        for (int sd : {-1, 1})
        {
            for (int i = 0; i <= 14; i++) { float t = i / 14.0f; an.push_back({cx + sd * (2.0f + t * w * 0.32f), cy - h * 0.04f - t * h * 0.28f - std::sin(t * 3.0f) * 1.2f, 1.5f - 0.5f * t}); }
            for (int tn = 0; tn < 4; tn++)
            {
                float t0 = 0.25f + tn * 0.2f, bx = cx + sd * (2.0f + t0 * w * 0.32f), by = cy - h * 0.04f - t0 * h * 0.28f;
                for (int i = 0; i <= 4; i++) an.push_back({bx + sd * i * 0.5f, by - i * 1.5f, 1.0f - 0.12f * i});
            }
        }
        stitched(c, an, LINEN, tone(field, 0), 2);
        c.ell(cx + 0.8f, cy + h * 0.14f + 0.8f, w * 0.15f, h * 0.21f, tone(field, 0));
        c.ball(cx, cy + h * 0.12f, w * 0.15f, h * 0.21f, CLAY, 2); // the head, shaded
        c.px((int)cx - 2, (int)cy + 1, NIGHT[0]); c.px((int)cx + 2, (int)cy + 1, NIGHT[0]); c.px((int)cx, (int)(cy + h * 0.3f), NIGHT[1]);
        break;
    }
    case 3: // a serpent knotted through itself
        for (int i = 0; i < 60; i++)
        {
            float t = i / 59.0f, x = cx + std::sin(t * 9) * w * 0.28f, y = fy0 + 3 + t * (h - 6);
            c.disc(x, y, 1.6f, i % 2 ? gold : goldD);
        }
        c.disc(cx + std::sin(9) * w * 0.28f, fy1 - 3, 2.4f, gold); c.px((int)(cx + std::sin(9) * w * 0.28f) + 1, fy1 - 4, CRIM[3]);
        break;
    case 4: // a sun with a ring of rays
        c.disc(cx, cy, std::min(w, h) * 0.2f, gold); c.disc(cx, cy, std::min(w, h) * 0.13f, OCHRE[3]);
        for (int r = 0; r < 12; r++) { float a = r * 0.5236f; c.line(cx + std::cos(a) * w * 0.26f, cy + std::sin(a) * w * 0.26f, cx + std::cos(a) * w * 0.42f, cy + std::sin(a) * w * 0.42f, gold, 1); }
        break;
    case 5: // a longship under a striped sail
        c.line(cx - w * 0.36f, cy + h * 0.2f, cx + w * 0.36f, cy + h * 0.2f, tone(WOOD, 2), 2);
        c.line(cx - w * 0.36f, cy + h * 0.2f, cx - w * 0.42f, cy + h * 0.08f, tone(WOOD, 3), 1); c.line(cx + w * 0.36f, cy + h * 0.2f, cx + w * 0.42f, cy + h * 0.08f, tone(WOOD, 3), 1);
        c.vline((int)cx, (int)(cy - h * 0.3f), (int)(cy + h * 0.2f), tone(WOOD, 3));
        for (int y = (int)(cy - h * 0.26f); y < (int)(cy + h * 0.1f); y++) for (int x = (int)(cx - w * 0.28f); x <= (int)(cx + w * 0.28f); x++) c.px(x, y, ((x - (int)cx + 40) / 4) % 2 ? tone(CRIM, 3) : pale);
        for (int x = (int)(cx - w * 0.4f); x < (int)(cx + w * 0.4f); x += 3) c.px(x, (int)(cy + h * 0.3f) + (x / 3) % 2, tone(NAVY, 4));
        for (int k = -2; k <= 2; k++) { float sx = cx + k * w * 0.12f, sy = cy + h * 0.17f; c.disc(sx, sy, 1.5f, tone(k % 2 ? CRIM : OCHRE, 3)); c.px((int)sx, (int)sy, IRON[3]); } // shields along the gunwale
        c.disc(cx - w * 0.43f, cy + h * 0.05f, 1.6f, tone(WOOD, 3)); c.line(cx - w * 0.43f, cy + h * 0.05f, cx - w * 0.47f, cy - h * 0.05f, tone(WOOD, 4), 1); // a dragon's head on the prow
        for (int y = (int)(cy - h * 0.26f); y < (int)(cy + h * 0.1f); y += 3) for (int x = (int)(cx - w * 0.28f); x <= (int)(cx + w * 0.28f); x++) c.px(x, y, tone(NAVY, 0)); // the sail's reef lines
        break;
    case 6: // a triskele on black: three spiral arms curling from a boss, a beaded ring, all in raised cream thread
    {
        float R = std::min(w * 0.5f, h * 0.3f);
        Color sh = tone(field, 0);
        std::vector<Pt> arms, ring, beads;
        for (int k = 0; k < 3; k++)
            for (int i = 0; i <= 26; i++)
            {
                float t = i / 26.0f, a = k * 2.0944f - 0.9f + t * 2.3f, rho = R * (0.14f + 0.8f * std::pow(t, 0.9f));
                arms.push_back({cx + std::cos(a) * rho, cy + std::sin(a) * rho, 0.75f - 0.25f * t});
                if (i == 26) arms.push_back({cx + std::cos(a + 0.45f) * rho, cy + std::sin(a + 0.45f) * rho, 0.75f}); // the curl's end
            }
        for (int i = 0; i < 72; i++) { float a = i * 0.0873f; { if (i % 6 < 4) ring.push_back({cx + std::cos(a) * R * 1.12f, cy + std::sin(a) * R * 1.12f, 0.55f}); } }
        stitched(c, ring, LINEN, sh, 1);
        stitched(c, beads, LINEN, sh, 2);
        stitched(c, arms, LINEN, sh, 2);
        c.disc(cx + 0.7f, cy + 0.7f, R * 0.2f, sh); c.ball(cx, cy, R * 0.2f, R * 0.2f, LINEN, 3); c.px((int)cx - 1, (int)cy - 1, LINEN[4]); // the boss
        break;
    }
    default: // crossed swords behind a shield
        for (int s : {-1, 1}) { c.line(cx - s * w * 0.3f, cy - h * 0.3f, cx + s * w * 0.3f, cy + h * 0.3f, IRON[4], 1); c.line(cx - s * w * 0.3f + 1, cy - h * 0.3f, cx + s * w * 0.3f + 1, cy + h * 0.3f, IRON[2], 1); }
        c.disc(cx, cy, 5, tone(WOOD, 2)); c.disc(cx, cy, 3.4f, gold); c.disc(cx, cy, 1.4f, IRON[3]);
        break;
    }
    (void)dark;
}

void tapestry(Cv& c, int var, int H)
{ // a woven hanging from a rod, fringed and tasselled: twilled cloth in deep folds, a double gold border with a diamond band, a design in the field
    int W = c.w;
    int design = var % 8, ci = design == 6 ? 5 : (var >> 3) & 3; // black for the triskele, else red/blue/green/ochre
    const Ramp& cloth = CLOTHS[ci];
    int rodY = 1, top = 4, bot = H - 8;
    for (int x = 2; x < W - 2; x++) c.px(x, rodY, WOOD[4]), c.px(x, rodY + 1, WOOD[2]), c.px(x, rodY + 2, WOOD[0]); // the rod, lit on top
    for (int k = 0; k < 2; k++) { c.ball(1.5f + k * (W - 3), 2, 2.2f, 2.2f, BRASS, 3); c.px(k ? W - 3 : 2, 1, BRASS[4]); }  // finials
    for (int x = 5; x < W - 5; x += 7) { c.px(x, rodY + 3, BRASS[2]); c.px(x, rodY + 4, BRASS[1]); }                       // rings it hangs from
    for (int y = top; y <= bot; y++)
        for (int x = 3; x < W - 3; x++)
        {
            float fold = std::sin(x * 0.55f + var) * 0.9f + std::sin(x * 1.3f + var * 2.0f) * 0.4f; // broad folds with a finer ripple on them
            int twill = ((x + y / 2) % 4 == 0) ? 1 : 0;                                                // the diagonal twill of the weave
            int t = 2 + (fold > 0.5f) - (fold < -0.4f) - (fold < -1.0f) + twill - ((y - top) < 3) - (y > bot - 3 && (x + y) % 2);
            if ((y % 3 == 0) && R(x, y, 93) > 0.7f) t--; // a thread or two out of line
            c.px(x, y, tone(cloth, t));
        }
    for (int x = 3; x < W - 3; x++) { c.px(x, top, tone(cloth, 0)); c.px(x, top - 1 + 0, WOOD[0]); } // shadow of the rod on the cloth
    auto gold = [&](int i) { return (i / 2) % 2 ? BRASS[3] : BRASS[2]; };
    for (int y = top + 2; y <= bot - 1; y++) for (int k = 0; k < 2; k++) { c.px(5 + k, y, gold(y)); c.px(W - 6 - k, y, gold(y)); }                   // the woven border
    for (int x = 5; x < W - 5; x++) for (int k = 0; k < 2; k++) { c.px(x, top + 2 + k, gold(x)); c.px(x, bot - 1 - k, gold(x)); }
    for (int y = top + 5; y <= bot - 4; y++) { c.px(8, y, BRASS[1]); c.px(W - 9, y, BRASS[1]); }                                                    // a fine inner line
    for (int x = 8; x <= W - 9; x++) { c.px(x, top + 5, BRASS[1]); c.px(x, bot - 4, BRASS[1]); }
    for (int x = 10; x < W - 10; x += 4) { c.px(x, top + 7, BRASS[3]); c.px(x + 1, top + 8, BRASS[2]); c.px(x, top + 9, BRASS[3]); c.px(x - 1, top + 8, BRASS[2]); } // a band of diamonds
    for (int k = 0; k < 4; k++) c.px(k < 2 ? 6 : W - 7, k % 2 ? top + 2 : bot - 1, BRASS[4]);                                                         // corner studs
    motif(c, design, 11, top + 11, W - 12, bot - 7, cloth);
    for (int x = 3; x < W - 3; x += 2) // the fringe, knotted every so often, with a tassel at each corner
    {
        int fl = 3 + (int)(R(x, var, 95) * 3);
        for (int k = 0; k < fl; k++) c.px(x, bot + 1 + k, k == 0 ? BRASS[1] : (k > fl - 2 ? tone(BRASS, 2) : tone(BRASS, 3)));
    }
    for (int s = 0; s < 2; s++) { int tx = s ? W - 5 : 4; c.ball((float)tx, (float)(bot + 6), 1.8f, 1.8f, BRASS, 3); c.vline(tx, bot + 1, bot + 4, BRASS[2]); }
    for (int i = 0; i < 6; i++) { int mx = 9 + (int)(R(i, var, 97) * (W - 18)), my = top + 11 + (int)(R(var, i, 98) * (bot - top - 20)); c.px(mx, my, Color{0, 0, 0, 0}); if (i % 2) c.px(mx + 1, my, tone(cloth, 0)); } // moth holes
    if (R(var, 1, 99) > 0.4f) // a torn corner: the hem ragged away
        for (int y = bot - 8; y <= bot + 6; y++) for (int x = W - 14 + (bot - y) / 2; x < W - 8; x++) if (y > bot - 8 + (x - (W - 14)) * 1 - 3) c.px(x, y, Color{0, 0, 0, 0});
}

void drape(Cv& c, int var, int H)
{ // heavy curtains drawn aside and tied back with a cord, a swag across the top, dust over all
    int W = c.w;
    const Ramp& cl = CLOTHS[var % 5];
    int tieY = H * 5 / 9;
    for (int x = 0; x < W; x++) if (x > 2 && x < W - 3) { c.px(x, 1, WOOD[3]); c.px(x, 2, WOOD[1]); } // the rod
    c.ball(1.8f, 1.8f, 2, 2, BRASS, 2); c.ball(W - 2.8f, 1.8f, 2, 2, BRASS, 2);
    for (int side = 0; side < 2; side++)
        for (int y = 5; y < H - 1; y++)
        {
            float u = (y - 5.0f) / (H - 6), tie = (float)(y - tieY) / (H - tieY);
            float hw = y < tieY ? 8.0f - 3.5f * (float)(y - 5) / (tieY - 5) : 4.5f + 6.0f * tie * tie; // gathered at the tie, flaring below it
            for (int i = 0; i < (int)hw; i++)
            {
                int x = side ? W - 1 - i : i;
                float fold = std::sin(i * 1.3f + y * 0.05f + side * 2 + var);
                int t = 2 + (fold > 0.5f) - (fold < -0.4f) - (i >= (int)hw - 2) + (u < 0.05f ? -1 : 0);
                if (y > H - 6 && R(x, y, 101 + var) > 0.7f - (y - (H - 6)) * 0.05f) continue; // a ragged hem
                c.px(x, y, tone(cl, t));
            }
        }
    for (int side = 0; side < 2; side++) // the tie-back cord and tassel
    {
        int x0 = side ? W - 12 : 1, x1 = side ? W - 2 : 11;
        c.hline(x0, x1, tieY, BRASS[3]); c.hline(x0, x1, tieY + 1, BRASS[1]);
        int tx = side ? x0 + 1 : x1 - 1;
        c.rect(tx, tieY + 2, tx + 1, tieY + 6, BRASS[2]); c.px(tx, tieY + 6, BRASS[4]);
    }
    for (int x = 3; x < W - 3; x++) // the swag over the rod, scalloped, with a gold edge
    {
        float u = (x - 3.0f) / (W - 6), sc = std::fabs(std::sin(u * 3.14159f * 3));
        int depth = 3 + (int)(sc * 4);
        for (int y = 3; y < 3 + depth; y++) c.px(x, y, tone(cl, y == 3 ? 3 : 2 - (y > depth)));
        c.px(x, 3 + depth, BRASS[3]);
    }
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) // dust: the whole cloth gone grey and the odd web
        if (c.on(x, y) && R(x, y, 103) > 0.93f) c.px(x, y, lerpColor(c.at(x, y), LINEN[2], 0.45f));
    for (int k = 0; k < 7; k++) { c.px(W / 2 - 1 + k / 2, 4 + k, Color{150, 150, 160, 255}); c.px(W / 2 + 1 + k / 2, 4 + k / 2, Color{130, 130, 140, 255}); }
}

void horns(Cv& c, int slope10)
{ // the gable boards run on past the ridge and cross, each ending in a carved beast's head
    int W = c.w, H = c.h, cx = W / 2;
    float s = slope10 / 10.0f;
    for (int sd : {-1, 1})
    {
        float x1 = cx + sd * 14.0f, y1 = H - 4 - 14.0f * s;
        c.line((float)cx, (float)H - 3, x1, y1, WOOD[2], 2);
        c.line((float)cx, (float)H - 4, x1, y1 - 1, WOOD[4], 1);
        c.line((float)cx + 1, (float)H - 2, x1 + 1, y1 + 1, WOOD[1], 1);
        for (int a = 0; a < 18; a++) // the head: a hooked, beaked curl, an eye in it
        {
            float t = a / 17.0f * 3.6f - 0.4f;
            int hx = (int)std::lround(x1 + sd * std::sin(t) * 5), hy = (int)std::lround(y1 - 1 - (1 - std::cos(t)) * 4);
            c.px(hx, hy, WOOD[a < 4 ? 3 : 4]); c.px(hx, hy + 1, WOOD[1]);
        }
        c.px((int)std::lround(x1 + sd * 2), (int)std::lround(y1 - 3), BRASS[3]);
    }
    c.rect(cx - 1, H - 4, cx, H - 1, WOOD[0]); // the lashing where they cross
}


void antlers(Cv& c)
{ // an elk skull with its antlers spread: 40 x 28
    int cx = 20;
    for (int sd : {-1, 1})
    {
        c.line(cx + sd * 4.0f, 14, cx + sd * 17.0f, 5, LINEN[3], 2);
        c.line(cx + sd * 4.0f, 13, cx + sd * 17.0f, 4, LINEN[4], 1);
        for (int t = 0; t < 3; t++)
        {
            float bx = cx + sd * (8.0f + t * 3.4f), by = 12 - t * 2.2f;
            c.line(bx, by, bx + sd * 1.5f, by - 6 - t, LINEN[3], 1);
            c.px((int)(bx + sd * 1.5f), (int)(by - 7 - t), LINEN[4]);
        }
        c.line(cx + sd * 17.0f, 5, cx + sd * 19.0f, 0, LINEN[3], 1);
    }
    c.ball(cx, 19, 5.2f, 6.4f, LINEN, 3);
    c.rect(cx - 2, 24, cx + 1, 27, tone(LINEN, 2)); // the muzzle
    c.hline(cx - 2, cx + 1, 27, tone(LINEN, 1));
    for (int sd : {-1, 1}) { c.px(cx + sd * 3 - (sd < 0), 18, NIGHT[0]); c.px(cx + sd * 3 - (sd < 0), 19, NIGHT[0]); }
    c.px(cx - 1, 25, NIGHT[1]); c.px(cx, 25, NIGHT[1]);
}

void dragonPillar(Cv& c, int var, int H)
{ // a red pillar carved with two interlaced dragons, a dark capital and plinth: 18 px wide
    int W = c.w;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
        {
            float edge = 1.0f - 0.4f * std::fabs((x - 5.0f) / (W / 2.0f)) * 0.5f - (x > W / 2 ? 0.05f : 0);
            if (y < 8 || y >= H - 10) { int t = (y == 7 || y == H - 10) ? 1 : 2; c.px(x, y, tone(WOOD, t + (x < 3) - (x > W - 4))); continue; }
            float u = (y - 8) * 0.13f, dxu = (x - W / 2) * 0.28f;
            float a = std::sin(u + dxu * 1.0f), b = std::sin(u - dxu * 1.0f + 1.6f);
            bool carved = std::fabs(a) < 0.17f || std::fabs(b) < 0.17f;
            int t = carved ? 0 : 2 + (x < 4) - (x > W - 5) + (R(x, y / 3, 131) > 0.8f ? 1 : 0);
            if (x == 0 || x == W - 1) t = 1;
            (void)edge;
            c.px(x, y, tone(CRIM, t));
        }
    (void)var;
}

void idol(Cv& c, int H)
{ // a post carved into a bearded god's face near its top: 22 px wide
    int W = c.w;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
        {
            if (y < 6 && (x < 4 || x >= W - 4)) continue; // a rounded crown
            int t = 2 + (x < 5) - (x > W - 6) + (R(x, y / 4, 133) > 0.8f);
            if (y > 52) { if (x < 7 || x >= W - 7) continue; t = y == 53 ? 0 : 2 + (x < 9) - (x > W - 10); }
            c.px(x, y, tone(WOOD, t + 1));
        }
    int m = W / 2;
    c.hline(m - 8, m + 7, 13, WOOD[0]); c.hline(m - 8, m + 7, 12, WOOD[4]); // the brow
    for (int sd : {-1, 1}) { c.rect(m + sd * 5 - (sd < 0), 16, m + sd * 5 - (sd < 0) + 1, 18, NIGHT[0]); c.px(m + sd * 5 - (sd < 0), 16, BRASS[3]); }
    c.vline(m - 1, 16, 26, WOOD[4]); c.vline(m, 16, 26, WOOD[4]); c.vline(m + 1, 18, 26, WOOD[1]); // the nose
    c.hline(m - 4, m + 3, 30, NIGHT[0]);
    for (int y = 32; y < 50; y++) for (int x = 3; x < W - 3; x++) if ((x + y) % 4 == 0 || (x - y + 400) % 6 == 0) c.px(x, y, WOOD[0]); // the beard
}

void crane(Cv& c)
{ // a hearth pit and a cauldron hung from a crane of lashed poles: 92 x 66
    int W = c.w, H = c.h, m = W / 2;
    for (int x = m - 18; x <= m + 18; x++) // the stone ring
    {
        c.px(x, H - 1, tone(STONE, 2 + (x % 3 == 0))); c.px(x, H - 2, tone(STONE, 3 - (x % 5 == 0))); c.px(x, H - 3, std::abs(x - m) > 14 ? tone(STONE, 2) : C(150, 60, 24));
        c.px(x, H - 4, std::abs(x - m) > 14 ? tone(STONE, 3) : (R(x, 1, 141) > 0.5f ? C(236, 90, 30) : C(110, 30, 16)));
    }
    for (int sd : {-1, 1})
        for (int k = 0; k < 62; k++)
        {
            int x1 = m + sd * (22 + k / 6), x2 = m + sd * (42 - k / 6);
            for (int t = 0; t < 2; t++) { c.px(x1 + t, H - 1 - k, tone(WOOD, 2 + (t == 0))); c.px(x2 + t, H - 1 - k, tone(WOOD, 2 + (t == 0))); }
            if (k % 14 == 6) { c.hline(x1 - 1, x1 + 2, H - 1 - k, tone(LINEN, 1)); c.hline(x2 - 1, x2 + 2, H - 1 - k, tone(LINEN, 1)); } // lashings
        }
    for (int x = m - 30; x <= m + 30; x++) { c.px(x, 2, tone(WOOD, 3)); c.px(x, 3, tone(WOOD, 2)); c.px(x, 4, tone(WOOD, 0)); }
    for (int y = 5; y < 32; y++) { c.px(m, y, tone(IRON, y % 4 < 2 ? 3 : 1)); c.px(m + 1, y, tone(IRON, y % 4 < 2 ? 2 : 0)); } // the chain
    c.ball(m + 0.5f, 42, 11, 10, IRON, 2);
    c.hline(m - 10, m + 11, 33, tone(IRON, 4)); c.hline(m - 10, m + 11, 34, tone(IRON, 0));
    c.hline(m - 9, m + 10, 33, C(120, 70, 30)); // the stew showing at the rim
}

void spike(Cv& c)
{ // a carved finial along a roof ridge: 4 x 6
    for (int y = 0; y < 6; y++) { c.px(1, y, WOOD[3]); c.px(2, y, WOOD[1]); if (y > 3) { c.px(0, y, WOOD[2]); c.px(3, y, WOOD[0]); } }
}

// A cobweb in a ceiling corner (the corner is the top centre; the web fans right, flip mirrors it): uneven spokes, then rings
// that sag between them, in fine translucent silk with a few dew beads.
void cobweb(Cv& c, int var, int rad)
{
    const int n = 6 + var % 2;
    const float cx = c.w * 0.5f;
    std::vector<float> ang(n + 1), len(n + 1);
    for (int i = 0; i <= n; i++)
    {
        ang[i] = i * 1.5708f / n + (i > 0 && i < n ? (R(var, i, 1) - 0.5f) * 0.14f : 0);
        len[i] = rad * (0.78f + 0.22f * R(var, i, 2));
    }
    auto silk = [&](float x, float y, int a) { int ix = (int)std::floor(x), iy = (int)std::floor(y); if (c.in(ix, iy) && c.at(ix, iy).a < a) c.px(ix, iy, Color{214, 214, 226, (unsigned char)a}); };
    for (int i = 0; i <= n; i++)
        for (float r = 0; r < len[i]; r += 0.5f) silk(cx + std::cos(ang[i]) * r, std::sin(ang[i]) * r, r < len[i] * 0.3f ? 200 : 150);
    for (int k = 1; k <= 6; k++)
    {
        float rk = rad * 0.8f * k / 6.0f * (0.92f + 0.16f * R(var, k, 3));
        for (int i = 0; i < n; i++)
        {
            if (rk > std::min(len[i], len[i + 1])) continue;
            for (float u = 0; u <= 1; u += 0.04f)
            {
                float a = ang[i] + (ang[i + 1] - ang[i]) * u, r = rk * (1 - 0.14f * std::sin(u * 3.14159f)); // sags between the spokes
                silk(cx + std::cos(a) * r, std::sin(a) * r, 100);
            }
        }
    }
    for (int k = 0; k < 3; k++) { int i = (int)(R(var, k, 4) * n), r = (int)(rad * (0.2f + 0.5f * R(var, k, 5))); c.px((int)(cx + std::cos(ang[i]) * r), (int)(std::sin(ang[i]) * r) + 1, Color{240, 244, 255, 230}); }
}

// A small hole broken through the wall, chipped round its edge, with a wet dark streak running down from it (the water itself is live: entities.cpp).
void leak(Cv& c, int var)
{
    int cx = c.w / 2;
    for (int y = 3; y < c.h; y++) // the streak
    {
        float f = 1 - (y - 3) / (float)(c.h - 3);
        int wdt = 1 + (int)(f * 2.5f) + (R(y, var, 61) > 0.8f ? 1 : 0);
        for (int x = cx - wdt; x <= cx + wdt; x++) c.px(x, y, Color{10, 14, 24, (unsigned char)(150 * f)});
    }
    c.ell((float)cx, 2.5f, 3.6f, 3.0f, Color{74, 70, 82, 255});     // the broken edge
    c.ell((float)cx, 2.5f, 2.4f, 2.0f, Color{8, 8, 12, 255});       // the hole
    c.px(cx - 1, 1, Color{150, 190, 230, 255}); c.px(cx + 1, 3, Color{110, 150, 200, 255}); // a wet glint
}

void chain(Cv& c, int var, int H)
{ // a chain hung from a plate in the ceiling, the lantern chain's grey, ending in a hook, manacles, a ring or a snapped end
    int cx = 5;
    c.slab(1, 0, 10, 2, IRON, 2); c.px(3, 1, IRON[4]); c.px(8, 1, IRON[4]);
    int end = H - (var % 4 == 1 ? 14 : (var % 4 == 0 ? 8 : 6));
    for (int y = 3; y < end; y++)
    {
        int k = (y - 3) % 6; // a face-on link, then one edge-on
        Color a = (y / 6) % 2 ? IRON[3] : IRON[2], b = IRON[1];
        if (k < 3) { c.px(cx, y, k == 0 ? IRON[4] : a); c.px(cx + 1, y, k == 2 ? IRON[0] : b); if (k == 1 && R(y, var, 105) > 0.5f) { c.px(cx - 1, y, IRON[1]); } }
        else c.px(cx + (k % 2), y, k == 4 ? IRON[3] : IRON[1]);
        if (R(cx, y, 107 + var) > 0.97f) c.px(cx + 1, y, C(122, 64, 36)); // a fleck of rust
    }
    switch (var % 4)
    {
    case 0: // an S-hook
        c.line(cx + 0.5f, (float)end, cx + 0.5f, (float)end + 2, IRON[3], 1);
        for (int a = 0; a < 16; a++) { float t = a / 15.0f * 4.2f; c.px(cx + 1 + (int)std::lround(std::sin(t) * 2.4f), end + 2 + (int)std::lround((1 - std::cos(t)) * 2.6f), IRON[a > 10 ? 2 : 3]); }
        break;
    case 1: // a manacle: an open iron cuff
        for (int a = 0; a < 22; a++) { float t = 0.6f + a / 21.0f * 5.2f; c.px(cx + 1 + (int)std::lround(std::cos(t) * 4), end + 5 + (int)std::lround(std::sin(t) * 4), a < 3 || a > 18 ? IRON[4] : (std::cos(t) < 0 ? IRON[4] : IRON[2])); c.px(cx + 1 + (int)std::lround(std::cos(t) * 3), end + 5 + (int)std::lround(std::sin(t) * 3), IRON[1]); }
        c.rect(cx - 1, end - 1, cx + 2, end + 1, IRON[3]);
        break;
    case 2: // a ring
        for (int a = 0; a < 24; a++) { float t = a / 24.0f * 6.283f; c.px(cx + (int)std::lround(std::cos(t) * 2.4f), end + 2 + (int)std::lround(std::sin(t) * 2.4f), a % 3 ? IRON[3] : IRON[4]); }
        break;
    default: // snapped off: a broken link, nothing below
        c.px(cx, end, IRON[3]); c.px(cx + 1, end, IRON[1]); c.px(cx - 1, end + 1, IRON[2]);
        break;
    }
}

void blood(Cv& c, int var)
{ // old blood on the wall or floor: a splash, runs, a hand, a drag, a pool, a spray
    int W = c.w, H = c.h, k = var % 6;
    float cx = W / 2.0f, cy = H / 2.0f;
    auto spot = [&](int x, int y, int sz) { Color col = tone(BLOODR, 1 + (R(x, y, 111) > 0.55f) + (R(x, y, 112) > 0.9f) * 2); for (int dy = 0; dy < sz; dy++) for (int dx = 0; dx < sz; dx++) c.px(x + dx, y + dy, col); };
    if (k == 0) // a splash with streaks flung out
    {
        c.ball(cx, cy, 4.5f, 4, BLOODR, 2);
        for (int i = 0; i < 14; i++)
        {
            float a = i * 0.449f + R(i, var, 113), len = 6 + R(i, var, 114) * (W / 2.0f - 7);
            for (float t = 3; t < len; t += 0.5f) c.px((int)(cx + std::cos(a) * t), (int)(cy + std::sin(a) * t * 0.8f), tone(BLOODR, t > len - 2 ? 3 : 1 + (int)(t) % 2));
            spot((int)(cx + std::cos(a) * (len + 2)), (int)(cy + std::sin(a) * (len + 2) * 0.8f), 1 + (i % 3 == 0));
        }
    }
    else if (k == 1) // a smear along the top with long runs down the wall
    {
        for (int x = 3; x < W - 3; x++) for (int y = 1; y < 4; y++) c.px(x, y, tone(BLOODR, 1 + (y == 1) + (R(x, y, 115) > 0.6f)));
        for (int x = 5; x < W - 4; x += 2 + (int)(R(x, var, 116) * 4))
        {
            int len = 6 + (int)(R(x, var, 117) * (H - 12));
            for (int y = 4; y < len; y++) { c.px(x, y, tone(BLOODR, 2 + (y % 5 == 0))); if (y < len - 4) c.px(x + 1, y, tone(BLOODR, 1)); }
            c.ball(x + 0.5f, (float)len + 1, 1.6f, 1.8f, BLOODR, 3);
        }
    }
    else if (k == 2) // a hand pressed to the wall, smeared as it slid
    {
        for (int rep = 0; rep < 2; rep++)
        {
            float ox = cx - 3 + rep * 5, oy = cy + 2 + rep * 3;
            c.ball(ox, oy, 4.2f, 4.6f, BLOODR, 3);
            for (int f = 0; f < 4; f++) { int fx = (int)(ox - 4 + f * 2.8f); c.rect(fx, (int)(oy - 5 - (f == 1 || f == 2) * 2 - 4), fx + 1, (int)(oy - 2), tone(BLOODR, 3 - (f % 2))); }
            c.line(ox + 4, oy, ox + 8, oy - 3, tone(BLOODR, 3), 2);
            for (int s = 0; s < 6; s++) c.px((int)(ox - 2 + s), (int)(oy + 5 + s / 3), tone(BLOODR, 1)); // drag marks beneath
        }
    }
    else if (k == 3) // a long drag across the floor or wall, tapering
        for (int x = 2; x < W - 2; x++)
        {
            float u = (x - 2.0f) / (W - 4), wid = 1 + 2.4f * std::sin(u * 3.14159f), y0 = cy + (u - 0.5f) * (H * 0.5f) + std::sin(u * 7) * 1.6f;
            for (int dy = 0; dy < (int)(wid + 1); dy++) if (R(x, dy, 119 + var) > 0.18f) c.px(x, (int)(y0 + dy), tone(BLOODR, 1 + (dy == 0) + (R(x, dy, 120) > 0.7f)));
        }
    else if (k == 4) // a pool on the floor, a wet gleam in it (the floor is its bottom edge)
    {
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++)
            {
                float dx = (x + 0.5f - cx) / (W * 0.48f), dy = (y + 0.5f - (H - 3.0f)) / 3.0f;
                float r = dx * dx + dy * dy + (R(x / 2, y, 121 + var) - 0.5f) * 0.25f;
                if (r < 1) c.px(x, y, tone(BLOODR, 1 + (r < 0.55f) + (r < 0.12f && x % 5 == 0 ? 2 : 0)));
            }
        for (int i = 0; i < 5; i++) spot((int)(cx + (R(i, var, 122) - 0.5f) * W * 0.9f), H - 6 - (int)(R(var, i, 123) * 3), 1);
    }
    else // a spray in an arc, as a blade swung
        for (int i = 0; i < 40; i++)
        {
            float a = -2.5f + i * 0.1f + (R(i, var, 125) - 0.5f) * 0.2f, r = W * 0.36f + (R(i, var, 126) - 0.5f) * 7;
            int x = (int)(cx + std::cos(a) * r), y = (int)(H * 0.85f + std::sin(a) * r * 0.55f);
            spot(x, y, i % 4 == 0 ? 2 : 1);
            if (i % 3 == 0) c.line((float)x, (float)y, cx + std::cos(a) * (r + 4), H * 0.85f + std::sin(a) * (r + 4) * 0.55f, tone(BLOODR, 1), 1);
        }
}
// ------------------------------------------------------------------------------------------------ Hearthwick's market stalls
// Layers 0 (the back, behind the merchant) and 1 (counter and wares) of entities.cpp:drawStall, painted at half a unit per
// pixel on a 144 x 124 canvas whose foot is at (72, 116). kind: 0 weaponsmith, 1 arcanist, 2 outfitter.
const Ramp TEAL = {C(10, 30, 38), C(20, 56, 70), C(36, 92, 112), C(60, 128, 148), C(104, 170, 184)};

struct StallCv : Cv
{
    StallCv() : Cv(144, 124) {}
    int X(float u) const { return (int)std::lround(72 + 2 * u); }
    int Y(float u) const { return (int)std::lround(116 + 2 * u); }
};

void stallBack(StallCv& c, int kind)
{
    const Ramp& acc = kind == 0 ? CRIM : (kind == 1 ? PLUM : TEAL);
    for (int y = c.Y(-36); y < c.Y(-12); y++) // the wool hanging behind, in folds
        for (int x = c.X(-22); x < c.X(22); x++)
        {
            int t = 1 + ((x / 5) % 2 == 0) + (R(x, y / 3, 151) > 0.82f) - (y > c.Y(-16));
            c.px(x, y, lerpColor(tone(BLOODR, 0), C(70, 56, 44), 0.25f * t));
        }
    for (int sd : {-1, 1}) // lashed posts, their tops crossed
    {
        int px = c.X(sd < 0 ? -25 : 22);
        for (int y = c.Y(-46); y <= c.Y(0); y++)
            for (int k = 0; k < 6; k++) c.px(px + k, y, tone(WOOD, (k < 2 ? 3 : (k < 4 ? 2 : 1)) + (R(px + k, y / 4, 153) > 0.85f)));
        c.line((float)px - 4, (float)c.Y(-50), (float)px + 10, (float)c.Y(-43), tone(WOOD, 2), 2);
        c.line((float)px + 10, (float)c.Y(-50), (float)px - 4, (float)c.Y(-43), tone(WOOD, 3), 2);
        for (int y : {c.Y(-47), c.Y(-45), c.Y(-30)}) { c.hline(px - 1, px + 6, y, LINEN[3]); c.hline(px - 1, px + 6, y + 1, LINEN[1]); }
    }
    plank(c, c.X(-24), c.Y(-40), c.X(24), c.Y(-37), true, 155, 3);
    for (int lx : {-12, 0, 12}) c.rect(c.X((float)lx), c.Y(-41), c.X((float)lx) + 3, c.Y(-36), tone(LINEN, 2));
    for (int x = c.X(-22); x < c.X(22); x++) // the awning, sagging between its ties, with a coloured scalloped hem
    {
        float k = (x - c.X(-22)) / 2.0f;
        int sag = (int)(std::sin(k * 3.14159f / 11) * 2.4f + 2.4f), bot = c.Y(-40) + sag + 4;
        for (int y = c.Y(-45); y < bot; y++) c.px(x, y, tone(LINEN, 3 - (y > bot - 3) + (((int)k % 11) < 2 ? -1 : 0) - (R(x, y, 157) > 0.9f)));
        int sc = (x / 4) % 2 ? 0 : 2;
        for (int y = bot; y < bot + 3 + sc; y++) c.px(x, y, tone(acc, y == bot ? 3 : 2));
    }
    for (int k = -19; k <= 19; k++) c.px(c.X((float)k), c.Y(-33 + 1.5f * (1 - (k / 19.0f) * (k / 19.0f))), LINEN[3]), c.px(c.X((float)k) + 1, c.Y(-33 + 1.5f * (1 - (k / 19.0f) * (k / 19.0f))), LINEN[3]); // the rope of goods
    for (int i = 0; i < 6; i++)
    {
        float fx = -16.0f + i * 6;
        int hx = c.X(fx), hy = c.Y(-32 + 1.5f * (1 - (fx / 19.0f) * (fx / 19.0f)));
        c.vline(hx, hy, hy + 3, LINEN[2]);
        if (kind == 0) // horseshoes, tongs, hammers
        {
            if (i % 3 == 0) { for (int a = 0; a < 20; a++) { float t = 0.5f + a / 19.0f * 4.3f; c.px(hx + (int)std::lround(std::cos(t) * 3.2f), hy + 7 + (int)std::lround(std::sin(t) * 3.4f), tone(IRON, a % 6 < 3 ? 3 : 2)); } }
            else if (i % 3 == 1) { c.line((float)hx - 1, (float)hy + 3, (float)hx - 2, (float)hy + 15, IRON[3], 1); c.line((float)hx + 1, (float)hy + 3, (float)hx + 2, (float)hy + 15, IRON[2], 1); c.hline(hx - 1, hx + 1, hy + 4, IRON[1]); }
            else { c.vline(hx, hy + 3, hy + 12, WOOD[3]); c.vline(hx + 1, hy + 3, hy + 12, WOOD[1]); c.slab(hx - 3, hy + 12, hx + 4, hy + 15, IRON, 3); }
        }
        else if (kind == 1) // herbs, charms, rune-bones
        {
            if (i % 3 == 0) { for (int k = 0; k < 8; k++) c.line(hx + (k - 4) * 0.5f, (float)hy + 4, hx + (k - 4) * 1.1f, (float)hy + 12, tone(FOREST, 2 + k % 3), 1); c.hline(hx - 1, hx + 1, hy + 5, LINEN[3]); }
            else if (i % 3 == 1) { c.ball(hx + 0.5f, (float)hy + 8, 3.2f, 4.0f, LINEN, 3); c.px(hx - 1, hy + 7, NIGHT[0]); c.px(hx + 1, hy + 7, NIGHT[0]); c.px(hx, hy + 9, NIGHT[1]); }
            else { c.ball(hx + 0.5f, (float)hy + 8, 3.0f, 3.4f, PLUM, 3); c.px(hx, hy + 6, PLUM[4]); c.vline(hx, hy + 11, hy + 14, PLUM[1]); }
        }
        else // pelts, rope coils, a net
        {
            if (i % 3 == 0) { for (int y = hy + 4; y < hy + 16; y++) for (int x = hx - 3; x <= hx + 4; x++) { if (y > hy + 12 && (x + y) % 3 == 0) continue; c.px(x, y, tone(CLAY, 2 + (x < hx) - (y > hy + 11) + (R(x, y, 159) > 0.8f))); } }
            else if (i % 3 == 1) { for (int r = 0; r < 3; r++) c.ell(hx + 0.5f, (float)hy + 8, 4.2f - r * 1.3f, 4.2f - r * 1.3f, tone(STRAW, 3 - r)); c.disc(hx + 0.5f, (float)hy + 8, 0.9f, WOOD[0]); }
            else { for (int y = hy + 4; y < hy + 14; y++) for (int x = hx - 3; x <= hx + 4; x++) if ((x + y) % 4 == 0 || (x - y + 100) % 4 == 0) c.px(x, y, tone(LINEN, 2)); }
        }
    }
    if (kind == 1) // the ox skull and a banner on the left post
    {
        int ox = c.X(-24);
        for (int sd : {-1, 1}) c.line((float)ox + sd * 5.0f, (float)c.Y(-50), (float)ox + sd * 12.0f, (float)c.Y(-56), LINEN[3], 2);
        c.ball((float)ox, (float)c.Y(-47), 5.5f, 6.0f, LINEN, 3); c.rect(ox - 3, c.Y(-44), ox + 2, c.Y(-41), tone(LINEN, 2));
        c.rect(ox - 4, c.Y(-48), ox - 3, c.Y(-46), NIGHT[0]); c.rect(ox + 2, c.Y(-48), ox + 3, c.Y(-46), NIGHT[0]);
        for (int y = c.Y(-42); y < c.Y(-26); y++) for (int x = ox - 8; x < ox + 4; x++) c.px(x, y, tone(PLUM, 2 + (x < ox - 5) - (x > ox)));
        for (int x = ox - 8; x < ox + 4; x += 2) c.rect(x, c.Y(-26), x, c.Y(-26) + 2, tone(PLUM, 1));
        c.disc((float)ox - 3, (float)c.Y(-37), 1.6f, LINEN[3]); c.disc((float)ox - 3, (float)c.Y(-31), 1.6f, LINEN[3]);
    }
    if (kind == 0) // the shield on the right post
    {
        int sx = c.X(23), sy = c.Y(-30);
        c.disc((float)sx, (float)sy, 13, tone(IRON, 1));
        for (int y = -12; y <= 12; y++) for (int x = -12; x <= 12; x++) if (x * x + y * y <= 121) c.px(sx + x, sy + y, tone((x < 0) == (y < 0) ? CRIM : LINEN, 2 + ((x + y) < -8) - ((x + y) > 8)));
        c.disc((float)sx, (float)sy, 3.2f, tone(IRON, 3)); c.px(sx - 1, sy - 1, IRON[4]);
    }
    c.rect(c.X(-22), c.Y(-30), c.X(-22) + 1, c.Y(-27), IRON[1]); // the lantern on the left post
    c.slab(c.X(-22) - 3, c.Y(-27), c.X(-22) + 5, c.Y(-21), IRON, 2); c.rect(c.X(-22) - 1, c.Y(-26), c.X(-22) + 3, c.Y(-22), C(120, 90, 50));
    c.hline(c.X(24), c.X(36), c.Y(-46), tone(WOOD, 3)); // the sign's arm, and the sign
    c.vline(c.X(27), c.Y(-45), c.Y(-43), LINEN[2]); c.vline(c.X(34), c.Y(-45), c.Y(-43), LINEN[2]);
    c.slab(c.X(26), c.Y(-43), c.X(36), c.Y(-37), WOOD, 3);
    int sx = c.X(31), sy = c.Y(-40);
    if (kind == 0) { c.vline(sx, sy - 5, sy + 3, IRON[4]); c.vline(sx + 1, sy - 5, sy + 3, IRON[2]); c.hline(sx - 2, sx + 3, sy + 1, BRASS[3]); c.rect(sx, sy + 2, sx + 1, sy + 4, WOOD[1]); }
    else if (kind == 1) { c.vline(sx - 1, sy - 4, sy + 4, PLUM[4]); c.line((float)sx - 1, (float)sy - 4, (float)sx + 3, (float)sy - 1, PLUM[4], 1); c.line((float)sx + 3, (float)sy - 1, (float)sx - 1, (float)sy + 1, PLUM[4], 1); }
    else { c.rect(sx - 2, sy - 3, sx, sy + 2, CLAY[3]); c.rect(sx - 2, sy + 1, sx + 3, sy + 3, CLAY[3]); c.hline(sx - 2, sx + 3, sy + 4, CLAY[0]); }
    for (int x = c.X(-24); x < c.X(24); x += 6) c.rect(x, c.Y(-1), x + 3, c.Y(-1) + 1, ((x / 6) % 2) ? tone(acc, 3) : BRASS[2]); // the woven mat at its foot
}

void stallFront(StallCv& c, int kind)
{
    const Ramp& acc = kind == 0 ? CRIM : (kind == 1 ? PLUM : TEAL);
    plank(c, c.X(-19), c.Y(-14), c.X(19), c.Y(-12) + 1, true, 161, 3); // the counter, under a woven runner
    for (int y = c.Y(-12) + 2; y < c.Y(-1); y++)
        for (int x = c.X(-19); x < c.X(19); x++) c.px(x, y, tone(WOOD, 1 + (R(x, y / 4, 163) > 0.8f) - ((x - c.X(-19)) % 12 == 0 ? 1 : 0) + (x - c.X(-19) < 2)));
    for (int y = c.Y(-14); y < c.Y(-4); y++) for (int x = c.X(2); x < c.X(16); x++) c.px(x, y, tone(acc, 2 + ((x - c.X(2)) % 8 < 2) - (y > c.Y(-6))));
    for (int x = c.X(2); x < c.X(16); x += 2) c.rect(x, c.Y(-4), x, c.Y(-4) + 3, tone(BRASS, 2));
    for (int k = 0; k < 4; k++) { int a = c.X((float)(5 + k * 2)), b = c.X((float)(13 - k * 2)), yy = c.Y((float)(-11 + (k % 2) * 2)); c.rect(a, yy, a + 3, yy + 1, LINEN[3]); c.rect(b, yy, b + 3, yy + 1, LINEN[3]); }
    if (kind == 0) // a barrel for the spears, a grindstone on its frame
    {
        c.slab(c.X(26), c.Y(-9), c.X(34), c.Y(0), WOOD, 2);
        for (int y : {c.Y(-7), c.Y(-3)}) { c.hline(c.X(26), c.X(34), y, IRON[2]); c.hline(c.X(26), c.X(34), y + 1, IRON[1]); }
        for (int x = c.X(26) + 3; x < c.X(34); x += 4) c.vline(x, c.Y(-9) + 1, c.Y(0) - 1, tone(WOOD, 1));
        c.slab(c.X(-35), c.Y(-5), c.X(-26), c.Y(-3), WOOD, 2); c.vline(c.X(-34), c.Y(-3), c.Y(0), WOOD[1]); c.vline(c.X(-28), c.Y(-3), c.Y(0), WOOD[1]);
        c.ball((float)c.X(-31), (float)c.Y(-9), 9, 9, STONE, 3); c.disc((float)c.X(-31), (float)c.Y(-9), 1.5f, WOOD[0]);
        c.line((float)c.X(-31), (float)c.Y(-9), (float)c.X(-26), (float)c.Y(-9), WOOD[2], 1);
    }
    else if (kind == 1) // an urn of staves, scrolls, a candle, a seeing-stone on its stand
    {
        c.ball((float)c.X(29), (float)c.Y(-4), 8, 8, CLAY, 2); c.hline(c.X(25), c.X(33), c.Y(-8), CLAY[4]);
        static const Color gems[3] = {C(120, 200, 255), C(200, 120, 255), C(255, 140, 60)};
        for (int k = 1; k < 3; k++) { int sx = c.X((float)(27 + k * 2)), top = c.Y((float)(-26 - k * 3)); c.line((float)sx, (float)c.Y(-8), (float)sx + k * 2, (float)top, WOOD[3], 1); c.disc((float)sx + k * 2, (float)top - 1, 2.6f, gems[k]); }
        c.rect(c.X(-2), c.Y(-18), c.X(-2) + 3, c.Y(-14), LINEN[4]);
        for (int k = 0; k < 3; k++) c.rect(c.X((float)(-35 + k * 3)), c.Y((float)(-4 - k % 2 * 2)), c.X((float)(-35 + k * 3)) + 3, c.Y(0), tone(LINEN, 3 - (k % 2)));
        c.rect(c.X(11), c.Y(-15), c.X(16), c.Y(-14), WOOD[1]); c.ball((float)c.X(13), (float)c.Y(-17), 5, 5, TEAL, 3); c.px(c.X(13) - 2, c.Y(-17) - 2, TEAL[4]);
    }
    else // a folded cloak and cloth, a basket of wool
    {
        c.slab(c.X(-17), c.Y(-18), c.X(-11), c.Y(-14), NAVY, 2); c.slab(c.X(-17), c.Y(-20), c.X(-11), c.Y(-18), CRIM, 2);
        c.slab(c.X(-31), c.Y(-6), c.X(-22), c.Y(0), STRAW, 2);
        for (int y = c.Y(-6); y < c.Y(0); y += 2) for (int x = c.X(-31); x < c.X(-22); x++) if ((x + y) % 3 == 0) c.px(x, y, STRAW[1]);
        c.ball((float)c.X(-29), (float)c.Y(-7), 4, 4, CRIM, 3); c.ball((float)c.X(-25), (float)c.Y(-7), 4, 4, NAVY, 3);
    }
}

} // namespace

Image stallImageFine(int kind, int layer)
{
    StallCv c;
    if (layer == 0) stallBack(c, kind); else stallFront(c, kind);
    return c.image();
}

// where a decor sprite hangs from: 0 its bottom centre stands at (x, y), 1 its top centre hangs from it, 2 its centre is on it
// A tone (0 dark .. 4 light) of a cloth colour, for the live pennants (rig.cpp:drawDecor).
Color clothTone(int var, int t) { return tone(CLOTHS[((var % 6) + 6) % 6], t); }

int decorAnchor(int kind, int var)
{
    switch (kind)
    {
    case DK_TAPESTRY: case DK_DRAPE: case DK_CHAIN: case DK_TOOL: case DK_COBWEB: case DK_LEAK: return 1;
    case DK_ANTLERS: return 2;
    case DK_PICTURE: case DK_SHIELD: return 2;
    case DK_LEANSHIELD: case DK_SPEARPOST: case DK_TARGET: case DK_BOWRACK: return 0;
    case DK_BLOOD: return var % 6 == 4 ? 0 : 2;
    default: return 0;
    }
}

// size: the kind's one free dimension in world units (a table's length, a post's height, a hanging's drop...)
Image decorImageFine(int kind, int var, int size)
{
    size = std::max(size, 4);
    switch (kind)
    {
    case DK_DRESSER: { Cv c(34, 40); dresser(c, var); return c.image(); }
    case DK_TABLE: { Cv c((size + 8) * 2, 26); table(c, size, var); return c.image(); }
    case DK_PICTURE: { int k = var % 5; Cv c(k == 2 ? 28 : 34 + (var / 5) % 3 * 2, k == 2 ? 34 : 26); picture(c, var); return c.image(); }
    case DK_TOOL: { Cv c(18, 30); tool(c, var); return c.image(); }
    case DK_SHELF: { Cv c(size * 2, 22); shelf(c, var); return c.image(); }
    case DK_RACK: { Cv c(size * 2, 52); rack(c, var); return c.image(); }
    case DK_ARROWS: { Cv c(22, 34); arrows(c, var); return c.image(); }
    case DK_BUNK: { Cv c(42, size * 2); bunk(c, var); return c.image(); }
    case DK_HEARTH: { Cv c(54, std::max(size, 24) * 2); hearth(c, c.h); return c.image(); }
    case DK_SHIELD: { Cv c(22, 22); shield(c, var); return c.image(); }
    case DK_BOWRACK: { Cv c(46, 56); bowRack(c, var); return c.image(); }
    case DK_TARGET: { Cv c(54, 56); archeryTarget(c, var); return c.image(); }
    case DK_LEANSHIELD: { Cv c(30, 32); leanShield(c, var); return c.image(); }
    case DK_SPEARPOST: { Cv c(10, std::max(size, 20) * 2); spearPost(c, var, c.h); return c.image(); }
    case DK_POST: { Cv c(6, size * 2); post(c, var); return c.image(); }
    case DK_LADDER: { Cv c(14, size * 2); ladder(c, var); return c.image(); }
    case DK_YARD: { Cv c(22, 22); yard(c, var); return c.image(); }
    case DK_TAPESTRY: { Cv c(34 + ((var >> 5) & 1) * 8, size * 2 + 6); tapestry(c, var, c.h); return c.image(); }
    case DK_DRAPE: { Cv c(40, size * 2); drape(c, var, c.h); return c.image(); }
    case DK_CHAIN: { Cv c(12, size * 2 + 4); chain(c, var, c.h); return c.image(); }
    case DK_COBWEB: { Cv c(size * 4 + 2, size * 2 + 2); cobweb(c, var, size * 2); return c.image(); }
    case DK_LEAK: { Cv c(14, 30); leak(c, var); return c.image(); }
    case DK_ANTLERS: { Cv c(40, 28); antlers(c); return c.image(); }
    case DK_DRAGONPILLAR: { Cv c(18, size * 2); dragonPillar(c, var, c.h); return c.image(); }
    case DK_IDOL: { Cv c(22, std::max(size, 30) * 2); idol(c, c.h); return c.image(); }
    case DK_CRANE: { Cv c(92, 66); crane(c); return c.image(); }
    case DK_SPIKE: { Cv c(4, 6); spike(c); return c.image(); }
    case DK_HORNS: { Cv c(40, (int)(14 * (size / 10.0f)) + 20); horns(c, size); return c.image(); }
    case DK_BLOOD: { int k = var % 6; Cv c(k == 3 ? size * 2 : (k == 4 ? size * 2 : 28), k == 1 ? 34 : (k == 4 ? 10 : (k == 3 ? 18 : 30))); blood(c, var); return c.image(); }
    default: return GenImageColor(2, 2, BLANK);
    }
}

// dev: a contact sheet of every kind and variant, each at 4x on dark grey: <dir>/decor.png
void exportDecorSheet(const char* path)
{
    struct Row { int kind, size, variants; };
    const Row rows[] = {{DK_DRESSER, 0, 3}, {DK_TABLE, 20, 2}, {DK_PICTURE, 0, 5}, {DK_TOOL, 0, 5}, {DK_SHELF, 14, 3}, {DK_RACK, 22, 2}, {DK_ARROWS, 0, 1}, {DK_BUNK, 22, 2}, {DK_HEARTH, 40, 1},
                        {DK_SHIELD, 0, 12}, {DK_LEANSHIELD, 0, 12}, {DK_SPEARPOST, 30, 3}, {DK_TARGET, 0, 3}, {DK_BOWRACK, 0, 2}, {DK_RACK, 22, 2}, {DK_POST, 30, 1}, {DK_LADDER, 30, 1}, {DK_YARD, 0, 3}, {DK_TAPESTRY, 40, 39}, {DK_DRAPE, 44, 5}, {DK_CHAIN, 40, 4}, {DK_BLOOD, 14, 6}};
    const int S = 3, ROWW = 1500;
    std::vector<Image> ims;
    std::vector<Vector2> pos;
    int x = 4, y = 4, rowH = 0;
    for (auto& r : rows)
        for (int v = 0; v < r.variants; v++)
        {
            Image im = decorImageFine(r.kind, v, r.size);
            if (x + im.width * S + 4 > ROWW) { x = 4; y += rowH + 6; rowH = 0; }
            ims.push_back(im); pos.push_back({(float)x, (float)y});
            x += im.width * S + 6; rowH = std::max(rowH, im.height * S);
        }
    Image sheet = GenImageColor(ROWW, y + rowH + 8, Color{54, 50, 58, 255});
    for (size_t i = 0; i < ims.size(); i++)
    {
        ImageResizeNN(&ims[i], ims[i].width * S, ims[i].height * S);
        ImageDraw(&sheet, ims[i], {0, 0, (float)ims[i].width, (float)ims[i].height}, {pos[i].x, pos[i].y, (float)ims[i].width, (float)ims[i].height}, WHITE);
        UnloadImage(ims[i]);
    }
    ExportImage(sheet, path);
    UnloadImage(sheet);
}

// ---------------------------------------------------------------- the longship (art baked by tools/ship.py into sprites_ship.h)
#include "sprites_ship.h"
static Image shipImage(const char* const* rows, int w, int h)
{
    Image im = GenImageColor(w, h, BLANK);
    Color* px = (Color*)im.data;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
        {
            const char* p = std::strchr(SHIP_ALPHABET, rows[y][x]);
            if (rows[y][x] != '.' && p) px[y * w + x] = SHIP_PAL[p - SHIP_ALPHABET];
        }
    return im;
}

Image longshipImage(int part) // 0 hull, 1 mast, 2 wreck hull, 3 wreck mast
{
    switch (part)
    {
    case 0: return shipImage(SHIP_HULL, SHIP_HULL_W, SHIP_HULL_H);
    case 1: return shipImage(SHIP_MAST, SHIP_MAST_W, SHIP_MAST_H);
    case 2: return shipImage(SHIP_WRECK_HULL, SHIP_WRECK_HULL_W, SHIP_WRECK_HULL_H);
    default: return shipImage(SHIP_WRECK_MAST, SHIP_WRECK_MAST_W, SHIP_WRECK_MAST_H);
    }
}

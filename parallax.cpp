// Parallax backdrop: five silhouette layers behind the land, drawn wherever the sky shows. The further back, the paler, the chunkier
// (blocks of 2^q cells) and the slower it scrolls. Mountains, then castles on hills, a hamlet, a pine forest, a near ridge with
// farmsteads. A few houses burn: flames flicker (shimmer) over the roofs and glow warms everything behind them.
#include "world.h"
#include "util.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace {

struct LP
{
    float f, lift, ridgeBase, ridgeAmp, ridgeFreq; // scroll speed, cells above the horizon, hill shape
    int q;                                         // block size 2^q cells
    int castleSlot; float castleP, castleScale;
    int houseSlot; float houseP, burnP, houseScale;
    int treeSlot; float treeP, treeMin, treeMax;
    Color body, roof;
};

const LP LAY[5] = {
    // f     lift base amp  freq     q  castle: slot P scale   house: slot P burn scale  tree: slot P min max   body            roof
    {0.04f, 24, 14, 150, 0.0050f, 0,    0, 0, 1,            0, 0, 0, 1,               0, 0, 0, 0,        {58, 68, 108, 255}, {58, 68, 108, 255}},
    {0.10f, 32, 8, 90, 0.0070f, 0,     380, 0.6f, 1.7f,    0, 0, 0, 1,               0, 0, 0, 0,        {46, 54, 90, 255}, {40, 46, 80, 255}},
    {0.22f, 18, 6, 60, 0.0100f, 0,     0, 0, 1,            90, 0.55f, 0.22f, 1.1f,   26, 0.3f, 12, 22,  {34, 40, 70, 255}, {42, 40, 62, 255}},
    {0.40f, 8, 4, 50, 0.0120f, 0,      0, 0, 1,            260, 0.4f, 0.25f, 1.3f,   6, 0.85f, 14, 28,  {22, 28, 50, 255}, {30, 32, 50, 255}},
    {0.65f, 0, 3, 36, 0.0150f, 0,      0, 0, 1,            300, 0.35f, 0.25f, 2.0f,  16, 0.5f, 26, 46,  {13, 17, 32, 255}, {20, 21, 34, 255}},
};
constexpr int NL = 5;
enum Kind : uint8_t { K_GROUND, K_TREE, K_HOUSE, K_CASTLE };

struct Col { short rise, ground, wall, flame; uint8_t kind, fid; bool burn; };
struct LayerState { std::vector<Col> col; std::vector<float> glow; std::vector<short> glowY; int base, off; };

LayerState st[NL];
int sw = 0, sframe = 0;
std::vector<int> clearAbove; // per column: rows above this show no layer, flame, ember or glow (most of the sky)

inline int quant(float v, int q) { return ((int)v >> q) << q; }

float hill(const LP& L, int k, int p)
{
    float r = L.ridgeBase + std::max(0.0f, fbm(p * L.ridgeFreq, 1.0f + k * 7.3f, 700 + k, 3) - 0.28f) * L.ridgeAmp;
    return (float)quant(r, L.q);
}

void buildLayer(int k, int vw, int vh, int camX, int camY)
{
    const LP& L = LAY[k];
    LayerState& S = st[k];
    int fq = 0;
    S.off = (int)std::floor(L.f * camX);
    S.base = (int)(vh * 0.44f) - (int)L.lift; // fixed on screen: the camera follows the player, so the land stays near the middle and the backdrop never bobs
    S.col.assign(vw, Col{});
    S.glow.assign(vw, 0.0f);
    S.glowY.assign(vw, 0);
    for (int i = 0; i < vw; i++)
    {
        int pq = ((i + S.off) >> fq) << fq;
        short r = (short)hill(L, k, pq);
        S.col[i] = Col{r, r, r, 0, K_GROUND, 0, false};
    }
    int p0 = S.off, p1 = S.off + vw;
    auto put = [&](int p, short rise, short wall, Kind kind, uint8_t fid, short ground) {
        int i = p - S.off;
        if (i < 0 || i >= vw) return;
        Col& c = S.col[i];
        if (rise > c.rise) c = Col{rise, ground, wall, 0, kind, fid, false};
    };
    auto slots = [&](int slot, int pad, auto fn) {
        for (int s = (p0 - pad) / slot - 1; s <= (p1 + pad) / slot + 1; s++) fn(s);
    };
    if (L.castleSlot) // drum towers and a curtain wall round a keep
        slots(L.castleSlot, 120, [&](int s) {
            if (hash2(s, 11, 800 + k) >= L.castleP) return;
            int cx = s * L.castleSlot + L.castleSlot / 2 + (int)((hash2(s, 12, 800 + k) - 0.5f) * 120);
            short hb = (short)hill(L, k, cx);
            float cs = L.castleScale;
            for (int p = cx - (int)(70 * cs); p <= cx + (int)(70 * cs); p++)
            {
                if (p < p0 || p >= p1) continue;
                float u = (p - cx) / cs, au = std::fabs(u);
                float h = 0;
                if (au <= 62) h = 20 - (((int)(u + 200) / 4) % 2 == 0 ? 3 : 0);
                if (au <= 15) h = 54 - (((int)(u + 200) / 3) % 2 == 0 ? 3 : 0);
                if (au <= 1) h = 54 + 12;
                for (float tc : {-62.0f, -34.0f, 34.0f, 62.0f})
                {
                    float d = std::fabs(u - tc);
                    if (d <= 5.5f) h = std::max(h, 38.0f);
                    if (d <= 6.5f) h = std::max(h, 38.0f + 16 * (1 - d / 6.5f));
                }
                if (h > 0) put(quant((float)p, 0), (short)(hb + (int)(h * cs)), 0, K_CASTLE, (uint8_t)(hash2(s, 13, k) * 255), hb);
            }
        });
    if (L.houseSlot)
        slots(L.houseSlot, 60, [&](int s) {
            if (hash2(s, 21, 810 + k) >= L.houseP) return;
            int cx = s * L.houseSlot + L.houseSlot / 2 + (int)((hash2(s, 22, 810 + k) - 0.5f) * L.houseSlot * 0.3f);
            float hh = hash2(s, 23, 810 + k), hs2 = L.houseScale;
            int hw = (int)((10 + 7 * hh) * hs2), wallH = (int)((9 + 8 * hash2(s, 24, 810 + k)) * hs2), roofH = (int)((7 + 8 * hash2(s, 25, 810 + k)) * hs2);
            bool burn = hash2(s, 26, 810 + k) < L.burnP;
            short hb = (short)hill(L, k, cx), wall = (short)(hb + wallH);
            uint8_t fid = (uint8_t)(hh * 255);
            for (int p = cx - hw - 2; p <= cx + hw + 2; p++)
            {
                if (p < p0 || p >= p1) continue;
                float dx = (float)std::abs(p - cx);
                int roof = std::max(0, (int)(roofH * (1 - dx / (hw + 3))));
                if (burn) roof = roof * 3 / 5 - (hash2(p >> 1, s, 5) < 0.4f ? 2 : 0); // the roof has fallen in
                short rise = (short)(wall + std::max(roof, 0));
                if (dx > hw) rise = (short)(wall + std::max(roof, 0));
                put(p, rise, wall, K_HOUSE, fid, hb);
                if (burn)
                {
                    int i = p - S.off;
                    float env = 1 - (dx / (hw + 3)) * (dx / (hw + 3));
                    float fl = (7 + 12 * hh) * hs2 * env * (0.45f + 0.9f * vnoise(p * 0.28f, sframe * 0.1f, 830 + s));
                    if (S.col[i].kind == K_HOUSE && S.col[i].fid == fid) { S.col[i].burn = true; S.col[i].flame = (short)std::max(0.0f, fl); }
                }
            }
            if (burn) // a warm glow over the neighbourhood
            {
                float gs = 0.5f * (0.8f + 0.4f * vnoise(sframe * 0.12f, (float)s, 840));
                for (int p = cx - 90; p <= cx + 90; p++)
                {
                    int i = p - S.off;
                    if (i < 0 || i >= vw) continue;
                    float d = 1 - std::abs(p - cx) / 90.0f;
                    S.glow[i] += gs * d * d;
                    S.glowY[i] = (short)(wall + 6);
                }
            }
        });
    if (L.treeSlot) // pines: stepped tiers, tapering to a point
        slots(L.treeSlot, 40, [&](int s) {
            if (hash2(s, 31, 820 + k) >= L.treeP) return;
            int cx = s * L.treeSlot + L.treeSlot / 2 + (int)((hash2(s, 32, 820 + k) - 0.5f) * L.treeSlot * 0.9f);
            float h = L.treeMin + (L.treeMax - L.treeMin) * hash2(s, 33, 820 + k);
            float hw = 3 + h * 0.2f;
            short hb = (short)hill(L, k, cx);
            for (int p = cx - (int)hw; p <= cx + (int)hw; p++)
            {
                if (p < p0 || p >= p1) continue;
                float dx = std::fabs((float)(p - cx)), stepped = std::floor(dx / 2.5f) * 2.5f;
                put(p, (short)(hb + (int)(h * (1 - stepped / hw))), 0, K_TREE, 0, hb);
            }
        });
    const int B = 1 << fq; // one pixel = a block of B x B cells: copy each block's first column, snap heights to the block grid
    for (int i = 0; i < vw; i++)
    {
        int src = i - ((i + S.off) & (B - 1));
        Col c = S.col[src < 0 ? i : src];
        c.rise = (short)((c.rise >> fq) << fq); c.ground = (short)((c.ground >> fq) << fq); c.wall = (short)((c.wall >> fq) << fq);
        c.flame = (short)((c.flame >> fq) << fq);
        S.col[i] = c;
        if (src >= 0) { S.glow[i] = S.glow[src]; S.glowY[i] = S.glowY[src]; }
    }
}

Color mulc(Color c, float k) { return Color{(unsigned char)std::min(255.0f, c.r * k), (unsigned char)std::min(255.0f, c.g * k), (unsigned char)std::min(255.0f, c.b * k), 255}; }

} // namespace

void parallaxPrep(int camX, int camY, int vw, int vh, int frame)
{
    sw = vw;
    sframe = frame;
    parallelFor(NL, [&](int k) { buildLayer(k, vw, vh, camX, camY); });
    clearAbove.assign(vw, 1 << 30);
    for (int k = 0; k < NL; k++)
        for (int i = 0; i < vw; i++)
        {
            const Col& c = st[k].col[i];
            int reach = c.rise + std::max<int>(c.flame, c.burn ? 36 : 0);
            clearAbove[i] = std::min(clearAbove[i], st[k].glow[i] > 0.01f ? -(1 << 30) : st[k].base - reach);
        }
}

// Colour of the backdrop at view cell (i, j): returns 1 and `out` if a layer covers it. `glow` is how far the burning houses'
// light tints the sky or layer there (0 for flames themselves).
int parallaxAt(int i, int j, Color& out, float& glow)
{
    glow = 0;
    if (j < clearAbove[i]) return 0;
    for (int k = NL - 1; k >= 0; k--)
    {
        const LayerState& S = st[k];
        const LP& L = LAY[k];
        const Col& c = S.col[i];
        const int Q = 0;
        int hgt = ((S.base - j) >> Q) << Q;
        if (S.glow[i] > 0.01f) glow += S.glow[i] * std::max(0.0f, 1 - std::abs(hgt - S.glowY[i]) / 70.0f);
        int pq = ((i + S.off) >> Q) << Q, bx = pq >> Q;
        if (hgt <= c.rise)
        {
            Color col = L.body;
            float lit = 1;
            if (c.kind == K_HOUSE)
            {
                if (hgt > c.wall) col = L.roof;
                else if (hgt > c.ground)
                {
                    int by = (hgt - c.ground) >> Q;
                    bool win = (bx % 7 == 3 || bx % 7 == 4) && (by % 6 == 3 || by % 6 == 4);
                    if (win && hash2(bx / 7, c.fid, 9) < 0.45f) col = Color{255, 188, 100, 255}; // a candle in the window
                    else lit = 1.15f;
                }
            }
            else if (c.kind == K_CASTLE)
            {
                int d = (hgt - c.ground) >> Q;
                bool win = (bx % 8 == 3 || bx % 8 == 4) && (d % 12 == 6 || d % 12 == 7) && d > 8;
                if (win) col = hash2(bx / 8, d / 12, c.fid) < 0.4f ? Color{255, 190, 104, 255} : mulc(L.body, 0.55f);
                else lit = ((bx + 1000) % 8) < 2 ? 1.12f : 1.0f;
            }
            else if (hgt == c.rise && k > 0) lit = 1.25f; // a thread of moonlight on the edge
            if (k == 0 && hgt < c.rise * 0.6f) lit *= 0.92f;
            if (k == 0) col = lerpColor(col, Color{96, 60, 132, 255}, clampf(1 - hgt / 140.0f, 0, 1)); // the far range sinks into violet
            out = lit == 1 ? col : mulc(col, lit);
            glow = std::min(glow, 0.7f);
            return 1;
        }
        if (c.flame > 0 && hgt <= c.rise + c.flame)
        {
            int fh = hgt - c.rise;
            float t = fh / (float)c.flame;
            if (hash2(bx, fh >> Q, sframe / 2) < 1.15f - t * 0.9f) // ragged, shimmering tongues
            {
                out = t < 0.25f ? Color{255, 238, 150, 255} : (t < 0.55f ? Color{255, 172, 52, 255} : (t < 0.8f ? Color{232, 92, 30, 255} : Color{140, 46, 30, 255}));
                glow = 0;
                return 1;
            }
        }
        else if (c.burn && hgt > c.rise && hgt < c.rise + 36 && hash2(bx, (hgt + sframe / 2) >> Q, 99) > 0.99f) // embers
        {
            out = Color{255, 160, 60, 255};
            glow = 0;
            return 1;
        }
    }
    glow = std::min(glow, 0.7f);
    return 0;
}

// How violet the open sky is at view row j: the horizon haze, strongest just above the far range's foot.
float parallaxHaze(int j) { return clampf(1 - (st[0].base - j) / 160.0f, 0, 1); }

// ---------------------------------------------------------------- the cavern backdrop
// Where a cave's back wall is flagged sky = 2 the view shows the dark of a far, bigger cavern: four layers of rock, each a ceiling of hanging
// stalactites and a floor of stalagmites repeating every few hundred cells, the far ones paler and slower and lost in mist, the near ones
// dark and close. Heights are worked out once per column per frame (cavePrep), so a pixel costs a comparison per layer.
namespace {
constexpr int CL = 4;
struct CaveLayer { float fx, fy; int T; Color rock; float fog; float scale; };
const CaveLayer CAVE[CL] = {
    {0.10f, 0.06f, 240, {118, 108, 148, 255}, 0.62f, 0.8f},
    {0.20f, 0.12f, 210, {88, 80, 118, 255}, 0.42f, 0.95f},
    {0.34f, 0.20f, 180, {62, 56, 86, 255}, 0.24f, 1.1f},
    {0.52f, 0.32f, 156, {40, 36, 58, 255}, 0.06f, 1.3f},
};
std::vector<float> cvCeil[CL], cvFloor[CL];
int cvPer[CL], cvN0[CL], cvOffX[CL], cvOffY[CL], cvW = 0;

inline float tri(float x) { float f = x - std::floor(x); return std::fabs(f * 2 - 1); }

float capHeight(float x, int n, int k, bool floorSide)
{
    int so = floorSide ? 31 : 0;
    float base = 8 + 20 * vnoise(x * 0.015f + k * 5.3f, n * 3.1f + so, 620 + k);
    float best = 0;
    for (int scale = 0; scale < 3; scale++) // big fangs, middling ones and small teeth: each its own random length, and the odd gap
    {
        float p = scale == 0 ? 23.0f : (scale == 1 ? 11.0f : 5.5f), amp = scale == 0 ? 46.0f : (scale == 1 ? 24.0f : 9.0f);
        float xx = x / p + hash2(n, k, 1 + so + scale) * 9.0f;
        int cell = (int)std::floor(xx);
        float a = hash2(cell, n * 7 + k, 40 + so + scale * 3);
        if (a < 0.28f) continue;                                   // no spike in this cell
        float w = 0.55f + 0.45f * hash2(cell, k, 60 + so + scale); // fat or slender
        float f = xx - cell + (hash2(cell, n, 70 + scale) - 0.5f) * 0.3f; // not quite centred
        float t = std::fabs(f - 0.5f) * 2 / w;                     // 0 at the tip's axis
        if (t >= 1) continue;
        best = std::max(best, std::pow(1 - t, 1.5f + 0.5f * hash2(cell, 3, 80 + so)) * amp * (0.35f + 0.65f * a));
    }
    return (base + best) * CAVE[k].scale;
}
} // namespace

void cavePrep(int camX, int camY, int vw, int vh)
{
    cvW = vw;
    for (int k = 0; k < CL; k++)
    {
        const CaveLayer& L = CAVE[k];
        cvOffX[k] = (int)std::floor(camX * L.fx);
        cvOffY[k] = (int)std::floor(camY * L.fy);
        cvN0[k] = (int)std::floor((float)cvOffY[k] / L.T);
        int per = (vh + L.T - 1) / L.T + 2;
        cvPer[k] = per;
        cvCeil[k].assign((size_t)vw * per, 0.0f);
        cvFloor[k].assign((size_t)vw * per, 0.0f);
        for (int i = 0; i < vw; i++)
            for (int m = 0; m < per; m++)
            {
                cvCeil[k][(size_t)i * per + m] = capHeight((float)(i + cvOffX[k]), cvN0[k] + m, k, false);
                cvFloor[k][(size_t)i * per + m] = capHeight((float)(i + cvOffX[k]), cvN0[k] + m, k, true);
            }
    }
}

Color caveAt(int i, int j, Color base)
{
    if (i < 0 || i >= cvW) return base;
    Color mist = {54, 50, 76, 255};
    mist = lerpColor(mist, Color{76, 70, 104, 255}, clampf(0.5f + 0.5f * std::sin(j * 0.02f), 0, 1) * 0.5f);
    for (int k = CL - 1; k >= 0; k--) // the near layer first: whatever it covers hides the rest
    {
        const CaveLayer& L = CAVE[k];
        int py = j + cvOffY[k], n = (int)std::floor((float)py / L.T), m = n - cvN0[k], yy = py - n * L.T;
        if (m < 0 || m >= cvPer[k]) continue;
        float ch = cvCeil[k][(size_t)i * cvPer[k] + m], fh = cvFloor[k][(size_t)i * cvPer[k] + m];
        float dC = ch - yy, dF = yy - (L.T - fh); // how far inside the ceiling / floor rock this pixel is (negative: in the open)
        float depth = std::max(dC, dF);
        if (depth < 0) continue;
        int px = i + cvOffX[k];
        // a neighbouring column's depth, to find the flanks: the left face of a spike catches the light, the right falls into shadow
        auto depthAt = [&](int ii) -> float {
            if (ii < 0 || ii >= cvW) return depth;
            float c2 = cvCeil[k][(size_t)ii * cvPer[k] + m], f2 = cvFloor[k][(size_t)ii * cvPer[k] + m];
            return std::max(c2 - yy, yy - (L.T - f2));
        };
        float dl = depthAt(i - 1), dr = depthAt(i + 1);
        bool leftEdge = dl < 0, rightEdge = dr < 0;
        float body = 0.82f + 0.36f * vnoise(px * 0.16f, py * 0.07f, 651 + k);                // mottled stone, streaked down the spike
        float strata = 0.9f + 0.2f * std::sin(py * 0.55f + vnoise(px * 0.05f, py * 0.05f, 652 + k) * 6.0f); // faint layers
        float grit = 0.92f + 0.16f * hash2(px, py, 650 + k);
        float form = depth < 3.0f ? 0.72f + 0.1f * depth : 1.0f;                              // darker toward the open edge of the silhouette
        float k2 = body * strata * grit * form;
        if (leftEdge) k2 *= 1.55f; else if (rightEdge) k2 *= 0.62f;                            // the lit and shaded flanks
        else if (dl < 3.0f) k2 *= 1.25f; else if (dr < 3.0f) k2 *= 0.8f;
        if (depth < 1.6f && !leftEdge && !rightEdge) k2 *= 1.3f;                              // the tip
        Color c = mulc(L.rock, k2);
        return lerpColor(c, mist, L.fog);
    }
    return mist;
}

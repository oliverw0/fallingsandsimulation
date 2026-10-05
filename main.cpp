#include <raylib.h>
#include <cmath>
#include <ctime>
#include <string>
#include <vector>
#include <algorithm>
#include <cstdio>
#include <thread>
#include <functional>
#include "game.h"
#include "util.h"
#include <rlgl.h>

static RenderTexture2D rt{};
static Texture2D worldTex{};
static std::vector<Color> pix;

// The world renders at one texel per cell - SUB cells to the world unit - into a render texture where
// characters draw in units (scaled by SUB), then the whole thing scales up crisp.
static const int SUB = 2; // must match the play grid's scale (world.scale)
static void setupView()
{
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    int scale = 4; // fixed: a bigger window or fullscreen shows more of the world, not the same view bigger
    int vw = (sw + scale - 1) / scale, vh = (sh + scale - 1) / scale;
    if (rt.id && vw == G.vw && vh == G.vh && scale == G.scale)
        return;
    G.scale = scale;
    G.vw = vw;
    G.vh = vh;
    if (rt.id)
    {
        UnloadRenderTexture(rt);
        UnloadTexture(worldTex);
    }
    rt = LoadRenderTexture(vw * SUB, vh * SUB);
    Image img = GenImageColor(vw * SUB, vh * SUB, BLACK); // the grid is SUB cells per unit, drawn one texel each
    worldTex = LoadTextureFromImage(img);
    UnloadImage(img);
    pix.assign((size_t)vw * SUB * vh * SUB, BLACK);
}

// ---------------------------------------------------------------- lighting
// A light map at half resolution, smoothed by bilinear filtering and multiplied over the finished
// scene: moonlight falling from open sky, shadow-casting point lights (torches, lanterns, spells,
// a little around the hero) and a soft glow off fire and lava.

static const int LS = 2; // world pixels per light texel
static Texture2D lightTex{};
static int lw = 0, lh = 0, lox = 0, loy = 0;
static std::vector<float> lr, lg, lb, lsky, er, eg, eb;
static std::vector<int> ltop, lwet;
static std::vector<Color> lpix;

static bool opaque(int x, int y) // x, y in world units: the light map works in units, sampling one cell of each
{
    if (!world.inU(x, y)) return true;
    CellMaterial m = world.get(x * world.scale, y * world.scale).material;
    Kind k = props(m).kind;
    return (k == Kind::Solid || k == Kind::Powder) && m != CellMaterial::Glass && m != CellMaterial::Ice;
}

static void boxBlur(std::vector<float>& v, int w, int h, int rx, int ry)
{
    std::vector<float> t(v.size());
    auto pass = [&](const std::vector<float>& src, std::vector<float>& dst, int n, int lines, int r, int stride, int step) {
        for (int l = 0; l < lines; l++)
        {
            const float* a = &src[(size_t)l * stride];
            float* o = &dst[(size_t)l * stride];
            auto at = [&](int i) { return a[(size_t)std::clamp(i, 0, n - 1) * step]; };
            float acc = 0;
            for (int i = -r; i <= r; i++) acc += at(i);
            for (int i = 0; i < n; i++)
            {
                o[(size_t)i * step] = acc / (2 * r + 1);
                acc += at(i + r + 1) - at(i - r);
            }
        }
    };
    pass(v, t, w, h, rx, w, 1);
    pass(t, v, h, w, ry, 1, w);
}

// Rays fan out from the light and lose strength in each opaque cell, so walls catch a lit rim
// and cast shadows behind them. Overlapping rays keep the brightest value, not the sum.
// The frame's lights are gathered first, then cast on several threads, each into its own buffer (the world is
// only read), and the buffers summed: the same picture in any order.
struct PointLight { float x, y, R; Color c; float I; };
static std::vector<PointLight> plist;
struct LightAcc
{
    std::vector<float> r, g, b, last;
    std::vector<int> stamp;
    int id = 0, i0 = 1 << 30, j0 = 1 << 30, i1 = -1, j1 = -1; // the texels it touched this frame
};
static std::vector<LightAcc> lacc;

static void pointLight(float x, float y, float R, Color c, float I)
{
    if (x + R < lox || y + R < loy || x - R > lox + lw * LS || y - R > loy + lh * LS) return;
    plist.push_back({x, y, R, c, I});
}

static void castLight(LightAcc& A, const PointLight& L)
{
    A.id++;
    float cr = L.c.r / 255.0f * L.I, cg = L.c.g / 255.0f * L.I, cb = L.c.b / 255.0f * L.I;
    A.i0 = std::max(0, std::min(A.i0, (int)((L.x - L.R - lox) / LS))); A.i1 = std::min(lw - 1, std::max(A.i1, (int)((L.x + L.R - lox) / LS) + 1));
    A.j0 = std::max(0, std::min(A.j0, (int)((L.y - L.R - loy) / LS))); A.j1 = std::min(lh - 1, std::max(A.j1, (int)((L.y + L.R - loy) / LS) + 1));
    int rays = std::max(48, (int)(L.R * 3.2f)); // enough that neighbouring rays never skip a light texel
    for (int k = 0; k < rays; k++)
    {
        float a = k * 6.2832f / rays, dx = std::cos(a), dy = std::sin(a), t = 1;
        for (float d = 0; d < L.R; d += (float)LS) // a step per light texel
        {
            int wx = (int)std::floor(L.x + dx * d), wy = (int)std::floor(L.y + dy * d);
            if (opaque(wx, wy) && (t *= 0.55f) < 0.04f) break;
            if (wx < lox || wy < loy) continue;
            int i = (wx - lox) / LS, j = (wy - loy) / LS;
            if (i >= lw || j >= lh) continue;
            float f = 1 - d / L.R, v = f * f * t;
            size_t q = (size_t)j * lw + i;
            if (A.stamp[q] != A.id) { A.stamp[q] = A.id; A.last[q] = 0; }
            if (v <= A.last[q]) continue;
            float inc = v - A.last[q];
            A.last[q] = v;
            A.r[q] += inc * cr; A.g[q] += inc * cg; A.b[q] += inc * cb;
        }
    }
}

// Casts every gathered light into lr/lg/lb.
static void castLights()
{
    int nt = workerCount();
    size_t n = (size_t)lw * lh;
    int use = std::min(nt, (int)plist.size());
    if ((int)lacc.size() < nt) lacc.resize(nt);
    for (int t = 0; t < use; t++)
        if (lacc[t].r.size() != n) { LightAcc& A = lacc[t]; for (auto* v : {&A.r, &A.g, &A.b, &A.last}) v->assign(n, 0); A.stamp.assign(n, 0); A.id = 0; }
    auto run = [&](int t) { for (size_t k = t; k < plist.size(); k += use) castLight(lacc[t], plist[k]); };
    parallelFor(use, run);
    for (int t = 0; t < use; t++) // sum each buffer's touched box into the map, clearing it for next frame
    {
        LightAcc& A = lacc[t];
        for (int j = A.j0; j <= A.j1; j++)
            for (int i = A.i0; i <= A.i1; i++)
            {
                size_t q = (size_t)j * lw + i;
                lr[q] += A.r[q]; lg[q] += A.g[q]; lb[q] += A.b[q];
                A.r[q] = A.g[q] = A.b[q] = 0;
            }
        A.i0 = A.j0 = 1 << 30; A.i1 = A.j1 = -1;
    }
    plist.clear();
}

static void buildLight(int cx, int cy)
{
    int nw = G.vw / LS + 2, nh = G.vh / LS + 2;
    if (nw != lw || nh != lh)
    {
        lw = nw; lh = nh;
        if (lightTex.id) UnloadTexture(lightTex);
        Image img = GenImageColor(lw, lh, WHITE);
        lightTex = LoadTextureFromImage(img);
        UnloadImage(img);
        SetTextureFilter(lightTex, TEXTURE_FILTER_BILINEAR);
        size_t n = (size_t)lw * lh;
        for (auto* v : {&lr, &lg, &lb, &lsky, &er, &eg, &eb}) v->assign(n, 0);
        lacc.clear(); // their buffers are the map's size
        lpix.assign(n, WHITE);
    }
    lox = cx - ((cx % LS) + LS) % LS; // texels stay locked to the world grid, so light doesn't crawl as the camera moves
    loy = cy - ((cy % LS) + LS) % LS;
    size_t n = (size_t)lw * lh;

    bool outdoors = !G.sandbox; // moonlight reaches wherever there's open sky above, in any biome
    float amb[3] = {0.11f, 0.11f, 0.14f};
    if (G.sandbox) amb[0] = amb[1] = amb[2] = 0.85f;

    // moonlight: open sky down to the first opaque cell in each column, then a short fall-off into the ground
    std::fill(lsky.begin(), lsky.end(), 0.0f);
    if (outdoors)
    {
        ltop.assign(lw, 0);
        lwet.assign(lw, 1 << 30);
        int nt = workerCount();
        parallelFor(nt, [&](int t) { // each column on its own
            for (int i = lw * t / nt; i < lw * (t + 1) / nt; i++)
            {
                int x = lox + i * LS + LS / 2, y = 0;
                while (y < world.hU() && !opaque(x, y))
                {
                    if (lwet[i] > y && props(world.get(x * world.scale, y * world.scale).material).kind == Kind::Liquid) lwet[i] = y;
                    if (y - lwet[i] > 260) break; // the moonlight is gone by here: no need to sound the rest of the deep
                    y++;
                }
                ltop[i] = y;
            }
        });
        for (int j = 0; j < lh; j++)
            for (int i = 0; i < lw; i++)
            {
                int y = loy + j * LS + LS / 2;
                float sea = std::max(0.0f, 1 - std::max(0, std::min(y, ltop[i]) - lwet[i]) / 260.0f); // deep water swallows the moonlight
                lsky[(size_t)j * lw + i] = sea * (y < ltop[i] ? 1.0f : std::max(0.0f, 1 - (y - ltop[i]) / 12.0f));
            }
        boxBlur(lsky, lw, lh, 4, 2); // soft edges: light spills a little way into doorways and overhangs
    }

    // glow from burning and molten cells
    std::fill(er.begin(), er.end(), 0.0f);
    std::fill(eg.begin(), eg.end(), 0.0f);
    std::fill(eb.begin(), eb.end(), 0.0f);
    bool anyEmit = false;
    { // in bands of whole light rows, one per thread (each writes only its own rows)
        int nt = workerCount();
        std::vector<char> any(nt, 0);
        auto band = [&](int t) {
            for (int jl = lh * t / nt; jl < lh * (t + 1) / nt; jl++)
                for (int j = jl * LS; j < jl * LS + LS; j++)
                    for (int i = 0; i < lw * LS; i++)
                    {
                        int x = lox + i, y = loy + j;
                        if (!world.inU(x, y)) continue;
                        const Cell& c = world.get(x * world.scale, y * world.scale);
                        Color gl = (c.flags & CF_BURNING) ? props(CellMaterial::Fire).glow : props(c.material).glow; // burning things glow like fire
                        if (!gl.a) continue;
                        size_t q = (size_t)jl * lw + i / LS;
                        er[q] += gl.r / 255.0f; eg[q] += gl.g / 255.0f; eb[q] += gl.b / 255.0f;
                        any[t] = 1;
                    }
        };
        parallelFor(nt, band);
        for (char a : any) anyEmit = anyEmit || a;
    }
    if (anyEmit) // the three channels blur side by side
    {
        std::vector<float>* ch[3] = {&er, &eg, &eb};
        parallelFor(3, [&](int k) { boxBlur(*ch[k], lw, lh, 7, 7); boxBlur(*ch[k], lw, lh, 7, 7); });
    }

    std::fill(lr.begin(), lr.end(), 0.0f);
    std::fill(lg.begin(), lg.end(), 0.0f);
    std::fill(lb.begin(), lb.end(), 0.0f);
    const Color warm = {255, 160, 80, 255};
    for (auto& it : G.inter)
    {
        float fl = 0.88f + 0.12f * hash2((int)it.x, G.frame / 4, 9);
        if (it.type == IT_TORCH) pointLight(it.x, it.y - 15, 90, warm, fl);
        else if (it.type == IT_LANTERN && !it.used) { Vector2 lp = lanternPos(it); pointLight(lp.x, lp.y + 4, 72, warm, fl); }
        else if (it.type == IT_SHRINE && !it.used) pointLight(it.x, it.y - 34, 56, {200, 190, 255, 255}, 0.7f);
        else if (it.type == IT_STONE && !it.used) pointLight(it.x, it.y - 16, 44, G.stoneLoot[it.data].glow, 0.7f);
    }
    for (auto& l : G.lamps) if (!l.smoke) pointLight(l.x, l.y, l.r, l.c, l.flame ? 0.88f + 0.12f * hash2((int)l.x, G.frame / 4, 9) : 0.85f);
    for (auto& p : G.projs)
        if (p.kind == PK_SPELL && p.spell != SP_DIG && p.spell != SP_BOMB) pointLight(p.x, p.y, 34, p.col, 0.9f);
    for (auto& pu : G.pickups) // Önd glows: you can find it in the dark of the deep
        if (pu.kind == PU_OND && pu.alive) pointLight(pu.b.x + 4, pu.b.y + 3, 44, {140, 230, 250, 255}, 0.85f);
    if (G.p.m.alive && G.p.hasWisp) pointLight(G.p.wx, G.p.wy, 150, {255, 238, 200, 255}, 0.8f); // Baldr's Offering
    if (G.p.m.alive) pointLight(G.p.m.cx(), G.p.m.cy() - 4, 64, {255, 236, 210, 255}, 0.5f); // enough to see your own feet
    for (auto& m : G.mobs) // anything on fire lights its surroundings
        if (m.burn > 0) pointLight(m.cx(), m.cy() - 4, 48, {255, 140, 50, 255}, 0.8f);
    if (G.p.m.alive && G.p.m.burn > 0) pointLight(G.p.m.cx(), G.p.m.cy() - 4, 70, {255, 140, 50, 255}, 0.95f);
    castLights();

    const float moon[3] = {0.66f, 0.72f, 0.92f};
    for (size_t q = 0; q < n; q++)
    {
        int wx = lox + (int)(q % lw) * LS, wy = loy + (int)(q / lw) * LS;
        if (world.inU(wx, wy) && world.skyOf(wx * world.scale, wy * world.scale)) { lpix[q] = WHITE; continue; } // the night sky keeps its own colours
        float e = 2.2f, s = lsky[q] * (1 - 0.6f * world.storm) + lsky[q] * world.flash * 1.4f;
        float r = amb[0] + s * moon[0] + lr[q] + std::min(1.0f, er[q] * e);
        float g = amb[1] + s * moon[1] + lg[q] + std::min(0.7f, eg[q] * e);
        float b = amb[2] + s * moon[2] + lb[q] + std::min(0.4f, eb[q] * e);
        lpix[q] = {(unsigned char)(std::min(1.0f, r) * 255), (unsigned char)(std::min(1.0f, g) * 255), (unsigned char)(std::min(1.0f, b) * 255), 255};
    }
    UpdateTexture(lightTex, lpix.data());
}

static void renderScene()
{
    int cx = G.rcx, cy = G.rcy;
    double pt = GetTime();
    world.pushX = G.p.m.cx() * SUB; world.pushY = (G.p.m.y + G.p.m.h) * SUB;
    renderWorld(pix.data(), cx * SUB, cy * SUB, G.vw * SUB, G.vh * SUB);
    UpdateTexture(worldTex, pix.data());
    profLap(pt, PF_WORLD);
    buildLight(cx, cy);
    profLap(pt, PF_LIGHT);
    prepareStallArt();
    BeginTextureMode(rt);
    ClearBackground(BLACK);
    DrawTexture(worldTex, 0, 0, WHITE);
    rlPushMatrix();
    rlScalef(SUB, SUB, 1); // entities draw in world units as before
    drawEntities(cx, cy);
    profLap(pt, PF_DRAW);
    BeginBlendMode(BLEND_MULTIPLIED);
    DrawTexturePro(lightTex, {0, 0, (float)lw, (float)lh}, {(float)(lox - cx), (float)(loy - cy), (float)lw * LS, (float)lh * LS}, {0, 0}, 0, WHITE);
    EndBlendMode();
    rlPopMatrix();
    EndTextureMode();
    float shake = G.shake * (G.reduceShake ? 0.2f : 1.0f);
    float sx = shake > 0.5f ? frange(-shake, shake) : 0;
    float sy = shake > 0.5f ? frange(-shake, shake) : 0;
    rlDrawRenderBatchActive();
    rlDisableColorBlend(); // copy the scene as-is; its alpha channel is meaningless after in-texture blending
    DrawTexturePro(rt.texture, {0, 0, (float)G.vw * SUB, -(float)G.vh * SUB},
                   {sx, sy, (float)G.vw * G.scale, (float)G.vh * G.scale}, {0, 0}, 0, WHITE);
    rlDrawRenderBatchActive();
    rlEnableColorBlend();
    if (G.sailT > 100) DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), {0, 0, 0, (unsigned char)std::min(255, (G.sailT - 100) * 4)});
}

static void centered(const char* s, float y, float size, Color c, int style = 0)
{
    uiTextCentered(s, GetScreenWidth() / 2.0f, y, size, c, style);
}

static void drawTitle()
{
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    float u = sh / 768.0f;
    DrawRectangleGradientV(0, 0, sw, sh, {14, 12, 26, 255}, {60, 26, 30, 255});
    double t = GetTime();
    for (int i = 0; i < 160; i++) // drifting embers
    {
        float x = std::fmod(hash2(i, 1, 7) * sw + std::sin(t * 0.7 + i) * 30, (float)sw);
        float y = sh - std::fmod((float)(t * (20 + 60 * hash2(i, 2, 7))) + hash2(i, 3, 7) * sh, (float)sh);
        DrawRectangle((int)x, (int)y, (int)(3 * u), (int)(3 * u), {255, (unsigned char)(120 + 100 * hash2(i, 4, 7)), 40, 200});
    }
    centered("SANDS OF SORCERY", sh * 0.18f, 84 * u, {240, 200, 110, 255}, 2);
    centered("a falling-sand roguelike of steel and spellcraft", sh * 0.18f + 96 * u, 24 * u, {210, 190, 170, 255});
    centered("Enter  -  Begin the descent", sh * 0.42f, 30 * u, RAYWHITE, 1);
    centered((std::to_string(META.bank) + " coins banked   |   " + std::to_string(META.runs) + " runs   |   deepest: stage " + std::to_string(META.deepest)).c_str(), sh * 0.42f + 120 * u, 18 * u, {220, 190, 120, 255});
    centered("S  -  Sandbox", sh * 0.42f + 44 * u, 26 * u, {200, 200, 200, 255}, 1);
    centered("Esc  -  Quit", sh * 0.42f + 82 * u, 22 * u, GRAY, 1);
    const char* help[] = {
        "A / D  move      W / SPACE  jump      hold toward a wall + W  climb      SPACE on a wall  wall-jump",
        "Left mouse  attack / cast      hold Right mouse  grappling hook  (W / S reel in / out)",
        "1-6 or wheel  switch item      Q  drink flask      G  drop item      F  interact      TAB  inventory & staff editing",
        "Mine ore with blades, bombs and digging bolts, then forge better gear at the sanctuary anvil.",
    };
    for (int i = 0; i < 4; i++) centered(help[i], sh * 0.68f + i * 28 * u, 18 * u, {190, 180, 170, 255});
}

static void drawOverlay(const char* title, const std::string& sub, const char* hint, Color tc)
{
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    float u = sh / 768.0f;
    DrawRectangle(0, 0, sw, sh, {0, 0, 0, 170});
    centered(title, sh * 0.3f, 64 * u, tc, 2);
    centered(sub.c_str(), sh * 0.3f + 80 * u, 24 * u, RAYWHITE);
    centered(hint, sh * 0.3f + 130 * u, 20 * u, GRAY);
}

// The loading screen, redrawn from inside the slow generators so the bar moves: `frac` 0..1, `what` the step in hand.
void loadStep(float frac, const char* what)
{
    if (!IsWindowReady() || IsWindowHidden()) return;
    float sw = (float)GetScreenWidth(), sh = (float)GetScreenHeight(), u = sh / 768.0f;
    BeginDrawing();
    DrawRectangle(0, 0, (int)sw, (int)sh, {10, 8, 14, 255});
    centered("Sailing for distant shores...", sh * 0.40f, 40 * u, {220, 200, 160, 255});
    float bw = sw * 0.42f, bh = 14 * u, bx = (sw - bw) / 2, by = sh * 0.40f + 90 * u;
    DrawRectangle((int)bx - 3, (int)by - 3, (int)bw + 6, (int)bh + 6, {70, 56, 40, 255});
    DrawRectangle((int)bx, (int)by, (int)bw, (int)bh, {26, 22, 28, 255});
    DrawRectangle((int)bx, (int)by, (int)(bw * std::min(1.0f, std::max(0.0f, frac))), (int)bh, {214, 170, 70, 255});
    DrawRectangle((int)bx, (int)by, (int)(bw * std::min(1.0f, std::max(0.0f, frac))), (int)(bh * 0.35f), {244, 214, 120, 255});
    centered(what, by + 34 * u, 20 * u, {170, 156, 130, 255});
    EndDrawing();
}

int main(int argc, char** argv)
{
    rngState() = (uint32_t)time(nullptr) * 2654435761u | 1u;
    if (argc > 1 && std::string(argv[1]) == "--selftest")
    {
        castSelfTest();
        collapseSelfTest();
        materialSelfTest();
        return 0;
    }
    if (argc > 2 && std::string(argv[1]) == "--dump")
    {
        dumpStages(argv[2]);
        return 0;
    }

    if (argc > 2 && std::string(argv[1]) == "--ui") // dev: <dir>/hud.png and inv.png (the HUD and the inventory, 1366x768)
    {
        SetConfigFlags(FLAG_WINDOW_HIDDEN);
        InitWindow(1366, 768, "ui");
        setupView();
        initUI();
        newGameKit(false);
        generateVillage();
        G.state = GS_PLAY;
        G.p.m.hp = G.p.m.maxHp * 0.7f;
        G.p.stamina = 62;
        for (int s = 0; s < SC_COUNT; s++) G.p.scrolls.push_back(s); // so the screenshot shows the scrolls
        G.p.hasMap = true;
        G.p.breath = 70;
        for (int f = 0; f < 60; f++) updateGame();
        for (auto& it : G.inter) // sail0..1.png: aboard the longship at the pier, then under way
            if (it.type == IT_BOAT && !it.used)
                for (int k = 0; k < 2; k++)
                {
                    G.sailT = 1 + k * 60;
                    for (int f = 0; f < 4; f++) updateGame();
                    G.camX = it.x - G.vw / 2.0f; G.camY = it.y - G.vh / 2.0f;
                    syncRenderCamera();
                    for (int r = 0; r < 2; r++) { BeginDrawing(); ClearBackground(BLACK); renderScene(); EndDrawing(); } // (the screen read lags a frame)
                    Image si = LoadImageFromScreen();
                    ExportImage(si, (std::string(argv[2]) + "/sail" + std::to_string(k) + ".png").c_str());
                    UnloadImage(si);
                }
        G.sailT = 0;
        for (int k = 0; k < 4; k++) // along the street: vil0..3.png
        {
            G.camX = 30 + k * 400; G.camY = G.p.m.cy() - G.vh / 2.0f;
            syncRenderCamera();
            BeginDrawing(); ClearBackground(BLACK); renderScene(); EndDrawing();
            Image img = LoadImageFromScreen();
            ExportImage(img, (std::string(argv[2]) + "/vil" + std::to_string(k) + ".png").c_str());
            UnloadImage(img);
        }
        for (int pass = 0; pass < 2; pass++)
        {
            G.state = pass ? GS_INVENTORY : GS_PLAY;
            G.camX = G.p.m.cx() - G.vw / 2.0f; G.camY = G.p.m.cy() - G.vh / 2.0f;
            syncRenderCamera();
            for (int f = 0; f < 3; f++)
            {
                BeginDrawing(); ClearBackground(BLACK); renderScene(); drawHUD();
                if (pass) updateDrawInventory();
                EndDrawing();
            }
            Image img = LoadImageFromScreen();
            ExportImage(img, (std::string(argv[2]) + (pass ? "/inv.png" : "/hud.png")).c_str());
            UnloadImage(img);
        }
        for (int shop = 0; shop < 3; shop++) // the three stalls' counters: shop0..2.png
        {
            G.state = GS_SHOP; G.shopId = shop; META.bank = 2500;
            for (int f = 0; f < 3; f++) { BeginDrawing(); ClearBackground(BLACK); renderScene(); drawHUD(); updateDrawShop(); EndDrawing(); }
            Image img = LoadImageFromScreen();
            ExportImage(img, (std::string(argv[2]) + "/shop" + std::to_string(shop) + ".png").c_str());
            UnloadImage(img);
        }
        return 0;
    }

    if (argc > 2 && std::string(argv[1]) == "--fx") // dev: close-ups of fire, torches and hangings, <dir>/fx0..3.png
    {
        SetConfigFlags(FLAG_WINDOW_HIDDEN);
        InitWindow(1366, 768, "fx");
        setupView();
        newGameKit(false);
        generateVillage();
        G.state = GS_PLAY;
        Player& P = G.p;
        P.m.x = 150;
        { int yy = (int)P.m.y - 80; while (world.get(150 * world.scale, yy * world.scale).material == CellMaterial::Empty) yy++; P.m.y = (float)yy - P.m.h; }
        float px = P.m.cx(), fy = P.m.y + P.m.h;
        for (int i = 0; i < 3; i++) // two torches and a hanging
        {
            G.inter.push_back({IT_TORCH, px - 40.0f + i * 22, fy});
        }
        Interact t{IT_DECOR, px + 30, fy - 52, false, DK_TAPESTRY, 8 + 16};
        t.w = 30;
        G.inter.push_back(t);
        for (int k = 0; k < 4; k++) { Interact tt{IT_DECOR, px + 62 + k * 24.0f, fy - 52, false, DK_TAPESTRY, k * 2 + (k << 3)}; tt.w = 26; G.inter.push_back(tt); }
        G.lamps.push_back({px - 60, fy - 3, 90, {255, 160, 80, 255}, true});
        Mob e = makeEnemy(0, px + 10, fy); e.burn = 200; e.facing = 1; G.mobs.push_back(e);
        P.m.burn = 200;
        paintCircle((int)(px - 90) * world.scale, (int)(fy - 6) * world.scale, 5 * world.scale, CellMaterial::Fire, true);
        for (int k = 0; k < 50; k++) updateGame();
        for (int i = 0; i < 4; i++)
        {
            for (int k = 0; k < 7; k++) { updateGame(); P.m.burn = 200; for (auto& mm : G.mobs) mm.burn = 200; }
            printf("burn %d mobs %d alive %d\n", P.m.burn, (int)G.mobs.size(), (int)P.m.alive);
            G.camX = px - 20; G.camY = fy - G.vh * 0.6f;
            syncRenderCamera();
            BeginDrawing(); renderScene(); EndDrawing();
            Image img = LoadImageFromTexture(rt.texture);
            ImageFlipVertical(&img);
            if (i < 2) ImageCrop(&img, {(px - 100 - G.rcx) * 2, (fy - 70 - G.rcy) * 2, 360, 160}); else ImageCrop(&img, {(px - 8 - G.rcx) * 2, (fy - 34 - G.rcy) * 2, 90, 80});
            ImageResizeNN(&img, img.width * (i < 2 ? 3 : 7), img.height * (i < 2 ? 3 : 7));
            ExportImage(img, (std::string(argv[2]) + "/fx" + std::to_string(i) + ".png").c_str());
            UnloadImage(img);
        }
        return 0;
    }

    if (argc > 2 && std::string(argv[1]) == "--sea") // dev: the nearest wreck's chest, anchor and the opening animation, <dir>/sea0..3.png
    {
        SetConfigFlags(FLAG_WINDOW_HIDDEN);
        InitWindow(1366, 768, "sea");
        setupView();
        newGameKit(false);
        startRun();
        G.state = GS_PLAY;
        Interact* chest = nullptr;
        for (auto& it : G.inter) if (it.type == IT_CHEST && it.style == 1 && (!chest || (argc > 3 ? it.y > chest->y : it.x > chest->x))) chest = &it; // (a third argument: the deepest one)
        if (!chest) { printf("no sea chest\n"); return 1; }
        for (int i = 0; i < 4; i++)
        {
            if (i == 1) { chest->used = true; chest->fade = 84; }
            else if (i > 1) for (int k = 0; k < 28; k++) updateGame();
            G.camX = chest->x - 130; G.camY = chest->y - 130;
            syncRenderCamera();
            BeginDrawing(); renderScene(); EndDrawing();
            Image img = LoadImageFromTexture(rt.texture);
            ImageFlipVertical(&img);
            ImageCrop(&img, {(chest->x - 100 - G.rcx) * 2, (chest->y - 110 - G.rcy) * 2, 420, 240});
            ImageResizeNN(&img, img.width * 3, img.height * 3);
            ExportImage(img, (std::string(argv[2]) + "/sea" + std::to_string(i) + ".png").c_str());
            UnloadImage(img);
        }
        return 0;
    }

    if (argc > 2 && std::string(argv[1]) == "--door") // dev: a farmhouse door closed, mid-swing and open, and spell pickups, <dir>/door0..3.png
    {
        SetConfigFlags(FLAG_WINDOW_HIDDEN);
        InitWindow(1366, 768, "door");
        setupView();
        newGameKit(false);
        startRun();
        G.state = GS_PLAY;
        Interact* door = nullptr;
        for (auto& it : G.inter) if (it.type == IT_CRATE && it.style > 0 && (!door || it.x < door->x)) door = &it;
        if (!door) { printf("no door\n"); return 1; }
        for (int s = 0; s < SC_COUNT; s++) addPickup(door->x + 30 + s * 14, door->y - 30, PU_SCROLL), G.pickups.back().spell = s;
        for (int i = 0; i < 4; i++)
        {
            if (i == 1) { door->used = true; door->fade = 1; }
            if (i == 2) door->fade = 14;
            if (i == 3) door->fade = 30;
            for (int k = 0; k < (i == 0 ? 40 : 2); k++) { updateGame(); if (i > 1) door->fade = i == 2 ? 14 : 30; }
            G.camX = door->x - 60; G.camY = door->y - 80;
            syncRenderCamera();
            BeginDrawing(); renderScene(); EndDrawing();
            Image img = LoadImageFromTexture(rt.texture);
            ImageFlipVertical(&img);
            ImageCrop(&img, {(door->x - 20 - G.rcx) * 2, (door->y - 50 - G.rcy) * 2, 360, 120});
            ImageResizeNN(&img, img.width * 3, img.height * 3);
            ExportImage(img, (std::string(argv[2]) + "/door" + std::to_string(i) + ".png").c_str());
            UnloadImage(img);
        }
        return 0;
    }

    if (argc > 2 && std::string(argv[1]) == "--vik") // dev: the Viking in every weapon and state, <dir>/vik0.png...
    {
        SetConfigFlags(FLAG_WINDOW_HIDDEN);
        InitWindow(1366, 768, "vik");
        setupView();
        newGameKit(false);
        generateVillage();
        G.state = GS_PLAY;
        Player& P = G.p;
        P.m.x = 150;
        { int yy = (int)P.m.y - 80; while (world.get(150 * world.scale, yy * world.scale).material == CellMaterial::Empty) yy++; P.m.y = (float)yy - P.m.h; }
        float px = P.m.cx(), fy = P.m.y + P.m.h;
        struct Shot { std::string name; std::function<void()> setup; };
        std::vector<Shot> shots;
        auto reset = [&]() {
            P.rollT = 0; P.prone = P.crouch = P.climb = false; P.onWall = 0; P.hook = 0; P.swingT = 0; P.combatT = 0; P.recoil = 0; P.squash = 1;
            P.m.onGround = true; P.m.inLiquid = false; P.m.vx = P.m.vy = 0; P.m.facing = 1; P.m.iframes = 0; P.m.hurtFlash = 0; P.aim = 0; P.m.y = fy - P.m.h; P.armour = -1;
        };
        int types[] = {W_DAGGER, W_SWORD, W_AXE, W_SPEAR, W_MACE, W_PAN, W_CROSSBOW, W_STAFF};
        const char* tn[] = {"dagger", "sword", "axe", "spear", "mace", "pan", "crossbow", "staff"};
        auto weapon = [&](int t) { Weapon w; w.type = t; w.metal = M_IRON; if (t == W_STAFF) w.staff.gem = SKYBLUE; P.hotbar = {w}; P.sel = 0; };
        for (int i = 0; i < 8; i++) shots.push_back({std::string("idle_") + tn[i], [&, i]() { weapon(types[i]); }});
        for (int i = 0; i < 8; i++) shots.push_back({std::string("run_") + tn[i], [&, i]() { weapon(types[i]); P.m.vx = 1.5f; P.runPhase = 0.8f * i; }});
        for (int k = 0; k < 8; k++) shots.push_back({"axe_atk" + std::to_string(k), [&, k]() { weapon(W_AXE); P.atkStyle = ATK_CHOP; P.atkLen = 22; P.atkHitAt = 10; P.combo = 0; P.swingT = 22 - k * 3; P.combatT = 60; }});
        for (int k = 0; k < 6; k++) shots.push_back({"sword_atk" + std::to_string(k), [&, k]() { weapon(W_SWORD); P.atkStyle = ATK_SWEEP; P.atkLen = 16; P.atkHitAt = 5; P.combo = 0; P.swingT = 16 - k * 3; P.combatT = 60; }});
        for (int k = 0; k < 5; k++) shots.push_back({"spear_atk" + std::to_string(k), [&, k]() { weapon(W_SPEAR); P.atkStyle = ATK_THRUST; P.atkLen = 15; P.atkHitAt = 5; P.combo = 0; P.swingT = 15 - k * 3; P.combatT = 60; }});
        shots.push_back({"roll_in", [&]() { weapon(W_AXE); P.rollT = 18; }});
        for (int k = 0; k < 4; k++) shots.push_back({"roll" + std::to_string(k), [&, k]() { weapon(W_AXE); P.rollT = 15 - k * 3; }});
        shots.push_back({"crawl", [&]() { weapon(W_AXE); P.prone = true; }});
        shots.push_back({"crouch", [&]() { weapon(W_AXE); P.crouch = true; }});
        shots.push_back({"climb", [&]() { weapon(W_AXE); P.climb = true; }});
        shots.push_back({"jump", [&]() { weapon(W_AXE); P.m.onGround = false; P.m.vy = -1.5f; P.m.y = fy - P.m.h - 12; }});
        shots.push_back({"fall", [&]() { weapon(W_AXE); P.m.onGround = false; P.m.vy = 1.5f; P.m.y = fy - P.m.h - 12; }});
        shots.push_back({"swim", [&]() { weapon(W_AXE); P.m.onGround = false; P.m.inLiquid = true; P.m.vx = 1.0f; P.m.y = fy - P.m.h - 6; }});
        shots.push_back({"wall", [&]() { weapon(W_AXE); P.onWall = 1; }});
        shots.push_back({"hang", [&]() { weapon(W_AXE); P.hook = 2; P.hx = px + 6; P.hy = fy - 60; P.m.onGround = false; P.m.y = fy - P.m.h - 8; }});
        for (int k = 0; k < 5; k++) shots.push_back({"xbow_aim" + std::to_string(k), [&, k]() { weapon(W_CROSSBOW); P.combatT = 60; P.aim = (-1.2f + k * 0.6f); P.m.facing = 1; }});
        shots.push_back({"xbow_fire", [&]() { weapon(W_CROSSBOW); P.combatT = 60; P.aim = -0.3f; P.recoil = 4; }});
        shots.push_back({"staff_aim", [&]() { weapon(W_STAFF); P.combatT = 60; P.aim = -0.5f; }});
        shots.push_back({"hookaim", [&]() { weapon(W_AXE); P.hook = 1; P.hx = px + 40; P.hy = fy - 40; }});
        shots.push_back({"left_axe", [&]() { weapon(W_AXE); P.m.facing = -1; }});
        for (int k = 0; k < 4; k++) shots.push_back({"armour_run" + std::to_string(k), [&, k]() { weapon(W_SWORD); P.armour = M_IRON; P.m.vx = 1.5f; P.runPhase = k * 1.5f; }});
        for (int am : std::initializer_list<int>{M_COPPER, M_IRON, M_STEEL, 4, 8}) shots.push_back({"armour" + std::to_string(am), [&, am]() { weapon(W_SWORD); P.armour = am; }});
        const int CW = 150, CH = 120, COLS = 8;
        int rows = ((int)shots.size() + COLS - 1) / COLS;
        Image sheet = GenImageColor(CW * COLS, CH * rows, {24, 18, 36, 255});
        for (size_t i = 0; i < shots.size(); i++)
        {
            reset();
            shots[i].setup();
            G.frame = 12 + (int)i * 3;
            G.camX = px - 40; G.camY = fy - G.vh * 0.6f;
            syncRenderCamera();
            BeginDrawing(); renderScene(); EndDrawing();
            Image img = LoadImageFromTexture(rt.texture);
            ImageFlipVertical(&img);
            ImageCrop(&img, {(px - 37 - G.rcx) * 2, (fy - 52 - G.rcy) * 2, (float)CW, (float)CH});
            ImageDraw(&sheet, img, {0, 0, (float)CW, (float)CH}, {(float)(i % COLS) * CW, (float)(i / COLS) * CH, (float)CW, (float)CH}, WHITE);
            UnloadImage(img);
            printf("%d %s\n", (int)i, shots[i].name.c_str());
        }
        ImageResizeNN(&sheet, sheet.width * 2, sheet.height * 2);
        ExportImage(sheet, (std::string(argv[2]) + "/vik.png").c_str());
        return 0;
    }

    if (argc > 2 && std::string(argv[1]) == "--scr") // dev: read each scroll in turn and watch it fly, <dir>/scr.png
    {
        SetConfigFlags(FLAG_WINDOW_HIDDEN);
        InitWindow(1366, 768, "scr");
        setupView();
        newGameKit(false);
        generateVillage();
        G.state = GS_PLAY;
        Player& P = G.p;
        P.m.x = 150;
        { int yy = (int)P.m.y - 80; while (world.get(150 * world.scale, yy * world.scale).material == CellMaterial::Empty) yy++; P.m.y = (float)yy - P.m.h; }
        float px = P.m.cx(), fy = P.m.y + P.m.h;
        const int CW = 300, CH = 170, COLS = 3;
        Image sheet = GenImageColor(CW * COLS * 2, CH * 4, {24, 18, 36, 255});
        int n = 0;
        for (int sc = 0; sc < SC_COUNT; sc++)
            for (int ph = 0; ph < 2; ph++)
            {
                if (ph == 0)
                {
                    G.projs.clear();
                    P.scrolls = {sc};
                    P.scrollSel = 0;
                    P.aim = -0.25f;
                    P.m.facing = 1;
                    readScroll();
                    for (int k = 0; k < 6; k++) updateGame();
                }
                else for (int k = 0; k < 14; k++) updateGame();
                G.camX = px - 40; G.camY = fy - G.vh * 0.6f;
                syncRenderCamera();
                BeginDrawing(); renderScene(); EndDrawing();
                Image img = LoadImageFromTexture(rt.texture);
                ImageFlipVertical(&img);
                ImageCrop(&img, {(px - 30 - G.rcx) * 2, (fy - 70 - G.rcy) * 2, (float)CW, (float)CH});
                ImageDraw(&sheet, img, {0, 0, (float)CW, (float)CH}, {(float)((n % (COLS * 2)) * CW), (float)((n / (COLS * 2)) * CH), (float)CW, (float)CH}, WHITE);
                UnloadImage(img);
                n++;
            }
        ImageResizeNN(&sheet, sheet.width * 1, sheet.height * 1);
        ExportImage(sheet, (std::string(argv[2]) + "/scr.png").c_str());
        return 0;
    }

    if (argc > 2 && std::string(argv[1]) == "--corpse") // dev: each starter foe killed and let fall, <dir>/corpse.png
    {
        SetConfigFlags(FLAG_WINDOW_HIDDEN);
        InitWindow(1366, 768, "corpse");
        setupView();
        newGameKit(false);
        generateVillage();
        G.state = GS_PLAY;
        Player& P = G.p;
        P.m.x = 150;
        { int yy = (int)P.m.y - 80; while (world.get(150 * world.scale, yy * world.scale).material == CellMaterial::Empty) yy++; P.m.y = (float)yy - P.m.h; }
        float fy = P.m.y + P.m.h;
        int types[] = {E_GOBLIN, E_BOMBER, E_REDCAP, E_RAIDER, E_RISEN, E_BAT, E_SLIME, E_SCORPION, E_SERPENT, E_WOLF};
        const int CW = 200, CH = 110;
        Image sheet = GenImageColor(CW * 5, CH * 2, {24, 18, 36, 255});
        int n = 0;
        for (int t : types)
        {
            G.corpses.clear(); G.mobs.clear();
            P.m.x = 150; P.m.y = fy - P.m.h;
            float x = 150 + 22;
            Mob e = makeEnemy(t, x, fy - 36);
            e.facing = 1; e.vx = 1.0f; e.hp = 0;
            spawnCorpse(e);
            for (int k = 0; k < 90; k++) updateGame();
            G.camX = x - 50; G.camY = fy - G.vh * 0.6f;
            syncRenderCamera();
            BeginDrawing(); renderScene(); EndDrawing();
            Image img = LoadImageFromTexture(rt.texture);
            ImageFlipVertical(&img);
            ImageCrop(&img, {(x - 40 - G.rcx) * 2, (fy - 50 - G.rcy) * 2, (float)CW, (float)CH});
            ImageDraw(&sheet, img, {0, 0, (float)CW, (float)CH}, {(float)((n % 5) * CW), (float)((n / 5) * CH), (float)CW, (float)CH}, WHITE);
            UnloadImage(img);
            n++;
        }
        ImageResizeNN(&sheet, sheet.width * 2, sheet.height * 2);
        ExportImage(sheet, (std::string(argv[2]) + "/corpse.png").c_str());
        return 0;
    }

    if (argc > 2 && std::string(argv[1]) == "--decor") { exportDecorSheet(argv[2]); return 0; } // dev: a contact sheet of every decor kind, 3x
    if (argc > 2 && std::string(argv[1]) == "--terr") // dev: lit shots of the run's terrain and buildings along the surface, <dir>/terr0..N.png (x offsets from argv[3...])
    {
        SetConfigFlags(FLAG_WINDOW_HIDDEN);
        InitWindow(1366, 768, "terr");
        setupView();
        newGameKit(false);
        startRun();
        G.state = GS_PLAY;
        printf("start x=%d seaEnd=%d duneEnd=%d storm=%d..%d world=%d\n", (int)G.p.m.x, G.seaEnd, G.duneEnd, G.stormX0, G.stormX1, world.w / world.scale);
        std::vector<float> xs;
        for (int i = 3; i < argc; i++) xs.push_back((float)atof(argv[i]));
        if (xs.empty()) xs = {0, 250, 500, 800, 1100, 1500};
        for (size_t i = 0; i < xs.size(); i++)
        {
            float x = xs[i];
            if (x <= -1000) { int n = (int)(-x - 1000); x = 5000; for (auto& it : G.inter) if (it.type == IT_DECOR && it.data == DK_TARGET && n-- == 0) { x = it.x; break; } } // -1000-n: the n-th archery butt
            else if (x < 0) { int n = (int)-x - 1; x = 5000; for (auto& it : G.inter) if (it.type == IT_DECOR && it.data == DK_SPEARPOST && n-- == 0) { x = it.x; break; } } // negative: the n-th planted spear
            int yy = 0; while (yy < world.h / world.scale - 1 && world.get((int)x * world.scale, yy * world.scale).material == CellMaterial::Empty) yy++;
            G.p.m.x = x; G.p.m.y = (float)yy - G.p.m.h - 2;
            G.camX = x - G.vw / 2.0f; G.camY = (float)yy - G.vh * 0.55f + (getenv("TERR_DY") ? (float)atof(getenv("TERR_DY")) : 0.0f);
            syncRenderCamera();
            for (int k = 0; k < 3; k++) { BeginDrawing(); renderScene(); EndDrawing(); }
            Image img = LoadImageFromTexture(rt.texture);
            ImageFlipVertical(&img);
            ExportImage(img, (std::string(argv[2]) + "/terr" + std::to_string(i) + ".png").c_str());
            UnloadImage(img);
            printf("terr%d x=%d surf=%d\n", (int)i, (int)x, yy);
        }
        return 0;
    }

    if (argc > 2 && std::string(argv[1]) == "--shot") // dev: lit screenshots of Hearthwick's stalls, <dir>/stall0..2.png
    {
        SetConfigFlags(FLAG_WINDOW_HIDDEN);
        InitWindow(1366, 768, "shot");
        setupView();
        newGameKit(false);
        generateVillage();
        G.state = GS_PLAY;
        for (int page = 0; page < 2; page++) // every enemy beside the player, in two line-ups (lineup0/1.png, 3x)
        {
            float fx = G.p.m.cx() + 24, fy = G.p.m.y + G.p.m.h, x = fx;
            for (int t = page * 11; t < std::min((int)ENEMY_COUNT, page * 11 + 11); t++)
            {
                Mob e = makeEnemy(t, x, fy - (ENEMIES[t].ai == AI_FLY || ENEMIES[t].ai == AI_FLYCAST ? 14 : 0));
                e.y = fy - e.h - (ENEMIES[t].ai == AI_FLY || ENEMIES[t].ai == AI_FLYCAST ? 8 : 0);
                e.facing = 1; e.anim = 0;
                G.mobs.push_back(e);
                x += e.w + 18;
            }
            G.camX = G.p.m.cx() - 20; G.camY = fy - G.vh * 0.6f;
            syncRenderCamera();
            BeginDrawing(); renderScene(); EndDrawing();
            Image img = LoadImageFromTexture(rt.texture);
            ImageFlipVertical(&img);
            ImageCrop(&img, {(G.p.m.cx() - 12 - G.rcx) * 2, (fy - 80 - G.rcy) * 2, std::min((x - G.p.m.cx() + 16) * 2, (float)img.width), 180});
            ImageResizeNN(&img, img.width * 3, img.height * 3);
            ExportImage(img, (std::string(argv[2]) + "/lineup" + std::to_string(page) + ".png").c_str());
            UnloadImage(img);
            G.mobs.clear();
        }
        { // combat.png: each weapon wound up, landing and following through; icons.png; pickups.png
            Player& P = G.p;
            int types[] = {W_DAGGER, W_SWORD, W_SPEAR, W_AXE, W_MACE, W_PAN, W_CROSSBOW, W_STAFF};
            const int CW = 120, CH = 84;
            Image sheet = GenImageColor(CW * 3, CH * 8, BLACK);
            auto grab = [&](int col, int row) {
                G.camX = P.m.cx() - G.vw / 2.0f; G.camY = P.m.cy() - G.vh / 2.0f;
                syncRenderCamera();
                BeginDrawing(); renderScene(); EndDrawing();
                Image img = LoadImageFromTexture(rt.texture);
                ImageFlipVertical(&img);
                int px = (int)(P.m.cx() - G.rcx) * 2, py = (int)(P.m.cy() - G.rcy) * 2;
                ImageDraw(&sheet, img, {(float)px - CW / 2 + 10, (float)py - CH / 2 - 6, (float)CW, (float)CH}, {(float)col * CW, (float)row * CH, (float)CW, (float)CH}, WHITE);
                UnloadImage(img);
            };
            P.m.facing = 1; P.aim = -0.15f; P.combatT = 100;
            for (int r = 0; r < 8; r++)
            {
                Weapon w; w.type = types[r]; w.metal = M_IRON;
                if (w.type == W_STAFF) w.staff.gem = SKYBLUE;
                P.hotbar = {w}; P.sel = 0;
                for (int c = 0; c < 3; c++)
                {
                    P.swingT = 0;
                    if (isMelee(w.type))
                    {
                        P.comboT = 0; P.combo = 2; // so this is the chain's first blow
                        startAttack(w);
                        int at = c == 0 ? P.atkHitAt / 2 : (c == 1 ? P.atkHitAt : P.atkHitAt + 3);
                        P.swingT = P.atkLen - at;
                    }
                    else P.recoil = c == 1 ? 6 : 0;
                    grab(c, r);
                }
            }
            P.swingT = 0;
            ExportImage(sheet, (std::string(argv[2]) + "/combat.png").c_str());
            UnloadImage(sheet);
            RenderTexture2D icons = LoadRenderTexture(8 * 64, 64);
            BeginTextureMode(icons);
            ClearBackground({28, 26, 38, 255});
            for (int i = 0; i < 8; i++)
            {
                Weapon w; w.type = types[i]; w.metal = i % 2 ? M_STEEL : M_COPPER;
                if (w.type == W_STAFF) w.staff.gem = SKYBLUE;
                drawItemIcon(w, i * 64 + 6.0f, 6, 52);
            }
            EndTextureMode();
            Image ic = LoadImageFromTexture(icons.texture);
            ImageFlipVertical(&ic);
            ExportImage(ic, (std::string(argv[2]) + "/icons.png").c_str());
            UnloadImage(ic);
            for (int i = 0; i < 8; i++)
            {
                Weapon w; w.type = types[i]; w.metal = i % 2 ? M_STEEL : M_FIRESTONE;
                if (w.type == W_STAFF) w.staff.gem = SKYBLUE;
                addPickupWeapon(P.m.cx() + 14 + i * 14, P.m.y + P.m.h - 10, w);
            }
            for (int k = 0; k < 90; k++) updateGame(); // let them land
            P.hotbar.clear();
            G.camX = P.m.cx() - 20; G.camY = P.m.cy() - G.vh / 2.0f;
            syncRenderCamera();
            BeginDrawing(); renderScene(); EndDrawing();
            Image pk = LoadImageFromTexture(rt.texture);
            ImageFlipVertical(&pk);
            ImageCrop(&pk, {(float)(P.m.cx() - G.rcx) * 2 - 20, (float)(P.m.cy() - G.rcy) * 2 - 40, 300, 80});
            ExportImage(pk, (std::string(argv[2]) + "/pickups.png").c_str());
            UnloadImage(pk);
            G.pickups.clear();
        }
        { // cave.png: a stretch of the Greenmarch caves, lit
            startRun();
            float px = G.p.m.x, py = G.p.m.y;
            for (int i = 0; i < 30; i++) updateGame();
            G.camX = px + 900; G.camY = py + 120;
            syncRenderCamera();
            G.rcx = (int)G.camX; G.rcy = (int)G.camY;
            BeginDrawing(); renderScene(); EndDrawing();
            Image img = LoadImageFromTexture(rt.texture);
            ImageFlipVertical(&img);
            ExportImage(img, (std::string(argv[2]) + "/cave.png").c_str());
            UnloadImage(img);
            for (int k = 0; k < 4; k++) // plains0..3.png: the parallax backdrop along the road
            {
                G.camX = px + k * 1300 - G.vw / 2.0f; G.camY = py - G.vh / 2.0f;
                syncRenderCamera();
                G.rcx = (int)G.camX; G.rcy = (int)G.camY;
                BeginDrawing(); renderScene(); EndDrawing();
                Image pi = LoadImageFromTexture(rt.texture);
                ImageFlipVertical(&pi);
                ExportImage(pi, (std::string(argv[2]) + "/plains" + std::to_string(k) + ".png").c_str());
                UnloadImage(pi);
            }
            { // castle0..5.png: the keep from outside, its roofs, then the halls
                int kx = (int)px;
                while (kx < world.w / world.scale - 2 && world.get(kx * world.scale, (int)(py - 120) * world.scale).material != CellMaterial::Masonry) kx++;
                struct V { float x, y; } views[] = {{kx - 60.0f, py - 420}, {kx + 380.0f, py - 420}, {kx + 150.0f, py - 100}, {kx + 480.0f, py - 100}, {kx + 820.0f, py - 100}, {kx + 1160.0f, py - 100}, {kx + 1500.0f, py - 100}, {kx + 1840.0f, py - 100}};
                for (int k = 0; k < 8; k++)
                {
                    G.camX = views[k].x; G.camY = views[k].y;
                    syncRenderCamera();
                    G.rcx = (int)G.camX; G.rcy = (int)G.camY;
                    BeginDrawing(); renderScene(); EndDrawing();
                    Image ci = LoadImageFromTexture(rt.texture);
                    ImageFlipVertical(&ci);
                    ExportImage(ci, (std::string(argv[2]) + "/castle" + std::to_string(k) + ".png").c_str());
                    UnloadImage(ci);
                }
            }
            for (auto& t : G.traps) // trap.png: the first dart trap, whole and then broken
            {
                if (t.type != TR_ARROW) continue;
                G.camX = t.x - G.vw / 2.0f; G.camY = t.y - G.vh / 2.0f;
                syncRenderCamera();
                G.rcx = (int)G.camX; G.rcy = (int)G.camY;
                const char* names[2] = {"/trap.png", "/trap_broken.png"};
                for (int k = 0; k < 2; k++)
                {
                    t.done = k == 1;
                    BeginDrawing(); renderScene(); EndDrawing();
                    Image ti = LoadImageFromTexture(rt.texture);
                    ImageFlipVertical(&ti);
                    ImageCrop(&ti, {(float)(t.x - G.rcx) * 2 - 120, (float)(t.y - G.rcy) * 2 - 60, 240, 120});
                    ExportImage(ti, (std::string(argv[2]) + names[k]).c_str());
                    UnloadImage(ti);
                }
                break;
            }
            generateVillage();
        }
        for (auto& it : G.inter)
        {
            if (it.type != IT_SHOP) continue;
            G.camX = it.x - G.vw / 2.0f;
            G.camY = it.y - G.vh * 0.62f;
            syncRenderCamera();
            BeginDrawing();
            renderScene();
            EndDrawing();
            Image img = LoadImageFromTexture(rt.texture);
            ImageFlipVertical(&img);
            ExportImage(img, (std::string(argv[2]) + "/stall" + std::to_string(it.data) + ".png").c_str());
            UnloadImage(img);
            std::printf("stall%d at %d,%d\n", it.data, (int)it.x - G.rcx, (int)it.y - G.rcy);
        }
        CloseWindow();
        return 0;
    }

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(1366, 768, "Sands of Sorcery");
    SetExitKey(KEY_NULL);
    initAudio();
    initUI();
    loadMeta();
    SetTargetFPS(60);
    bool quit = false;

    GameState lastState = G.state;
    int stateAge = 0; // frames since the state last changed; menus ignore close keys on their first frames

    while (!WindowShouldClose() && !quit)
    {
        if (IsKeyPressed(KEY_F11)) ToggleBorderlessWindowed();
        setupView();
        updateAudio(G.state != GS_TITLE && G.state != GS_LOADING);
        if (G.state != lastState) { lastState = G.state; stateAge = 0; }
        else stateAge++;

        switch (G.state)
        {
        case GS_TITLE:
            if (IsKeyPressed(KEY_ENTER)) { newGameKit(false); G.loadTarget = LOAD_VILLAGE; G.state = GS_LOADING; }
            if (IsKeyPressed(KEY_S)) { newGameKit(true); G.loadTarget = LOAD_SANDBOX; G.state = GS_LOADING; }
            if (IsKeyPressed(KEY_ESCAPE)) quit = true;
            break;
        case GS_PLAY:
        {
            bool mapUp = G.devMap;
            devUpdate();
            if (mapUp) break; // the dev map has the keys and the mouse; the world waits
            updateGame();
            if (G.state == GS_PLAY && (IsKeyPressed(KEY_TAB) || IsKeyPressed(KEY_I))) { G.state = GS_INVENTORY; G.invSel = G.p.sel; }
            else if (G.state == GS_PLAY && IsKeyPressed(KEY_ESCAPE)) G.state = GS_PAUSE;
            if (IsKeyPressed(KEY_F1)) G.showHelp = !G.showHelp;
            break;
        }
        case GS_INVENTORY:
            if (stateAge > 3 && (IsKeyPressed(KEY_TAB) || IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_I)))
            {
                cancelInventoryDrag();
                G.state = GS_PLAY;
            }
            break;
        case GS_ANVIL:
        case GS_SHOP:
            if (stateAge > 3 && (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_TAB))) G.state = GS_PLAY;
            break;
        case GS_PAUSE:
            if (IsKeyPressed(KEY_ESCAPE)) G.state = GS_PLAY;
            if (IsKeyPressed(KEY_Q)) G.state = GS_TITLE;
            if (IsKeyPressed(KEY_MINUS)) G.uiScale = std::max(0.75f, G.uiScale - 0.1f);
            if (IsKeyPressed(KEY_EQUAL)) G.uiScale = std::min(1.6f, G.uiScale + 0.1f);
            if (IsKeyPressed(KEY_K)) G.reduceShake = !G.reduceShake;
            if (IsKeyPressed(KEY_R) && !G.inVillage) { returnToRoad(); G.state = GS_PLAY; }
            break;
        case GS_DEAD:
        case GS_WIN:
            if (IsKeyPressed(KEY_ENTER))
            {
                bool wasSandbox = G.sandbox;
                newGameKit(wasSandbox);
                G.loadTarget = wasSandbox ? LOAD_SANDBOX : LOAD_VILLAGE;
                G.state = GS_LOADING;
            }
            break;
        default:
            break;
        }

        if (G.state == GS_PLAY && !G.devMap) { if (!IsCursorHidden()) HideCursor(); }
        else if (IsCursorHidden()) ShowCursor();

        BeginDrawing();
        ClearBackground(BLACK);
        if (G.state == GS_TITLE)
            drawTitle();
        else if (G.state == GS_LOADING)
        {
            DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), {10, 8, 14, 255});
            const char* where = G.loadTarget == LOAD_VILLAGE ? "Returning to Hearthwick..." : G.loadTarget == LOAD_SANDBOX ? "Shaping the sandbox..." : G.stage == 0 ? "Sailing for distant shores..." : "Descending...";
            centered(where, GetScreenHeight() * 0.45f, 40 * GetScreenHeight() / 768.0f, {220, 200, 160, 255});
        }
        else
        {
            renderScene();
            double ht = GetTime();
            drawHUD();
            profLap(ht, PF_HUD);
            if (G.state == GS_PLAY) devDraw();
            switch (G.state)
            {
            case GS_INVENTORY: updateDrawInventory(); break;
            case GS_ANVIL: updateDrawAnvil(); break;
            case GS_SHOP: updateDrawShop(); break;
            case GS_PAUSE:
            {
                char sub[160];
                std::snprintf(sub, sizeof(sub), "UI size %d%%  ( - / = )      Screen shake: %s  ( K )", (int)std::lround(G.uiScale * 100), G.reduceShake ? "reduced" : "full");
                drawOverlay("Paused", sub, "Esc  resume     R  return to the road (stuck?)     Q  abandon the run     F1  controls", {240, 210, 140, 255});
                break;
            }
            case GS_DEAD:
                drawOverlay("YOU HAVE FALLEN",
                            std::string("Slain in ") + (G.sanctuary ? "a haven" : STAGES[G.stage].name) + " with " + std::to_string(G.p.kills) + " foes vanquished.  " +
                                std::to_string(G.p.coins) + " coins banked (" + std::to_string(META.bank) + " total)",
                            "Enter  return to Hearthwick", {220, 60, 60, 255});
                break;
            case GS_WIN:
                drawOverlay("THE LICH KING IS NO MORE",
                            "The citadel crumbles. " + std::to_string(G.p.kills) + " foes vanquished, " + std::to_string(G.p.coins) + " coins banked.",
                            "Enter  return to Hearthwick", {240, 210, 110, 255});
                break;
            default: break;
            }
        }
        EndDrawing();

        if (G.state == GS_LOADING) // the loading frame is on screen; now do the slow work
        {
            if (G.loadTarget == LOAD_STAGE) startRun();
            else if (G.loadTarget == LOAD_VILLAGE) generateVillage();
            else generateSandbox();
            G.state = GS_PLAY;
        }
    }

    closeAudio();
    CloseWindow();
    return 0;
}

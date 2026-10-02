#include <raylib.h>
#include <cmath>
#include <ctime>
#include <string>
#include <vector>
#include <algorithm>
#include <cstdio>
#include "game.h"
#include "util.h"
#include <rlgl.h>

static RenderTexture2D rt{};
static Texture2D worldTex{};
static std::vector<Color> pix;

// The world renders at one texel per cell into a small render texture, then scales up crisp.
static void setupView()
{
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    int scale = std::max(2, (int)std::lround(sh / 256.0));
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
    rt = LoadRenderTexture(vw, vh);
    Image img = GenImageColor(vw, vh, BLACK);
    worldTex = LoadTextureFromImage(img);
    UnloadImage(img);
    pix.assign((size_t)vw * vh, BLACK);
}

// ---------------------------------------------------------------- lighting
// A light map at half resolution, smoothed by bilinear filtering and multiplied over the finished
// scene: moonlight falling from open sky, shadow-casting point lights (torches, lanterns, spells,
// a little around the hero) and a soft glow off fire and lava.

static const int LS = 2; // world pixels per light texel
static Texture2D lightTex{};
static int lw = 0, lh = 0, lox = 0, loy = 0, lightId = 0;
static std::vector<float> lr, lg, lb, lsky, er, eg, eb, llast;
static std::vector<int> lstamp, ltop;
static std::vector<Color> lpix;

static bool opaque(int x, int y)
{
    if (!world.in(x, y)) return true;
    CellMaterial m = world.cells[(size_t)y * world.w + x].material;
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
static void pointLight(float x, float y, float R, Color c, float I)
{
    if (x + R < lox || y + R < loy || x - R > lox + lw * LS || y - R > loy + lh * LS) return;
    lightId++;
    float cr = c.r / 255.0f * I, cg = c.g / 255.0f * I, cb = c.b / 255.0f * I;
    int rays = std::max(48, (int)(R * 3.2f)); // enough that neighbouring rays never skip a light texel
    for (int k = 0; k < rays; k++)
    {
        float a = k * 6.2832f / rays, dx = std::cos(a), dy = std::sin(a), t = 1;
        for (float d = 0; d < R; d += 1.5f)
        {
            int wx = (int)std::floor(x + dx * d), wy = (int)std::floor(y + dy * d);
            if (opaque(wx, wy) && (t *= 0.55f) < 0.04f) break;
            if (wx < lox || wy < loy) continue;
            int i = (wx - lox) / LS, j = (wy - loy) / LS;
            if (i >= lw || j >= lh) continue;
            float f = 1 - d / R, v = f * f * t;
            size_t q = (size_t)j * lw + i;
            if (lstamp[q] != lightId) { lstamp[q] = lightId; llast[q] = 0; }
            if (v <= llast[q]) continue;
            float inc = v - llast[q];
            llast[q] = v;
            lr[q] += inc * cr; lg[q] += inc * cg; lb[q] += inc * cb;
        }
    }
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
        for (auto* v : {&lr, &lg, &lb, &lsky, &er, &eg, &eb, &llast}) v->assign(n, 0);
        lstamp.assign(n, 0);
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
        for (int i = 0; i < lw; i++)
        {
            int x = lox + i * LS + LS / 2, y = 0;
            while (y < world.h && !opaque(x, y)) y++;
            ltop[i] = y;
        }
        for (int j = 0; j < lh; j++)
            for (int i = 0; i < lw; i++)
            {
                int y = loy + j * LS + LS / 2;
                lsky[(size_t)j * lw + i] = y < ltop[i] ? 1.0f : std::max(0.0f, 1 - (y - ltop[i]) / 12.0f);
            }
        boxBlur(lsky, lw, lh, 4, 2); // soft edges: light spills a little way into doorways and overhangs
    }

    // glow from burning and molten cells
    std::fill(er.begin(), er.end(), 0.0f);
    std::fill(eg.begin(), eg.end(), 0.0f);
    std::fill(eb.begin(), eb.end(), 0.0f);
    bool anyEmit = false;
    for (int j = 0; j < lh * LS; j++)
        for (int i = 0; i < lw * LS; i++)
        {
            int x = lox + i, y = loy + j;
            if (!world.in(x, y)) continue;
            const Cell& c = world.cells[(size_t)y * world.w + x];
            float r = 0, g = 0, b = 0;
            if (c.material == CellMaterial::Fire || (c.flags & CF_BURNING)) r = 1.0f, g = 0.55f, b = 0.2f;
            else if (c.material == CellMaterial::Lava) r = 1.0f, g = 0.42f, b = 0.12f;
            else if (c.material == CellMaterial::Acid) r = 0.12f, g = 0.3f, b = 0.06f;
            else continue;
            size_t q = (size_t)(j / LS) * lw + i / LS;
            er[q] += r; eg[q] += g; eb[q] += b;
            anyEmit = true;
        }
    if (anyEmit)
        for (auto* v : {&er, &eg, &eb}) { boxBlur(*v, lw, lh, 7, 7); boxBlur(*v, lw, lh, 7, 7); }

    std::fill(lr.begin(), lr.end(), 0.0f);
    std::fill(lg.begin(), lg.end(), 0.0f);
    std::fill(lb.begin(), lb.end(), 0.0f);
    const Color warm = {255, 160, 80, 255};
    for (auto& it : G.inter)
    {
        float fl = 0.88f + 0.12f * hash2((int)it.x, G.frame / 4, 9);
        if (it.type == IT_TORCH) pointLight(it.x, it.y - 15, 90, warm, fl);
        else if (it.type == IT_SHRINE && !it.used) pointLight(it.x, it.y - 34, 56, {200, 190, 255, 255}, 0.7f);
        else if (it.type == IT_STONE && !it.used) pointLight(it.x, it.y - 16, 44, G.stoneLoot[it.data].glow, 0.7f);
    }
    for (auto& l : G.lamps) pointLight(l.x, l.y, l.r, l.c, l.flame ? 0.88f + 0.12f * hash2((int)l.x, G.frame / 4, 9) : 0.85f);
    for (auto& p : G.projs)
        if (p.kind == PK_SPELL && p.spell != SP_DIG && p.spell != SP_BOMB) pointLight(p.x, p.y, 34, p.col, 0.9f);
    if (G.p.m.alive) pointLight(G.p.m.cx(), G.p.m.cy() - 4, 64, {255, 236, 210, 255}, 0.5f); // enough to see your own feet

    const float moon[3] = {0.66f, 0.72f, 0.92f};
    for (size_t q = 0; q < n; q++)
    {
        int wx = lox + (int)(q % lw) * LS, wy = loy + (int)(q / lw) * LS;
        if (world.in(wx, wy) && world.sky[(size_t)wy * world.w + wx]) { lpix[q] = WHITE; continue; } // the night sky keeps its own colours
        float e = 2.2f, s = lsky[q];
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
    renderWorld(pix.data(), cx, cy, G.vw, G.vh);
    UpdateTexture(worldTex, pix.data());
    buildLight(cx, cy);
    BeginTextureMode(rt);
    ClearBackground(BLACK);
    DrawTexture(worldTex, 0, 0, WHITE);
    drawEntities(cx, cy);
    BeginBlendMode(BLEND_MULTIPLIED);
    DrawTexturePro(lightTex, {0, 0, (float)lw, (float)lh}, {(float)(lox - cx), (float)(loy - cy), (float)lw * LS, (float)lh * LS}, {0, 0}, 0, WHITE);
    EndBlendMode();
    EndTextureMode();
    float shake = G.shake * (G.reduceShake ? 0.2f : 1.0f);
    float sx = shake > 0.5f ? frange(-shake, shake) : 0;
    float sy = shake > 0.5f ? frange(-shake, shake) : 0;
    rlDrawRenderBatchActive();
    rlDisableColorBlend(); // copy the scene as-is; its alpha channel is meaningless after in-texture blending
    DrawTexturePro(rt.texture, {0, 0, (float)G.vw, -(float)G.vh},
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

int main(int argc, char** argv)
{
    rngState() = (uint32_t)time(nullptr) * 2654435761u | 1u;
    if (argc > 1 && std::string(argv[1]) == "--selftest")
    {
        castSelfTest();
        return 0;
    }
    if (argc > 2 && std::string(argv[1]) == "--dump")
    {
        dumpStages(argv[2]);
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
            updateGame();
            if (G.state == GS_PLAY && (IsKeyPressed(KEY_TAB) || IsKeyPressed(KEY_I))) { G.state = GS_INVENTORY; G.invSel = G.p.sel; }
            else if (G.state == GS_PLAY && IsKeyPressed(KEY_ESCAPE)) G.state = GS_PAUSE;
            if (IsKeyPressed(KEY_F1)) G.showHelp = !G.showHelp;
            break;
        case GS_INVENTORY:
            if (stateAge > 3 && (IsKeyPressed(KEY_TAB) || IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_I)))
            {
                cancelInventoryDrag();
                G.state = GS_PLAY;
            }
            break;
        case GS_ANVIL:
        case GS_SHRINE:
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

        if (G.state == GS_PLAY) { if (!IsCursorHidden()) HideCursor(); }
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
            drawHUD();
            switch (G.state)
            {
            case GS_INVENTORY: updateDrawInventory(); break;
            case GS_ANVIL: updateDrawAnvil(); break;
            case GS_SHRINE: updateDrawShrine(); break;
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

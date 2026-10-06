// Dev tools (F2): god mode, fly, clear the screen, supplies, the sandbox brush in a real run, a profiler panel,
// and a map of the whole stitched world - click it to teleport, or press a number to warp to that waystone.
#include <raylib.h>
#include <cmath>
#include <string>
#include <thread>
#include <vector>
#include <algorithm>
#include "game.h"
#include "world.h"

static Texture2D mapTex{};
static float mapS = 1; // world units per map pixel
static int mapW = 0, mapH = 0, mapX = 0, mapY = 0;

// The world, one map pixel per mapS units: each pixel shows the cell at its middle (open space shows the back wall, dimmed).
static void buildMap()
{
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    mapS = std::max(world.wU() / (sw * 0.94f), world.hU() / (sh * 0.84f));
    mapW = std::max(1, (int)(world.wU() / mapS)); mapH = std::max(1, (int)(world.hU() / mapS));
    mapX = (sw - mapW) / 2; mapY = (int)(sh * 0.1f);
    Image img = GenImageColor(mapW, mapH, BLACK);
    Color* px = (Color*)img.data;
    int nt = workerCount(), sc = world.scale;
    auto band = [&](int t) {
        for (int j = mapH * t / nt; j < mapH * (t + 1) / nt; j++)
            for (int i = 0; i < mapW; i++)
            {
                int cx = (int)((i + 0.5f) * mapS) * sc, cy = (int)((j + 0.5f) * mapS) * sc;
                const Cell& c = world.get(cx, cy);
                Color col;
                if (c.material != CellMaterial::Empty) col = cellColor(c, cx, cy);
                else if (world.skyOf(cx, cy) == 1) col = {14, 18, 40, 255};
                else { Color b = world.bgOf(cx, cy); col = {(unsigned char)(b.r * 0.45f), (unsigned char)(b.g * 0.45f), (unsigned char)(b.b * 0.45f), 255}; }
                col.a = 255;
                px[(size_t)j * mapW + i] = col;
            }
    };
    parallelFor(nt, band);
    if (mapTex.id) UnloadTexture(mapTex);
    mapTex = LoadTextureFromImage(img);
    UnloadImage(img);
}

// Puts the hero at the nearest open spot to (x, y) (units, feet) and snaps the camera there.
static void teleport(float x, float y)
{
    Mob& m = G.p.m;
    for (int r = 0; r <= 240; r += 2)
        for (int k = 0; k < std::max(1, r * 2); k++)
        {
            float a = k * 6.2832f / std::max(1, r * 2), tx = x + std::cos(a) * r, ty = y + std::sin(a) * r;
            if (tx < 4 || ty < m.h + 4 || tx > world.wU() - 4 || ty > world.hU() - 4 || boxSolid(tx - m.w / 2.0f, ty - m.h, m.w, m.h)) continue;
            m.x = tx - m.w / 2.0f; m.y = ty - m.h; m.vx = m.vy = 0;
            G.camX = m.cx() - G.vw / 2.0f; G.camY = m.cy() - G.vh / 2.0f;
            syncRenderCamera();
            return;
        }
    message("No room to stand there.");
}

static void warpToWaystone(int stage)
{
    for (auto& h : G.havens)
        if (h.stage == stage) { teleport(h.x0 + 60.0f, h.floor - 1.0f); message(std::string("Warped to ") + h.name + (h.locked ? " (barred: kill the guardian, F5)" : "")); return; }
    message("That waystone isn't built yet: warp to an earlier one and walk through it.");
}

void devUpdate()
{
    if (IsKeyPressed(KEY_F2)) { G.dev = !G.dev; if (!G.dev) G.devGod = G.devFly = G.devMap = false; message(G.dev ? "Dev tools on (F2)" : "Dev tools off"); }
    if (!G.dev) return;
    if (G.devMap)
    {
        if (IsKeyPressed(KEY_M) || IsKeyPressed(KEY_ESCAPE)) { G.devMap = false; return; }
        Vector2 mp = GetMousePosition();
        bool in = mp.x >= mapX && mp.y >= mapY && mp.x < mapX + mapW && mp.y < mapY + mapH;
        if (in && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) { teleport((mp.x - mapX) * mapS, (mp.y - mapY) * mapS); G.devMap = false; }
        for (int k = 0; k < STAGE_COUNT; k++)
            if (IsKeyPressed(KEY_ONE + k)) { warpToWaystone(k); G.devMap = false; }
        return;
    }
    if (IsKeyPressed(KEY_M)) { buildMap(); G.devMap = true; }
    if (IsKeyPressed(KEY_F3)) { G.devGod = !G.devGod; message(G.devGod ? "God mode" : "God mode off"); }
    if (IsKeyPressed(KEY_F4)) { G.devFly = !G.devFly; message(G.devFly ? "Flying: WASD / Space / S, Shift for speed" : "Flying off"); }
    if (IsKeyPressed(KEY_F5)) // every foe on screen
    {
        int n = 0;
        for (auto& m : G.mobs)
            if (m.alive && m.cx() > G.camX && m.cx() < G.camX + G.vw && m.cy() > G.camY && m.cy() < G.camY + G.vh) { damageMob(m, 1e6f, EL_PHYS, 0, -1, 0); n++; }
        message("Struck down " + std::to_string(n) + " foes");
    }
    if (IsKeyPressed(KEY_F6))
    {
        for (int r = 0; r < RES_COUNT; r++) G.p.res[r] += 50;
        G.p.bombs += 5; G.p.potions += 5; G.p.coins += 100;
        message("Supplies: +50 of every ore, 5 bombs, 5 flasks, 100 coins");
    }
}

void devDraw()
{
    if (!G.dev) return;
    int sw = GetScreenWidth(), sh = GetScreenHeight(), fs = std::max(14, sh / 60);
    if (G.devMap)
    {
        DrawRectangle(0, 0, sw, sh, {0, 0, 0, 220});
        DrawTexture(mapTex, mapX, mapY, WHITE);
        auto at = [&](float ux, float uy) { return Vector2{mapX + ux / mapS, mapY + uy / mapS}; };
        for (auto& r : devPieceRects()) { Vector2 a = at(r.x, r.y); DrawRectangleLines((int)a.x, (int)a.y, (int)(r.width / mapS), (int)(r.height / mapS), {255, 255, 255, 50}); }
        for (auto& m : G.mobs)
            if (m.alive) { Vector2 p = at(m.cx(), m.cy()); DrawRectangle((int)p.x - (m.boss ? 3 : 1), (int)p.y - (m.boss ? 3 : 1), m.boss ? 7 : 2, m.boss ? 7 : 2, m.boss ? MAGENTA : Color{230, 60, 50, 255}); }
        for (auto& it : G.inter)
            if (it.type == IT_CHEST && !it.used) { Vector2 p = at(it.x, it.y); DrawRectangle((int)p.x - 1, (int)p.y - 2, 3, 3, GOLD); }
        for (auto& h : G.havens)
        {
            Vector2 a = at((float)h.x0, (float)h.top);
            Color c = h.sealed ? Color{90, 220, 110, 255} : (h.locked ? Color{230, 70, 60, 255} : Color{240, 210, 90, 255});
            DrawRectangleLines((int)a.x - 2, (int)a.y - 2, std::max(5, (int)((h.x1 - h.x0) / mapS) + 4), std::max(5, (int)((h.floor - h.top) / mapS) + 4), c);
            DrawText(TextFormat("%d", h.stage + 1), (int)a.x, (int)a.y - fs - 2, fs, c);
        }
        Vector2 p = at(G.p.m.cx(), G.p.m.cy());
        DrawCircleLines((int)p.x, (int)p.y, 6 + 2 * std::sin(GetTime() * 6), WHITE);
        DrawCircle((int)p.x, (int)p.y, 2, WHITE);
        Vector2 mp = GetMousePosition();
        if (mp.x >= mapX && mp.y >= mapY && mp.x < mapX + mapW && mp.y < mapY + mapH)
            DrawText(TextFormat("%.0f, %.0f", (mp.x - mapX) * mapS, (mp.y - mapY) * mapS), (int)mp.x + 12, (int)mp.y + 8, fs, WHITE);
        DrawText("World map  -  click to teleport   1-7 warp to that waystone (walking in seals it and builds on)   M / Esc close", mapX, mapY - fs * 2, fs, {230, 220, 200, 255});
        DrawText("red: foes   magenta: guardians   gold: chests   waystones: yellow open, red barred, green sealed", mapX, mapY + mapH + fs / 2, fs, {180, 170, 160, 255});
        return;
    }
    // the panel, bottom left
    std::vector<std::string> L;
    L.push_back(TextFormat("DEV  F2 off   F3 god %s   F4 fly %s   F5 kill on-screen   F6 supplies   M map", G.devGod ? "ON" : "off", G.devFly ? "ON" : "off"));
    L.push_back(TextFormat("Ctrl+E foe   Ctrl+C chest   Ctrl+mouse paint %s r%d  ([ ] material, - = size)", sandboxBrushName(), G.brushR));
    float up = 0, rd = 0;
    for (int s = 0; s < PF_COUNT; s++) (s >= PF_WORLD ? rd : up) += PROF[s];
    L.push_back(TextFormat("%d fps   frame %.1f ms   update %.1f   render+HUD %.1f (CPU)", GetFPS(), GetFrameTime() * 1000, up, rd));
    int alive = 0, near = 0, awake = 0, sc = world.scale;
    for (auto& m : G.mobs) { alive += m.alive; near += m.alive && std::fabs(m.cx() - G.p.m.cx()) < G.vw && std::fabs(m.cy() - G.p.m.cy()) < G.vh; }
    for (int cy = std::max(0, ((int)G.camY - 100) * sc / CS); cy <= std::min(world.ch - 1, ((int)G.camY + G.vh + 100) * sc / CS); cy++)
        for (int cx = std::max(0, ((int)G.camX - 100) * sc / CS); cx <= std::min(world.cw - 1, ((int)G.camX + G.vw + 100) * sc / CS); cx++)
        {
            auto& c = world.chunks[(size_t)cy * world.cw + cx];
            awake += c && c->awake > 0;
        }
    L.push_back(TextFormat("foes %d (%d near)   particles %zu   corpses %zu   falling slabs %d   shots %zu   awake chunks %d   stage %d   at %.0f, %.0f",
                           alive, near, G.parts.size(), G.corpses.size(), fallingBodies(), G.projs.size(), awake, G.stage + 1, G.p.m.cx(), G.p.m.cy()));
    int lineH = fs + 4, bars = PF_COUNT, h = (int)L.size() * lineH + bars * lineH + 12, y = sh - h - 10;
    int bw = (int)(sw * 0.3f) + fs * 12;
    for (auto& s : L) bw = std::max(bw, MeasureText(s.c_str(), fs) + 16);
    DrawRectangle(8, y - 6, bw, h + 6, {0, 0, 0, 170});
    for (auto& s : L) { DrawText(s.c_str(), 16, y, fs, {235, 230, 210, 255}); y += lineH; }
    for (int s = 0; s < PF_COUNT; s++) // ms per section, a full bar = the whole 16.7 ms budget
    {
        DrawText(PROF_NAMES[s], 16, y, fs, {190, 185, 170, 255});
        float w = std::min(1.0f, PROF[s] / 16.7f) * sw * 0.3f;
        Color c = PROF[s] > 4 ? Color{230, 80, 60, 255} : PROF[s] > 1.5f ? Color{230, 190, 70, 255} : Color{110, 200, 120, 255};
        DrawRectangle(16 + fs * 9, y + 2, (int)w, fs - 3, c);
        DrawText(TextFormat("%.2f", PROF[s]), 16 + fs * 9 + (int)w + 6, y, fs, {190, 185, 170, 255});
        y += lineH;
    }
}

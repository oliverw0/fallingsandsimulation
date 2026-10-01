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

static void renderScene()
{
    int cx = G.rcx, cy = G.rcy;
    renderWorld(pix.data(), cx, cy, G.vw, G.vh);
    UpdateTexture(worldTex, pix.data());
    BeginTextureMode(rt);
    ClearBackground(BLACK);
    DrawTexture(worldTex, 0, 0, WHITE);
    drawEntities(cx, cy);
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
            const char* where = G.loadTarget == LOAD_VILLAGE ? "Returning to Hearthwick..." : G.loadTarget == LOAD_SANCTUARY ? "Finding sanctuary..." :
                                G.loadTarget == LOAD_SANDBOX ? "Shaping the sandbox..." : "Descending...";
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
                            std::string("Slain in ") + (G.sanctuary ? "a sanctuary" : STAGES[G.stage].name) + " with " + std::to_string(G.p.kills) + " foes vanquished.  " +
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
            if (G.loadTarget == LOAD_STAGE) generateStage(G.stage);
            else if (G.loadTarget == LOAD_SANCTUARY) generateSanctuary();
            else if (G.loadTarget == LOAD_VILLAGE) generateVillage();
            else generateSandbox();
            G.state = GS_PLAY;
        }
    }

    closeAudio();
    CloseWindow();
    return 0;
}

// minimap.cpp: the Wayfinder's Map. A square window on the world about the player, redrawn every frame so it moves with
// them, in a thin carved-wood Norse frame. One texel is TEXEL world units; terrain is coloured from the materials' own
// colours (dimmed), water blue, fire and lava hot, the night sky dark; hostile foes show red, chests gold, waystones cyan.
#include "game.h"
#include "util.h"
#include <cmath>
#include <cstdio>
#include <vector>
#include <algorithm>

static const int MN = 112;      // texels a side
static const int TEXEL = 2;     // world units per texel: the map shows 224 x 224 units about you
static Texture2D mapTex{};
static std::vector<Color> mapPix;

static Color shade(Color c, float k) { return {(unsigned char)std::min(255.0f, c.r * k), (unsigned char)std::min(255.0f, c.g * k), (unsigned char)std::min(255.0f, c.b * k), 255}; }

static Color texelColour(int ux, int uy)
{
    if (!world.inU(ux, uy)) return {6, 5, 10, 255};
    int k = world.scale;
    const Cell& c = world.get(ux * k, uy * k);
    if (c.material == CellMaterial::Empty)
    {
        if (world.skyOf(ux * k, uy * k)) return {18, 24, 52, 255};                    // open night sky
        Color bg = world.bgOf(ux * k, uy * k);
        return {(unsigned char)(10 + bg.r * 0.28f), (unsigned char)(10 + bg.g * 0.28f), (unsigned char)(14 + bg.b * 0.32f), 255}; // back wall: a dim hollow
    }
    const MaterialProps& p = props(c.material);
    if (c.flags & CF_BURNING) return {255, 150, 50, 255};
    switch (p.kind)
    {
    case Kind::Fire: return {255, 170, 60, 255};
    case Kind::Liquid:
        if (c.material == CellMaterial::Water) return {52, 96, 190, 255};
        if (c.material == CellMaterial::Lava) return {236, 90, 24, 255};
        return shade(p.a, 0.9f);
    case Kind::Gas: return {30, 32, 44, 255};
    case Kind::Powder: return shade(p.a, 0.85f);
    default: break;
    }
    return shade(p.a, 0.78f);                                                          // solid ground and rock, a touch darker than life
}

// A thin carved-wood border: dark edge, a plank band with grain and cut runes, a gold line, brass studs at the corners.
static void frame(Rectangle r, float u)
{
    float t = std::max(4.0f, 6.0f * u);
    Rectangle o = {r.x - t, r.y - t, r.width + 2 * t, r.height + 2 * t};
    DrawRectangleLinesEx({o.x - 1.5f * u, o.y - 1.5f * u, o.width + 3 * u, o.height + 3 * u}, std::max(1.0f, 1.5f * u), {14, 9, 6, 255});
    Color wood = {88, 60, 36, 255};
    DrawRectangleRec({o.x, o.y, o.width, t}, wood); DrawRectangleRec({o.x, o.y + o.height - t, o.width, t}, wood);                     // the four bands of the frame
    DrawRectangleRec({o.x, o.y + t, t, o.height - 2 * t}, wood); DrawRectangleRec({o.x + o.width - t, o.y + t, t, o.height - 2 * t}, wood);   // (the picture shows through the middle)
    for (int i = 0; i < 4; i++) // grain along each band
    {
        bool horiz = i < 2;
        Rectangle b = horiz ? Rectangle{o.x, i == 0 ? o.y : o.y + o.height - t, o.width, t} : Rectangle{i == 2 ? o.x : o.x + o.width - t, o.y + t, t, o.height - 2 * t};
        float L = horiz ? b.width : b.height;
        for (int k = 0; k < 4; k++)
        {
            float off = (k + 0.5f) / 4.0f * t;
            Color c = k % 2 ? Color{64, 42, 26, 255} : Color{116, 82, 50, 255};
            if (horiz) DrawRectangleRec({b.x, b.y + off - 0.4f * u, L, std::max(1.0f, 0.8f * u)}, c);
            else DrawRectangleRec({b.x + off - 0.4f * u, b.y, std::max(1.0f, 0.8f * u), L}, c);
        }
        // runes cut into the band: short strokes, a diamond between them
        int n = (int)(L / (13 * u));
        for (int k = 0; k < n; k++)
        {
            float p = (k + 0.5f) / n * L, h = hash2(k, i, 77);
            Color cut = {34, 22, 14, 255};
            float cx = horiz ? b.x + p : b.x + t / 2, cy = horiz ? b.y + t / 2 : b.y + p;
            float s = t * 0.28f;
            if (h < 0.34f) { DrawLineEx({cx - s, cy + s}, {cx, cy - s}, std::max(1.0f, 0.9f * u), cut); DrawLineEx({cx, cy - s}, {cx + s, cy + s}, std::max(1.0f, 0.9f * u), cut); }
            else if (h < 0.67f) { DrawLineEx({cx - s, cy - s}, {cx + s, cy + s}, std::max(1.0f, 0.9f * u), cut); DrawLineEx({cx - s, cy + s}, {cx + s, cy - s}, std::max(1.0f, 0.9f * u), cut); }
            else { DrawLineEx({cx, cy - s}, {cx, cy + s}, std::max(1.0f, 0.9f * u), cut); DrawLineEx({cx - s * 0.8f, cy - s * 0.2f}, {cx, cy - s}, std::max(1.0f, 0.9f * u), cut); }
        }
    }
    DrawRectangleLinesEx(o, std::max(1.0f, 1.2f * u), {18, 12, 8, 255});
    DrawRectangleLinesEx({r.x - 1, r.y - 1, r.width + 2, r.height + 2}, std::max(1.0f, 1.2f * u), {176, 134, 76, 255}); // the gold inner line
    for (int i = 0; i < 4; i++) // brass studs
    {
        Vector2 c = {i % 2 ? o.x + o.width - t * 0.5f : o.x + t * 0.5f, i / 2 ? o.y + o.height - t * 0.5f : o.y + t * 0.5f};
        DrawCircleV(c, t * 0.52f, {20, 14, 9, 255});
        DrawCircleV(c, t * 0.4f, {196, 156, 86, 255});
        DrawCircleV({c.x - t * 0.1f, c.y - t * 0.1f}, t * 0.14f, {250, 226, 150, 255});
    }
}

// x, y: the top-left of the map's picture (screen pixels); size: its side; u: the UI scale
void drawMinimap(float x, float y, float size, float u)
{
    Player& P = G.p;
    Mob& m = P.m;
    if (!mapTex.id)
    {
        Image img = GenImageColor(MN, MN, BLACK);
        mapTex = LoadTextureFromImage(img);
        UnloadImage(img);
        SetTextureFilter(mapTex, TEXTURE_FILTER_POINT);
        mapPix.assign((size_t)MN * MN, BLACK);
    }
    int cx = (int)std::floor(m.cx()), cy = (int)std::floor(m.cy());
    int ox = cx - (MN / 2) * TEXEL, oy = cy - (MN / 2) * TEXEL;
    for (int j = 0; j < MN; j++)
        for (int i = 0; i < MN; i++) mapPix[(size_t)j * MN + i] = texelColour(ox + i * TEXEL, oy + j * TEXEL);
    auto mark = [&](float wx, float wy, Color c, int r) { // a marker, 1 + r texels wide
        int i = (int)std::floor((wx - ox) / TEXEL), j = (int)std::floor((wy - oy) / TEXEL);
        for (int dy = -r; dy <= r; dy++)
            for (int dx = -r; dx <= r; dx++)
                if (i + dx >= 0 && j + dy >= 0 && i + dx < MN && j + dy < MN) mapPix[(size_t)(j + dy) * MN + i + dx] = c;
    };
    for (auto& it : G.inter)
    {
        if (it.type == IT_CHEST && !it.used) mark(it.x, it.y - 4, {240, 200, 70, 255}, 1);
        else if (it.type == IT_STONE && !it.used) mark(it.x, it.y - 6, {200, 120, 240, 255}, 1);
        else if (it.type == IT_ANVIL || it.type == IT_SHRINE) mark(it.x, it.y - 6, {120, 230, 250, 255}, 1);
    }
    for (auto& h : G.havens) mark(h.x0 + 40, h.floor - 10, {120, 230, 250, 255}, 2);
    for (auto& e : G.mobs)
        if (e.alive) mark(e.cx(), e.cy(), e.boss ? Color{230, 80, 255, 255} : Color{230, 60, 56, 255}, e.boss ? 2 : 1);
    mark(m.cx(), m.cy(), {255, 255, 255, 255}, 1);   // you
    mark(m.cx(), m.cy(), {255, 220, 120, 255}, 0);
    UpdateTexture(mapTex, mapPix.data());
    DrawTexturePro(mapTex, {0, 0, (float)MN, (float)MN}, {x, y, size, size}, {0, 0}, 0, WHITE);
    // a faint vignette so the edge dissolves into the frame
    DrawRectangleGradientV((int)x, (int)y, (int)size, (int)(size * 0.12f), {0, 0, 0, 90}, {0, 0, 0, 0});
    DrawRectangleGradientV((int)x, (int)(y + size * 0.88f), (int)size, (int)(size * 0.12f), {0, 0, 0, 0}, {0, 0, 0, 90});
    frame({x, y, size, size}, u);
}

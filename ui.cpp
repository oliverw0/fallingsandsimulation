// HUD, inventory (drag and drop), anvil and shrine screens, icons and text helpers.
#include "game.h"
#include "util.h"
#include <cmath>
#include <cctype>
#include <cstring>
#include <cstdio>
#include <algorithm>
#include <string>
#include <vector>

// ---------------------------------------------------------------- fonts & text
// One pixel font, drawn from stroke lists (no font files): a runic-cut Latin alphabet - angular, thin, carved.
// Each glyph is polylines on a grid, x 0-4, y 0-8 (capitals), x-height from y 3, descenders to y 10 ("a" = 10).
// They're rasterised once into an atlas and drawn at whole-pixel scale, so every size stays crisp.

struct PixGlyph { int cp; const char* s; };
static const PixGlyph PIXGLYPHS[] = {
    {'A', "08-20-48 15-35"}, {'B', "00-08 00-31-33-04-35-37-08"}, {'C', "40-10-01-07-18-48"}, {'D', "00-08 00-41-47-08"},
    {'E', "00-08 00-40 04-34 08-48"}, {'F', "00-08 00-40 04-34"}, {'G', "40-10-01-07-18-48-45-25"}, {'H', "00-08 40-48 04-44"},
    {'I', "10-30 20-28 18-38"}, {'J', "40-47-38-18-07"}, {'K', "00-08 40-04-48"}, {'L', "00-08-48"},
    {'M', "08-00-24-40-48"}, {'N', "08-00-48-40"}, {'O', "10-01-07-18-38-47-41-30-10"}, {'P', "00-08 00-41-43-04"},
    {'Q', "10-01-07-18-38-47-41-30-10 25-49"}, {'R', "00-08 00-41-43-04-48"}, {'S', "40-10-01-13-35-47-38-08"}, {'T', "00-40 20-28"},
    {'U', "00-07-18-38-47-40"}, {'V', "00-28-40"}, {'W', "00-18-23-38-40"}, {'X', "00-48 40-08"},
    {'Y', "00-24-40 24-28"}, {'Z', "00-40-08-48"},
    {'a', "13-33-38 35-15-06-07-18-38"}, {'b', "00-08 03-23-34-37-28-08"}, {'c', "33-13-04-07-18-38"}, {'d', "30-38 33-13-04-07-18-38"},
    {'e', "05-35-34-23-13-04-07-18-38"}, {'f', "31-20-11-18 03-33"}, {'g', "33-39-2a-0a 33-13-04-06-17-37"}, {'h', "00-08 03-23-34-38"},
    {'i', "23-28 20-21 18-38 13-23"}, {'j', "23-29-1a-0a 20-21"}, {'k', "00-08 33-06-38"}, {'l', "10-20-28 18-38"},
    {'m', "03-08 04-13-24-28 24-33-44-48"}, {'n', "03-08 03-23-34-38"}, {'o', "13-23-34-37-28-18-07-04-13"}, {'p', "03-0a 03-23-34-37-28-08"},
    {'q', "33-3a 33-13-04-07-18-38"}, {'r', "03-08 04-13-23"}, {'s', "33-13-04-15-25-36-37-28-08"}, {'t', "11-17-28-38 03-33"},
    {'u', "03-07-18-38 33-38"}, {'v', "03-28-43"}, {'w', "03-18-25-38-43"}, {'x', "03-38 33-08"},
    {'y', "03-28 43-1a"}, {'z', "03-33-08-38"},
    {'0', "10-01-07-18-38-47-41-30-10"}, {'1', "10-20-28 18-38"}, {'2', "01-10-30-41-43-08-48"}, {'3', "01-10-30-41-43-24 24-45-47-38-18-07"},
    {'4', "30-05-45 30-38"}, {'5', "40-00-03-33-44-47-38-18-07"}, {'6', "30-10-01-07-18-38-47-45-34-04"}, {'7', "00-40-18"},
    {'8', "10-01-03-14-34-43-41-30-10 14-05-07-18-38-47-45-34"}, {'9', "10-01-03-14-34-43-41-30-10 43-47-38-18"},
    {'.', "27-28 37-38"}, {',', "27-28-19"}, {':', "23-24 27-28"}, {';', "23-24 27-28-19"},
    {'!', "20-26 27-28"}, {'?', "01-10-30-41-43-24-26 27-28"}, {'\'', "20-22"}, {'"', "10-12 30-32"},
    {'-', "15-35"}, {'+', "15-35 23-27"}, {'/', "07-41"}, {'(', "30-21-27-38"}, {')', "10-21-27-18"},
    {'%', "00-10-11-01-00 38-48-47-37-38 40-08"}, {'&', "20-11-13-48 20-31-33-08-18-37"}, {'*', "22-26 04-40 00-44"},
    {'=', "04-44 06-46"}, {'<', "30-04-38"}, {'>', "10-34-18"}, {'_', "09-49"}, {'[', "30-10-18-38"}, {']', "10-30-38-18"},
    {'#', "12-18 32-38 04-44 06-46"}, {'~', "04-13-24-33"}, {'@', "43-23-24-44-46-48-08-01-10-40"}, {0xB7, "24-24"},
    {0xD6, "12-03-07-18-38-47-43-32-12 10-10 30-30"}, {'|', "20-29"}, {'$', "40-10-01-13-35-47-38-08 20-29"}, {'^', "02-20-42"}, {'\\', "01-47"},
    {'{', "30-21-23-14-25-27-38"}, {'}', "10-21-23-34-25-27-18"}, {'`', "10-21"},
};
static const int GW = 8, GH = 12, GCOLS = 16, NPIX = (int)(sizeof(PIXGLYPHS) / sizeof(PIXGLYPHS[0]));
static Texture2D pixTex{};
static int pixIdx[256], pixAdv[256];
static bool haveFonts = false;

static void rasterLine(Image& img, int ox, int oy, int x0, int y0, int x1, int y1)
{
    int dx = std::abs(x1 - x0), dy = -std::abs(y1 - y0), sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, err = dx + dy;
    for (;;)
    {
        ImageDrawPixel(&img, ox + x0, oy + y0, WHITE);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void initUI()
{
    int rows = (NPIX + GCOLS - 1) / GCOLS;
    Image img = GenImageColor(GCOLS * GW, rows * GH, BLANK);
    for (int i = 0; i < 256; i++) pixIdx[i] = -1, pixAdv[i] = 3;
    for (int i = 0; i < NPIX; i++)
    {
        int ox = (i % GCOLS) * GW, oy = (i / GCOLS) * GH, maxx = 0, px = -1, py = -1;
        auto num = [](char c) { return c >= 'a' ? c - 'a' + 10 : c - '0'; };
        for (const char* p = PIXGLYPHS[i].s;; p++)
        {
            if (*p == 0 || *p == ' ' || *p == '-')
            {
                if (*p != '-') px = py = -1; // a new polyline
                if (!*p) break;
                continue;
            }
            int x = num(p[0]), y = num(p[1]);
            p++;
            maxx = std::max(maxx, x);
            if (px >= 0) rasterLine(img, ox, oy, px, py, x, y);
            else rasterLine(img, ox, oy, x, y, x, y);
            px = x; py = y;
        }
        pixIdx[PIXGLYPHS[i].cp & 255] = i;
        pixAdv[PIXGLYPHS[i].cp & 255] = maxx + 2;
    }
    pixTex = LoadTextureFromImage(img);
    SetTextureFilter(pixTex, TEXTURE_FILTER_POINT);
    UnloadImage(img);
    haveFonts = true;
}

static const char* havenName()
{
    for (auto& h : G.havens)
        if (G.p.m.cx() > h.x0 && G.p.m.cx() < h.x1) return h.name;
    return "Haven";
}

static int pixScale(float size) { return std::max(1, (int)std::lround(size / 13.0f)); }

// UTF-8 decoded to the Latin-1 range we have glyphs for (anything else is a question mark)
static int nextCp(const char*& p)
{
    unsigned char c = (unsigned char)*p++;
    if (c < 0x80) return c;
    if ((c & 0xE0) == 0xC0 && *p) { int cp = ((c & 0x1F) << 6) | (*p++ & 0x3F); return cp < 256 ? cp : '?'; }
    while (*p && ((unsigned char)*p & 0xC0) == 0x80) p++;
    return '?';
}

float uiTextWidth(const std::string& s, float size, int style)
{
    (void)style;
    int ps = pixScale(size);
    float w = 0;
    for (const char* p = s.c_str(); *p;)
    {
        int cp = nextCp(p);
        w += (cp == ' ' ? 3 : pixAdv[cp & 255]) * ps;
    }
    return w;
}

void uiText(const std::string& s, float x, float y, float size, Color c, int style)
{
    int ps = pixScale(size);
    float top = std::floor(y + (size - 9 * ps) * 0.4f);
    Color sh = {0, 0, 0, (unsigned char)(c.a * 0.75f)};
    for (int pass = 0; pass < 2; pass++)
    {
        float cx = std::floor(x) + (pass ? 0 : ps), cy = top + (pass ? 0 : ps);
        for (const char* p = s.c_str(); *p;)
        {
            int cp = nextCp(p) & 255, g = pixIdx[cp];
            if (cp == ' ') { cx += 3 * ps; continue; }
            if (g < 0) { g = pixIdx['?']; cp = '?'; }
            Rectangle src = {(float)(g % GCOLS * GW), (float)(g / GCOLS * GH), (float)GW, (float)GH};
            for (int b = 0; b < (style ? 2 : 1); b++) // bold: struck twice, a pixel apart
                DrawTexturePro(pixTex, src, {cx + b * ps, cy, (float)(GW * ps), (float)(GH * ps)}, {0, 0}, 0, pass ? c : sh);
            cx += pixAdv[cp] * ps;
        }
    }
}

void uiTextCentered(const std::string& s, float cx, float y, float size, Color c, int style)
{
    uiText(s, cx - uiTextWidth(s, size, style) / 2, y, size, c, style);
}

static float U() { return GetScreenHeight() / 768.0f * G.uiScale; }
static void text(const std::string& s, float x, float y, float size, Color c, int style = 0) { uiText(s, x, y, size, c, style); }
static void textC(const std::string& s, float cx, float y, float size, Color c, int style = 0) { uiTextCentered(s, cx, y, size, c, style); }
static bool hovered(Rectangle r) { return CheckCollisionPointRec(GetMousePosition(), r); }
static bool clicked(Rectangle r) { return hovered(r) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT); }
static bool rclicked(Rectangle r) { return hovered(r) && IsMouseButtonPressed(MOUSE_BUTTON_RIGHT); }

static const Color C_GOLD = {240, 205, 130, 255};
static const Color INK = {236, 230, 220, 255};
static const Color DIM = {150, 144, 160, 255};

// ---------------------------------------------------------------- runes: the Elder Futhark as strokes on a 4x8 grid
// Drawn, not loaded (no font or image files): glowing blue channels, headings with their words spelled in runes.

struct RSeg { int x0, y0, x1, y1; };
static std::vector<RSeg> RUNES[24];
static const char* RUNE_SRC[24] = {
    "1,0,1,8 1,3,4,0 1,5,4,2",                 // 0 Fehu
    "1,0,1,8 1,0,3,3 3,3,3,8",                 // 1 Uruz
    "1,0,1,8 1,2,3,4 3,4,1,6",                 // 2 Thurisaz
    "1,0,1,8 1,1,3,3 1,4,3,6",                 // 3 Ansuz
    "1,0,1,8 1,0,3,2 3,2,1,4 1,4,3,8",         // 4 Raidho
    "3,1,1,4 1,4,3,7",                         // 5 Kenaz
    "0,1,4,7 0,7,4,1",                         // 6 Gebo
    "1,0,1,8 1,0,3,2 3,2,1,4",                 // 7 Wunjo
    "1,0,1,8 3,0,3,8 1,3,3,5",                 // 8 Hagalaz
    "2,0,2,8 0,3,4,5",                         // 9 Naudiz
    "2,0,2,8",                                 // 10 Isa
    "0,1,2,3 2,3,0,5 2,3,4,5 4,5,2,7",         // 11 Jera
    "2,0,2,8 2,0,0,2 2,8,4,6",                 // 12 Eihwaz
    "1,0,1,8 1,0,3,2 3,2,3,4 3,4,1,6",         // 13 Perthro
    "2,0,2,8 2,4,0,1 2,4,4,1",                 // 14 Algiz
    "3,0,1,3 1,3,3,5 3,5,1,8",                 // 15 Sowilo
    "2,0,2,8 2,0,0,3 2,0,4,3",                 // 16 Tiwaz
    "1,0,1,8 1,0,3,2 3,2,1,4 1,4,3,6 3,6,1,8", // 17 Berkano
    "0,0,0,8 4,0,4,8 0,0,2,3 4,0,2,3",         // 18 Ehwaz
    "0,0,0,8 4,0,4,8 0,1,4,5 4,1,0,5",         // 19 Mannaz
    "1,0,1,8 1,1,3,3",                         // 20 Laguz
    "2,0,0,4 0,4,2,8 2,8,4,4 4,4,2,0",         // 21 Ingwaz
    "0,0,0,8 4,0,4,8 0,0,4,8 4,0,0,8",         // 22 Dagaz
    "2,0,0,3 0,3,2,6 2,0,4,3 4,3,2,6 2,6,0,8 2,6,4,8", // 23 Othala
};
static const int RUNE_OF[26] = {3, 17, 5, 22, 18, 0, 6, 8, 10, 11, 5, 20, 19, 9, 23, 13, 5, 4, 15, 16, 1, 7, 7, 14, 12, 14}; // a-z
enum { RN_FEHU = 0, RN_URUZ = 1, RN_KENAZ = 5, RN_ALGIZ = 14 };

static void drawRune(int r, float x, float y, float h, float th, Color c)
{
    if (RUNES[0].empty())
        for (int i = 0; i < 24; i++)
        {
            const char* p = RUNE_SRC[i];
            int a, b, cc, d, n;
            while (sscanf(p, "%d,%d,%d,%d%n", &a, &b, &cc, &d, &n) == 4) { RUNES[i].push_back({a, b, cc, d}); p += n; }
        }
    float s = h / 8;
    for (auto& g : RUNES[r % 24]) DrawLineEx({x + g.x0 * s, y + g.y0 * s}, {x + g.x1 * s, y + g.y1 * s}, th, c);
}
// a rune with a halo, for use inside BLEND_ADDITIVE
static void glowRune(int r, float x, float y, float h, Color c, float a)
{
    drawRune(r, x, y, h, std::max(2.0f, h * 0.34f), {c.r, c.g, c.b, (unsigned char)(a * 46)});
    drawRune(r, x, y, h, std::max(1.0f, h * 0.13f), {c.r, c.g, c.b, (unsigned char)(a * 235)});
}
static float runeAdvance(float h) { return h * 0.78f; }
static float runeLineWidth(const std::string& s, float h)
{
    float w = 0;
    for (char ch : s) w += ch == ' ' ? h * 0.45f : runeAdvance(h);
    return w;
}
static void glowRuneLine(const std::string& s, float x, float y, float h, Color c, float a)
{
    for (char ch : s)
    {
        if (ch == ' ') { x += h * 0.45f; continue; }
        int l = std::tolower((unsigned char)ch) - 'a';
        if (l >= 0 && l < 26) glowRune(RUNE_OF[l], x, y, h, c, a);
        x += runeAdvance(h);
    }
}

// A heading: the words in carved capitals, spelled again in glowing runes beneath.
static void heading(const std::string& s, float x, float y, float size, Color c, bool centre = false)
{
    std::string up = s;
    for (auto& ch : up) ch = (char)std::toupper((unsigned char)ch);
    float w = uiTextWidth(up, size, 2), rh = size * 0.42f;
    if (centre) x -= w / 2;
    uiText(up, x, y, size, c, 2);
    BeginBlendMode(BLEND_ADDITIVE);
    glowRuneLine(s, x + (w - runeLineWidth(s, rh)) / 2, y + size * 1.08f, rh, {96, 176, 255, 255}, 0.85f);
    EndBlendMode();
}

// Planks behind a carved border: dark channels along the edges (all four if `sides`) with runes glowing in them.
// The inventory's tabletop: scarred old planks with knots and nails, cup rings, wine and soot stains, knife scratches and
// runes cut into the wood. Painted once into a small image and scaled up with hard edges, like the rest of the pixel art.
static Texture2D& boardTexture()
{
    static Texture2D tex{};
    if (tex.id) return tex;
    const int W = 320, H = 192, PH = 24;
    std::vector<Color> px((size_t)W * H);
    auto at = [&](int x, int y) -> Color& { static Color dummy; return x >= 0 && y >= 0 && x < W && y < H ? px[(size_t)y * W + x] : dummy; };
    auto mul = [&](int x, int y, float r, float g, float b) { if (x < 0 || y < 0 || x >= W || y >= H) return; Color& c = px[(size_t)y * W + x]; c.r = (unsigned char)std::min(255.0f, c.r * r); c.g = (unsigned char)std::min(255.0f, c.g * g); c.b = (unsigned char)std::min(255.0f, c.b * b); };
    auto blend = [&](int x, int y, Color c, float a) { if (x < 0 || y < 0 || x >= W || y >= H) return; Color& d = px[(size_t)y * W + x]; d.r = (unsigned char)(d.r + (c.r - d.r) * a); d.g = (unsigned char)(d.g + (c.g - d.g) * a); d.b = (unsigned char)(d.b + (c.b - d.b) * a); };
    for (int y = 0; y < H; y++) // planks laid lengthways, each its own tone, with long grain
    {
        int p = y / PH, ry = y % PH;
        float tone = 0.8f + 0.36f * hash2(p, 3, 201), warm = hash2(p, 5, 202);
        for (int x = 0; x < W; x++)
        {
            float g = vnoise(x * 0.035f + p * 17.0f, y * 0.55f, 211) * 0.6f + vnoise(x * 0.13f + p * 5.0f, y * 1.6f, 212) * 0.3f + vnoise(x * 0.5f, y * 0.4f, 213) * 0.1f;
            float k = tone * (0.72f + 0.5f * g);
            if (ry == 0) k *= 0.42f;          // the seam between planks
            else if (ry == 1) k *= 1.18f;     // light catching its edge
            else if (ry == PH - 1) k *= 0.8f;
            at(x, y) = {(unsigned char)std::min(255.0f, 66 * k * (0.96f + 0.1f * warm)), (unsigned char)std::min(255.0f, 46 * k), (unsigned char)std::min(255.0f, 30 * k * (1.04f - 0.12f * warm)), 255};
        }
        int jx = (int)(hash2(p, 7, 203) * (W - 60)) + 30; // a butt joint, with a nail either side
        for (int yy = p * PH; yy < p * PH + PH; yy++) { mul(jx, yy, 0.45f, 0.45f, 0.45f); mul(jx + 1, yy, 1.12f, 1.12f, 1.12f); }
        for (int s : {-6, 7})
            for (int j : {4, PH - 5}) { at(jx + s, p * PH + j) = {44, 40, 42, 255}; at(jx + s + 1, p * PH + j) = {120, 114, 112, 255}; }
    }
    for (int n = 0; n < 7; n++) // knots
    {
        float kx = 20 + hash2(n, 1, 221) * (W - 40), ky = (int)(hash2(n, 2, 222) * 8) * PH + 6 + hash2(n, 3, 223) * (PH - 12), r = 3 + hash2(n, 4, 224) * 3;
        for (int y = (int)ky - 8; y <= (int)ky + 8; y++)
            for (int x = (int)kx - 14; x <= (int)kx + 14; x++)
            {
                float d = std::hypot((x - kx) / 1.9f, (float)(y - ky));
                if (d < r) mul(x, y, 0.5f + 0.3f * std::sin(d * 2.6f), 0.5f + 0.3f * std::sin(d * 2.6f), 0.5f + 0.25f * std::sin(d * 2.6f));
                else if (d < r + 2.5f) mul(x, y, 0.78f, 0.76f, 0.74f);
            }
    }
    for (int n = 0; n < 5; n++) // cup rings: a drinking horn, set down wet, over and over
    {
        float cx = hash2(n, 8, 231) * W, cy = hash2(n, 9, 232) * H, R = 8 + hash2(n, 10, 233) * 4, gap = hash2(n, 11, 234) * 6.28f;
        for (float a = 0; a < 6.28f; a += 0.02f)
        {
            if (std::fabs(std::fmod(a - gap + 12.56f, 6.28f)) < 0.7f) continue;
            for (float t = 0; t < 1.3f; t += 0.65f) blend((int)(cx + std::cos(a) * (R + t)), (int)(cy + std::sin(a) * (R + t) * 0.8f), {22, 12, 8, 255}, 0.34f);
        }
    }
    for (int n = 0; n < 3; n++) // spilt wine and soot
    {
        float cx = hash2(n, 12, 241) * W, cy = hash2(n, 13, 242) * H, R = 7 + hash2(n, 14, 243) * 9;
        bool wine = n != 2;
        for (int y = (int)(cy - R - 3); y <= (int)(cy + R + 3); y++)
            for (int x = (int)(cx - R * 1.5f - 3); x <= (int)(cx + R * 1.5f + 3); x++)
            {
                float d = std::hypot((x - cx) / 1.5f, (float)(y - cy)), edge = R * (0.75f + 0.5f * vnoise(x * 0.2f, y * 0.2f, 244));
                if (d > edge) continue;
                float a = (1 - d / edge) * 0.5f + (d > edge * 0.85f ? 0.18f : 0);
                blend(x, y, wine ? Color{70, 14, 20, 255} : Color{14, 10, 8, 255}, std::min(0.62f, a));
            }
    }
    auto line = [&](float x0, float y0, float x1, float y1, Color c, float a) {
        int n = (int)(std::max(std::fabs(x1 - x0), std::fabs(y1 - y0))) + 1;
        for (int i = 0; i <= n; i++) blend((int)std::lround(x0 + (x1 - x0) * i / n), (int)std::lround(y0 + (y1 - y0) * i / n), c, a);
    };
    for (int n = 0; n < 70; n++) // knife scratches: pale slivers where the blade skipped
    {
        float x0 = hash2(n, 15, 251) * W, y0 = hash2(n, 16, 252) * H, ang = (hash2(n, 17, 253) - 0.5f) * 1.6f + (n % 3 == 0 ? 1.57f : 0), len = 4 + hash2(n, 18, 254) * 22;
        line(x0, y0, x0 + std::cos(ang) * len, y0 + std::sin(ang) * len, {176, 138, 92, 255}, 0.35f);
    }
    // carvings: a dark cut with a pale lip below it, as a knife leaves in the grain
    auto carve = [&](float x0, float y0, float x1, float y1) { line(x0, y0 + 1, x1, y1 + 1, {150, 112, 72, 255}, 0.55f); line(x0, y0, x1, y1, {18, 10, 7, 255}, 0.92f); };
    static const char* RUNES[] = { // each stroke x0 y0 x1 y1 on a 4 x 8 cell, ';' between strokes
        "0 0 0 8;0 2 3 0;0 4 3 2", "0 8 0 0;0 0 3 2;3 2 3 8", "0 0 0 8;0 2 3 4;3 4 0 6", "0 0 0 8;0 1 3 3;0 4 3 2", "0 0 0 8;0 0 3 2;3 2 0 4;0 4 3 8",
        "3 0 0 4;0 4 3 8", "0 0 3 8;3 0 0 8", "0 0 0 8;0 0 3 2;3 2 0 4", "0 0 0 8;3 0 3 8;0 3 3 5", "0 4 3 4;1 0 1 8;2 0 2 8", "1 0 1 8", "0 0 0 4;0 4 3 8;3 0 3 4",
        "0 0 3 3;3 3 0 5;0 5 3 8", "0 3 3 0;3 0 3 8;0 3 3 8", "0 0 0 8;0 0 3 3;3 3 0 5;0 5 3 8;3 3 3 8", "0 0 0 8;3 0 3 8;0 0 3 4;3 4 0 8", "0 0 0 8;0 0 3 2;3 2 3 8", "0 8 0 0;0 0 3 3",
        "1 0 3 4;3 4 1 8;1 8 0 4;0 4 1 0", "0 0 3 8;3 0 0 8;0 0 0 8;3 0 3 8"};
    auto rune = [&](int k, float x, float y, float s, float ca, float sa) {
        std::string spec = RUNES[k % 20];
        size_t pos = 0;
        while (pos < spec.size())
        {
            size_t e = spec.find(';', pos);
            float v[4];
            if (sscanf(spec.substr(pos, e == std::string::npos ? std::string::npos : e - pos).c_str(), "%f %f %f %f", &v[0], &v[1], &v[2], &v[3]) == 4)
            {
                auto tf = [&](float a, float b, float& ox, float& oy) { a = (a - 1.5f) * s; b = (b - 4) * s; ox = x + a * ca - b * sa; oy = y + a * sa + b * ca; };
                float ax, ay, bx, by;
                tf(v[0], v[1], ax, ay); tf(v[2], v[3], bx, by);
                carve(ax, ay, bx, by);
            }
            if (e == std::string::npos) break;
            pos = e + 1;
        }
    };
    auto inscription = [&](float x, float y, int n, float s, float ang, int seed) { // a line of runes, a little uneven, as if cut in an idle hour
        float ca = std::cos(ang), sa = std::sin(ang);
        for (int i = 0; i < n; i++)
        {
            float off = i * 6.0f * s * 0.7f, wob = (hash2(i, seed, 261) - 0.5f) * 1.6f;
            if (i % 5 == 4 && n > 6) continue; // a word break
            rune((int)(hash2(i, seed, 262) * 20), x + off * ca - wob * sa, y + off * sa + wob * ca, s * 0.78f, ca, sa);
        }
    };
    inscription(70, 172, 14, 1.0f, -0.02f, 1);   // along the foot of the board
    inscription(190, 10, 10, 0.9f, 0.015f, 2);   // up by the title
    inscription(14, 128, 6, 0.85f, -1.2f, 3);    // and one running down the left margin
    inscription(300, 112, 7, 0.9f, 1.45f, 4);
    inscription(12, 150, 7, 0.9f, -0.04f, 5);
    {   // a longship scratched in, sail and oars
        float sx = 252, sy = 160;
        for (int i = 0; i < 26; i++) { float t = i / 25.0f; carve(sx + t * 26, sy + std::sin(t * 3.14f) * 3, sx + (t + 0.04f) * 26, sy + std::sin((t + 0.04f) * 3.14f) * 3); }
        carve(sx, sy, sx - 3, sy - 6); carve(sx + 26, sy, sx + 29, sy - 6); carve(sx + 13, sy + 2, sx + 13, sy - 16);
        carve(sx + 13, sy - 16, sx + 22, sy - 6); carve(sx + 22, sy - 6, sx + 13, sy - 4); carve(sx + 13, sy - 14, sx + 5, sy - 6); carve(sx + 5, sy - 6, sx + 13, sy - 4);
        for (int i = 0; i < 5; i++) carve(sx + 5 + i * 4, sy + 3, sx + 3 + i * 4, sy + 8);
    }
    {   // a valknut: three knotted triangles
        float cx = 40, cy = 40;
        for (int t = 0; t < 3; t++)
        {
            float ox = (t - 1) * 8.0f, oy = t == 1 ? -4.0f : 0.0f;
            for (int i = 0; i < 3; i++)
            {
                float a = -1.57f + i * 2.094f, b = -1.57f + (i + 1) * 2.094f;
                carve(cx + ox + std::cos(a) * 8, cy + oy + std::sin(a) * 8, cx + ox + std::cos(b) * 8, cy + oy + std::sin(b) * 8);
            }
        }
    }
    {   // Mjolnir, a wheel of the sun, tallies of days
        float hx = 292, hy = 30;
        carve(hx - 7, hy - 6, hx + 7, hy - 6); carve(hx - 7, hy + 2, hx + 7, hy + 2); carve(hx - 7, hy - 6, hx - 7, hy + 2); carve(hx + 7, hy - 6, hx + 7, hy + 2);
        carve(hx, hy + 2, hx, hy + 20); carve(hx - 2, hy + 20, hx + 2, hy + 20); carve(hx - 2, hy + 14, hx + 2, hy + 14);
        float wx = 150, wy = 184;
        for (float a = 0; a < 6.28f; a += 0.3f) carve(wx + std::cos(a) * 5, wy + std::sin(a) * 5, wx + std::cos(a + 0.3f) * 5, wy + std::sin(a + 0.3f) * 5);
        carve(wx - 5, wy, wx + 5, wy); carve(wx, wy - 5, wx, wy + 5);
        for (int g = 0; g < 4; g++) { float tx = 22 + g * 9, ty = 100; for (int i = 0; i < 4; i++) carve(tx + i * 1.6f, ty, tx + i * 1.6f, ty + 8); if (g < 3) carve(tx - 1, ty + 6, tx + 6, ty + 1); }
    }
    for (int y = 0; y < H; y++) // the edges fall into shadow, as a table does away from the lamp
        for (int x = 0; x < W; x++)
        {
            float dx = (x - W / 2.0f) / (W / 2.0f), dy = (y - H / 2.0f) / (H / 2.0f), v = 1 - 0.28f * (dx * dx * 0.8f + dy * dy * 0.8f);
            mul(x, y, v, v, v);
        }
    Image img = GenImageColor(W, H, BLACK);
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) ImageDrawPixel(&img, x, y, px[(size_t)y * W + x]);
    tex = LoadTextureFromImage(img);
    UnloadImage(img);
    SetTextureFilter(tex, TEXTURE_FILTER_POINT);
    return tex;
}

static void norseFrame(Rectangle r, float u, bool sides, int seed = 0, bool tabletop = false)
{
    const float edge = 3 * u, chan = 14 * u, in = edge + 6 * u;
    DrawRectangle((int)r.x - 2, (int)r.y + 3, (int)r.width + 4, (int)r.height + 4, {0, 0, 0, 110});
    float pw = 30 * u;
    for (int i = 0; r.x + i * pw < r.x + r.width; i++) // upright planks, each its own tone, with grain
    {
        float px = r.x + i * pw, w = std::min(pw, r.x + r.width - px), t = 0.86f + 0.26f * hash2(i, seed, 61);
        DrawRectangleRec({px, r.y, w, r.height}, {(unsigned char)(54 * t), (unsigned char)(38 * t), (unsigned char)(26 * t), 255});
        DrawRectangleRec({px, r.y, std::max(1.0f, u), r.height}, {20, 13, 9, 255});
        for (int k = 1; k < 4; k++) DrawRectangleRec({px + w * (k / 4.0f + 0.06f * hash2(i, k, 62)), r.y, std::max(1.0f, u * 0.7f), r.height}, {30, 20, 13, 70});
    }
    DrawRectangleLinesEx(r, edge, {18, 12, 9, 255});
    DrawRectangleLinesEx({r.x + edge, r.y + edge, r.width - 2 * edge, r.height - 2 * edge}, std::max(1.0f, 1.5f * u), {156, 116, 68, 255});
    Rectangle top = {r.x + in, r.y + in, r.width - 2 * in, chan}, bot = {r.x + in, r.y + r.height - in - chan, r.width - 2 * in, chan};
    Rectangle lef = {r.x + in, r.y + in, chan, r.height - 2 * in}, rig = {r.x + r.width - in - chan, r.y + in, chan, r.height - 2 * in};
    Rectangle inner = sides ? Rectangle{r.x + in + chan + 2 * u, r.y + in + chan + 2 * u, r.width - 2 * (in + chan + 2 * u), r.height - 2 * (in + chan + 2 * u)}
                            : Rectangle{r.x + in, r.y + in + chan + 2 * u, r.width - 2 * in, r.height - 2 * (in + chan + 2 * u)};
    if (tabletop) { Texture2D& bt = boardTexture(); DrawTexturePro(bt, {0, 0, (float)bt.width, (float)bt.height}, inner, {0, 0}, 0, WHITE); }
    else DrawRectangleRec(inner, {42, 30, 22, 255});
    DrawRectangleLinesEx(inner, std::max(1.0f, u), {12, 8, 6, 255});
    Rectangle ch[4] = {top, bot, lef, rig};
    for (int i = 0; i < (sides ? 4 : 2); i++)
    {
        DrawRectangleRec(ch[i], {9, 13, 24, 255});
        DrawRectangleLinesEx(ch[i], std::max(1.0f, u), {82, 62, 38, 255});
    }
    BeginBlendMode(BLEND_ADDITIVE);
    float rh = chan * 0.74f, adv = runeAdvance(rh) + 2 * u;
    for (int i = 0; i < (sides ? 4 : 2); i++)
    {
        bool horiz = i < 2;
        float len = horiz ? ch[i].width : ch[i].height, step = horiz ? adv : rh + 3 * u;
        int n = std::max(1, (int)((len - 2 * chan) / step));
        float start = (len - n * step) / 2;
        for (int k = 0; k < n; k++)
        {
            float pulse = 0.72f + 0.28f * std::sin(G.frame * 0.03f + k * 0.9f + i * 1.7f);
            float x = horiz ? ch[i].x + start + k * step : ch[i].x + (chan - rh * 0.5f) / 2, y = horiz ? ch[i].y + (chan - rh) / 2 : ch[i].y + start + k * step;
            glowRune((int)(hash2(k, i + seed * 7, 63) * 24), x, y, rh, {92, 168, 255, 255}, pulse);
        }
    }
    EndBlendMode();
    for (int i = 0; i < 4; i++) // iron studs at the corners
    {
        float cx = r.x + (i % 2 ? r.width - in - chan / 2 : in + chan / 2), cy = r.y + (i / 2 ? r.height - in - chan / 2 : in + chan / 2);
        DrawCircleV({cx, cy}, chan * 0.62f, {20, 14, 10, 255});
        DrawCircleV({cx, cy}, chan * 0.5f, {128, 96, 56, 255});
        DrawCircleV({cx - chan * 0.12f, cy - chan * 0.12f}, chan * 0.18f, {222, 190, 120, 255});
    }
}

// Compact wooden plate: tooltips, small panels.
static void panel(Rectangle r, Color fill = {18, 16, 26, 238})
{
    float t = std::max(1.0f, 2 * U());
    DrawRectangleRec(r, {(unsigned char)(fill.r * 0.5f + 20), (unsigned char)(fill.g * 0.5f + 14), (unsigned char)(fill.b * 0.4f + 10), fill.a});
    DrawRectangleLinesEx(r, t * 1.5f, {18, 12, 9, 255});
    DrawRectangleLinesEx({r.x + t * 1.5f, r.y + t * 1.5f, r.width - 3 * t, r.height - 3 * t}, std::max(1.0f, t * 0.6f), {132, 98, 58, 255});
    for (int i = 0; i < 4; i++) DrawCircleV({r.x + (i % 2 ? r.width - 4 * t : 4 * t), r.y + (i / 2 ? r.height - 4 * t : 4 * t)}, t * 1.4f, {196, 158, 94, 255});
}

// A carved slot: sunk into the wood, lit from the top left.
static void slotBox(Rectangle r, bool hot, bool selected)
{
    Color base = selected ? Color{76, 54, 30, 255} : (hot ? Color{60, 45, 34, 255} : Color{35, 26, 20, 255});
    float t = std::max(1.0f, r.width * 0.035f);
    DrawRectangleRec(r, base);
    DrawRectangleRec({r.x, r.y, r.width, t}, {12, 8, 6, 255});
    DrawRectangleRec({r.x, r.y, t, r.height}, {12, 8, 6, 255});
    DrawRectangleRec({r.x, r.y + r.height - t, r.width, t}, {104, 78, 48, 255});
    DrawRectangleRec({r.x + r.width - t, r.y, t, r.height}, {104, 78, 48, 255});
    if (selected) DrawRectangleLinesEx({r.x - 1, r.y - 1, r.width + 2, r.height + 2}, 2.5f, {255, 214, 140, 255});
    else if (hot) DrawRectangleLinesEx(r, 1.5f, {196, 156, 96, 255});
}

static void bar(float x, float y, float w, float h, float t, Color fill, Color back)
{
    Rectangle r = {x, y, w, h};
    DrawRectangleRounded(r, 0.4f, 4, back);
    if (t > 0) DrawRectangleRounded({x, y, std::max(h, w * clampf(t, 0, 1)), h}, 0.4f, 4, fill);
    if (t > 0) DrawRectangleRounded({x + 2, y + 1, std::max(0.0f, w * clampf(t, 0, 1) - 4), h * 0.35f}, 0.4f, 4, {255, 255, 255, 40});
    DrawRectangleRoundedLinesEx(r, 0.4f, 4, 1, {0, 0, 0, 160});
}

static float keycapW(const std::string& k, float size) { return std::max(size * 1.2f, uiTextWidth(k, size * 0.8f, 1) + size * 0.6f); }
static void keycap(const std::string& k, float x, float y, float size)
{
    float w = keycapW(k, size);
    Rectangle r = {x, y, w, size * 1.3f};
    DrawRectangleRounded(r, 0.25f, 4, {50, 46, 62, 255});
    DrawRectangleRounded({x, y + size * 1.1f, w, size * 0.2f}, 0.25f, 4, {20, 18, 26, 255});
    DrawRectangleRoundedLinesEx(r, 0.25f, 4, 1.5f, {170, 160, 190, 255});
    textC(k, x + w / 2, y + size * 0.18f, size * 0.85f, INK, 1);
}

static void tooltip(const std::vector<std::pair<std::string, Color>>& lines)
{
    if (lines.empty()) return;
    float u = U(), fs = 17 * u, pad = 10 * u;
    float w = 0;
    for (size_t i = 0; i < lines.size(); i++) w = std::max(w, uiTextWidth(lines[i].first, i == 0 ? fs * 1.15f : fs, i == 0 ? 1 : 0));
    float h = lines.size() * (fs + 5 * u) + pad * 2 + fs * 0.2f;
    Vector2 m = GetMousePosition();
    float x = std::min(m.x + 18, GetScreenWidth() - w - pad * 2 - 4), y = std::min(m.y + 18, GetScreenHeight() - h - 4);
    panel({x, y, w + pad * 2, h}, {12, 10, 18, 245});
    float ty = y + pad;
    for (size_t i = 0; i < lines.size(); i++)
    {
        float s = i == 0 ? fs * 1.15f : fs;
        text(lines[i].first, x + pad, ty, s, lines[i].second, i == 0 ? 1 : 0);
        ty += s + 5 * u;
    }
}

static std::string fmt1(float v)
{
    char b[32];
    std::snprintf(b, sizeof(b), "%.1f", v);
    return b;
}

// ---------------------------------------------------------------- pixel icons

// '.' empty, 'a' main colour, 'A' main dark, other letters fixed colours; every icon gets an outline
static void pixelIcon(const char* const* rows, int n, float x, float y, float size, Color main)
{
    int w = (int)std::strlen(rows[0]);
    float p = size / std::max(w, n);
    float ox = x + (size - w * p) / 2, oy = y + (size - n * p) / 2;
    for (int pass = 0; pass < 2; pass++)
        for (int j = 0; j < n; j++)
            for (int i = 0; i < w; i++)
            {
                char ch = rows[j][i];
                if (ch == '.') continue;
                Color c;
                switch (ch)
                {
                case 'a': c = main; break;
                case 'A': c = {(unsigned char)(main.r * 0.6f), (unsigned char)(main.g * 0.6f), (unsigned char)(main.b * 0.6f), 255}; break;
                case 'w': c = {250, 250, 255, 255}; break;
                case 'y': c = {255, 220, 90, 255}; break;
                case 'o': c = {255, 140, 40, 255}; break;
                case 'r': c = {220, 50, 40, 255}; break;
                case 'R': c = {140, 26, 30, 255}; break;
                case 'b': c = {90, 160, 255, 255}; break;
                case 'B': c = {40, 80, 170, 255}; break;
                case 'g': c = {120, 230, 90, 255}; break;
                case 'k': c = {40, 36, 44, 255}; break;
                case 'n': c = {150, 150, 160, 255}; break;
                case 'p': c = {130, 88, 50, 255}; break;
                default: c = main;
                }
                float px = ox + i * p, py = oy + j * p;
                if (pass == 0) DrawRectangle((int)(px - p * 0.6f), (int)(py - p * 0.6f), (int)std::ceil(p * 2.2f), (int)std::ceil(p * 2.2f), {12, 10, 16, 210});
                else DrawRectangle((int)px, (int)py, (int)std::ceil(p), (int)std::ceil(p), c);
            }
}

static const char* IC_SPARK[] = {"....w....", "....a....", ".a..a..a.", "..a.a.a..", "....w....", "wawwwwwaw", "....w....", "..a.a.a..", ".a..a..a.", "....a....", "....w...."};
static const char* IC_MISSILE[] = {".....AAA.", "....AaaaA", "...AawaaA", ".a.AaaaaA", "a.a.AaaA.", ".a...AA..", "a.a......", ".a.......", "a........"};
static const char* IC_FIRE[] = {"....r....", "...rr....", "...ror...", "..rooor..", "..royoor.", ".roywyor.", ".royyyor.", ".rooyoor.", "..rooor..", "...rrr..."};
static const char* IC_ICE[] = {"....w....", "...wbw...", "...wbB...", "..wbbB...", "..wbbbB..", ".wbbbbB..", ".wbbbbBB.", "..bbbbB..", "...bBB...", "....B...."};
static const char* IC_BOLT[] = {".....yy..", "....yy...", "...yy....", "..yyyyy..", "....yy...", "...yy....", "..yy.....", ".yy......", "yy......."};
static const char* IC_ACID[] = {"....g....", "....g....", "...ggg...", "..gwggg..", "..wgggg..", ".gggggga.", ".ggggggA.", "..gggAA..", "...AAA..."};
static const char* IC_BOMB[] = {"......yo.", ".....p.y.", "....p....", "..kkkk...", ".kkwkkk..", "kkwkkkkk.", "kkkkkkkk.", "kkkkkkkk.", ".kkkkkk..", "..kkkk..."};
static const char* IC_DIG[] = {"..nnnnn..", ".n.....n.", "n...p...n", "....p....", "....p....", "....p....", "....p....", "....p....", "....p...."};
static const char* IC_WATER[] = {"....b....", "...bb....", "...bwb...", "..bwbbb..", "..bbbbb..", ".bbbbbbB.", ".bbbbbbB.", "..bbbBB..", "...BBB..."};
static const char* IC_TRIGGER[] = {"....w....", "...waw...", "..wa.aw..", ".wa.y.aw.", "wa.yyy.aw", ".wa.y.aw.", "..wa.aw..", "...waw...", "....w...."};
static const char* IC_DMG[] = {"....r....", "...rrr...", "..rrrrr..", ".rr.r.rr.", "....r....", "....r....", "..rrrrr..", "....r....", "....r...."};
static const char* IC_SPEED[] = {"a...a....", ".a...a...", "..a...a..", "...a...a.", "....a...a", "...a...a.", "..a...a..", ".a...a...", "a...a...."};
static const char* IC_BOUNCE[] = {".........", ".a.....a.", ".a.....a.", "..a...a..", "..a...a..", "...a.a...", "...a.a...", "....w....", "nnnnnnnnn"};
static const char* IC_HOMING[] = {"...aaa...", ".aa...aa.", ".a.....a.", "a...r...a", "a..rwr..a", "a...r...a", ".a.....a.", ".aa...aa.", "...aaa..."};
static const char* IC_PIERCE[] = {"....a....", "...aaa...", "..a.a.a..", "....a....", "nnnnannnn", "....a....", "....a....", "...aaa...", "..a...a.."};
static const char* IC_IGNITE[] = {"...r.....", "..ro...r.", "..roo.ro.", ".royoroo.", ".roywyor.", ".royyyor.", "..rooor..", "...rrr...", "........."};
static const char* IC_FROST[] = {"....w....", ".w..w..w.", "..w.w.w..", "...www...", "wwwwbwwww", "...www...", "..w.w.w..", ".w..w..w.", "....w...."};
static const char* IC_EXPLO[] = {"....y....", ".y..o..y.", "..yoooy..", ".yoorooy.", "yoorwroy.", ".yoorooy.", "..yoooy..", ".y..o..y.", "....y...."};
static const char* IC_DOUBLE[] = {".........", "..aa.aa..", ".aaaaaaa.", ".aawaawa.", ".aaaaaaa.", "..aa.aa..", ".........", "...w.w...", "........."};
static const char* IC_TRIPLE[] = {"...aaa...", "..aawaa..", "...aaa...", ".........", "aaa...aaa", "awa...awa", "aaa...aaa", ".........", "........."};

static const char* IC_MAPICON[] = {"ppppppppp", "pwwwwwwwp", "pw.b.wgwp", "pw..bb.wp", "pwr.b..wp", "pw..b.gwp", "pwg.bb.wp", "pwwwwwwwp", "ppppppppp"};
static const char* IC_BLOODSPEAR[] = {".......aa", "......aaw", ".....aa..", "....aa...", "...aa....", "..aA.....", ".aA......", "aA.......", "A........"};

static const char* const* spellIcon(int s, int& n)
{
    static const char* const* list[SPELL_COUNT] = {IC_SPARK, IC_MISSILE, IC_FIRE, IC_ICE, IC_BOLT, IC_ACID, IC_BOMB, IC_DIG, IC_WATER, IC_TRIGGER,
                                                   IC_DMG, IC_SPEED, IC_BOUNCE, IC_HOMING, IC_PIERCE, IC_IGNITE, IC_FROST, IC_EXPLO, IC_DOUBLE, IC_TRIPLE, IC_BLOODSPEAR};
    static const int rows[SPELL_COUNT] = {11, 9, 10, 10, 9, 9, 10, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9};
    n = rows[s];
    return list[s];
}

static const char* IC_NUGGET[] = {"..........", "...aaa....", "..aawaa...", ".aawaaaA..", ".aaaaaaAA.", ".AaaaaaAA.", "..AAAAAA..", ".........."};
static const char* IC_CRYSTAL[] = {"....w.....", "...waa....", "..waaaA...", "..waaaA...", ".waaaaAA..", ".waaaaAA..", "..aaaaA...", "...AAA...."};
static const char* IC_HEART[] = {".rr...rr.", "rwrr.rrrr", "rwrrrrrrr", "rrrrrrrrr", ".rrrrrrr.", "..rrrrr..", "...rrr...", "....r...."};
static const char* IC_DROP[] = {"....b....", "...bb....", "...bwb...", "..bwbbb..", "..bbbbb..", ".bbbbbbB.", ".bbbbbbB.", "..bbbBB..", "...BBB..."};
static const char* IC_BOOT[] = {"..ppp....", "..ppp....", "..ppp....", "..ppp....", "..pppp...", "..ppppppp", ".pppppppp", ".kkkkkkkk"};
static const char* IC_SKULL[] = {"..wwwww..", ".wwwwwww.", "wwkkwkkww", "wwkkwkkww", "wwwwwwwww", ".wwwkwww.", "..wkwkw..", "..wwwww.."};
static const char* IC_OND[] = {"..pppp..", ".pawabp.", "pawbaabp", "pabwabbp", "pabawaBp", "paBbaBBp", ".pBBBBp.", "..pppp.."};
static const char* IC_FLASK[] = {"...nn....", "...ww....", "...ww....", "..wrrw...", ".wrrrrw..", ".rrwrrr..", ".rrrrrr..", "..rrrr..."};
static const char* IC_COIN[] = {"..yyyy..", ".yywwyy.", "yywyyyyo", "yywyyyyo", "yyyyyyyo", "yyyyyyoo", ".yyyyoo.", "..oooo.."};
static const char* IC_HOOK[] = {"....nn...", "...n..n..", "...n..n..", "....nn...", ".....n...", ".....n...", "n....n...", "nn..nn...", ".nnnn...."};
static const char* IC_WISP[] = {"...y....", "..yww...", ".ywwwy..", ".ywwwy..", "..ywy...", "...yo...", "..o.o...", "...o...."};
static const char* IC_SHIELD[] = {"aaaaaaaa", "awaaaaaA", "awaaaaaA", "aaaaaaaA", "AaaaaaAA", ".AaaaAA.", "..AaAA..", "...AA..."};

void drawResIcon(int r, float x, float y, float size)
{
    bool gem = r >= R_FIRESTONE || r == R_GOLD;
    pixelIcon(gem ? IC_CRYSTAL : IC_NUGGET, 8, x, y, size, RES_COLORS[r]);
}

static const char* ICON_DAGGER[] = {"..........", "..........", "........m.", ".......mM.", "......mM..", "..y..mM...", "...ymM....", "..byy.....", ".bb..y....", "bb........"};
static const char* ICON_SWORD[] = {".........m", "........mM", ".......mM.", "......mM..", ".....mM...", "..y.mM....", "...yM.....", "..byy.....", ".bb..y....", "bb........"};
static const char* ICON_AXE[] = {"......mmm.", ".....mMMMm", "....bmMMm.", "...b..mm..", "..b.......", ".b........", "b.........", "..........", "..........", ".........."};
static const char* ICON_XBOW[] = {"..........", "......m...", ".......m..", "........m.", "bbbbbbbbmb", "BB......m.", ".......m..", "......m...", "..........", ".........."};
static const char* ICON_STAFF[] = {"........gg", ".......gGg", "......bgg.", ".....b....", "....b.....", "...b......", "..b.......", ".b........", "b.........", ".........."};
static const char* ICON_SPEAR[] = {".........m", "........mm", ".......mM.", "......b...", ".....b....", "....b.....", "...b......", "..b.......", ".b........", "b........."};
static const char* ICON_MACE[] = {".......mm.", "......mMMm", "......mMMm", ".......mm.", "......b...", ".....b....", "....b.....", "...b......", "..b.......", ".b........"};
static const char* ICON_PAN[] = {"....MMMM..", "...MmmmmM.", "..MmmmmmmM", "..MmmmmmmM", "..MmmmmmmM", "...MmmmmM.", "..b.MMMM..", ".b........", "b.........", ".........."};
static const char* ICON_ARMOUR[] = {"..........", ".mm....mm.", ".mMmmmmMm.", "..mmmmmm..", "..mMmmMm..", "..mmmmmm..", "..mMmmMm..", "...mmmm...", "..........", ".........."};

static void drawIcon10(const char* const* rows, float x, float y, float size, Color metal, Color gem)
{
    float px = size / 10.0f;
    for (int pass = 0; pass < 2; pass++)
        for (int j = 0; j < 10; j++)
            for (int i = 0; i < 10; i++)
            {
                char ch = rows[j][i];
                Color c;
                switch (ch)
                {
                case 'm': c = metal; break;
                case 'M': c = {(unsigned char)(metal.r * 0.6f), (unsigned char)(metal.g * 0.6f), (unsigned char)(metal.b * 0.6f), 255}; break;
                case 'b': c = {130, 86, 48, 255}; break;
                case 'B': c = {78, 50, 28, 255}; break;
                case 'y': c = {226, 186, 64, 255}; break;
                case 'g': c = gem; break;
                case 'G': c = WHITE; break;
                default: continue;
                }
                float X = x + i * px, Y = y + j * px;
                if (pass == 0) DrawRectangle((int)(X - px * 0.5f), (int)(Y - px * 0.5f), (int)std::ceil(px * 2), (int)std::ceil(px * 2), {12, 10, 16, 220});
                else DrawRectangle((int)X, (int)Y, (int)std::ceil(px), (int)std::ceil(px), c);
            }
}

static Color tierColor(int t)
{
    static const Color c[6] = {{150, 150, 150, 255}, {190, 130, 90, 255}, {190, 190, 200, 255}, {120, 200, 255, 255}, {200, 140, 255, 255}, {255, 210, 90, 255}};
    return c[std::max(0, std::min(5, t))];
}

void drawItemIcon(const Weapon& w, float x, float y, float size)
{
    if (w.type != W_STAFF && w.type != W_ARMOUR && size >= 24) // tier pips in the corner
    {
        int t = weaponTier(w);
        float p = std::max(2.0f, size * 0.07f);
        for (int i = 0; i < t; i++)
        {
            DrawRectangle((int)(x + i * (p + 2)) - 1, (int)(y + size - p) - 1, (int)p + 2, (int)p + 2, {12, 10, 16, 230});
            DrawRectangle((int)(x + i * (p + 2)), (int)(y + size - p), (int)p, (int)p, tierColor(t));
        }
    }
    Color mc = METALS[w.metal].color;
    if (w.glow.a) // legendary: pulsing aura behind the icon
    {
        float p = 0.7f + 0.3f * std::sin(GetTime() * 4);
        BeginBlendMode(BLEND_ADDITIVE);
        DrawCircleGradient((int)(x + size / 2), (int)(y + size / 2), size * 0.75f, {w.glow.r, w.glow.g, w.glow.b, (unsigned char)(120 * p)}, {w.glow.r, w.glow.g, w.glow.b, 0});
        EndBlendMode();
    }
    if (w.type == W_PAN) mc = {96, 96, 104, 255};
    if (w.type != W_ARMOUR) // the weapon's own sprite, laid corner to corner
    {
        float L = weaponLength(w) * 2 + 6;
        drawWeaponSprite(w, {x + size / 2, y + size / 2}, -PI / 4, size * 1.2f / L, true);
        return;
    }
    switch (w.type)
    {
    case W_SPEAR: drawIcon10(ICON_SPEAR, x, y, size, mc, WHITE); break;
    case W_MACE: drawIcon10(ICON_MACE, x, y, size, mc, WHITE); break;
    case W_PAN: drawIcon10(ICON_PAN, x, y, size, mc, WHITE); break;
    case W_DAGGER: drawIcon10(ICON_DAGGER, x, y, size, mc, WHITE); break;
    case W_SWORD: drawIcon10(ICON_SWORD, x, y, size, mc, WHITE); break;
    case W_AXE: drawIcon10(ICON_AXE, x, y, size, mc, WHITE); break;
    case W_CROSSBOW: drawIcon10(ICON_XBOW, x, y, size, mc, WHITE); break;
    case W_STAFF: drawIcon10(ICON_STAFF, x, y, size, mc, w.staff.gem); break;
    case W_ARMOUR: drawIcon10(ICON_ARMOUR, x, y, size, mc, WHITE); break;
    }
}

void drawSpellIcon(int spell, float x, float y, float size, bool highlight, int uses)
{
    Rectangle r = {x, y, size, size};
    if (spell < 0)
    {
        DrawRectangleRounded(r, 0.15f, 4, {26, 24, 34, 255});
        DrawRectangleRoundedLinesEx(r, 0.15f, 4, 1.5f, highlight ? Color{255, 230, 150, 255} : Color{66, 60, 82, 255});
        return;
    }
    const SpellDef& d = SPELLS[spell];
    Color col = d.col;
    Color bg = {(unsigned char)(14 + col.r * 0.16f), (unsigned char)(12 + col.g * 0.16f), (unsigned char)(20 + col.b * 0.16f), 255};
    Color border = d.type == ST_PROJ ? Color{100, 150, 255, 255} : (d.type == ST_MOD ? Color{110, 220, 120, 255} : Color{240, 200, 80, 255});
    float t = std::max(1.0f, size * 0.03f), pulse = 0.8f + 0.2f * std::sin(G.frame * 0.05f + spell * 1.3f);
    DrawRectangleRounded(r, 0.15f, 4, bg);
    BeginBlendMode(BLEND_ADDITIVE); // the glyph glows in its own colour, a little brighter when picked up
    DrawCircleGradient((int)(x + size / 2), (int)(y + size / 2), size * (highlight ? 0.62f : 0.5f) * pulse, {col.r, col.g, col.b, (unsigned char)(highlight ? 120 : 78)}, {col.r, col.g, col.b, 0});
    EndBlendMode();
    DrawRectangleRec({x + size * 0.1f, y + t * 2, size * 0.8f, t}, {255, 255, 255, 40});                      // a bevel: light along the top, dark along the foot
    DrawRectangleRec({x + size * 0.1f, y + size - t * 3, size * 0.8f, t}, {0, 0, 0, 90});
    DrawRectangleRoundedLinesEx({x + t * 2, y + t * 2, size - t * 4, size - t * 4}, 0.12f, 4, std::max(1.0f, t * 0.6f), {border.r, border.g, border.b, 60}); // an inner rim
    DrawRectangleRoundedLinesEx(r, 0.15f, 4, highlight ? 3.0f : 2.0f, highlight ? Color{255, 240, 180, 255} : border);
    int n;
    const char* const* ic = spellIcon(spell, n);
    pixelIcon(ic, n, x + size * 0.15f, y + size * 0.12f, size * 0.7f, col);
    float pr = size * 0.07f, px0 = x + size * 0.14f, py0 = y + size * 0.14f; // a pip for the kind of spell: a gem, a diamond or a pair
    if (d.type == ST_PROJ) DrawCircleV({px0, py0}, pr, border);
    else if (d.type == ST_MOD) DrawPoly({px0, py0}, 4, pr * 1.3f, 0, border);
    else { DrawCircleV({px0 - pr, py0}, pr * 0.8f, border); DrawCircleV({px0 + pr, py0}, pr * 0.8f, border); }
    if (uses >= 0)
    {
        std::string s = std::to_string(uses);
        float fs = std::max(11.0f, size * 0.32f);
        float w = uiTextWidth(s, fs, 1) + 6;
        DrawRectangleRounded({x + size - w - 1, y + size - fs - 3, w, fs + 2}, 0.4f, 4, {10, 8, 14, 220});
        text(s, x + size - w + 2, y + size - fs - 2, fs, uses <= 2 ? Color{255, 120, 100, 255} : INK, 1);
    }
}

// A spell lying on the ground: a rune-tablet of dark stone, bevelled and rimmed in its kind's colour, the glyph glowing
// on it, bobbing in a halo with a spark circling (world units: the tablet is 11 x 11, centred on cx, cy).
void drawSpellPickup(int spell, float cx, float cy, float phase)
{
    const SpellDef& d = SPELLS[spell];
    Color col = d.col, border = d.type == ST_PROJ ? Color{100, 150, 255, 255} : (d.type == ST_MOD ? Color{110, 220, 120, 255} : Color{240, 200, 80, 255});
    int x = (int)std::floor(cx) - 5, y = (int)std::floor(cy) - 5;
    float pulse = 0.75f + 0.25f * std::sin(phase);
    DrawEllipse(x + 5, y + 14, 5, 1.3f, {0, 0, 0, 80});
    BeginBlendMode(BLEND_ADDITIVE);
    DrawCircleGradient(x + 5, y + 5, 15 * pulse, {col.r, col.g, col.b, 80}, {col.r, col.g, col.b, 0});
    EndBlendMode();
    DrawRectangle(x + 1, y, 9, 11, {14, 12, 20, 255}); // a tablet with its corners clipped
    DrawRectangle(x, y + 1, 11, 9, {14, 12, 20, 255});
    DrawRectangle(x + 1, y + 1, 9, 9, {(unsigned char)(26 + col.r * 0.12f), (unsigned char)(24 + col.g * 0.12f), (unsigned char)(34 + col.b * 0.12f), 255});
    DrawRectangle(x + 1, y + 1, 9, 1, {86, 80, 106, 255}); // lit from above
    DrawRectangle(x + 1, y + 1, 1, 9, {64, 60, 82, 255});
    DrawRectangle(x + 1, y + 9, 9, 1, {10, 8, 14, 255});
    DrawRectangle(x + 9, y + 1, 1, 9, {10, 8, 14, 255});
    for (int i = 0; i < 4; i++) DrawRectangle(x + (i % 2 ? 8 : 2), y + (i / 2 ? 8 : 2), 1, 1, border); // rivets in the kind's colour
    int n;
    const char* const* ic = spellIcon(spell, n);
    pixelIcon(ic, n, (float)(x + 1), (float)(y + 1), 9, col);
    float a = phase * 0.7f; // a mote circling it
    BeginBlendMode(BLEND_ADDITIVE);
    DrawRectangle((int)(x + 5 + std::cos(a) * 8), (int)(y + 5 + std::sin(a) * 6), 1, 1, {(unsigned char)std::min(255, col.r + 80), (unsigned char)std::min(255, col.g + 80), (unsigned char)std::min(255, col.b + 80), 230});
    EndBlendMode();
    if (G.frame % 10 == 0) spawnParticle(cx + frange(-4, 4), cy + frange(-3, 3), 0, -0.25f, 26, col, -0.002f);
}

// A parchment scroll lying on its side, tied with a band and sealed in wax of its own colour; w wide, centred on (cx, cy).
void drawScroll(int sc, float cx, float cy, float w, float glow)
{
    Color col = SCROLLS[sc].col;
    float h = w * 0.5f, x0 = cx - w / 2, y0 = cy - h / 2, pulse = 0.8f + 0.2f * std::sin(G.frame * 0.06f + sc * 1.7f);
    BeginBlendMode(BLEND_ADDITIVE);
    DrawCircleGradient((int)cx, (int)cy, w * 0.95f * pulse * glow, {col.r, col.g, col.b, (unsigned char)(80 * glow)}, {col.r, col.g, col.b, 0});
    EndBlendMode();
    Color pa = {240, 228, 186, 255}, pb = {212, 194, 146, 255}, pc = {150, 126, 88, 255}, ink = {96, 76, 52, 255};
    float by0 = y0 + h * 0.18f, bh = h * 0.64f;                                              // the unrolled middle of the sheet
    DrawRectangleRec({x0 + w * 0.1f, by0, w * 0.8f, bh}, pb);
    DrawRectangleRec({x0 + w * 0.1f, by0, w * 0.8f, bh * 0.26f}, pa);                          // lit along the top
    DrawRectangleRec({x0 + w * 0.1f, by0 + bh * 0.76f, w * 0.8f, bh * 0.24f}, pc);             // shaded below
    for (int s = 0; s < 2; s++)                                                              // the rolled ends: a spiral seen end-on
    {
        float ex = s ? x0 + w * 0.91f : x0 + w * 0.09f, r = h * 0.4f;
        DrawCircleV({ex, cy}, r, pc);
        DrawCircleV({ex, cy}, r * 0.78f, pb);
        DrawCircleV({ex, cy}, r * 0.5f, pc);
        DrawCircleV({ex, cy}, r * 0.26f, pb);
    }
    for (int s = 0; s < 2; s++)                                                              // lines of script either side of the band
        for (int k = 0; k < 2; k++)
        {
            float lx = s ? cx + w * 0.14f : x0 + w * 0.2f, ly = by0 + bh * (0.28f + 0.3f * k);
            DrawRectangleRec({lx, ly, w * 0.2f * (k ? 0.7f : 1.0f), std::max(0.4f, h * 0.06f)}, ink);
        }
    DrawRectangleRec({cx - w * 0.06f, y0 + h * 0.08f, w * 0.12f, h * 0.84f}, {(unsigned char)(col.r * 0.55f), (unsigned char)(col.g * 0.55f), (unsigned char)(col.b * 0.55f), 255}); // the band
    DrawRectangleRec({cx - w * 0.06f, y0 + h * 0.08f, w * 0.045f, h * 0.84f}, col);
    DrawCircleV({cx, cy}, h * 0.22f, {(unsigned char)(col.r * 0.5f), (unsigned char)(col.g * 0.5f), (unsigned char)(col.b * 0.5f), 255}); // the wax seal
    DrawCircleV({cx, cy}, h * 0.17f, col);
    DrawCircleV({cx - h * 0.05f, cy - h * 0.05f}, h * 0.06f, lerpColor(col, WHITE, 0.7f));
}

static std::vector<std::pair<std::string, Color>> spellTip(int s, int uses = -1)
{
    const SpellDef& d = SPELLS[s];
    static const char* types[] = {"Projectile", "Modifier", "Multicast"};
    std::vector<std::pair<std::string, Color>> t = {{d.name, d.col}, {types[d.type], DIM}, {d.desc, INK}, {"Mana cost: " + std::to_string(d.mana), {120, 170, 255, 255}}};
    if (d.type == ST_PROJ)
    {
        t.push_back({"Damage: " + fmt1(d.dmg * spellPower()) + "  (" + ELEMENT_NAMES[d.el] + ")", ELEMENT_COLORS[d.el]});
        if (d.blast) t.push_back({"Explosion radius: " + std::to_string(d.blast), ORANGE});
    }
    if (d.delay) t.push_back({"Cast delay: +" + fmt1(d.delay / 60.0f) + "s", DIM});
    if (d.uses) t.push_back({"Charges: " + std::to_string(uses >= 0 ? uses : d.uses) + " / " + std::to_string(d.uses) + "  (consumed, never restored)", {255, 170, 120, 255}});
    return t;
}

static std::vector<std::pair<std::string, Color>> weaponTip(const Weapon& w)
{
    std::vector<std::pair<std::string, Color>> t = {{weaponName(w), w.glow.a ? w.glow : (w.type == W_STAFF ? w.staff.gem : METALS[w.metal].color)}};
    static const char* roman[] = {"", "I", "II", "III", "IV", "V"};
    if (w.fx) t.push_back({std::string("Legendary ") + METALS[w.metal].name + " " + WTYPES[w.type].name + "  -  Tier " + roman[weaponTier(w)], C_GOLD});
    else if (w.type != W_STAFF) t.push_back({std::string("Tier ") + roman[weaponTier(w)], tierColor(weaponTier(w))});
    if (w.type == W_STAFF)
    {
        const Staff& s = w.staff;
        t.push_back({"Mana " + std::to_string((int)s.manaMax) + "    Regen " + std::to_string((int)(s.regen * 60)) + " / s", {120, 170, 255, 255}});
        t.push_back({"Cast delay " + fmt1(s.delay / 60.0f) + "s    Recharge " + fmt1(s.recharge / 60.0f) + "s", INK});
        t.push_back({"Capacity " + std::to_string(s.slots.size()) + "    Spread " + fmt1(s.spread) + "    Spells per cast " + std::to_string(s.perCast), INK});
    }
    else
    {
        const MetalDef& md = METALS[w.metal];
        t.push_back({"Damage " + fmt1(weaponDamage(w)) + "  (" + ELEMENT_NAMES[md.el] + ")", ELEMENT_COLORS[md.el]});
        t.push_back({"Attacks " + fmt1(60.0f / weaponCooldown(w)) + " per second", INK});
        for (int b = 1; b < (1 << UF_COUNT); b <<= 1)
            if (w.fx & b) t.push_back({std::string("* ") + fxDescription(b), w.glow.a ? w.glow : INK});
        if (w.type != W_CROSSBOW && w.type != W_PAN) t.push_back({"Can chip ore up to hardness " + std::to_string(md.mine), DIM});
        if (!w.lore.empty()) t.push_back({w.lore, DIM});
    }
    return t;
}

// ---------------------------------------------------------------- HUD

void drawHUD()
{
    float u = U();
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    Player& P = G.p;
    Mob& pm = P.m;

    for (auto& t : G.texts)
    {
        float x = (t.x - G.rcx) * G.scale, y = (t.y - G.rcy) * G.scale;
        Color c = t.col;
        c.a = (unsigned char)(255 * std::min(1.0f, t.life / 20.0f));
        textC(t.s, x, y, 17 * u, c, 1);
    }
    Vector2 tag;
    if (G.state == GS_PLAY && indiTag(tag)) // Indi's name tag
    {
        float x = (tag.x - G.rcx) * G.scale, y = (tag.y - G.rcy) * G.scale, fs = 15 * u, w = uiTextWidth("Indi", fs, 1) + 12 * u;
        DrawRectangleRounded({x - w / 2, y - 3 * u, w, fs + 6 * u}, 0.5f, 6, {20, 18, 24, 190});
        textC("Indi", x, y, fs, {236, 222, 190, 255}, 1);
    }

    if (G.nearInteract >= 0 && G.nearInteract < (int)G.inter.size() && G.state == GS_PLAY)
    {
        const Interact& it = G.inter[G.nearInteract];
        static const char* verbs[] = {"Open chest", "Forge at the anvil", "Pray at the shrine", "", "Take the weapon", "Trade", "", "Open the door", "Board the longship and set sail"};
        float x = (it.x - G.rcx) * G.scale, y = (it.y - 46 - G.rcy) * G.scale;
        float fs = 18 * u, kw = keycapW("F", fs);
        float total = kw + 8 * u + uiTextWidth(it.type == IT_SHOP ? "Trade with the Weaponsmith" : verbs[it.type], fs, 1);
        DrawRectangleRounded({x - total / 2 - 8 * u, y - 6 * u, total + 16 * u, fs * 1.3f + 12 * u}, 0.3f, 4, {10, 8, 14, 190});
        keycap("F", x - total / 2, y, fs);
        std::string verb = it.type == IT_SHOP ? std::string("Trade with the ") + SHOP_NAMES[it.data % 3] : verbs[it.type];
        text(verb, x - total / 2 + kw + 8 * u, y + 2 * u, fs, C_GOLD, 1);
    }

    // vitals: a runed plate above the hotbar - health on top, mana and stamina side by side beneath
    float slot = 44 * u, gap = 6 * u;
    float hx = sw / 2.0f - (6 * slot + 5 * gap) / 2, hy = sh - slot - 16 * u;
    const Weapon* cur = P.hotbar.empty() ? nullptr : &P.hotbar[P.sel];
    float pw = 6 * slot + 5 * gap + 20 * u, ph = 24 * u;
    Rectangle plate = {hx - 10 * u, hy - 8 * u - 4 * u - ph, pw, ph};
    {
        // thin vitals just over the hotbar: health on top, mana and stamina side by side, a rune at the head of each
        float px = plate.x, pwid = plate.width, py = plate.y + 1 * u;
        auto vital = [&](float x0, float y0, float w, float h, int rune, Color glow, float t, Color fill, Color back) {
            float rh = h + 5 * u, lead = rh * 0.5f + 5 * u;
            BeginBlendMode(BLEND_ADDITIVE);
            glowRune(rune, x0, y0 + h / 2 - rh / 2, rh, glow, 0.9f);
            EndBlendMode();
            float bx0 = x0 + lead, bwid = w - lead;
            DrawRectangleRec({bx0 - 1, y0 - 1, bwid + 2, h + 2}, {10, 7, 5, 255});
            DrawRectangleRec({bx0, y0, bwid, h}, back);
            if (t > 0) DrawRectangleRec({bx0, y0, bwid * clampf(t, 0, 1), h}, fill);
            if (t > 0) DrawRectangleRec({bx0, y0, bwid * clampf(t, 0, 1), std::max(1.0f, h * 0.3f)}, {255, 255, 255, 50});
            DrawRectangleLinesEx({bx0 - 1, y0 - 1, bwid + 2, h + 2}, 1, {120, 90, 54, 255});
            return Rectangle{bx0, y0, bwid, h};
        };
        Rectangle hb = vital(px, py, pwid, 9 * u, RN_ALGIZ, {255, 90, 90, 255}, pm.hp / pm.maxHp, {206, 46, 56, 255}, {50, 12, 18, 255});
        if (u >= 1.0f) textC(std::to_string((int)std::max(0.0f, std::ceil(pm.hp))), hb.x + hb.width / 2, hb.y - 2 * u, 13 * u, INK, 1);
        float py2 = py + 9 * u + 5 * u;
        vital(px, py2, pwid, 5 * u, RN_URUZ, {255, 214, 90, 255}, P.stamina / 100, {232, 202, 74, 255}, {44, 38, 12, 255});
        if (P.breath < 100) // air: a thin bar just over the vitals
        {
            Rectangle ab = {plate.x, plate.y - 5 * u, plate.width, 3 * u};
            DrawRectangleRec(ab, {14, 30, 46, 230});
            DrawRectangleRec({ab.x, ab.y, ab.width * P.breath / 100, ab.height}, P.breath < 30 && G.frame / 8 % 2 ? Color{240, 90, 80, 255} : Color{150, 210, 250, 255});
        }
    }
    float plateTop = plate.y - (P.breath < 100 ? 6 * u : 0);

    float x = 16 * u, y = 14 * u;
    float mapSz = 156 * u, mapFrame = 7 * u;
    if (P.hasMap && G.state == GS_PLAY) drawMinimap(x + mapFrame, y + mapFrame, mapSz, u);
    float cx = x, cy = y + (P.hasMap ? mapSz + 2 * mapFrame + 6 * u : 0);
    auto chip = [&](const char* label, Color c) {
        float w = uiTextWidth(label, 14 * u, 1) + 14 * u;
        DrawRectangleRounded({cx, cy, w, 22 * u}, 0.5f, 4, {c.r, c.g, c.b, 70});
        DrawRectangleRoundedLinesEx({cx, cy, w, 22 * u}, 0.5f, 4, 1.5f, c);
        text(label, cx + 7 * u, cy + 3 * u, 14 * u, INK, 1);
        cx += w + 6 * u;
    };
    if (pm.burn > 0) // on fire: a pulsing warning and the edges of the screen ablaze
    {
        float p = 0.5f + 0.5f * std::sin(G.frame * 0.35f);
        int e = (int)(sh * 0.16f);
        Color hot = {255, (unsigned char)(90 + 60 * p), 20, (unsigned char)(70 + 60 * p)}, none = {255, 90, 20, 0};
        BeginBlendMode(BLEND_ADDITIVE);
        DrawRectangleGradientV(0, sh - e, sw, e, none, hot);
        DrawRectangleGradientV(0, 0, sw, e / 2, hot, none);
        DrawRectangleGradientH(0, 0, e / 2, sh, hot, none);
        DrawRectangleGradientH(sw - e / 2, 0, e / 2, sh, none, hot);
        EndBlendMode();
        chip(G.frame % 30 < 15 ? "ON FIRE - find water!" : "ON FIRE", {255, (unsigned char)(120 + 100 * p), 40, 255});
    }
    if (pm.wet > 0) chip("Soaked - won't burn, conducts", {90, 150, 230, 255});
    if (pm.oily > 0) chip("Oiled - burns hotter", {150, 110, 50, 255});
    if (pm.bloody > 0) chip("Bloodied", {190, 30, 36, 255});
    if (pm.poison > 0) chip("Poisoned", ELEMENT_COLORS[EL_POISON]);
    if (pm.chill > 0) chip("Chilled", ELEMENT_COLORS[EL_ICE]);
    if (pm.shock > 0) chip("Shocked", ELEMENT_COLORS[EL_SHOCK]);

    // stage info, kills, flasks, armour, materials (right)
    float rx = sw - 16 * u;
    std::string title = G.sandbox ? "Sandbox" : (G.inVillage ? "Hearthwick" : (G.sanctuary ? havenName() : regionId() ? regionName(regionId()) : STAGES[G.stage].name));
    text(title, rx - uiTextWidth(title, 24 * u, 2), 12 * u, 24 * u, C_GOLD, 2);
    float ry = 46 * u, rs = 20 * u;
    auto rightStat = [&](const char* const* icon, int rows, Color ic, const std::string& label) {
        float w = uiTextWidth(label, rs * 0.82f, 1);
        pixelIcon(icon, rows, rx - w - rs - 6, ry, rs, ic);
        text(label, rx - w, ry + rs * 0.05f, rs * 0.82f, INK, 1);
        ry += rs + 6 * u;
    };
    if (G.inVillage) rightStat(IC_COIN, 8, WHITE, std::to_string(META.bank) + " coins banked");
    else rightStat(IC_COIN, 8, WHITE, std::to_string(P.coins) + " coins");
    rightStat(IC_SKULL, 8, WHITE, std::to_string(P.kills) + " slain");
    rightStat(IC_FLASK, 8, RED, std::to_string(P.potions) + " flasks  (Q)");
    if (P.ondT > 0) rightStat(IC_OND, 8, {120, 220, 240, 255}, "Önd: " + std::to_string(P.ondT / 60 + 1) + "s, no need of air");
    if (P.bombs > 0) rightStat(IC_BOMB, 8, WHITE, std::to_string(P.bombs) + (P.bombs == 1 ? " rune bomb  (B)" : " rune bombs  (B)"));
    rightStat(IC_SHIELD, 8, P.armour < 0 ? Color{168, 140, 104, 255} : METALS[P.armour].color,
              std::string(P.armour < 0 ? "Gambeson" : METALS[P.armour].name) + "  " + std::to_string((int)(armourDef(P.armour) * 100)) + "%");
    if (P.amulet >= 0) // the amulet worn, on its cord
    {
        std::string s = AMULETS[P.amulet].name;
        float w = uiTextWidth(s, rs * 0.82f, 1), px = rs * 1.4f / 20;
        drawAmulet(P.amulet, rx - w - rs - 6 - 2 * px, ry - rs * 0.2f, px);
        text(s, rx - w, ry + rs * 0.05f, rs * 0.82f, AMULETS[P.amulet].col, 1);
        ry += rs + 10 * u;
    }
    if (P.amulet == AM_VEGVISIR && !G.inVillage && !G.sandbox) // the wayfinder's rune points on
        for (auto& h : G.havens)
        {
            if (h.sealed || h.x0 < pm.cx()) continue;
            float px0 = (pm.cx() - G.rcx) * G.scale, py0 = (pm.cy() - G.rcy) * G.scale;
            float a = std::atan2(h.floor - 20 - pm.cy(), h.x0 + 110 - pm.cx()), R = 54 * u, s = 9 * u;
            Vector2 tip = {px0 + std::cos(a) * R, py0 + std::sin(a) * R};
            Vector2 l = {tip.x - std::cos(a) * s * 2 + std::sin(a) * s, tip.y - std::sin(a) * s * 2 - std::cos(a) * s};
            Vector2 r = {tip.x - std::cos(a) * s * 2 - std::sin(a) * s, tip.y - std::sin(a) * s * 2 + std::cos(a) * s};
            float pulse = 0.6f + 0.4f * std::sin(G.frame * 0.1f);
            DrawTriangle(tip, r, l, {150, 190, 255, (unsigned char)(200 * pulse)});
            break;
        }
    // materials: down the left, beneath the map and any status chips
    {
        bool anyChip = pm.burn > 0 || pm.wet > 0 || pm.oily > 0 || pm.bloody > 0 || pm.poison > 0 || pm.chill > 0 || pm.shock > 0;
        float ly = cy + (anyChip ? 30 * u : 4 * u);
        for (int r = 0; r < RES_COUNT; r++)
        {
            if (!P.res[r]) continue;
            std::string s = std::to_string(P.res[r]) + " " + RES_NAMES[r];
            drawResIcon(r, x + 2 * u, ly, 18 * u);
            text(s, x + 26 * u, ly + 1 * u, 15 * u, INK, 1);
            ly += 21 * u;
        }
    }

    // hotbar
    panel({hx - 10 * u, hy - 8 * u, 6 * slot + 5 * gap + 20 * u, slot + 16 * u}, {12, 10, 18, 220});
    for (int i = 0; i < 6; i++)
    {
        Rectangle r = {hx + i * (slot + gap), hy, slot, slot};
        slotBox(r, false, i == P.sel);
        text(std::to_string(i + 1), r.x + 4 * u, r.y + 1 * u, 13 * u, DIM, 1);
        if (i < (int)P.hotbar.size())
        {
            const Weapon& w = P.hotbar[i];
            drawItemIcon(w, r.x + 9 * u, r.y + 9 * u, slot - 18 * u);
            if (w.type == W_STAFF) bar(r.x + 5 * u, r.y + slot - 8 * u, slot - 10 * u, 4 * u, w.staff.mana / w.staff.manaMax, {64, 116, 236, 255}, {10, 10, 20, 200});
            if (i == P.sel && w.type != W_STAFF && P.attackCd > 0)
            {
                float t = P.attackCd / (float)WTYPES[w.type].cooldown;
                DrawRectangle((int)r.x + 2, (int)(r.y + slot * (1 - t)), (int)slot - 4, (int)(slot * t), {0, 0, 0, 110});
            }
        }
    }
    if (cur)
    {
        float ny = plateTop - 28 * u;
        textC(weaponName(*cur), sw / 2.0f, ny, 19 * u, {244, 226, 190, 255}, 1);
    }

    // scrolls: the case beside the hotbar. R reads the lit one, T chooses the next
    if (!P.scrolls.empty())
    {
        float ss = 40 * u, sg = 5 * u, bx = hx + 6 * slot + 5 * gap + 26 * u, by = hy + (slot - ss) / 2;
        text("R read   T next", bx, by - 18 * u, 13 * u, DIM, 1);
        { int si0 = std::max(0, std::min(P.scrollSel, (int)P.scrolls.size() - 1)); text(SCROLLS[P.scrolls[si0]].name, bx + 112 * u, by - 18 * u, 13 * u, SCROLLS[P.scrolls[si0]].col, 1); }
        for (int i = 0; i < SCROLL_CASE; i++)
        {
            Rectangle r = {bx + i * (ss + sg), by, ss, ss};
            bool have = i < (int)P.scrolls.size();
            slotBox(r, false, have && i == P.scrollSel);
            if (have) drawScroll(P.scrolls[i], r.x + ss / 2, r.y + ss / 2, ss * 0.8f, i == P.scrollSel ? 1.0f : 0.5f);
        }
    }

    float my = plateTop - 28 * u - 30 * u;
    for (int i = (int)G.msgs.size() - 1; i >= 0; i--)
    {
        Color c = {255, 240, 214, (unsigned char)std::min(255, G.msgs[i].second * 4)};
        textC(G.msgs[i].first, sw / 2.0f, my, 18 * u, c, 1);
        my -= 24 * u;
    }

    for (auto& m : G.mobs)
            if (m.boss && m.aggro)
            {
                float w = 560 * u;
                textC(ENEMIES[m.type].name, sw / 2.0f, 10 * u, 26 * u, {226, 140, 255, 255}, 2);
                bar(sw / 2.0f - w / 2, 44 * u, w, 16 * u, m.hp / m.maxHp, {174, 48, 206, 255}, {30, 10, 40, 230});
            }

    if (G.bannerTimer > 0 && !G.sandbox && !G.sanctuary)
    {
        float a = std::min(1.0f, G.bannerTimer / 60.0f);
        int rg = regionId();
        textC(rg ? regionName(rg) : STAGES[G.stage].name, sw / 2.0f, sh * 0.26f, 60 * u, {240, 210, 140, (unsigned char)(255 * a)}, 2);
        textC(rg ? regionSub(rg) : STAGES[G.stage].subtitle, sw / 2.0f, sh * 0.26f + 70 * u, 24 * u, {214, 200, 180, (unsigned char)(255 * a)});
    }

    if (G.sandbox)
        text(std::string("Brush: ") + sandboxBrushName() + "  size " + std::to_string(G.brushR) + "    CTRL + mouse to paint,  [ ] material,  - = size,  E spawn foe",
             16 * u, sh - 54 * u, 15 * u, INK);
    text("F1  controls", 16 * u, sh - 30 * u, 15 * u, DIM, 1);

    if (G.showHelp)
    {
        static const char* keys[][2] = {
            {"A / D", "Move"}, {"W / Space", "Jump  (W swims)"}, {"Hold toward wall + W", "Climb (uses stamina)"}, {"Space on a wall", "Wall-jump"},
            {"Shift", "Dodge roll"}, {"Left mouse", "Attack / cast"}, {"Hold right mouse", "Grappling hook  (W / S reel)"},
            {"1 - 6 / wheel", "Switch item"}, {"Q", "Drink a flask"}, {"G", "Drop held item"}, {"F", "Interact"},
            {"Tab", "Inventory & scrolls"}, {"R / T", "Read a scroll / choose the next"}, {"Esc", "Pause & settings"}, {"F1", "Toggle this panel"}, {"F11", "Fullscreen"}};
        int n = 15;
        float pw = 540 * u, ph = (n * 30 + 70) * u;
        Rectangle pr = {sw / 2.0f - pw / 2, sh / 2.0f - ph / 2, pw, ph};
        panel(pr);
        textC("Controls", pr.x + pw / 2, pr.y + 12 * u, 26 * u, C_GOLD, 2);
        for (int i = 0; i < n; i++)
        {
            float ly = pr.y + (56 + i * 30) * u;
            keycap(keys[i][0], pr.x + 24 * u, ly, 16 * u);
            text(keys[i][1], pr.x + 260 * u, ly + 2 * u, 17 * u, INK);
        }
    }

    if (G.state == GS_PLAY)
    {
        Vector2 m = GetMousePosition();
        float a = 9 * u, b = 3 * u;
        for (int pass = 0; pass < 2; pass++)
        {
            Color c = pass ? Color{255, 244, 210, 235} : Color{0, 0, 0, 180};
            float t = pass ? 2.0f : 4.0f;
            DrawLineEx({m.x - a, m.y}, {m.x - b, m.y}, t, c);
            DrawLineEx({m.x + b, m.y}, {m.x + a, m.y}, t, c);
            DrawLineEx({m.x, m.y - a}, {m.x, m.y - b}, t, c);
            DrawLineEx({m.x, m.y + b}, {m.x, m.y + a}, t, c);
        }
    }
}

// ---------------------------------------------------------------- inventory (drag and drop)

enum DragKind { DRAG_NONE, DRAG_SPELL, DRAG_ITEM };
static int dragKind = DRAG_NONE;
static int dragItem = -1;
static int dragSrcSlot = -1, dragSrcStaff = -1; // where a dragged spell came from (-1 = bag)

static void returnDraggedSpell()
{
    if (G.heldSpell.id < 0) return;
    Player& P = G.p;
    if (dragSrcStaff >= 0 && dragSrcStaff < (int)P.hotbar.size() && P.hotbar[dragSrcStaff].type == W_STAFF &&
        dragSrcSlot >= 0 && dragSrcSlot < (int)P.hotbar[dragSrcStaff].staff.slots.size() && P.hotbar[dragSrcStaff].staff.slots[dragSrcSlot].id < 0)
        P.hotbar[dragSrcStaff].staff.slots[dragSrcSlot] = G.heldSpell;
    else
        P.bag.push_back(G.heldSpell);
    G.heldSpell = SpellCard{};
}

void cancelInventoryDrag()
{
    returnDraggedSpell();
    dragKind = DRAG_NONE;
    dragItem = -1;
}

void updateDrawInventory()
{
    float u = U();
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    Player& P = G.p;
    DrawRectangle(0, 0, sw, sh, {0, 0, 0, 160});
    Rectangle pn = {sw / 2.0f - 560 * u, sh / 2.0f - 345 * u, 1120 * u, 690 * u};
    norseFrame({pn.x - 26 * u, pn.y - 26 * u, pn.width + 52 * u, pn.height + 52 * u}, u, true, 2, true);
    heading("Inventory", pn.x + 24 * u, pn.y + 8 * u, 30 * u, C_GOLD);
    std::vector<std::pair<std::string, Color>> tip;
    Vector2 mouse = GetMousePosition();
    bool released = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    if (G.invSel >= (int)P.hotbar.size()) G.invSel = std::max(0, (int)P.hotbar.size() - 1);

    // equipment: armour + hotbar
    float ex = pn.x + 24 * u, ey = pn.y + 62 * u;
    heading("Equipment", ex, ey - 2 * u, 18 * u, C_GOLD);
    ey += 28 * u;
    Rectangle ar = {ex, ey, 64 * u, 64 * u};
    slotBox(ar, hovered(ar), false);
    Weapon armourIcon;
    armourIcon.type = W_ARMOUR;
    armourIcon.metal = P.armour < 0 ? M_COPPER : P.armour;
    drawItemIcon(armourIcon, ar.x + 10 * u, ar.y + 10 * u, 44 * u);
    text(P.armour < 0 ? "Padded gambeson" : std::string(METALS[P.armour].name) + " armour", ex + 76 * u, ey + 10 * u, 17 * u, INK, 1);
    text("Blocks " + std::to_string((int)(armourDef(P.armour) * 100)) + "% of damage", ex + 76 * u, ey + 34 * u, 14 * u, DIM);
    if (hovered(ar))
    {
        tip = {{P.armour < 0 ? "Padded gambeson" : std::string(METALS[P.armour].name) + " armour", P.armour < 0 ? INK : METALS[P.armour].color},
               {"Reduces damage by " + std::to_string((int)(armourDef(P.armour) * 100)) + "%", INK}};
        if (P.armour >= 0 && METALS[P.armour].el != EL_PHYS) tip.push_back({std::string("Resists ") + ELEMENT_NAMES[METALS[P.armour].el] + " (80%)", ELEMENT_COLORS[METALS[P.armour].el]});
        tip.push_back({"Forge better armour at a sanctuary anvil.", DIM});
    }
    ey += 82 * u;
    text("Hotbar  -  drag to reorder", ex, ey, 15 * u, DIM, 1);
    ey += 24 * u;
    float hs = 64 * u, hg = 10 * u;
    int hoverItem = -1;
    for (int i = 0; i < 6; i++)
    {
        Rectangle r = {ex + (i % 3) * (hs + hg), ey + (i / 3) * (hs + hg), hs, hs};
        bool hot = hovered(r);
        if (hot) hoverItem = i;
        slotBox(r, hot, i == G.invSel);
        text(std::to_string(i + 1), r.x + 4 * u, r.y + 1 * u, 12 * u, DIM, 1);
        if (i < (int)P.hotbar.size() && !(dragKind == DRAG_ITEM && dragItem == i))
        {
            drawItemIcon(P.hotbar[i], r.x + 10 * u, r.y + 10 * u, hs - 20 * u);
            if (hot && dragKind == DRAG_NONE) tip = weaponTip(P.hotbar[i]);
            if (hot && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && dragKind == DRAG_NONE)
            {
                dragKind = DRAG_ITEM;
                dragItem = i;
                G.invSel = i;
                playSfx(SFX_CLICK, 0.5f);
            }
        }
    }
    Rectangle bin = {ex, ey + 2 * (hs + hg) + 4 * u, 3 * hs + 2 * hg, 40 * u};
    bool binHot = hovered(bin) && dragKind == DRAG_ITEM;
    DrawRectangleRec(bin, binHot ? Color{110, 30, 30, 255} : Color{40, 20, 18, 255});
    DrawRectangleLinesEx(bin, 1.5f, {170, 70, 60, 255});
    textC("Drop here to discard", bin.x + bin.width / 2, bin.y + 10 * u, 16 * u, {240, 170, 170, 255}, 1);

    // the selected weapon's details
    float dx = pn.x + 290 * u, dy = pn.y + 62 * u, dw = 520 * u;
    DrawRectangleRec({dx, dy, dw, 290 * u}, {30, 22, 17, 255});
    DrawRectangleLinesEx({dx, dy, dw, 290 * u}, std::max(1.0f, 2 * u), {12, 8, 6, 255});
    DrawRectangleLinesEx({dx + 2 * u, dy + 2 * u, dw - 4 * u, 286 * u}, std::max(1.0f, u), {104, 78, 48, 255});
    if (!P.hotbar.empty())
    {
        Weapon& w = P.hotbar[G.invSel];
        drawItemIcon(w, dx + 14 * u, dy + 14 * u, 52 * u);
        auto lines = weaponTip(w);
        text(lines[0].first, dx + 80 * u, dy + 14 * u, 22 * u, lines[0].second, 2);
        for (size_t i = 1; i < lines.size(); i++) text(lines[i].first, dx + 80 * u, dy + (22 + 24 * i) * u, 16 * u, lines[i].second);
    }

    // the scroll case: one-shot spells. Click to choose the one R will read; right-click to drop it
    float bx = pn.x + 290 * u, by = pn.y + 410 * u, bis = 78 * u, bgap = 14 * u;
    heading("Scrolls", bx, by - 40 * u, 18 * u, C_GOLD);
    text("One-shot spells: R reads the chosen one, T chooses the next. Found in chests and on the fallen.", bx + 100 * u, by - 36 * u, 14 * u, DIM, 1);
    for (int i = 0; i < SCROLL_CASE; i++)
    {
        Rectangle r = {bx + i * (bis + bgap), by, bis, bis};
        bool have = i < (int)P.scrolls.size(), hot = hovered(r);
        slotBox(r, hot, have && i == P.scrollSel);
        text(std::to_string(i + 1), r.x + 4 * u, r.y + 1 * u, 12 * u, DIM, 1);
        if (!have) continue;
        drawScroll(P.scrolls[i], r.x + bis / 2, r.y + bis / 2, bis * 0.78f, hot || i == P.scrollSel ? 1.0f : 0.55f);
        if (hot)
        {
            const ScrollDef& sd = SCROLLS[P.scrolls[i]];
            tip = {{sd.name, sd.col}, {sd.desc, INK}, {"One use, then it crumbles to ash.", DIM}};
            if (clicked(r)) { P.scrollSel = i; playSfx(SFX_CLICK, 0.5f); }
            if (rclicked(r))
            {
                addPickupScroll(P.m.cx(), P.m.y, P.scrolls[i]);
                message("Dropped " + std::string(sd.name));
                P.scrolls.erase(P.scrolls.begin() + i);
                P.scrollSel = std::max(0, std::min(P.scrollSel, (int)P.scrolls.size() - 1));
                break;
            }
        }
    }
    if (!P.scrolls.empty())
    {
        const ScrollDef& sd = SCROLLS[std::max(0, std::min(P.scrollSel, (int)P.scrolls.size() - 1)) < (int)P.scrolls.size() ? P.scrolls[std::max(0, std::min(P.scrollSel, (int)P.scrolls.size() - 1))] : 0];
        text(sd.name, bx, by + bis + 18 * u, 20 * u, sd.col, 2);
        text(sd.desc, bx, by + bis + 46 * u, 16 * u, INK, 1);
    }
    else text("No scrolls. Find them in chests and on foes, or buy one from the Arcanist before you sail.", bx, by + bis + 18 * u, 16 * u, DIM, 1);

    // materials
    float mx = pn.x + pn.width - 230 * u, my = pn.y + 62 * u;
    heading("Materials", mx, my - 2 * u, 18 * u, C_GOLD);
    my += 30 * u;
    for (int r = 0; r < RES_COUNT; r++)
    {
        Rectangle row = {mx - 6 * u, my - 3 * u, 210 * u, 28 * u};
        if (hovered(row)) DrawRectangleRec(row, {58, 44, 32, 255});
        drawResIcon(r, mx, my, 22 * u);
        text(RES_NAMES[r], mx + 30 * u, my + 1 * u, 16 * u, P.res[r] ? INK : DIM, 1);
        std::string n = std::to_string(P.res[r]);
        text(n, mx + 196 * u - uiTextWidth(n, 16 * u, 1), my + 1 * u, 16 * u, P.res[r] ? C_GOLD : DIM, 1);
        my += 32 * u;
    }
    my += 10 * u;
    pixelIcon(IC_FLASK, 8, mx, my, 22 * u, RED);
    text(std::to_string(P.potions) + " healing flasks", mx + 30 * u, my + 1 * u, 16 * u, INK, 1);

    textC("Drag weapons to reorder the hotbar.  Click a scroll to choose it, right-click to drop it.  Tab or Esc to close.",
          pn.x + pn.width / 2, pn.y + pn.height - 32 * u, 15 * u, DIM);

    // finish drags
    if (dragKind == DRAG_ITEM)
    {
        if (dragItem >= 0 && dragItem < (int)P.hotbar.size()) drawItemIcon(P.hotbar[dragItem], mouse.x - 26 * u, mouse.y - 26 * u, 52 * u);
        if (released)
        {
            if (binHot && P.hotbar.size() > 1)
            {
                addPickupWeapon(P.m.cx(), P.m.y, P.hotbar[dragItem]);
                message("Dropped " + weaponName(P.hotbar[dragItem]));
                P.hotbar.erase(P.hotbar.begin() + dragItem);
                P.sel = std::min(P.sel, (int)P.hotbar.size() - 1);
                G.invSel = std::min(G.invSel, (int)P.hotbar.size() - 1);
            }
            else if (hoverItem >= 0 && hoverItem != dragItem && !P.hotbar.empty())
            {
                int target = std::min(hoverItem, (int)P.hotbar.size() - 1);
                std::swap(P.hotbar[dragItem], P.hotbar[target]);
                if (P.sel == dragItem) P.sel = target;
                else if (P.sel == target) P.sel = dragItem;
                G.invSel = target;
                playSfx(SFX_CLICK, 0.6f, 1.2f);
            }
            dragKind = DRAG_NONE;
            dragItem = -1;
        }
    }
    else
        tooltip(tip);
}

// ---------------------------------------------------------------- anvil

void updateDrawAnvil()
{
    float u = U();
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    Player& P = G.p;
    DrawRectangle(0, 0, sw, sh, {0, 0, 0, 160});
    Rectangle pn = {sw / 2.0f - 580 * u, sh / 2.0f - 330 * u, 1160 * u, 660 * u};
    panel(pn);
    text("The Anvil", pn.x + 24 * u, pn.y + 14 * u, 30 * u, C_GOLD, 2);
    float hx = pn.x + 24 * u, hy = pn.y + 58 * u;
    for (int r = 0; r < RES_COUNT; r++)
    {
        drawResIcon(r, hx, hy, 20 * u);
        std::string n = std::to_string(P.res[r]);
        text(n, hx + 24 * u, hy + 1 * u, 16 * u, P.res[r] ? INK : DIM, 1);
        hx += 34 * u + uiTextWidth(n, 16 * u, 1) + 16 * u;
    }

    std::vector<std::pair<std::string, Color>> tip;
    static const int types[] = {W_DAGGER, W_SWORD, W_AXE, W_SPEAR, W_MACE, W_CROSSBOW, W_ARMOUR};
    float gx = pn.x + 150 * u, gy = pn.y + 118 * u, cw = 108 * u, ch = 50 * u;
    for (int m = 0; m < METAL_COUNT; m++) textC(METALS[m].name, gx + m * cw + cw / 2, gy - 26 * u, 15 * u, METALS[m].color, 1);
    for (int t = 0; t < 7; t++)
    {
        int type = types[t];
        text(WTYPES[type].name, pn.x + 24 * u, gy + t * (ch + 8 * u) + ch / 2 - 10 * u, 18 * u, INK, 1);
        for (int m = 0; m < METAL_COUNT; m++)
        {
            Rectangle r = {gx + m * cw + 3 * u, gy + t * (ch + 8 * u), cw - 6 * u, ch};
            int cost[RES_COUNT];
            recipeCost(type, m, cost);
            bool ok = canAfford(cost);
            slotBox(r, hovered(r), false);
            if (ok) DrawRectangleRoundedLinesEx(r, 0.12f, 4, 2, {140, 220, 120, 255});
            Weapon w;
            w.type = type;
            w.metal = m;
            drawItemIcon(w, r.x + r.width / 2 - 15 * u, r.y + 3 * u, 30 * u);
            float cx = r.x + 6 * u;
            for (int r2 = 0; r2 < RES_COUNT; r2++)
                if (cost[r2])
                {
                    drawResIcon(r2, cx, r.y + ch - 16 * u, 12 * u);
                    std::string n = std::to_string(cost[r2]);
                    text(n, cx + 13 * u, r.y + ch - 17 * u, 12 * u, P.res[r2] >= cost[r2] ? INK : Color{240, 120, 110, 255}, 1);
                    cx += 18 * u + uiTextWidth(n, 12 * u, 1);
                }
            if (!ok) DrawRectangleRounded(r, 0.12f, 4, {0, 0, 0, 90});
            if (hovered(r))
            {
                tip = type == W_ARMOUR ? std::vector<std::pair<std::string, Color>>{{std::string(METALS[m].name) + " Armour", METALS[m].color},
                                                                                     {"Reduces damage by " + std::to_string((int)(METALS[m].armor * 100)) + "%", INK}}
                                       : weaponTip(w);
                if (type == W_ARMOUR && METALS[m].el != EL_PHYS)
                    tip.push_back({std::string("Resists ") + ELEMENT_NAMES[METALS[m].el] + " (80%)", ELEMENT_COLORS[METALS[m].el]});
                std::string c = "Cost: ";
                for (int r2 = 0; r2 < RES_COUNT; r2++)
                    if (cost[r2]) c += std::to_string(cost[r2]) + " " + RES_NAMES[r2] + "   ";
                tip.push_back({c, ok ? Color{140, 220, 120, 255} : Color{240, 120, 110, 255}});
                tip.push_back({ok ? "Click to forge" : "Not enough materials", ok ? C_GOLD : DIM});
            }
            if (clicked(r) && ok)
            {
                if (type == W_ARMOUR)
                {
                    payCost(cost);
                    playSfx(SFX_CRAFT, 0.8f);
                    P.armour = m;
                    message(std::string("You don ") + METALS[m].name + " armour.");
                }
                else if (P.hotbar.size() < 6)
                {
                    payCost(cost);
                    playSfx(SFX_CRAFT, 0.8f);
                    P.hotbar.push_back(w);
                    message("Forged: " + weaponName(w));
                }
                else
                    message("Hotbar full - drop something first (Tab, drag to the bin).");
            }
        }
    }
    Rectangle sr = {gx + 3 * u, gy + 7 * (ch + 8 * u) + 4 * u, 4 * cw, ch * 0.9f};
    int scost[RES_COUNT] = {0, 0, 0, 40, 0, 0, 0, 0, 0};
    bool sok = canAfford(scost);
    slotBox(sr, hovered(sr), false);
    if (sok) DrawRectangleRoundedLinesEx(sr, 0.12f, 4, 2, {140, 220, 120, 255});
    Weapon sw2;
    sw2.type = W_STAFF;
    sw2.staff.gem = SKYBLUE;
    drawItemIcon(sw2, sr.x + 8 * u, sr.y + 6 * u, 34 * u);
    drawResIcon(R_GOLD, sr.x + 52 * u, sr.y + sr.height / 2 - 9 * u, 18 * u);
    text("Bind an Arcane Staff   40 Gold", sr.x + 76 * u, sr.y + sr.height / 2 - 10 * u, 17 * u, sok ? INK : DIM, 1);
    if (hovered(sr)) tip = {{"Arcane Staff", SKYBLUE}, {"A random staff for this depth, with a few spells.", INK}, {"Cost: 40 Gold", sok ? Color{140, 220, 120, 255} : Color{240, 120, 110, 255}}};
    if (clicked(sr) && sok)
    {
        if (P.hotbar.size() < 6)
        {
            payCost(scost);
            playSfx(SFX_CRAFT, 0.8f, 1.3f);
            Weapon w;
            w.type = W_STAFF;
            w.staff = randomStaff(G.stage + 1);
            P.hotbar.push_back(w);
            message("Bound: " + w.staff.name);
        }
        else
            message("Hotbar full - drop something first (Tab, drag to the bin).");
    }
    textC("Blades chip ore loose; bombs and digging bolts tunnel. Walk over loose ore to collect it.   Esc to leave.", pn.x + pn.width / 2, pn.y + pn.height - 32 * u, 15 * u, DIM);
    tooltip(tip);
}

// ---------------------------------------------------------------- shrine

// The shrine's three gifts: one for the weapon in hand, one for you, one for your staff. Taking one
// opens the haven's far gate.
// ---------------------------------------------------------------- village shops

void updateDrawShop()
{
    float u = U();
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    DrawRectangle(0, 0, sw, sh, {0, 0, 0, 160});
    Rectangle pn = {sw / 2.0f - 470 * u, sh / 2.0f - 320 * u, 940 * u, 640 * u};
    panel(pn);
    text(std::string("The ") + SHOP_NAMES[G.shopId % 3], pn.x + 24 * u, pn.y + 14 * u, 30 * u, C_GOLD, 2);
    std::string bank = std::to_string(META.bank) + " coins banked";
    pixelIcon(IC_COIN, 8, pn.x + pn.width - 50 * u - uiTextWidth(bank, 18 * u, 1), pn.y + 22 * u, 22 * u, WHITE);
    text(bank, pn.x + pn.width - 24 * u - uiTextWidth(bank, 18 * u, 1), pn.y + 22 * u, 18 * u, C_GOLD, 1);
    text(G.shopId == 2 ? "Gear bought here is yours for good. Equipped gear joins every run."
                       : "What you buy here goes with you on the next run only. Click a readied item to sell it back.",
         pn.x + 24 * u, pn.y + 56 * u, 15 * u, DIM);

    float y = pn.y + 90 * u, rowH = 66 * u;
    for (int i = 0; i < UNLOCK_COUNT; i++)
    {
        const Unlock& un = UNLOCKS[i];
        if (un.shop != G.shopId) continue;
        Rectangle row = {pn.x + 20 * u, y, pn.width - 40 * u, rowH - 8 * u};
        bool kit = isKitKind(un.kind), ready = META.stocked[i];
        bool owned = META.owned[i] || ready, eq = META.equipped[i] || ready;
        DrawRectangleRounded(row, 0.15f, 4, hovered(row) ? Color{44, 40, 58, 255} : Color{28, 26, 38, 255});
        if (eq) DrawRectangleRoundedLinesEx(row, 0.15f, 4, 2, {140, 220, 120, 255});
        float ix = row.x + 10 * u, iy = row.y + 6 * u, is = rowH - 20 * u;
        switch (un.kind)
        {
        case UK_WEAPON: { Weapon w; w.type = un.a; w.metal = un.b; drawItemIcon(w, ix, iy, is); break; }
        case UK_SCROLL: drawScroll(un.a, ix + is / 2, iy + is / 2, is * 0.94f, 1.0f); break;
        case UK_MAP: pixelIcon(IC_MAPICON, 9, ix, iy, is, WHITE); break;
        case UK_HOOK: pixelIcon(IC_HOOK, 9, ix, iy, is, WHITE); break;
        case UK_ARMOUR: { Weapon w; w.type = W_ARMOUR; w.metal = un.a; drawItemIcon(w, ix, iy, is); break; }
        case UK_FLASK: pixelIcon(IC_FLASK, 8, ix, iy, is, RED); break;
        case UK_WISP: pixelIcon(IC_WISP, 8, ix, iy, is, WHITE); break;
        }
        text(un.name, row.x + rowH + 4 * u, row.y + 8 * u, 19 * u, INK, 1);
        text(un.desc, row.x + rowH + 4 * u, row.y + 32 * u, 15 * u, DIM);
        Rectangle btn = {row.x + row.width - 170 * u, row.y + 10 * u, 158 * u, rowH - 28 * u};
        bool can = !owned && META.bank >= un.price;
        Color bc = owned ? (eq ? Color{60, 110, 60, 255} : Color{60, 56, 80, 255}) : (can ? Color{120, 90, 40, 255} : Color{50, 40, 40, 255});
        DrawRectangleRounded(btn, 0.3f, 4, hovered(btn) ? brighten(bc, 25) : bc);
        std::string label = ready ? (hovered(btn) ? "Sell back" : "Readied") : owned ? (eq ? "Equipped" : "Equip") : "Buy  " + std::to_string(un.price);
        textC(label, btn.x + btn.width / 2, btn.y + btn.height / 2 - 10 * u, 18 * u, owned || can ? INK : DIM, 1);
        if (!owned) pixelIcon(IC_COIN, 8, btn.x + 10 * u, btn.y + btn.height / 2 - 9 * u, 18 * u, WHITE);
        if (clicked(btn))
        {
            if (ready) { sellBack(i); playSfx(SFX_CLICK, 0.6f, 0.8f); message(std::string("Sold back: ") + un.name); }
            else if (!owned)
            {
                if (buyUnlock(i)) { playSfx(SFX_CRAFT, 0.7f, 1.2f); message(std::string(kit ? "Readied for the next run: " : "Unlocked: ") + un.name); }
            }
            else
            {
                toggleEquip(i);
                playSfx(SFX_CLICK, 0.6f);
            }
        }
        y += rowH;
    }

    std::string kit = "Next run: Frying Pan";
    for (int i = 0; i < UNLOCK_COUNT; i++)
        if (META.stocked[i] || (META.owned[i] && META.equipped[i])) kit += std::string(", ") + UNLOCKS[i].name;
    text(kit, pn.x + 24 * u, pn.y + pn.height - 62 * u, 16 * u, INK, 1);
    textC("One weapon and two scrolls per run.   Esc to leave.", pn.x + pn.width / 2, pn.y + pn.height - 32 * u, 15 * u, DIM);
}

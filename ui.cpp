// HUD, inventory (drag and drop), anvil and shrine screens, icons and text helpers.
#include "game.h"
#include "util.h"
#include <cmath>
#include <cstring>
#include <cstdio>
#include <algorithm>
#include <string>

// ---------------------------------------------------------------- fonts & text

static Font fBody{}, fBold{}, fTitle{};
static bool haveFonts = false;

static Font loadFirst(std::initializer_list<const char*> paths)
{
    for (const char* p : paths)
        if (FileExists(p))
        {
            Font f = LoadFontEx(p, 64, nullptr, 0);
            if (f.texture.id)
            {
                SetTextureFilter(f.texture, TEXTURE_FILTER_BILINEAR);
                return f;
            }
        }
    return GetFontDefault();
}

void initUI()
{
    fBody = loadFirst({"C:/Windows/Fonts/segoeui.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"});
    fBold = loadFirst({"C:/Windows/Fonts/segoeuib.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"});
    fTitle = loadFirst({"C:/Windows/Fonts/georgiab.ttf", "C:/Windows/Fonts/timesbd.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSerif-Bold.ttf"});
    haveFonts = true;
}

static const Font& fontOf(int style) { return style == 2 ? fTitle : (style == 1 ? fBold : fBody); }

float uiTextWidth(const std::string& s, float size, int style)
{
    if (!haveFonts) return (float)MeasureText(s.c_str(), (int)size);
    return MeasureTextEx(fontOf(style), s.c_str(), size, size * 0.02f).x;
}

void uiText(const std::string& s, float x, float y, float size, Color c, int style)
{
    if (!haveFonts)
    {
        DrawText(s.c_str(), (int)x + 1, (int)y + 1, (int)size, {0, 0, 0, (unsigned char)(c.a * 0.7f)});
        DrawText(s.c_str(), (int)x, (int)y, (int)size, c);
        return;
    }
    Color sh = {0, 0, 0, (unsigned char)(c.a * 0.75f)};
    float o = std::max(1.0f, size / 16);
    DrawTextEx(fontOf(style), s.c_str(), {std::floor(x + o), std::floor(y + o)}, size, size * 0.02f, sh);
    DrawTextEx(fontOf(style), s.c_str(), {std::floor(x), std::floor(y)}, size, size * 0.02f, c);
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

static void panel(Rectangle r, Color fill = {18, 16, 26, 238})
{
    DrawRectangleRounded(r, 0.04f, 6, fill);
    DrawRectangleRoundedLinesEx(r, 0.04f, 6, std::max(1.0f, 2 * U()), {120, 96, 64, 255});
}

static void slotBox(Rectangle r, bool hot, bool selected)
{
    DrawRectangleRounded(r, 0.12f, 4, selected ? Color{70, 56, 36, 255} : (hot ? Color{44, 40, 58, 255} : Color{30, 28, 40, 255}));
    DrawRectangleRoundedLinesEx(r, 0.12f, 4, selected ? 2.5f : 1.5f, selected ? Color{255, 214, 140, 255} : (hot ? Color{170, 150, 200, 255} : Color{74, 66, 90, 255}));
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

static const char* const* spellIcon(int s, int& n)
{
    static const char* const* list[SPELL_COUNT] = {IC_SPARK, IC_MISSILE, IC_FIRE, IC_ICE, IC_BOLT, IC_ACID, IC_BOMB, IC_DIG, IC_WATER, IC_TRIGGER,
                                                   IC_DMG, IC_SPEED, IC_BOUNCE, IC_HOMING, IC_PIERCE, IC_IGNITE, IC_FROST, IC_EXPLO, IC_DOUBLE, IC_TRIPLE};
    static const int rows[SPELL_COUNT] = {11, 9, 10, 10, 9, 9, 10, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9};
    n = rows[s];
    return list[s];
}

static const char* IC_NUGGET[] = {"..........", "...aaa....", "..aawaa...", ".aawaaaA..", ".aaaaaaAA.", ".AaaaaaAA.", "..AAAAAA..", ".........."};
static const char* IC_CRYSTAL[] = {"....w.....", "...waa....", "..waaaA...", "..waaaA...", ".waaaaAA..", ".waaaaAA..", "..aaaaA...", "...AAA...."};
static const char* IC_HEART[] = {".rr...rr.", "rwrr.rrrr", "rwrrrrrrr", "rrrrrrrrr", ".rrrrrrr.", "..rrrrr..", "...rrr...", "....r...."};
static const char* IC_DROP[] = {"....b....", "...bb....", "...bwb...", "..bwbbb..", "..bbbbb..", ".bbbbbbB.", ".bbbbbbB.", "..bbbBB..", "...BBB..."};
static const char* IC_BOOT[] = {"..ppp....", "..ppp....", "..ppp....", "..ppp....", "..pppp...", "..ppppppp", ".pppppppp", ".kkkkkkkk"};
static const char* IC_SKULL[] = {"..wwwww..", ".wwwwwww.", "wwkkwkkww", "wwkkwkkww", "wwwwwwwww", ".wwwkwww.", "..wkwkw..", "..wwwww.."};
static const char* IC_FLASK[] = {"...nn....", "...ww....", "...ww....", "..wrrw...", ".wrrrrw..", ".rrwrrr..", ".rrrrrr..", "..rrrr..."};
static const char* IC_COIN[] = {"..yyyy..", ".yywwyy.", "yywyyyyo", "yywyyyyo", "yyyyyyyo", "yyyyyyoo", ".yyyyoo.", "..oooo.."};
static const char* IC_HOOK[] = {"....nn...", "...n..n..", "...n..n..", "....nn...", ".....n...", ".....n...", "n....n...", "nn..nn...", ".nnnn...."};
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
    Color bg = {(unsigned char)(d.col.r * 0.22f), (unsigned char)(d.col.g * 0.22f), (unsigned char)(d.col.b * 0.22f), 255};
    Color border = d.type == ST_PROJ ? Color{100, 150, 255, 255} : (d.type == ST_MOD ? Color{110, 220, 120, 255} : Color{240, 200, 80, 255});
    DrawRectangleRounded(r, 0.15f, 4, bg);
    DrawRectangleRoundedLinesEx(r, 0.15f, 4, highlight ? 3.0f : 2.0f, highlight ? Color{255, 240, 180, 255} : border);
    int n;
    const char* const* ic = spellIcon(spell, n);
    pixelIcon(ic, n, x + size * 0.15f, y + size * 0.12f, size * 0.7f, d.col);
    if (uses >= 0)
    {
        std::string s = std::to_string(uses);
        float fs = std::max(11.0f, size * 0.32f);
        float w = uiTextWidth(s, fs, 1) + 6;
        DrawRectangleRounded({x + size - w - 1, y + size - fs - 3, w, fs + 2}, 0.4f, 4, {10, 8, 14, 220});
        text(s, x + size - w + 2, y + size - fs - 2, fs, uses <= 2 ? Color{255, 120, 100, 255} : INK, 1);
    }
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

    if (G.nearInteract >= 0 && G.nearInteract < (int)G.inter.size() && G.state == GS_PLAY)
    {
        const Interact& it = G.inter[G.nearInteract];
        static const char* verbs[] = {"Open chest", "Forge at the anvil", "Pray at the shrine", "Step through the portal", "", "Take the weapon", "Trade", "", "", "Board the longship and set sail"};
        float x = (it.x - G.rcx) * G.scale, y = (it.y - 46 - G.rcy) * G.scale;
        float fs = 18 * u, kw = keycapW("F", fs);
        float total = kw + 8 * u + uiTextWidth(it.type == IT_SHOP ? "Trade with the Weaponsmith" : verbs[it.type], fs, 1);
        DrawRectangleRounded({x - total / 2 - 8 * u, y - 6 * u, total + 16 * u, fs * 1.3f + 12 * u}, 0.3f, 4, {10, 8, 14, 190});
        keycap("F", x - total / 2, y, fs);
        std::string verb = it.type == IT_SHOP ? std::string("Trade with the ") + SHOP_NAMES[it.data % 3] : (it.type == IT_PORTAL && G.inVillage ? "Set out on a new run" : verbs[it.type]);
        text(verb, x - total / 2 + kw + 8 * u, y + 2 * u, fs, C_GOLD, 1);
    }

    // vitals
    float x = 16 * u, y = 14 * u, bw = 250 * u, is = 22 * u;
    panel({x - 8 * u, y - 6 * u, bw + is + 30 * u, 92 * u}, {12, 10, 18, 190});
    pixelIcon(IC_HEART, 8, x, y, is, RED);
    bar(x + is + 8 * u, y + 2 * u, bw, 18 * u, pm.hp / pm.maxHp, {206, 46, 56, 255}, {50, 12, 18, 230});
    text(std::to_string((int)std::max(0.0f, std::ceil(pm.hp))) + " / " + std::to_string((int)pm.maxHp), x + is + 16 * u, y + 2 * u, 15 * u, INK, 1);
    y += 28 * u;
    const Weapon* cur = P.hotbar.empty() ? nullptr : &P.hotbar[P.sel];
    pixelIcon(IC_DROP, 9, x, y, is, BLUE);
    if (cur && cur->type == W_STAFF)
    {
        const Staff& s = cur->staff;
        bar(x + is + 8 * u, y + 2 * u, bw, 16 * u, s.mana / s.manaMax, {64, 116, 236, 255}, {12, 20, 52, 230});
        text(std::to_string((int)s.mana) + " mana", x + is + 16 * u, y + 1 * u, 14 * u, INK, 1);
        if (s.cd > 0) bar(x + is + 8 * u, y + 20 * u, bw, 4 * u, 1 - s.cd / (float)std::max(1, std::max(s.delay, s.recharge) + 20), WHITE, {30, 30, 30, 200});
    }
    else
    {
        bar(x + is + 8 * u, y + 2 * u, bw, 16 * u, 0, BLANK, {24, 24, 34, 200});
        text("hold a staff to use mana", x + is + 16 * u, y + 1 * u, 14 * u, DIM);
    }
    y += 28 * u;
    pixelIcon(IC_BOOT, 8, x, y, is * 0.9f, WHITE);
    bar(x + is + 8 * u, y + 6 * u, bw, 8 * u, P.stamina / 100, {232, 202, 74, 255}, {44, 38, 12, 220});

    float cx = x, cy = y + 30 * u;
    auto chip = [&](const char* label, Color c) {
        float w = uiTextWidth(label, 14 * u, 1) + 14 * u;
        DrawRectangleRounded({cx, cy, w, 22 * u}, 0.5f, 4, {c.r, c.g, c.b, 70});
        DrawRectangleRoundedLinesEx({cx, cy, w, 22 * u}, 0.5f, 4, 1.5f, c);
        text(label, cx + 7 * u, cy + 3 * u, 14 * u, INK, 1);
        cx += w + 6 * u;
    };
    if (pm.burn > 0) chip("Burning", ELEMENT_COLORS[EL_FIRE]);
    if (pm.poison > 0) chip("Poisoned", ELEMENT_COLORS[EL_POISON]);
    if (pm.chill > 0) chip("Chilled", ELEMENT_COLORS[EL_ICE]);
    if (pm.shock > 0) chip("Shocked", ELEMENT_COLORS[EL_SHOCK]);

    // stage info, kills, flasks, armour, materials (right)
    float rx = sw - 16 * u;
    std::string title = G.sandbox ? "Sandbox" : (G.inVillage ? "Hearthwick" : (G.sanctuary ? "Sanctuary" : inDunes() ? "The Whispering Dunes" : STAGES[G.stage].name));
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
    rightStat(IC_SHIELD, 8, P.armour < 0 ? Color{168, 140, 104, 255} : METALS[P.armour].color,
              std::string(P.armour < 0 ? "Gambeson" : METALS[P.armour].name) + "  " + std::to_string((int)(armourDef(P.armour) * 100)) + "%");
    ry += 4 * u;
    for (int r = 0; r < RES_COUNT; r++)
    {
        if (!P.res[r]) continue;
        std::string s = std::to_string(P.res[r]) + " " + RES_NAMES[r];
        float w = uiTextWidth(s, 15 * u, 1);
        drawResIcon(r, rx - w - 26 * u, ry, 18 * u);
        text(s, rx - w, ry + 1 * u, 15 * u, INK, 1);
        ry += 21 * u;
    }

    // hotbar
    float slot = 58 * u, gap = 8 * u;
    float hx = sw / 2.0f - (6 * slot + 5 * gap) / 2, hy = sh - slot - 16 * u;
    panel({hx - 10 * u, hy - 8 * u, 6 * slot + 5 * gap + 20 * u, slot + 16 * u}, {12, 10, 18, 170});
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
        textC(weaponName(*cur), sw / 2.0f, hy - 34 * u, 19 * u, {244, 226, 190, 255}, 1);
        if (cur->type == W_STAFF)
        {
            const Staff& s = cur->staff;
            float is2 = 26 * u, ix = sw / 2.0f - s.slots.size() * (is2 + 4 * u) / 2;
            for (size_t k = 0; k < s.slots.size(); k++)
                drawSpellIcon(s.slots[k].id, ix + k * (is2 + 4 * u), hy - 66 * u, is2, (int)k == s.cursor, s.slots[k].uses);
        }
    }

    float my = hy - (cur && cur->type == W_STAFF ? 100 : 64) * u;
    for (int i = (int)G.msgs.size() - 1; i >= 0; i--)
    {
        Color c = {255, 240, 214, (unsigned char)std::min(255, G.msgs[i].second * 4)};
        textC(G.msgs[i].first, sw / 2.0f, my, 18 * u, c, 1);
        my -= 24 * u;
    }

    if (G.bossId)
        for (auto& m : G.mobs)
            if (m.id == G.bossId && m.aggro)
            {
                float w = 560 * u;
                textC(ENEMIES[m.type].name, sw / 2.0f, 10 * u, 26 * u, {226, 140, 255, 255}, 2);
                bar(sw / 2.0f - w / 2, 44 * u, w, 16 * u, m.hp / m.maxHp, {174, 48, 206, 255}, {30, 10, 40, 230});
            }

    if (G.bannerTimer > 0 && !G.sandbox && !G.sanctuary)
    {
        float a = std::min(1.0f, G.bannerTimer / 60.0f);
        bool dunes = inDunes();
        textC(dunes ? "The Whispering Dunes" : STAGES[G.stage].name, sw / 2.0f, sh * 0.26f, 60 * u, {240, 210, 140, (unsigned char)(255 * a)}, 2);
        textC(dunes ? "Only the wind lives here" : STAGES[G.stage].subtitle, sw / 2.0f, sh * 0.26f + 70 * u, 24 * u, {214, 200, 180, (unsigned char)(255 * a)});
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
            {"Tab", "Inventory & staff editing"}, {"Esc", "Pause & settings"}, {"F1", "Toggle this panel"}};
        int n = 14;
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
    panel(pn);
    text("Inventory", pn.x + 24 * u, pn.y + 14 * u, 30 * u, C_GOLD, 2);
    std::vector<std::pair<std::string, Color>> tip;
    Vector2 mouse = GetMousePosition();
    bool released = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    if (G.invSel >= (int)P.hotbar.size()) G.invSel = std::max(0, (int)P.hotbar.size() - 1);

    // equipment: armour + hotbar
    float ex = pn.x + 24 * u, ey = pn.y + 62 * u;
    text("Equipment", ex, ey, 18 * u, C_GOLD, 1);
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
    DrawRectangleRounded(bin, 0.3f, 4, binHot ? Color{110, 30, 30, 255} : Color{40, 22, 26, 255});
    DrawRectangleRoundedLinesEx(bin, 0.3f, 4, 1.5f, {170, 70, 70, 255});
    textC("Drop here to discard", bin.x + bin.width / 2, bin.y + 10 * u, 16 * u, {240, 170, 170, 255}, 1);

    // details + staff editor
    float dx = pn.x + 290 * u, dy = pn.y + 62 * u, dw = 520 * u;
    DrawRectangleRounded({dx, dy, dw, 290 * u}, 0.04f, 4, {26, 24, 36, 255});
    int staffIdx = -1;
    if (!P.hotbar.empty())
    {
        Weapon& w = P.hotbar[G.invSel];
        drawItemIcon(w, dx + 14 * u, dy + 14 * u, 52 * u);
        auto lines = weaponTip(w);
        text(lines[0].first, dx + 80 * u, dy + 14 * u, 22 * u, lines[0].second, 2);
        for (size_t i = 1; i < lines.size(); i++) text(lines[i].first, dx + 80 * u, dy + (22 + 24 * i) * u, 16 * u, lines[i].second);
        if (w.type == W_STAFF)
        {
            staffIdx = G.invSel;
            Staff& s = w.staff;
            float sy = dy + 130 * u;
            text("Spell slots  -  cast left to right", dx + 14 * u, sy, 15 * u, DIM, 1);
            sy += 24 * u;
            float is = 52 * u;
            for (size_t k = 0; k < s.slots.size(); k++)
            {
                float sx = dx + 14 * u + (k % 8) * (is + 8 * u), syy = sy + (k / 8) * (is + 8 * u);
                Rectangle r = {sx, syy, is, is};
                bool hot = hovered(r);
                drawSpellIcon(s.slots[k].id, sx, syy, is, hot && dragKind == DRAG_SPELL, s.slots[k].uses);
                if (hot && s.slots[k].id >= 0 && dragKind == DRAG_NONE) tip = spellTip(s.slots[k].id, s.slots[k].uses);
                if (hot && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && dragKind == DRAG_NONE && s.slots[k].id >= 0)
                {
                    dragKind = DRAG_SPELL;
                    G.heldSpell = s.slots[k];
                    s.slots[k] = SpellCard{};
                    dragSrcStaff = staffIdx;
                    dragSrcSlot = (int)k;
                    s.cursor = 0;
                    playSfx(SFX_CLICK, 0.5f);
                }
                else if (hot && released && dragKind == DRAG_SPELL)
                {
                    std::swap(G.heldSpell, s.slots[k]); // drop, swapping with whatever was there
                    if (G.heldSpell.id >= 0) returnDraggedSpell();
                    dragKind = DRAG_NONE;
                    s.cursor = 0;
                    playSfx(SFX_CLICK, 0.6f, 1.2f);
                }
                if (rclicked(r) && s.slots[k].id >= 0 && dragKind == DRAG_NONE)
                {
                    P.bag.push_back(s.slots[k]);
                    s.slots[k] = SpellCard{};
                    s.cursor = 0;
                    playSfx(SFX_CLICK, 0.5f);
                }
            }
        }
        else
            text("Select a staff on the left to edit its spells.", dx + 14 * u, dy + (40 + 24 * lines.size()) * u, 15 * u, DIM);
    }

    // spell bag
    float bx = pn.x + 290 * u, by = pn.y + 386 * u, bis = 50 * u, bgap = 6 * u;
    int cols = 10, rows = 4;
    text("Spell bag", bx, by - 30 * u, 18 * u, C_GOLD, 1);
    Rectangle sortBtn = {bx + cols * (bis + bgap) - 110 * u, by - 34 * u, 104 * u, 28 * u};
    DrawRectangleRounded(sortBtn, 0.3f, 4, hovered(sortBtn) ? Color{70, 60, 90, 255} : Color{44, 40, 58, 255});
    textC("Sort", sortBtn.x + sortBtn.width / 2, sortBtn.y + 5 * u, 16 * u, INK, 1);
    if (clicked(sortBtn) && dragKind == DRAG_NONE)
    {
        std::sort(P.bag.begin(), P.bag.end(), [](const SpellCard& a, const SpellCard& b) { return a.id != b.id ? a.id < b.id : a.uses > b.uses; });
        playSfx(SFX_CLICK, 0.5f);
    }
    Rectangle bagArea = {bx - 6 * u, by - 6 * u, cols * (bis + bgap) + 6 * u, rows * (bis + bgap) + 6 * u};
    DrawRectangleRounded(bagArea, 0.03f, 4, {22, 20, 30, 255});
    if (hovered(bagArea) && dragKind == DRAG_SPELL) DrawRectangleRoundedLinesEx(bagArea, 0.03f, 4, 2, {170, 150, 220, 255});
    for (int i = 0; i < cols * rows; i++)
    {
        float sx = bx + (i % cols) * (bis + bgap), sy = by + (i / cols) * (bis + bgap);
        Rectangle r = {sx, sy, bis, bis};
        bool hot = hovered(r);
        if (i < (int)P.bag.size())
        {
            drawSpellIcon(P.bag[i].id, sx, sy, bis, hot, P.bag[i].uses);
            if (hot && dragKind == DRAG_NONE) tip = spellTip(P.bag[i].id, P.bag[i].uses);
            if (hot && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && dragKind == DRAG_NONE)
            {
                dragKind = DRAG_SPELL;
                G.heldSpell = P.bag[i];
                P.bag.erase(P.bag.begin() + i);
                dragSrcStaff = dragSrcSlot = -1;
                playSfx(SFX_CLICK, 0.5f);
                break;
            }
            if (rclicked(r) && staffIdx >= 0 && dragKind == DRAG_NONE) // quick-equip into the first free slot
            {
                Staff& s = P.hotbar[staffIdx].staff;
                for (auto& sl : s.slots)
                    if (sl.id < 0)
                    {
                        sl = P.bag[i];
                        P.bag.erase(P.bag.begin() + i);
                        s.cursor = 0;
                        playSfx(SFX_CLICK, 0.6f, 1.2f);
                        break;
                    }
                break;
            }
        }
        else
            drawSpellIcon(-1, sx, sy, bis, false);
    }
    if (P.bag.size() > (size_t)(cols * rows)) text("+" + std::to_string(P.bag.size() - cols * rows) + " more - sort or equip to see them", bx, by + rows * (bis + bgap) + 4 * u, 14 * u, DIM);

    // materials
    float mx = pn.x + pn.width - 230 * u, my = pn.y + 62 * u;
    text("Materials", mx, my, 18 * u, C_GOLD, 1);
    my += 30 * u;
    for (int r = 0; r < RES_COUNT; r++)
    {
        Rectangle row = {mx - 6 * u, my - 3 * u, 210 * u, 28 * u};
        if (hovered(row)) DrawRectangleRounded(row, 0.3f, 4, {40, 36, 52, 255});
        drawResIcon(r, mx, my, 22 * u);
        text(RES_NAMES[r], mx + 30 * u, my + 1 * u, 16 * u, P.res[r] ? INK : DIM, 1);
        std::string n = std::to_string(P.res[r]);
        text(n, mx + 196 * u - uiTextWidth(n, 16 * u, 1), my + 1 * u, 16 * u, P.res[r] ? C_GOLD : DIM, 1);
        my += 32 * u;
    }
    my += 10 * u;
    pixelIcon(IC_FLASK, 8, mx, my, 22 * u, RED);
    text(std::to_string(P.potions) + " healing flasks", mx + 30 * u, my + 1 * u, 16 * u, INK, 1);

    textC("Drag spells between the bag and staff slots.  Right-click: quick equip / unequip.  Tab or Esc to close.",
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
    else if (dragKind == DRAG_SPELL)
    {
        drawSpellIcon(G.heldSpell.id, mouse.x - 24 * u, mouse.y - 24 * u, 48 * u, true, G.heldSpell.uses);
        if (released && G.heldSpell.id >= 0)
        {
            if (hovered(bagArea)) { P.bag.push_back(G.heldSpell); G.heldSpell = SpellCard{}; }
            else returnDraggedSpell();
            dragKind = DRAG_NONE;
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

void updateDrawShrine()
{
    float u = U();
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    DrawRectangle(0, 0, sw, sh, {0, 0, 0, 160});
    textC("The shrine offers a single gift", sw / 2.0f, sh / 2.0f - 220 * u, 32 * u, C_GOLD, 2);
    float cw = 270 * u, chh = 320 * u, gap = 30 * u;
    float x0 = sw / 2.0f - (3 * cw + 2 * gap) / 2;
    for (int i = 0; i < 3; i++)
    {
        int s = G.shrineChoice[i];
        const SpellDef& d = SPELLS[s];
        Rectangle r = {x0 + i * (cw + gap), sh / 2.0f - 160 * u, cw, chh};
        panel(r);
        if (hovered(r)) DrawRectangleRoundedLinesEx(r, 0.04f, 6, 3, d.col);
        drawSpellIcon(s, r.x + cw / 2 - 44 * u, r.y + 24 * u, 88 * u, false, makeCard(s).uses);
        textC(d.name, r.x + cw / 2, r.y + 126 * u, 22 * u, d.col, 2);
        std::string desc = d.desc, line;
        float ty = r.y + 164 * u;
        size_t pos = 0;
        while (pos < desc.size())
        {
            size_t sp = desc.find(' ', pos);
            std::string word = desc.substr(pos, sp == std::string::npos ? std::string::npos : sp - pos);
            std::string trial = line.empty() ? word : line + " " + word;
            if (uiTextWidth(trial, 16 * u, 0) > cw - 28 * u)
            {
                textC(line, r.x + cw / 2, ty, 16 * u, INK);
                ty += 21 * u;
                line = word;
            }
            else
                line = trial;
            if (sp == std::string::npos) break;
            pos = sp + 1;
        }
        if (!line.empty()) textC(line, r.x + cw / 2, ty, 16 * u, INK);
        textC("Mana " + std::to_string(d.mana) + (d.uses ? "    " + std::to_string(d.uses) + " charges" : ""), r.x + cw / 2, r.y + chh - 40 * u, 16 * u, {120, 170, 255, 255}, 1);
        if (clicked(r))
        {
            G.p.bag.push_back(makeCard(s));
            playSfx(SFX_PICKUP, 0.8f, 0.7f);
            for (auto& it : G.inter)
                if (it.type == IT_SHRINE) it.used = true;
            message(std::string("The shrine grants you ") + d.name + ". (Tab to equip)");
            G.state = GS_PLAY;
        }
    }
    textC("Esc to decide later", sw / 2.0f, sh / 2.0f + 180 * u, 16 * u, DIM);
}

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
    text("Buy once to unlock forever. Equipped items join every new run.", pn.x + 24 * u, pn.y + 56 * u, 15 * u, DIM);

    float y = pn.y + 90 * u, rowH = 66 * u;
    for (int i = 0; i < UNLOCK_COUNT; i++)
    {
        const Unlock& un = UNLOCKS[i];
        if (un.shop != G.shopId) continue;
        Rectangle row = {pn.x + 20 * u, y, pn.width - 40 * u, rowH - 8 * u};
        bool owned = META.owned[i], eq = META.equipped[i];
        DrawRectangleRounded(row, 0.15f, 4, hovered(row) ? Color{44, 40, 58, 255} : Color{28, 26, 38, 255});
        if (eq) DrawRectangleRoundedLinesEx(row, 0.15f, 4, 2, {140, 220, 120, 255});
        float ix = row.x + 10 * u, iy = row.y + 6 * u, is = rowH - 20 * u;
        switch (un.kind)
        {
        case UK_WEAPON: { Weapon w; w.type = un.a; w.metal = un.b; drawItemIcon(w, ix, iy, is); break; }
        case UK_STAFF: { Weapon w; w.type = W_STAFF; w.staff.gem = SKYBLUE; drawItemIcon(w, ix, iy, is); break; }
        case UK_SPELL: drawSpellIcon(un.a, ix, iy, is, false, makeCard(un.a).uses); break;
        case UK_HOOK: pixelIcon(IC_HOOK, 9, ix, iy, is, WHITE); break;
        case UK_ARMOUR: { Weapon w; w.type = W_ARMOUR; w.metal = un.a; drawItemIcon(w, ix, iy, is); break; }
        case UK_FLASK: pixelIcon(IC_FLASK, 8, ix, iy, is, RED); break;
        }
        text(un.name, row.x + rowH + 4 * u, row.y + 8 * u, 19 * u, INK, 1);
        text(un.desc, row.x + rowH + 4 * u, row.y + 32 * u, 15 * u, DIM);
        Rectangle btn = {row.x + row.width - 170 * u, row.y + 10 * u, 158 * u, rowH - 28 * u};
        bool can = !owned && META.bank >= un.price;
        Color bc = owned ? (eq ? Color{60, 110, 60, 255} : Color{60, 56, 80, 255}) : (can ? Color{120, 90, 40, 255} : Color{50, 40, 40, 255});
        DrawRectangleRounded(btn, 0.3f, 4, hovered(btn) ? brighten(bc, 25) : bc);
        std::string label = owned ? (eq ? "Equipped" : "Equip") : "Buy  " + std::to_string(un.price);
        textC(label, btn.x + btn.width / 2, btn.y + btn.height / 2 - 10 * u, 18 * u, owned || can ? INK : DIM, 1);
        if (!owned) pixelIcon(IC_COIN, 8, btn.x + 10 * u, btn.y + btn.height / 2 - 9 * u, 18 * u, WHITE);
        if (clicked(btn))
        {
            if (!owned)
            {
                if (buyUnlock(i)) { playSfx(SFX_CRAFT, 0.7f, 1.2f); message(std::string("Unlocked: ") + un.name); }
                else message("Not enough coins - delve deeper and bring more back.");
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
        if (META.owned[i] && META.equipped[i]) kit += std::string(", ") + UNLOCKS[i].name;
    text(kit, pn.x + 24 * u, pn.y + pn.height - 62 * u, 16 * u, INK, 1);
    textC("One weapon, one staff and two spells may be equipped.   Esc to leave.", pn.x + pn.width / 2, pn.y + pn.height - 32 * u, 15 * u, DIM);
}

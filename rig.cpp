// Character art and animation:
//  - the player is a jointed rig drawn into a small pixel canvas
//  - creature sprites are hand-shaded with no outline (bosses are drawn at 2x)
//  - ragdolls: verlet stick figures that tumble through the falling-sand world
#include "game.h"
#include "sprites.h"
#include "util.h"
#include <cmath>
#include <algorithm>
#include <map>
#include <tuple>
#include <cstring>

static Color mul(Color c, float k)
{
    return {(unsigned char)std::min(255.0f, c.r * k), (unsigned char)std::min(255.0f, c.g * k), (unsigned char)std::min(255.0f, c.b * k), c.a};
}
static Vector2 add(Vector2 a, Vector2 b) { return {a.x + b.x, a.y + b.y}; }
static const Color OUTLINE = {24, 18, 28, 255};

// ---------------------------------------------------------------- canvas
// The rig draws into a small pixel canvas so the whole silhouette can be outlined at once.

static const int CW = 72, CH = 72;
static Color canvas[CW * CH];
static int cox = 0, coy = 0; // canvas origin in render-texture pixels
static float rotA = 0, rotCx = 0, rotCy = 0;

static void px(float x, float y, Color c)
{
    if (rotA != 0)
    {
        float dx = x - rotCx, dy = y - rotCy, cs = std::cos(rotA), sn = std::sin(rotA);
        x = rotCx + dx * cs - dy * sn;
        y = rotCy + dx * sn + dy * cs;
    }
    int ix = (int)std::floor(x) - cox, iy = (int)std::floor(y) - coy;
    if (ix < 0 || iy < 0 || ix >= CW || iy >= CH) return;
    canvas[iy * CW + ix] = c;
}

static void seg(Vector2 a, Vector2 b, Color c)
{
    int n = std::max(1, (int)std::ceil(std::max(std::fabs(b.x - a.x), std::fabs(b.y - a.y)) * 1.5f));
    for (int i = 0; i <= n; i++)
    {
        float t = (float)i / n;
        px(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, c);
    }
}

static void thick(Vector2 a, Vector2 b, int w, Color c, float f)
{
    for (int k = 0; k < w; k++)
    {
        float o = (k - (w - 1) / 2.0f) * f;
        seg({a.x + o, a.y}, {b.x + o, b.y}, k == 0 && w > 1 ? mul(c, 0.8f) : c);
    }
}

static void canvasBegin(int ox, int oy)
{
    std::memset(canvas, 0, sizeof(canvas));
    cox = ox;
    coy = oy;
}

static void canvasEnd(bool flash)
{
    for (int y = 0; y < CH; y++)
        for (int x = 0; x < CW; x++)
        {
            Color c = canvas[y * CW + x];
            if (!c.a) continue;
            if (flash) c = WHITE;
            DrawRectangle(cox + x, coy + y, 1, 1, c);
        }
}

// ---------------------------------------------------------------- player heads (6 wide, facing right)
// h/H hair & beard, s/S skin, u eye, A/D/L armour, c/C mail, v eye slot, g glow, n horn

static const char* HEAD_BARE[] = { // braided hair and a full beard
    ".hHHh.",
    "hHhhHh",
    "hhssus",
    "hhSsss",
    "hhSSs.",
    "h.hHh.",
    "..hh..",
};
static const char* HEAD_SPANGEN[] = { // riveted cap with a nasal guard
    "..LL..",
    ".LAAD.",
    "LAAAAD",
    "DDDDDA",
    "hhssuA",
    "hhSssS",
    "h.hHh.",
    "..hh..",
};
static const char* HEAD_GJERMUNDBU[] = { // rounded helm with spectacle guards
    "..LL..",
    ".LAAD.",
    "LAAAAD",
    "DDDDDD",
    "hhsAvA",
    "hhSsAS",
    "h.hHh.",
    "..hh..",
};
static const char* HEAD_RUNE[] = { // closed helm, mail aventail, eyes burning with the armour's element
    "..LL..",
    ".LAAD.",
    "LAAAAD",
    "DDDDDD",
    "DAAggA",
    "DAAAAD",
    "cCcCc.",
    ".CcC..",
};
static const char* HEAD_HORNED[] = { // the adamantium war-helm
    "n....n",
    "nLLLLn",
    "LAAAAD",
    "DDDDDD",
    "DAAggA",
    "DAAAAD",
    "cCcCc.",
    ".CcC..",
};

// ---------------------------------------------------------------- player rig

// angles are measured from straight down, positive = towards facing
struct Pose { float fThigh, fShin, bThigh, bShin, lean, backArm, frontArm, bob; };

static void simulateCape(Player& P, Vector2 anchor, float f)
{
    Mob& m = P.m;
    if (!P.capeInit || std::hypot(P.cape[0].x - anchor.x, P.cape[0].y - anchor.y) > 30)
    {
        for (int i = 0; i < 7; i++) P.cape[i] = P.capePrev[i] = {anchor.x - f * i * 1.6f, anchor.y + i * 0.6f};
        P.capeInit = true;
    }
    P.cape[0] = P.capePrev[0] = anchor;
    float grav = m.inLiquid ? 0.03f : 0.16f;
    for (int i = 1; i < 7; i++)
    {
        Vector2 v = {(P.cape[i].x - P.capePrev[i].x) * 0.86f, (P.cape[i].y - P.capePrev[i].y) * 0.86f};
        P.capePrev[i] = P.cape[i];
        P.cape[i].x += v.x - m.vx * 0.025f + std::sin(G.frame * 0.1f + i) * 0.04f;
        P.cape[i].y += v.y + grav;
    }
    for (int it = 0; it < 2; it++)
        for (int i = 1; i < 7; i++)
        {
            float dx = P.cape[i].x - P.cape[i - 1].x, dy = P.cape[i].y - P.cape[i - 1].y;
            float d = std::sqrt(dx * dx + dy * dy);
            if (d > 0.01f)
            {
                P.cape[i].x = P.cape[i - 1].x + dx / d * 1.7f;
                P.cape[i].y = P.cape[i - 1].y + dy / d * 1.7f;
            }
        }
}

// Five armour sets, each visible on the body: wool tunic, leather jerkin, mail byrnie, rune-etched lamellar, adamantium scale.
enum ArmourSet { AS_WOOL, AS_LEATHER, AS_MAIL, AS_LAMELLAR, AS_SCALE };
struct Look
{
    int set;
    Color tunic, tunicD, tunicL, trim, pants, wrap, boot, bracer, cloak, fur, skin, skinD, hairD, hairL, A, Ad, Ah, glow;
    bool metal;
    const char* const* head;
    int headRows;
};

static Look playerLook()
{
    Player& P = G.p;
    Look k;
    k.metal = P.armour >= 0;
    k.A = k.metal ? METALS[P.armour].color : Color{150, 150, 156, 255};
    k.Ad = mul(k.A, 0.6f);
    k.Ah = brighten(k.A, 50);
    Element el = k.metal ? METALS[P.armour].el : EL_PHYS;
    k.glow = el != EL_PHYS ? ELEMENT_COLORS[el] : Color{120, 255, 230, 255};
    k.skin = {236, 190, 150, 255};
    k.skinD = {196, 136, 102, 255};
    k.hairD = {116, 70, 40, 255}; // auburn
    k.hairL = {164, 104, 56, 255};
    k.pants = {86, 72, 60, 255};
    k.wrap = {150, 132, 104, 255};
    k.boot = {96, 62, 40, 255};
    k.bracer = {110, 74, 46, 255};
    k.fur = {150, 132, 110, 255};
    if (!k.metal) k.set = AS_WOOL;
    else if (P.armour <= M_IRON) k.set = AS_LEATHER;
    else if (P.armour == M_ADAMANTIUM) k.set = AS_SCALE;
    else if (el != EL_PHYS) k.set = AS_LAMELLAR;
    else k.set = AS_MAIL;
    switch (k.set)
    {
    case AS_WOOL:
        k.tunic = {84, 98, 64, 255}; k.trim = {184, 146, 70, 255}; k.cloak = {132, 38, 36, 255};
        k.head = HEAD_BARE; k.headRows = 7;
        break;
    case AS_LEATHER:
        k.tunic = {128, 84, 50, 255}; k.trim = k.A; k.cloak = {44, 60, 96, 255};
        k.head = HEAD_SPANGEN; k.headRows = 8;
        break;
    case AS_MAIL:
        k.tunic = k.A; k.trim = k.Ad; k.cloak = {70, 66, 64, 255}; k.bracer = k.Ad;
        k.head = HEAD_GJERMUNDBU; k.headRows = 8;
        break;
    case AS_LAMELLAR:
        k.tunic = k.A; k.trim = k.glow; k.cloak = mul(k.glow, 0.42f); k.bracer = k.Ad; k.fur = {70, 64, 60, 255};
        k.head = HEAD_RUNE; k.headRows = 8;
        break;
    default:
        k.tunic = k.A; k.trim = {214, 180, 80, 255}; k.cloak = {32, 30, 38, 255}; k.bracer = k.A; k.fur = {60, 56, 54, 255};
        k.pants = {48, 44, 50, 255}; k.wrap = k.Ad;
        k.head = HEAD_HORNED; k.headRows = 8;
        break;
    }
    k.tunicD = mul(k.tunic, 0.66f);
    k.tunicL = brighten(k.tunic, 34);
    return k;
}

// Body pixel colour for the armour set at torso row r (0 = shoulders) and column c (front is +).
static Color bodyColor(const Look& k, int r, int c, int half)
{
    bool front = c == half, back = c == -half;
    Color base = k.tunic;
    switch (k.set)
    {
    case AS_WOOL: base = (r + c * 3) % 7 == 0 ? mul(k.tunic, 0.9f) : k.tunic; break; // a coarse weave
    case AS_LEATHER: base = (r % 3 == 1 && (c + 8) % 2 == 0 && !front && !back) ? mul(k.A, 0.85f) : ((r + c * 2) % 5 == 0 ? mul(k.tunic, 0.9f) : k.tunic); break; // riveted hide
    case AS_MAIL: base = (r + c + 16) % 2 ? k.A : mul(k.A, 0.78f); break;
    case AS_LAMELLAR: base = r % 2 ? k.Ad : (((c + 9 + r / 2) % 3) ? k.A : mul(k.A, 0.82f)); break;
    default: base = ((c + 9 + (r / 2) % 2) % 2 == 0 && r % 2) ? k.Ad : k.A; break; // overlapping scales
    }
    if (front) return brighten(base, 30);
    if (back) return mul(base, 0.68f);
    return base;
}

// The weapon carried on the back while out of combat.
static void drawStowed(const Weapon& w, Vector2 shoulder, Vector2 hip, float f)
{
    Color brown = {110, 70, 40, 255};
    if (isMelee(w.type))
    {
        Color mc = METALS[w.metal].color;
        Vector2 hilt = {shoulder.x - f * 3, shoulder.y - 3};
        Vector2 dir = {hip.x + f * 3 - hilt.x, hip.y + 2 - hilt.y};
        float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
        dir = {dir.x / len, dir.y / len};
        float L = w.type == W_DAGGER ? 6.0f : (w.type == W_SPEAR ? 18.0f : (w.type == W_PAN ? 4.0f : 11.0f));
        if (w.type == W_PAN) // pan slung on the back
        {
            Vector2 c = {shoulder.x - f * 3, shoulder.y + 3};
            for (int dy = -3; dy <= 3; dy++)
                for (int dx = -3; dx <= 3; dx++)
                    if (dx * dx + dy * dy <= 9) px(c.x + dx, c.y + dy, dx * dx + dy * dy >= 6 ? Color{60, 60, 66, 255} : Color{90, 90, 98, 255});
            seg({c.x, c.y - 3}, {c.x + f, c.y - 8}, brown);
            return;
        }
        if (w.type == W_SPEAR)
        {
            seg({hilt.x - dir.x * 4, hilt.y - dir.y * 4}, {hilt.x + dir.x * L, hilt.y + dir.y * L}, brown);
            seg({hilt.x - dir.x * 4, hilt.y - dir.y * 4}, {hilt.x - dir.x * 7, hilt.y - dir.y * 7}, mc);
            return;
        }
        seg({hilt.x - dir.x * 3, hilt.y - dir.y * 3}, hilt, brown);
        px(hilt.x - dir.x * 3.5f, hilt.y - dir.y * 3.5f, {200, 170, 60, 255});
        seg({hilt.x - dir.y * 2, hilt.y + dir.x * 2}, {hilt.x + dir.y * 2, hilt.y - dir.x * 2}, {200, 170, 60, 255});
        seg(hilt, {hilt.x + dir.x * L, hilt.y + dir.y * L}, mc);
        seg({hilt.x + 0.7f, hilt.y}, {hilt.x + dir.x * L + 0.7f, hilt.y + dir.y * L}, mul(mc, 0.75f));
        if (w.type == W_AXE || w.type == W_MACE)
            for (int k = -2; k <= 2; k++) px(hilt.x + dir.x * L + dir.y * k, hilt.y + dir.y * L - dir.x * k, mc);
    }
    else if (w.type == W_CROSSBOW)
    {
        Vector2 a = {shoulder.x - f * 5, shoulder.y}, b = {shoulder.x + f * 2, shoulder.y + 4};
        seg(a, b, brown);
        seg({a.x, a.y - 3}, {a.x - f, a.y + 3}, METALS[w.metal].color);
    }
    else if (w.type == W_STAFF)
    {
        Vector2 top = {shoulder.x - f * 5, shoulder.y - 8};
        seg({hip.x + f * 2, hip.y + 3}, top, brown);
        px(top.x, top.y - 1, w.staff.gem);
        px(top.x - 1, top.y, w.staff.gem);
        px(top.x + 1, top.y, w.staff.gem);
        px(top.x, top.y, WHITE);
    }
}

void drawPlayerRig(int camX, int camY)
{
    Player& P = G.p;
    Mob& m = P.m;
    if (!m.alive) return;

    float f = (float)(P.onWall ? P.onWall : m.facing);
    bool air = !m.onGround;
    float t = (float)G.frame;
    bool hasWeapon = !P.hotbar.empty();
    const Weapon* wpn = hasWeapon ? &P.hotbar[P.sel] : nullptr;
    bool drawn = hasWeapon && (P.combatT > 0 || P.swingT > 0);

    Pose ps;
    if (P.rollT > 0) ps = {1.6f, -1.5f, 1.3f, -1.2f, 0.9f, 1.4f, 1.4f, 0};
    else if (P.prone) // crawling on elbows, legs trailing behind
    {
        float q = P.runPhase * 1.3f, mv = std::fabs(m.vx) > 0.05f ? 1.0f : 0.0f;
        ps = {-1.35f + 0.25f * std::sin(q) * mv, -1.5f, -1.45f - 0.25f * std::sin(q) * mv, -1.55f, 1.42f,
              1.35f + 0.45f * std::sin(q) * mv, 1.35f - 0.45f * std::sin(q) * mv, 0};
    }
    else if (P.crouch)
    {
        float q = P.runPhase, mv = std::fabs(m.vx) > 0.08f ? 1.0f : 0.0f;
        ps = {1.05f + 0.35f * std::sin(q) * mv, -0.6f - 0.3f * std::max(0.0f, std::sin(q + 1.9f)) * mv,
              0.9f + 0.35f * std::sin(q + PI) * mv, -0.75f - 0.3f * std::max(0.0f, std::sin(q + PI + 1.9f)) * mv,
              0.7f, 0.7f + 0.4f * std::sin(q) * mv, 0.9f, 0};
    }
    else if (m.inLiquid)
    {
        float k = std::sin(t * 0.2f);
        ps = {0.4f * k + 0.3f, 0.2f, -0.4f * k + 0.3f, -0.2f, 0.5f, -0.5f + k * 0.6f, 2.5f - k * 0.6f, 0};
    }
    else if (P.climb) // hand over hand up the rope, one knee hooked round it
    {
        float k = std::sin(P.runPhase * 2.5f) * 0.45f;
        ps = {0.9f + k, -0.9f, -0.15f - k, -0.2f, 0.0f, 2.9f + k, 2.8f - k, 0};
    }
    else if (P.onWall)
    {
        float k = std::sin(t * 0.25f) * (m.vy < 0 ? 0.5f : 0); // climbing shuffle
        ps = {1.0f + k, -0.4f, 0.7f - k, 0.2f, -0.08f, 2.7f + k, 2.5f - k, 0};
    }
    else if (P.hook == 2 && air) ps = {0.35f, 0.1f, -0.3f, -0.1f, 0.0f, -0.4f, 2.8f, 0};
    else if (air && m.vy < -0.3f) ps = {1.0f, -0.6f, -0.3f, 0.5f, 0.12f, -0.8f, 2.2f, 0};
    else if (air) ps = {0.45f, 0.15f, -0.5f, -0.15f, 0.0f, 2.4f, 2.0f, 0};
    else if (std::fabs(m.vx) > 0.12f)
    {
        float q = P.runPhase * (m.vx * f < 0 ? -1.0f : 1.0f); // backpedal plays the cycle in reverse
        float fa = 0.8f * std::sin(q), ba = 0.8f * std::sin(q + PI);
        ps.fThigh = fa;
        ps.fShin = fa - 1.0f * (0.5f + 0.5f * std::sin(q + 1.9f));
        ps.bThigh = ba;
        ps.bShin = ba - 1.0f * (0.5f + 0.5f * std::sin(q + PI + 1.9f));
        ps.lean = 0.2f * std::min(1.0f, std::fabs(m.vx) * 1.2f) * (m.vx * f < 0 ? -0.5f : 1.0f);
        ps.backArm = -0.9f * std::sin(q);
        ps.frontArm = 0.9f * std::sin(q);
        ps.bob = std::fabs(std::sin(q)) * 1.0f;
    }
    else
        ps = {0.1f, 0.03f, -0.1f, -0.03f, 0.0f, 0.12f, -0.12f, std::sin(t * 0.05f) > 0.3f ? 1.0f : 0.0f}; // idle breathing

    float sy = P.squash * (air && !P.rollT ? 1 + clampf(-m.vy * 0.03f, -0.1f, 0.12f) : 1);
    if (P.rollT) sy *= 0.75f;
    float sx = 1.0f / std::sqrt(sy);
    float bx = std::floor(m.x) + m.w * 0.5f - camX, by = std::floor(m.y) + m.h - camY; // whole-pixel anchor, no shimmer
    auto off = [&](float ang, float len) { return Vector2{f * std::sin(ang) * len * sx, std::cos(ang) * len * sy}; };

    // a lean warrior with real proportions: ~23px tall, legs 10, torso 7, head 6
    const float LEG = 5.0f, TORSO = 7.0f;
    Vector2 hip = {bx, by - 2 * LEG * sy + 0.5f + ps.bob};
    if (P.crouch && !P.prone) // knees bent: the hips sit as high as the legs reach
        hip.y = by - std::max(std::cos(ps.fThigh) + std::cos(ps.fShin), std::cos(ps.bThigh) + std::cos(ps.bShin)) * LEG * sy;
    if (P.prone) { hip.y = by - 3.0f; hip.x = bx - f * 5; }
    Vector2 shoulder = {hip.x + f * std::sin(ps.lean) * TORSO * sx, hip.y - std::cos(ps.lean) * TORSO * sy};

    simulateCape(P, {shoulder.x + camX - f * 2.0f, shoulder.y + camY + 0.5f}, f);
    if (m.iframes > 0 && P.rollT == 0 && (G.frame / 3) % 2) return; // hurt blink

    Look k = playerLook();
    canvasBegin((int)std::floor(bx) - CW / 2, (int)std::floor(by) - CH + 16);

    for (int i = 0; i < 6; i++) // the cloak: wide at the shoulders, tapering as it streams out
    {
        Color c = lerpColor(k.cloak, mul(k.cloak, 0.5f), i / 6.0f);
        Vector2 a = {P.cape[i].x - camX, P.cape[i].y - camY}, b = {P.cape[i + 1].x - camX, P.cape[i + 1].y - camY};
        int w = i < 2 ? 3 : 2;
        for (int j = 0; j < w; j++) seg({a.x, a.y + j}, {b.x, b.y + j}, j == w - 1 ? mul(c, 0.75f) : (j == 0 ? brighten(c, 14) : c));
    }

    if (P.rollT > 0)
    {
        rotA = f * (1 - P.rollT / 20.0f) * 2 * PI;
        rotCx = bx;
        rotCy = by - 11;
    }

    if (hasWeapon && !drawn && P.rollT == 0 && !P.prone) drawStowed(*wpn, shoulder, hip, f);

    auto leg = [&](float th, float sh, float dim) {
        Vector2 kn = add(hip, off(th, LEG));
        Vector2 ft = add(kn, off(sh, LEG));
        thick(hip, kn, 2, mul(k.pants, dim), f);
        thick(kn, ft, 2, mul(k.wrap, dim), f);
        for (float tt : {0.25f, 0.6f}) // leg wraps wound round the shin
        {
            Vector2 w = {kn.x + (ft.x - kn.x) * tt, kn.y + (ft.y - kn.y) * tt};
            for (int i = -1; i <= 0; i++) px(w.x + i * 0.5f, w.y, mul(k.wrap, 0.72f * dim));
        }
        for (int i = -1; i <= 1; i++) px(ft.x + i * f, ft.y, mul(k.boot, dim * (i == 1 ? 0.8f : 1.0f)));
        px(ft.x, ft.y - 1, mul(k.boot, dim * 1.15f));
    };
    auto arm = [&](Vector2 sh, float ang, float dim) {
        Vector2 el = add(sh, off(ang, 3.6f));
        Vector2 hand = add(el, off(ang * 0.9f, 3.0f));
        thick(sh, el, 2, mul(k.set == AS_WOOL || k.set == AS_LEATHER ? mul(k.tunic, 1.05f) : k.A, dim), f);
        thick(el, hand, 2, mul(k.bracer, dim), f);
        px(hand.x, hand.y, mul(k.skin, dim));
        px(hand.x + f, hand.y, mul(k.skinD, dim));
        return hand;
    };

    arm({shoulder.x - f * 2.0f, shoulder.y + 1}, ps.backArm, 0.72f);
    leg(ps.bThigh, ps.bShin, 0.75f);
    leg(ps.fThigh, ps.fShin, 1.0f);

    // torso: broad shoulders narrowing to the belt, patterned by the armour set
    static const int halfW[8] = {3, 3, 3, 2, 2, 2, 2, 2};
    for (int r = 0; r <= 7; r++)
    {
        float tt = r / 7.0f;
        float cx = shoulder.x + (hip.x - shoulder.x) * tt, cy = shoulder.y + (hip.y - shoulder.y) * tt;
        int half = halfW[r];
        for (int c = -half; c <= half; c++) px(cx + c * f, cy, bodyColor(k, r, c, half));
    }
    // the skirt of the tunic (or byrnie) flares over the thighs
    for (int r = 1; r <= 3; r++)
    {
        int half = r < 3 ? 2 : 3;
        for (int c = -half; c <= half; c++)
        {
            Color col = r == 3 ? (k.set == AS_WOOL ? k.trim : mul(bodyColor(k, r + 8, c, half), 0.8f)) : bodyColor(k, r + 8, c, half);
            if (k.set == AS_LEATHER && r == 3 && c % 2) continue; // hanging leather strips
            px(hip.x + c * f, hip.y + r * sy, col);
        }
    }
    for (int c = -2; c <= 2; c++) // belt and buckle
        px(hip.x + c * f, hip.y, c == 1 ? Color{224, 190, 90, 255} : Color{66, 44, 30, 255});
    if (k.set == AS_WOOL) for (int c = -3; c <= 3; c++) px(shoulder.x + c * f, shoulder.y + 3 + (c > 0 ? 0 : 0), (c + 3) % 2 ? k.trim : mul(k.trim, 0.8f)); // woven band across the chest
    if (k.set == AS_LAMELLAR || k.set == AS_SCALE) // a rune glowing on the breast (Algiz, for protection)
    {
        Color g = (G.frame / 8) % 7 ? k.glow : WHITE;
        float rx = shoulder.x + (hip.x - shoulder.x) * 0.35f + f * 0.5f, ry = shoulder.y + 2;
        for (int d = 0; d < 4; d++) px(rx, ry + d, g);
        px(rx - 1, ry, g);
        px(rx + 1, ry, g);
    }
    for (int c = -2; c <= 2; c++) // fur collar where the cloak is pinned
        px(shoulder.x + c * f, shoulder.y, (c + 3) % 3 ? k.fur : mul(k.fur, 0.8f));
    if (k.set >= AS_MAIL) // pauldrons
        for (int dy = 0; dy < 2; dy++)
            for (int dx = 0; dx < 2; dx++)
                px(shoulder.x + f * (2 + dx), shoulder.y + dy, dy == 0 ? k.Ah : (dx == 1 ? k.Ad : k.A));

    // head
    bool blink = (G.frame % 220) < 6;
    int rows = k.headRows;
    float hx = std::floor(shoulder.x) - 3 + (f > 0 ? 1 : 0) + (P.prone ? f * 3 : 0), hy = std::floor(shoulder.y) - rows + 1 + (P.prone ? 2 : 0);
    for (int j = 0; j < rows; j++)
        for (int i = 0; i < 6; i++)
        {
            char ch = k.head[j][i];
            Color c;
            switch (ch)
            {
            case 's': c = k.skin; break;
            case 'S': c = k.skinD; break;
            case 'F': c = blink ? k.skinD : Color{241, 240, 253, 255}; break;
            case 'u': c = blink ? k.skinD : Color{60, 110, 170, 255}; break;
            case 'h': c = k.hairD; break;
            case 'H': c = k.hairL; break;
            case 'A': c = k.A; break;
            case 'D': c = k.Ad; break;
            case 'L': c = k.Ah; break;
            case 'c': c = mul(k.A, 0.85f); break;
            case 'C': c = k.Ad; break;
            case 'v': c = {18, 18, 26, 255}; break;
            case 'g': c = (G.frame / 8) % 6 ? k.glow : WHITE; break;
            case 'n': c = {232, 226, 206, 255}; break;
            default: continue;
            }
            px(hx + (f > 0 ? i : 5 - i), hy + j, c);
        }

    // front arm: swings while unarmed, lowers a drawn blade, aims ranged weapons, reaches for the rope
    Vector2 shF = {shoulder.x + f * 1.5f, shoulder.y + 1};
    Vector2 handF;
    bool weaponInHand = drawn && P.rollT == 0 && !P.prone;
    float aimAng = P.aim;
    if (weaponInHand && isMelee(wpn->type))
    {
        if (P.swingT > 0)
        {
            float tt = P.swingT / 10.0f;
            aimAng = P.aim - P.swingDir * 1.4f + P.swingDir * 2.8f * (1 - tt * tt);
        }
        else
            aimAng = f > 0 ? 1.0f : PI - 1.0f; // relaxed, blade lowered in front
    }
    if (P.hook == 2) { aimAng = std::atan2(P.hy - camY - shF.y, P.hx - camX - shF.x); weaponInHand = false; }
    if (weaponInHand || P.hook == 2)
    {
        Vector2 el = {shF.x + std::cos(aimAng) * 3.0f, shF.y + std::sin(aimAng) * 3.0f};
        handF = {shF.x + std::cos(aimAng) * 5.5f, shF.y + std::sin(aimAng) * 5.5f};
        thick(shF, el, 2, k.set == AS_WOOL || k.set == AS_LEATHER ? mul(k.tunic, 1.05f) : k.A, f);
        thick(el, handF, 2, k.bracer, f);
        px(handF.x, handF.y, k.skin);
        px(handF.x + 1, handF.y, k.skinD);
    }
    else
        handF = arm(shF, ps.frontArm, 1.0f);

    rotA = 0;
    canvasEnd(m.hurtFlash > 0);
    if (weaponInHand) drawHeld(handF.x, handF.y);
}

// ---------------------------------------------------------------- creature sprites

// Sprite pixels, optionally enlarged 2x: Scale2x (EPX) smooths the simple object icons,
// plain doubling keeps the hand-shaded boss art crisp.
struct BigSprite { int w = 0, h = 0; std::vector<Color> px; };
enum { SC_NATIVE, SC_EPX, SC_DOUBLE };

static const BigSprite& bigOf(const Sprite& s, Color tint, int mode)
{
    static std::map<std::tuple<const void*, unsigned, int>, BigSprite> cache;
    auto key = std::make_tuple((const void*)&s, (unsigned)ColorToInt(tint), mode);
    auto it = cache.find(key);
    if (it != cache.end()) return it->second;

    int w = spriteWidth(s), h = s.h;
    auto at = [&](int x, int y) -> char { return (x < 0 || y < 0 || x >= w || y >= h) ? '.' : s.rows[y][x]; };
    int k = mode == SC_NATIVE ? 1 : 2;
    BigSprite b;
    b.w = w * k;
    b.h = h * k;
    b.px.assign(b.w * b.h, BLANK);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
        {
            char P = at(x, y), q[4] = {P, P, P, P};
            if (mode == SC_EPX)
            {
                char A = at(x, y - 1), B = at(x + 1, y), C = at(x - 1, y), D = at(x, y + 1);
                q[0] = (C == A && C != D && A != B) ? A : P;
                q[1] = (A == B && A != C && B != D) ? B : P;
                q[2] = (D == C && D != B && C != A) ? C : P;
                q[3] = (B == D && B != A && D != C) ? D : P;
            }
            for (int j = 0; j < k * k; j++)
            {
                char ch = q[mode == SC_NATIVE ? 0 : j];
                if (ch != '.') b.px[(y * k + j / k) * b.w + x * k + j % k] = paletteColor(ch, tint);
            }
        }
    return cache[key] = b;
}

// Draws a sprite anchored at its bottom-centre with squash, lean and flip.
static void drawBig(const BigSprite& b, float ax, float ay, bool flip, float sxs, float sys, float lean, Color flash, float alpha)
{
    int rw = std::max(1, (int)std::ceil(sxs)), rh = std::max(1, (int)std::ceil(sys));
    for (int j = 0; j < b.h; j++)
        for (int i = 0; i < b.w; i++)
        {
            Color c = b.px[j * b.w + i];
            if (!c.a) continue;
            int si = flip ? b.w - 1 - i : i;
            float lx = (si - b.w * 0.5f) * sxs, ly = (j - b.h + 1) * sys;
            float X = ax + lx + lean * (-ly) / b.h * 4.0f, Y = ay + ly;
            if (flash.a) c = Color{flash.r, flash.g, flash.b, c.a};
            c.a = (unsigned char)(c.a * alpha);
            DrawRectangle((int)std::floor(X), (int)std::floor(Y), rw, rh, c);
        }
}

void drawSpriteNative(const Sprite& s, float x, float bottom, bool flip)
{
    drawBig(bigOf(s, WHITE, SC_NATIVE), x, bottom, flip, 1, 1, 0, BLANK, 1);
}

void drawSpriteBig(const Sprite& s, float x, float bottom, bool flip, Color tint)
{
    drawBig(bigOf(s, tint, SC_EPX), x, bottom, flip, 1, 1, 0, BLANK, 1);
}

const Sprite& enemySprite(const Mob& m)
{
    bool alt = ((int)m.anim) & 1;
    switch (m.type)
    {
    case E_GOBLIN: return alt ? SPR_GOBLIN_B : SPR_GOBLIN_A;
    case E_BOMBER: return SPR_BOMBER;
    case E_SKELETON: return alt ? SPR_SKELETON_B : SPR_SKELETON_A;
    case E_ARCHER: return SPR_ARCHER;
    case E_BAT: return ((G.frame / 6 + m.id) & 1) ? SPR_BAT_B : SPR_BAT_A;
    case E_SLIME: return SPR_SLIME;
    case E_CULTIST: return SPR_CULTIST;
    case E_KNIGHT: return alt ? SPR_KNIGHT_B : SPR_KNIGHT_A;
    case E_IMP: return SPR_IMP;
    case E_WRAITH: return SPR_WRAITH;
    case E_GOLEM: return SPR_GOLEM;
    case E_BLACKKNIGHT: return SPR_BLACKKNIGHT;
    case E_WOLF: return alt ? SPR_WOLF_B : SPR_WOLF_A;
    case E_REDCAP: return SPR_REDCAP;
    case E_DRAUGR: return SPR_DRAUGR;
    case E_TROLL: return SPR_TROLL;
    case E_BANSHEE: return SPR_BANSHEE;
    case E_KELPIE: return SPR_KELPIE;
    case E_GUARD: return alt ? SPR_GUARD_B : SPR_GUARD_A;
    default: return SPR_LICH;
    }
}

static const BigSprite& mobArt(const Mob& m)
{
    static const Color coats[5] = {{132, 128, 122, 255}, {128, 92, 60, 255}, {64, 60, 62, 255}, {204, 200, 190, 255}, {156, 98, 54, 255}}; // grey, brown, black, white, russet
    return bigOf(enemySprite(m), m.type == E_WOLF ? coats[(m.id * 7) % 5] : WHITE, m.boss ? SC_DOUBLE : SC_NATIVE);
}

void drawMobAnimated(const Mob& m, int camX, int camY)
{
    const BigSprite& b = mobArt(m);
    const EnemyDef& d = ENEMIES[m.type];
    bool flying = d.ai == AI_FLY || d.ai == AI_FLYCAST || d.ai == AI_BOSS_LICH;

    float sys = m.squash, bob = 0, lunge = 0;
    if (!flying && !m.onGround) sys *= 1 + clampf(-m.vy * 0.04f, -0.12f, 0.18f);
    if (m.onGround && std::fabs(m.vx) < 0.1f) sys *= 1 + 0.04f * std::sin(G.frame * 0.08f + m.id); // breathing
    if (flying) bob = std::sin(G.frame * 0.12f + m.id) * 2.0f;
    if (m.type == E_SLIME) sys *= 1 + 0.12f * std::sin(G.frame * 0.15f + m.id);
    float lean = clampf(m.vx * 0.35f, -0.45f, 0.45f);
    float sxs = 1.0f / sys;
    if (m.attackT > 0)
    {
        bool wind = m.attackT > 8; // anticipation, then the strike
        lunge = m.facing * (wind ? -2.0f : 4.0f);
        sxs *= wind ? 0.9f : 1.12f;
        sys *= wind ? 1.08f : 0.92f;
    }
    if (m.hurtFlash > 0) { sxs *= 1.1f; sys *= 0.9f; }

    float ax = std::floor(m.x) + m.w * 0.5f - camX + lunge, ay = std::floor(m.y) + m.h - camY + bob;
    Color flash = m.hurtFlash > 0 ? WHITE : BLANK;
    if (m.type == E_BLACKKNIGHT && m.state == 2 && (G.frame / 3) % 2) flash = {255, 60, 60, 255};
    drawBig(b, ax, ay, m.facing < 0, sxs, sys, lean, flash, m.type == E_WRAITH ? 0.75f : 1.0f);

    if (m.hp < m.maxHp && !m.boss)
    {
        int bw = std::max(m.w, 10);
        int bx = (int)(std::floor(m.x) + m.w * 0.5f - camX - bw / 2.0f), by = (int)(std::floor(m.y) - camY - 5 + bob);
        DrawRectangle(bx - 1, by - 1, bw + 2, 4, {10, 6, 10, 200});
        DrawRectangle(bx, by, (int)(bw * std::max(0.0f, m.hp / m.maxHp)), 2, {230, 40, 40, 255});
    }
}

// Shatter a sprite into its pixels.
void burstSprite(const Mob& m)
{
    const BigSprite& b = mobArt(m);
    float ox = m.cx() - b.w / 2.0f, oy = m.y + m.h - b.h;
    for (int j = 0; j < b.h; j++)
        for (int i = 0; i < b.w; i++)
        {
            Color c = b.px[j * b.w + i];
            if (c.a < 200) continue;
            float x = ox + (m.facing < 0 ? b.w - 1 - i : i), y = oy + j;
            float dx = x - m.cx(), dy = y - m.cy();
            spawnParticle(x, y, dx * 0.08f + frange(-0.6f, 0.6f) + m.vx * 0.5f, dy * 0.05f + frange(-2.0f, -0.4f), irange(30, 70), c, 0.12f);
        }
}

// ---------------------------------------------------------------- ragdolls

// joints: 0 head, 1 chest, 2 hip, 3 knee L, 4 foot L, 5 knee R, 6 foot R, 7 hand L, 8 hand R
static const int STICKS[][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 4}, {2, 5}, {5, 6}, {1, 7}, {1, 8}, {0, 2}};
static const int NSTICK = 9;

void spawnRagdoll(float cx, float bottom, float h, float vx, float vy, Color head, Color body, Color limb, bool bony, CellMaterial gore)
{
    if (G.rags.size() >= 40) G.rags.erase(G.rags.begin());
    Ragdoll r;
    float s = h / 20.0f;
    r.scale = s;
    float y0 = bottom - h;
    Vector2 pose[9] = {{cx, y0 + 4 * s}, {cx, y0 + 9 * s}, {cx, y0 + 14 * s}, {cx - 1.5f * s, y0 + 17 * s}, {cx - 2 * s, bottom - 0.5f},
                       {cx + 1.5f * s, y0 + 17 * s}, {cx + 2 * s, bottom - 0.5f}, {cx - 4 * s, y0 + 12 * s}, {cx + 4 * s, y0 + 12 * s}};
    for (int i = 0; i < 9; i++)
    {
        r.p[i] = pose[i];
        float jitter = i == 0 ? 1.2f : 0.5f;
        r.pp[i] = {pose[i].x - vx - frange(-jitter, jitter), pose[i].y - vy - frange(-jitter, jitter)};
    }
    for (int k = 0; k < NSTICK; k++)
    {
        Vector2 a = pose[STICKS[k][0]], b = pose[STICKS[k][1]];
        r.len[k] = std::hypot(b.x - a.x, b.y - a.y);
    }
    r.head = head;
    r.body = body;
    r.limb = limb;
    r.bony = bony;
    r.gore = gore;
    G.rags.push_back(r);
}

void pushRagdolls(float x, float y, float radius, float force)
{
    for (auto& r : G.rags)
        for (auto& p : r.p)
        {
            float dx = p.x - x, dy = p.y - y, d = std::sqrt(dx * dx + dy * dy);
            if (d > radius || d < 0.01f) continue;
            float k = force * (1 - d / radius);
            p.x += dx / d * k;
            p.y += dy / d * k - k * 0.5f;
        }
}

void updateRagdolls()
{
    for (auto& r : G.rags)
    {
        r.life++;
        // fresh corpses keep spilling: blood (or bone dust, rubble...) from the chest and neck
        if (r.gore != CellMaterial::Empty && r.life < 240 && irand(r.life / 30 + 2) == 0 && G.parts.size() < 5000)
        {
            int j = irand(2);
            Vector2 v = {r.p[j].x - r.pp[j].x, r.p[j].y - r.pp[j].y};
            Cell c;
            c.material = r.gore;
            c.shade = (uint8_t)xr();
            Particle q{r.p[j].x, r.p[j].y, v.x + frange(-0.6f, 0.6f), v.y + frange(-1.2f, 0.2f), 200, cellColor(c, 0, 0), 0.15f};
            q.toCell = r.gore;
            G.parts.push_back(q);
        }
        for (int i = 0; i < 9; i++)
        {
            Vector2& p = r.p[i];
            Vector2 v = {(p.x - r.pp[i].x) * 0.985f, (p.y - r.pp[i].y) * 0.985f};
            Vector2 np = {p.x + v.x, p.y + v.y + 0.2f};
            if (isSolid((int)std::floor(np.x), (int)std::floor(p.y))) np.x = p.x - v.x * 0.3f;
            if (isSolid((int)std::floor(np.x), (int)std::floor(np.y)))
            {
                np.y = p.y - v.y * 0.25f;
                np.x = p.x + (np.x - p.x) * 0.6f; // friction on the ground
            }
            r.pp[i] = p;
            p = np;
        }
        for (int it = 0; it < 4; it++)
            for (int k = 0; k < NSTICK; k++)
            {
                Vector2& a = r.p[STICKS[k][0]];
                Vector2& b = r.p[STICKS[k][1]];
                float dx = b.x - a.x, dy = b.y - a.y, d = std::sqrt(dx * dx + dy * dy);
                if (d < 0.001f) continue;
                float diff = (d - r.len[k]) / d * 0.5f;
                a.x += dx * diff; a.y += dy * diff;
                b.x -= dx * diff; b.y -= dy * diff;
            }
        for (auto& p : r.p) // never leave a joint buried in rock
            for (int tries = 0; tries < 3 && isSolid((int)std::floor(p.x), (int)std::floor(p.y)); tries++) p.y -= 1;
    }
    G.rags.erase(std::remove_if(G.rags.begin(), G.rags.end(), [](const Ragdoll& r) { return r.life > 720; }), G.rags.end());
}

void drawRagdolls(int camX, int camY)
{
    for (auto& r : G.rags)
    {
        float alpha = r.life > 660 ? (720 - r.life) / 60.0f : 1.0f;
        auto P = [&](int i) { return Vector2{r.p[i].x - camX, r.p[i].y - camY}; };
        auto limbLine = [&](int a, int b, float w, Color c) {
            c.a = (unsigned char)(255 * alpha);
            DrawLineEx(P(a), P(b), w, c);
        };
        float s = r.scale;
        limbLine(1, 7, 1.5f * s, r.limb);
        limbLine(2, 3, 2 * s, r.limb);
        limbLine(3, 4, 2 * s, r.limb);
        limbLine(2, 5, 2 * s, mul(r.limb, 0.85f));
        limbLine(5, 6, 2 * s, mul(r.limb, 0.85f));
        limbLine(1, 2, 4 * s, r.body);
        limbLine(1, 8, 1.5f * s, mul(r.limb, 0.9f));
        Vector2 h = P(0);
        Color hc = r.head;
        hc.a = (unsigned char)(255 * alpha);
        DrawCircleV(h, 2.6f * s, hc);
        if (r.bony) DrawRectangle((int)h.x - 1, (int)h.y - 1, 1, 1, OUTLINE);
    }
}

// Pick ragdoll colours for a creature (blobs and ghosts burst into pixels instead).
void ragdollForMob(const Mob& m)
{
    Color head, body, limb;
    bool bony = false;
    switch (m.type)
    {
    case E_GOBLIN: head = {146, 166, 92, 255}; body = {92, 66, 46, 255}; limb = {102, 122, 64, 255}; break;
    case E_BOMBER: head = {196, 62, 52, 255}; body = {92, 66, 46, 255}; limb = {102, 122, 64, 255}; break;
    case E_SKELETON: head = body = limb = {222, 214, 190, 255}; bony = true; break;
    case E_ARCHER: head = {222, 214, 190, 255}; body = {108, 72, 130, 255}; limb = {172, 164, 142, 255}; bony = true; break;
    case E_CULTIST: head = {70, 46, 88, 255}; body = {108, 72, 130, 255}; limb = {150, 104, 168, 255}; break;
    case E_KNIGHT: head = {200, 206, 214, 255}; body = {196, 62, 52, 255}; limb = {142, 148, 162, 255}; break;
    case E_IMP: head = body = limb = {242, 128, 36, 255}; break;
    case E_GOLEM: head = body = limb = {116, 118, 132, 255}; break;
    case E_BLACKKNIGHT: head = {94, 100, 116, 255}; body = {58, 62, 76, 255}; limb = {58, 62, 76, 255}; break;
    case E_LICH: head = {222, 214, 190, 255}; body = {108, 72, 130, 255}; limb = {70, 46, 88, 255}; bony = true; break;
    case E_REDCAP: head = {196, 62, 52, 255}; body = {92, 66, 46, 255}; limb = {186, 138, 106, 255}; break;
    case E_DRAUGR: head = {130, 180, 220, 255}; body = {78, 80, 94, 255}; limb = {80, 120, 170, 255}; break;
    case E_TROLL: head = body = {118, 134, 88, 255}; limb = {82, 98, 62, 255}; break;
    case E_GUARD: head = {186, 138, 106, 255}; body = {196, 62, 52, 255}; limb = {78, 80, 94, 255}; break;
    default: burstSprite(m); return;
    }
    spawnRagdoll(m.cx(), m.y + m.h, (float)m.h, m.vx * 0.8f + frange(-0.5f, 0.5f), std::min(m.vy, 0.0f) - frange(0.5f, 1.5f), head, body, limb, bony, ENEMIES[m.type].gore);
}

void ragdollForPlayer()
{
    Mob& m = G.p.m;
    Look k = playerLook();
    spawnRagdoll(m.cx(), m.y + m.h, (float)m.h + 3, m.vx + frange(-0.5f, 0.5f), m.vy - 1.5f, k.metal ? k.A : k.hairD, k.tunic, k.pants, false, CellMaterial::Blood);
}

// ---------------------------------------------------------------- held weapon

void drawHeld(float ox, float oy)
{
    Player& P = G.p;
    if (P.hotbar.empty()) return;
    const Weapon& w = P.hotbar[P.sel];
    float f = (float)G.p.m.facing;
    float a = P.aim;
    Color brown = {110, 70, 40, 255};
    auto pt = [&](float ang, float d) { return Vector2{ox + std::cos(ang) * d, oy + std::sin(ang) * d}; };
    float pa = 0;
    auto line = [&](Vector2 p0, Vector2 p1, float th, Color c) { DrawLineEx(p0, p1, th, c); };
    auto across = [&](Vector2 c, float half, float th, Color col) {
        line({c.x - std::cos(pa) * half, c.y - std::sin(pa) * half}, {c.x + std::cos(pa) * half, c.y + std::sin(pa) * half}, th, col);
    };
    if (w.glow.a && G.frame % 6 == 0) // legendary shimmer
        spawnParticle(ox + std::cos(a) * 8 + frange(-2, 2) + G.rcx, oy + std::sin(a) * 8 + frange(-2, 2) + G.rcy, 0, -0.3f, 25, w.glow, -0.002f);
    if (isMelee(w.type))
    {
        float L = w.type == W_DAGGER ? 6.0f : (w.type == W_SPEAR ? 15.0f : (w.type == W_MACE ? 9.0f : (w.type == W_PAN ? 6.0f : 10.0f)));
        Color mc = METALS[w.metal].color;
        if (P.swingT > 0)
        {
            // eased swing (fast snap, slow follow-through) with a Dead Cells style smear
            float t = P.swingT / 10.0f, e = 1 - t * t;
            float start = a - P.swingDir * 1.4f;
            a = start + P.swingDir * 2.8f * e;
            Color sm = lerpColor(brighten(mc, 60), ELEMENT_COLORS[METALS[w.metal].el], METALS[w.metal].el ? 0.6f : 0.0f);
            sm.a = (unsigned char)(170 * t);
            BeginBlendMode(BLEND_ADDITIVE);
            DrawRing({ox, oy}, L * 0.55f + 2, 5 + L, start * RAD2DEG, a * RAD2DEG, 18, sm);
            DrawRing({ox, oy}, L * 0.8f + 2, 5 + L, (a - P.swingDir * 0.5f) * RAD2DEG, a * RAD2DEG, 8, {255, 255, 255, (unsigned char)(120 * t)});
            EndBlendMode();
        }
        else
            a = f > 0 ? 1.0f : PI - 1.0f; // lowered at rest
        pa = a + PI / 2;
        if (w.type == W_SPEAR)
        {
            line(pt(a, -5), pt(a, L - 2), 1.5f, brown);
            line(pt(a, L - 3), pt(a, L + 2), 2.5f, mc);
            DrawLineEx(pt(a, L - 2), pt(a, L + 1), 1, brighten(mc, 55));
        }
        else if (w.type == W_MACE)
        {
            line(pt(a, -2), pt(a, L), 1.5f, brown);
            Vector2 hd = pt(a, L + 1.5f);
            DrawCircleV(hd, 3.2f, mul(mc, 0.7f));
            DrawCircleV(hd, 2.6f, mc);
            DrawCircleV({hd.x - 1, hd.y - 1}, 1.2f, brighten(mc, 60));
        }
        else if (w.type == W_PAN)
        {
            line(pt(a, -2), pt(a, L), 1.5f, brown);
            Vector2 hd = pt(a, L + 3.5f);
            DrawCircleV(hd, 4.0f, {70, 70, 78, 255});
            DrawCircleV(hd, 2.6f, {96, 96, 104, 255});
        }
        else
        {
            line(pt(a, -2), pt(a, 2), 1.5f, brown);
            line(pt(a, 2), pt(a, 2 + L), 2, mc);
            DrawLineEx(pt(a, 3), pt(a, 1 + L), 1, brighten(mc, 55));
            if (w.type == W_AXE) across(pt(a, 1 + L), 2.5f, 3, mc);
            else across(pt(a, 2), 2.0f, 1.2f, {215, 184, 77, 255});
        }
    }
    else if (w.type == W_CROSSBOW)
    {
        float back = P.recoil * 0.4f;
        pa = a + PI / 2;
        line(pt(a, -2 - back), pt(a, 9 - back), 2, brown);
        across(pt(a, 7 - back), 4.0f, 1.2f, METALS[w.metal].color);
    }
    else if (w.type == W_STAFF)
    {
        line(pt(a, -4), pt(a, 11), 1.5f, brown);
        Vector2 g = pt(a, 12);
        Color gem = w.staff.gem;
        if (w.staff.cd > 0) gem = lerpColor(gem, WHITE, 0.4f);
        BeginBlendMode(BLEND_ADDITIVE);
        DrawCircleGradient((int)g.x, (int)g.y, 8, {gem.r, gem.g, gem.b, 110}, {gem.r, gem.g, gem.b, 0});
        EndBlendMode();
        DrawRectangle((int)g.x - 1, (int)g.y - 2, 2, 4, gem);
        DrawRectangle((int)g.x - 2, (int)g.y - 1, 4, 2, gem);
        DrawRectangle((int)g.x - 1, (int)g.y - 1, 1, 1, WHITE);
    }
}

// Character art and animation:
//  - the player is a jointed chibi rig drawn into a small canvas, then outlined
//  - creature sprites get automatic outlines/rim light (bosses are also Scale2x'd)
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
            if (!c.a)
            {
                bool edge = (x > 0 && canvas[y * CW + x - 1].a) || (x < CW - 1 && canvas[y * CW + x + 1].a) ||
                            (y > 0 && canvas[(y - 1) * CW + x].a) || (y < CH - 1 && canvas[(y + 1) * CW + x].a);
                if (!edge) continue;
                c = OUTLINE;
            }
            else if (flash)
                c = WHITE;
            DrawRectangle(cox + x, coy + y, 1, 1, c);
        }
}

// ---------------------------------------------------------------- player heads (12x9, facing right)
// h/H hair, s/S skin, F eye white, u iris, A/D/L armour, v visor, g glow, p plume, n horn

static const char* HEAD_HAIR[] = {
    "..hHHh..",
    ".hhHHHh.",
    "hhhSssH.",
    "hhSsFus.",
    "hhSssss.",
    ".hSssSs.",
    "..hSss..",
};
static const char* HEAD_KETTLE[] = {
    "...LL...",
    "..LAAD..",
    "DDDDDDDD",
    ".hSsFus.",
    ".hSssss.",
    "..SssSs.",
    "...Sss..",
};
static const char* HEAD_GREAT[] = {
    "..LLLL..",
    ".LAAAAD.",
    ".AAAAAAD",
    ".AAvvvvD",
    ".AAAAvAD",
    ".DAAAAAD",
    "..DDDD..",
};
static const char* HEAD_CREST[] = {
    "..LDLL..",
    ".LAAAAD.",
    ".AAAAAAD",
    ".AAvvvvD",
    ".AAAAvAD",
    ".DAAAAAD",
    "..DDDD..",
};
static const char* HEAD_PLUME[] = {
    "pp.LLL..",
    ".pLAAAD.",
    ".AAAAAAD",
    ".AAggggD",
    ".AAAAvAD",
    ".DAAAAAD",
    "..DDDD..",
};
static const char* HEAD_HORNED[] = {
    "n......n",
    "n.LLLL.n",
    ".nAAAAn.",
    ".AAggggD",
    ".AAAAAAD",
    ".DAAAAAD",
    "..DDDD..",
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

struct Look { Color tunic, tunicD, sleeve, sleeveD, pants, boot, skin, skinD, hairD, hairL, A, Ad, Ah; bool metal; };

static Look playerLook()
{
    Player& P = G.p;
    Look k;
    k.metal = P.armour >= 0;
    k.A = k.metal ? METALS[P.armour].color : Color{164, 136, 100, 255};
    k.Ad = mul(k.A, 0.62f);
    k.Ah = brighten(k.A, 45);
    k.tunic = k.metal ? k.A : Color{168, 40, 52, 255};
    k.tunicD = k.metal ? k.Ad : Color{112, 24, 36, 255};
    k.sleeve = k.metal ? k.Ah : Color{218, 217, 204, 255};
    k.sleeveD = k.metal ? k.A : Color{180, 180, 174, 255};
    k.pants = k.metal ? mul(k.A, 0.55f) : Color{146, 153, 164, 255};
    k.boot = {143, 98, 63, 255};
    k.skin = {255, 209, 156, 255};
    k.skinD = {229, 147, 99, 255};
    k.hairD = {105, 72, 58, 255};
    k.hairL = {145, 100, 81, 255};
    return k;
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
        float fa = 0.85f * std::sin(q), ba = 0.85f * std::sin(q + PI);
        ps.fThigh = fa;
        ps.fShin = fa - 1.0f * (0.5f + 0.5f * std::sin(q + 1.9f));
        ps.bThigh = ba;
        ps.bShin = ba - 1.0f * (0.5f + 0.5f * std::sin(q + PI + 1.9f));
        ps.lean = 0.22f * std::min(1.0f, std::fabs(m.vx) * 1.2f) * (m.vx * f < 0 ? -0.5f : 1.0f);
        ps.backArm = -1.0f * std::sin(q);
        ps.frontArm = 1.0f * std::sin(q);
        ps.bob = std::fabs(std::sin(q)) * 1.0f;
    }
    else
        ps = {0.12f, 0.04f, -0.12f, -0.04f, 0.0f, 0.15f, -0.15f, std::sin(t * 0.05f) > 0.3f ? 1.0f : 0.0f}; // idle breathing

    float sy = P.squash * (air && !P.rollT ? 1 + clampf(-m.vy * 0.03f, -0.1f, 0.12f) : 1);
    if (P.rollT) sy *= 0.75f;
    float sx = 1.0f / std::sqrt(sy);
    float bx = std::floor(m.x) + m.w * 0.5f - camX, by = std::floor(m.y) + m.h - camY; // whole-pixel anchor, no shimmer
    auto off = [&](float ang, float len) { return Vector2{f * std::sin(ang) * len * sx, std::cos(ang) * len * sy}; };

    // full-height proportions: long legs, torso, modest head
    Vector2 hip = {bx, by - 8.2f * sy + ps.bob};
    if (P.crouch && !P.prone) // knees bent: the hips sit as high as the legs reach
        hip.y = by - std::max(std::cos(ps.fThigh) + std::cos(ps.fShin), std::cos(ps.bThigh) + std::cos(ps.bShin)) * 4.1f * sy;
    if (P.prone) { hip.y = by - 2.5f; hip.x = bx - f * 4; }
    Vector2 shoulder = {hip.x + f * std::sin(ps.lean) * 6 * sx, hip.y - std::cos(ps.lean) * 6 * sy};

    simulateCape(P, {shoulder.x + camX - f * 2.0f, shoulder.y + camY + 0.5f}, f);
    if (m.iframes > 0 && P.rollT == 0 && (G.frame / 3) % 2) return; // hurt blink

    Look k = playerLook();
    canvasBegin((int)std::floor(bx) - CW / 2, (int)std::floor(by) - CH + 16);

    Color capeC = {150, 28, 40, 255};
    for (int i = 0; i < 6; i++)
    {
        Color c = lerpColor(capeC, mul(capeC, 0.55f), i / 6.0f);
        Vector2 a = {P.cape[i].x - camX, P.cape[i].y - camY}, b = {P.cape[i + 1].x - camX, P.cape[i + 1].y - camY};
        int w = i < 2 ? 3 : (i < 4 ? 2 : 1);
        for (int j = 0; j < w; j++) seg({a.x, a.y + j}, {b.x, b.y + j}, j == w - 1 ? mul(c, 0.8f) : c);
    }

    if (P.rollT > 0)
    {
        rotA = f * (1 - P.rollT / 20.0f) * 2 * PI;
        rotCx = bx;
        rotCy = by - 10;
    }

    if (hasWeapon && !drawn && P.rollT == 0 && !P.prone) drawStowed(*wpn, shoulder, hip, f);

    auto leg = [&](float th, float sh, Color pc) {
        Vector2 kn = add(hip, off(th, 4.1f));
        Vector2 ft = add(kn, off(sh, 4.1f));
        thick(hip, kn, 2, pc, f);
        thick(kn, ft, 2, mul(pc, 0.92f), f);
        for (int i = -1; i <= 1; i++) px(ft.x + i * f, ft.y, k.boot);
        px(ft.x, ft.y - 1, mul(k.boot, 1.15f));
        px(ft.x - f, ft.y - 1, mul(k.boot, 0.8f));
    };
    auto arm = [&](Vector2 sh, float ang, Color sleeve, Color sleeveD) {
        Vector2 el = add(sh, off(ang, 3.2f));
        Vector2 hand = add(el, off(ang * 0.9f, 2.6f));
        thick(sh, el, 2, sleeve, f);
        seg(el, hand, sleeveD);
        px(hand.x, hand.y, k.skin);
        px(hand.x + f, hand.y, k.skinD);
        return hand;
    };

    arm({shoulder.x - f * 2.5f, shoulder.y + 0.5f}, ps.backArm, k.sleeveD, mul(k.sleeveD, 0.85f));
    leg(ps.bThigh, ps.bShin, mul(k.pants, 0.75f));

    // torso: tunic (or armour), gold belt
    for (int r = 0; r <= 6; r++)
    {
        float tt = r / 6.0f;
        float cx = shoulder.x + (hip.x - shoulder.x) * tt, cy = shoulder.y + (hip.y - shoulder.y) * tt;
        int half = r < 1 ? 3 : 2;
        for (int c = -half; c <= half; c++)
        {
            Color col = c == (int)(half * f) ? mul(k.tunic, 1.15f) : (c == -(int)(half * f) ? k.tunicD : k.tunic);
            px(cx + c, cy, col);
        }
    }
    for (int c = -2; c <= 2; c++) px(hip.x + c, hip.y - 1, c == (int)f ? Color{215, 184, 77, 255} : Color{181, 154, 61, 255});
    for (int c = -2; c <= 2; c++) px(hip.x + c * 1.1f, hip.y, k.tunicD); // tunic hem
    if (!k.metal)
    {
        px(shoulder.x + f, shoulder.y + 1, k.tunicD);
        px(shoulder.x, shoulder.y + 1, mul(k.tunic, 1.2f));
    }
    else
    {
        px(shoulder.x + f, shoulder.y + 2, brighten(k.A, 70)); // breastplate shine
        for (int c = 0; c < 3; c++)
        {
            px(shoulder.x + f * (3 + 0.4f * c), shoulder.y - 1 + c, c ? k.A : k.Ah); // pauldrons
            px(shoulder.x - f * 3, shoulder.y + c, k.Ad);
        }
    }

    // head: hair for the hero, a helmet that shows the armour tier otherwise
    const char* const* head = HEAD_HAIR;
    Element el = k.metal ? METALS[P.armour].el : EL_PHYS;
    if (k.metal)
    {
        if (P.armour <= M_IRON) head = HEAD_KETTLE;
        else if (P.armour == M_ADAMANTIUM) head = HEAD_HORNED;
        else if (el != EL_PHYS) head = HEAD_PLUME;
        else if (P.armour == M_DAMASCUS) head = HEAD_CREST;
        else head = HEAD_GREAT;
    }
    Color glow = el != EL_PHYS ? ELEMENT_COLORS[el] : Color{120, 255, 230, 255};
    bool blink = (G.frame % 220) < 6;
    float hx = std::floor(shoulder.x) - 4 + (f > 0 ? 1 : 0) + (P.prone ? f * 3 : 0), hy = std::floor(shoulder.y) - 7 + (P.prone ? 2 : 0);
    for (int j = 0; j < 7; j++)
        for (int i = 0; i < 8; i++)
        {
            char ch = head[j][i];
            if (ch == '.') continue;
            Color c;
            switch (ch)
            {
            case 's': c = k.skin; break;
            case 'S': c = k.skinD; break;
            case 'F': c = blink ? k.skinD : Color{241, 240, 253, 255}; break;
            case 'u': c = blink ? k.skinD : Color{40, 93, 170, 255}; break;
            case 'h': c = k.hairD; break;
            case 'H': c = k.hairL; break;
            case 'A': c = k.A; break;
            case 'D': c = k.Ad; break;
            case 'L': c = k.Ah; break;
            case 'v': c = {18, 18, 26, 255}; break;
            case 'g': c = (G.frame / 8) % 6 ? glow : WHITE; break;
            case 'p': c = (j + G.frame / 6) % 3 ? glow : mul(glow, 0.7f); break;
            case 'n': c = {232, 226, 206, 255}; break;
            default: continue;
            }
            px(hx + (f > 0 ? i : 7 - i), hy + j, c);
        }

    leg(ps.fThigh, ps.fShin, k.pants);

    // front arm: swings while unarmed, lowers a drawn blade, aims ranged weapons, reaches for the rope
    Vector2 shF = {shoulder.x + f * 2.0f, shoulder.y + 0.5f};
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
        handF = {shF.x + std::cos(aimAng) * 4.5f, shF.y + std::sin(aimAng) * 4.5f};
        thick(shF, handF, 2, k.sleeve, f);
        px(handF.x, handF.y, k.skin);
        px(handF.x + 1, handF.y, k.skinD);
    }
    else
        handF = arm(shF, ps.frontArm, k.sleeve, k.sleeveD);

    rotA = 0;
    canvasEnd(m.hurtFlash > 0);
    if (weaponInHand) drawHeld(handF.x, handF.y);
}

// ---------------------------------------------------------------- creature sprites

// Optional Scale2x (EPX), then rim light and a dark outline.
struct BigSprite { int w = 0, h = 0; std::vector<Color> px; };

static const BigSprite& bigOf(const Sprite& s, Color tint, bool epx)
{
    static std::map<std::tuple<const void*, unsigned, bool>, BigSprite> cache;
    auto key = std::make_tuple((const void*)&s, (unsigned)ColorToInt(tint), epx);
    auto it = cache.find(key);
    if (it != cache.end()) return it->second;

    int w = spriteWidth(s), h = s.h;
    bool wolfArt = (&s == &SPR_WOLF_A || &s == &SPR_WOLF_B) && ColorToInt(tint) != ColorToInt(WHITE);
    auto at = [&](int x, int y) -> char { return (x < 0 || y < 0 || x >= w || y >= h) ? '.' : s.rows[y][x]; };
    int k = epx ? 2 : 1, W2 = w * k, H2 = h * k;
    std::vector<char> g(W2 * H2);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
        {
            char P = at(x, y);
            if (!epx) { g[y * W2 + x] = P; continue; }
            char A = at(x, y - 1), B = at(x + 1, y), C = at(x - 1, y), D = at(x, y + 1);
            g[(2 * y) * W2 + 2 * x] = (C == A && C != D && A != B) ? A : P;
            g[(2 * y) * W2 + 2 * x + 1] = (A == B && A != C && B != D) ? B : P;
            g[(2 * y + 1) * W2 + 2 * x] = (D == C && D != B && C != A) ? C : P;
            g[(2 * y + 1) * W2 + 2 * x + 1] = (B == D && B != A && D != C) ? D : P;
        }
    BigSprite b;
    b.w = W2 + 2;
    b.h = H2 + 2;
    b.px.assign(b.w * b.h, BLANK);
    auto G2 = [&](int x, int y) -> char { return (x < 0 || y < 0 || x >= W2 || y >= H2) ? '.' : g[y * W2 + x]; };
    for (int y = -1; y <= H2; y++)
        for (int x = -1; x <= W2; x++)
        {
            char ch = G2(x, y);
            Color c;
            if (ch == '.')
            {
                bool edge = G2(x - 1, y) != '.' || G2(x + 1, y) != '.' || G2(x, y - 1) != '.' || G2(x, y + 1) != '.';
                if (!edge) continue;
                c = OUTLINE;
            }
            else
            {
                c = paletteColor(ch, tint);
                if (ch == 'n' && wolfArt) c = tint; // wolf coats: 'n' fur takes the tint, 'N' a darker shade of it
                if (ch == 'N' && wolfArt) c = {(unsigned char)(tint.r * 0.66f), (unsigned char)(tint.g * 0.66f), (unsigned char)(tint.b * 0.66f), 255};
                if (G2(x, y - 1) == '.' || G2(x - 1, y) == '.') c = brighten(c, 24);
                else if (G2(x, y + 1) == '.' || G2(x + 1, y) == '.') c = brighten(c, -20);
            }
            b.px[(y + 1) * b.w + (x + 1)] = c;
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
            bool outline = c.r == OUTLINE.r && c.g == OUTLINE.g && c.b == OUTLINE.b;
            if (flash.a && !outline) c = Color{flash.r, flash.g, flash.b, c.a};
            c.a = (unsigned char)(c.a * alpha);
            DrawRectangle((int)std::floor(X), (int)std::floor(Y), rw, rh, c);
        }
}

void drawSpriteNative(const Sprite& s, float x, float bottom, bool flip)
{
    drawBig(bigOf(s, WHITE, false), x, bottom, flip, 1, 1, 0, BLANK, 1);
}

void drawSpriteBig(const Sprite& s, float x, float bottom, bool flip, Color tint)
{
    drawBig(bigOf(s, tint, true), x, bottom, flip, 1, 1, 0, BLANK, 1);
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
    return bigOf(enemySprite(m), m.type == E_WOLF ? coats[(m.id * 7) % 5] : WHITE, m.boss);
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
            if (c.a < 200 || (c.r == OUTLINE.r && c.g == OUTLINE.g)) continue;
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
            Color o = OUTLINE;
            o.a = (unsigned char)(255 * alpha);
            c.a = (unsigned char)(255 * alpha);
            DrawLineEx(P(a), P(b), w + 2, o);
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
        Color o = OUTLINE, hc = r.head;
        o.a = hc.a = (unsigned char)(255 * alpha);
        DrawCircleV(h, 3.2f * s + 1, o);
        DrawCircleV(h, 3.2f * s, hc);
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
    case E_GOBLIN: head = {98, 162, 62, 255}; body = {131, 88, 56, 255}; limb = {98, 162, 62, 255}; break;
    case E_BOMBER: head = {214, 52, 52, 255}; body = {131, 88, 56, 255}; limb = {98, 162, 62, 255}; break;
    case E_SKELETON: head = body = limb = {236, 232, 214, 255}; bony = true; break;
    case E_ARCHER: head = {236, 232, 214, 255}; body = {124, 58, 156, 255}; limb = {236, 232, 214, 255}; bony = true; break;
    case E_CULTIST: head = {70, 30, 98, 255}; body = {124, 58, 156, 255}; limb = {124, 58, 156, 255}; break;
    case E_KNIGHT: head = {226, 232, 242, 255}; body = {182, 190, 204, 255}; limb = {104, 112, 130, 255}; break;
    case E_IMP: head = body = limb = {242, 128, 36, 255}; break;
    case E_GOLEM: head = body = limb = {132, 128, 122, 255}; break;
    case E_BLACKKNIGHT: head = {104, 112, 130, 255}; body = {70, 76, 92, 255}; limb = {50, 54, 66, 255}; break;
    case E_LICH: head = {236, 232, 214, 255}; body = {124, 58, 156, 255}; limb = {70, 30, 98, 255}; bony = true; break;
    case E_REDCAP: head = {214, 52, 52, 255}; body = {131, 88, 56, 255}; limb = {255, 209, 156, 255}; break;
    case E_DRAUGR: head = {104, 112, 130, 255}; body = {132, 128, 122, 255}; limb = {110, 168, 222, 255}; break;
    case E_TROLL: head = body = {98, 162, 62, 255}; limb = {56, 110, 44, 255}; break;
    case E_GUARD: head = {182, 190, 204, 255}; body = {214, 52, 52, 255}; limb = {146, 153, 164, 255}; break;
    default: burstSprite(m); return;
    }
    spawnRagdoll(m.cx(), m.y + m.h, (float)m.h, m.vx * 0.8f + frange(-0.5f, 0.5f), std::min(m.vy, 0.0f) - frange(0.5f, 1.5f), head, body, limb, bony, ENEMIES[m.type].gore);
}

void ragdollForPlayer()
{
    Mob& m = G.p.m;
    Look k = playerLook();
    spawnRagdoll(m.cx(), m.y + m.h, (float)m.h, m.vx + frange(-0.5f, 0.5f), m.vy - 1.5f, k.metal ? k.A : k.hairD, k.tunic, k.pants, false, CellMaterial::Blood);
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
    // every stroke gets a dark outline underneath to match the sprites
    auto line = [&](Vector2 p0, Vector2 p1, float th, Color c) {
        DrawLineEx(p0, p1, th + 2, OUTLINE);
        DrawLineEx(p0, p1, th, c);
    };
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
            DrawCircleV(hd, 4.2f, OUTLINE);
            DrawCircleV(hd, 3.2f, mc);
            DrawCircleV({hd.x - 1, hd.y - 1}, 1.2f, brighten(mc, 60));
        }
        else if (w.type == W_PAN)
        {
            line(pt(a, -2), pt(a, L), 1.5f, brown);
            Vector2 hd = pt(a, L + 3.5f);
            DrawCircleV(hd, 5.0f, OUTLINE);
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
        DrawRectangle((int)g.x - 2, (int)g.y - 2, 4, 4, OUTLINE);
        DrawRectangle((int)g.x - 1, (int)g.y - 2, 2, 4, gem);
        DrawRectangle((int)g.x - 2, (int)g.y - 1, 4, 2, gem);
        DrawRectangle((int)g.x - 1, (int)g.y - 1, 1, 1, WHITE);
    }
}

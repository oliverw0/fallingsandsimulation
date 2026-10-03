// Character art and animation:
//  - the player is a jointed rig drawn into a small pixel canvas
//  - creature sprites are hand-shaded with no outline (bosses are drawn at 2x)
//  - ragdolls: verlet stick figures that tumble through the falling-sand world
#include "game.h"
#include "sprites.h"
#include "sprites_hd.h"
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

void detail2x(const Color* src, int w, int h, std::vector<Color>& out);
// Whose sprite is being drawn, for what it's covered in (set around drawBig / canvasEnd).
static const Mob* coatMob = nullptr;
// Blood in splashes, oil as a dark slick with a rainbow sheen, water as a cold darkening with glints, and
// fire as a flickering orange glare from below. x, y: the pixel in the sprite; h: its height in pixels.
static Color coat(Color c, const Mob& m, int x, int y, int h)
{
    int f = G.frame;
    if (m.bloody)
    {
        float k = std::min(1.0f, m.bloody / 240.0f), low = (float)y / h;
        if (vnoise(x * 0.35f + m.id * 7.3f, y * 0.35f, 77) < k * (0.22f + 0.3f * low)) c = lerpColor(c, Color{128, 12, 18, c.a}, 0.75f); // splashes, heavier low down
    }
    if (m.oily)
    {
        float k = std::min(1.0f, m.oily / 240.0f);
        c = lerpColor(c, Color{26, 22, 16, c.a}, 0.5f * k);
        if (hash2(x, y / 2 + m.id, 5 + f / 8) > 1 - 0.05f * k) { Color sh = ColorFromHSV((float)((x * 9 + y * 5 + f * 3) % 360), 0.55f, 0.75f); sh.a = c.a; c = sh; }
    }
    if (m.wet)
    {
        float k = std::min(1.0f, m.wet / 180.0f);
        c = lerpColor(c, Color{60, 96, 160, c.a}, 0.28f * k);
        if (hash2(x, y, 11 + f / 6) > 1 - 0.03f * k) c = Color{210, 232, 255, c.a};
    }
    if (m.burn)
    {
        float fl = 0.5f + 0.5f * std::sin(f * 0.6f + x * 0.9f + y * 0.4f), low = (float)y / h;
        c = lerpColor(c, fl > 0.6f ? Color{255, 220, 110, c.a} : Color{255, 110, 30, c.a}, 0.25f + 0.3f * fl * (1 - low * 0.5f));
    }
    return c;
}

static void canvasEnd(bool flash)
{
    static std::vector<Color> fine;
    detail2x(canvas, CW, CH, fine); // drawn at half a unit a pixel, like everything else
    for (int y = 0; y < CH * 2; y++)
        for (int x = 0; x < CW * 2; x++)
        {
            Color c = fine[(size_t)y * CW * 2 + x];
            if (!c.a) continue;
            if (flash) c = WHITE;
            else if (coatMob) c = coat(c, *coatMob, x, y, CH * 2);
            DrawRectangleRec({cox + x * 0.5f, coy + y * 0.5f, 0.5f, 0.5f}, c);
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
    const float SEG = 1.35f; // twelve links: down to the calves
    if (!P.capeInit || std::hypot(P.cape[0].x - anchor.x, P.cape[0].y - anchor.y) > 30)
    {
        for (int i = 0; i < CAPE_N; i++) P.cape[i] = P.capePrev[i] = {anchor.x - f * i * 0.3f, anchor.y + i * SEG};
        P.capeInit = true;
    }
    P.cape[0] = P.capePrev[0] = anchor;
    // Gentle forces only: gravity always wins, so it hangs, trails behind as you move, and ripples a little.
    float grav = m.inLiquid ? 0.025f : 0.14f, damp = m.inLiquid ? 0.7f : 0.78f, run = std::min(std::fabs(m.vx), 2.0f);
    for (int i = 1; i < CAPE_N; i++)
    {
        float t = (float)i / (CAPE_N - 1);
        // damp only the cloth's motion relative to you: damping its speed through the world would act like a gale whenever you ran
        Vector2 v = {clampf((P.cape[i].x - P.capePrev[i].x - m.vx) * damp, -1.0f, 1.0f), clampf((P.cape[i].y - P.capePrev[i].y - m.vy) * damp, -1.0f, 1.0f)};
        v.x += m.vx; v.y += m.vy; // (and a jolt - a stop, a turn, a landing - can't fling it)
        P.capePrev[i] = P.cape[i];
        float ripple = std::sin(G.frame * 0.11f + i * 0.7f) * 0.012f * (1 + run) * t;
        P.cape[i].x += v.x - m.vx * 0.035f * t + ripple; // the air you run through pushes it back
        P.cape[i].y += v.y + grav - run * 0.03f * t; // a run lifts the hem, never past level
    }
    for (int it = 0; it < 4; it++) // links keep their length
        for (int i = 1; i < CAPE_N; i++)
        {
            float dx = P.cape[i].x - P.cape[i - 1].x, dy = P.cape[i].y - P.cape[i - 1].y;
            float d = std::sqrt(dx * dx + dy * dy);
            if (d > 0.01f) { dx = dx / d * SEG; dy = dy / d * SEG; }
            if (dy < -0.1f * SEG) // cloth hangs: no link rises past level with the one above it, so it never whips over your head
            {
                dy = -0.1f * SEG;
                dx = (dx < 0 ? -1 : 1) * std::sqrt(SEG * SEG - dy * dy);
            }
            P.cape[i].x = P.cape[i - 1].x + dx;
            P.cape[i].y = P.cape[i - 1].y + dy;
        }
    for (int i = 1; i < CAPE_N; i++) // and rest on the ground rather than in it, without bouncing off it
    {
        float y0 = P.cape[i].y;
        for (int k = 0; k < 4 && isSolid((int)std::floor(P.cape[i].x), (int)std::floor(P.cape[i].y)); k++) P.cape[i].y = std::floor(P.cape[i].y) - 0.01f;
        if (P.cape[i].y != y0) P.capePrev[i] = {P.capePrev[i].x * 0.5f + P.cape[i].x * 0.5f, P.cape[i].y}; // friction, and no rebound
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

    { // pinned to the body, not the posed shoulder: a landing squash or a stride's bob would yank it about every frame
        static float shoulderH = 14; // the shoulder's height above the feet, eased
        shoulderH += (by - shoulder.y - shoulderH) * 0.3f;
        simulateCape(P, {std::floor(m.x) + m.w * 0.5f - f * 2.0f, std::floor(m.y) + m.h - shoulderH + 0.5f}, f);
    }
    if (m.iframes > 0 && P.rollT == 0 && (G.frame / 3) % 2) return; // hurt blink

    Look k = playerLook();
    canvasBegin((int)std::floor(bx) - CW / 2, (int)std::floor(by) - CH + 16);

    // the cloak: a great sweep of cloth, flaring from the shoulders to a gold-trimmed hem, its lining showing beneath
    Color lining = mul(k.cloak, 0.45f);
    auto outward = [&](Vector2 a, Vector2 b) { // the cloth's thickness: always the same side of the line, turning smoothly with it
        float dx = b.x - a.x, dy = b.y - a.y, d = std::sqrt(dx * dx + dy * dy) + 0.001f;
        return Vector2{-dy / d * f, dx / d * f}; // hanging straight down, that's behind you
    };
    auto cloth = [&](Vector2 a, Vector2 b, Color c) { // like seg, but cloth never shows inside the ground it rests on
        int n = std::max(1, (int)std::ceil(std::max(std::fabs(b.x - a.x), std::fabs(b.y - a.y)) * 1.5f));
        for (int i = 0; i <= n; i++)
        {
            float x = a.x + (b.x - a.x) * i / n, y = a.y + (b.y - a.y) * i / n;
            if (!isSolid((int)std::floor(x + camX), (int)std::floor(y + camY))) px(x, y, c);
        }
    };
    for (int i = 0; i < CAPE_N - 1; i++)
    {
        float t = (float)i / (CAPE_N - 1);
        Vector2 a = {P.cape[i].x - camX, P.cape[i].y - camY}, b = {P.cape[i + 1].x - camX, P.cape[i + 1].y - camY};
        Vector2 n = outward(a, b);
        float w0 = 2.5f + t * 4.5f, w1 = 2.5f + (t + 1.0f / (CAPE_N - 1)) * 4.5f; // the depth of the folds
        int layers = (int)std::ceil(std::max(w0, w1) / 0.7f);
        for (int j = 0; j <= layers; j++)
        {
            float u = (float)j / layers;
            Color c = lerpColor(k.cloak, mul(k.cloak, 0.6f), t);
            if (j == 0) c = brighten(c, 16);                              // the crease the light catches
            else if (u > 0.8f) c = lining;                               // the lining, underneath
            else if (((int)(i * 0.7f + u * 3)) % 2) c = mul(c, 0.86f);   // folds
            cloth({a.x + n.x * w0 * u, a.y + n.y * w0 * u}, {b.x + n.x * w1 * u, b.y + n.y * w1 * u}, c);
        }
        if (i >= CAPE_N - 3) cloth({a.x + n.x * w0, a.y + n.y * w0}, {b.x + n.x * w1, b.y + n.y * w1}, k.trim); // gilded hem
    }
    {
        Vector2 e0 = {P.cape[CAPE_N - 1].x - camX, P.cape[CAPE_N - 1].y - camY}, e1 = {P.cape[CAPE_N - 2].x - camX, P.cape[CAPE_N - 2].y - camY};
        Vector2 n = outward(e1, e0);
        cloth(e0, {e0.x + n.x * 7.0f, e0.y + n.y * 7.0f}, k.trim); // the hem's edge
    }
    for (int c = -2; c <= 3; c++) // a fur mantle across the shoulders, where the cape is pinned
        for (int r = 0; r < 2; r++)
            px(P.cape[0].x - camX + c * f * 0.8f - f, P.cape[0].y - camY - 1 + r, (c + r) % 2 ? k.fur : mul(k.fur, 0.78f));
    px(P.cape[0].x - camX + f * 2, P.cape[0].y - camY, k.trim); // the brooch

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
    float aimAng = P.aim, reach = 0;
    if (weaponInHand && isMelee(wpn->type))
    {
        if (P.swingT > 0)
        {
            float ext;
            attackPose(aimAng, ext, 0);
            reach = P.atkStyle == ATK_STAB ? ext : std::min(ext, 2.5f); // the arm drives the blow
        }
        else
            aimAng = f > 0 ? 1.0f : PI - 1.0f; // relaxed, blade lowered in front
    }
    if (P.hook == 2) { aimAng = std::atan2(P.hy - camY - shF.y, P.hx - camX - shF.x); weaponInHand = false; }
    if (weaponInHand || P.hook == 2)
    {
        Vector2 el = {shF.x + std::cos(aimAng) * 3.0f, shF.y + std::sin(aimAng) * 3.0f};
        handF = {shF.x + std::cos(aimAng) * (5.5f + reach), shF.y + std::sin(aimAng) * (5.5f + reach)};
        thick(shF, el, 2, k.set == AS_WOOL || k.set == AS_LEATHER ? mul(k.tunic, 1.05f) : k.A, f);
        thick(el, handF, 2, k.bracer, f);
        px(handF.x, handF.y, k.skin);
        px(handF.x + 1, handF.y, k.skinD);
    }
    else
        handF = arm(shF, ps.frontArm, 1.0f);

    rotA = 0;
    coatMob = (m.wet || m.oily || m.bloody || m.burn) ? &m : nullptr;
    canvasEnd(m.hurtFlash > 0);
    coatMob = nullptr;
    if (weaponInHand) drawHeld(handF.x, handF.y);
}

// ---------------------------------------------------------------- detail at twice the resolution
// Sprites are designed a pixel per world unit; the world is drawn at two cells per unit. To match it they're
// doubled with Scale2x (which keeps every design but rounds off its stair-steps), then given light from
// the upper left - a lit rim along edges facing it, shade along edges facing away - and a gentle fall-off
// from head to foot, and drawn at half a unit per pixel.
void detail2x(const Color* src, int w, int h, std::vector<Color>& out)
{
    auto at = [&](int x, int y) -> Color { return (x < 0 || y < 0 || x >= w || y >= h) ? BLANK : src[y * w + x]; };
    auto eq = [](Color a, Color b) { return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a; };
    int W = w * 2, H = h * 2;
    std::vector<Color> up((size_t)W * H, BLANK);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
        {
            Color P = at(x, y), A = at(x, y - 1), B = at(x + 1, y), C = at(x - 1, y), D = at(x, y + 1);
            Color q[4] = {P, P, P, P};
            if (eq(C, A) && !eq(C, D) && !eq(A, B)) q[0] = A;
            if (eq(A, B) && !eq(A, C) && !eq(B, D)) q[1] = B;
            if (eq(D, C) && !eq(D, B) && !eq(C, A)) q[2] = C;
            if (eq(B, D) && !eq(B, A) && !eq(D, C)) q[3] = D;
            for (int j = 0; j < 4; j++) up[(size_t)(y * 2 + j / 2) * W + x * 2 + j % 2] = q[j];
        }
    out.assign(up.size(), BLANK);
    auto clear = [&](int x, int y) { return x < 0 || y < 0 || x >= W || y >= H || up[(size_t)y * W + x].a == 0; };
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
        {
            Color c = up[(size_t)y * W + x];
            if (!c.a) continue;
            float k = 1.04f - 0.1f * y / H;
            if (clear(x, y - 1)) k += 0.14f;      // a lit rim along the top
            else if (clear(x - 1, y)) k += 0.07f; // and the left
            if (clear(x, y + 1)) k -= 0.18f;      // shade along the bottom
            else if (clear(x + 1, y)) k -= 0.1f;  // and the right
            out[(size_t)y * W + x] = {(unsigned char)std::min(255.0f, c.r * k), (unsigned char)std::min(255.0f, c.g * k), (unsigned char)std::min(255.0f, c.b * k), c.a};
        }
}

// ---------------------------------------------------------------- creature sprites

// Sprite pixels, optionally enlarged 2x: Scale2x (EPX) smooths the simple object icons,
// plain doubling keeps the hand-shaded boss art crisp.
struct BigSprite { int w = 0, h = 0; float unit = 1; std::vector<Color> px; }; // unit: world cells per sprite pixel
enum { SC_NATIVE, SC_EPX, SC_DOUBLE, SC_FINE, SC_FINE2 }; // FINE: detail2x at half a unit a pixel; FINE2: twice over, at the size of DOUBLE

static const BigSprite& bigOf(const Sprite& s, Color tint, int mode)
{
    static std::map<std::tuple<const void*, unsigned, int>, BigSprite> cache;
    auto key = std::make_tuple((const void*)&s, (unsigned)ColorToInt(tint), mode);
    auto it = cache.find(key);
    if (it != cache.end()) return it->second;

    int w = spriteWidth(s), h = s.h;
    auto at = [&](int x, int y) -> char { return (x < 0 || y < 0 || x >= w || y >= h) ? '.' : s.rows[y][x]; };
    if (mode == SC_FINE || mode == SC_FINE2)
    {
        BigSprite b;
        std::vector<Color> base((size_t)w * h, BLANK), out;
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++)
                if (at(x, y) != '.') base[(size_t)y * w + x] = paletteColor(at(x, y), tint);
        detail2x(base.data(), w, h, out);
        b.w = w * 2; b.h = h * 2;
        if (mode == SC_FINE2) // twice over: four times the pixels, drawn at the doubled size
        {
            std::vector<Color> again;
            detail2x(out.data(), b.w, b.h, again);
            out.swap(again);
            b.w *= 2; b.h *= 2;
        }
        b.px = out;
        b.unit = 0.5f;
        return cache[key] = b;
    }
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

// The detailed character art (tools/art_hd.py), drawn at half a world cell per pixel.
static const BigSprite& bigOfHD(const HDSprite& s)
{
    static std::map<const void*, BigSprite> cache;
    auto it = cache.find(&s);
    if (it != cache.end()) return it->second;
    BigSprite b;
    b.w = s.w;
    b.h = s.h;
    b.unit = 0.5f;
    b.px.assign(b.w * b.h, BLANK);
    for (int y = 0; y < s.h; y++)
        for (int x = 0; x < s.w; x++)
        {
            char ch = s.rows[y][x];
            if (ch != '.') b.px[y * b.w + x] = s.pal[std::strchr(HD_ALPHABET, ch) - HD_ALPHABET];
        }
    return cache[&s] = b;
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
            float u = b.unit, lx = (si - b.w * 0.5f) * sxs * u, ly = (j - b.h + 1) * sys * u;
            float X = ax + lx + lean * (-ly) / (b.h * u) * 4.0f, Y = ay + ly;
            if (flash.a) c = Color{flash.r, flash.g, flash.b, c.a};
            else if (coatMob) c = coat(c, *coatMob, si, j, b.h);
            c.a = (unsigned char)(c.a * alpha);
            if (u >= 1) DrawRectangle((int)std::floor(X), (int)std::floor(Y), rw, rh, c);
            else // snapped to the half-cell grid of the scene texture
                DrawRectangleRec({std::floor(X * 2) / 2, std::floor(Y * 2) / 2, std::max(u, std::ceil(sxs * 2 * u) / 2), std::max(u, std::ceil(sys * 2 * u) / 2)}, c);
        }
}

// Fire you can't miss: a glare round the body and pixel flames licking up off it, taller where the oil is.
void drawBurning(const Mob& m, int camX, int camY)
{
    float x0 = std::floor(m.x) - camX, top = std::floor(m.y) - camY, h = (float)m.h, f = (float)G.frame;
    BeginBlendMode(BLEND_ADDITIVE);
    float pulse = 0.85f + 0.15f * std::sin(f * 0.3f);
    DrawCircleGradient((int)(x0 + m.w / 2.0f), (int)(top + h * 0.45f), h * 0.95f * pulse, {255, 120, 30, 70}, {255, 80, 20, 0});
    EndBlendMode();
    float reach = h * (m.oily ? 0.75f : 0.5f);
    for (float cx = -1; cx <= m.w + 1; cx += 0.5f) // a column of flame per half unit
    {
        float n = vnoise(cx * 0.9f + m.id, f * 0.22f, 31), fh = reach * (0.35f + 0.65f * n) * (1 - std::fabs(cx - m.w / 2.0f) / (m.w / 2.0f + 2) * 0.6f);
        float base = top + h * 0.55f; // licking up from the waist
        for (float y = 0; y < fh + h * 0.4f; y += 0.5f)
        {
            float t = y / (fh + h * 0.4f); // 0 at the root, 1 at the tip
            if (hash2((int)(cx * 2) + m.id * 7, (int)(y * 2) - (int)(f * 1.5f), 13) > 1.1f - t) continue; // ragged, flickering edges
            Color c = t < 0.25f ? Color{255, 246, 190, 230} : (t < 0.55f ? Color{255, 190, 60, 220} : (t < 0.8f ? Color{240, 100, 24, 200} : Color{170, 40, 20, 150}));
            DrawRectangleRec({x0 + cx, base - y, 0.5f, 0.5f}, c);
        }
    }
}

// The chest art as textures, so it can tumble.
void drawChest(float cx, float cy, float ang, bool open)
{
    static Texture2D tex[2] = {};
    int k = open ? 1 : 0;
    if (!tex[k].id)
    {
        const HDSprite& s = open ? HD_CHEST_OPEN : HD_CHEST;
        Image img = GenImageColor(s.w, s.h, BLANK);
        for (int y = 0; y < s.h; y++)
            for (int x = 0; x < s.w; x++)
                if (s.rows[y][x] != '.') ImageDrawPixel(&img, x, y, s.pal[std::strchr(HD_ALPHABET, s.rows[y][x]) - HD_ALPHABET]);
        tex[k] = LoadTextureFromImage(img);
        UnloadImage(img);
    }
    const Texture2D& t = tex[k];
    float w = t.width * 0.5f, h = t.height * 0.5f; // half a unit a pixel, like the rest of the fine art
    DrawTexturePro(t, {0, 0, (float)t.width, (float)t.height}, {cx, cy, w, h}, {w / 2, h / 2}, ang * RAD2DEG, WHITE);
}

void drawSpriteNative(const Sprite& s, float x, float bottom, bool flip)
{
    drawBig(bigOf(s, WHITE, SC_FINE), x, bottom, flip, 1, 1, 0, BLANK, 1);
}

void drawSpriteTint(const Sprite& s, float x, float bottom, bool flip, Color tint)
{
    drawBig(bigOf(s, tint, SC_FINE), x, bottom, flip, 1, 1, 0, BLANK, 1);
}

void drawSpriteBig(const Sprite& s, float x, float bottom, bool flip, Color tint)
{
    drawBig(bigOf(s, tint, SC_FINE2), x, bottom, flip, 1, 1, 0, BLANK, 1);
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
    case E_RISEN: return alt ? SPR_RISEN_B : SPR_RISEN_A;
    default: return SPR_LICH;
    }
}

// Enemies drawn with detailed art from tools/art_hd.py (half a world cell per pixel). None use it yet;
// to try one, map it here, e.g.  case E_GUARD: return alt ? &HD_WARRIOR_AXE_B : &HD_WARRIOR_AXE_A;
static const HDSprite* hdArt(const Mob& m)
{
    switch (m.type)
    {
    default: return nullptr;
    }
}

static const BigSprite& mobArt(const Mob& m)
{
    if (const HDSprite* hd = hdArt(m)) return bigOfHD(*hd);
    static const Color coats[5] = {{132, 128, 122, 255}, {128, 92, 60, 255}, {64, 60, 62, 255}, {204, 200, 190, 255}, {156, 98, 54, 255}}; // grey, brown, black, white, russet
    return bigOf(enemySprite(m), m.type == E_WOLF ? coats[(m.id * 7) % 5] : WHITE, m.boss ? SC_FINE2 : SC_FINE);
}

static void bolt(Vector2 at, float ang, float len);

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
    if (m.atkPhase == 1) // winding up: rocked back, coiled, trembling at the last
    {
        float k = 1 - clampf(m.atkT / 15.0f, 0, 1);
        lunge = -m.facing * (1.0f + 2.0f * k) + (m.atkT < 6 ? std::sin(G.frame * 2.1f) * 0.6f : 0);
        lean -= m.facing * 0.25f * k;
        sxs *= 1 + 0.1f * k;
        sys *= 1 - 0.1f * k;
    }
    else if (m.atkPhase == 2) // the strike: thrown forward, stretched
    {
        lunge = m.facing * 3.5f;
        lean += m.facing * 0.3f;
        sxs *= 1.15f;
        sys *= 0.9f;
    }
    else if (m.attackT > 0)
    {
        bool wind = m.attackT > 8; // anticipation, then the strike
        lunge = m.facing * (wind ? -2.0f : 4.0f);
        sxs *= wind ? 0.9f : 1.12f;
        sys *= wind ? 1.08f : 0.92f;
    }
    if (m.hurtFlash > 0) { sxs *= 1.1f; sys *= 0.9f; }
    float recoil = 0;
    if (m.hitT > 0) // struck: snapped back the way the blow went, a little lift, easing back upright
    {
        float k = m.hitT / 14.0f, e = k * k;
        lean = clampf(lean + m.hitDir * (flying ? 0.35f : 0.6f) * e, -0.9f, 0.9f);
        recoil = m.hitDir * 2.5f * e;
        bob -= std::sin(k * PI) * 1.5f;
        sxs *= 1 + 0.12f * e;
        sys *= 1 - 0.1f * e;
    }

    float ax = std::floor(m.x) + m.w * 0.5f - camX + lunge + recoil, ay = std::floor(m.y) + m.h - camY + bob;
    Color flash = m.hurtFlash > 0 ? WHITE : BLANK;
    if (m.type == E_BLACKKNIGHT && m.state == 2 && (G.frame / 3) % 2) flash = {255, 60, 60, 255};
    coatMob = (m.wet || m.oily || m.bloody || m.burn) ? &m : nullptr;
    drawBig(b, ax, ay, m.facing < 0, sxs, sys, lean, flash, m.type == E_WRAITH ? 0.75f : 1.0f);
    coatMob = nullptr;
    for (int k = 0; k < m.nWounds; k++)
        if (m.woundK[k] == WK_BOLT) bolt({ax + m.woundX[k] * m.facing, ay + m.woundY[k]}, m.facing > 0 ? m.woundA[k] : PI - m.woundA[k], 6);

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
    float u = b.unit, ox = m.cx() - b.w * u / 2.0f, oy = m.y + m.h - b.h * u;
    int step = u < 1 ? 2 : 1; // one particle per world cell
    for (int j = 0; j < b.h; j += step)
        for (int i = 0; i < b.w; i += step)
        {
            Color c = b.px[j * b.w + i];
            if (c.a < 200) continue;
            float x = ox + (m.facing < 0 ? b.w - 1 - i : i) * u, y = oy + j * u;
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

// ---------------------------------------------------------------- corpses
// A creature dies as itself: its own sprite, now a dead weight that falls, tumbles and comes to rest. A
// sword or an axe may cut it in two, a blast tears it into pieces; every cut spurts like a punctured
// waterskin until it runs dry, and stab wounds and stuck bolts go on seeping.

static void bolt(Vector2 at, float ang, float len) // a crossbow bolt stuck in, its shaft and fletching out
{
    Vector2 d = {std::cos(ang), std::sin(ang)}, tail = {at.x - d.x * len, at.y - d.y * len};
    DrawLineEx(at, tail, 1.0f, {120, 86, 52, 255});
    DrawLineEx({tail.x + d.x * 1.5f, tail.y + d.y * 1.5f}, tail, 1.6f, {214, 206, 190, 255});
    DrawRectangle((int)at.x, (int)at.y, 1, 1, {150, 150, 158, 255});
}

static Vector2 rot(Vector2 v, float a) { float c = std::cos(a), s = std::sin(a); return {v.x * c - v.y * s, v.x * s + v.y * c}; }

void clearCorpses()
{
    for (auto& c : G.corpses) if (c.tex.id) UnloadTexture(c.tex);
    G.corpses.clear();
}

void spawnCorpse(const Mob& m)
{
    bool ethereal = m.type == E_SLIME || m.type == E_WRAITH || m.type == E_BANSHEE;
    if (ethereal || !IsWindowReady()) { burstSprite(m); return; }
    const BigSprite& b = mobArt(m);
    const EnemyDef& d = ENEMIES[m.type];
    const float u = b.unit, W = b.w * u, H = b.h * u, bx = m.cx(), by = m.y + m.h;
    auto pos = [&](int k) { return Vector2{(k % b.w + 0.5f) * u - W / 2, (k / b.w + 0.5f) * u - H}; }; // from the feet, as it faced
    std::vector<Color> px(b.px.size());
    for (int j = 0; j < b.h; j++)
        for (int i = 0; i < b.w; i++) px[j * b.w + i] = b.px[j * b.w + (m.facing < 0 ? b.w - 1 - i : i)];
    std::vector<std::vector<int>> pieces(1);
    for (int k = 0; k < (int)px.size(); k++) if (px[k].a > 100) pieces[0].push_back(k);
    if (pieces[0].size() < 8) { burstSprite(m); return; }

    // the cuts: a line through the body (a point on it, and its direction)
    struct Cut { Vector2 p, d; };
    std::vector<Cut> cuts;
    float a = m.lastAng;
    if ((m.lastHit == HK_SLASH && chance(3)) || (m.lastHit == HK_CHOP && chance(2)))
    {
        float ca = m.lastHit == HK_SLASH ? a + frange(-0.35f, 0.35f) : a + PI / 2 + frange(-0.3f, 0.3f); // a slash goes through the waist, a chop splits it
        cuts.push_back({{frange(-W, W) * 0.12f, -H * frange(0.4f, 0.62f)}, {std::cos(ca), std::sin(ca)}});
    }
    if (m.lastHit == HK_BLAST)
        for (int k = irange(2, 4); k > 0; k--) { float ca = frand() * 2 * PI; cuts.push_back({{frange(-W, W) * 0.3f, -H * frange(0.2f, 0.8f)}, {std::cos(ca), std::sin(ca)}}); }
    std::vector<std::vector<Wound>> cutWounds(1);
    for (auto& c : cuts)
    {
        std::vector<std::vector<int>> next;
        std::vector<std::vector<Wound>> nextW;
        for (size_t p = 0; p < pieces.size(); p++)
        {
            std::vector<int> side[2];
            for (int k : pieces[p]) { Vector2 v = pos(k); side[c.d.x * (v.y - c.p.y) - c.d.y * (v.x - c.p.x) > 0].push_back(k); }
            if (side[0].size() < 12 || side[1].size() < 12) { next.push_back(pieces[p]); nextW.push_back(cutWounds[p]); continue; }
            for (int s = 0; s < 2; s++) // each half gets a wound where it was cut, spurting away from itself
            {
                Vector2 sum = {0, 0};
                int n = 0;
                for (int k : side[s])
                {
                    Vector2 v = pos(k);
                    if (std::fabs(c.d.x * (v.y - c.p.y) - c.d.y * (v.x - c.p.x)) < u * 1.6f) { sum.x += v.x; sum.y += v.y; n++; }
                }
                std::vector<Wound> w = cutWounds[p];
                if (n) w.push_back({sum.x / n, sum.y / n, std::atan2(c.d.x, -c.d.y) + (s ? PI : 0), WK_CUT, 1.0f}); // (the side's normal, pointing at the line)
                next.push_back(side[s]);
                nextW.push_back(w);
            }
        }
        pieces = next;
        cutWounds = nextW;
    }

    for (size_t p = 0; p < pieces.size(); p++)
    {
        const std::vector<int>& pc = pieces[p];
        if (pc.size() < 10) // a scrap: just gore
        {
            for (int k : pc) if (chance(2)) { Vector2 v = pos(k); spawnParticle(bx + v.x, by + v.y, frange(-1.5f, 1.5f), frange(-2.5f, -0.5f), irange(30, 60), px[k], 0.15f); }
            continue;
        }
        int i0 = b.w, j0 = b.h, i1 = 0, j1 = 0;
        Vector2 c = {0, 0};
        for (int k : pc) { i0 = std::min(i0, k % b.w); i1 = std::max(i1, k % b.w); j0 = std::min(j0, k / b.w); j1 = std::max(j1, k / b.w); Vector2 v = pos(k); c.x += v.x; c.y += v.y; }
        c.x /= pc.size(); c.y /= pc.size();
        Corpse cp;
        cp.unit = u;
        cp.gore = d.gore;
        int tw = i1 - i0 + 1, th = j1 - j0 + 1;
        std::vector<uint8_t> in((size_t)tw * th, 0);
        Image img = GenImageColor(tw, th, BLANK);
        float I = 0;
        for (int k : pc)
        {
            int i = k % b.w - i0, j = k / b.w - j0;
            in[(size_t)j * tw + i] = 1;
            ((Color*)img.data)[j * tw + i] = px[k];
            Vector2 v = pos(k);
            I += (v.x - c.x) * (v.x - c.x) + (v.y - c.y) * (v.y - c.y);
        }
        cp.I = std::max(1.0f, I / pc.size());
        cp.tex = LoadTextureFromImage(img);
        UnloadImage(img);
        cp.ox = i0 * u - W / 2 - c.x;
        cp.oy = j0 * u - H - c.y;
        std::vector<Vector2> edge; // its outline: pixels with open space beside them
        for (int j = 0; j < th; j++)
            for (int i = 0; i < tw; i++)
            {
                if (!in[(size_t)j * tw + i]) continue;
                bool e = i == 0 || j == 0 || i == tw - 1 || j == th - 1 || !in[(size_t)j * tw + i - 1] || !in[(size_t)j * tw + i + 1] || !in[(size_t)(j - 1) * tw + i] || !in[(size_t)(j + 1) * tw + i];
                if (e) edge.push_back({cp.ox + (i + 0.5f) * u, cp.oy + (j + 0.5f) * u});
            }
        size_t step = std::max<size_t>(1, edge.size() / 40);
        for (size_t k = 0; k < edge.size(); k += step) cp.pts.push_back(edge[k]);
        for (auto w : cutWounds[p]) { w.x -= c.x; w.y -= c.y; cp.wounds.push_back(w); }
        for (int k = 0; k < m.nWounds; k++) // stab wounds and bolts go with whichever piece holds them
        {
            Vector2 wv = {m.woundX[k] * m.facing, m.woundY[k]}, best = {0, 0};
            float bd = 1e9f;
            int owner = -1;
            for (size_t q = 0; q < pieces.size(); q++)
                for (int kk : pieces[q]) { Vector2 v = pos(kk); float dd = (v.x - wv.x) * (v.x - wv.x) + (v.y - wv.y) * (v.y - wv.y); if (dd < bd) { bd = dd; owner = (int)q; best = v; } }
            if (owner == (int)p) cp.wounds.push_back({best.x - c.x, best.y - c.y, m.facing > 0 ? m.woundA[k] : PI - m.woundA[k], m.woundK[k], 0.6f});
        }
        if (cp.wounds.empty() && d.gore == CellMaterial::Blood) cp.wounds.push_back({0, -H * 0.6f - c.y, -PI / 2, WK_PIERCE, 0.45f}); // it bleeds out a while anyway

        // how it goes down
        RigidBody& rb = cp.b;
        rb.x = bx + c.x; rb.y = by + c.y;
        rb.vx = m.vx * 0.8f + frange(-0.3f, 0.3f);
        rb.vy = std::min(m.vy, 0.0f) - frange(0.3f, 1.0f);
        rb.va = (rb.vx >= 0 ? 1 : -1) * frange(0.02f, 0.05f); // it topples
        auto overlaps = [&](float dx) { // a sprite bigger than its body may reach into a wall or a roof: start it clear
            for (auto& q : cp.pts) { Vector2 r = rot(q, rb.ang); if (isSolid((int)std::floor(rb.x + dx + r.x), (int)std::floor(rb.y + r.y))) return true; }
            return false;
        };
        auto clear = [&]() {
            if (!overlaps(0)) return true;
            for (int k = 1; k <= 12; k++)
            {
                if (!overlaps((float)k)) { rb.x += k; return true; }
                if (!overlaps((float)-k)) { rb.x -= k; return true; }
            }
            return false;
        };
        if (!clear() && pieces.size() == 1) // no room to stand: it's already down, lying on the floor
        {
            rb.ang = (rb.vx >= 0 ? 1 : -1) * PI / 2;
            float lo = -1e9f;
            for (auto& q : cp.pts) lo = std::max(lo, rot(q, rb.ang).y);
            rb.y = by - lo - 0.6f;
            rb.va = 0;
            clear();
        }
        Vector2 dir = {std::cos(a), std::sin(a)};
        if (m.lastHit == HK_BLUNT) // a hammer's blow sends the body flying
        {
            rb.vx = dir.x * 4.5f + m.vx * 0.3f; rb.vy = std::min(dir.y * 4.0f, 0.0f) - 3.0f; rb.va = (dir.x >= 0 ? 1 : -1) * frange(0.15f, 0.3f);
        }
        else if (m.lastHit == HK_BLAST)
        {
            float l = std::sqrt(c.x * c.x + (c.y + H * 0.5f) * (c.y + H * 0.5f)) + 0.5f, f = (1.5f + m.lastK * 0.3f) * frange(0.7f, 1.4f);
            Vector2 out = {dir.x + c.x / l * 0.8f, dir.y + (c.y + H * 0.5f) / l * 0.8f};
            rb.vx = out.x * f; rb.vy = out.y * f - f * 0.5f; rb.va = frange(-0.4f, 0.4f);
        }
        else if (!cuts.empty() && pieces.size() > 1) // cut through: the halves part along the blow
        {
            Vector2 n = {-cuts[0].d.y, cuts[0].d.x};
            float s = (c.x - cuts[0].p.x) * n.x + (c.y - cuts[0].p.y) * n.y > 0 ? 1.0f : -1.0f;
            rb.vx += dir.x * 1.2f + n.x * s * 0.7f; rb.vy += n.y * s * 0.7f - 0.6f; rb.va = s * frange(0.05f, 0.12f);
        }
        G.corpses.push_back(std::move(cp));
    }
    while (G.corpses.size() > 40) // the oldest of the dead are cleared away
    {
        if (G.corpses.front().tex.id) UnloadTexture(G.corpses.front().tex);
        G.corpses.erase(G.corpses.begin());
    }
}

void updateCorpses()
{
    for (auto& c : G.corpses)
    {
        RigidBody& b = c.b;
        if (std::fabs(b.x - G.camX - G.vw / 2) > G.vw + 300 || std::fabs(b.y - G.camY - G.vh / 2) > G.vh + 300) continue;
        c.life++;
        if (b.rest > 40)
        {
            if ((G.frame + c.life) % 10 == 0) // asleep: is the ground still under it?
            {
                bool held = false;
                for (auto& p : c.pts) { Vector2 r = rot(p, b.ang); held = held || isSolid((int)std::floor(b.x + r.x), (int)std::floor(b.y + r.y + 1.2f)); }
                if (!held) b.rest = 0;
            }
        }
        else rigidStep(b, c.pts, c.I, false);
        if (c.gore == CellMaterial::Empty) continue;
        for (auto& w : c.wounds)
        {
            if (w.pressure < 0.03f) continue;
            Vector2 at = rot({w.x, w.y}, b.ang);
            at.x += b.x; at.y += b.y;
            float ang = w.ang + b.ang;
            int n = 0;
            float sp = 0;
            if (w.kind == WK_CUT) // a jet, wavering, weaker as it empties
            {
                n = w.pressure > 0.35f ? 2 : 1;
                sp = w.pressure * 2.6f;
                ang += std::sin(c.life * 0.9f + w.x) * 0.25f;
                w.pressure *= 0.993f;
                b.vx -= std::cos(ang) * w.pressure * 0.004f; // and it pushes back a little
                b.vy -= std::sin(ang) * w.pressure * 0.004f;
            }
            else
            {
                n = frand() < w.pressure * 0.35f ? 1 : 0; // seeping
                sp = 0.25f;
                w.pressure *= 0.997f;
            }
            for (int k = 0; k < n && G.parts.size() < 4800; k++)
            {
                Cell cell;
                cell.material = c.gore;
                cell.shade = (uint8_t)xr();
                Particle q{at.x, at.y, b.vx + std::cos(ang) * (sp + frange(0, 0.5f)) + frange(-0.15f, 0.15f), b.vy + std::sin(ang) * (sp + frange(0, 0.5f)), 200, cellColor(cell, 0, 0), 0.15f};
                q.toCell = c.gore;
                G.parts.push_back(q);
            }
        }
    }
}

void drawCorpses(int camX, int camY)
{
    for (auto& c : G.corpses)
    {
        float x = c.b.x - camX, y = c.b.y - camY;
        if (x < -60 || y < -60 || x > G.vw + 60 || y > G.vh + 60) continue;
        DrawTexturePro(c.tex, {0, 0, (float)c.tex.width, (float)c.tex.height}, {x, y, c.tex.width * c.unit, c.tex.height * c.unit}, {-c.ox, -c.oy}, c.b.ang * RAD2DEG, WHITE);
        for (auto& w : c.wounds)
            if (w.kind == WK_BOLT)
            {
                Vector2 at = rot({w.x, w.y}, c.b.ang);
                bolt({x + at.x, y + at.y}, w.ang + c.b.ang, 6);
            }
    }
}

void ragdollForPlayer()
{
    Mob& m = G.p.m;
    Look k = playerLook();
    spawnRagdoll(m.cx(), m.y + m.h, (float)m.h + 3, m.vx + frange(-0.5f, 0.5f), m.vy - 1.5f, k.metal ? k.A : k.hairD, k.tunic, k.pants, false, CellMaterial::Blood);
}

// ---------------------------------------------------------------- held weapon

// ---------------------------------------------------------------- weapon sprites
// One pixel sprite per weapon type, tinted by its metal: drawn in the hand, on the ground, on display and
// in the toolbar. Each points along `base` with the grip at its pivot. All but the crossbow are painted by
// tools/weapons.py (paste its output here); their letters are listed in weaponTex. The crossbow's own:
// 'B' metal, 'H' its lit edge, 'm' dark metal, 'h'/'d' wood, 's' string.
struct WeaponArt { const char* rows[48]; int pivX, pivY; float base = 0; };
static const WeaponArt WART[WTYPE_COUNT] = {
    {{".......................D",
      ".....................DD.",
      "....................DAE.",
      "...................DFE..",
      ".................DCCE...",
      "................DCAA....",
      "..............DFCFFE....",
      ".............DCCACE.....",
      ".........E.DDFCCAE......",
      ".........rqCCCCFFE......",
      "..........rqCFAAE.......",
      "..........nrqCCE........",
      ".........nnkqrAE........",
      "........nnkklqr.........",
      ".......nnkkl..qE........",
      "......nnkll.............",
      ".....nnkkl..............",
      "....nnkll...............",
      "...qqkkl................",
      ".nnnqql.................",
      "nnnkkq..................",
      "nnkkl...................",
      ".kkll...................",
      "..ll....................", nullptr}, 8, 15, -PI / 4}, // dagger: Damascus blade, disc guard, ringed grip
    {{"...........................DD",
      "........................DDCAE",
      "......................DCCCAA.",
      ".....................DCCCAAE.",
      "....................DCCFFAAE.",
      "...................DCCFFAAA..",
      "..................DCCFFAAAE..",
      ".................DCCFFAAAE...",
      "................DCCFFAAAE....",
      "...............DCCFFAAAE.....",
      "..............DCCFFAAAE......",
      "........f....DCCFFAAAE.......",
      ".......ffD..DCCFFAAAE........",
      ".......frrrDCCFFAAAE.........",
      "........DDrDCFFAAAE..........",
      ".........DrDDAAAAE...........",
      ".........crrDrAAE............",
      "........ccarDrrE.............",
      "........eeeeDDDD.............",
      "......caaaaccrrDr............",
      ".....cceeeec..rDff...........",
      ".....aaaaa.....ff............",
      "...cceeecc...................",
      "...caaaac....................",
      ".jjeeec......................",
      ".jffacc......................",
      "jfffi........................",
      ".ffii........................",
      "..i..........................", nullptr}, 7, 21, -PI / 4}, // sword
    {{"..................DDCC..........",
      ".................DCCCAAEE.......",
      "...............DDCCAAAAAAE......",
      "..............DDCAFFFAAAAAE.....",
      "..............DCAAAAAAFFFAAE....",
      ".............DCAAAAAAAAAAAFFEn..",
      "............DCCAAAAAAAAAAAAEEnk.",
      "............DCFFAAAAAAAAAAEEnkkl",
      "...........DCCAAAFFFAAAAAEEnkkl.",
      "...........DCEEEEEEEEEEFEEnkkl..",
      "......................EEEnkkl...",
      ".......................Enkkl....",
      "......................nnkkl.....",
      ".....................nnkkl......",
      "....................nnkkl.......",
      "...................nnkkl........",
      "..................nnkkl.........",
      ".................nnkkl..........",
      "................nnkkl...........",
      "...............nnkkl............",
      "..............nnkkl.............",
      ".............nnkkl..............",
      "............nnkkl...............",
      "...........nnkkl................",
      "..........nnkkl.................",
      ".........nnkkl..................",
      ".......cankkl...................",
      "......cceekl....................",
      "......aaaaa.....................",
      "....cceeecc.....................",
      "....caaaac......................",
      "...eeeec........................",
      "..caaacc........................",
      ".jjeee..........................",
      "jjffc...........................",
      ".ffi............................",
      "..i.............................", nullptr}, 7, 29, -PI / 4}, // bearded axe
    {{"...........mm..",
      "..........sm...",
      ".........s.m...",
      "........s...m..",
      ".......s....m..",
      "hhhhhhhhhhhhmBH",
      "dddddddddddhm..",
      ".......s....m..",
      "........s...m..",
      ".........s.m...",
      "..........sm...",
      "...........mm..", nullptr}, 3, 5}, // crossbow
    {{"................................nnn..",
      "...............................nnuu..",
      "..............................nnGGGGl",
      "..............................n.GGGGl",
      "................................GGGll",
      ".............................ffk..ll.",
      ".............................nfi.ll..",
      "............................nkli.....",
      "...........................nkl.......",
      "..........................nkl........",
      ".........................nkl.........",
      "........................nkl..........",
      ".......................nkl...........",
      "......................nkl............",
      ".....................nkl.............",
      "...................fnkl..............",
      "...................ffl...............",
      "..................nkii...............",
      ".................nkl.................",
      "................nkl..................",
      "...............nkl...................",
      "..............nkl....................",
      "............ffkl.....................",
      "...........anfi......................",
      ".........ccaali......................",
      ".........caaaa.......................",
      ".........aaac........................",
      "........nkacc........................",
      "......fnkl...........................",
      "......ffl............................",
      ".....nkii............................",
      "....nkl..............................",
      "...nkl...............................",
      "..nkl................................",
      ".nkl.................................",
      "jfl..................................",
      "fi...................................", nullptr}, 10, 26, -PI / 4}, // staff
    {{nullptr}, 0, 0}, // armour is never held
    {{"......................................DD",
      "..................................DDDCCE",
      ".................................DCCCCA.",
      "................................DCCCCAE.",
      "................................CCCCAAE.",
      "...............................DCCCAAAE.",
      "...............................DCCAAAE..",
      "...............................CCAAAE...",
      "...............................CAEE.....",
      "............................ffE.........",
      "............................ffi.........",
      "...........................nkii.........",
      "..........................nkl...........",
      ".........................nkl............",
      "........................nkl.............",
      ".......................nkl..............",
      "......................nkl...............",
      ".....................nkl................",
      "....................nkl.................",
      "...................nkl..................",
      "..................nkl...................",
      ".................nkl....................",
      "................nkl.....................",
      "...............nkl......................",
      "..............nkl.......................",
      ".............nkl........................",
      "............nkl.........................",
      ".........caakl..........................",
      "........cceee...........................",
      "........aaaaa...........................",
      "......ceeeecc...........................",
      ".....ccaaaac............................",
      "......aaaa..............................",
      ".....nkacc..............................",
      "....nkl.c...............................",
      "...nkl..................................",
      "..nkl...................................",
      ".nkl....................................",
      "jfl.....................................",
      "fi......................................", nullptr}, 9, 30, -PI / 4}, // spear
    {{"......................DC........",
      ".....................DCCC.......",
      "....................qCCCCC......",
      "...................qrqCCAAC.....",
      "..................DCqqrAAAAC....",
      ".................DCCCrqqAAAAC...",
      ".................ECCCAqrqAAAAC..",
      "..................ECAAAqrrAAAAC.",
      "...................EAAAArqrAAAAC",
      "....................EAAAArrqAAAE",
      "...................nnEAAAAqrrAE.",
      "..................nnklEAAAArqr..",
      ".................nnkll.EAAAAr...",
      "................nnkll...EAAE....",
      "...............nnkll.....EE.....",
      "..............nnkll.............",
      ".............nnkll..............",
      "............nnkll...............",
      "...........nnkll................",
      "..........nnkll.................",
      ".........nnkll..................",
      "........nnkll...................",
      ".......enkll....................",
      ".....caaall.....................",
      "....cceeee......................",
      "....aaaaa.......................",
      "..cceeecc.......................",
      "..caaaac........................",
      ".eeeec..........................",
      "jjaacc..........................",
      "jfie............................",
      "fii.............................", nullptr}, 5, 26, -PI / 4}, // mace, made a war hammer
    {{"...............DDDDDDDD......",
      ".............DDCCCCCCCCDD....",
      "............DCCCrrrrrrCCCD...",
      "...........DCCrrAAAAArrrCCD..",
      "..........DCCrAAAAAAAAArrCCD.",
      ".........DCCrAAAAAAAAAAArrAE.",
      ".........DCrrAAAAAAAAAAAACAAE",
      ".........DCrAAAAAAAAAAAAACCAE",
      "........DCCrAAAAAAAAAAAAAACAE",
      "........DCCrAAAAAAAAAAAAAACAE",
      "........DCCrAAAAAAAAAAAAAACAE",
      "........DCCrAACAAAAAAAACAACAE",
      ".........CCrAACCAAAAAACAAACAE",
      ".........DCCAAACCAAAACCAACAAE",
      ".........EAACAAACCCCCAAAACAE.",
      "..........EACCAAAAAAAAAACAAE.",
      "..........EEACCAAAAAAACCAAE..",
      "..........EEEAACCCCCCCCAAE...",
      ".........EEEEEAAAAAAAAAAE....",
      "......c.EEE...EEAAAAAEEE.....",
      "......caEE.......EEEE........",
      "....caaaa....................",
      "...cceeecc...................",
      "...aaaaa.....................",
      ".ceeeecc.....................",
      "ccaaaac......................",
      ".eeee........................",
      "jfacc........................",
      "fi.c.........................", nullptr}, 5, 23, -PI / 4}, // frying pan
};

static Texture2D weaponTex(const Weapon& w)
{
    Color mc = w.type == W_PAN ? METALS[M_IRON].color : METALS[w.metal].color;
    Color gem = w.type == W_STAFF ? w.staff.gem : BLANK;
    static std::map<std::tuple<int, unsigned, unsigned>, Texture2D> cache;
    auto key = std::make_tuple(w.type, (unsigned)ColorToInt(mc), (unsigned)ColorToInt(gem));
    auto it = cache.find(key);
    if (it != cache.end()) return it->second;
    const WeaponArt& a = WART[w.type];
    int h = 0, wd = (int)std::strlen(a.rows[0]);
    while (a.rows[h]) h++;
    Image img = GenImageColor(wd, h, BLANK);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < wd; x++)
        {
            Color c;
            switch (a.rows[y][x])
            {
            case 'B': c = mc; break;
            case 'H': c = brighten(mc, 70); break;
            case 'm': c = mul(mc, 0.38f); break;
            case 'h': c = {140, 98, 60, 255}; break;
            case 'd': c = {88, 60, 36, 255}; break;
            case 's': c = {214, 204, 176, 255}; break;
            case 'G': c = (x + y) % 3 ? gem : brighten(gem, 80); break;
            case 'A': c = mul(mc, 0.58f); break; // tools/weapons.py: blade body, bevel, lit edge, shadow, fuller
            case 'C': c = mul(mc, 0.66f); break;
            case 'D': c = mul(mc, 0.74f); break;
            case 'E': c = mul(mc, 0.33f); break;
            case 'F': c = mul(mc, 0.45f); break;
            case 'a': c = {58, 34, 27, 255}; break; // leather grip, its shadow and binding
            case 'c': c = {40, 24, 19, 255}; break;
            case 'e': c = {78, 48, 36, 255}; break;
            case 'f': c = {187, 159, 73, 255}; break; // gold
            case 'i': c = {140, 112, 48, 255}; break;
            case 'j': c = {230, 204, 118, 255}; break;
            case 'k': c = {92, 62, 44, 255}; break; // wood
            case 'l': c = {62, 40, 28, 255}; break;
            case 'n': c = {120, 84, 58, 255}; break;
            case 'q': c = {196, 170, 96, 255}; break; // brass
            case 'r': c = {34, 32, 32, 255}; break; // blackened iron
            case 'u': c = brighten(gem, 90); break;
            default: continue;
            }
            ImageDrawPixel(&img, x, y, c);
        }
    Texture2D t = LoadTextureFromImage(img);
    UnloadImage(img);
    SetTextureFilter(t, TEXTURE_FILTER_POINT);
    return cache[key] = t;
}

// Draws a weapon sprite turned to `ang` about its grip (or its middle, for icons). `scale`: units per pixel.
void drawWeaponSprite(const Weapon& w, Vector2 at, float ang, float scale, bool centred)
{
    const WeaponArt& a = WART[w.type];
    if (!a.rows[0]) return;
    Texture2D t = weaponTex(w);
    bool flip = std::cos(ang) < -0.01f; // pointing left: mirror top to bottom so the edge stays on top
    Rectangle src = {0, 0, (float)t.width, flip ? -(float)t.height : (float)t.height};
    float py = flip ? t.height - 1 - a.pivY : a.pivY;
    Vector2 origin = centred ? Vector2{t.width * scale / 2, t.height * scale / 2} : Vector2{(a.pivX + 0.5f) * scale, (py + 0.5f) * scale};
    DrawTexturePro(t, src, {at.x, at.y, t.width * scale, t.height * scale}, origin, (ang - (flip ? -a.base : a.base)) * RAD2DEG, WHITE);
}

float weaponLength(const Weapon& w) // grip to tip, in world cells, as drawn
{
    const WeaponArt& a = WART[w.type];
    float best = 0;
    for (int y = 0; a.rows[y]; y++)
        for (int x = 0; a.rows[y][x]; x++)
            if (a.rows[y][x] != '.') best = std::max(best, std::hypot((float)(x - a.pivX), (float)(y - a.pivY)));
    return best * 0.5f;
}

// ---------------------------------------------------------------- melee attack poses
// Where the weapon points and how far it's pushed out along that line, `back` frames ago, for the
// current attack: slashes sweep, stabs and spear pokes thrust along the aim, axes and hammers come up
// behind the head and down in front.
static float lerpf(float a, float b, float t) { return a + (b - a) * clampf(t, 0, 1); }
void attackPose(float& ang, float& ext, int back)
{
    const Player& P = G.p;
    float f = (float)P.m.facing;
    int len = std::max(1, P.atkLen), left = std::min(len, P.swingT + back);
    float p = 1 - (float)left / len, hp = (float)P.atkHitAt / len;
    bool fin = P.combo == 2;
    ext = 0;
    auto mirror = [&](float a) { return f > 0 ? a : PI - a; };
    switch (P.atkStyle)
    {
    case ATK_SLASH:
    {
        float k = fin ? 1.3f : 1.0f, tt = (float)left / len;
        ang = P.aim - P.swingDir * 1.4f * k + P.swingDir * 2.8f * k * (1 - tt * tt);
        if (fin) ext = 1.5f * std::sin(PI * p);
        break;
    }
    case ATK_STAB:
    case ATK_THRUST:
    {
        float reach = P.atkStyle == ATK_STAB ? 4.0f : (fin ? 13.0f : 9.0f), pull = P.atkStyle == ATK_STAB ? -1.5f : -4.0f;
        ang = P.aim;
        if (p < hp) ext = lerpf(0, pull, p / std::max(0.01f, hp * 0.7f)) + (p > hp * 0.7f ? (reach - pull) * (p - hp * 0.7f) / (hp * 0.3f) : 0);
        else if (p < hp + 0.2f) ext = reach;
        else ext = lerpf(reach, 0, (p - hp - 0.2f) / std::max(0.01f, 0.8f - hp));
        break;
    }
    default: // chop and slam: up over the shoulder, then down hard in front
    {
        float up = P.atkStyle == ATK_SLAM ? -PI / 2 - 0.9f : -PI / 2 - 0.5f, down = P.atkStyle == ATK_SLAM ? 1.25f : 0.95f;
        float a;
        if (p < hp * 0.8f) a = lerpf(0.9f, up, (p / (hp * 0.8f)) * (2 - p / (hp * 0.8f))); // eased wind-up
        else if (p < hp) a = lerpf(up, down, (p - hp * 0.8f) / (hp * 0.2f)); // the blow
        else a = lerpf(down + 0.15f, 0.9f, (p - hp) / (1 - hp));
        ang = mirror(a);
        break;
    }
    }
}

void drawHeld(float ox, float oy)
{
    Player& P = G.p;
    if (P.hotbar.empty()) return;
    const Weapon& w = P.hotbar[P.sel];
    float f = (float)G.p.m.facing;
    float a = P.aim;
    if (w.glow.a && G.frame % 6 == 0) // legendary shimmer
        spawnParticle(ox + std::cos(a) * 8 + frange(-2, 2) + G.rcx, oy + std::sin(a) * 8 + frange(-2, 2) + G.rcy, 0, -0.3f, 25, w.glow, -0.002f);
    if (isMelee(w.type))
    {
        float L = weaponLength(w), ext = 0;
        Color mc = w.type == W_PAN ? Color{150, 150, 160, 255} : METALS[w.metal].color;
        Color sm = lerpColor(brighten(mc, 60), ELEMENT_COLORS[METALS[w.metal].el], METALS[w.metal].el ? 0.6f : 0.0f);
        if (P.swingT > 0)
        {
            attackPose(a, ext, 0);
            ext -= P.atkStyle == ATK_STAB ? ext : std::min(ext, 2.5f); // what the arm doesn't reach, the shaft slides
            int len = std::max(1, P.atkLen);
            float p = 1 - (float)P.swingT / len, hp = (float)P.atkHitAt / len;
            BeginBlendMode(BLEND_ADDITIVE);
            if (P.atkStyle == ATK_STAB || P.atkStyle == ATK_THRUST)
            {
                // speed lines trailing the point while it drives forward
                float s = clampf(1 - std::fabs(p - hp) / 0.25f, 0, 1);
                if (s > 0)
                    for (int k = -1; k <= 1; k++)
                    {
                        Vector2 n = {-std::sin(a) * k * 1.8f, std::cos(a) * k * 1.8f};
                        float tip = ext + L, tail = tip - (k ? 7.0f : 11.0f) * s;
                        DrawLineEx({ox + std::cos(a) * tail + n.x, oy + std::sin(a) * tail + n.y}, {ox + std::cos(a) * tip + n.x, oy + std::sin(a) * tip + n.y},
                                   k ? 0.6f : 1.0f, {sm.r, sm.g, sm.b, (unsigned char)(200 * s)});
                    }
            }
            else
            {
                // a smear behind the blade through the fast part of the swing, brightest at its leading edge
                float a0, e0;
                attackPose(a0, e0, 3);
                bool fast = P.atkStyle == ATK_SLASH || (p > hp * 0.75f && p <= hp + 0.01f);
                float span = std::remainder(a - a0, 2 * PI);
                if (P.atkStyle != ATK_SLASH) // overhead blows turn more than half a circle: follow their real direction
                {
                    span = std::fmod(a - a0, 2 * PI);
                    if (f > 0 && span < 0) span += 2 * PI;
                    if (f < 0 && span > 0) span -= 2 * PI;
                }
                if (fast && std::fabs(span) > 0.05f)
                {
                    float r0 = std::max(2.0f, L * 0.35f), r1 = L + ext + 2.5f;
                    float lo = std::min(a0, a0 + span) * RAD2DEG, hi = std::max(a0, a0 + span) * RAD2DEG;
                    float heavy = P.atkStyle == ATK_SLASH ? 1.0f : 1.4f;
                    DrawRing({ox, oy}, r0, r1, lo, hi, 20, {sm.r, sm.g, sm.b, (unsigned char)(70 * heavy)});
                    DrawRing({ox, oy}, r1 - 3.0f, r1, lo, hi, 20, {sm.r, sm.g, sm.b, (unsigned char)(140 * heavy)});
                    DrawRing({ox, oy}, r1 - 1.0f, r1 + 0.6f, lo, hi, 20, {255, 255, 255, 190});
                    float lead = (span > 0 ? hi : lo) * DEG2RAD; // a hot spark at the tip
                    DrawCircleV({ox + std::cos(lead) * r1, oy + std::sin(lead) * r1}, 1.2f, {255, 255, 240, 220});
                }
            }
            EndBlendMode();
        }
        else
            a = f > 0 ? 1.0f : PI - 1.0f; // lowered at rest
        drawWeaponSprite(w, {ox + std::cos(a) * ext, oy + std::sin(a) * ext}, a, 0.5f, false);
    }
    else if (w.type == W_CROSSBOW)
    {
        float back = P.recoil * 0.4f;
        drawWeaponSprite(w, {ox - std::cos(a) * back, oy - std::sin(a) * back}, a, 0.5f, false);
    }
    else if (w.type == W_STAFF)
    {
        drawWeaponSprite(w, {ox, oy}, a, 0.5f, false);
        Vector2 g = {ox + std::cos(a) * 7, oy + std::sin(a) * 7};
        Color gem = w.staff.gem;
        if (w.staff.cd > 0) gem = lerpColor(gem, WHITE, 0.4f);
        float pulse = 0.8f + 0.2f * std::sin(G.frame * 0.15f);
        BeginBlendMode(BLEND_ADDITIVE);
        DrawCircleGradient((int)g.x, (int)g.y, 8 * pulse, {gem.r, gem.g, gem.b, 110}, {gem.r, gem.g, gem.b, 0});
        EndBlendMode();
        if (G.frame % 9 == 0) spawnParticle(g.x + G.rcx + frange(-1, 1), g.y + G.rcy + frange(-1, 1), frange(-0.2f, 0.2f), frange(-0.4f, -0.1f), irange(12, 22), gem, -0.005f);
    }
}

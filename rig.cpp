// Character art and animation:
//  - the hero and every creature are painted frame by frame (tools/anim.py -> sprites_anim.h)
//  - when a creature dies its body becomes a Verlet ragdoll of parts cut from the same painting
//  - the player's cape is cloth, simulated and drawn into a small pixel canvas
#include "game.h"
#include "sprites.h"
#include "sprites_hd.h"
#include "sprites_anim.h"
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
        c = lerpColor(c, fl > 0.75f ? Color{255, 176, 70, c.a} : Color{232, 84, 26, c.a}, 0.08f + 0.14f * fl * (1 - low * 0.5f)); // a warm cast; the flames themselves do the burning
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

// ---------------------------------------------------------------- player rig

// The limb rigs' joints (see "limb rigs" below); the player is drawn with them too.
enum { J_NECK, J_PELVIS, J_HEADB, J_HEADT, J_SHF, J_ELF, J_HAF, J_SHN, J_ELN, J_HAN, J_HIPF, J_KNF, J_FTF, J_HIPN, J_KNN, J_FTN, J_WB, J_WT };
static_assert(J_WT + 1 == RJ_COUNT, "joint count");
// The player's parts paint their armour in neutral grey (palette alpha 254) and runes in white (253): drawn with
// these set, the grey takes the armour's metal and the runes its element's glow.
static Color rigMetal = BLANK, rigGlow = BLANK;
static void drawRig(const RigSpec& R, const Vector2* J, int f, Color tint, bool white, float flap);
static float boneLen(const RigPart& p);
static Color coatTint(const Mob& m);
static Vector2 lerpV(Vector2 a, Vector2 b, float t);
static int clipFrame(const AnimSheet& A, int clip, float k);
static void animJoints(const AnimSheet& A, int fr, float x, float y, int facing, Vector2* J);
static void drawSheet(const AnimSheet& A, int fr, float x, float y, int facing, Color tint, bool white);


static const bool HERO_CAPE = false; // the robed hero (tools/userart.py) wears none; the cloth sim is kept for another look

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
       
        break;
    case AS_LEATHER:
        k.tunic = {128, 84, 50, 255}; k.trim = k.A; k.cloak = {44, 60, 96, 255};
       
        break;
    case AS_MAIL:
        k.tunic = k.A; k.trim = k.Ad; k.cloak = {70, 66, 64, 255}; k.bracer = k.Ad;
       
        break;
    case AS_LAMELLAR:
        k.tunic = k.A; k.trim = k.glow; k.cloak = mul(k.glow, 0.42f); k.bracer = k.Ad; k.fur = {70, 64, 60, 255};
       
        break;
    default:
        k.tunic = k.A; k.trim = {214, 180, 80, 255}; k.cloak = {32, 30, 38, 255}; k.bracer = k.A; k.fur = {60, 56, 54, 255};
        k.pants = {48, 44, 50, 255}; k.wrap = k.Ad;
       
        break;
    }
    k.tunicD = mul(k.tunic, 0.66f);
    k.tunicL = brighten(k.tunic, 34);
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

    // which frame of the hero's sheets (tools/anim.py): one per armour set, the near arm on a layer of its own
    Look k = playerLook();
    static const AnimSheet* BODY[] = {&ANIM_HERO_WOOL, &ANIM_HERO_LEATHER, &ANIM_HERO_MAIL, &ANIM_HERO_LAMELLAR, &ANIM_HERO_SCALE};
    static const AnimSheet* ARM[] = {&ANIM_HERO_WOOL_ARM, &ANIM_HERO_LEATHER_ARM, &ANIM_HERO_MAIL_ARM, &ANIM_HERO_LAMELLAR_ARM, &ANIM_HERO_SCALE_ARM};
    static const AnimSheet* AIM[] = {&ANIM_HERO_WOOL_AIM, &ANIM_HERO_LEATHER_AIM, &ANIM_HERO_MAIL_AIM, &ANIM_HERO_LAMELLAR_AIM, &ANIM_HERO_SCALE_AIM};
    const AnimSheet &A = *BODY[k.set], &AA = *ARM[k.set], &AM = *AIM[k.set];
    auto cyc = [](float x) { return x - std::floor(x); };
    const float TAU = 2 * PI;
    bool moving = std::fabs(m.vx) > 0.08f;
    int clip = AC_IDLE;
    float kk = cyc(t / 60.0f);
    bool swinging = hasWeapon && isMelee(wpn->type) && P.swingT > 0 && !air && !P.prone && !P.crouch && !m.inLiquid && !P.climb && !P.onWall && P.hook != 2;
    if (P.rollT > 0)
    {
        int e = 19 - P.rollT; // 0..18: dip into the tuck, a fast tight spin, skid back up
        if (e < 2) clip = AC_ROLLIN, kk = e / 2.0f;
        else if (e < 16) clip = AC_ROLL, kk = (e - 2) / 14.0f;
        else clip = AC_ROLLOUT, kk = (e - 16) / 3.0f;
    }
    else if (swinging)
    { // the body commits to the blow: coil (before the hit), lunge (on it), follow through and settle (after)
        clip = P.atkStyle == ATK_SLASH || P.atkStyle == ATK_SWEEP ? AC_SLASH : P.atkStyle == ATK_CHOP || P.atkStyle == ATK_SLAM ? AC_CHOP : AC_THRUST; // stab, thrust, bash
        float p = 1 - (float)P.swingT / std::max(1, P.atkLen), hp = (float)P.atkHitAt / std::max(1, P.atkLen);
        kk = p < hp ? 0.4f * p / hp : p < hp + 0.15f ? 0.4f + 0.2f * (p - hp) / 0.15f : 0.6f + 0.4f * (p - hp - 0.15f) / std::max(0.05f, 1 - hp - 0.15f);
    }
    else if (P.prone) clip = AC_CRAWL, kk = moving ? cyc(P.runPhase * 1.3f / TAU) : 0;
    else if (P.crouch) clip = moving ? AC_CROUCHWALK : AC_CROUCH, kk = moving ? cyc(P.runPhase / TAU) : cyc(t / 48.0f);
    else if (m.inLiquid) // off the bottom: a crawl stroke when going somewhere, treading water when hanging; on it: wading, the stride played slow
    {
        bool going = std::fabs(m.vx) > 0.15f || std::fabs(m.vy) > 0.25f;
        if (!m.onGround) { clip = going ? AC_SWIM : AC_TREAD; kk = cyc(t * (going ? 0.2f : 0.1f) / TAU); }
        else if (going) { clip = AC_WALK; kk = cyc(t * 0.09f / TAU); if (m.vx * f < 0) kk = 1 - kk; }
    }
    else if (P.climb) clip = AC_CLIMB, kk = cyc(P.runPhase * 2.5f / TAU);
    else if (P.onWall) clip = AC_WALL;
    else if (P.hook == 2 && air) clip = AC_HANG;
    else if (air) clip = m.vy < -0.3f ? AC_JUMP : AC_FALL;
    else if (P.squash < 0.9f) clip = AC_LAND; // the knees take a hard landing
    else if (std::fabs(m.vx) > 0.12f)
    {
        clip = AC_WALK;
        kk = cyc(P.runPhase / TAU);
        if (m.vx * f < 0) kk = 1 - kk; // backpedalling plays the stride in reverse
    }
    int fr = clipFrame(A, clip, kk);
    float bx = std::floor(m.x) + m.w * 0.5f - camX, by = std::floor(m.y) + m.h - camY; // whole-pixel anchor, no shimmer
    Vector2 J[RJ_COUNT];
    animJoints(A, fr, bx, by, (int)f, J);
    Vector2 shoulder = J[J_NECK], hip = J[J_PELVIS];

    { // pinned to the body, not the posed shoulder: a stride's bob would yank it about every frame
        static float shoulderH = 17; // the shoulder's height above the feet, eased
        shoulderH += (by - shoulder.y - shoulderH) * 0.3f;
        simulateCape(P, {std::floor(m.x) + m.w * 0.5f - f * 2.0f, std::floor(m.y) + m.h - shoulderH + 0.5f}, f);
    }
    if (m.iframes > 0 && P.rollT == 0 && (G.frame / 3) % 2) return; // hurt blink

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
    if (HERO_CAPE)
    {
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
    }

    if (hasWeapon && !drawn && P.rollT == 0 && !P.prone) drawStowed(*wpn, shoulder, hip, f);
    coatMob = (m.wet || m.oily || m.bloody || m.burn) ? &m : nullptr;
    canvasEnd(m.hurtFlash > 0);
    coatMob = nullptr;

    rigMetal = k.metal ? k.A : Color{150, 150, 156, 255};
    rigGlow = (G.frame / 8) % 7 ? k.glow : WHITE;
    Color tint = coatTint(m);
    if (m.inLiquid) tint = {(unsigned char)(tint.r * 0.7f), (unsigned char)(tint.g * 0.88f), (unsigned char)(tint.b * 1.0f), tint.a}; // seen through the water: cooler, dimmer
    bool white = m.hurtFlash > 0;
    drawSheet(A, fr, bx, by, (int)f, tint, white);

    // the near arm: with the stride, unless it's holding a drawn weapon out (or the grappling rope) - then it's
    // the arm alone, painted at 16 angles, turned to the nearest
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
    if (P.hook == 2) { aimAng = std::atan2(P.hy - camY - J[J_SHN].y, P.hx - camX - J[J_SHN].x); weaponInHand = false; }
    if (weaponInHand || P.hook == 2)
    {
        float a = std::atan2(f * std::cos(aimAng), std::sin(aimAng)); // 0 down, turning towards the way you face
        int ki = (((int)std::lround(a / (2 * PI) * 16)) % 16 + 16) % 16;
        float ak = ki * 2 * PI / 16;
        Vector2 sh = add(J[J_SHN], {std::cos(aimAng) * reach, std::sin(aimAng) * reach});
        drawSheet(AM, ki, sh.x, sh.y, (int)f, tint, white);
        Vector2 hand = add(sh, {f * std::sin(ak) * 6.6f, std::cos(ak) * 6.6f});
        rigMetal = rigGlow = BLANK;
        if (weaponInHand) drawHeld(hand.x, hand.y);
        return;
    }
    drawSheet(AA, fr, bx, by, (int)f, tint, white);
    rigMetal = rigGlow = BLANK;
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

// One tongue of flame as 0.5-unit pixels, its root at (x, by): a white-yellow core, yellow, orange and red toward a ragged,
// swaying tip. `seed` makes every tongue its own; `alpha` thins the whole thing.
static void flameShape(float x, float by, float w, float h, int seed, float alpha)
{
    float f = (float)G.frame;
    for (float cx = -w / 2; cx <= w / 2; cx += 0.5f)
    {
        float u = cx / (w / 2), prof = std::pow(std::max(0.0f, 1 - u * u), 0.7f);
        float n = vnoise(seed * 3.1f + cx * 0.8f, f * 0.23f, 5), colH = h * prof * (0.5f + 0.75f * n);
        float sway = std::sin(f * 0.17f + seed * 1.7f) * w * 0.3f + (vnoise(seed * 1.3f, f * 0.1f, 8) - 0.5f) * w * 0.4f;
        for (float y = 0; y < colH; y += 0.5f)
        {
            float t = y / colH, lean = t * t * sway;
            if (t > 0.55f && hash2((int)(cx * 2) + seed * 7, (int)(y * 2) - (int)(f * 1.5f), 13) > 1.25f - t * 0.9f) continue; // ragged, flickering edges
            float v = t * 0.85f + std::fabs(u) * 0.55f;
            Color c = v < 0.26f ? Color{255, 247, 200, 255} : (v < 0.5f ? Color{255, 208, 74, 255} : (v < 0.75f ? Color{250, 126, 30, 240} : (v < 0.95f || t < 0.8f ? Color{206, 54, 24, 210} : Color{120, 30, 20, 140})));
            c.a = (unsigned char)(c.a * alpha);
            DrawRectangleRec({x + cx + lean, by - y - 0.5f, 0.5f, 0.5f}, c);
        }
    }
}

// A live flame rooted at (x, y), about 7 * s units tall, with its glow and the odd spark.
void drawFlame(float x, float y, float s, int seed)
{
    float fl = hash2(seed, G.frame / 4, 9);
    BeginBlendMode(BLEND_ADDITIVE);
    DrawCircleGradient((int)x, (int)(y - 3 * s), 9 * s + fl * 2, {255, 140, 50, 60}, {255, 140, 50, 0});
    EndBlendMode();
    flameShape(x, y, 3.2f * s, 7.5f * s, seed, 1);
    flameShape(x + (fl - 0.5f) * s, y, 2.0f * s, 5.0f * s, seed + 5, 1);
    if (fl > 0.84f) spawnParticle(x + G.rcx + frange(-s, s), y + G.rcy - 6 * s, frange(-0.2f, 0.2f), -0.45f, 28, {255, 180, 70, 255}, -0.004f);
}

// Fire you can't miss: a glare round the body and tongues of flame licking up its length, taller where the oil is,
// with embers and smoke rising off it.
void drawBurning(const Mob& m, int camX, int camY)
{
    float x0 = std::floor(m.x) - camX, top = std::floor(m.y) - camY, h = (float)m.h, w = (float)m.w, f = (float)G.frame;
    BeginBlendMode(BLEND_ADDITIVE);
    float pulse = 0.85f + 0.15f * std::sin(f * 0.3f);
    DrawCircleGradient((int)(x0 + w / 2), (int)(top + h * 0.5f), h * 0.95f * pulse, {255, 120, 30, 70}, {255, 80, 20, 0});
    EndBlendMode();
    int n = 5 + (int)(w / 4) + (m.oily ? 3 : 0);
    for (int i = 0; i < n; i++)
    {
        int seed = m.id * 13 + i + (G.frame / 14 + i * 3) / 5 * 31; // tongues pop up in new places now and then
        float fx = x0 + 0.5f + (w - 1) * hash2(seed, i, 41), fy = top + h * (0.3f + 0.65f * hash2(seed, i, 43));
        float th = h * (m.oily ? 0.55f : 0.38f) * (0.7f + 0.5f * hash2(seed, i, 47));
        flameShape(fx, fy, std::max(2.4f, w * 0.38f), th, seed, 0.95f);
    }
    if (G.frame % 3 == 0)
        spawnParticle(m.x + frange(0, w), m.y + frange(0, h * 0.5f), frange(-0.1f, 0.1f), frange(-0.5f, -0.25f), irange(20, 36), {255, 170, 60, 255}, -0.004f);
    if (G.frame % 6 == 0)
        spawnParticle(m.x + frange(0, w), m.y - 1, frange(-0.1f, 0.1f), frange(-0.4f, -0.2f), irange(40, 70), {60, 56, 58, 120}, -0.002f);
}

// A detailed sprite as a texture, made the first time it's asked for.
static const Texture2D& hdTexture(const HDSprite& s)
{
    static std::map<const HDSprite*, Texture2D> cache;
    Texture2D& t = cache[&s];
    if (!t.id)
    {
        Image img = GenImageColor(s.w, s.h, BLANK);
        for (int y = 0; y < s.h; y++)
            for (int x = 0; x < s.w; x++)
                if (s.rows[y][x] != '.') ImageDrawPixel(&img, x, y, s.pal[std::strchr(HD_ALPHABET, s.rows[y][x]) - HD_ALPHABET]);
        t = LoadTextureFromImage(img);
        UnloadImage(img);
    }
    return t;
}

// The chest art as textures, so it can tumble.
void drawChest(float cx, float cy, float ang, bool open, Color tint, float sink)
{
    const Texture2D& t = hdTexture(open ? HD_CHEST_OPEN : HD_CHEST);
    float w = t.width * 0.5f, h = t.height * 0.5f; // half a unit a pixel, like the rest of the fine art
    if (sink > 0) // sunk into the sand: lowered, and whatever is under the ground line left undrawn
    {
        float vis = h - sink;
        DrawTexturePro(t, {0, 0, (float)t.width, vis * 2}, {cx - w / 2, cy - h / 2 + sink, w, vis}, {0, 0}, 0, tint);
        return;
    }
    DrawTexturePro(t, {0, 0, (float)t.width, (float)t.height}, {cx, cy, w, h}, {w / 2, h / 2}, ang * RAD2DEG, tint);
}

// A rune bomb: knotwork on blued iron, its fuse lit (tools: the user's reference art, redrawn at 15x19).
static const Color HDP_BOMB[] = {{20, 26, 38, 255}, {39, 50, 74, 255}, {54, 69, 106, 255}, {76, 95, 134, 255}, {145, 169, 201, 255}, {100, 124, 159, 255}, {30, 37, 50, 255}, {255, 224, 112, 255}, {255, 154, 60, 255}, {196, 212, 232, 255}};
static const char* const HDR_BOMB[] = {
    ".......8.......",
    "......878......",
    ".......8.......",
    "......606......",
    ".....06660.....",
    "......000......",
    "....9033300....",
    "..09333432200..",
    "..03354545220..",
    ".0335432245220.",
    ".0335422245220.",
    ".0334244424110.",
    ".0345224225410.",
    ".0225524255110.",
    ".0024445444100.",
    "..02221111110..",
    "...021111110...",
    "....0000000....",
    "...............",
};
static const HDSprite HD_BOMB = {15, 19, HDP_BOMB, HDR_BOMB};
void drawBomb(float cx, float cy, float ang, float scale)
{
    const Texture2D& t = hdTexture(HD_BOMB);
    float w = t.width * 0.5f * scale, h = t.height * 0.5f * scale;
    DrawTexturePro(t, {0, 0, (float)t.width, (float)t.height}, {cx, cy, w, h}, {w / 2, h * 0.62f}, ang * RAD2DEG, WHITE);
}

// Background decorations (IT_DECOR) at half a unit per pixel, the same grain as the creatures: each variant is
// painted once into a texture. House style: no outline, lit from the upper left, a ramp of five tones.
static Image decorImage(int kind, int var)
{
    auto rnd = [&](int k) { return hash2(var * 31 + k, kind * 7, 4242); };
    auto put = [](Image& im, int x, int y, Color c) { if (x >= 0 && y >= 0 && x < im.width && y < im.height) ImageDrawPixel(&im, x, y, c); };
    if (kind == DK_CACTUS)
    {
        const Color G5[5] = {{24, 46, 30, 255}, {36, 66, 40, 255}, {52, 90, 52, 255}, {74, 118, 66, 255}, {100, 146, 84, 255}};
        const Color spine = {214, 206, 168, 255}, bloom = {232, 120, 150, 255};
        int h = 34 + (int)(rnd(1) * 28), W = 36, cx = 13;
        Image im = GenImageColor(W, h + 1, BLANK);
        auto column = [&](int x0, int y0, int y1, int w) { // a ribbed stem, rounded on top, lit on its left
            for (int y = y0; y <= y1; y++)
                for (int k = 0; k < w; k++)
                {
                    int top = y - y0;
                    if ((top == 0 && (k < 2 || k >= w - 2)) || (top == 1 && (k == 0 || k == w - 1))) continue;
                    int t = k % 3 == 0 ? 1 : 2;   // grooves between the ribs
                    if (k <= 1) t = 3;            // the lit flank
                    if (k == 1 && k % 3) t = 4;
                    if (k >= w - 2) t = 1;        // the shaded one
                    if (top < 2 && t > 0) t++;    // the crown catches the light
                    put(im, x0 + k, y, G5[std::min(4, t)]);
                    if (k % 3 == 0 && (y + k) % 5 == 0 && top > 2) put(im, x0 + k + (k == 0 ? -1 : 0), y, spine); // spines along the grooves
                }
        };
        column(cx, 0, h, 10);
        for (int sd : {-1, 1})
        {
            if (rnd(sd + 5) < 0.3f) continue;
            int ay = (int)(h * (0.35f + 0.3f * rnd(sd + 9))), reach = 4 + (int)(rnd(sd + 11) * 3), up = 8 + (int)(rnd(sd + 13) * 10);
            int ax = sd > 0 ? cx + 10 : cx - reach;
            for (int y = ay; y < ay + 5; y++) for (int k = 0; k < reach; k++) put(im, ax + k, y, G5[y == ay ? 3 : (y == ay + 4 ? 1 : 2)]);
            int colX = sd > 0 ? cx + 10 + reach - 6 : cx - reach;
            column(colX, ay - up, ay + 4, 6);
        }
        if (rnd(20) < 0.35f) for (int k = 3; k < 7; k++) put(im, cx + k, 0, bloom), put(im, cx + k - 1, 1, k % 2 ? bloom : Color{250, 220, 120, 255});
        return im;
    }
    if (kind == DK_BUSH)
    {
        const Color T[4] = {{58, 42, 30, 255}, {86, 62, 42, 255}, {116, 88, 60, 255}, {146, 116, 82, 255}};
        Image im = GenImageColor(28, 20, BLANK);
        std::function<void(float, float, float, float, int)> twig = [&](float x, float y, float a, float len, int depth) {
            for (float d = 0; d < len; d += 0.5f)
            {
                int px = (int)(x + std::cos(a) * d), py = (int)(y + std::sin(a) * d);
                put(im, px, py, T[std::min(3, depth + (std::cos(a) < 0 ? 1 : 0))]);
                if (depth == 0) put(im, px + 1, py, T[0]);
            }
            if (depth >= 3) return;
            float ex = x + std::cos(a) * len, ey = y + std::sin(a) * len;
            twig(ex, ey, a - 0.5f - rnd(depth * 3 + (int)len) * 0.3f, len * 0.65f, depth + 1);
            twig(ex, ey, a + 0.45f + rnd(depth * 5 + (int)len) * 0.3f, len * 0.6f, depth + 1);
        };
        for (int b = 0; b < 3; b++) twig(13.0f + b, 19.5f, -PI / 2 + (b - 1) * 0.5f + (rnd(b) - 0.5f) * 0.4f, 7 + rnd(b + 3) * 4, 0);
        return im;
    }
    if (kind == DK_GIANT) // the bones of a giant beast, half swallowed by the sand: skull, sagging spine, a cage of ribs
    {
        const Color B5[5] = {{70, 62, 48, 255}, {112, 102, 84, 255}, {160, 152, 130, 255}, {196, 190, 166, 255}, {224, 220, 200, 255}};
        Image im = GenImageColor(210, 100, BLANK);
        const int gy = 99;
        auto spineY = [&](int x) { return gy - 10 + (int)(std::sin(x * 0.03f) * 6); };
        for (int r = 0; r < 7; r++) // ribs first, so the spine lies over their roots
        {
            int rx = 70 + r * 19, rh = 70 - std::abs(r - 2) * 9, base = spineY(rx);
            float stop = rnd(30 + r) < 0.3f ? 0.55f + 0.2f * rnd(40 + r) : 1.0f; // some snapped off
            for (float a = 0; a < PI * stop; a += 0.01f)
            {
                int x = rx + (int)(10 * std::cos(a)), y = base - (int)(rh * std::sin(a));
                for (int t = 0; t < 3; t++) put(im, x + t, y, B5[t == 0 ? (std::cos(a) > 0 ? 3 : 4) : (t == 2 ? 1 : 2)]);
            }
        }
        for (int x = 40; x < 208; x++) // the spine, vertebra by vertebra
            for (int t = 0; t < 6; t++)
            {
                int y = spineY(x) - 2 + t;
                bool notch = x % 9 == 0 && t < 2;
                if (!notch) put(im, x, y, B5[t == 0 ? 4 : (t >= 4 ? 1 : (x % 9 < 2 ? 2 : 3))]);
            }
        for (int y = gy - 34; y <= gy; y++) // the skull, its long snout in the sand
            for (int x = 0; x < 56; x++)
            {
                float dx = (x - 30) / 26.0f, dy = (y - (gy - 16)) / 15.0f;
                float snout = x < 22 ? (x - 2) / 20.0f : 1; // tapering to the left
                if (dx * dx + dy * dy > snout * snout + 0.05f) continue;
                put(im, x, y, B5[dx + dy < -0.7f ? 4 : (dx + dy > 0.6f ? 1 : (dy > 0.2f ? 2 : 3))]);
            }
        for (int y = gy - 24; y <= gy - 16; y++) for (int x = 32; x <= 40; x++) // the eye, a dark hollow
        {
            float dx = (x - 36) / 4.5f, dy = (y - (gy - 20)) / 4.5f;
            if (dx * dx + dy * dy <= 1) put(im, x, y, dy < -0.3f ? B5[1] : B5[0]);
        }
        for (int x = 6; x < 30; x += 4) put(im, x, gy - 5, B5[0]), put(im, x + 1, gy - 4, B5[4]); // teeth
        for (float a = 0; a < 1.6f; a += 0.02f) // a horn sweeping back
            for (int t = 0; t < 4 - (int)(a * 1.5f); t++) put(im, 42 + (int)(std::sin(a) * 22) + t, gy - 28 - (int)((1 - std::cos(a)) * 16), B5[t == 0 ? 4 : 2]);
        return im;
    }
    // DK_SKELETON: someone who didn't make it, sprawled on the floor, skull at the left
    const Color B5[5] = {{70, 62, 48, 255}, {112, 102, 84, 255}, {160, 152, 130, 255}, {200, 194, 172, 255}, {228, 224, 206, 255}};
    Image im = GenImageColor(48, 18, BLANK);
    const int gy = 17; // the floor line
    auto bone = [&](float x0, float y0, float x1, float y1) { // a long bone: lit top, shaded underside, knobbed ends
        int n = (int)std::max(std::fabs(x1 - x0), std::fabs(y1 - y0)) * 2 + 1;
        for (int i = 0; i <= n; i++)
        {
            float t = (float)i / n;
            int x = (int)std::lround(x0 + (x1 - x0) * t), y = (int)std::lround(y0 + (y1 - y0) * t);
            put(im, x, y, B5[3]); put(im, x, y + 1, B5[1]);
        }
        put(im, (int)x0, (int)y0 - 1, B5[4]); put(im, (int)x1, (int)y1 - 1, B5[4]); put(im, (int)x1 + 1, (int)y1, B5[2]);
    };
    // the skull, lying on its side
    for (int y = gy - 8; y <= gy - 1; y++)
        for (int x = 1; x <= 9; x++)
        {
            float dx = (x - 5) / 4.5f, dy = (y - (gy - 5)) / 4.0f;
            if (dx * dx + dy * dy > 1) continue;
            put(im, x, y, B5[dx + dy < -0.6f ? 4 : (dx + dy > 0.7f ? 1 : 3)]);
        }
    for (int x = 6; x <= 11; x++) put(im, x, gy - 2, (x % 2) ? B5[4] : B5[1]), put(im, x, gy - 1, B5[2]); // the jaw and its teeth
    put(im, 6, gy - 5, B5[0]); put(im, 7, gy - 5, B5[0]); put(im, 6, gy - 6, B5[0]); put(im, 7, gy - 6, B5[1]); // the eye socket
    put(im, 9, gy - 4, B5[0]); // the nose
    // the spine, the ribcage arching over it, the pelvis
    for (int x = 12; x <= 29; x++) { put(im, x, gy - 2, B5[2]); if (x % 2 == 0) put(im, x, gy - 3, B5[3]); put(im, x, gy - 1, B5[1]); }
    int sprung = (int)(rnd(2) * 3);
    for (int r = 0; r < 5; r++)
    {
        int rx = 13 + r * 3, rh = 7 - std::abs(r - 1) - (r == sprung ? 3 : 0);
        for (int k = 0; k < rh; k++) put(im, rx + k / 3, gy - 3 - k, k == rh - 1 ? B5[4] : B5[3]), put(im, rx + k / 3 + 1, gy - 3 - k, B5[1]);
    }
    for (int y = gy - 6; y <= gy - 1; y++)
        for (int x = 29; x <= 34; x++)
        {
            bool hole = x >= 31 && x <= 32 && y >= gy - 4 && y <= gy - 3;
            if (!hole) put(im, x, y, B5[y == gy - 6 ? 4 : (x == 34 || y == gy - 1 ? 1 : 3)]);
        }
    // limbs: an arm flung up (or lying along), a leg drawn up at the knee (or straight)
    if (rnd(3) < 0.5f) { bone(14, gy - 3, 11, gy - 10); bone(11, gy - 10, 16, gy - 14); }
    else { bone(14, gy - 2, 20, gy - 1); bone(20, gy - 1, 26, gy - 1); }
    if (rnd(4) < 0.5f) { bone(34, gy - 3, 39, gy - 9); bone(39, gy - 9, 45, gy - 2); }
    else { bone(34, gy - 2, 40, gy - 2); bone(40, gy - 2, 46, gy - 1); }
    bone(33, gy - 2, 38, gy - 1);
    return im;
}

// Önd, the breath Odin gave the first people: a glass orb with a wind spiralling inside, bound in twisted roots,
// moss and a few sea crystals. Painted once, at half a unit per pixel.
void drawOnd(float cx, float cy, float scale)
{
    static Texture2D t{};
    if (!t.id)
    {
        const int N = 26;
        Image im = GenImageColor(N, N, BLANK);
        auto put = [&](int x, int y, Color c) { if (x >= 0 && y >= 0 && x < N && y < N) ImageDrawPixel(&im, x, y, c); };
        const float c0 = 12.5f;
        for (int y = 0; y < N; y++) // the glass: deep sea-blue at its rim, pale at heart, lit from the upper left
            for (int x = 0; x < N; x++)
            {
                float dx = x + 0.5f - c0, dy = y + 0.5f - c0, d = std::sqrt(dx * dx + dy * dy) / 9.0f;
                if (d > 1) continue;
                float lit = clampf(0.55f - d * 0.5f - (dx + dy) * 0.02f, 0, 1);
                put(x, y, lerpColor(Color{18, 70, 96, 255}, Color{120, 214, 232, 255}, lit));
            }
        for (int arm = 0; arm < 2; arm++) // the breath inside: a double spiral of wind
            for (float a = 0; a < 7.5f; a += 0.05f)
            {
                float r = 0.6f + a * 0.95f, ang = a + arm * 3.14159f;
                if (r > 8) break;
                put((int)(c0 + std::cos(ang) * r), (int)(c0 + std::sin(ang) * r * 0.8f), a > 5 ? Color{150, 230, 245, 255} : Color{220, 252, 255, 255});
            }
        put(8, 7, WHITE); put(9, 6, WHITE); put(8, 6, {230, 250, 255, 255}); put(16, 15, {200, 240, 250, 255}); // glints
        for (float a = 0; a < 6.283f; a += 0.02f) // roots twisting round it, two strands crossing, moss in the crooks
            for (int s = 0; s < 2; s++)
            {
                float r = 10.2f + (s ? 1 : -1) * std::sin(a * 6) * 1.1f;
                int x = (int)(c0 + std::cos(a) * r), y = (int)(c0 + std::sin(a) * r);
                bool over = (std::sin(a * 6) > 0) == (s == 0);
                put(x, y, over ? Color{112, 92, 60, 255} : Color{64, 50, 36, 255});
                if (hash2((int)(a * 40), s, 77) > 0.86f) put(x + (s ? 1 : -1), y, {74, 120, 66, 255});
            }
        for (auto c : {Vector2{21, 3}, Vector2{22, 4}, Vector2{3, 19}, Vector2{4, 20}, Vector2{20, 21}}) put((int)c.x, (int)c.y, {150, 236, 230, 255}); // sea crystals
        t = LoadTextureFromImage(im);
        UnloadImage(im);
    }
    float w = t.width * 0.5f * scale;
    DrawTexturePro(t, {0, 0, (float)t.width, (float)t.height}, {cx - w / 2, cy - w / 2, w, w}, {0, 0}, 0, WHITE);
}

// A long pennant on a spear, live: a strip of cloth tied at the lashing, streaming downwind in a wave that grows toward the free end,
// sagging a little, with a swallowtail cut in it. One rectangle per column (hem light, body, underside dark) on the half-unit grid.
static void drawPennant(float x0, float y0, int dir, int var, float t)
{
    const float u = 0.5f;
    int N = 30 + (var % 4) * 6;
    float ph = var * 1.7f, gust = 0.75f + 0.25f * std::sin(t * 0.017f + x0 * 0.01f + ph), px = 0;
    for (int i = 0; i < N; i++)
    {
        float k = (float)i / N, a = t * 0.13f * (0.8f + 0.4f * gust) - i * 0.42f + ph;
        float wave = std::sin(a) * (0.5f + 2.8f * k) * gust, slope = std::cos(a) * (0.5f + 2.8f * k);
        float yc = y0 + (wave + k * k * 3.0f * (1.2f - gust * 0.8f)) * u;
        float half = 3.6f * (1 - 0.5f * k) * u;
        float x = x0 + dir * px;
        px += u * (1.0f - 0.06f * std::fabs(slope)); // the folds shorten it a little
        int tn = 2 + (slope * -dir > 0.5f ? 1 : 0) - (slope * -dir < -0.5f ? 1 : 0);
        float xs = std::floor(x / u) * u, top = std::floor((yc - half) / u) * u, bot = std::floor((yc + half) / u) * u + u;
        float cut = k > 0.76f ? (k - 0.76f) / 0.24f * (bot - top) * 0.42f : 0; // the swallowtail
        auto col = [&](int tone) { Color c = clothTone(var, tone); return Color{(unsigned char)(c.r * 0.74f), (unsigned char)(c.g * 0.74f), (unsigned char)(c.b * 0.78f), 255}; }; // a step darker: it stands back in the field, behind the fighting
        if (cut < 0.01f) DrawRectangleRec({xs - (dir < 0 ? u : 0), top, u, bot - top}, col(tn));
        else
        {
            float mid = (top + bot) / 2, g = std::floor(cut / u) * u * 0.5f;
            DrawRectangleRec({xs - (dir < 0 ? u : 0), top, u, std::max(u, mid - g - top)}, col(tn));
            DrawRectangleRec({xs - (dir < 0 ? u : 0), mid + g, u, std::max(u, bot - mid - g)}, col(tn));
        }
        DrawRectangleRec({xs - (dir < 0 ? u : 0), top, u, u}, col(std::min(4, tn + 1)));              // the hem catches the light along the top
        if (cut < 0.01f || i % 2) DrawRectangleRec({xs - (dir < 0 ? u : 0), bot - u, u, u}, col(std::max(0, tn - 2))); // and the underside is in shadow
    }
}

void drawDecor(const Interact& it, float x, float y)
{
    struct Art { Texture2D t, sh; };
    static std::map<int, Art> cache;
    int var = it.style & 63, key = (it.w << 15) | (it.data * 128 + (it.style & 127));
    auto c = cache.find(key);
    if (c == cache.end())
    {
        Image im = it.data < DK_DRESSER ? decorImage(it.data, var) : decorImageFine(it.data, var, it.w);
        if (it.data >= DK_DRESSER && it.data != DK_BLOOD && it.data != DK_TAPESTRY && it.data != DK_DRAPE && it.data != DK_CHAIN && it.data != DK_LEANSHIELD && it.data != DK_COBWEB && it.data != DK_LEAK)
        { // a shading pass over every piece: light catching its top and left edges, its lower and right edges falling into shade
            Image b = ImageCopy(im);
            Color* src = (Color*)b.data; Color* dst = (Color*)im.data;
            auto solid = [&](int xx, int yy) { return xx >= 0 && yy >= 0 && xx < b.width && yy < b.height && src[yy * b.width + xx].a > 128; };
            for (int yy = 0; yy < b.height; yy++)
                for (int xx = 0; xx < b.width; xx++)
                {
                    if (!solid(xx, yy)) continue;
                    int d = 0;
                    if (!solid(xx, yy - 1)) d += 14; else if (!solid(xx - 1, yy)) d += 8;
                    if (!solid(xx, yy + 1)) d -= 18; else if (!solid(xx + 1, yy)) d -= 12;
                    Color& c = dst[yy * b.width + xx];
                    c.r = (unsigned char)std::max(0, std::min(255, c.r + d)); c.g = (unsigned char)std::max(0, std::min(255, c.g + d)); c.b = (unsigned char)std::max(0, std::min(255, c.b + d));
                }
            UnloadImage(b);
        }
        if (it.style & 64) ImageFlipHorizontal(&im);
        // the shadow it throws on the wall behind: its outline, dropped two cells down and to the right (the light is upper left), more for things stood out from the wall
        int an = it.data < DK_DRESSER ? 0 : decorAnchor(it.data, var), dx = an == 0 ? 3 : 2, dy = an == 0 ? 1 : 2;
        bool none = it.data < DK_DRESSER || it.data == DK_BLOOD || it.data == DK_SPIKE || it.data == DK_YARD || it.data == DK_LEANSHIELD || it.data == DK_COBWEB || it.data == DK_LEAK;
        Texture2D sh = {};
        if (!none)
        {
            Image s = GenImageColor(im.width + dx, im.height + dy, BLANK);
            Color* src = (Color*)im.data; Color* dst = (Color*)s.data;
            for (int yy = 0; yy < im.height; yy++)
                for (int xx = 0; xx < im.width; xx++)
                    if (src[yy * im.width + xx].a > 0) dst[(yy + dy) * s.width + xx + dx] = Color{0, 0, 0, 255};
            sh = LoadTextureFromImage(s);
            UnloadImage(s);
        }
        c = cache.emplace(key, Art{LoadTextureFromImage(im), sh}).first;
        UnloadImage(im);
    }
    const Texture2D& t = c->second.t;
    float w = t.width * 0.5f, h = t.height * 0.5f;
    int an = it.data < DK_DRESSER ? 0 : decorAnchor(it.data, var);
    float top = an == 0 ? y - h + 0.5f : (an == 1 ? y : y - h / 2); // a standing one has its foot just in the ground
    float left = std::floor((x - w / 2) * 2 + 0.5f) / 2; // on the cell grid
    top = std::floor(top * 2 + 0.5f) / 2;
    if (c->second.sh.id) DrawTexturePro(c->second.sh, {0, 0, (float)c->second.sh.width, (float)c->second.sh.height}, {left, top, c->second.sh.width * 0.5f, c->second.sh.height * 0.5f}, {0, 0}, 0, Color{255, 255, 255, an == 0 ? (unsigned char)58 : (unsigned char)78});
    DrawTexturePro(t, {0, 0, (float)t.width, (float)t.height}, {left, top, w, h}, {0, 0}, 0, it.data == DK_SPEARPOST ? Color{176, 176, 188, 255} : WHITE);
    if (it.data == DK_SPEARPOST && (var & 32)) // the cloth is tied to the lashing just under the butt
        drawPennant((it.style & 64) ? left + w - 5.5f * 0.5f : left + 6.0f * 0.5f, top + 6.5f * 0.5f, (it.style & 64) ? -1 : 1, var, (float)G.frame);
}

// The wyrm-head dart trap: its slab sunk 3 units into the wall, the head jutting out along `dir`, its jaws
// (sprite pixel 15.5 down) at mouthY. Shakes and reddens for `hit` frames after a blow.
void drawDartTrap(float faceX, float mouthY, int dir, bool broken, int hit, int hp)
{
    const Texture2D& t = hdTexture(broken ? HD_DART_TRAP_BROKEN : HD_DART_TRAP);
    float w = t.width * 0.5f, h = t.height * 0.5f, j = hit > 0 ? (hit % 2 ? 0.5f : -0.5f) : 0;
    float left = dir > 0 ? faceX - 3 : faceX + 3 - w;
    Color tint = hit > 0 ? Color{255, 190, 180, 255} : WHITE;
    DrawTexturePro(t, {0, 0, (float)(dir > 0 ? t.width : -t.width), (float)t.height}, {left + j, mouthY - 7.75f, w, h}, {0, 0}, 0, tint);
    if (broken) return;
    float hx = faceX + dir * 5.0f + j, hy = mouthY - 1.0f; // cracks across the head, one more for every blow it's taken
    const Color CR = {34, 30, 28, 255};
    if (hp <= 2) { DrawLineEx({hx - dir * 2.5f, hy - 4}, {hx - dir * 0.5f, hy - 1.5f}, 0.7f, CR); DrawLineEx({hx - dir * 0.5f, hy - 1.5f}, {hx + dir * 1.0f, hy - 2.5f}, 0.7f, CR); }
    if (hp <= 1) { DrawLineEx({hx + dir * 2.5f, hy + 2.5f}, {hx, hy + 0.5f}, 0.7f, CR); DrawLineEx({hx, hy + 0.5f}, {hx - dir * 2.0f, hy + 3.0f}, 0.7f, CR); }
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

static const RigSpec* rigFor(int type);
static const AnimSheet& animFor(int type);
static void drawAnimMob(const Mob& m, const AnimSheet& A, int camX, int camY);

void drawMobAnimated(const Mob& m, int camX, int camY)
{
    drawAnimMob(m, animFor(m.type), camX, camY); // painted frame by frame (tools/anim.py)
    if (m.hp < m.maxHp && !m.boss)
    {
        int bw = std::max(m.w, 10);
        int bx = (int)(std::floor(m.x) + m.w * 0.5f - camX - bw / 2.0f), by = (int)(std::floor(m.y) - camY - 7);
        DrawRectangle(bx - 1, by - 1, bw + 2, 4, {10, 6, 10, 200});
        DrawRectangle(bx, by, (int)(bw * std::max(0.0f, m.hp / m.maxHp)), 2, {230, 40, 40, 255});
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

// ---------------------------------------------------------------- limb rigs
// Creatures are drawn as parts (painted by tools/art_hd.py into sprites_hd.h) hung on a skeleton of joints.
// Alive, each frame of its sheet carries the joints (tools/anim.py). Dead, the same joints
// become a Verlet ragdoll: points joined by sticks (one per limb, plus braces holding shoulders and hips
// to the torso), knees, elbows and the neck kept within limits, every point colliding with the world. So
// a corpse falls in a heap with its arms and legs flopping, and a hard enough blow breaks sticks - limbs
// come off, spurting. Angles: 0 points down, positive turns towards the way the creature faces, PI is up.

enum { G_HEAD = 1, G_ARMF = 2, G_ARMN = 4, G_LEGF = 8, G_LEGN = 16, G_WAIST = 32, G_WEAPON = 64, G_MIN = 128 }; // what a cut can sever; G_MIN: a brace that only pushes apart

namespace Tune // every ragdoll tunable, in one place
{
const float UNIT = 0.5f;                  // world units per sprite pixel
const float GRAV = 0.2f, DAMP = 0.99f;    // per frame
const float FRICTION = 0.55f;
const float GROUND_DAMP = 0.82f;          // while anything touches the ground, all of it slows (or it rolls for ever)
const int TIMEOUT = 360;
const int DRIFT_WINDOW = 10; const float DRIFT_MIN = 0.3f; // (10 frames: a whole number of any 2- or 5-frame contact jitter)                  // frames after the last shove when it sleeps anyway
const int ITERS = 6;                      // constraint passes per step
const float SETTLE_SPEED = 0.05f;         // slower than this (units/frame) for SETTLE_FRAMES and it sleeps
const int SETTLE_FRAMES = 60, MAX_ACTIVE = 12, MAX_CORPSES = 40;
const float FOLD_MIN = 0.55f;             // a limb can't fold tighter than this share of its length, nor the head onto the chest
const float JET = 2.6f, JET_FADE = 0.993f, SEEP_FADE = 0.997f;
}

static const RigSpec* rigFor(int type) // its corpse's parts, cut from its painting
{
    switch (type)
    {
    case E_GOBLIN: return &RIG_A_GOBLIN;
    case E_BOMBER: return &RIG_A_BOMBER;
    case E_SKELETON: return &RIG_A_SKELETON;
    case E_ARCHER: return &RIG_A_ARCHER;
    case E_BAT: return &RIG_A_BAT;
    case E_CULTIST: return &RIG_A_CULTIST;
    case E_KNIGHT: return &RIG_A_KNIGHT;
    case E_IMP: return &RIG_A_IMP;
    case E_WRAITH: return &RIG_A_WRAITH;
    case E_GOLEM: return &RIG_A_GOLEM;
    case E_WOLF: return &RIG_A_WOLF;
    case E_REDCAP: return &RIG_A_REDCAP;
    case E_DRAUGR: return &RIG_A_DRAUGR;
    case E_TROLL: return &RIG_A_TROLL;
    case E_BANSHEE: return &RIG_A_BANSHEE;
    case E_KELPIE: return &RIG_A_KELPIE;
    case E_GUARD: return &RIG_A_GUARD;
    case E_RISEN: return &RIG_A_RISEN;
    case E_RAIDER: return &RIG_A_RAIDER;
    case E_BLACKKNIGHT: return &RIG_A_BLACKKNIGHT;
    case E_LICH: return &RIG_A_LICH;
    default: return nullptr; // the slime bursts
    }
}

static float boneLen(const RigPart& p) { return p.spr ? std::hypot(p.ex - p.px, p.ey - p.py) * Tune::UNIT : 0; }
static Vector2 dirA(float a) { return {std::sin(a), std::cos(a)}; }
static Vector2 lerpV(Vector2 a, Vector2 b, float t) { return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t}; }
static bool weaponNear(const RigSpec& R) { return !R.part[RS_SHIELD].spr || R.shieldFar; } // with a shield on the near arm, the weapon's in the far hand

// The limb segments a wound can sit on, per kind of rig (pairs of joints).
static const uint8_t SEGS[][2] = {{J_NECK, J_PELVIS}, {J_HEADB, J_HEADT}, {J_SHF, J_ELF}, {J_ELF, J_HAF}, {J_SHN, J_ELN}, {J_ELN, J_HAN},
                                  {J_HIPF, J_KNF}, {J_KNF, J_FTF}, {J_HIPN, J_KNN}, {J_KNN, J_FTN}};
static int segCount(const RigSpec& R) { return R.kind == RK_BAT ? 1 : (R.part[RS_THIGH].spr ? 10 : 6); }

// ---- textures: each part, and a white silhouette of it for the flash when struck
struct PartTex { Texture2D tex, white; };
// Palette alpha 254 takes the armour's metal (grey 156 becomes the metal's own colour), 253 its glow.
static Color recolour(Color c)
{
    Color to = c.a == 254 ? rigMetal : (c.a == 253 ? rigGlow : BLANK);
    auto tone = [](unsigned char v, unsigned char t) { return (unsigned char)std::min(255.0f, v * (0.2f + 0.8f * t / 156.0f)); };
    if (to.a) c = {tone(c.r, to.r), tone(c.g, to.g), tone(c.b, to.b), 255};
    c.a = 255;
    return c;
}
static const PartTex& partTex(const HDSprite* s)
{
    static std::map<std::tuple<const HDSprite*, unsigned, unsigned>, PartTex> cache;
    auto key = std::make_tuple(s, (unsigned)ColorToInt(rigMetal), (unsigned)ColorToInt(rigGlow));
    auto it = cache.find(key);
    if (it != cache.end()) return it->second;
    Image a = GenImageColor(s->w, s->h, BLANK), b = GenImageColor(s->w, s->h, BLANK);
    for (int y = 0; y < s->h; y++)
        for (int x = 0; x < s->w; x++)
        {
            char ch = s->rows[y][x];
            if (ch == '.') continue;
            ((Color*)a.data)[y * s->w + x] = recolour(s->pal[std::strchr(HD_ALPHABET, ch) - HD_ALPHABET]);
            ((Color*)b.data)[y * s->w + x] = WHITE;
        }
    PartTex t{LoadTextureFromImage(a), LoadTextureFromImage(b)};
    UnloadImage(a);
    UnloadImage(b);
    return cache[key] = t;
}

// One part, hung from joint `a` with its bone pointing at `b` (both in render-texture units).
static void drawPart(const RigPart& rp, Vector2 a, Vector2 b, int facing, Color tint, bool white)
{
    if (!rp.spr) return;
    const PartTex& t = partTex(rp.spr);
    const float w = (float)rp.spr->w, h = (float)rp.spr->h, U = Tune::UNIT;
    float px = facing < 0 ? w - rp.px : rp.px, ex = facing < 0 ? w - rp.ex : rp.ex;
    float a0 = std::atan2(rp.ey - rp.py, ex - px), aw = std::atan2(b.y - a.y, b.x - a.x);
    if (std::fabs(b.x - a.x) + std::fabs(b.y - a.y) < 1e-4f) aw = a0;
    DrawTexturePro(white ? t.white : t.tex, {0, 0, facing < 0 ? -w : w, h}, {a.x, a.y, w * U, h * U}, {px * U, rp.py * U}, (aw - a0) * RAD2DEG, tint);
}

// A part fixed to another at an angle (a shield on the forearm, a wing on the shoulders): its bone runs from
// `at` along the local angle `ang` (in the creature's own facing-right frame).
static void drawAttached(const RigPart& rp, Vector2 at, float ang, int facing, Color tint, bool white)
{
    Vector2 d = dirA(ang);
    drawPart(rp, at, {at.x + d.x * facing * boneLen(rp), at.y + d.y * boneLen(rp)}, facing, tint, white);
}

static float localAng(Vector2 a, Vector2 b, int facing) { return std::atan2((b.x - a.x) * facing, b.y - a.y); } // a segment's angle, facing-right frame

static void bolt(Vector2 at, float ang, float len) // a crossbow bolt stuck in, its shaft and fletching out
{
    Vector2 d = {std::cos(ang), std::sin(ang)}, tail = {at.x - d.x * len, at.y - d.y * len};
    DrawLineEx(at, tail, 1.0f, {120, 86, 52, 255});
    DrawLineEx({tail.x + d.x * 1.5f, tail.y + d.y * 1.5f}, tail, 1.6f, {214, 206, 190, 255});
    DrawRectangle((int)at.x, (int)at.y, 1, 1, {150, 150, 158, 255});
}

// Draws a posed skeleton, back to front. `flap` swings bat and imp wings.
static void drawRig(const RigSpec& R, const Vector2* J, int f, Color tint, bool white, float flap)
{
    const RigPart* P = R.part;
    Color far = mul(tint, 0.68f);
    far.a = tint.a;
    if (R.kind == RK_BAT)
    {
        drawPart(P[RS_UARM], J[J_SHF], J[J_ELF], f, far, white);
        drawPart(P[RS_TORSO], J[J_NECK], J[J_PELVIS], f, tint, white);
        drawPart(P[RS_UARM], J[J_SHN], J[J_ELN], f, tint, white);
        return;
    }
    if (R.kind == RK_QUAD)
    {
        drawPart(P[RS_THIGH], J[J_HIPF], J[J_KNF], f, far, white); drawPart(P[RS_SHIN], J[J_KNF], J[J_FTF], f, far, white);
        drawPart(P[RS_UARM], J[J_SHF], J[J_ELF], f, far, white); drawPart(P[RS_FARM], J[J_ELF], J[J_HAF], f, far, white);
        drawPart(P[RS_TAIL], J[J_WB], J[J_WT], f, tint, white);
        drawPart(P[RS_TORSO], J[J_NECK], J[J_PELVIS], f, tint, white);
        drawPart(P[RS_HEAD], J[J_HEADB], J[J_HEADT], f, tint, white);
        drawPart(P[RS_THIGH], J[J_HIPN], J[J_KNN], f, tint, white); drawPart(P[RS_SHIN], J[J_KNN], J[J_FTN], f, tint, white);
        drawPart(P[RS_UARM], J[J_SHN], J[J_ELN], f, tint, white); drawPart(P[RS_FARM], J[J_ELN], J[J_HAN], f, tint, white);
        return;
    }
    bool wn = weaponNear(R);
    float torso = localAng(J[J_PELVIS], J[J_NECK], f); // PI when upright
    if (P[RS_WING].spr) drawAttached(P[RS_WING], J[J_SHF], torso - PI - 2.0f + flap * 0.8f, f, far, white);
    if (!wn) drawPart(P[RS_WEAPON], J[J_WB], J[J_WT], f, far, white);
    drawPart(P[RS_UARM], J[J_SHF], J[J_ELF], f, far, white);
    drawPart(P[RS_FARM], J[J_ELF], J[J_HAF], f, far, white);
    drawPart(P[RS_THIGH], J[J_HIPF], J[J_KNF], f, far, white);
    drawPart(P[RS_SHIN], J[J_KNF], J[J_FTF], f, far, white);
    drawPart(P[RS_TORSO], J[J_NECK], J[J_PELVIS], f, tint, white);
    drawPart(P[RS_HEAD], J[J_HEADB], J[J_HEADT], f, tint, white);
    if (P[RS_SHIELD].spr && R.shieldFar) drawAttached(P[RS_SHIELD], J[J_HAF], localAng(J[J_ELF], J[J_HAF], f) + R.sgrip, f, tint, white);
    drawPart(P[RS_THIGH], J[J_HIPN], J[J_KNN], f, tint, white);
    drawPart(P[RS_SHIN], J[J_KNN], J[J_FTN], f, tint, white);
    if (wn) drawPart(P[RS_WEAPON], J[J_WB], J[J_WT], f, tint, white);
    drawPart(P[RS_UARM], J[J_SHN], J[J_ELN], f, tint, white);
    drawPart(P[RS_FARM], J[J_ELN], J[J_HAN], f, tint, white);
    if (P[RS_SHIELD].spr && !R.shieldFar) drawAttached(P[RS_SHIELD], J[J_HAN], localAng(J[J_ELN], J[J_HAN], f) + R.sgrip, f, tint, white);
    if (P[RS_WING].spr) drawAttached(P[RS_WING], J[J_SHN], torso - PI - 1.7f + flap * 0.8f, f, tint, white);
}

// ---------------------------------------------------------------- frame-by-frame animation
// Creatures painted whole, frame by frame (tools/anim.py -> sprites_anim.h): every pose is its own crisp picture,
// nothing rotated. Each frame also carries its rig joints, so wounds and bolts sit on the limbs, and when the
// creature dies its corpse (the rig parts above, cut from the same painting) starts from the pose it died in.

static const AnimSheet& animFor(int type)
{
    switch (type)
    {
    case E_GOBLIN: return ANIM_GOBLIN;
    case E_BOMBER: return ANIM_BOMBER;
    case E_SKELETON: return ANIM_SKELETON;
    case E_ARCHER: return ANIM_ARCHER;
    case E_BAT: return ANIM_BAT;
    case E_SLIME: return ANIM_SLIME;
    case E_CULTIST: return ANIM_CULTIST;
    case E_KNIGHT: return ANIM_KNIGHT;
    case E_IMP: return ANIM_IMP;
    case E_WRAITH: return ANIM_WRAITH;
    case E_GOLEM: return ANIM_GOLEM;
    case E_WOLF: return ANIM_WOLF;
    case E_REDCAP: return ANIM_REDCAP;
    case E_DRAUGR: return ANIM_DRAUGR;
    case E_TROLL: return ANIM_TROLL;
    case E_BANSHEE: return ANIM_BANSHEE;
    case E_KELPIE: return ANIM_KELPIE;
    case E_GUARD: return ANIM_GUARD;
    case E_RISEN: return ANIM_RISEN;
    case E_SERPENT: return ANIM_SERPENT;
    case E_SCORPION: return ANIM_SCORPION;
    case E_RAIDER: return ANIM_RAIDER;
    case E_BLACKKNIGHT: return ANIM_BLACKKNIGHT;
    default: return ANIM_LICH;
    }
}

static const PartTex& sheetTex(const AnimSheet& A)
{
    static std::map<std::tuple<const AnimSheet*, unsigned, unsigned>, PartTex> cache;
    auto key = std::make_tuple(&A, (unsigned)ColorToInt(rigMetal), (unsigned)ColorToInt(rigGlow));
    auto it = cache.find(key);
    if (it != cache.end()) return it->second;
    int n = A.fw * A.fh * A.frames;
    Image a = GenImageColor(A.fw, A.fh * A.frames, BLANK), b = GenImageColor(A.fw, A.fh * A.frames, BLANK);
    for (int i = 0; i < n; i++)
    {
        if (A.px[i] == '.') continue;
        ((Color*)a.data)[i] = recolour(A.pal[std::strchr(HD_ALPHABET, A.px[i]) - HD_ALPHABET]);
        ((Color*)b.data)[i] = WHITE;
    }
    PartTex t{LoadTextureFromImage(a), LoadTextureFromImage(b)};
    UnloadImage(a);
    UnloadImage(b);
    return cache[key] = t;
}

// A frame of clip `clip`, `k` (0..1) of the way through it; a creature without the clip stands idle.
static int clipFrame(const AnimSheet& A, int clip, float k)
{
    int n = A.clip[clip][1];
    if (!n) clip = AC_IDLE, n = A.clip[AC_IDLE][1];
    return A.clip[clip][0] + std::min(n - 1, std::max(0, (int)(k * n)));
}

static int mobFrame(const Mob& m, const AnimSheet& A)
{
    const EnemyDef& d = ENEMIES[m.type];
    bool flying = d.ai == AI_FLY || d.ai == AI_FLYCAST || d.ai == AI_BOSS_LICH;
    bool ranged = d.ai == AI_RANGED || d.ai == AI_BOMB || d.ai == AI_FLYCAST || d.ai == AI_BOSS_LICH;
    bool leaps = m.type == E_WOLF || m.type == E_KELPIE || m.type == E_SERPENT || m.type == E_SLIME || m.type == E_BAT; // their strike is a lunge, pounce or dive
    if (m.type == E_BLACKKNIGHT && (m.state == 2 || m.state == 3)) // the blade lowered like a lance, then the charge
    {
        int n = A.clip[AC_CHARGE][1] - 1;
        return m.state == 2 ? A.clip[AC_CHARGE][0] : A.clip[AC_CHARGE][0] + 1 + (int)(std::fmod(m.anim, 2.6f) / 2.6f * n) % n;
    }
    if (m.atkPhase == 1) return clipFrame(A, AC_WINDUP, 1 - (float)m.atkT / windupFor(m.type));
    if (m.atkPhase == 2) return clipFrame(A, AC_STRIKE, 1 - m.atkT / (m.type == E_SLIME ? 45.0f : m.type == E_BAT ? 32.0f : leaps ? 30.0f : 6.0f));
    if (m.atkPhase == 3) return clipFrame(A, AC_RECOVER, 1 - m.atkT / (leaps ? 18.0f : m.w > 16 ? 22.0f : 14.0f));
    if (m.hitT > 0) return clipFrame(A, AC_HURT, 1 - m.hitT / 14.0f);
    if (m.attackT > 0) return clipFrame(A, A.clip[AC_CAST][1] ? AC_CAST : AC_STRIKE, 1 - m.attackT / 15.0f); // the shot, the throw, the spell let go
    if (ranged && m.aggro && m.los && m.cd > 0 && m.cd <= 16) return clipFrame(A, AC_WINDUP, 1 - m.cd / 16.0f); // drawing, aiming, gathering it
    if (flying) return clipFrame(A, AC_IDLE, std::fmod((G.frame + m.id * 7) / 24.0f, 1.0f)); // wingbeats, drifting
    if (!m.onGround && !m.inLiquid) return clipFrame(A, AC_WALK, 0.3f);
    if (m.squash < 0.9f && A.clip[AC_LAND][1]) return clipFrame(A, AC_LAND, 0); // just come down hard
    if (std::fabs(m.vx) > 0.08f) return clipFrame(A, AC_WALK, std::fmod(m.anim, 2.6f) / 2.6f); // a stride per ~9 units walked
    return clipFrame(A, AC_IDLE, std::fmod((G.frame + m.id * 13) / 54.0f, 1.0f));
}

// Frame `fr` with its feet at (x, y) (render-texture units), snapped to whole pixels.
static void drawSheet(const AnimSheet& A, int fr, float x, float y, int facing, Color tint, bool white)
{
    const PartTex& t = sheetTex(A);
    const float U = Tune::UNIT, w = (float)A.fw, h = (float)A.fh;
    float left = std::floor((facing < 0 ? x - (w - A.ax) * U : x - A.ax * U) / U) * U, top = std::floor((y - A.ay * U) / U) * U;
    DrawTexturePro(white ? t.white : t.tex, {0, fr * h, facing < 0 ? -w : w, h}, {left, top, w * U, h * U}, {0, 0}, 0, tint);
}

// A villager (0 man, 1 woman, 2 child) with its feet at (x, y) in render-texture units; its coat takes `coat`'s colour.
int folkLooks(int kind) { return kind == 0 || kind == 1 ? 4 : 3; }
void drawFolk(int kind, int look, float anim, bool walking, int dir, float x, float y, Color coat, int seed)
{
    static const AnimSheet* const FOLK[3][4] = {{&ANIM_FOLK_M0, &ANIM_FOLK_M1, &ANIM_FOLK_M2, &ANIM_FOLK_M3},
                                                {&ANIM_FOLK_F0, &ANIM_FOLK_F1, &ANIM_FOLK_F2, &ANIM_FOLK_F3},
                                                {&ANIM_FOLK_C0, &ANIM_FOLK_C1, &ANIM_FOLK_C2, &ANIM_FOLK_C0}};
    const AnimSheet& A = *FOLK[kind % 3][look % folkLooks(kind)];
    int fr = walking ? clipFrame(A, AC_WALK, std::fmod(anim, 2.6f) / 2.6f) : clipFrame(A, AC_IDLE, std::fmod((G.frame + seed * 17) / 70.0f, 1.0f));
    Color keep = rigMetal;
    rigMetal = coat;
    drawSheet(A, fr, x, y, dir, WHITE, false);
    rigMetal = keep;
}

static void animJoints(const AnimSheet& A, int fr, float x, float y, int facing, Vector2* J)
{
    const float* j = A.joints + fr * RJ_COUNT * 2;
    for (int i = 0; i < RJ_COUNT; i++) J[i] = {x + j[i * 2] * Tune::UNIT * facing, y + j[i * 2 + 1] * Tune::UNIT};
}

// A living creature's joints in world units, from the frame it's showing.
static void poseJoints(const Mob& m, Vector2* J)
{
    const AnimSheet& A = animFor(m.type);
    animJoints(A, mobFrame(m, A), m.cx(), m.y + m.h, m.facing, J);
}

static void drawAnimMob(const Mob& m, const AnimSheet& A, int camX, int camY)
{
    int fr = mobFrame(m, A);
    float x = m.cx() - camX, y = m.y + m.h - camY;
    drawSheet(A, fr, x, y, m.facing, coatTint(m), m.hurtFlash > 0);
    const RigSpec* R = rigFor(m.type);
    if (!m.nWounds || !R) return;
    Vector2 J[RJ_COUNT];
    animJoints(A, fr, x, y, m.facing, J);
    for (int k = 0; k < m.nWounds; k++)
        if (m.woundK[k] == WK_BOLT && m.woundS[k] < segCount(*R))
        {
            Vector2 a = J[SEGS[m.woundS[k]][0]], b = J[SEGS[m.woundS[k]][1]];
            bolt(lerpV(a, b, m.woundT[k]), std::atan2(b.y - a.y, b.x - a.x) + m.woundA[k], 6);
        }
}

// Shatter a creature into its pixels, a particle per world unit, from the frame it died in.
void burstSprite(const Mob& m)
{
    const AnimSheet& A = animFor(m.type);
    const char* px = A.px + (size_t)mobFrame(m, A) * A.fw * A.fh;
    const float U = Tune::UNIT;
    for (int j = 0; j < A.fh; j += 2)
        for (int i = 0; i < A.fw; i += 2)
        {
            char ch = px[j * A.fw + i];
            if (ch == '.') continue;
            Color c = recolour(A.pal[std::strchr(HD_ALPHABET, ch) - HD_ALPHABET]);
            float x = m.cx() + (i - A.ax) * U * m.facing, y = m.y + m.h + (j - A.ay) * U;
            float dx = x - m.cx(), dy = y - m.cy();
            spawnParticle(x, y, dx * 0.08f + frange(-0.6f, 0.6f) + m.vx * 0.5f, dy * 0.05f + frange(-2.0f, -0.4f), irange(30, 70), c, 0.12f);
        }
}

static Color coatTint(const Mob& m)
{
    Color c = WHITE;
    if (m.bloody) c = {255, 160, 150, 255};
    if (m.oily) c = {160, 150, 120, 255};
    if (m.wet) c = {190, 210, 255, 255};
    if (m.chill > 0) c = {180, 220, 255, 255};
    if (m.poison > 0) c = {170, 230, 150, 255};
    if (m.burn > 0) c = {255, 190, 150, 255};
    if (m.type == E_BLACKKNIGHT && m.state == 2 && (G.frame / 3) % 2) c = {255, 90, 90, 255};
    if (m.type == E_WRAITH || m.type == E_BANSHEE) c.a = 200;
    return c;
}

bool rigLocate(const Mob& m, Vector2 at, float ang, uint8_t& seg, float& t, float& rel)
{
    const RigSpec* R = rigFor(m.type);
    if (!R) return false;
    Vector2 J[RJ_COUNT];
    poseJoints(m, J);
    float best = 1e9f;
    for (int s = 0; s < segCount(*R); s++)
    {
        Vector2 a = J[SEGS[s][0]], b = J[SEGS[s][1]], ab = {b.x - a.x, b.y - a.y};
        float l2 = ab.x * ab.x + ab.y * ab.y + 1e-4f, u = clampf(((at.x - a.x) * ab.x + (at.y - a.y) * ab.y) / l2, 0.1f, 0.9f);
        float dx = a.x + ab.x * u - at.x, dy = a.y + ab.y * u - at.y, d = dx * dx + dy * dy;
        if (d < best) { best = d; seg = (uint8_t)s; t = u; rel = ang - std::atan2(ab.y, ab.x); }
    }
    return true;
}

Vector2 rigWoundPos(const Mob& m, int k)
{
    const RigSpec* R = rigFor(m.type);
    if (!R || m.woundS[k] >= segCount(*R)) return {m.cx(), m.cy()};
    Vector2 J[RJ_COUNT];
    poseJoints(m, J);
    return lerpV(J[SEGS[m.woundS[k]][0]], J[SEGS[m.woundS[k]][1]], m.woundT[k]);
}

// ---------------------------------------------------------------- corpses: the rig as a ragdoll

void clearCorpses() { G.corpses.clear(); }

static void addStick(Corpse& c, int a, int b, int grp)
{
    if (c.ns >= RST_MAX) return;
    c.sa[c.ns] = (uint8_t)a; c.sb[c.ns] = (uint8_t)b; c.sg[c.ns] = (uint8_t)grp;
    c.sl[c.ns] = std::hypot(c.p[b].x - c.p[a].x, c.p[b].y - c.p[a].y);
    c.used |= (1u << a) | (1u << b);
    c.ns++;
}

// A brace across a joint that only stops the limb folding flat: no closer than FOLD_MIN of the two bones.
static void addFold(Corpse& c, int a, int mid, int b, int grp, float frac = Tune::FOLD_MIN)
{
    addStick(c, a, b, grp | G_MIN);
    c.sl[c.ns - 1] = frac * (std::hypot(c.p[mid].x - c.p[a].x, c.p[mid].y - c.p[a].y) + std::hypot(c.p[b].x - c.p[mid].x, c.p[b].y - c.p[mid].y));
}

static void addWound(Corpse& c, int a, int b, float t, float ang, int kind, float pressure)
{
    if (c.nw >= RCW_MAX) return;
    c.w[c.nw++] = {(uint8_t)a, (uint8_t)b, t, ang, kind, pressure};
}

// Breaks the sticks of `grp`, and leaves both ends of the cut spurting.
static void sever(Corpse& c, int grp)
{
    if (c.cut & grp) return;
    c.cut |= grp;
    const RigSpec& R = *c.rig;
    const float pr = 1.0f;
    switch (grp)
    {
    case G_HEAD: addWound(c, J_NECK, J_PELVIS, 0, PI, WK_CUT, pr); addWound(c, J_HEADB, J_HEADT, 0, PI, WK_CUT, pr * 0.7f); break;
    case G_ARMF: addWound(c, J_SHF, J_ELF, 0, PI, WK_CUT, pr * 0.7f); addWound(c, J_NECK, J_PELVIS, R.shoulder, -PI / 2, WK_CUT, pr * 0.8f); break;
    case G_ARMN: addWound(c, J_SHN, J_ELN, 0, PI, WK_CUT, pr * 0.7f); addWound(c, J_NECK, J_PELVIS, R.shoulder, PI / 2, WK_CUT, pr * 0.8f); break;
    case G_LEGF: addWound(c, J_HIPF, J_KNF, 0, PI, WK_CUT, pr * 0.7f); addWound(c, J_PELVIS, J_NECK, 0, PI, WK_CUT, pr * 0.8f); break;
    case G_LEGN: addWound(c, J_HIPN, J_KNN, 0, PI, WK_CUT, pr * 0.7f); addWound(c, J_PELVIS, J_NECK, 0, PI, WK_CUT, pr * 0.8f); break;
    case G_WAIST: addWound(c, J_PELVIS, J_NECK, 0, PI, WK_CUT, pr); addWound(c, J_HIPN, J_KNN, 0, PI, WK_CUT, pr * 0.8f); break;
    default: break;
    }
}

void spawnCorpse(const Mob& m)
{
    const RigSpec* R = rigFor(m.type);
    if (!R || m.type == E_WRAITH || m.type == E_BANSHEE) { burstSprite(m); return; } // ghosts and slimes come apart in pixels
    const EnemyDef& d = ENEMIES[m.type];
    Corpse c;
    c.rig = R;
    c.facing = m.facing;
    c.gore = d.gore;
    poseJoints(m, c.p);
    const RigPart* P = R->part;
    // the sticks: one per limb, braces holding shoulders and hips to the torso, the weapon fixed in the hand
    addStick(c, J_NECK, J_PELVIS, 0);
    if (R->kind == RK_BAT)
    {
        addStick(c, J_SHF, J_ELF, 0); addStick(c, J_SHF, J_NECK, G_ARMF); addStick(c, J_SHF, J_PELVIS, G_ARMF);
        addStick(c, J_SHN, J_ELN, 0); addStick(c, J_SHN, J_NECK, G_ARMN); addStick(c, J_SHN, J_PELVIS, G_ARMN);
    }
    else
    {
        addStick(c, J_HEADB, J_HEADT, 0); addStick(c, J_HEADB, J_NECK, G_HEAD);
        addStick(c, J_SHF, J_ELF, 0); addStick(c, J_ELF, J_HAF, 0); addStick(c, J_SHF, J_NECK, G_ARMF); addStick(c, J_SHF, J_PELVIS, G_ARMF);
        addStick(c, J_SHN, J_ELN, 0); addStick(c, J_ELN, J_HAN, 0); addStick(c, J_SHN, J_NECK, G_ARMN); addStick(c, J_SHN, J_PELVIS, G_ARMN);
        if (P[RS_THIGH].spr)
        {
            int legF = R->kind == RK_QUAD ? G_LEGF : G_LEGF | G_WAIST, legN = R->kind == RK_QUAD ? G_LEGN : G_LEGN | G_WAIST;
            addStick(c, J_HIPF, J_KNF, 0); addStick(c, J_KNF, J_FTF, 0); addStick(c, J_HIPF, J_NECK, legF); addStick(c, J_HIPF, J_PELVIS, legF);
            addStick(c, J_HIPN, J_KNN, 0); addStick(c, J_KNN, J_FTN, 0); addStick(c, J_HIPN, J_NECK, legN); addStick(c, J_HIPN, J_PELVIS, legN);
            if (R->kind != RK_QUAD) addStick(c, J_HIPF, J_HIPN, G_LEGF | G_LEGN);
        }
        addFold(c, J_SHF, J_ELF, J_HAF, G_ARMF);
        addFold(c, J_SHN, J_ELN, J_HAN, G_ARMN);
        if (P[RS_THIGH].spr)
        {
            addFold(c, J_HIPF, J_KNF, J_FTF, R->kind == RK_QUAD ? G_LEGF : G_LEGF | G_WAIST);
            addFold(c, J_HIPN, J_KNN, J_FTN, R->kind == RK_QUAD ? G_LEGN : G_LEGN | G_WAIST);
        }
        if (R->kind != RK_QUAD) addFold(c, J_PELVIS, J_NECK, J_HEADT, G_HEAD, 0.85f); // the head can't loll onto the chest
        if (R->kind == RK_QUAD && P[RS_TAIL].spr) { addStick(c, J_WB, J_WT, 0); addStick(c, J_WB, J_PELVIS, G_WEAPON); addStick(c, J_WB, J_NECK, G_WEAPON); }
        else if (P[RS_WEAPON].spr)
        {
            bool wn = weaponNear(*R);
            int hand = wn ? J_HAN : J_HAF, elbow = wn ? J_ELN : J_ELF;
            addStick(c, J_WB, J_WT, 0); addStick(c, J_WB, hand, G_WEAPON); addStick(c, J_WT, elbow, G_WEAPON); addStick(c, J_WB, elbow, G_WEAPON);
        }
    }
    // how it goes down: carried on by its own momentum, and by the blow that killed it
    Vector2 v0 = {m.vx * 0.8f, std::min(m.vy, 0.0f)}, dir = {std::cos(m.lastAng), std::sin(m.lastAng)};
    Vector2 vel[RJ_COUNT];
    for (int i = 0; i < RJ_COUNT; i++) vel[i] = v0;
    auto push = [&](float k, float up, float spread) {
        for (int i = 0; i < RJ_COUNT; i++)
        {
            float s = 1 + frange(-spread, spread);
            vel[i].x += dir.x * k * s;
            vel[i].y += dir.y * k * s - up * s;
        }
    };
    bool limbs = R->kind == RK_BIPED || R->kind == RK_QUAD;
    switch (m.lastHit)
    {
    case HK_BLUNT: push(4.0f, 2.5f, 0.15f); vel[J_NECK].x += dir.x * 1.5f; break; // a hammer sends the body flying
    case HK_BLAST:
        push(1.5f + m.lastK * 0.3f, 1.5f, 0.45f);
        if (limbs)
        {
            static const int GROUPS[] = {G_HEAD, G_ARMF, G_ARMN, G_LEGF, G_LEGN, G_WAIST};
            for (int n = irange(2, 4); n > 0; n--) sever(c, GROUPS[irand(R->kind == RK_QUAD ? 5 : 6)]);
        }
        break;
    case HK_SLASH:
        push(1.0f, 0.5f, 0.1f);
        if (limbs && chance(3)) sever(c, R->kind == RK_BIPED && !chance(4) && P[RS_THIGH].spr ? G_WAIST : G_HEAD); // cut through the middle, or the neck
        break;
    case HK_CHOP:
        push(1.2f, 0.6f, 0.1f);
        if (limbs && chance(2)) { int r = irand(3); sever(c, r == 0 ? G_HEAD : r == 1 ? (chance(2) ? G_ARMN : G_ARMF) : (chance(2) ? G_LEGN : G_LEGF)); }
        break;
    case HK_PIERCE: case HK_BOLT: push(1.0f, 0.4f, 0.2f); break;
    default: vel[J_NECK].x += (m.vx >= 0 ? 1 : -1) * 0.6f; break; // it topples
    }
    if (limbs && P[RS_WEAPON].spr && R->kind == RK_BIPED && !chance(3)) sever(c, G_WEAPON); // the weapon falls from its hand
    for (int i = 0; i < RJ_COUNT; i++) c.pp[i] = {c.p[i].x - vel[i].x, c.p[i].y - vel[i].y};
    // its wounds: stabs and bolts carried over from life, and a seep if nothing else bleeds
    for (int k = 0; k < m.nWounds; k++)
        if (m.woundS[k] < segCount(*R)) addWound(c, SEGS[m.woundS[k]][0], SEGS[m.woundS[k]][1], m.woundT[k], m.woundA[k], m.woundK[k], 0.6f);
    if (c.nw == 0 && d.gore == CellMaterial::Blood) addWound(c, J_NECK, J_PELVIS, 0.3f, PI / 2, WK_PIERCE, 0.45f);
    // whatever reached into a wall as it died starts clear of it
    for (int i = 0; i < RJ_COUNT; i++)
        if ((c.used >> i) & 1)
            for (int tries = 0; tries < 6 && isSolid((int)std::floor(c.p[i].x), (int)std::floor(c.p[i].y)); tries++) { c.p[i].y -= 1; c.pp[i].y -= 1; }
    G.corpses.push_back(c);
    if ((int)G.corpses.size() > Tune::MAX_CORPSES) G.corpses.erase(G.corpses.begin()); // the oldest of the dead are cleared away
}

// Out of the rock the shortest way; `respond` also bounces and rubs off its speed. Returns the grip of
// what it hit (0: nothing).
static float collidePoint(Vector2& p, Vector2& pp, bool respond)
{
    int x = (int)std::floor(p.x), y = (int)std::floor(p.y);
    if (!isSolid(x, y)) return 0;
    float g = gripAt(x, y);
    Vector2 v = {p.x - pp.x, p.y - pp.y}, was = p;
    bool up = false;
    for (int d = 1; d <= 4; d++)
    {
        if (!isSolid(x, y - d)) { p.y = (float)(y - d + 1) - 0.01f; up = true; break; }
        if (!isSolid(x - d, y)) { p.x = (float)(x - d + 1) - 0.01f; break; }
        if (!isSolid(x + d, y)) { p.x = (float)(x + d) + 0.01f; break; }
        if (!isSolid(x, y + d)) { p.y = (float)(y + d) + 0.01f; break; }
    }
    if (!respond) { pp.x += p.x - was.x; pp.y += p.y - was.y; return g; } // a push-out moves it, it doesn't fling it
    if (up) { pp.y = p.y; pp.x = p.x - v.x * (1 - std::min(1.0f, Tune::FRICTION * g)); } // landing: no bounce, and it rubs along the ground
    else if (p.x != was.x) pp.x = p.x;
    else pp.y = p.y;
    return g;
}

static void stepCorpse(Corpse& c)
{
    Vector2 start[RJ_COUNT];
    for (int i = 0; i < RJ_COUNT; i++)
    {
        start[i] = c.p[i];
        if (!((c.used >> i) & 1)) continue;
        Vector2 v = {(c.p[i].x - c.pp[i].x) * Tune::DAMP, (c.p[i].y - c.pp[i].y) * Tune::DAMP};
        c.pp[i] = c.p[i];
        c.p[i] = {c.p[i].x + v.x, c.p[i].y + v.y + Tune::GRAV};
    }
    for (int it = 0; it < Tune::ITERS; it++)
    {
        for (int s = 0; s < c.ns; s++)
        {
            if (c.sg[s] & c.cut) continue;
            Vector2& a = c.p[c.sa[s]];
            Vector2& b = c.p[c.sb[s]];
            float dx = b.x - a.x, dy = b.y - a.y, dd = std::sqrt(dx * dx + dy * dy);
            if (dd < 1e-4f || ((c.sg[s] & G_MIN) && dd >= c.sl[s])) continue;
            float k = (dd - c.sl[s]) / dd * 0.5f;
            a.x += dx * k; a.y += dy * k;
            b.x -= dx * k; b.y -= dy * k;
        }
        float touched = 0; // the grippiest ground it's on
        for (int i = 0; i < RJ_COUNT; i++)
            if ((c.used >> i) & 1) touched = std::max(touched, collidePoint(c.p[i], c.pp[i], it == Tune::ITERS - 1));
        if (touched > 0 && it == Tune::ITERS - 1)
        {
            float damp = 1 - std::min(1.0f, (1 - Tune::GROUND_DAMP) * touched); // ice barely slows it
            for (int i = 0; i < RJ_COUNT; i++)
                if ((c.used >> i) & 1) { c.pp[i].x = c.p[i].x - (c.p[i].x - c.pp[i].x) * damp; c.pp[i].y = c.p[i].y - (c.p[i].y - c.pp[i].y) * damp; }
        }
    }
    float moved = 0; // how far it really went this step (gravity pressing it into the floor doesn't count)
    for (int i = 0; i < RJ_COUNT; i++)
        if ((c.used >> i) & 1) moved = std::max(moved, std::fabs(c.p[i].x - start[i].x) + std::fabs(c.p[i].y - start[i].y));
    c.rest = moved < Tune::SETTLE_SPEED ? c.rest + 1 : 0;
    if (++c.awakeT > Tune::TIMEOUT) c.rest = Tune::SETTLE_FRAMES + 1; // (or a timeout)
    if (c.awakeT % Tune::DRIFT_WINDOW == 0) // a body that ends up where it was a moment ago has stopped, whatever its joints are twitching at
    {
        float drift = 0;
        for (int i = 0; i < RJ_COUNT; i++)
            if ((c.used >> i) & 1) drift = std::max(drift, std::fabs(c.p[i].x - c.snap[i].x) + std::fabs(c.p[i].y - c.snap[i].y));
        if (c.awakeT > Tune::DRIFT_WINDOW && drift < Tune::DRIFT_MIN) c.rest = Tune::SETTLE_FRAMES + 1;
        for (int i = 0; i < RJ_COUNT; i++) c.snap[i] = c.p[i];
    }
}

static void bleedCorpse(Corpse& c)
{
    if (c.gore == CellMaterial::Empty) return;
    for (int k = 0; k < c.nw; k++)
    {
        CorpseWound& w = c.w[k];
        if (w.pressure < 0.03f) continue;
        Vector2 a = c.p[w.a], b = c.p[w.b], ab = {b.x - a.x, b.y - a.y};
        float l = std::sqrt(ab.x * ab.x + ab.y * ab.y) + 1e-4f;
        Vector2 at = lerpV(a, b, w.t), v = {c.p[w.a].x - c.pp[w.a].x, c.p[w.a].y - c.pp[w.a].y};
        float ang = std::atan2(ab.y / l, ab.x / l) + w.ang;
        int n;
        float sp;
        if (w.kind == WK_CUT) // a jet, wavering, weaker as it empties, pushing back on the body
        {
            n = w.pressure > 0.35f ? 2 : 1;
            sp = w.pressure * Tune::JET;
            ang += std::sin(c.life * 0.9f + k) * 0.25f;
            w.pressure *= Tune::JET_FADE;
            if (c.rest < Tune::SETTLE_FRAMES) { c.pp[w.a].x += std::cos(ang) * w.pressure * 0.01f; c.pp[w.a].y += std::sin(ang) * w.pressure * 0.01f; }
        }
        else // seeping
        {
            n = frand() < w.pressure * 0.35f ? 1 : 0;
            sp = 0.25f;
            w.pressure *= Tune::SEEP_FADE;
        }
        for (int i = 0; i < n && G.parts.size() < 4800; i++)
        {
            Cell cell;
            cell.material = c.gore;
            cell.shade = (uint8_t)xr();
            Particle q{at.x, at.y, v.x + std::cos(ang) * (sp + frange(0, 0.5f)) + frange(-0.15f, 0.15f), v.y + std::sin(ang) * (sp + frange(0, 0.5f)), 200, cellColor(cell, 0, 0), 0.15f};
            q.toCell = c.gore;
            G.parts.push_back(q);
        }
    }
}

void updateCorpses()
{
    int active = 0;
    for (int i = (int)G.corpses.size() - 1; i >= 0; i--) // newest first: past MAX_ACTIVE the oldest are frozen
    {
        Corpse& c = G.corpses[i];
        Vector2 m = c.p[J_PELVIS];
        if (std::fabs(m.x - G.camX - G.vw / 2) > G.vw + 300 || std::fabs(m.y - G.camY - G.vh / 2) > G.vh + 300) continue;
        c.life++;
        if (c.rest > Tune::SETTLE_FRAMES)
        {
            if ((G.frame + i) % 15 == 0) // asleep: has the ground gone from under it?
            {
                bool held = false;
                for (int j = 0; j < RJ_COUNT && !held; j++)
                    held = ((c.used >> j) & 1) && isSolid((int)std::floor(c.p[j].x), (int)std::floor(c.p[j].y + 1.5f));
                if (!held) { c.rest = 0; c.awakeT = 0; }
            }
        }
        else if (++active > Tune::MAX_ACTIVE) c.rest = Tune::SETTLE_FRAMES + 1;
        else stepCorpse(c);
        bleedCorpse(c);
    }
}

void drawCorpses(int camX, int camY)
{
    for (auto& c : G.corpses)
    {
        Vector2 m = c.p[J_PELVIS];
        if (m.x - camX < -80 || m.y - camY < -80 || m.x - camX > G.vw + 80 || m.y - camY > G.vh + 80) continue;
        Vector2 J[RJ_COUNT];
        for (int i = 0; i < RJ_COUNT; i++) J[i] = {c.p[i].x - camX, c.p[i].y - camY};
        drawRig(*c.rig, J, c.facing, WHITE, false, -0.6f);
        for (int k = 0; k < c.nw; k++)
            if (c.w[k].kind == WK_BOLT)
            {
                Vector2 a = J[c.w[k].a], b = J[c.w[k].b];
                bolt(lerpV(a, b, c.w[k].t), std::atan2(b.y - a.y, b.x - a.x) + c.w[k].ang, 6);
            }
    }
}

Vector2 corpseCentre(const Corpse& c) { return lerpV(c.p[J_NECK], c.p[J_PELVIS], 0.5f); }

// A shove: every point gets some of it, those near `at` the most.
void corpseKick(Corpse& c, Vector2 at, Vector2 v)
{
    for (int i = 0; i < RJ_COUNT; i++)
    {
        float d = std::hypot(c.p[i].x - at.x, c.p[i].y - at.y), k = 0.5f + 0.5f * clampf(1 - d / 12.0f, 0, 1);
        c.pp[i].x -= v.x * k;
        c.pp[i].y -= v.y * k;
    }
    c.rest = 0;
    c.awakeT = 0;
}

void shiftCorpses(float dx, float dy)
{
    for (auto& c : G.corpses)
        for (int i = 0; i < RJ_COUNT; i++) { c.p[i].x += dx; c.p[i].y += dy; c.pp[i].x += dx; c.pp[i].y += dy; }
}

void ragdollForPlayer()
{
    Mob& m = G.p.m;
    // the Viking's colours (tools/viking.py): a steel helm, a yellow tunic, dark trousers
    spawnRagdoll(m.cx(), m.y + m.h, (float)m.h + 3, m.vx + frange(-0.5f, 0.5f), m.vy - 1.5f, {152, 160, 180, 255}, {206, 132, 38, 255}, {50, 38, 70, 255}, false, CellMaterial::Blood);
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
    if (weapon3dDraw(w, at, ang, scale, centred)) return; // the 3D-modelled weapon (tools/weapons3d.py); the old painted art below is the fallback
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
    if (float l3 = weapon3dLength(w)) return l3;
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
        // coil back a little, then the whole sweep inside a few frames around the hit, easing out into the follow-through
        float k = fin ? 1.3f : 1.0f, p0 = hp * 0.45f;
        float u = clampf((p - p0) / (hp * 0.55f + 0.2f), 0, 1), w = 1 - (1 - u) * (1 - u) * (1 - u);
        if (p < p0) w = -0.18f * std::sin(PI * p / std::max(0.01f, p0));
        ang = P.aim - P.swingDir * 1.4f * k + P.swingDir * 2.8f * k * w;
        float rec = clampf((p - 0.55f) / 0.45f, 0, 1); // then the blade comes back down to its guard instead of hanging in the air
        ang += std::remainder((f > 0 ? 1.0f : PI - 1.0f) - ang, 2 * PI) * rec * rec * (3 - 2 * rec);
        if (fin) ext = 1.5f * std::sin(PI * p);
        break;
    }
    case ATK_SWEEP: // a flat sweep seen from the side: the blade swings from behind to in front under the arm, the circle squashed
    {         // so it stays low and level. Odd blows go back to front, even ones front to back.
        float p0 = hp * 0.45f;
        float u = clampf((p - p0) / (hp * 0.55f + 0.2f), 0, 1), w = 1 - (1 - u) * (1 - u) * (1 - u);
        if (p < p0) w = -0.15f * std::sin(PI * p / std::max(0.01f, p0)); // drawn back first
        float s = P.swingDir < 0 ? w : 1 - w, al = PI * (1 - s);
        ang = P.aim + f * std::atan2(0.5f * std::sin(al), std::cos(al));
        float rec = clampf((p - 0.55f) / 0.45f, 0, 1);
        ang += std::remainder((f > 0 ? 1.0f : PI - 1.0f) - ang, 2 * PI) * rec * rec * (3 - 2 * rec);
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
                attackPose(a0, e0, P.atkStyle == ATK_SLASH ? 4 : 3);
                bool fast = ((P.atkStyle == ATK_SLASH || P.atkStyle == ATK_SWEEP) && p < hp + 0.3f) || (P.atkStyle != ATK_SLASH && P.atkStyle != ATK_SWEEP && p > hp * 0.75f && p <= hp + 0.01f);
                float span = std::remainder(a - a0, 2 * PI);
                if (P.atkStyle != ATK_SLASH && P.atkStyle != ATK_SWEEP) // overhead blows turn more than half a circle: follow their real direction
                {
                    span = std::fmod(a - a0, 2 * PI);
                    if (f > 0 && span < 0) span += 2 * PI;
                    if (f < 0 && span > 0) span -= 2 * PI;
                }
                if (fast && std::fabs(span) > 0.05f && std::fabs(span) < 3.0f)
                {
                    float r0 = std::max(2.0f, L * 0.35f), r1 = L + ext + 2.5f;
                    float heavy = P.atkStyle == ATK_SLASH || P.atkStyle == ATK_SWEEP ? 1.0f : 1.4f;
                    // a crescent: a sliver at the tail, widest and brightest at the blade, with a white edge on the outside
                    int n = std::max(4, (int)(std::fabs(span) * 9));
                    auto at = [&](float t, float r) { float an = a0 + span * t; return Vector2{ox + std::cos(an) * r, oy + std::sin(an) * r}; };
                    auto tri = [](Vector2 u, Vector2 v, Vector2 w, Color c)
                    {
                        float cr = (v.x - u.x) * (w.y - u.y) - (v.y - u.y) * (w.x - u.x);
                        if (cr < 0) DrawTriangle(u, v, w, c); else DrawTriangle(u, w, v, c); // raylib wants them counter-clockwise on screen (y down)
                    };
                    for (int i = 0; i < n; i++)
                    {
                        float t0 = (float)i / n, t1 = (float)(i + 1) / n, tm = (t0 + t1) * 0.5f;
                        float i0 = r1 - (r1 - r0) * t0 * t0, i1 = r1 - (r1 - r0) * t1 * t1; // the inner edge sweeps out as it nears the blade
                        Color c = {sm.r, sm.g, sm.b, (unsigned char)std::min(255.0f, 190 * heavy * tm * tm)};
                        tri(at(t0, r1), at(t1, r1), at(t1, i1), c);
                        tri(at(t0, r1), at(t1, i1), at(t0, i0), c);
                        DrawLineEx(at(t0, r1), at(t1, r1), 1.0f, {255, 255, 255, (unsigned char)(220 * tm)});
                    }
                    Vector2 tip = at(1, r1); // a hot spark at the tip
                    DrawCircleV(tip, 1.3f, {255, 255, 240, 230});
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

// viking.cpp: the player, drawn from the baked Viking sheets (tools/viking*.py -> sprites_viking.h). Each frame is a finished
// picture (the weapon in his hands, the strike smears); this file only picks the frame and tints it:
//  - which sheet: the weapon in hand (or none), or the body sheet for moves with the weapon sheathed (roll, jump, climb, swim...)
//  - which clip and frame: from the player's state, an attack's timing, or the aim angle
//  - tags in the palette: weapon metal and staff gem take their colours; status coats (wet, oil, burning...) tint the lot.
// Textures are decoded once per (frame, tint) and drawn at half a unit a pixel, like the rest of the fine art.
#include "game.h"
#include "util.h"
#include "sprites_viking.h"
#include "sprites_weapons3d.h"
#include <map>
#include <tuple>
#include <cmath>
#include <algorithm>

static int setFor(const Weapon* w)
{
    if (!w) return VS_NONE;
    switch (w->type)
    {
    case W_DAGGER: return VS_DAGGER;
    case W_SWORD: return VS_SWORD;
    case W_AXE: return VS_AXE;
    case W_SPEAR: return VS_SPEAR;
    case W_MACE: return VS_MACE;
    case W_PAN: return VS_PAN;
    case W_CROSSBOW: return VS_CROSSBOW;
    case W_STAFF: return VS_STAFF;
    default: return VS_NONE;
    }
}

// What he wears over the tunic: padded gambeson, or by the armour metal's tier mail, lamellar, then plate.
static int armourLook(int armour)
{
    if (armour < 0) return 0;
    static const int tier[METAL_COUNT] = {1, 2, 3, 4, 4, 4, 4, 4, 5}; // as items.cpp's METAL_TIER
    int t = tier[std::min(armour, METAL_COUNT - 1)];
    return t <= 2 ? 1 : (t <= 4 ? 2 : 3);
}

// the frames of a blow where it lands: (first live, impact) per weapon, as baked in tools/viking_clips.py:STRIKE
static void strikeOf(int type, int variant, int& s0, int& s1)
{
    switch (type)
    {
    case W_AXE: case W_MACE: s0 = variant == 2 ? 4 : 3; s1 = variant == 2 ? 7 : 6; break;
    case W_PAN: s0 = 3; s1 = 5; break;
    case W_SWORD: s0 = 3; s1 = 6; break;
    case W_DAGGER: s0 = 3; s1 = 4; break;
    default: s0 = 3; s1 = 4; break;
    }
}

static Color remap(Color base, int tag, Color metal, Color gem, Color armour = BLANK)
{
    if (tag < 1 || tag > 3) return base;
    float lum = (0.30f * base.r + 0.59f * base.g + 0.11f * base.b) / 255.0f;
    Color t = tag == 1 ? metal : (tag == 3 ? armour : gem);
    float k = tag == 2 ? 0.2f + 1.3f * lum : 0.2f + 1.15f * lum; // the ramp's dark-to-light, laid over the metal's (or the gem's) own colour
    auto ch = [&](int c) { return (unsigned char)std::min(255.0f, c * k); };
    return {ch(t.r), ch(t.g), ch(t.b), 255};
}

static const Texture2D& frameTexture(const VkSheet& S, int fr, Color metal, Color gem, Color armour, bool flash)
{
    static std::map<std::tuple<const VkSheet*, int, unsigned, unsigned, unsigned, bool>, Texture2D> cache;
    if (cache.size() > 900) // tints come and go (a new weapon, a new gem): start over rather than grow
    {
        for (auto& kv : cache) UnloadTexture(kv.second);
        cache.clear();
    }
    auto key = std::make_tuple(&S, fr, ColorToInt(metal), ColorToInt(gem), ColorToInt(armour), flash);
    Texture2D& t = cache[key];
    if (t.id) return t;
    Image img = GenImageColor(S.fw, S.fh, BLANK);
    Color* px = (Color*)img.data;
    const unsigned short* p = S.rle + S.off[fr];
    int n = S.fw * S.fh, i = 0;
    while (i < n)
    {
        int v = p[0], run = p[1];
        p += 2;
        if (v)
        {
            Color c = VK_PAL[v];
            int tag = VK_TAG[v];
            if (flash) c = {255, 255, 255, c.a};
            else c = remap(c, tag, metal, gem, armour);
            for (int k = 0; k < run && i + k < n; k++) px[i + k] = c;
        }
        i += run;
    }
    t = LoadTextureFromImage(img);
    UnloadImage(img);
    SetTextureFilter(t, TEXTURE_FILTER_POINT);
    return t;
}

// feet at (bx, by) in render-texture units, facing 1 (right) or -1 (the picture mirrored about his centre)
static void drawFrame(const VkSheet& S, int fr, float bx, float by, int facing, Color metal, Color gem, Color armour, Color tint, bool flash)
{
    fr = std::max(0, std::min(S.frames - 1, fr));
    const Texture2D& t = frameTexture(S, fr, metal, gem, armour, flash);
    float w = S.fw * 0.5f, h = S.fh * 0.5f;
    float x = facing > 0 ? bx - S.ax * 0.5f : bx - (S.fw - S.ax) * 0.5f, y = by - S.ay * 0.5f;
    DrawTexturePro(t, {0, 0, (float)(facing > 0 ? S.fw : -S.fw), (float)S.fh}, {x, y, w, h}, {0, 0}, 0, tint);
}

static Color statusTint(const Mob& m)
{
    Color c = WHITE;
    if (m.bloody) c = {255, 160, 150, 255};
    if (m.oily) c = {160, 150, 120, 255};
    if (m.wet) c = {190, 210, 255, 255};
    if (m.chill > 0) c = {180, 220, 255, 255};
    if (m.poison > 0) c = {170, 230, 150, 255};
    if (m.burn > 0) c = {255, 190, 150, 255};
    if (m.inLiquid) c = {(unsigned char)(c.r * 0.7f), (unsigned char)(c.g * 0.88f), c.b, c.a}; // seen through the water: cooler, dimmer
    return c;
}

static int aimIndex(float relRad)
{
    float deg = std::atan2(std::sin(relRad), std::cos(relRad)) * RAD2DEG;
    deg = std::max(-90.0f, std::min(90.0f, deg));
    return (int)std::lround((deg + 90.0f) / 15.0f); // 13 poses, -90 (straight up) to +90 (straight down)
}

void drawPlayerViking(int camX, int camY)
{
    Player& P = G.p;
    Mob& m = P.m;
    if (!m.alive) return;
    if (m.iframes > 0 && P.rollT == 0 && (G.frame / 3) % 2) return; // hurt blink

    int f = P.onWall ? P.onWall : m.facing;
    if (f == 0) f = 1;
    bool air = !m.onGround;
    const Weapon* wpn = P.hotbar.empty() ? nullptr : &P.hotbar[P.sel];
    bool drawn = wpn && (P.combatT > 0 || P.swingT > 0);
    bool moving = std::fabs(m.vx) > 0.12f;
    auto cyc = [](float x) { return x - std::floor(x); };
    const float TAU = 2 * PI;

    int look = armourLook(P.armour);
    const VkSheet* S = VK_LOOKS[look][setFor(wpn)];
    const VkSheet& B = *VK_LOOKS[look][VS_BODY];
    int clip = VC_IDLE, fr = 0;
    bool body = false; // the weapon sheathed: from the body sheet
    bool swinging = wpn && isMelee(wpn->type) && P.swingT > 0 && !P.prone && !P.crouch && !m.inLiquid && !P.climb && !P.onWall && P.hook != 2;
    float aimRel = f > 0 ? P.aim : PI - P.aim;

    if (P.rollT > 0)
    {
        int e = 23 - P.rollT; // 0..22: spring into a dive, curl into the tuck, a fast tight spin, skid back up
        body = true;
        if (e < 5) { clip = VC_ROLLIN; fr = e * B.count[VC_ROLLIN] / 5; }
        else if (e < 20) { clip = VC_ROLL; fr = (e - 5) * B.count[VC_ROLL] / 15; }
        else { clip = VC_ROLLOUT; fr = e - 20; }
    }
    else if (swinging)
    {
        bool heavy2 = wpn->type == W_AXE || wpn->type == W_MACE; // left swing, right swing, overhead
        int variant = heavy2 ? P.combo : (P.combo == 2 && wpn->type != W_PAN ? 1 : 0);
        clip = variant == 2 ? VC_ATK2 : (variant ? VC_ATK1 : VC_ATK0);
        int n = S->count[clip], s0, s1;
        strikeOf(wpn->type, variant, s0, s1);
        if (wpn->type == W_DAGGER && variant == 0) s0 = 3, s1 = 4;
        s1 = std::min(s1, n - 2);
        s0 = std::min(s0, s1);
        float p = 1 - (float)P.swingT / std::max(1, P.atkLen), hp = (float)P.atkHitAt / std::max(1, P.atkLen), a = std::max(0.02f, hp - 0.12f);
        fr = 0;
        for (int k = 0; k < n; k++)
        {
            float t = k < s0 ? a * k / std::max(1, s0) : (k <= s1 ? a + (hp - a) * (k - s0) / std::max(1, s1 - s0) : hp + (1 - hp) * (k - s1) / std::max(1, n - 1 - s1));
            if (t <= p) fr = k;
        }
    }
    else if (P.prone) { body = true; clip = VC_CRAWL; fr = moving ? (int)(cyc(P.runPhase * 1.3f / TAU) * B.count[clip]) % B.count[clip] : 0; }
    else if (P.crouch)
    {
        body = true;
        clip = moving ? VC_CROUCHWALK : VC_CROUCH;
        fr = (int)(cyc(moving ? P.runPhase / TAU : G.frame / 48.0f) * B.count[clip]) % B.count[clip];
    }
    else if (m.inLiquid && !m.onGround)
    {
        body = true;
        bool going = std::fabs(m.vx) > 0.15f || std::fabs(m.vy) > 0.25f;
        clip = going ? VC_SWIM : VC_TREAD;
        fr = (int)(cyc(G.frame * (going ? 0.2f : 0.1f) / TAU) * B.count[clip]) % B.count[clip];
    }
    else if (P.climb) { body = true; clip = VC_CLIMB; fr = (int)(cyc(P.runPhase * 2.5f / TAU) * B.count[clip]) % B.count[clip]; }
    else if (P.onWall) { body = true; clip = VC_WALL; fr = (G.frame / 14) % 2; }
    else if (P.readT > 0) { body = true; clip = VC_HOOKAIM; fr = aimIndex(aimRel); } // the hand flung out as a scroll is read
    else if (P.hook == 2 && air) { body = true; clip = VC_HANG; fr = (G.frame / 8) % B.count[clip]; }
    else if (P.hook != 0)
    { // reaching for the hook: the arm out toward it
        body = true;
        clip = VC_HOOKAIM;
        float dx = (P.hx - m.cx()) * f, dy = P.hy - (m.y + 6);
        fr = aimIndex(std::atan2(dy, dx));
    }
    else if (wpn && drawn && (wpn->type == W_CROSSBOW || wpn->type == W_STAFF) && !air && !m.inLiquid)
    {
        clip = P.recoil > 0 ? VC_FIRE : VC_AIM;
        fr = aimIndex(aimRel);
    }
    else if (air)
    {
        body = true;
        clip = m.vy < -0.3f ? VC_JUMP : VC_FALL;
    }
    else if (P.squash < 0.9f) { body = true; clip = VC_LAND; fr = P.squash < 0.8f ? 0 : 1; }
    else if (m.hurtFlash > 2 && !drawn) { body = true; clip = VC_HURT; fr = m.hurtFlash > 4 ? 0 : 1; }
    else if (moving)
    {
        clip = VC_RUN;
        float k = cyc(P.runPhase / TAU);
        if (m.vx * f < 0) k = 1 - k; // backpedalling plays the stride in reverse
        fr = (int)(k * S->count[clip]) % std::max(1, S->count[clip]);
    }
    else { clip = VC_IDLE; fr = (G.frame / 9) % std::max(1, S->count[clip]); }

    const VkSheet& SS = body ? B : *S;
    if (SS.count[clip] == 0) return;
    fr = std::max(0, std::min(SS.count[clip] - 1, fr));
    float bx = std::floor(m.x) + m.w * 0.5f - camX, by = std::floor(m.y) + m.h - camY; // whole-unit anchor, no shimmer
    Color metal = wpn && wpn->type != W_STAFF ? METALS[wpn->metal].color : Color{150, 150, 156, 255};
    Color gem = wpn && wpn->type == W_STAFF ? wpn->staff.gem : Color{150, 200, 255, 255};
    Color armour = P.armour >= 0 ? METALS[P.armour].color : Color{150, 150, 156, 255};
    drawFrame(SS, SS.first[clip] + fr, bx, by, f, metal, gem, armour, statusTint(m), m.hurtFlash > 0 && (m.hurtFlash % 4 < 2));
}


// ---------------------------------------------------------------- weapon icons (tools/weapons3d.py)
// The same 3D-modelled weapons the Viking swings, baked at 16 angles with the light held fixed: a weapon on the ground, in the
// hotbar or on a rack is the picture of the nearest angle. Nothing is rotated at draw time. Tag 1 takes the metal, tag 2 the gem.
static const Texture2D& weaponTexture(int type, int k, Color metal, Color gem)
{
    static std::map<std::tuple<int, int, unsigned, unsigned>, Texture2D> cache;
    auto key = std::make_tuple(type, k, ColorToInt(metal), ColorToInt(gem));
    Texture2D& t = cache[key];
    if (t.id) return t;
    const W3Type& W = W3[type];
    int w = W.w[k], h = W.h[k];
    Image img = GenImageColor(w, h, BLANK);
    Color* px = (Color*)img.data;
    const unsigned char* p = W.rle + W.off[k];
    int n = w * h, i = 0;
    while (i < n)
    {
        int v = p[0], run = p[1];
        p += 2;
        if (v)
        {
            Color c = remap(W3_PAL[v], W3_TAG[v], metal, gem);
            for (int q = 0; q < run && i + q < n; q++) px[i + q] = c;
        }
        i += run;
    }
    t = LoadTextureFromImage(img);
    UnloadImage(img);
    SetTextureFilter(t, TEXTURE_FILTER_POINT);
    return t;
}

// Draws weapon `w` pointing along `ang` (radians, 0 = right, clockwise) with its grip at `at` (or its middle, if `centred`),
// `scale` world units a pixel. False if this weapon type has no baked icon.
bool weapon3dDraw(const Weapon& w, Vector2 at, float ang, float scale, bool centred)
{
    if (w.type < 0 || w.type >= (int)(sizeof(W3) / sizeof(W3[0])) || !W3[w.type].w) return false;
    int k = ((int)std::lround(ang / (PI / 8)) % 16 + 16) % 16;
    Color metal = w.type == W_PAN ? METALS[M_IRON].color : METALS[w.metal].color;
    Color gem = w.type == W_STAFF ? w.staff.gem : Color{150, 200, 255, 255};
    const Texture2D& t = weaponTexture(w.type, k, metal, gem);
    const W3Type& W = W3[w.type];
    Vector2 origin = centred ? Vector2{t.width * scale / 2, t.height * scale / 2} : Vector2{(W.px[k] + 0.5f) * scale, (W.py[k] + 0.5f) * scale};
    DrawTexturePro(t, {0, 0, (float)t.width, (float)t.height}, {at.x, at.y, t.width * scale, t.height * scale}, origin, 0, WHITE);
    return true;
}

// The grip-to-tip length of the icon, in the units the old sprites reported (half the pixels).
float weapon3dLength(const Weapon& w)
{
    if (w.type < 0 || w.type >= (int)(sizeof(W3) / sizeof(W3[0])) || !W3[w.type].w) return 0;
    const W3Type& W = W3[w.type];
    float best = 0;
    for (int k = 0; k < 16; k += 4) // right, down, left, up: the farthest reach of the four
        best = std::max(best, std::hypot((float)std::max(W.px[k], W.w[k] - W.px[k]), (float)std::max(W.py[k], W.h[k] - W.py[k])));
    return best * 0.5f;
}

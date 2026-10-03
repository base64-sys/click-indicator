#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

using namespace geode::prelude;

// ════
//  Compile-time constants
// ════
namespace cfg {

    // Ring geometry (GD world-units; 1 block ≈ 30 units, player ≈ 30×30).
    constexpr float RING_RADIUS = 22.f;

    // Quality choices exposed in the settings menu.
    constexpr int RING_SEGMENTS_LOW  = 16;
    constexpr int RING_SEGMENTS_MED  = 24;
    constexpr int RING_SEGMENTS_HIGH = 32;
    constexpr int RING_SEGMENTS_ULTRA = 48;

    // All pulse layers burst outward from near-zero.
    constexpr float SCALE_START = 0.05f;

    // ── Pulse ring per-layer multipliers ────
    constexpr float MUL_SCALE_FLASH = 0.40f;
    constexpr float MUL_SCALE_MAIN  = 1.00f;
    constexpr float MUL_SCALE_GHOST = 1.55f;

    constexpr float MUL_DUR_FLASH = 0.50f;
    constexpr float MUL_DUR_MAIN  = 1.00f;
    constexpr float MUL_DUR_GHOST = 1.40f;

    constexpr float ALPHA_FLASH = 1.00f;
    constexpr float ALPHA_MAIN  = 0.85f;
    constexpr float ALPHA_GHOST = 0.35f;

    // ── Hold ring ────
    constexpr float HOLD_APPEAR_DUR = 0.08f;

    // ── Ripple ────
    constexpr float RIPPLE_DELAY      = 0.08f;
    constexpr float RIPPLE_ECHO_ALPHA = 0.55f;

    // ── Crosshair ────
    constexpr float CROSS_EXTEND = 0.35f;

    // ── Helix ────
    constexpr float HELIX_ARC_DEG  = 270.f;
    constexpr float HELIX_SPIN_DEG = 720.f;

    // ── Hold trail ────
    constexpr int HOLD_TRAIL_MARKER_SEGMENTS = 12;
    constexpr float HOLD_TRAIL_MARKER_RADIUS = 4.f;
    constexpr float HOLD_TRAIL_UPDATE_EPSILON_SQ = 0.25f;

    // ── Z-orders within m_objectLayer ────
    constexpr int Z_TRAIL = 1;
    constexpr int Z_HOLD  = 2;
    constexpr int Z_PULSE = 3;

    // Tags for hold indicators (above GD's range to avoid collisions).
    constexpr int HOLD_TAG_P1 = 7001;
    constexpr int HOLD_TAG_P2 = 7002;
    constexpr int TRAIL_TAG_P1 = 7101;
    constexpr int TRAIL_TAG_P2 = 7102;

} // namespace cfg


// ════
//  Enums
// ════
enum class AnimType : int {
    Pulse,
    Ripple,
    Implode,
    Crosshair,
    Diamond,
    Helix
};

enum class EaseType : int {
    Linear,
    EaseOut,
    EaseInOut
};


// ════
//  Runtime settings snapshot
//
//  Geode setting storage is queried once per input event. Nodes then capture the
//  values they need and never query Mod::get() from their animation/update loops.
// ════
namespace settings {

    struct Runtime {
        bool clickBurstEnabled = true;

        bool p1Enabled = true;
        ccColor3B p1Color = {0, 220, 255};
        bool p2Enabled = true;
        ccColor3B p2Color = {255, 60, 180};

        AnimType animation = AnimType::Pulse;
        float duration = 0.35f;
        float scaleEnd = 0.75f;
        float opacity = 0.9f;
        float ringThickness = 2.5f;
        EaseType easing = EaseType::EaseOut;
        int layerCount = 3;
        bool additive = true;
        bool releaseBurst = false;

        bool holdEnabled = true;
        float holdOpacity = 0.6f;
        float holdThickness = 2.0f;
        float holdScale = 0.45f;
        bool holdFollow = true;
        float fadeDuration = 0.25f;

        bool holdTrailEnabled = false;
        float holdTrailOpacity = 0.95f;
        float holdTrailThickness = 3.5f;
        bool holdTrailMarker = true;

        bool colorMatch = false;
        bool rainbowMode = false;
        float rainbowSpeed = 1.f;

        bool performanceMode = false;
        int ringSegments = cfg::RING_SEGMENTS_MED;
        int maxActiveEffects = 24;
        bool rippleEchoes = true;
    };

    static AnimType parseAnimation(const std::string& value) {
        if (value == "ripple") return AnimType::Ripple;
        if (value == "implode") return AnimType::Implode;
        if (value == "crosshair") return AnimType::Crosshair;
        if (value == "diamond") return AnimType::Diamond;
        if (value == "helix") return AnimType::Helix;
        return AnimType::Pulse;
    }

    static EaseType parseEasing(const std::string& value) {
        if (value == "linear") return EaseType::Linear;
        if (value == "ease-in-out") return EaseType::EaseInOut;
        return EaseType::EaseOut;
    }

    static int parseRingSegments(const std::string& value) {
        if (value == "low") return cfg::RING_SEGMENTS_LOW;
        if (value == "high") return cfg::RING_SEGMENTS_HIGH;
        if (value == "ultra") return cfg::RING_SEGMENTS_ULTRA;
        return cfg::RING_SEGMENTS_MED;
    }

    inline Runtime read() {
        auto* mod = Mod::get();
        Runtime r;

        r.clickBurstEnabled = mod->getSettingValue<bool>("click-burst-enabled");

        r.p1Enabled = mod->getSettingValue<bool>("p1-enabled");
        r.p1Color = mod->getSettingValue<ccColor3B>("p1-color");
        r.p2Enabled = mod->getSettingValue<bool>("p2-enabled");
        r.p2Color = mod->getSettingValue<ccColor3B>("p2-color");

        r.animation = parseAnimation(mod->getSettingValue<std::string>("animation"));
        r.duration = mod->getSettingValue<float>("duration");
        r.scaleEnd = mod->getSettingValue<float>("scale-end");
        r.opacity = mod->getSettingValue<float>("opacity");
        r.ringThickness = mod->getSettingValue<float>("ring-thickness");
        r.easing = parseEasing(mod->getSettingValue<std::string>("easing"));
        r.layerCount = std::clamp(mod->getSettingValue<int>("layer-count"), 1, 3);
        r.additive = mod->getSettingValue<bool>("additive-blend");
        r.releaseBurst = mod->getSettingValue<bool>("release-burst");

        r.holdEnabled = mod->getSettingValue<bool>("hold-enabled");
        r.holdOpacity = mod->getSettingValue<float>("hold-opacity");
        r.holdThickness = mod->getSettingValue<float>("hold-thickness");
        r.holdScale = mod->getSettingValue<float>("hold-scale");
        r.holdFollow = mod->getSettingValue<bool>("hold-follow");
        r.fadeDuration = mod->getSettingValue<float>("fade-duration");

        r.holdTrailEnabled = mod->getSettingValue<bool>("hold-trail-enabled");
        r.holdTrailOpacity = mod->getSettingValue<float>("hold-trail-opacity");
        r.holdTrailThickness = mod->getSettingValue<float>("hold-trail-thickness");
        r.holdTrailMarker = mod->getSettingValue<bool>("hold-trail-start-marker");

        r.colorMatch = mod->getSettingValue<bool>("color-match-player");
        r.rainbowMode = mod->getSettingValue<bool>("rainbow-mode");
        r.rainbowSpeed = mod->getSettingValue<float>("rainbow-speed");

        r.performanceMode = mod->getSettingValue<bool>("performance-mode");
        r.ringSegments = parseRingSegments(mod->getSettingValue<std::string>("ring-quality"));
        r.maxActiveEffects = std::clamp(mod->getSettingValue<int>("max-active-effects"), 4, 64);

        if (r.performanceMode) {
            r.ringSegments = cfg::RING_SEGMENTS_LOW;
            r.maxActiveEffects = 12;
            r.layerCount = 1;
            r.additive = false;
            r.releaseBurst = false;
            r.rippleEchoes = false;
        }

        r.duration = std::fmax(r.duration, 0.001f);
        r.fadeDuration = std::fmax(r.fadeDuration, 0.001f);
        return r;
    }

} // namespace settings


// ════
//  Utility functions
// ════

/// HSV → RGB. h ∈ [0, 360), s/v ∈ [0, 1].
static ccColor3B hsvToRgb(float h, float s, float v) {
    const float c = v * s;
    const float x = c * (1.f - std::fabs(std::fmod(h / 60.f, 2.f) - 1.f));
    const float m = v - c;
    float r, g, b;
    if (h < 60.f)       { r = c; g = x; b = 0; }
    else if (h < 120.f) { r = x; g = c; b = 0; }
    else if (h < 180.f) { r = 0; g = c; b = x; }
    else if (h < 240.f) { r = 0; g = x; b = c; }
    else if (h < 300.f) { r = x; g = 0; b = c; }
    else                { r = c; g = 0; b = x; }

    return {
        static_cast<uint8_t>((r + m) * 255.f),
        static_cast<uint8_t>((g + m) * 255.f),
        static_cast<uint8_t>((b + m) * 255.f)
    };
}

static float applyEasing(float t, EaseType e) {
    switch (e) {
        case EaseType::Linear:
            return t;
        case EaseType::EaseOut:
            return 1.f - std::exp2f(-10.f * t);
        case EaseType::EaseInOut:
            return t * t * (3.f - 2.f * t);
    }
    return t;
}

static GLubyte alphaToByte(float alpha) {
    return static_cast<GLubyte>(std::clamp(alpha, 0.f, 1.f) * 255.f);
}


// ════
//  Shape drawing primitives
//
//  IMPORTANT: geometry is generated ONCE per node. Animation updates only the
//  node transform and the DrawNode opacity, avoiding clear()+many drawSegment()
//  calls every frame.
// ════

static void drawCircle(CCDrawNode* draw, float thick, const ccColor4F& col, int segments) {
    constexpr float kTau = 6.28318530718f;
    for (int i = 0; i < segments; ++i) {
        const float a0 = kTau * float(i) / float(segments);
        const float a1 = kTau * float(i + 1) / float(segments);
        draw->drawSegment(
            { std::cos(a0) * cfg::RING_RADIUS, std::sin(a0) * cfg::RING_RADIUS },
            { std::cos(a1) * cfg::RING_RADIUS, std::sin(a1) * cfg::RING_RADIUS },
            thick, col);
    }
}

static void drawCrossLines(CCDrawNode* draw, float thick, const ccColor4F& col) {
    const float ext   = cfg::RING_RADIUS * (1.f + 0.35f);
    const float inner = cfg::RING_RADIUS * 0.3f;
    const float hw    = thick * 0.6f;
    draw->drawSegment({0, inner},  {0, ext},   hw, col);
    draw->drawSegment({0, -inner}, {0, -ext},  hw, col);
    draw->drawSegment({inner, 0},  {ext, 0},   hw, col);
    draw->drawSegment({-inner, 0}, {-ext, 0},  hw, col);
}

static void drawDiamond(CCDrawNode* draw, float thick, const ccColor4F& col) {
    const float r = cfg::RING_RADIUS;
    const CCPoint pts[4] = { {0, r}, {r, 0}, {0, -r}, {-r, 0} };
    for (int i = 0; i < 4; ++i)
        draw->drawSegment(pts[i], pts[(i + 1) % 4], thick, col);
}

static void drawArc(CCDrawNode* draw, float thick, const ccColor4F& col, float degSpan, int baseSegments) {
    const float radSpan = degSpan * 3.14159265359f / 180.f;
    const int segs = std::max(8, int(baseSegments * degSpan / 360.f));
    for (int i = 0; i < segs; ++i) {
        const float a0 = radSpan * float(i) / float(segs);
        const float a1 = radSpan * float(i + 1) / float(segs);
        draw->drawSegment(
            { std::cos(a0) * cfg::RING_RADIUS, std::sin(a0) * cfg::RING_RADIUS },
            { std::cos(a1) * cfg::RING_RADIUS, std::sin(a1) * cfg::RING_RADIUS },
            thick, col);
    }
}

static void drawShape(CCDrawNode* draw, float thick, const ccColor4F& col, AnimType anim, int segments) {
    draw->clear();
    switch (anim) {
        case AnimType::Pulse:
        case AnimType::Ripple:
        case AnimType::Implode:
            drawCircle(draw, thick, col, segments);
            break;
        case AnimType::Crosshair:
            drawCircle(draw, thick, col, segments);
            drawCrossLines(draw, thick, col);
            break;
        case AnimType::Diamond:
            drawDiamond(draw, thick, col);
            break;
        case AnimType::Helix:
            drawArc(draw, thick, col, cfg::HELIX_ARC_DEG, segments);
            break;
    }
}

static void drawTrail(CCDrawNode* draw, const CCPoint& start, const CCPoint& end,
                    float thick, const ccColor4F& col, bool marker) {
    draw->clear();
    draw->drawSegment(start, end, thick, col);

    if (marker) {
        constexpr float kTau = 6.28318530718f;
        for (int i = 0; i < cfg::HOLD_TRAIL_MARKER_SEGMENTS; ++i) {
            const float a0 = kTau * float(i) / float(cfg::HOLD_TRAIL_MARKER_SEGMENTS);
            const float a1 = kTau * float(i + 1) / float(cfg::HOLD_TRAIL_MARKER_SEGMENTS);
            draw->drawSegment(
                { start.x + std::cos(a0) * cfg::HOLD_TRAIL_MARKER_RADIUS,
                  start.y + std::sin(a0) * cfg::HOLD_TRAIL_MARKER_RADIUS },
                { start.x + std::cos(a1) * cfg::HOLD_TRAIL_MARKER_RADIUS,
                  start.y + std::sin(a1) * cfg::HOLD_TRAIL_MARKER_RADIUS },
                std::fmax(thick * 0.75f, 1.5f), col);
        }
    }
}


// ════
//  Ring colour resolution
// ════
struct RingColor {
    float r, g, b;
};

static RingColor resolveColor(bool isP1, CCNode* player, const settings::Runtime& cfg) {
    if (cfg.rainbowMode) {
        float hue = std::fmod(
            float(CCDirector::sharedDirector()->getTotalFrames()) * cfg.rainbowSpeed,
            360.f);
        if (!isP1) hue = std::fmod(hue + 180.f, 360.f);
        const auto c = hsvToRgb(hue, 1.f, 1.f);
        return { c.r / 255.f, c.g / 255.f, c.b / 255.f };
    }

    if (cfg.colorMatch && player) {
        const auto c = static_cast<CCNodeRGBA*>(player)->getColor();
        return { c.r / 255.f, c.g / 255.f, c.b / 255.f };
    }

    const ccColor3B c = isP1 ? cfg.p1Color : cfg.p2Color;
    return { c.r / 255.f, c.g / 255.f, c.b / 255.f };
}


// ════
//  Active-effect budget
// ════
static int g_activePulseNodes = 0;

static bool reservePulseSlot(int maxActiveEffects) {
    if (g_activePulseNodes >= maxActiveEffects)
        return false;
    ++g_activePulseNodes;
    return true;
}

static void releasePulseSlot() {
    if (g_activePulseNodes > 0)
        --g_activePulseNodes;
}


// ════
//  RingNode — animated pulse ring
//
//  Geometry is built once in init(). During animation, only transform/opacity
//  changes, which removes the old per-frame clear()+48 segment redraw cost.
// ════
class RingNode : public CCNode {

    CCDrawNode* m_draw = nullptr;
    float m_startAlpha = 1.f;
    float m_elapsed = 0.f;
    float m_total = 0.35f;
    float m_startScale = cfg::SCALE_START;
    float m_endScale = 1.f;
    AnimType m_anim = AnimType::Pulse;
    EaseType m_easing = EaseType::EaseOut;
    bool m_slotReserved = false;
    float m_delay = 0.f;

    bool init(float r, float g, float b, float startAlpha,
              float dur, float startSc, float endSc, float thick,
              bool additive, AnimType anim, EaseType easing, float delay,
              int ringSegments, int maxActiveEffects) {
        if (!CCNode::init()) return false;

        if (!reservePulseSlot(maxActiveEffects))
            return false;
        m_slotReserved = true;

        m_startAlpha = startAlpha;
        m_total = std::fmax(dur, 0.001f);
        m_startScale = startSc;
        m_endScale = endSc;
        m_anim = anim;
        m_easing = easing;
        m_delay = std::fmax(delay, 0.f);

        m_draw = CCDrawNode::create();
        if (!m_draw) return false;
        if (additive) {
            ccBlendFunc blend{ GL_SRC_ALPHA, GL_ONE };
            m_draw->setBlendFunc(blend);
        }

        this->addChild(m_draw);

        const ccColor4F col = { r, g, b, 1.f };
        drawShape(m_draw, thick, col, anim, ringSegments);
        m_draw->setOpacity(delay > 0.f ? 0 : alphaToByte(startAlpha));

        this->setScale(startSc);
        this->scheduleUpdate();
        return true;
    }

public:
    ~RingNode() override {
        if (m_slotReserved)
            releasePulseSlot();
    }

    static RingNode* create(float r, float g, float b, float startAlpha,
                    float dur, float startSc, float endSc, float thick,
                    bool additive, AnimType anim, EaseType easing, float delay,
                    int ringSegments, int maxActiveEffects) {
        auto* ret = new RingNode();
        if (ret && ret->init(r, g, b, startAlpha, dur, startSc, endSc, thick,
                    additive, anim, easing, delay, ringSegments, maxActiveEffects)) {
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }

    void update(float dt) override {
        if (m_delay > 0.f) {
            m_delay -= dt;
            if (m_delay > 0.f)
                return;
            m_elapsed = 0.f;
            m_draw->setOpacity(alphaToByte(m_startAlpha));
        }

        m_elapsed += dt;
        if (m_elapsed >= m_total) {
            this->unscheduleUpdate();
            this->removeFromParentAndCleanup(true);
            return;
        }

        const float t = std::clamp(m_elapsed / m_total, 0.f, 1.f);
        const float eased = applyEasing(t, m_easing);

        this->setScale(m_startScale + (m_endScale - m_startScale) * eased);

        if (m_anim == AnimType::Helix)
            this->setRotation(cfg::HELIX_SPIN_DEG * eased);

        m_draw->setOpacity(alphaToByte(m_startAlpha * (1.f - t)));
    }
};


// ════
//  HoldRingNode — persistent held ring
//
//  When player-follow is disabled, the node unschedules itself once appearance
//  is complete and wakes again only when the button is released. This avoids a
//  useless per-frame update loop for a static hold ring.
// ════
class HoldRingNode : public CCNode {

    enum class State { APPEARING, HELD, FADING };

    CCDrawNode* m_draw = nullptr;
    CCNode* m_target = nullptr;
    float m_r = 0.f, m_g = 0.f, m_b = 0.f;
    float m_alpha = 0.6f;
    float m_elapsed = 0.f;
    float m_endScale = 0.45f;
    float m_fadeDur = 0.25f;
    bool m_follow = true;
    State m_state = State::APPEARING;
    int m_ringSegments = cfg::RING_SEGMENTS_MED;

    bool init(float r, float g, float b, float alpha,
              float endScale, float thick, float fadeDur,
              bool additive, bool follow, CCNode* target, int ringSegments) {
        if (!CCNode::init()) return false;

        m_r = r;
        m_g = g;
        m_b = b;
        m_alpha = alpha;
        m_endScale = endScale;
        m_fadeDur = std::fmax(fadeDur, 0.001f);
        m_follow = follow;
        m_target = target;
        m_ringSegments = ringSegments;

        m_draw = CCDrawNode::create();
        if (!m_draw) return false;
        if (additive) {
            ccBlendFunc blend{ GL_SRC_ALPHA, GL_ONE };
            m_draw->setBlendFunc(blend);
        }
        this->addChild(m_draw);

        const ccColor4F col = { m_r, m_g, m_b, 1.f };
        drawShape(m_draw, thick, col, AnimType::Pulse, m_ringSegments);
        m_draw->setOpacity(0);

        this->setScale(cfg::SCALE_START);
        this->scheduleUpdate();
        return true;
    }

public:
    static HoldRingNode* create(float r, float g, float b, float alpha,
                    float endScale, float thick, float fadeDur,
                    bool additive, bool follow, CCNode* target, int ringSegments) {
        auto* ret = new HoldRingNode();
        if (ret && ret->init(r, g, b, alpha, endScale, thick, fadeDur,
                    additive, follow, target, ringSegments)) {
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }

    void startFade() {
        if (m_state == State::FADING) return;
        m_state = State::FADING;
        m_elapsed = 0.f;
        this->scheduleUpdate();
    }

    void update(float dt) override {
        if (m_target && !m_target->getParent()) {
            this->unscheduleUpdate();
            this->removeFromParentAndCleanup(true);
            return;
        }

        if (m_follow && m_target) {
            this->setPosition(m_target->getPosition());
        }

        if (m_state == State::APPEARING) {
            m_elapsed += dt;
            const float t = std::clamp(m_elapsed / cfg::HOLD_APPEAR_DUR, 0.f, 1.f);
            const float eased = 1.f - std::exp2f(-10.f * t);
            this->setScale(cfg::SCALE_START + (m_endScale - cfg::SCALE_START) * eased);
            m_draw->setOpacity(alphaToByte(m_alpha * t));

            if (t >= 1.f) {
                m_state = State::HELD;
                m_elapsed = 0.f;
                m_draw->setOpacity(alphaToByte(m_alpha));
                if (!m_follow)
                    this->unscheduleUpdate();
            }
            return;
        }

        if (m_state == State::HELD)
            return;

        if (m_state == State::FADING) {
            m_elapsed += dt;
            const float t = m_elapsed / m_fadeDur;
            if (t >= 1.f) {
                this->unscheduleUpdate();
                this->removeFromParentAndCleanup(true);
                return;
            }
            m_draw->setOpacity(alphaToByte(m_alpha * (1.f - t)));
        }
    }
};


// ════
//  HoldTrailNode — press-to-current displacement line
//
//  The trail is deliberately a STRAIGHT line from the position where the hold
//  began to the player's current position. It shows the exact displacement
//  without creating a trail of hundreds of points. It fades after release using
//  the same Hold Fade Duration setting as the hold ring.
// ════
class HoldTrailNode : public CCNode {

    enum class State { TRACKING, FADING };

    CCDrawNode* m_draw = nullptr;
    CCNode* m_target = nullptr;
    float m_r = 1.f, m_g = 1.f, m_b = 1.f;
    CCPoint m_startPos = {0, 0};
    CCPoint m_lastPos = {0, 0};
    float m_alpha = 0.95f;
    float m_thick = 3.5f;
    float m_fadeDur = 0.25f;
    float m_elapsed = 0.f;
    bool m_marker = true;
    State m_state = State::TRACKING;

    void redraw(const CCPoint& endPos) {
        const ccColor4F col = {m_r, m_g, m_b, 1.f};
        drawTrail(m_draw, m_startPos, endPos, m_thick, col, m_marker);
    }

    bool init(float r, float g, float b, float alpha,
              float thick, float fadeDur, bool marker, CCPoint startPos,
              CCNode* target) {
        if (!CCNode::init()) return false;

        m_target = target;
        m_r = r;
        m_g = g;
        m_b = b;
        m_startPos = startPos;
        m_lastPos = startPos;
        m_alpha = alpha;
        m_thick = thick;
        m_fadeDur = std::fmax(fadeDur, 0.001f);
        m_marker = marker;

        m_draw = CCDrawNode::create();
        if (!m_draw) return false;
        this->addChild(m_draw);

        const ccColor4F col = { m_r, m_g, m_b, 1.f };
        drawTrail(m_draw, m_startPos, startPos, m_thick, col, m_marker);
        m_draw->setOpacity(alphaToByte(m_alpha));
        this->scheduleUpdate();
        return true;
    }

public:
    static HoldTrailNode* create(float r, float g, float b, float alpha,
                    float thick, float fadeDur, bool marker,
                    CCPoint startPos, CCNode* target) {
        auto* ret = new HoldTrailNode();
        if (ret && ret->init(r, g, b, alpha, thick, fadeDur, marker, startPos, target)) {
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }

    void startFade() {
        if (m_state == State::FADING) return;
        if (m_target)
            m_lastPos = m_target->getPosition();
        redraw(m_lastPos);
        m_state = State::FADING;
        m_elapsed = 0.f;
        this->scheduleUpdate();
    }

    void update(float dt) override {
        if (m_target && !m_target->getParent()) {
            this->unscheduleUpdate();
            this->removeFromParentAndCleanup(true);
            return;
        }

        if (m_state == State::TRACKING) {
            if (!m_target)
                return;

            const CCPoint current = m_target->getPosition();
            const float dx = current.x - m_lastPos.x;
            const float dy = current.y - m_lastPos.y;
            if ((dx * dx + dy * dy) >= cfg::HOLD_TRAIL_UPDATE_EPSILON_SQ) {
                m_lastPos = current;
                redraw(current);
            }
            return;
        }

        m_elapsed += dt;
        const float t = m_elapsed / m_fadeDur;
        if (t >= 1.f) {
            this->unscheduleUpdate();
            this->removeFromParentAndCleanup(true);
            return;
        }
        m_draw->setOpacity(alphaToByte(m_alpha * (1.f - t)));
    }
};


// ════
//  Pulse spawner
// ════
static void spawnPulse(CCLayer* layer, CCPoint pos, RingColor col, const settings::Runtime& cfg) {
    const float dur = cfg.duration;
    const float scEnd = cfg.scaleEnd;
    const float opGlob = cfg.opacity;
    const float thick = cfg.ringThickness;
    const bool add = cfg.additive;
    const AnimType anim = cfg.animation;
    const EaseType eas = cfg.easing;
    const int layers = cfg.layerCount;

    auto spawnLayer = [&](float alphaMul, float durMul, float scaleMul) {
        const float target = scEnd * scaleMul;
        const float startSc = (anim == AnimType::Implode) ? target : cfg::SCALE_START;
        const float endSc = (anim == AnimType::Implode) ? cfg::SCALE_START : target;

        auto* n = RingNode::create(
            col.r, col.g, col.b,
            alphaMul * opGlob,
            dur * durMul,
            startSc,
            endSc,
            thick,
            add,
            anim,
            eas,
            0.f,
            cfg.ringSegments,
            cfg.maxActiveEffects
        );
        if (n) {
            n->setPosition(pos);
            layer->addChild(n, cfg::Z_PULSE);
        }

        // Ripple echoes are skipped in Performance Mode because they multiply
        // node count without being essential to the main visual.
        if (n && anim == AnimType::Ripple && cfg.rippleEchoes) {
            auto* echo = RingNode::create(
                col.r, col.g, col.b,
                alphaMul * opGlob * cfg::RIPPLE_ECHO_ALPHA,
                dur * durMul * 1.2f,
                cfg::SCALE_START,
                endSc * 1.15f,
                thick * 0.7f,
                add,
                AnimType::Pulse,
                eas,
                cfg::RIPPLE_DELAY,
                cfg.ringSegments,
                cfg.maxActiveEffects
            );
            if (echo) {
                echo->setPosition(pos);
                layer->addChild(echo, cfg::Z_PULSE);
            }
        }
    };

    if (layers >= 3) spawnLayer(cfg::ALPHA_FLASH, cfg::MUL_DUR_FLASH, cfg::MUL_SCALE_FLASH);
    spawnLayer(cfg::ALPHA_MAIN, cfg::MUL_DUR_MAIN, cfg::MUL_SCALE_MAIN);
    if (layers >= 2) spawnLayer(cfg::ALPHA_GHOST, cfg::MUL_DUR_GHOST, cfg::MUL_SCALE_GHOST);
}


// ════
//  Hook
// ════
class $modify(ClickIndicatorLayer, GJBaseGameLayer) {

    void handleButton(bool down, int button, bool isPlayer1) {
        // Always run the original first — we observe only, never block.
        GJBaseGameLayer::handleButton(down, button, isPlayer1);

        // Gate: jump / action button only.
        if (button != 1) return;
        if (!m_objectLayer) return;

        const auto cfg = settings::read();
        const bool enabled = isPlayer1 ? cfg.p1Enabled : cfg.p2Enabled;
        CCNode* player = isPlayer1 ? static_cast<CCNode*>(m_player1)
                    : static_cast<CCNode*>(m_player2);
        const int holdTag = isPlayer1 ? cfg::HOLD_TAG_P1 : cfg::HOLD_TAG_P2;
        const int trailTag = isPlayer1 ? cfg::TRAIL_TAG_P1 : cfg::TRAIL_TAG_P2;

        if (!player || !player->getParent()) return;

        if (down) {
            if (!enabled) return;

            const RingColor col = resolveColor(isPlayer1, player, cfg);
            const CCPoint pos = player->getPosition();

            // Optional click burst. This can now be disabled independently so
            // the hold ring/trail can be used as a standalone visualization.
            if (cfg.clickBurstEnabled)
                spawnPulse(m_objectLayer, pos, col, cfg);

            if (cfg.holdEnabled) {
                m_objectLayer->removeChildByTag(holdTag, true);
                auto* hold = HoldRingNode::create(
                    col.r, col.g, col.b,
                    cfg.holdOpacity,
                    cfg.holdScale,
                    cfg.holdThickness,
                    cfg.fadeDuration,
                    cfg.additive,
                    cfg.holdFollow,
                    player,
                    cfg.ringSegments
                );
                if (hold) {
                    hold->setTag(holdTag);
                    hold->setPosition(pos);
                    m_objectLayer->addChild(hold, cfg::Z_HOLD);
                }
            }

            if (cfg.holdTrailEnabled) {
                m_objectLayer->removeChildByTag(trailTag, true);
                auto* trail = HoldTrailNode::create(
                    col.r, col.g, col.b,
                    cfg.holdTrailOpacity,
                    cfg.holdTrailThickness,
                    cfg.fadeDuration,
                    cfg.holdTrailMarker,
                    pos,
                    player
                );
                if (trail) {
                    trail->setTag(trailTag);
                    m_objectLayer->addChild(trail, cfg::Z_TRAIL);
                }
            }

        } else {
            // Always fade existing hold visuals regardless of current setting
            // values, so changing settings mid-run cannot strand an indicator.
            if (auto* node = m_objectLayer->getChildByTag(holdTag)) {
                static_cast<HoldRingNode*>(node)->startFade();
                node->setTag(0);
            }

            if (auto* node = m_objectLayer->getChildByTag(trailTag)) {
                static_cast<HoldTrailNode*>(node)->startFade();
                node->setTag(0);
            }

            if (cfg.clickBurstEnabled && cfg.releaseBurst) {
                const RingColor col = resolveColor(isPlayer1, player, cfg);
                spawnPulse(m_objectLayer, player->getPosition(), col, cfg);
            }
        }
    }
};


// PlayLayer::resetLevel() is hookable in GD 2.2081, unlike
// GJBaseGameLayer::resetLevel(), which is inline in the generated bindings.
class $modify(ClickIndicatorPlayLayer, PlayLayer) {

    void resetLevel() {
        if (m_objectLayer) {
            m_objectLayer->removeChildByTag(cfg::HOLD_TAG_P1, true);
            m_objectLayer->removeChildByTag(cfg::HOLD_TAG_P2, true);
            m_objectLayer->removeChildByTag(cfg::TRAIL_TAG_P1, true);
            m_objectLayer->removeChildByTag(cfg::TRAIL_TAG_P2, true);
        }

        PlayLayer::resetLevel();
    }
};
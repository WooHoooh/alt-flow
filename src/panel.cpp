// Alt Flow control panel: a flat dark card in the violet of the logo, drawn with plain cocos nodes
// inside a Geode popup, so it works the same with a mouse or a finger. Opened from the pause menu
// (or F5 on PC). One fixed look, GD's own fonts, nothing borrowed from another mod.

#include "shared.hpp"

#include <Geode/Geode.hpp>
#include <Geode/ui/GeodeUI.hpp>
#include <Geode/ui/Notification.hpp>
#include <Geode/ui/Popup.hpp>

#include <array>
#include <cctype>
#include <cmath>
#include <functional>
#include <string>

using namespace geode::prelude;

namespace {

// ---- layout, in panel units (the card is scaled to fit the screen) ----
constexpr float W = 396.f, H = 276.f, PAD = 18.f;
constexpr float HEAD = 48.f, FOOT = 30.f;
constexpr float TOP = 186.f, BOTTOM = 36.f; // content area between the tabs and the footer
constexpr float GAP = 12.f;
constexpr float kColW = (W - 2 * PAD - GAP) / 2;

constexpr char const* kModes[] = {"cube", "ship", "ball", "ufo", "wave", "robot", "spider", "swing"};
constexpr char const* kModeNames[] = {"Cube", "Ship", "Ball", "UFO", "Wave", "Robot", "Spider", "Swing"};
constexpr char const* kTabs[] = {"Live", "Speed", "Modes", "Wave"};

struct Stepper {
    char const* key;
    char const* name;
    int min, max, step;
    char const* unit;
};

// ---- colors: one palette, violet to match the logo ----
ccColor3B hex(int rgb) {
    return ccc3((rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255);
}

ccColor4F c4(ccColor3B c, float a = 1.f) {
    return ccc4f(c.r / 255.f, c.g / 255.f, c.b / 255.f, a);
}

ccColor3B cBg() { return hex(0x13101C); }         // the card
ccColor3B cCard() { return hex(0x1B1829); }       // rows and tiles on the card
ccColor3B cInset() { return hex(0x262238); }      // controls sitting on a row
ccColor3B cAccent() { return hex(0x8B5CF6); }     // violet
ccColor3B cAccentSoft() { return hex(0xB9A4FF); } // violet for text and icons on dark
ccColor3B cText() { return hex(0xF4F2FB); }
ccColor3B cSub() { return hex(0xB0AAC8); }
ccColor3B cMute() { return hex(0x7E799A); }
ccColor3B cGood() { return hex(0x34D399); }       // the assist has the button

ccColor4F hairline(float a) {
    return ccc4f(1.f, 1.f, 1.f, a);
}

// ---- settings ----
Mod* mod() {
    return Mod::get();
}

bool flag(char const* key) {
    return mod()->getSettingValue<bool>(key);
}

int num(char const* key) {
    return static_cast<int>(mod()->getSettingValue<int64_t>(key));
}

bool isMobile() {
#ifdef GEODE_IS_MOBILE
    return true;
#else
    return false;
#endif
}

// ---- text: GD's own fonts, sized by line height so the look doesn't depend on a font's metrics ----
enum class FontKind { Display, Body };

char const* fontFile(FontKind kind) {
    return kind == FontKind::Display ? "bigFont.fnt" : "chatFont.fnt";
}

float lineHeightOf(FontKind kind) {
    static float cache[2] = {0.f, 0.f};
    float& h = cache[kind == FontKind::Display ? 0 : 1];
    if (h <= 0.f) {
        h = CCLabelBMFont::create("Ag", fontFile(kind))->getContentSize().height;
        if (h <= 1.f) h = 40.f; // never divide by nothing
    }
    return h;
}

std::string titleCase(std::string s) {
    if (!s.empty()) s[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(s[0])));
    return s;
}

// ---- shapes ----

// corner radii in order top-right, top-left, bottom-left, bottom-right
void roundRect(CCDrawNode* d, CCRect r, std::array<float, 4> rad, ccColor4F fill, float border = 0.f, ccColor4F edge = {0, 0, 0, 0}) {
    if (r.size.width <= 0.f || r.size.height <= 0.f) return;
    for (auto& x : rad) x = std::min({x, r.size.width / 2, r.size.height / 2});
    CCPoint corners[4] = {
        {r.getMaxX() - rad[0], r.getMaxY() - rad[0]}, {r.getMinX() + rad[1], r.getMaxY() - rad[1]},
        {r.getMinX() + rad[2], r.getMinY() + rad[2]}, {r.getMaxX() - rad[3], r.getMinY() + rad[3]},
    };
    constexpr int seg = 6;
    CCPoint pts[4 * (seg + 1)];
    int n = 0;
    for (int c = 0; c < 4; ++c) {
        if (rad[c] <= 0.f) {
            pts[n++] = corners[c];
            continue;
        }
        for (int i = 0; i <= seg; ++i) {
            float a = (c + i / static_cast<float>(seg)) * 1.5707963f;
            pts[n++] = corners[c] + CCPoint{std::cos(a), std::sin(a)} * rad[c];
        }
    }
    d->drawPolygon(pts, n, fill, border, border > 0.f ? edge : fill);
}

void roundRect(CCDrawNode* d, CCRect r, float rad, ccColor4F fill, float border = 0.f, ccColor4F edge = {0, 0, 0, 0}) {
    roundRect(d, r, {rad, rad, rad, rad}, fill, border, edge);
}

// an on/off switch: a pill with a knob
void drawSwitch(CCDrawNode* d, CCRect track, bool on) {
    float h = track.size.height;
    roundRect(d, track, h / 2, on ? c4(cAccent()) : c4(cInset()));
    CCPoint knob{on ? track.getMaxX() - h / 2 : track.getMinX() + h / 2, track.getMidY()};
    d->drawCircle(knob, h / 2 - 2.5f, on ? ccc4f(1.f, 1.f, 1.f, 1.f) : c4(cMute()), 0, {0, 0, 0, 0}, 16);
}

// a tiny picture of the wave's trail: dir 0 level, 1 climbing, -1 falling; the tilt is the Angle setting
void drawTrail(CCDrawNode* d, CCPoint center, float width, int dir, float angleDeg, ccColor4F color) {
    constexpr int cycles = 3;
    float slope = std::tan(angleDeg * 3.14159265f / 180.f);
    float hold = std::clamp(.5f + .5f * dir * slope, .15f, .85f);
    float cw = width / cycles;
    CCPoint pts[2 * cycles + 1];
    pts[0] = {0.f, 0.f};
    float x = 0.f, y = 0.f, lo = 0.f, hi = 0.f;
    for (int c = 0; c < cycles; ++c) {
        x += cw * hold, y += cw * hold;
        pts[2 * c + 1] = {x, y};
        x += cw * (1.f - hold), y -= cw * (1.f - hold);
        pts[2 * c + 2] = {x, y};
    }
    for (auto const& q : pts) lo = std::min(lo, q.y), hi = std::max(hi, q.y);
    float fit = std::min(1.f, 38.f / std::max(1.f, hi - lo)); // keep steep ones inside the tile, same angle
    CCPoint mid{x / 2, (lo + hi) / 2};
    for (int i = 0; i < 2 * cycles; ++i) {
        d->drawSegment(center + (pts[i] - mid) * fit, center + (pts[i + 1] - mid) * fit, .9f, color);
    }
    d->drawCircle(center + (pts[2 * cycles] - mid) * fit, 2.2f, color, 0, {0, 0, 0, 0}, 10);
}

CCRect grow(CCRect r, float by) {
    return {r.origin.x - by, r.origin.y - by, r.size.width + 2 * by, r.size.height + 2 * by};
}

CCRect join(CCRect a, CCRect b) {
    float x0 = std::min(a.getMinX(), b.getMinX()), y0 = std::min(a.getMinY(), b.getMinY());
    return {x0, y0, std::max(a.getMaxX(), b.getMaxX()) - x0, std::max(a.getMaxY(), b.getMaxY()) - y0};
}

// ---- where things sit, shared by the drawing and the tutorial spotlight ----
CCRect masterRect() { return {W - PAD - 24 - 8 - 24 - 12 - 56, H - HEAD / 2 - 12, 56, 24}; }
CCRect helpRect() { return {W - PAD - 24 - 8 - 24, H - HEAD / 2 - 12, 24, 24}; }
CCRect closeRect() { return {W - PAD - 24, H - HEAD / 2 - 12, 24, 24}; }
CCRect tabsRect() { return {PAD, H - HEAD - 8 - 26, W - 2 * PAD, 26}; }
float tabSlot() { return (tabsRect().size.width - 6) / 4; }
float tabCenterX(int tab) { return tabsRect().getMinX() + 3 + tabSlot() * (tab + .5f); }
CCRect statusCardRect() { return {PAD, TOP - 120, 150, 120}; }
CCRect showCpsRect() { return {PAD, BOTTOM, 150, 24}; }
CCRect activityRect() { return {PAD + 158, BOTTOM, W - 2 * PAD - 158, TOP - BOTTOM}; }
CCRect speedRow(int col, int i) { return {PAD + col * (kColW + GAP), TOP - 18 - 22.f * (i + 1) - 3.5f * i, kColW, 22}; }
CCRect tileRect(int i) {
    float w = (W - 2 * PAD - 18) / 4, h = 62;
    return {PAD + (i % 4) * (w + 6), TOP - 6 - h * (i / 4 + 1) - 8.f * (i / 4), w, h};
}
CCRect pathTileRect(int i) {
    float w = (W - 2 * PAD - 16) / 3, h = 70;
    return {PAD + i * (w + 8), TOP - 6 - h, w, h};
}
CCRect angleRowRect() { return {PAD, TOP - 6 - 70 - 12 - 22, W - 2 * PAD, 22}; }
CCRect settingsRect() { return {W - PAD - 82, 5, 82, 20}; }
CCRect resetRect() { return {W - PAD - 82 - 8 - 112, 5, 112, 20}; }

// the settings the recommended preset resets (their defaults are the recommended values)
constexpr char const* kPresetKeys[] = {
    "trigger-cps", "quick-start", "go-straight", "rhythm", "hold-percent", "cps-ship", "cps-wave", "cps-ufo", "cps-swing",
    "target-cps", "mode-ship", "mode-wave", "mode-ufo", "mode-swing", "mode-cube", "mode-ball", "mode-robot", "mode-spider",
};

struct TutorialStep {
    int tab;
    CCRect target; // zero size: no spotlight, card in the middle
    char const* title;
    char const* text;
    char const* mobileText; // nullptr: same as text
};

std::vector<TutorialStep> const& tutorialSteps() {
    static std::vector<TutorialStep> const steps = {
        {0, {}, "Quick tour", "Spam fast enough and the\nassist takes over at a steady\nspeed. Here's how it works.", nullptr},
        {0, masterRect(), "On / off", "Turns the assist on or off.\nF7 does it too.", "Turns the assist on or off."},
        {0, helpRect(), "Replay", "Tap ? any time to watch\nthis tour again.", nullptr},
        {0, tabsRect(), "Four pages", "Live, Speed, Modes and Wave.\nTap one to switch pages.", nullptr},
        {0, statusCardRect(), "Live", "Your CPS while you play. It\nturns green and shows the\nassist's speed when it's on.", nullptr},
        {0, {activityRect().getMinX(), TOP - 62, activityRect().size.width, 62}, "Activity",
         "Every takeover and hand-back\nshows up here, with how long\nit lasted and its clicks.", nullptr},
        {1, speedRow(0, 0), "Trigger CPS", "Spam this fast or faster and\nit takes over. Stop and you\nget control back.", nullptr},
        {1, speedRow(0, 2), "Go straight", "Ship, wave, UFO and swing\nhold your starting height,\nso you fly flat.", nullptr},
        {1, join(speedRow(1, 0), speedRow(1, 1)), "CPS by mode", "How fast it clicks in each\nmode. Higher wave CPS makes\na tighter zigzag.", nullptr},
        {1, resetRect(), "Recommended", "Puts every speed and mode\nback to the recommended\nsetup.", nullptr},
        {2, join(tileRect(0), tileRect(3)), "Modes", "Tap a gamemode to let the\nassist help there. Ship, wave,\nUFO and swing start on.", nullptr},
        {3, join(pathTileRect(0), pathTileRect(2)), "Wave path", "Level keeps the wave flat. Up\nand Down tilt it, like climbing\nor falling wave spam.", nullptr},
        {3, angleRowRect(), "Angle", "How steeply Up and Down\ntilt the wave, in degrees.", nullptr},
        {0, {}, "You're set", "Open this with F5 or the Alt\nFlow button in the pause menu,\nthen spam a ship section.",
         "Open this with the Alt Flow\nbutton in the pause menu, then\nspam a ship section."},
    };
    return steps;
}

CCDrawNode* drawNode() {
    auto d = CCDrawNode::create();
    d->setBlendFunc({GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA});
    return d;
}

class AAPanel : public Popup {
public:
    static inline AAPanel* s_open = nullptr;

    static AAPanel* create() {
        auto p = new AAPanel();
        if (p->init()) {
            p->autorelease();
            return p;
        }
        delete p;
        return nullptr;
    }

    void close() { onClose(nullptr); }

    ~AAPanel() override {
        if (s_open == this) s_open = nullptr;
    }

protected:
    CCNode* m_anim = nullptr; // only for the opening motion
    CCNode* m_root = nullptr;
    CCDrawNode* m_draw = nullptr;
    CCMenu* m_menu = nullptr;
    int m_tab = 0;
    float m_pillFrom = -1.f;
    bool m_dirty = false;
    float m_clock = 0.f;
    CCLabelBMFont* m_cpsLabel = nullptr;
    unsigned m_seenLog = 0;
    bool m_seenActive = false;
    bool m_seenEnabled = true;
    // tutorial: step (-1 = off), spotlight easing toward its goal
    int m_tut = -1;
    float m_tutTime = 0.f;
    CCRect m_spot{W / 2, H / 2, 0, 0};
    CCRect m_spotGoal{W / 2, H / 2, 0, 0};
    CCDrawNode* m_tutShade = nullptr;

    bool init() {
        if (!Popup::init(W, H)) return false;
        s_open = this;
        m_bgSprite->setVisible(false);
        m_closeBtn->setVisible(false);
        m_anim = CCNode::create();
        m_anim->setContentSize({W, H});
        m_anim->setAnchorPoint({.5f, .5f});
        m_anim->setPosition({W / 2, H / 2});
        m_mainLayer->addChild(m_anim);
        m_root = CCNode::create();
        m_root->setContentSize({W, H});
        m_root->setAnchorPoint({.5f, .5f});
        m_root->setPosition({W / 2, H / 2});
        m_anim->addChild(m_root);
        m_anim->setScale(.94f);
        m_anim->runAction(CCEaseExponentialOut::create(CCScaleTo::create(.22f, 1.f)));
        if (!mod()->getSavedValue<bool>("tutorial-done", false) || flag("replay-tutorial")) startTutorial();
        build();
        this->scheduleUpdate();
        return true;
    }

    void onClose(CCObject* sender) override {
        if (m_tut >= 0) finishTutorial();
        Popup::onClose(sender);
    }

    // ---- building blocks ----

    CCLabelBMFont* label(std::string const& text, FontKind kind, float size, ccColor3B color, CCPoint pos,
                         CCPoint anchor = {0.f, .5f}, CCNode* parent = nullptr, float maxWidth = 0.f) {
        auto l = CCLabelBMFont::create(text.c_str(), fontFile(kind));
        l->setScale(size / lineHeightOf(kind));
        l->setColor(color);
        l->setAnchorPoint(anchor);
        l->setPosition(pos);
        if (maxWidth > 0.f) limitNodeWidth(l, maxWidth, l->getScale(), .02f); // shrink to fit, never grow
        (parent ? parent : m_root)->addChild(l, 5);
        return l;
    }

    // a tappable area; `draw` paints it in its own 0..w, 0..h space
    CCNode* button(CCRect r, std::function<void(CCDrawNode*, CCNode*)> draw, std::function<void()> action, float bounce = 1.08f,
                   CCMenu* menu = nullptr) {
        auto face = CCNode::create();
        face->setContentSize(r.size);
        auto d = drawNode();
        face->addChild(d);
        draw(d, face);
        auto item = CCMenuItemExt::createSpriteExtra(face, [this, action](auto) {
            action();
            m_dirty = true; // redraw next frame, never from inside the tap
        });
        item->m_scaleMultiplier = bounce;
        item->setPosition(r.origin + r.size / 2);
        (menu ? menu : m_menu)->addChild(item);
        return face;
    }

    // a small pill button in the footer and the activity card
    void ghostButton(CCRect r, char const* name, std::function<void()> action) {
        button(r, [this, name](CCDrawNode* d, CCNode* face) {
            auto sz = face->getContentSize();
            roundRect(d, {0, 0, sz.width, sz.height}, sz.height / 2, c4(cCard()));
            label(name, FontKind::Body, 10.5f, cAccentSoft(), sz / 2, {.5f, .5f}, face, sz.width - 14);
        }, action);
    }

    // name on the left, a - value + capsule on the right
    void stepperRow(CCRect r, Stepper const& s) {
        roundRect(m_draw, r, 8, c4(cCard()));
        label(s.name, FontKind::Body, 11.f, cSub(), {r.getMinX() + 10, r.getMidY()}, {0.f, .5f}, nullptr, r.size.width - 112);
        int v = num(s.key);
        CCRect cap{r.getMaxX() - 94, r.getMinY() + 3, 88, r.size.height - 6};
        roundRect(m_draw, cap, cap.size.height / 2, c4(cInset()));
        label(fmt::format("{}{}", v, s.unit), FontKind::Display, 12.f, cText(), {cap.getMidX(), cap.getMidY()}, {.5f, .5f}, nullptr, 34.f);
        for (int dir : {-1, 1}) {
            CCRect b{dir < 0 ? cap.getMinX() : cap.getMaxX() - 30, r.getMinY(), 30, r.size.height}; // the whole row height, easier to hit
            bool can = dir < 0 ? v > s.min : v < s.max;
            button(b, [dir, can](CCDrawNode* d, CCNode* face) {
                auto sz = face->getContentSize();
                ccColor4F ink = c4(can ? cAccentSoft() : cMute(), can ? 1.f : .5f);
                float cx = sz.width / 2, cy = sz.height / 2;
                d->drawSegment({cx - 4, cy}, {cx + 4, cy}, .9f, ink);
                if (dir > 0) d->drawSegment({cx, cy - 4}, {cx, cy + 4}, .9f, ink);
            }, [s, v, dir] {
                mod()->setSettingValue<int64_t>(s.key, std::clamp(v + dir * s.step, s.min, s.max));
            }, 1.12f);
        }
    }

    // name on the left, a switch on the right; the whole row is the tap target
    void switchRow(CCRect r, char const* name, bool on, std::function<void()> flip) {
        button(r, [this, name, on](CCDrawNode* d, CCNode* face) {
            auto sz = face->getContentSize();
            roundRect(d, {0, 0, sz.width, sz.height}, 8, c4(cCard()));
            label(name, FontKind::Body, 11.f, cSub(), {10, sz.height / 2}, {0.f, .5f}, face, sz.width - 62);
            drawSwitch(d, {sz.width - 42, sz.height / 2 - 8, 32, 16}, on);
        }, flip, 1.02f);
    }

    void switchRow(CCRect r, char const* name, char const* key) {
        bool on = flag(key);
        switchRow(r, name, on, [key, on] { mod()->setSettingValue<bool>(key, !on); });
    }

    // ---- the panel ----

    void build() {
        m_root->removeAllChildren();
        m_tutShade = nullptr;
        m_cpsLabel = nullptr;
        auto win = CCDirector::get()->getWinSize();
        m_root->setScale(std::min({num("ui-panel-size") / 100.f, (win.width - 8) / W, (win.height - 8) / H}));

        m_draw = drawNode();
        m_root->addChild(m_draw);
        m_menu = CCMenu::create();
        m_menu->setPosition({0, 0});
        m_root->addChild(m_menu, 10);

        // soft shadow, then the card
        for (int i = 4; i >= 1; --i) {
            float g = i * 3.5f;
            roundRect(m_draw, {-g, -g - 3, W + 2 * g, H + 2 * g}, 14 + g, ccc4f(0, 0, 0, .05f + (4 - i) * .018f));
        }
        roundRect(m_draw, {0, 0, W, H}, 14, c4(cBg(), .985f), 1, hairline(.09f));

        header();
        tabs();
        switch (m_tab) {
            case 0: liveTab(); break;
            case 1: speedTab(); break;
            case 2: modesTab(); break;
            default: waveTab(); break;
        }
        footer();
        if (m_tut >= 0) tutorial();

        auto live = aa::liveView();
        m_seenLog = aa::eventCount();
        m_seenActive = live.active;
        m_seenEnabled = live.enabled;
    }

    void header() {
        float cy = H - HEAD / 2;

        // the mark from the logo: a tile holding a triangle and a chevron
        roundRect(m_draw, {PAD, cy - 13, 26, 26}, 7, c4(cCard()), 1.25f, c4(cAccent()));
        CCPoint tri[3] = {{PAD + 7.f, cy - 6.f}, {PAD + 14.5f, cy}, {PAD + 7.f, cy + 6.f}};
        m_draw->drawPolygon(tri, 3, c4(cAccentSoft()), 0, c4(cAccentSoft()));
        m_draw->drawSegment({PAD + 16.f, cy + 5.5f}, {PAD + 20.5f, cy}, 1.1f, c4(cAccentSoft()));
        m_draw->drawSegment({PAD + 20.5f, cy}, {PAD + 16.f, cy - 5.5f}, 1.1f, c4(cAccentSoft()));
        label("Alt Flow", FontKind::Display, 17.f, cText(), {PAD + 36, cy + 1}, {0.f, .5f}, nullptr, masterRect().getMinX() - PAD - 36 - 10);

        bool on = flag("enabled");
        button(masterRect(), [this, on](CCDrawNode* d, CCNode* face) {
            auto sz = face->getContentSize();
            roundRect(d, {0, 0, sz.width, sz.height}, sz.height / 2, on ? c4(cAccent()) : c4(cInset()));
            d->drawCircle({on ? sz.width - 13 : 13, sz.height / 2}, 8.5f, on ? ccc4f(1.f, 1.f, 1.f, 1.f) : c4(cMute()), 0, {0, 0, 0, 0}, 18);
            label(on ? "ON" : "OFF", FontKind::Display, 10.f, on ? ccc3(255, 255, 255) : cSub(),
                  {on ? 18.f : sz.width - 19, sz.height / 2}, {.5f, .5f}, face);
        }, [] { aa::toggleAssist(); }, 1.06f);

        button(helpRect(), [this](CCDrawNode* d, CCNode* face) {
            auto sz = face->getContentSize();
            d->drawCircle({sz.width / 2, sz.height / 2}, sz.width / 2, c4(cInset()), 0, {0, 0, 0, 0}, 24);
            label("?", FontKind::Display, 12.f, cSub(), sz / 2, {.5f, .5f}, face);
        }, [this] { startTutorial(); });

        button(closeRect(), [](CCDrawNode* d, CCNode* face) {
            auto sz = face->getContentSize();
            d->drawCircle({sz.width / 2, sz.height / 2}, sz.width / 2, c4(cInset()), 0, {0, 0, 0, 0}, 24);
            float cx = sz.width / 2, cy = sz.height / 2;
            d->drawSegment({cx - 3.6f, cy - 3.6f}, {cx + 3.6f, cy + 3.6f}, .9f, c4(cSub()));
            d->drawSegment({cx - 3.6f, cy + 3.6f}, {cx + 3.6f, cy - 3.6f}, .9f, c4(cSub()));
        }, [this] { this->close(); });
    }

    // a segmented control: the violet pill slides to the page you pick
    void tabs() {
        CCRect r = tabsRect();
        roundRect(m_draw, r, 9, c4(cCard()));
        float slot = tabSlot();
        float pillH = r.size.height - 6;

        auto pill = drawNode();
        roundRect(pill, {-slot / 2, -pillH / 2, slot, pillH}, 7, c4(cAccent()));
        float x = tabCenterX(m_tab), y = r.getMidY();
        pill->setPosition({m_pillFrom >= 0 ? m_pillFrom : x, y});
        m_root->addChild(pill, 6);
        if (m_pillFrom >= 0) pill->runAction(CCEaseExponentialOut::create(CCMoveTo::create(.25f, {x, y})));
        m_pillFrom = -1.f;

        for (int i = 0; i < 4; ++i) {
            button({r.getMinX() + 3 + slot * i, r.getMinY(), slot, r.size.height}, [this, i](CCDrawNode*, CCNode* face) {
                auto sz = face->getContentSize();
                label(kTabs[i], FontKind::Display, 11.f, i == m_tab ? ccc3(255, 255, 255) : cSub(), sz / 2, {.5f, .5f}, face);
            }, [this, i] {
                m_pillFrom = tabCenterX(m_tab);
                m_tab = i;
            }, 1.f);
        }
    }

    void liveTab() {
        auto live = aa::liveView();

        // status
        CCRect card = statusCardRect();
        roundRect(m_draw, card, 10, c4(cCard()));
        ccColor3B tone = !live.enabled ? cMute() : live.active ? cGood() : cAccentSoft();
        CCPoint dot{card.getMinX() + 16, card.getMaxY() - 16};
        if (live.active) m_draw->drawCircle(dot, 7.5f, c4(cGood(), .18f), 0, {0, 0, 0, 0}, 20);
        m_draw->drawCircle(dot, 3.5f, c4(tone), 0, {0, 0, 0, 0}, 16);
        label(!live.enabled ? "Off" : live.active ? "Spamming" : "Watching", FontKind::Body, 11.f, cSub(), {dot.x + 11, dot.y});

        m_cpsLabel = label(cpsText(live), FontKind::Display, 36.f, live.active ? cGood() : cText(), {card.getMidX(), card.getMaxY() - 52},
                           {.5f, .5f}, nullptr, card.size.width - 24);
        label(live.active ? "assist CPS" : "your CPS", FontKind::Body, 10.f, cMute(), {card.getMidX(), card.getMaxY() - 76}, {.5f, .5f});
        m_draw->drawSegment({card.getMinX() + 14, card.getMaxY() - 87}, {card.getMaxX() - 14, card.getMaxY() - 87}, .5f, hairline(.08f));

        std::string a, b;
        if (!live.enabled) a = "Assist is off", b = "Tap ON in the corner";
        else if (live.active) a = titleCase(live.mode), b = live.path ? live.path : live.straight ? "Flying straight" : "Steady rhythm";
        else if (live.inLevel && !live.modeOn) a = fmt::format("{} is off", titleCase(live.mode)), b = "Turn it on in Modes";
        else a = fmt::format("Spam {}+ CPS", num("trigger-cps")), b = "to hand it over";
        label(a, FontKind::Body, 11.f, cText(), {card.getMidX(), card.getMaxY() - 99}, {.5f, .5f}, nullptr, card.size.width - 20);
        label(b, FontKind::Body, 10.f, cMute(), {card.getMidX(), card.getMaxY() - 110}, {.5f, .5f}, nullptr, card.size.width - 20);

        switchRow(showCpsRect(), "CPS on screen", "show-cps");

        // activity
        CCRect feed = activityRect();
        roundRect(m_draw, feed, 10, c4(cCard()));
        label("Activity", FontKind::Display, 12.f, cText(), {feed.getMinX() + 14, feed.getMaxY() - 17});
        ghostButton({feed.getMaxX() - 56, feed.getMaxY() - 27, 46, 20}, "Clear", [] { aa::clearLog(); });
        m_draw->drawSegment({feed.getMinX() + 12, feed.getMaxY() - 34}, {feed.getMaxX() - 12, feed.getMaxY() - 34}, .5f, hairline(.08f));

        auto& log = aa::eventLog();
        if (log.empty()) {
            label("Nothing yet", FontKind::Body, 11.f, cSub(), {feed.getMidX(), feed.getMidY() - 4}, {.5f, .5f});
            label("Spam a ship section to see it work.", FontKind::Body, 10.f, cMute(), {feed.getMidX(), feed.getMidY() - 18}, {.5f, .5f}, nullptr,
                  feed.size.width - 24);
        }
        int row = 0;
        for (auto it = log.rbegin(); it != log.rend() && row < 4; ++it, ++row) {
            auto parts = splitLine(*it);
            float cy = feed.getMaxY() - 48 - row * 26.f;
            bool latest = row == 0;
            m_draw->drawCircle({feed.getMinX() + 18, cy}, 2.4f, c4(latest ? cAccentSoft() : cMute()), 0, {0, 0, 0, 0}, 10);
            float width = feed.size.width - 42;
            bool two = !parts.second.empty();
            label(parts.first, FontKind::Body, 11.f, latest ? cText() : cSub(), {feed.getMinX() + 30, cy + (two ? 6.f : 0.f)}, {0.f, .5f}, nullptr, width);
            if (two) label(parts.second, FontKind::Body, 9.5f, cMute(), {feed.getMinX() + 30, cy - 6.f}, {0.f, .5f}, nullptr, width);
        }
    }

    // "took over at 12 cps - ship, flying straight" -> a headline and its detail
    static std::pair<std::string, std::string> splitLine(std::string const& s) {
        auto at = s.find(" - ");
        if (at == std::string::npos) return {titleCase(s), ""};
        return {titleCase(s.substr(0, at)), s.substr(at + 3)};
    }

    static std::string cpsText(aa::LiveView const& v) {
        return fmt::format("{:.0f}", v.active ? v.cps : v.yourCps);
    }

    void speedTab() {
        static Stepper const behavior[] = {
            {"trigger-cps", "Trigger CPS", 3, 30, 1, ""},
            {"hold-percent", "Hold", 10, 90, 5, "%"},
        };
        static Stepper const byMode[] = {
            {"cps-ship", "Ship", 2, 60, 1, ""},
            {"cps-wave", "Wave", 2, 60, 1, ""},
            {"cps-ufo", "UFO", 2, 60, 1, ""},
            {"cps-swing", "Swing", 2, 60, 1, ""},
            {"target-cps", "Other modes", 2, 60, 1, ""}, // cube, ball, robot, spider
        };
        label("BEHAVIOR", FontKind::Body, 9.f, cMute(), {PAD + 2, TOP - 7});
        label("CPS BY MODE", FontKind::Body, 9.f, cMute(), {PAD + kColW + GAP + 2, TOP - 7});

        stepperRow(speedRow(0, 0), behavior[0]);
        stepperRow(speedRow(0, 1), behavior[1]);
        switchRow(speedRow(0, 2), "Go straight", "go-straight");
        switchRow(speedRow(0, 3), "Quick start", "quick-start");
        bool match = mod()->getSettingValue<std::string>("rhythm") == "match your speed";
        switchRow(speedRow(0, 4), "Match my speed", match, [match] {
            mod()->setSettingValue<std::string>("rhythm", match ? "fixed" : "match your speed");
        });
        for (int i = 0; i < 5; ++i) stepperRow(speedRow(1, i), byMode[i]);
    }

    void modesTab() {
        auto gm = GameManager::get();
        auto c1 = gm->colorForIdx(gm->getPlayerColor());
        auto c2 = gm->colorForIdx(gm->getPlayerColor2());
        for (int i = 0; i < 8; ++i) {
            auto key = fmt::format("mode-{}", kModes[i]);
            bool on = flag(key.c_str());
            button(tileRect(i), [this, i, on, c1, c2, gm](CCDrawNode* d, CCNode* face) {
                auto sz = face->getContentSize();
                roundRect(d, {0, 0, sz.width, sz.height}, 9, on ? c4(cAccent(), .16f) : c4(cCard()), 1.f,
                          on ? c4(cAccent(), .9f) : hairline(.07f));
                auto type = static_cast<IconType>(i);
                auto icon = SimplePlayer::create(1);
                icon->updatePlayerFrame(gm->activeIconForType(type), type);
                icon->setColors(c1, c2); // always your own icon in your own colors, on or off
                icon->setScale(i == 5 || i == 6 ? .5f : .62f); // robot and spider are tall
                icon->setPosition({sz.width / 2, sz.height / 2 + 7});
                face->addChild(icon, 2);
                label(kModeNames[i], FontKind::Body, 10.5f, on ? cText() : cSub(), {sz.width / 2, 10}, {.5f, .5f}, face);
            }, [key, on] { mod()->setSettingValue<bool>(key, !on); }, 1.05f);
        }
    }

    void waveTab() {
        static char const* const keys[] = {"straight", "up", "down"};
        static char const* const names[] = {"Level", "Up", "Down"};
        std::string path = mod()->getSettingValue<std::string>("wave-path");
        int angle = num("wave-angle");
        for (int i = 0; i < 3; ++i) {
            bool on = path == keys[i];
            button(pathTileRect(i), [this, i, on, angle](CCDrawNode* d, CCNode* face) {
                auto sz = face->getContentSize();
                roundRect(d, {0, 0, sz.width, sz.height}, 9, on ? c4(cAccent(), .16f) : c4(cCard()), 1.f, on ? c4(cAccent(), .9f) : hairline(.07f));
                drawTrail(d, {sz.width / 2, 44}, 60.f, i == 1 ? 1 : i == 2 ? -1 : 0, static_cast<float>(angle), on ? c4(cAccentSoft()) : c4(cSub()));
                label(names[i], FontKind::Body, 10.5f, on ? cText() : cSub(), {sz.width / 2, 10}, {.5f, .5f}, face);
            }, [i] { mod()->setSettingValue<std::string>("wave-path", keys[i]); }, 1.05f);
        }

        static Stepper const angleStep = {"wave-angle", "Angle (degrees)", 5, 35, 5, ""};
        stepperRow(angleRowRect(), angleStep);
        if (path == "straight") { // the angle does nothing while the path is level
            auto dim = drawNode();
            roundRect(dim, angleRowRect(), 8, c4(cBg(), .5f));
            m_root->addChild(dim, 12);
        }
        float y = angleRowRect().getMinY() - 14;
        label("Wave only. Up and Down keep your CPS and tilt the zigzag.", FontKind::Body, 10.f, cMute(), {PAD + 2, y}, {0.f, .5f}, nullptr, W - 2 * PAD - 4);
        label("Level follows Go straight on the Speed page.", FontKind::Body, 10.f, cMute(), {PAD + 2, y - 13}, {0.f, .5f}, nullptr, W - 2 * PAD - 4);
    }

    void footer() {
        m_draw->drawSegment({PAD, FOOT}, {W - PAD, FOOT}, .5f, hairline(.07f));
        static char const* const hints[] = {
            isMobile() ? "Open this from the pause menu" : "F5 opens this, F7 turns it on or off",
            "Speeds are clicks per second.",
            "It only takes over in the modes you pick.",
            "Climbing and falling wave spam, same CPS.",
        };
        float right = (m_tab == 1 ? resetRect().getMinX() : settingsRect().getMinX()) - 10;
        label(hints[m_tab], FontKind::Body, 10.f, cMute(), {PAD + 1, 15}, {0.f, .5f}, nullptr, right - PAD);
        if (m_tab == 1) {
            ghostButton(resetRect(), "Recommended", [] {
                for (auto key : kPresetKeys) {
                    if (auto setting = mod()->getSetting(key)) setting->reset();
                }
                Notification::create("Recommended settings applied", NotificationIcon::Success, .8f)->show();
            });
        }
        ghostButton(settingsRect(), "All settings", [] { openSettingsPopup(mod()); });
    }

    // ---- tutorial ----

    void startTutorial() {
        goToStep(0);
    }

    void finishTutorial() {
        m_tut = -1;
        mod()->setSavedValue<bool>("tutorial-done", true);
        mod()->setSettingValue<bool>("replay-tutorial", false);
    }

    void goToStep(int step) {
        auto const& steps = tutorialSteps();
        if (step >= static_cast<int>(steps.size())) return finishTutorial();
        if (steps[step].tab != m_tab) {
            m_pillFrom = tabCenterX(m_tab);
            m_tab = steps[step].tab;
        }
        m_tut = step;
        m_tutTime = 0.f;
        auto t = steps[step].target;
        m_spotGoal = t.size.width > 0 ? grow(t, 4) : CCRect{W / 2, H / 2, 0, 0};
        m_dirty = true;
    }

    // the card goes below, above, right or left of the spotlight, whichever fits first
    static CCPoint cardOrigin(CCRect t, CCSize c) {
        float m = 6, gap = 6;
        CCPoint middle{(W - c.width) / 2, (H - c.height) / 2};
        if (t.size.width <= 0) return middle;
        float x = std::clamp(t.getMidX() - c.width / 2, m, W - m - c.width);
        float y = std::clamp(t.getMidY() - c.height / 2, m, H - m - c.height);
        if (t.getMinY() - gap - c.height >= 2) return {x, t.getMinY() - gap - c.height};
        if (t.getMaxY() + gap + c.height <= H - 2) return {x, t.getMaxY() + gap};
        if (t.getMaxX() + gap + c.width <= W - m) return {t.getMaxX() + gap, y};
        if (t.getMinX() - gap - c.width >= m) return {t.getMinX() - gap - c.width, y};
        return middle;
    }

    void tutorial() {
        m_menu->setEnabled(false); // only the card's buttons work during the tour
        auto const& steps = tutorialSteps();
        auto const& step = steps[m_tut];
        auto layer = CCNode::create();
        m_root->addChild(layer, 50);
        m_tutShade = drawNode();
        layer->addChild(m_tutShade);

        CCSize cs{206, 92};
        auto origin = cardOrigin(grow(step.target, step.target.size.width > 0 ? 4 : 0), cs);
        auto card = CCNode::create();
        card->setContentSize(cs);
        card->setAnchorPoint({.5f, .5f});
        card->setPosition(origin + cs / 2);
        layer->addChild(card, 5);

        auto d = drawNode();
        card->addChild(d);
        roundRect(d, grow({0, 0, cs.width, cs.height}, 3), 14, c4(cAccent(), .14f));
        roundRect(d, {0, 0, cs.width, cs.height}, 11, c4(cCard()), 1.25f, c4(cAccent(), .9f));
        label(step.title, FontKind::Display, 13.f, cText(), {14, cs.height - 16}, {0.f, .5f}, card, cs.width - 28);
        label(isMobile() && step.mobileText ? step.mobileText : step.text, FontKind::Body, 10.5f, cSub(), {14, cs.height - 28}, {0.f, 1.f}, card,
              cs.width - 28);

        int count = static_cast<int>(steps.size());
        label(fmt::format("{} / {}", m_tut + 1, count), FontKind::Body, 10.f, cMute(), {14, 16}, {0.f, .5f}, card);

        auto menu = CCMenu::create();
        menu->setPosition({0, 0});
        card->addChild(menu, 10);
        bool last = m_tut == count - 1;
        button({cs.width - 14 - 46, 6, 46, 20}, [this, last](CCDrawNode* bd, CCNode* face) {
            auto sz = face->getContentSize();
            roundRect(bd, {0, 0, sz.width, sz.height}, sz.height / 2, c4(cAccent()));
            label(last ? "Done" : "Next", FontKind::Display, 10.f, ccc3(255, 255, 255), sz / 2, {.5f, .5f}, face);
        }, [this] { goToStep(m_tut + 1); }, 1.08f, menu);
        if (!last) {
            button({cs.width - 14 - 46 - 6 - 38, 6, 38, 20}, [this](CCDrawNode* bd, CCNode* face) {
                auto sz = face->getContentSize();
                roundRect(bd, {0, 0, sz.width, sz.height}, sz.height / 2, c4(cInset()));
                label("Skip", FontKind::Body, 10.5f, cSub(), sz / 2, {.5f, .5f}, face);
            }, [this] { finishTutorial(); }, 1.08f, menu);
        }

        card->setScale(.92f);
        card->runAction(CCEaseBackOut::create(CCScaleTo::create(.28f, 1.f)));
        tutorialFrame(0.f);
    }

    // a shade with a hole that glides to the target
    void tutorialFrame(float dt) {
        if (!m_tutShade) return;
        m_tutTime += dt;
        float k = 1.f - std::exp(-dt * 12.f);
        m_spot.origin = m_spot.origin + (m_spotGoal.origin - m_spot.origin) * k;
        m_spot.size = CCSize{m_spot.size.width + (m_spotGoal.size.width - m_spot.size.width) * k,
                             m_spot.size.height + (m_spotGoal.size.height - m_spot.size.height) * k};

        auto shade = ccc4f(0, 0, 0, .6f);
        auto s = m_spot;
        m_tutShade->clear();
        if (s.size.width < 2 || s.size.height < 2) {
            roundRect(m_tutShade, {0, 0, W, H}, 14, shade);
        }
        else {
            roundRect(m_tutShade, {0, s.getMaxY(), W, H - s.getMaxY()}, {14, 14, 0, 0}, shade);
            roundRect(m_tutShade, {0, 0, W, s.getMinY()}, {0, 0, 14, 14}, shade);
            roundRect(m_tutShade, {0, s.getMinY(), s.getMinX(), s.size.height}, 0, shade);
            roundRect(m_tutShade, {s.getMaxX(), s.getMinY(), W - s.getMaxX(), s.size.height}, 0, shade);
            float pulse = .5f + .5f * std::sin(m_tutTime * 4.f);
            roundRect(m_tutShade, grow(s, 4), 10, {0, 0, 0, 0}, 1.f, c4(cAccent(), .12f + .18f * pulse));
            roundRect(m_tutShade, grow(s, 1.5f), 8, {0, 0, 0, 0}, 1.5f, c4(cAccent()));
        }
    }

    void update(float dt) override {
        if (m_tut >= 0) tutorialFrame(dt);
        m_clock += dt;
        if (!m_dirty && m_tab == 0 && m_clock >= .1f) {
            m_clock = 0.f;
            auto live = aa::liveView();
            if (live.active != m_seenActive || live.enabled != m_seenEnabled || aa::eventCount() != m_seenLog) m_dirty = true;
            else if (m_cpsLabel) m_cpsLabel->setString(cpsText(live).c_str());
        }
        if (m_dirty) {
            m_dirty = false;
            build();
        }
    }
};

} // namespace

void aa::togglePanel() {
    if (AAPanel::s_open) return AAPanel::s_open->close();
    if (auto pl = PlayLayer::get(); pl && !pl->m_isPaused) pl->pauseGame(false); // don't fly blind behind the panel
    if (auto panel = AAPanel::create()) panel->show();
}

// Alt Flow control panel, styled after afterflow's (MIT, prsevend; Poppins fonts, OFL):
// a rounded card with a gradient wordmark, an on/off pill, tabs and theme swatches. Plain cocos
// nodes in a Geode popup, so it works the same with a mouse or a finger.

#include "shared.hpp"

#include <Geode/Geode.hpp>
#include <Geode/ui/GeodeUI.hpp>
#include <Geode/ui/Notification.hpp>
#include <Geode/ui/Popup.hpp>

#include <array>
#include <cmath>
#include <functional>

using namespace geode::prelude;

namespace {

constexpr float W = 380.f, H = 260.f, PAD = 14.f;
constexpr float TOP = 176.f, BOTTOM = 30.f; // content area between the tabs and the footer

struct Theme {
    char const* name;
    int primary, secondary, surface, text;
};

// afterflow's palette
constexpr Theme kThemes[] = {
    {"violet", 0x7C4DFF, 0xB79CFF, 0x16161A, 0xE8E9EE},  {"ocean", 0x3BA3FF, 0x8FD0FF, 0x121820, 0xE6EDF5},
    {"mint", 0x34D399, 0x8EE8C4, 0x101A17, 0xE4F2EC},    {"amber", 0xFFB454, 0xFFD79B, 0x1B1712, 0xF4EDE3},
    {"rose", 0xFF6B8A, 0xFFB3C3, 0x1B1418, 0xF5E8EC},    {"mono", 0xC9CDD6, 0xFFFFFF, 0x15161A, 0xEDEEF2},
    {"ice", 0x67D5FF, 0xBFEAFF, 0x101820, 0xE6F1F7},     {"sunset", 0xFF8A5B, 0xFFC29E, 0x1C1512, 0xF7EAE2},
    {"grape", 0xA855F7, 0xD5B4FF, 0x17131F, 0xEFE7F7},   {"lime", 0xA3E635, 0xD9F99D, 0x141A10, 0xEAF3DF},
    {"crimson", 0xF43F5E, 0xFF9EB0, 0x1B1115, 0xF5E4E8}, {"slate", 0x94A3B8, 0xE2E8F0, 0x151A21, 0xE8EDF3},
    {"dark", 0xF2F3F7, 0x14161B, 0x0A0B0E, 0xF2F3F7},    {"white", 0x3B82F6, 0xFFFFFF, 0xF4F2ED, 0x1C1D22},
    {"graphene", 0x8B94A3, 0xC9D1DC, 0x111318, 0xE6EAF0},
};

constexpr char const* kModes[] = {"cube", "ship", "ball", "ufo", "wave", "robot", "spider", "swing"};
constexpr char const* kTabs[] = {"live", "speed", "modes", "look"};

struct Stepper {
    char const* key;
    char const* name;
    int min, max, step;
    char const* unit;
};

ccColor3B hex(int rgb) {
    return ccc3((rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255);
}

bool same(ccColor3B a, ccColor3B b) {
    return a.r == b.r && a.g == b.g && a.b == b.b;
}

ccColor4F c4(ccColor3B c, float a = 1.f) {
    return ccc4f(c.r / 255.f, c.g / 255.f, c.b / 255.f, a);
}

ccColor3B mix(ccColor3B a, ccColor3B b, float t) {
    auto m = [t](int x, int y) { return static_cast<GLubyte>(x + (y - x) * t); };
    return ccc3(m(a.r, b.r), m(a.g, b.g), m(a.b, b.b));
}

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

bool customFonts() {
    static bool has = fileExistsInSearchPaths("afterflow-body.fnt"_spr) && fileExistsInSearchPaths("afterflow-display.fnt"_spr);
    return has;
}

char const* displayFont() {
    return customFonts() ? "afterflow-display.fnt"_spr : "bigFont.fnt";
}

char const* bodyFont() {
    return customFonts() ? "afterflow-body.fnt"_spr : "bigFont.fnt";
}

// the bundled .fnt files have no -hd/-uhd copies, so cocos shrinks them by the texture quality
// factor; scaling back by it keeps text the same size on low, medium and high quality
float fontScale() {
    return customFonts() ? CCDirector::get()->getContentScaleFactor() : 1.f;
}

// colors and sizes, read from the settings each time the panel is drawn
struct Look {
    ccColor3B primary, secondary, surface, text;
    bool glow = true, light = false;
    float font = 1.f;

    static Look read() {
        Look l;
        l.primary = mod()->getSettingValue<ccColor3B>("ui-primary-color");
        l.secondary = mod()->getSettingValue<ccColor3B>("ui-secondary-color");
        l.surface = mod()->getSettingValue<ccColor3B>("ui-surface-color");
        l.text = mod()->getSettingValue<ccColor3B>("ui-text-color");
        l.glow = flag("ui-glow");
        l.font = num("ui-text-size") / 100.f;
        l.light = .299f * l.surface.r + .587f * l.surface.g + .114f * l.surface.b > 145.f;
        return l;
    }
    ccColor3B soft() const { return mix(surface, text, .68f); }
    ccColor3B faint() const { return mix(surface, text, .46f); }
    ccColor3B strong() const { return light ? ccc3(24, 25, 30) : ccc3(244, 245, 250); }
    ccColor4F tint(float a) const { return light ? ccc4f(0, 0, 0, a) : ccc4f(1, 1, 1, a); }
};

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

CCRect grow(CCRect r, float by) {
    return {r.origin.x - by, r.origin.y - by, r.size.width + 2 * by, r.size.height + 2 * by};
}

CCRect join(CCRect a, CCRect b) {
    float x0 = std::min(a.getMinX(), b.getMinX()), y0 = std::min(a.getMinY(), b.getMinY());
    return {x0, y0, std::max(a.getMaxX(), b.getMaxX()) - x0, std::max(a.getMaxY(), b.getMaxY()) - y0};
}

// where things sit on the panel, shared by the drawing and the tutorial spotlight
CCRect pillRect() { return {W - PAD - 32 - 86, H - 36, 86, 22}; }
CCRect helpRect() { return {W - PAD - 32 - 86 - 28, H - 36, 22, 22}; }
CCRect closeRect() { return {W - PAD - 26, H - 36, 26, 22}; }
CCRect tabsRect() { return {PAD, H - 72, W - 2 * PAD, 26}; }
CCRect liveCardRect() { return {PAD, BOTTOM, 124, TOP - BOTTOM}; }
CCRect logCardRect() { return {PAD + 132, BOTTOM, W - 2 * PAD - 132, TOP - BOTTOM}; }
constexpr float kColW = (W - 2 * PAD - 8) / 2;
CCRect speedRow(int col, int i) { return {PAD + col * (kColW + 8), TOP - 4 - (i + 1) * 24.f - i * 3.f, kColW, 24}; }
CCRect chipRect(int i) {
    float w = (W - 2 * PAD - 18) / 4, h = 62;
    return {PAD + (i % 4) * (w + 6), TOP - 6 - (i / 4 + 1) * h - (i / 4) * 8.f, w, h};
}
CCRect swatchRect(int i) {
    float w = (W - 2 * PAD - 24) / 5, h = 22;
    return {PAD + (i % 5) * (w + 6), TOP - 5 - (i / 5 + 1) * h - (i / 5) * 5.f, w, h};
}
CCRect recommendedRect() { return {W - PAD - 72 - 6 - 84, 6, 84, 18}; }

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
        {0, {}, "hey! quick tour", "spam fast enough and the assist\ntakes over, clicking at a steady\nspeed for you. here's how.", nullptr},
        {0, pillRect(), "on / off", "tap this pill to turn the assist\non or off. f7 does it too.", "tap this pill to turn the assist\non or off."},
        {0, helpRect(), "replay", "tap ? any time to watch this\ntour again.", nullptr},
        {0, tabsRect(), "four pages", "live, speed, modes and look.\ntap one to switch pages.", nullptr},
        {0, liveCardRect(), "live readout", "your cps while you play. it\nturns green and shows the\nassist's speed when it's on.", nullptr},
        {0, {logCardRect().getMinX(), TOP - 60, logCardRect().size.width, 60}, "log",
         "every takeover and hand-back\nshows up here, with how long\nit lasted and how many clicks.", nullptr},
        {1, speedRow(0, 0), "trigger cps", "spam at least this fast and it\ntakes over. stop spamming and\nyou get control back.", nullptr},
        {1, speedRow(0, 3), "go straight", "ship, wave, ufo and swing hold\nthe height you started at, so\nyou fly flat instead of drifting.", nullptr},
        {1, join(speedRow(1, 0), speedRow(1, 1)), "speed per mode", "how fast it clicks in each mode.\nhigher wave cps makes a tighter\nzigzag.", nullptr},
        {1, recommendedRect(), "recommended", "puts every speed and mode back\nto the recommended setup.", nullptr},
        {2, join(chipRect(0), chipRect(3)), "modes", "tap a gamemode to let the assist\nhelp there. ship, wave, ufo and\nswing are on by default.", nullptr},
        {3, join(swatchRect(0), swatchRect(9)), "look", "pick a theme. glow, panel size\nand text size are right below.", nullptr},
        {0, {}, "you're set", "open this with f5 or the alt flow\nbutton in the pause menu, then\nspam a ship part to try it.",
         "open this with the alt flow button\nin the pause menu, then spam a\nship part to try it."},
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
    CCNode* m_root = nullptr;
    CCDrawNode* m_draw = nullptr;
    CCMenu* m_menu = nullptr;
    Look m_look;
    int m_tab = 0;
    float m_underlineFrom = -1.f;
    bool m_dirty = false;
    float m_clock = 0.f;
    CCLabelBMFont* m_cpsLabel = nullptr;
    unsigned m_seenLog = 0;
    bool m_seenActive = false;
    bool m_seenEnabled = true;
    // tutorial: step (-1 = off), spotlight easing toward its goal, the animated hand
    int m_tut = -1;
    float m_tutTime = 0.f;
    CCRect m_spot{W / 2, H / 2, 0, 0};
    CCRect m_spotGoal{W / 2, H / 2, 0, 0};
    CCPoint m_handFrom;
    CCDrawNode* m_tutShade = nullptr;
    CCDrawNode* m_tutHand = nullptr;

    bool init() {
        if (!Popup::init(W, H)) return false;
        s_open = this;
        m_bgSprite->setVisible(false);
        m_closeBtn->setVisible(false);
        m_root = CCNode::create();
        m_root->setContentSize({W, H});
        m_root->setAnchorPoint({.5f, .5f});
        m_root->setPosition({W / 2, H / 2});
        m_mainLayer->addChild(m_root);
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

    CCLabelBMFont* label(std::string const& text, bool display, float scale, ccColor3B color, CCPoint pos,
                         CCPoint anchor = {0.f, .5f}, CCNode* parent = nullptr) {
        auto l = CCLabelBMFont::create(text.c_str(), display ? displayFont() : bodyFont());
        l->setScale(scale * m_look.font * fontScale());
        l->setColor(color);
        l->setAnchorPoint(anchor);
        l->setPosition(pos);
        if (auto t = l->getTexture()) t->setAntiAliasTexParameters();
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

    void stepperRow(CCRect r, Stepper const& s) {
        roundRect(m_draw, r, 6, m_look.tint(.04f));
        auto name = label(s.name, false, .31f, m_look.soft(), {r.getMinX() + 10, r.getMidY()});
        limitNodeWidth(name, r.size.width - 108, name->getScale(), .1f);
        int v = num(s.key);
        label(fmt::format("{}{}", v, s.unit), true, .28f, m_look.strong(), {r.getMaxX() - 50, r.getMidY()}, {.5f, .5f});
        for (int dir : {-1, 1}) {
            CCRect b{dir < 0 ? r.getMaxX() - 96 : r.getMaxX() - 24, r.getMinY() + 3, 20, r.size.height - 6};
            bool can = dir < 0 ? v > s.min : v < s.max;
            button(b, [this, dir, can](CCDrawNode* d, CCNode* face) {
                auto sz = face->getContentSize();
                roundRect(d, {0, 0, sz.width, sz.height}, 5, m_look.tint(can ? .09f : .03f));
                label(dir < 0 ? "-" : "+", true, .42f, can ? m_look.primary : m_look.faint(), sz / 2, {.5f, .5f}, face);
            }, [s, v, dir] {
                mod()->setSettingValue<int64_t>(s.key, std::clamp(v + dir * s.step, s.min, s.max));
            });
        }
    }

    void toggleRow(CCRect r, char const* name, bool on, std::function<void()> flip) {
        button(r, [this, name, on](CCDrawNode* d, CCNode* face) {
            auto sz = face->getContentSize();
            roundRect(d, {0, 0, sz.width, sz.height}, 6, m_look.tint(.04f));
            auto text = label(name, false, .31f, m_look.soft(), {10, sz.height / 2}, {0.f, .5f}, face);
            limitNodeWidth(text, sz.width - 56, text->getScale(), .1f);
            CCRect track{sz.width - 40, sz.height / 2 - 7, 30, 14};
            roundRect(d, track, 7, on ? c4(m_look.primary) : m_look.tint(.14f));
            CCPoint knob{on ? track.getMaxX() - 7 : track.getMinX() + 7, track.getMidY()};
            d->drawCircle(knob, 5, on ? c4(m_look.surface) : m_look.tint(.55f), 0, {0, 0, 0, 0}, 16);
        }, flip, 1.03f);
    }

    void toggleRow(CCRect r, char const* name, char const* key) {
        bool on = flag(key);
        toggleRow(r, name, on, [key, on] { mod()->setSettingValue<bool>(key, !on); });
    }

    // ---- the panel ----

    void build() {
        m_root->removeAllChildren();
        m_tutShade = m_tutHand = nullptr;
        m_look = Look::read();
        m_cpsLabel = nullptr;
        auto win = CCDirector::get()->getWinSize();
        m_root->setScale(std::min({num("ui-panel-size") / 100.f, (win.width - 8) / W, (win.height - 8) / H}));

        m_draw = drawNode();
        m_root->addChild(m_draw);
        m_menu = CCMenu::create();
        m_menu->setPosition({0, 0});
        m_root->addChild(m_menu, 10);

        if (m_look.glow) {
            for (int i = 4; i >= 1; --i) {
                float g = i * 3.f;
                roundRect(m_draw, {-g, -g, W + 2 * g, H + 2 * g}, 12 + g, c4(m_look.primary, .025f + (4 - i) * .012f));
            }
        }
        roundRect(m_draw, {0, 0, W, H}, 12, c4(m_look.surface, .97f), 1, m_look.tint(.08f));

        header();
        tabs();
        switch (m_tab) {
            case 0: liveTab(); break;
            case 1: speedTab(); break;
            case 2: modesTab(); break;
            default: lookTab(); break;
        }
        footer();
        if (m_tut >= 0) tutorial();

        auto live = aa::liveView();
        m_seenLog = aa::eventCount();
        m_seenActive = live.active;
        m_seenEnabled = live.enabled;
    }

    void header() {
        // wordmark, each letter fading from the primary to the secondary color
        auto mark = label("alt flow", true, .42f, m_look.primary, {PAD + 2, H - 24});
        limitNodeWidth(mark, helpRect().getMinX() - PAD - 8, mark->getScale(), .1f);
        if (auto kids = mark->getChildren()) {
            int n = kids->count(), i = 0;
            for (auto glyph : CCArrayExt<CCSprite*>(kids)) {
                glyph->setColor(mix(m_look.primary, m_look.secondary, n > 1 ? i++ / (n - 1.f) : 0.f));
            }
        }

        bool on = flag("enabled");
        button(pillRect(), [this, on](CCDrawNode* d, CCNode* face) {
            auto sz = face->getContentSize();
            roundRect(d, {0, 0, sz.width, sz.height}, 11, on ? c4(m_look.primary, .2f) : m_look.tint(.05f), 1,
                      on ? c4(m_look.primary, .9f) : m_look.tint(.16f));
            d->drawCircle({12, sz.height / 2}, 3, on ? c4(m_look.primary) : m_look.tint(.35f), 0, {0, 0, 0, 0}, 12);
            label(on ? "assist on" : "assist off", false, .34f, on ? m_look.strong() : m_look.faint(),
                  {sz.width / 2 + 5, sz.height / 2}, {.5f, .5f}, face);
        }, [] { aa::toggleAssist(); });

        button(helpRect(), [this](CCDrawNode* d, CCNode* face) {
            auto sz = face->getContentSize();
            roundRect(d, {0, 0, sz.width, sz.height}, 11, m_look.tint(.05f), 1, m_look.tint(.16f));
            label("?", true, .36f, m_look.soft(), sz / 2, {.5f, .5f}, face);
        }, [this] { startTutorial(); });

        button(closeRect(), [this](CCDrawNode* d, CCNode* face) {
            auto sz = face->getContentSize();
            roundRect(d, {0, 0, sz.width, sz.height}, 6, m_look.tint(.05f));
            label("x", true, .4f, m_look.soft(), sz / 2, {.5f, .5f}, face);
        }, [this] { this->close(); });
    }

    void tabs() {
        float slot = (W - 2 * PAD) / 4;
        for (int i = 0; i < 4; ++i) {
            button({PAD + slot * i, tabsRect().getMinY(), slot, tabsRect().size.height}, [this, i](CCDrawNode*, CCNode* face) {
                auto sz = face->getContentSize();
                label(kTabs[i], true, .32f, i == m_tab ? m_look.strong() : m_look.faint(), sz / 2, {.5f, .5f}, face);
            }, [this, i, slot] {
                m_underlineFrom = PAD + slot * (m_tab + .5f);
                m_tab = i;
            }, 1.f);
        }
        m_draw->drawSegment({PAD, H - 78}, {W - PAD, H - 78}, .5f, m_look.tint(.08f));

        auto underline = drawNode();
        roundRect(underline, {-18, -1.5f, 36, 3}, 1.5f, c4(m_look.primary));
        float x = PAD + slot * (m_tab + .5f);
        underline->setPosition({m_underlineFrom >= 0 ? m_underlineFrom : x, H - 77});
        m_root->addChild(underline, 6);
        if (m_underlineFrom >= 0) underline->runAction(CCEaseExponentialOut::create(CCMoveTo::create(.3f, {x, H - 77})));
        m_underlineFrom = -1.f;
    }

    void liveTab() {
        auto live = aa::liveView();
        CCRect card = liveCardRect();
        roundRect(m_draw, card, 8, m_look.tint(.04f));
        ccColor4F beacon = !live.enabled ? m_look.tint(.25f) : live.active ? ccc4f(.35f, 1.f, .55f, 1.f) : c4(m_look.primary, .8f);
        if (live.active) m_draw->drawCircle({card.getMinX() + 14, card.getMaxY() - 14}, 7, ccc4f(.35f, 1.f, .55f, .18f), 0, {0, 0, 0, 0}, 20);
        m_draw->drawCircle({card.getMinX() + 14, card.getMaxY() - 14}, 3.5f, beacon, 0, {0, 0, 0, 0}, 16);
        label(!live.enabled ? "off" : live.active ? "spamming" : "watching", false, .32f, m_look.soft(),
              {card.getMinX() + 24, card.getMaxY() - 14});

        m_cpsLabel = label(cpsText(live), true, .95f, live.active ? ccc3(90, 255, 140) : m_look.strong(),
                           {card.getMidX(), card.getMidY() + 18}, {.5f, .5f});
        label(live.active ? "assist cps" : "your cps", false, .3f, m_look.faint(), {card.getMidX(), card.getMidY() - 16}, {.5f, .5f});

        std::string a, b;
        if (!live.enabled) a = "assist is off", b = "tap the pill to turn on";
        else if (live.active) a = live.mode, b = live.straight ? "flying straight" : "steady rhythm";
        else if (live.inLevel && !live.modeOn) a = fmt::format("{} is off", live.mode), b = "turn it on in modes";
        else a = fmt::format("spam {}+ cps", num("trigger-cps")), b = "to hand it over";
        label(a, false, .32f, m_look.text, {card.getMidX(), BOTTOM + 30}, {.5f, .5f});
        label(b, false, .28f, m_look.faint(), {card.getMidX(), BOTTOM + 16}, {.5f, .5f});

        CCRect logCard = logCardRect();
        roundRect(m_draw, logCard, 8, m_look.tint(.04f));
        auto& log = aa::eventLog();
        auto title = label("log", true, .42f, m_look.text, {logCard.getMinX() + 12, logCard.getMaxY() - 14});
        label(fmt::format("/ {} events", log.size()), false, .3f, m_look.soft(),
              {title->getPositionX() + title->getScaledContentSize().width + 6, logCard.getMaxY() - 14});
        button({logCard.getMaxX() - 52, logCard.getMaxY() - 24, 44, 18}, [this](CCDrawNode* d, CCNode* face) {
            auto sz = face->getContentSize();
            roundRect(d, {0, 0, sz.width, sz.height}, 5, m_look.tint(.07f));
            label("clear", false, .3f, m_look.primary, sz / 2, {.5f, .5f}, face);
        }, [] { aa::clearLog(); });
        m_draw->drawSegment({logCard.getMinX() + 10, logCard.getMaxY() - 30}, {logCard.getMaxX() - 10, logCard.getMaxY() - 30}, .5f, m_look.tint(.07f));

        if (log.empty()) {
            label("nothing yet. spam a ship part", false, .3f, m_look.faint(), {logCard.getMinX() + 12, logCard.getMaxY() - 46});
        }
        int row = 0;
        for (auto it = log.rbegin(); it != log.rend() && row < 6; ++it, ++row) {
            auto line = label(*it, false, .32f, mix(m_look.text, m_look.faint(), row / 6.f),
                              {logCard.getMinX() + 12, logCard.getMaxY() - 46 - row * 19.f});
            limitNodeWidth(line, logCard.size.width - 24, line->getScale(), .1f);
        }
    }

    static std::string cpsText(aa::LiveView const& v) {
        return fmt::format("{:.0f}", v.active ? v.cps : v.yourCps);
    }

    void speedTab() {
        static Stepper const left[] = {
            {"trigger-cps", "trigger cps", 3, 30, 1, ""},
            {"hold-percent", "hold", 10, 90, 5, "%"},
            {"target-cps", "others", 2, 60, 1, ""},  // cube, ball, robot, spider
        };
        static Stepper const right[] = {
            {"cps-ship", "ship", 2, 60, 1, ""},
            {"cps-wave", "wave", 2, 60, 1, ""},
            {"cps-ufo", "ufo", 2, 60, 1, ""},
            {"cps-swing", "swing", 2, 60, 1, ""},
        };
        auto rowAt = speedRow;
        for (int i = 0; i < 3; ++i) stepperRow(rowAt(0, i), left[i]);
        toggleRow(rowAt(0, 3), "go straight", "go-straight");
        toggleRow(rowAt(0, 4), "quick start", "quick-start");
        for (int i = 0; i < 4; ++i) stepperRow(rowAt(1, i), right[i]);
        bool match = mod()->getSettingValue<std::string>("rhythm") == "match your speed";
        toggleRow(rowAt(1, 4), "match your speed", match, [match] {
            mod()->setSettingValue<std::string>("rhythm", match ? "fixed" : "match your speed");
        });
    }

    void modesTab() {
        auto gm = GameManager::get();
        auto c1 = gm->colorForIdx(gm->getPlayerColor());
        auto c2 = gm->colorForIdx(gm->getPlayerColor2());
        for (int i = 0; i < 8; ++i) {
            CCRect r = chipRect(i);
            auto key = fmt::format("mode-{}", kModes[i]);
            bool on = flag(key.c_str());
            button(r, [this, i, on, c1, c2, gm](CCDrawNode* d, CCNode* face) {
                auto sz = face->getContentSize();
                roundRect(d, {0, 0, sz.width, sz.height}, 8, on ? c4(m_look.primary, .16f) : m_look.tint(.04f), 1,
                          on ? c4(m_look.primary, .85f) : m_look.tint(.08f));
                auto type = static_cast<IconType>(i);
                auto icon = SimplePlayer::create(1);
                icon->updatePlayerFrame(gm->activeIconForType(type), type);
                icon->setColors(on ? c1 : m_look.faint(), on ? c2 : m_look.faint());
                icon->setScale(i == 5 || i == 6 ? .5f : .62f); // robot and spider are tall
                icon->setPosition({sz.width / 2, sz.height / 2 + 7});
                face->addChild(icon, 2);
                label(kModes[i], false, .32f, on ? m_look.text : m_look.faint(), {sz.width / 2, 10}, {.5f, .5f}, face);
            }, [key, on] { mod()->setSettingValue<bool>(key, !on); }, 1.05f);
        }
    }

    void lookTab() {
        auto primary = m_look.primary, surface = m_look.surface;
        for (int i = 0; i < 15; ++i) {
            auto const& t = kThemes[i];
            CCRect r = swatchRect(i);
            bool current = same(hex(t.primary), primary) && same(hex(t.surface), surface);
            button(r, [this, t, current](CCDrawNode* d, CCNode* face) {
                auto sz = face->getContentSize();
                roundRect(d, {0, 0, sz.width, sz.height}, 6, c4(hex(t.surface)), current ? 1.5f : 1.f,
                          current ? c4(hex(t.primary)) : m_look.tint(.12f));
                roundRect(d, {6, 6, 4, sz.height - 12}, 2, c4(hex(t.primary)));
                label(t.name, false, .25f, hex(t.text), {sz.width / 2 + 5, sz.height / 2}, {.5f, .5f}, face);
            }, [t] {
                mod()->setSettingValue<ccColor3B>("ui-primary-color", hex(t.primary));
                mod()->setSettingValue<ccColor3B>("ui-secondary-color", hex(t.secondary));
                mod()->setSettingValue<ccColor3B>("ui-surface-color", hex(t.surface));
                mod()->setSettingValue<ccColor3B>("ui-text-color", hex(t.text));
            }, 1.05f);
        }
        auto rowAt = [](int col, int i) { return CCRect{PAD + col * (kColW + 8), 67 - i * 27.f, kColW, 22}; };
        toggleRow(rowAt(0, 0), "glow", "ui-glow");
        toggleRow(rowAt(1, 0), "show cps in game", "show-cps");
        stepperRow(rowAt(0, 1), {"ui-panel-size", "panel size", 60, 140, 10, "%"});
        stepperRow(rowAt(1, 1), {"ui-text-size", "text size", 80, 140, 10, "%"});
    }

    void footer() {
        static char const* const hints[] = {
            isMobile() ? "open this from the pause menu" : "f5 opens this, f7 turns it on or off",
            "clicks per second: trigger is yours",
            "it only takes over in the modes you pick",
            "colors and sizes save with the mod",
        };
        auto hint = label(hints[m_tab], false, .28f, m_look.faint(), {PAD + 2, 15});
        limitNodeWidth(hint, (m_tab == 1 ? recommendedRect().getMinX() : W - PAD - 72) - PAD - 10, hint->getScale(), .1f);
        if (m_tab == 1) {
            button(recommendedRect(), [this](CCDrawNode* d, CCNode* face) {
                auto sz = face->getContentSize();
                roundRect(d, {0, 0, sz.width, sz.height}, 5, c4(m_look.primary, .18f), 1, c4(m_look.primary, .7f));
                label("recommended", false, .28f, m_look.strong(), sz / 2, {.5f, .5f}, face);
            }, [] {
                for (auto key : kPresetKeys) {
                    if (auto setting = mod()->getSetting(key)) setting->reset();
                }
                Notification::create("Recommended settings applied", NotificationIcon::Success, .8f)->show();
            });
        }
        button({W - PAD - 72, 6, 72, 18}, [this](CCDrawNode* d, CCNode* face) {
            auto sz = face->getContentSize();
            roundRect(d, {0, 0, sz.width, sz.height}, 5, m_look.tint(.06f));
            label("all settings", false, .28f, m_look.primary, sz / 2, {.5f, .5f}, face);
        }, [] { openSettingsPopup(mod()); });
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
            m_underlineFrom = PAD + (W - 2 * PAD) / 4 * (m_tab + .5f);
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
        float m = 6, gap = 10;
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
        m_tutHand = drawNode();
        layer->addChild(m_tutHand, 10);

        CCSize cs{220, 100};
        auto origin = cardOrigin(grow(step.target, step.target.size.width > 0 ? 4 : 0), cs);
        auto card = CCNode::create();
        card->setContentSize(cs);
        card->setAnchorPoint({.5f, .5f});
        card->setPosition(origin + cs / 2);
        layer->addChild(card, 5);
        m_handFrom = origin + cs / 2;

        auto d = drawNode();
        card->addChild(d);
        roundRect(d, grow({0, 0, cs.width, cs.height}, 3), 13, c4(m_look.primary, .12f));
        roundRect(d, {0, 0, cs.width, cs.height}, 10, c4(m_look.surface), 1.5f, c4(m_look.primary, .9f));
        label(step.title, true, .34f, m_look.strong(), {12, cs.height - 17}, {0.f, .5f}, card);
        auto body = label(isMobile() && step.mobileText ? step.mobileText : step.text, false, .29f, m_look.soft(),
                          {12, cs.height - 30}, {0.f, 1.f}, card);
        limitNodeWidth(body, cs.width - 24, body->getScale(), .1f);

        int count = static_cast<int>(steps.size());
        for (int i = 0, x = 12; i < count; ++i) {
            if (i == m_tut) {
                roundRect(d, {static_cast<float>(x), 12, 10, 4}, 2, c4(m_look.primary));
                x += 13;
            }
            else {
                d->drawCircle({x + 2.f, 14}, 2, i < m_tut ? c4(m_look.primary, .55f) : m_look.tint(.22f), 0, {0, 0, 0, 0}, 10);
                x += 6;
            }
        }

        auto menu = CCMenu::create();
        menu->setPosition({0, 0});
        card->addChild(menu, 10);
        bool last = m_tut == count - 1;
        button({cs.width - 12 - 48, 5, 48, 18}, [this, last](CCDrawNode* bd, CCNode* face) {
            auto sz = face->getContentSize();
            roundRect(bd, {0, 0, sz.width, sz.height}, 5, c4(m_look.primary));
            label(last ? "done" : "next", false, .3f, m_look.surface, sz / 2, {.5f, .5f}, face);
        }, [this] { goToStep(m_tut + 1); }, 1.08f, menu);
        if (!last) {
            button({cs.width - 12 - 48 - 6 - 38, 5, 38, 18}, [this](CCDrawNode* bd, CCNode* face) {
                auto sz = face->getContentSize();
                roundRect(bd, {0, 0, sz.width, sz.height}, 5, m_look.tint(.06f));
                label("skip", false, .3f, m_look.faint(), sz / 2, {.5f, .5f}, face);
            }, [this] { finishTutorial(); }, 1.08f, menu);
        }

        card->setScale(.92f);
        card->runAction(CCEaseBackOut::create(CCScaleTo::create(.28f, 1.f)));
        tutorialFrame(0.f);
    }

    static float smooth(float x) {
        x = std::clamp(x, 0.f, 1.f);
        return x * x * (3 - 2 * x);
    }

    // shade with a hole that glides to the target, and a hand that taps it
    void tutorialFrame(float dt) {
        if (!m_tutShade || !m_tutHand) return;
        m_tutTime += dt;
        float k = 1.f - std::exp(-dt * 12.f);
        m_spot.origin = m_spot.origin + (m_spotGoal.origin - m_spot.origin) * k;
        m_spot.size = CCSize{m_spot.size.width + (m_spotGoal.size.width - m_spot.size.width) * k,
                             m_spot.size.height + (m_spotGoal.size.height - m_spot.size.height) * k};

        auto shade = ccc4f(0, 0, 0, .58f);
        auto s = m_spot;
        m_tutShade->clear();
        if (s.size.width < 2 || s.size.height < 2) {
            roundRect(m_tutShade, {0, 0, W, H}, 12, shade);
        }
        else {
            roundRect(m_tutShade, {0, s.getMaxY(), W, H - s.getMaxY()}, {12, 12, 0, 0}, shade);
            roundRect(m_tutShade, {0, 0, W, s.getMinY()}, {0, 0, 12, 12}, shade);
            roundRect(m_tutShade, {0, s.getMinY(), s.getMinX(), s.size.height}, 0, shade);
            roundRect(m_tutShade, {s.getMaxX(), s.getMinY(), W - s.getMaxX(), s.size.height}, 0, shade);
            float pulse = .5f + .5f * std::sin(m_tutTime * 4.f);
            roundRect(m_tutShade, grow(s, 4), 9, {0, 0, 0, 0}, 1.f, c4(m_look.primary, .12f + .18f * pulse));
            roundRect(m_tutShade, grow(s, 1.5f), 7, {0, 0, 0, 0}, 1.5f, c4(m_look.primary));
        }

        m_tutHand->clear();
        if (m_spotGoal.size.width <= 0) return;
        float u = std::fmod(m_tutTime, 2.4f);
        CCPoint to{m_spotGoal.getMidX(), m_spotGoal.getMidY()};
        CCPoint p = m_handFrom + (to - m_handFrom) * smooth((u - .2f) / .8f);
        float alpha = u < .2f ? u / .2f : u > 1.8f ? std::max(0.f, 1.f - (u - 1.8f) / .4f) : 1.f;
        float r = u > 1.f && u < 1.15f ? 5.f : 7.f;
        if (u > 1.05f && u < 1.75f) {
            float q = (u - 1.05f) / .7f;
            m_tutHand->drawCircle(to, 6 + 20 * q, {0, 0, 0, 0}, 1.5f, c4(m_look.primary, .6f * (1 - q)), 28);
        }
        m_tutHand->drawCircle(p + CCPoint{1, -1.5f}, r + 1.5f, ccc4f(0, 0, 0, .3f * alpha), 0, {0, 0, 0, 0}, 20);
        m_tutHand->drawCircle(p, r, ccc4f(1, 1, 1, .95f * alpha), 0, {0, 0, 0, 0}, 20);
        m_tutHand->drawCircle(p, r * .45f, c4(m_look.primary, alpha), 0, {0, 0, 0, 0}, 14);
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

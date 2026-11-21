// Alt Flow: while you spam the jump button at or above the trigger rate, it takes the
// button over and clicks at a steady rate on the 240 tps tick grid. Stop spamming and control
// goes straight back to you.

#include "assist.hpp"
#include "shared.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/ui/GeodeUI.hpp>
#include <Geode/ui/Notification.hpp>

#include <chrono>

using namespace geode::prelude;

namespace {

enum Mode { Cube, Ship, Ball, Ufo, Wave, Robot, Spider, Swing, ModeCount };
constexpr char const* kModeKeys[ModeCount] = {"cube", "ship", "ball", "ufo", "wave", "robot", "spider", "swing"};

Mode modeOf(PlayerObject* p) {
    if (p->m_isShip) return Ship;
    if (p->m_isBall) return Ball;
    if (p->m_isBird) return Ufo;
    if (p->m_isDart) return Wave;
    if (p->m_isRobot) return Robot;
    if (p->m_isSpider) return Spider;
    if (p->m_isSwing) return Swing;
    return Cube;
}

// settings, refreshed on every press of yours (the assist only runs while you press)
struct Config {
    bool enabled = true;
    double trigger = 8.0;
    bool quickStart = false;
    bool matchYou = false;
    bool straight = true;
    double hold = .5;
    int wavePath = 0;        // wave spam: 0 level, +1 climbing, -1 falling
    double waveAngle = 20.0; // degrees, for climbing and falling
    bool modeOn[ModeCount] = {};
    double modeCps[ModeCount] = {};
} g_cfg;

aa::Detector g_detector;
aa::Rhythm g_rhythm;
aa::Leveler g_level;
aa::Tilt g_tilt;
bool g_active = false;
bool g_down = false;     // what the assist last told the game
bool g_sending = false;
bool g_userHeld = false; // your finger, so control is handed back in the right state
bool g_userP1 = true;
unsigned g_lastTick = ~0u;
double g_cps = 0.0;      // what the assist is spamming at right now
double g_since = 0.0;    // when it took over
double g_stoppedAt = -1e9;
int g_clicks = 0;        // clicks it made since then
int g_lastMode = -1;

std::deque<std::string> g_log;
unsigned g_logCount = 0;

template <class... Args>
void note(fmt::format_string<Args...> f, Args&&... args) {
    g_log.push_back(fmt::format(f, std::forward<Args>(args)...));
    if (g_log.size() > 40) g_log.pop_front();
    ++g_logCount;
}

double now() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

void readSettings() {
    auto mod = Mod::get();
    g_cfg.enabled = mod->getSettingValue<bool>("enabled");
    g_cfg.trigger = static_cast<double>(mod->getSettingValue<int64_t>("trigger-cps"));
    g_cfg.quickStart = mod->getSettingValue<bool>("quick-start");
    g_cfg.matchYou = mod->getSettingValue<std::string>("rhythm") == "match your speed";
    g_cfg.straight = mod->getSettingValue<bool>("go-straight");
    g_cfg.hold = mod->getSettingValue<int64_t>("hold-percent") / 100.0;
    auto path = mod->getSettingValue<std::string>("wave-path");
    g_cfg.wavePath = path == "up" ? 1 : path == "down" ? -1 : 0;
    g_cfg.waveAngle = static_cast<double>(mod->getSettingValue<int64_t>("wave-angle"));
    double other = static_cast<double>(mod->getSettingValue<int64_t>("target-cps"));
    for (int m = 0; m < ModeCount; ++m) {
        g_cfg.modeOn[m] = mod->getSettingValue<bool>(fmt::format("mode-{}", kModeKeys[m]));
        g_cfg.modeCps[m] = other;
    }
    for (int m : {Ship, Wave, Ufo, Swing}) {
        g_cfg.modeCps[m] = static_cast<double>(mod->getSettingValue<int64_t>(fmt::format("cps-{}", kModeKeys[m])));
    }
}

void send(GJBaseGameLayer* layer, bool down) {
    g_down = down;
    auto queued = layer->m_queuedButtons.size();
    g_sending = true;
    layer->handleButton(down, static_cast<int>(PlayerButton::Jump), g_userP1);
    g_sending = false;
    g_clicks += down;
    // GD's queued input modes stamp inputs with wall-clock time; pin ours to this tick
    if (layer->m_queuedButtons.size() > queued) layer->m_queuedButtons.back().m_timestamp = layer->m_timestamp;
}

void stop(GJBaseGameLayer* layer, char const* why) {
    if (!g_active) return;
    g_active = false;
    g_stoppedAt = now();
    note("handed back{} - {:.1f}s - {} clicks", why, now() - g_since, g_clicks);
    if (g_down != g_userHeld) send(layer, g_userHeld);
}

bool flyingMode(Mode m) {
    return m == Ship || m == Wave || m == Ufo || m == Swing;
}

// ascending (+1) or descending (-1) wave spam; 0 for every other mode and for a level wave
int tiltOf(Mode m) {
    return m == Wave ? g_cfg.wavePath : 0;
}

// the rise per run that tilt asks for
double tiltSlope(int tilt) {
    return tilt * std::tan(g_cfg.waveAngle * 3.14159265358979323846 / 180.0);
}

// one tick of the assist: tilted wave spam, level flight when Go straight is on, steady spam otherwise
int stepAssist(PlayerObject* p, unsigned tick) {
    Mode mode = modeOf(p);
    double period = 240.0 / g_cps;
    if (int tilt = tiltOf(mode)) {
        return aa::tiltStep(g_tilt, g_rhythm, g_down, tick, p->getPositionX(), p->getPositionY(), p->m_isUpsideDown ? -1.0 : 1.0, period,
                            tiltSlope(tilt));
    }
    if (g_cfg.straight && flyingMode(mode)) {
        g_level.held = g_down;
        double y = p->getPositionY();
        double up = p->m_isUpsideDown ? -1.0 : 1.0;
        int half = std::max(1, static_cast<int>(std::lround(period / 2)));
        int gap = std::max(2, static_cast<int>(std::lround(period)));
        switch (mode) {
            case Ship: return g_level.stepHold(y, up, 10.0, half);
            case Wave: return g_level.stepHold(y, up, 1.0, half);
            case Ufo: return g_level.stepTap(y, up, 12.0, gap);
            default: return g_level.stepFlip(y, 2.0, gap);
        }
    }
    if (g_rhythm.held != g_down) { // last click came from the leveler
        g_rhythm.held = g_down;
        g_rhythm.releaseAt = tick;
    }
    return g_rhythm.step(tick, period, g_cfg.hold);
}

double cpsFor(Mode mode) {
    return g_cfg.matchYou ? std::clamp(g_detector.rate(), 2.0, 60.0) : g_cfg.modeCps[mode];
}

bool helpsHere(PlayLayer* pl) {
    return g_cfg.enabled && g_cfg.modeOn[modeOf(pl->m_player1)];
}

} // namespace

aa::LiveView aa::liveView() {
    LiveView v;
    v.enabled = Mod::get()->getSettingValue<bool>("enabled");
    v.active = g_active;
    v.cps = g_cps;
    v.yourCps = g_detector.alive(now(), 1.0) ? g_detector.rate() : 0.0;
    if (auto pl = PlayLayer::get()) {
        Mode m = modeOf(pl->m_player1);
        v.inLevel = true;
        v.mode = kModeKeys[m];
        v.modeOn = Mod::get()->getSettingValue<bool>(fmt::format("mode-{}", kModeKeys[m]));
        v.straight = g_active && g_cfg.straight && flyingMode(m) && tiltOf(m) == 0;
        if (g_active && tiltOf(m) != 0) v.path = tiltOf(m) > 0 ? "Climbing" : "Falling";
    }
    return v;
}

std::deque<std::string> const& aa::eventLog() { return g_log; }
unsigned aa::eventCount() { return g_logCount; }
void aa::clearLog() { g_log.clear(); ++g_logCount; }

void aa::toggleAssist() {
    bool on = !Mod::get()->getSettingValue<bool>("enabled");
    Mod::get()->setSettingValue<bool>("enabled", on);
    g_cfg.enabled = on;
    note("assist {}", on ? "on" : "off");
    Notification::create(on ? "Alt Flow on" : "Alt Flow off", NotificationIcon::Info, .8f)->show();
}

#ifdef GEODE_IS_DESKTOP
$on_mod(Loaded) { // phones have no keyboard: the panel opens from the pause menu there
    listenForKeybindSettingPresses("toggle-key", [](Keybind const&, bool down, bool repeat, double) {
        if (down && !repeat) aa::toggleAssist();
    });
    listenForKeybindSettingPresses("panel-key", [](Keybind const&, bool down, bool repeat, double) {
        if (down && !repeat) aa::togglePanel();
    });
}
#endif

class $modify(AAGameLayer, GJBaseGameLayer) {
    static void onModify(auto& self) {
        // see your raw clicks before input buffers (like afterflow) do anything with them
        (void)self.setHookPriority("GJBaseGameLayer::handleButton", Priority::VeryEarly);
    }

    void handleButton(bool down, int button, bool isPlayer1) {
        auto pl = PlayLayer::get();
        if (g_sending || !pl || static_cast<GJBaseGameLayer*>(pl) != this || button != static_cast<int>(PlayerButton::Jump)) {
            return GJBaseGameLayer::handleButton(down, button, isPlayer1);
        }
        g_userHeld = down;
        g_userP1 = isPlayer1;
        if (down) {
            readSettings();
            double t = now();
            g_detector.press(t);
            // right after a hand-back (a stumble in your spam), two quick clicks pick it straight back up
            int needed = g_cfg.quickStart || t - g_stoppedAt < .4 ? 2 : 3;
            if (!g_active && helpsHere(pl) && g_detector.spamming(g_cfg.trigger, needed)) {
                Mode mode = modeOf(pl->m_player1);
                unsigned tick = m_gameState.m_currentProgress;
                g_active = true;
                g_cps = cpsFor(mode);
                // the click that triggered it is its first click, so there's no gap at the start
                int tilt = tiltOf(mode);
                double up = pl->m_player1->m_isUpsideDown ? -1.0 : 1.0;
                g_rhythm.start(tick);
                g_rhythm.step(tick, 240.0 / g_cps, tilt ? g_tilt.holdFraction(tiltSlope(tilt), up) : g_cfg.hold, tilt != 0);
                g_level.start(pl->m_player1->getPositionY());
                g_level.change(true);
                g_tilt.start(pl->m_player1->getPositionX(), pl->m_player1->getPositionY());
                g_down = true;
                g_lastTick = ~0u;
                g_since = t;
                g_clicks = 1;
                g_lastMode = mode;
                bool level = tilt == 0 && g_cfg.straight && flyingMode(mode);
                note("took over at {:.0f} cps - {}{}", g_detector.rate(needed), kModeKeys[mode],
                     tilt > 0 ? ", climbing" : tilt < 0 ? ", falling" : level ? ", flying straight" : "");
                return GJBaseGameLayer::handleButton(down, button, isPlayer1);
            }
        }
        else {
            g_detector.release(now());
        }
        if (g_active) return; // the assist owns the button while you spam
        GJBaseGameLayer::handleButton(down, button, isPlayer1);
    }

    void processCommands(float dt, bool isHalfTick, bool isLastTick) {
        auto pl = PlayLayer::get();
        if (g_active && static_cast<GJBaseGameLayer*>(pl) == this) {
            unsigned tick = m_gameState.m_currentProgress;
            if (tick != g_lastTick) { // once per tick (half-tick calls share one)
                g_lastTick = tick;
                Mode mode = modeOf(pl->m_player1);
                if (!g_cfg.enabled) stop(this, " - switched off");
                else if (!g_cfg.modeOn[mode]) stop(this, fmt::format(" - {} is off", kModeKeys[mode]).c_str());
                else if (!g_detector.alive(now(), g_cfg.trigger)) stop(this, ""); // you stopped spamming
                else {
                    g_cps = cpsFor(mode);
                    if (mode != g_lastMode) {
                        note("{} -> {} - {:.0f} cps", kModeKeys[g_lastMode], kModeKeys[mode], g_cps);
                        g_lastMode = mode;
                    }
                    if (int action = stepAssist(pl->m_player1, tick)) send(this, action > 0);
                }
            }
        }
        GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);
    }
};

class $modify(AAPlayLayer, PlayLayer) {
    struct Fields {
        CCLabelBMFont* label = nullptr; // your live CPS, green while the assist has the button
    };

    void setupHasCompleted() {
        PlayLayer::setupHasCompleted();
        readSettings();
        auto label = CCLabelBMFont::create("0 cps", "bigFont.fnt");
        label->setScale(.4f);
        label->setAnchorPoint({0.f, 0.f});
        label->setPosition({6.f, 6.f});
        label->setID("cps-label"_spr);
        m_uiLayer->addChild(label, 1000);
        m_fields->label = label;
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        auto label = m_fields->label;
        if (!label) return;
        label->setVisible(Mod::get()->getSettingValue<bool>("show-cps"));
        if (g_active) {
            int tilt = tiltOf(modeOf(m_player1));
            bool level = tilt == 0 && g_cfg.straight && flyingMode(modeOf(m_player1));
            label->setString(fmt::format("{} {:.0f} cps", tilt > 0 ? "climbing" : tilt < 0 ? "falling" : level ? "straight" : "assist", g_cps).c_str());
            label->setColor({90, 255, 140});
            label->setOpacity(255);
        }
        else {
            double cps = g_detector.alive(now(), 1.0) ? g_detector.rate() : 0.0;
            auto off = !g_cfg.enabled ? "  (assist off)" : !g_cfg.modeOn[modeOf(m_player1)] ? "  (not this mode)" : "";
            label->setString(fmt::format("{:.0f} cps{}", cps, off).c_str());
            label->setColor({255, 255, 255});
            label->setOpacity(150);
        }
    }

    void resetLevel() {
        if (g_active) note("handed back - restart - {} clicks", g_clicks);
        g_active = false;
        g_down = false;
        g_rhythm.held = false;
        g_detector.reset();
        PlayLayer::resetLevel();
    }
};

// pause menu button that opens the panel (the only way in on phones)
class $modify(AAPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();
        auto sprite = ButtonSprite::create("alt flow");
        sprite->setScale(.6f);
        auto menu = CCMenu::createWithItem(CCMenuItemSpriteExtra::create(sprite, this, menu_selector(AAPauseLayer::onAssistPanel)));
        auto size = CCDirector::get()->getWinSize();
        menu->setPosition({45.f, size.height - 22.f});
        menu->setID("assist-menu"_spr);
        this->addChild(menu, 100);
    }

    void onAssistPanel(CCObject*) {
        aa::togglePanel();
    }
};

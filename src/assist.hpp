#pragma once

// Game-independent half of Alt Flow, kept apart so tests/assist_test.cpp can check it.

#include <algorithm>
#include <cmath>

namespace aa {

// Watches your presses and releases (seconds) and decides when you are spamming.
struct Detector {
    double times[4] = {};
    int count = 0;
    double lastRelease = -1e9;

    void press(double t) {
        std::copy(times + 1, times + 4, times);
        times[3] = t;
        count = std::min(count + 1, 4);
    }

    void release(double t) { lastRelease = t; }

    double last() const { return times[3]; }

    // presses per second over your last `n` presses (fewer if that's all there is, at least 2)
    double rate(int n = 4) const {
        int k = std::min(n, count);
        if (k < 2) return 0.0;
        double span = times[3] - times[4 - k];
        return span > 0.0 ? (k - 1) / span : 0.0;
    }

    // the last `presses` presses came at least `trigger` per second; only the recent ones count,
    // so a slow first click doesn't hold back the takeover
    bool spamming(double trigger, int presses) const {
        return count >= presses && rate(presses) >= trigger;
    }

    // Still spamming: you pressed or let go recently, judged by your own speed (1.5 of your
    // click intervals), so fast spam hands back fast. Never longer than the trigger speed allows.
    bool alive(double now, double trigger) const {
        if (count == 0) return false;
        double r = rate();
        double limit = 1.5 / trigger;
        double gap = r > 0.0 ? std::clamp(1.5 / r, 0.05, limit) : limit;
        return now - std::max(last(), lastRelease) <= gap;
    }

    void reset() {
        count = 0;
        lastRelease = -1e9;
    }
};

// Steady press/release rhythm on the 240 tps tick grid.
struct Rhythm {
    double nextPress = 0.0;
    unsigned releaseAt = 0;
    bool held = false;

    void start(unsigned tick) {
        nextPress = tick;
        held = false;
    }

    // +1 press, -1 release, 0 nothing, for this tick. `holdFraction` of each period is held,
    // always at least one tick pressed and one released so every click registers.
    int step(unsigned tick, double period, double holdFraction) {
        if (held && tick >= releaseAt) {
            held = false;
            return -1;
        }
        if (!held && tick >= nextPress) {
            int whole = std::max(2, static_cast<int>(period));
            int hold = std::clamp(static_cast<int>(std::lround(period * holdFraction)), 1, whole - 1);
            held = true;
            releaseAt = tick + hold;
            nextPress += period;
            if (nextPress <= tick) nextPress = tick + period; // fell behind (lag): don't burst to catch up
            return 1;
        }
        return 0;
    }
};

// Keeps a flying player at the height the assist took over at. Each tick it looks at where the
// player is heading and decides whether the button should be down, but never switches faster
// than the spam speed allows, so it still clicks like spam, just level.
struct Leveler {
    double target = 0.0;
    double lastY = 0.0;
    bool held = false;
    int sinceSwitch = 1 << 20;

    void start(double y) {
        target = lastY = y;
        held = false;
        sinceSwitch = 1 << 20;
    }

    // Shared bookkeeping; returns this tick's vertical speed. A big jump in height is a
    // teleport or portal, so the new height becomes the one to hold.
    double observe(double y) {
        double vy = y - lastY;
        lastY = y;
        if (std::abs(vy) > 40.0) {
            target = y;
            vy = 0.0;
        }
        ++sinceSwitch;
        return vy;
    }

    int change(bool want) {
        held = want;
        sinceSwitch = 0;
        return want ? 1 : -1;
    }

    // Ship, wave: hold while heading below the target. `up` is +1 when pressing moves the
    // player up on screen (normal gravity), -1 when flipped. `minTicks` = shortest press/release.
    int stepHold(double y, double up, double lookahead, int minTicks) {
        double vy = observe(y);
        bool want = (target - (y + vy * lookahead)) * up > 0.0;
        if (want != held && sinceSwitch >= minTicks) return change(want);
        return 0;
    }

    // UFO: a short tap whenever it's heading below the target, at most one every `minGap` ticks.
    int stepTap(double y, double up, double lookahead, int minGap) {
        double vy = observe(y);
        if (held) return sinceSwitch >= 1 ? change(false) : 0;
        bool want = (target - (y + vy * lookahead)) * up > 0.0;
        return want && sinceSwitch >= minGap ? change(true) : 0;
    }

    // Swing: a tap flips gravity, so flip whenever it's moving away from the target.
    int stepFlip(double y, double deadzone, int minGap) {
        double vy = observe(y);
        if (held) return sinceSwitch >= 1 ? change(false) : 0;
        bool away = (y < target - deadzone && vy < 0.0) || (y > target + deadzone && vy > 0.0);
        return away && sinceSwitch >= minGap ? change(true) : 0;
    }
};

} // namespace aa

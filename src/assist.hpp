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
    double carry = 0.0; // what rounding the hold to whole ticks left over, paid back on the next clicks

    void start(unsigned tick) {
        nextPress = tick;
        held = false;
        carry = 0.0;
    }

    // +1 press, -1 release, 0 nothing, for this tick. `holdFraction` of each period is held,
    // always at least one tick pressed and one released so every click registers.
    // With `dither` the rounding error carries over, so the average hold is exact however few ticks a click has.
    int step(unsigned tick, double period, double holdFraction, bool dither = false) {
        if (held && tick >= releaseAt) {
            held = false;
            return -1;
        }
        if (!held && tick >= nextPress) {
            int whole = std::max(2, static_cast<int>(period));
            double want = period * holdFraction + (dither ? carry : 0.0);
            int hold = std::clamp(static_cast<int>(std::lround(want)), 1, whole - 1);
            if (dither) carry = std::clamp(want - hold, -1.0, 1.0);
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

// Ascending / descending wave spam. The clicks stay as steady as ever, but each one is held longer
// than it is released (or shorter), so the wave's trail tilts up or down at the angle you pick.
struct Tilt {
    double k = 1.0; // vertical speed per unit of horizontal speed: 1 for a normal wave, more for a mini one
    double lastX = 0.0, lastY = 0.0;

    void start(double x, double y) {
        lastX = x;
        lastY = y;
    }

    // Learns k from how the wave actually moves. Sliding along a block or a teleport says nothing about it.
    void observe(double x, double y) {
        double dx = x - lastX, dy = std::abs(y - lastY);
        lastX = x;
        lastY = y;
        if (dx > 0.01 && dx < 50.0 && dy > 0.3 * dx && dy < 4.0 * dx) k = std::clamp(k + (dy / dx - k) * 0.1, 0.5, 4.0);
    }

    // How much of each click to hold so the trail rises `slope` per unit of run (negative: it falls).
    // `up` is +1 when holding moves the wave up on screen, -1 when gravity is flipped.
    double holdFraction(double slope, double up) const {
        return std::clamp(0.5 + 0.5 * slope * up / k, 0.12, 0.88);
    }
};

// One tick of tilted wave spam: +1 press, -1 release, 0 nothing. `down` is what the assist last told the game.
inline int tiltStep(Tilt& tilt, Rhythm& rhythm, bool down, unsigned tick, double x, double y, double up, double period, double slope) {
    tilt.observe(x, y);
    if (rhythm.held != down) { // the last click came from the leveler
        rhythm.held = down;
        rhythm.releaseAt = tick;
    }
    return rhythm.step(tick, period, tilt.holdFraction(slope, up), true);
}

} // namespace aa

#pragma once

// What the game half (main.cpp) and the control panel (panel.cpp) share.

#include <deque>
#include <string>

namespace aa {

struct LiveView {
    bool enabled = true;
    bool active = false;   // the assist has the button right now
    bool straight = false; // ...and is flying level
    bool inLevel = false;
    bool modeOn = false;   // the current gamemode is one it helps in
    double cps = 0.0;      // what it's spamming at
    double yourCps = 0.0;  // how fast you're clicking
    char const* mode = "cube";
    char const* path = nullptr; // "Climbing" or "Falling" while the wave spam is tilted
};

LiveView liveView();
std::deque<std::string> const& eventLog(); // newest at the back
unsigned eventCount();                     // total ever logged, to notice new lines
void clearLog();
void toggleAssist();
void togglePanel();

} // namespace aa

# Alt Flow

A Geode mod for Geometry Dash 2.2081: spam the jump button fast enough and it takes over at a steady rate (ship, wave, ufo and swing can fly level). Targets Windows, Android 64-bit (ARM64) and Android 32-bit (ARMv7).

## Get the .geode (GitHub Actions)
1. Put these files in your repo, replacing the old ones. The workflow is `.github/workflows/multi-platform.yml`.
2. Add your logo image to the repo root, as `logo-source.webp` (or `.png` / `.jpg`; any name works if it is the only image there). The build turns it into `logo.png`: cropped to the icon, transparent outside the rounded corners, 336x336. A `logo.png` you commit yourself is used as it is.
3. Actions tab, "Build Alt Flow", Run workflow (pushing also starts it). When it finishes, download the **Alt Flow** artifact: `alternate.altflow.geode`, with Windows, Android 64-bit and Android 32-bit in one file.

The run summary lists what is inside. A platform that fails to compile shows as MISSING and the run turns red, but the platforms that did build are still packaged.

## Install
- Android: copy the .geode to `/storage/emulated/0/Android/media/com.geode.launcher/game/geode/mods/`. The same file works on 32-bit and 64-bit phones.
- Windows: put it in the `geode/mods` folder of your Geometry Dash folder.

Open the panel with F5 (PC) or the **alt flow** button in the pause menu. F7 turns the assist on or off on PC.

## Checks you can run on a computer
```
c++ -std=c++20 tests/assist_test.cpp && ./a.out     # prints: assist_test: ok
python3 tools/preflight.py
```

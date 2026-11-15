#!/usr/bin/env python3
"""Quick checks that run before the (slow) build, so a typo fails in seconds, not minutes."""
import json
import pathlib
import re
import sys

root = pathlib.Path(__file__).resolve().parent.parent
errors = []

mod = json.loads((root / "mod.json").read_text(encoding="utf-8"))

mod_id = mod.get("id", "")
if not re.fullmatch(r"[a-z0-9_-]+\.[a-z0-9_-]+", mod_id):
    errors.append(f"mod.json id {mod_id!r} must be lowercase 'developer.modname'")

version = mod.get("version", "")
if not re.fullmatch(r"v\d+\.\d+\.\d+", version):
    errors.append(f"mod.json version {version!r} must look like v1.2.3")

cmake = (root / "CMakeLists.txt").read_text(encoding="utf-8")
found = re.search(r"project\(\s*\w+\s+VERSION\s+([0-9.]+)", cmake)
if found is None:
    print("preflight: could not read the version from CMakeLists.txt (skipping that check)")
elif "v" + found.group(1) != version:
    errors.append(f"CMakeLists.txt VERSION {found.group(1)} and mod.json {version} differ - keep them the same")

gd = mod.get("gd", {})
for platform in ("win", "android"):  # "android" covers both the 32-bit and the 64-bit build
    if platform not in gd:
        errors.append(f"mod.json gd has no {platform!r} entry")

for rel in mod.get("resources", {}).get("files", []):
    if not (root / rel).is_file():
        errors.append(f"mod.json lists {rel} but the file is missing")

for rel in re.findall(r"src/[\w./-]+\.cpp", cmake):
    if not (root / rel).is_file():
        errors.append(f"CMakeLists.txt builds {rel} but the file is missing")

if errors:
    for e in errors:
        print(f"::error title=Alt Flow preflight::{e}")
    sys.exit(1)

print(f"preflight ok: {mod.get('name')} {version} ({mod_id}); gd platforms: {', '.join(sorted(gd))}")

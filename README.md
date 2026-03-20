# PotatoCrosshair

PotatoCrosshair is a crosshair overlay for Windows made in C++.

I made it for a battle royale game, but it can also be used in other games.


## Build

Command Prompt or PowerShell:

```powershell
cmake -S . -B build -G "NMake Makefiles"
cmake --build build
```

The app will be built here:

`build/PotatoCrosshair.exe`


## Notes

- The overlay is still centered on the main monitor.
- It works best in windowed or borderless fullscreen games.
- Some anti-cheat games may block overlays.

## Next steps

Possible next steps:

- per-monitor targeting
- custom crosshair editor and better PNG controls
- profiles and per-game settings
- zoom features
- Direct2D/DirectComposition rendering for richer visuals

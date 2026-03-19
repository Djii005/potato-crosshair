# PotatoCrosshair

PotatoCrosshair is a crosshair overlay for Windows made in C++.

I made it for a battle royale game, but it can also be used in other games.

Main features:

- transparent overlay
- settings window
- tray icon
- built-in crosshair styles
- custom colors
- PNG crosshair support
- saved settings in `%LocalAppData%\PotatoCrosshair\settings.ini`

## Hotkeys

- `F8`: show or hide the crosshair
- `F9`: change the style (`Cross`, `Dot`, `Circle`, `T-Shape`)
- `Ctrl` + `Alt` + `C`: cycle color preset
- `Ctrl` + `Alt` + `Up` / `Down`: change line length
- `Ctrl` + `Alt` + `Left` / `Right`: change the center gap
- `Ctrl` + `Alt` + `PageUp` / `PageDown`: change thickness
- `Ctrl` + `Alt` + `Home` / `End`: change opacity
- `Ctrl` + `Alt` + `R`: reset the built-in settings
- `Ctrl` + `Alt` + `Q`: quit the app

## Desktop UI

- the settings window opens when the app starts
- minimize and close both send the window to the tray
- left-click the tray icon opens the settings window
- right-click the tray icon shows `Open Settings`, `Show/Hide Overlay`, and `Quit`

## Build

Use Visual Studio Build Tools or Visual Studio with the C++ desktop workload.

From a Visual Studio Developer Command Prompt or Developer PowerShell:

```powershell
cmake -S . -B build -G "NMake Makefiles"
cmake --build build
```

The app will be built here:

`build/PotatoCrosshair.exe`

On first run the app creates:

- `%LocalAppData%\PotatoCrosshair\settings.ini`
- `%LocalAppData%\PotatoCrosshair\assets\custom-reticle.png` when you import a PNG

If you prefer a Visual Studio solution instead, use:

```powershell
cmake -S . -B build-vs -G "Visual Studio 17 2022" -A x64
cmake --build build-vs --config Release
```

## Notes

- The overlay is still centered on the main monitor.
- It works best in windowed or borderless fullscreen games.
- Some anti-cheat games may block overlays.

## Next steps

Possible next steps:

- per-monitor targeting
- custom crosshair editor and better PNG controls
- profiles and per-game settings
- Direct2D/DirectComposition rendering for richer visuals

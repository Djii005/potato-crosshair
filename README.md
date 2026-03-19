# PotatoCrosshair

`PotatoCrosshair` is a Windows crosshair overlay written in C++ with a native desktop control panel, system tray integration, and a potato-themed UI shell. It follows the basic overlay pattern used by apps like Crosshair X: a transparent, click-through, always-on-top window with a configurable reticle.

This repository now includes:

- transparent Win32 overlay with built-in reticle styles
- potato-themed native settings window with live preview
- system tray icon with open, toggle, and quit actions
- preset palette and free custom color picker
- imported transparent PNG reticles
- global hotkeys for size, gap, thickness, opacity, and quick toggle
- persistent settings stored in `%LocalAppData%\PotatoCrosshair\settings.ini`

## Hotkeys

- `F8`: toggle the crosshair on or off
- `F9`: cycle reticle style (`Cross`, `Dot`, `Circle`, `T-Shape`)
- `Ctrl` + `Alt` + `C`: cycle color preset
- `Ctrl` + `Alt` + `Up` / `Down`: increase or decrease line length
- `Ctrl` + `Alt` + `Left` / `Right`: decrease or increase center gap
- `Ctrl` + `Alt` + `PageUp` / `PageDown`: increase or decrease thickness
- `Ctrl` + `Alt` + `Home` / `End`: increase or decrease opacity
- `Ctrl` + `Alt` + `R`: reset built-in settings to defaults
- `Ctrl` + `Alt` + `Q`: quit the app

## Desktop UI

- launch opens the settings window and creates the tray icon
- minimize and close both hide the window to tray
- left-click the tray icon reopens the settings window
- right-click the tray icon opens a menu for `Open Settings`, `Show/Hide Overlay`, and `Quit`

## Build

Use Visual Studio Build Tools or Visual Studio with the C++ desktop workload installed.

From a Visual Studio Developer Command Prompt or Developer PowerShell:

```powershell
cmake -S . -B build -G "NMake Makefiles"
cmake --build build
```

The executable will be generated at:

`build/PotatoCrosshair.exe`

On first launch the app creates:

- `%LocalAppData%\PotatoCrosshair\settings.ini`
- `%LocalAppData%\PotatoCrosshair\assets\custom-reticle.png` when a PNG is imported

If you prefer a Visual Studio solution instead, use:

```powershell
cmake -S . -B build-vs -G "Visual Studio 17 2022" -A x64
cmake --build build-vs --config Release
```

## Notes

- The overlay still centers itself on the primary monitor.
- Most overlays only work reliably with windowed or borderless-fullscreen games. Exclusive fullscreen often bypasses the desktop compositor.
- Anti-cheat protected games can reject or behave unpredictably with overlays. Test carefully.

## Next steps

If you want to turn this into a fuller product, the next useful additions are:

- per-monitor targeting
- custom reticle editor and richer PNG controls
- profiles and per-game settings
- Direct2D/DirectComposition rendering for richer visuals

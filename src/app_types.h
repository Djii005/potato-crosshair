#pragma once

#include <windows.h>

#include <array>
#include <string>

namespace potato
{
enum class CrosshairStyle
{
    Cross = 0,
    Dot = 1,
    Circle = 2,
    TShape = 3,
};

enum class RenderMode
{
    BuiltIn = 0,
    CustomImage = 1,
};

enum class AppSection
{
    General = 0,
    Crosshair = 1,
    CustomImage = 2,
    Hotkeys = 3,
    About = 4,
};

struct ColorPreset
{
    BYTE red;
    BYTE green;
    BYTE blue;
};

struct Settings
{
    int length = 22;
    int gap = 7;
    int thickness = 3;
    int opacity = 220;
    int colorIndex = 0;
    CrosshairStyle style = CrosshairStyle::Cross;
    bool visible = true;
    RenderMode renderMode = RenderMode::BuiltIn;
    COLORREF customColor = RGB(0, 255, 0);
    bool useCustomColor = false;
    std::wstring customImagePath;
    int imageSize = 96;
};

inline constexpr wchar_t kAppName[] = L"Potato Crosshair";
inline constexpr wchar_t kAppWindowClass[] = L"PotatoCrosshairMainWindow";
inline constexpr wchar_t kOverlayWindowClass[] = L"PotatoCrosshairOverlayWindow";
inline constexpr wchar_t kPreviewWindowClass[] = L"PotatoCrosshairPreviewWindow";
inline constexpr wchar_t kIniSection[] = L"crosshair";

inline constexpr COLORREF kThemeWindowBackground = RGB(0x12, 0x12, 0x12);
inline constexpr COLORREF kThemePanelBackground = RGB(0x1E, 0x1E, 0x1E);
inline constexpr COLORREF kThemePanelSecondary = RGB(0x26, 0x26, 0x26);
inline constexpr COLORREF kThemeAccent = RGB(0xC9, 0x8A, 0x2E);
inline constexpr COLORREF kThemeAccentHover = RGB(0xE7, 0xBE, 0x6E);
inline constexpr COLORREF kThemeText = RGB(0xF5, 0xEC, 0xDA);
inline constexpr COLORREF kThemeMutedText = RGB(0xC8, 0xBA, 0xA1);
inline constexpr COLORREF kThemeBorder = RGB(0x35, 0x35, 0x35);
inline constexpr COLORREF kThemeDanger = RGB(0x8A, 0x38, 0x32);

inline constexpr std::array<ColorPreset, 6> kColorPresets = {{
    {0, 255, 0},
    {255, 64, 64},
    {255, 255, 255},
    {0, 220, 255},
    {255, 210, 0},
    {255, 80, 255},
}};

inline constexpr COLORREF MakeColorRef(const ColorPreset preset)
{
    return RGB(preset.red, preset.green, preset.blue);
}

inline int ClampInt(const int value, const int minimum, const int maximum)
{
    return value < minimum ? minimum : (value > maximum ? maximum : value);
}

inline int StyleToInt(const CrosshairStyle style)
{
    return static_cast<int>(style);
}

inline CrosshairStyle IntToStyle(int value)
{
    value = ClampInt(value, 0, static_cast<int>(CrosshairStyle::TShape));
    return static_cast<CrosshairStyle>(value);
}

inline int RenderModeToInt(const RenderMode mode)
{
    return static_cast<int>(mode);
}

inline RenderMode IntToRenderMode(int value)
{
    value = ClampInt(value, 0, static_cast<int>(RenderMode::CustomImage));
    return static_cast<RenderMode>(value);
}

inline COLORREF GetPresetColor(const int index)
{
    const auto& preset = kColorPresets[static_cast<size_t>(ClampInt(index, 0, static_cast<int>(kColorPresets.size()) - 1))];
    return MakeColorRef(preset);
}
} // namespace potato

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

inline constexpr int kCrosshairStyleCount = static_cast<int>(CrosshairStyle::TShape) + 1;

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

struct CrosshairProfile
{
    int length = 22;
    int gap = 7;
    int thickness = 3;
    int opacity = 220;
    int rotation = 0;
    int colorIndex = 0;
    COLORREF customColor = RGB(0, 255, 0);
    bool useCustomColor = false;
    bool middleDot = false;
    bool outlineEnabled = true;
};

inline CrosshairProfile MakeDefaultCrosshairProfile(const CrosshairStyle style)
{
    CrosshairProfile profile{};
    switch (style)
    {
    case CrosshairStyle::Dot:
        profile.length = 10;
        profile.gap = 0;
        profile.thickness = 4;
        break;
    case CrosshairStyle::Circle:
        profile.length = 16;
        profile.gap = 0;
        break;
    case CrosshairStyle::TShape:
        profile.length = 20;
        profile.gap = 6;
        break;
    case CrosshairStyle::Cross:
    default:
        break;
    }

    return profile;
}

struct Settings
{
    int length = 22;
    int gap = 7;
    int thickness = 3;
    int opacity = 220;
    int rotation = 0;
    int colorIndex = 0;
    CrosshairStyle style = CrosshairStyle::Cross;
    bool visible = true;
    bool middleDot = false;
    bool outlineEnabled = true;
    RenderMode renderMode = RenderMode::BuiltIn;
    COLORREF customColor = RGB(0, 255, 0);
    bool useCustomColor = false;
    std::wstring customImagePath;
    int imageSize = 96;
    std::array<CrosshairProfile, kCrosshairStyleCount> styleProfiles = {{
        MakeDefaultCrosshairProfile(CrosshairStyle::Cross),
        MakeDefaultCrosshairProfile(CrosshairStyle::Dot),
        MakeDefaultCrosshairProfile(CrosshairStyle::Circle),
        MakeDefaultCrosshairProfile(CrosshairStyle::TShape),
    }};
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
    value = ClampInt(value, 0, kCrosshairStyleCount - 1);
    return static_cast<CrosshairStyle>(value);
}

inline CrosshairProfile CaptureCrosshairProfile(const Settings& settings)
{
    CrosshairProfile profile{};
    profile.length = settings.length;
    profile.gap = settings.gap;
    profile.thickness = settings.thickness;
    profile.opacity = settings.opacity;
    profile.rotation = settings.rotation;
    profile.colorIndex = settings.colorIndex;
    profile.customColor = settings.customColor;
    profile.useCustomColor = settings.useCustomColor;
    profile.middleDot = settings.middleDot;
    profile.outlineEnabled = settings.outlineEnabled;
    return profile;
}

inline void ApplyCrosshairProfile(Settings& settings, const CrosshairProfile& profile)
{
    settings.length = profile.length;
    settings.gap = profile.gap;
    settings.thickness = profile.thickness;
    settings.opacity = profile.opacity;
    settings.rotation = profile.rotation;
    settings.colorIndex = profile.colorIndex;
    settings.customColor = profile.customColor;
    settings.useCustomColor = profile.useCustomColor;
    settings.middleDot = profile.middleDot;
    settings.outlineEnabled = profile.outlineEnabled;
}

inline CrosshairProfile& GetCrosshairProfile(Settings& settings, const CrosshairStyle style)
{
    return settings.styleProfiles[static_cast<size_t>(StyleToInt(style))];
}

inline const CrosshairProfile& GetCrosshairProfile(const Settings& settings, const CrosshairStyle style)
{
    return settings.styleProfiles[static_cast<size_t>(StyleToInt(style))];
}

inline void SyncCurrentStyleProfile(Settings& settings)
{
    GetCrosshairProfile(settings, settings.style) = CaptureCrosshairProfile(settings);
}

inline void LoadStyleProfile(Settings& settings, const CrosshairStyle style)
{
    settings.style = style;
    ApplyCrosshairProfile(settings, GetCrosshairProfile(settings, style));
}

inline void SwitchStyleProfile(Settings& settings, const CrosshairStyle style)
{
    SyncCurrentStyleProfile(settings);
    LoadStyleProfile(settings, style);
}

inline const wchar_t* StyleDisplayName(const CrosshairStyle style)
{
    switch (style)
    {
    case CrosshairStyle::Cross:
        return L"Cross";
    case CrosshairStyle::Dot:
        return L"Dot";
    case CrosshairStyle::Circle:
        return L"Circle";
    case CrosshairStyle::TShape:
    default:
        return L"T-Shape";
    }
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

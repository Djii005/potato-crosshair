#include "settings_store.h"

#include <shlobj.h>

#include <array>
#include <system_error>
#include <vector>

namespace potato
{
namespace
{
std::filesystem::path GetModuleDirectory(HINSTANCE instance)
{
    std::array<wchar_t, MAX_PATH> buffer{};
    GetModuleFileNameW(instance, buffer.data(), static_cast<DWORD>(buffer.size()));
    return std::filesystem::path(buffer.data()).parent_path();
}

std::wstring ReadStringValue(const std::filesystem::path& iniPath, const wchar_t* key, const wchar_t* defaultValue)
{
    std::vector<wchar_t> buffer(2048);
    GetPrivateProfileStringW(kIniSection, key, defaultValue, buffer.data(), static_cast<DWORD>(buffer.size()), iniPath.c_str());
    return std::wstring(buffer.data());
}

void WriteIntValue(const std::filesystem::path& iniPath, const wchar_t* key, const int value)
{
    const std::wstring text = std::to_wstring(value);
    WritePrivateProfileStringW(kIniSection, key, text.c_str(), iniPath.c_str());
}

void WriteStringValue(const std::filesystem::path& iniPath, const wchar_t* key, const std::wstring& value)
{
    WritePrivateProfileStringW(kIniSection, key, value.c_str(), iniPath.c_str());
}

bool FileExists(const std::filesystem::path& path)
{
    std::error_code errorCode;
    return std::filesystem::exists(path, errorCode);
}

std::wstring StyleProfileKey(const CrosshairStyle style, const wchar_t* suffix)
{
    return L"style_" + std::to_wstring(StyleToInt(style)) + L"_" + suffix;
}
} // namespace

bool SettingsStore::Initialize(const HINSTANCE instance)
{
    moduleDirectory_ = GetModuleDirectory(instance);

    PWSTR localAppDataPath = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &localAppDataPath)))
    {
        return false;
    }

    appDataDirectory_ = std::filesystem::path(localAppDataPath) / L"PotatoCrosshair";
    CoTaskMemFree(localAppDataPath);

    assetsDirectory_ = appDataDirectory_ / L"assets";
    settingsPath_ = appDataDirectory_ / L"settings.ini";
    importedImagePath_ = assetsDirectory_ / L"custom-reticle.png";

    EnsureDirectories();
    MigrateLegacySettingsIfNeeded();
    return true;
}

Settings SettingsStore::Load() const
{
    Settings settings{};

    settings.length = ClampInt(GetPrivateProfileIntW(kIniSection, L"length", settings.length, settingsPath_.c_str()), 4, 120);
    settings.gap = ClampInt(GetPrivateProfileIntW(kIniSection, L"gap", settings.gap, settingsPath_.c_str()), 0, 80);
    settings.thickness = ClampInt(GetPrivateProfileIntW(kIniSection, L"thickness", settings.thickness, settingsPath_.c_str()), 1, 20);
    settings.opacity = ClampInt(GetPrivateProfileIntW(kIniSection, L"opacity", settings.opacity, settingsPath_.c_str()), 40, 255);
    settings.rotation = ClampInt(GetPrivateProfileIntW(kIniSection, L"rotation", settings.rotation, settingsPath_.c_str()), 0, 359);
    settings.colorIndex = ClampInt(GetPrivateProfileIntW(kIniSection, L"color_index", settings.colorIndex, settingsPath_.c_str()), 0, static_cast<int>(kColorPresets.size()) - 1);
    settings.style = IntToStyle(GetPrivateProfileIntW(kIniSection, L"style", StyleToInt(settings.style), settingsPath_.c_str()));
    settings.visible = GetPrivateProfileIntW(kIniSection, L"visible", settings.visible ? 1 : 0, settingsPath_.c_str()) != 0;
    settings.middleDot = GetPrivateProfileIntW(kIniSection, L"middle_dot", settings.middleDot ? 1 : 0, settingsPath_.c_str()) != 0;
    settings.outlineEnabled = GetPrivateProfileIntW(kIniSection, L"outline_enabled", settings.outlineEnabled ? 1 : 0, settingsPath_.c_str()) != 0;
    settings.renderMode = IntToRenderMode(GetPrivateProfileIntW(kIniSection, L"render_mode", RenderModeToInt(settings.renderMode), settingsPath_.c_str()));
    settings.useCustomColor = GetPrivateProfileIntW(kIniSection, L"use_custom_color", settings.useCustomColor ? 1 : 0, settingsPath_.c_str()) != 0;

    const int red = ClampInt(GetPrivateProfileIntW(kIniSection, L"custom_red", GetRValue(settings.customColor), settingsPath_.c_str()), 0, 255);
    const int green = ClampInt(GetPrivateProfileIntW(kIniSection, L"custom_green", GetGValue(settings.customColor), settingsPath_.c_str()), 0, 255);
    const int blue = ClampInt(GetPrivateProfileIntW(kIniSection, L"custom_blue", GetBValue(settings.customColor), settingsPath_.c_str()), 0, 255);
    settings.customColor = RGB(red, green, blue);

    settings.customImagePath = ReadStringValue(settingsPath_, L"custom_image_path", L"");
    settings.imageSize = ClampInt(GetPrivateProfileIntW(kIniSection, L"image_size", settings.imageSize, settingsPath_.c_str()), 16, 512);

    for (int index = 0; index < kCrosshairStyleCount; ++index)
    {
        const CrosshairStyle style = IntToStyle(index);
        CrosshairProfile profile = GetCrosshairProfile(settings, style);
        if (style == settings.style)
        {
            profile = CaptureCrosshairProfile(settings);
        }

        profile.length = ClampInt(GetPrivateProfileIntW(kIniSection, StyleProfileKey(style, L"length").c_str(), profile.length, settingsPath_.c_str()), 4, 120);
        profile.gap = ClampInt(GetPrivateProfileIntW(kIniSection, StyleProfileKey(style, L"gap").c_str(), profile.gap, settingsPath_.c_str()), 0, 80);
        profile.thickness = ClampInt(GetPrivateProfileIntW(kIniSection, StyleProfileKey(style, L"thickness").c_str(), profile.thickness, settingsPath_.c_str()), 1, 20);
        profile.opacity = ClampInt(GetPrivateProfileIntW(kIniSection, StyleProfileKey(style, L"opacity").c_str(), profile.opacity, settingsPath_.c_str()), 40, 255);
        profile.rotation = ClampInt(GetPrivateProfileIntW(kIniSection, StyleProfileKey(style, L"rotation").c_str(), profile.rotation, settingsPath_.c_str()), 0, 359);
        profile.colorIndex = ClampInt(GetPrivateProfileIntW(kIniSection, StyleProfileKey(style, L"color_index").c_str(), profile.colorIndex, settingsPath_.c_str()), 0, static_cast<int>(kColorPresets.size()) - 1);
        profile.useCustomColor = GetPrivateProfileIntW(kIniSection, StyleProfileKey(style, L"use_custom_color").c_str(), profile.useCustomColor ? 1 : 0, settingsPath_.c_str()) != 0;
        profile.middleDot = GetPrivateProfileIntW(kIniSection, StyleProfileKey(style, L"middle_dot").c_str(), profile.middleDot ? 1 : 0, settingsPath_.c_str()) != 0;
        profile.outlineEnabled = GetPrivateProfileIntW(kIniSection, StyleProfileKey(style, L"outline_enabled").c_str(), profile.outlineEnabled ? 1 : 0, settingsPath_.c_str()) != 0;

        const int profileRed = ClampInt(GetPrivateProfileIntW(kIniSection, StyleProfileKey(style, L"custom_red").c_str(), GetRValue(profile.customColor), settingsPath_.c_str()), 0, 255);
        const int profileGreen = ClampInt(GetPrivateProfileIntW(kIniSection, StyleProfileKey(style, L"custom_green").c_str(), GetGValue(profile.customColor), settingsPath_.c_str()), 0, 255);
        const int profileBlue = ClampInt(GetPrivateProfileIntW(kIniSection, StyleProfileKey(style, L"custom_blue").c_str(), GetBValue(profile.customColor), settingsPath_.c_str()), 0, 255);
        profile.customColor = RGB(profileRed, profileGreen, profileBlue);

        GetCrosshairProfile(settings, style) = profile;
    }

    LoadStyleProfile(settings, settings.style);

    if (settings.customImagePath.empty() && FileExists(importedImagePath_))
    {
        settings.customImagePath = importedImagePath_.wstring();
    }

    if (settings.renderMode == RenderMode::CustomImage && settings.customImagePath.empty())
    {
        settings.renderMode = RenderMode::BuiltIn;
    }

    if (!settings.customImagePath.empty() && !FileExists(settings.customImagePath))
    {
        if (FileExists(importedImagePath_))
        {
            settings.customImagePath = importedImagePath_.wstring();
        }
        else
        {
            settings.customImagePath.clear();
            settings.renderMode = RenderMode::BuiltIn;
        }
    }

    return settings;
}

void SettingsStore::Save(const Settings& settings) const
{
    EnsureDirectories();

    Settings persisted = settings;
    SyncCurrentStyleProfile(persisted);

    WriteIntValue(settingsPath_, L"length", persisted.length);
    WriteIntValue(settingsPath_, L"gap", persisted.gap);
    WriteIntValue(settingsPath_, L"thickness", persisted.thickness);
    WriteIntValue(settingsPath_, L"opacity", persisted.opacity);
    WriteIntValue(settingsPath_, L"rotation", persisted.rotation);
    WriteIntValue(settingsPath_, L"color_index", persisted.colorIndex);
    WriteIntValue(settingsPath_, L"style", StyleToInt(persisted.style));
    WriteIntValue(settingsPath_, L"visible", persisted.visible ? 1 : 0);
    WriteIntValue(settingsPath_, L"middle_dot", persisted.middleDot ? 1 : 0);
    WriteIntValue(settingsPath_, L"outline_enabled", persisted.outlineEnabled ? 1 : 0);
    WriteIntValue(settingsPath_, L"render_mode", RenderModeToInt(persisted.renderMode));
    WriteIntValue(settingsPath_, L"use_custom_color", persisted.useCustomColor ? 1 : 0);
    WriteIntValue(settingsPath_, L"custom_red", GetRValue(persisted.customColor));
    WriteIntValue(settingsPath_, L"custom_green", GetGValue(persisted.customColor));
    WriteIntValue(settingsPath_, L"custom_blue", GetBValue(persisted.customColor));
    WriteStringValue(settingsPath_, L"custom_image_path", persisted.customImagePath);
    WriteIntValue(settingsPath_, L"image_size", persisted.imageSize);

    for (int index = 0; index < kCrosshairStyleCount; ++index)
    {
        const CrosshairStyle style = IntToStyle(index);
        const CrosshairProfile& profile = GetCrosshairProfile(persisted, style);
        WriteIntValue(settingsPath_, StyleProfileKey(style, L"length").c_str(), profile.length);
        WriteIntValue(settingsPath_, StyleProfileKey(style, L"gap").c_str(), profile.gap);
        WriteIntValue(settingsPath_, StyleProfileKey(style, L"thickness").c_str(), profile.thickness);
        WriteIntValue(settingsPath_, StyleProfileKey(style, L"opacity").c_str(), profile.opacity);
        WriteIntValue(settingsPath_, StyleProfileKey(style, L"rotation").c_str(), profile.rotation);
        WriteIntValue(settingsPath_, StyleProfileKey(style, L"color_index").c_str(), profile.colorIndex);
        WriteIntValue(settingsPath_, StyleProfileKey(style, L"use_custom_color").c_str(), profile.useCustomColor ? 1 : 0);
        WriteIntValue(settingsPath_, StyleProfileKey(style, L"middle_dot").c_str(), profile.middleDot ? 1 : 0);
        WriteIntValue(settingsPath_, StyleProfileKey(style, L"outline_enabled").c_str(), profile.outlineEnabled ? 1 : 0);
        WriteIntValue(settingsPath_, StyleProfileKey(style, L"custom_red").c_str(), GetRValue(profile.customColor));
        WriteIntValue(settingsPath_, StyleProfileKey(style, L"custom_green").c_str(), GetGValue(profile.customColor));
        WriteIntValue(settingsPath_, StyleProfileKey(style, L"custom_blue").c_str(), GetBValue(profile.customColor));
    }
}

std::filesystem::path SettingsStore::ImportCustomImage(const std::wstring& sourcePath) const
{
    EnsureDirectories();
    std::filesystem::copy_file(sourcePath, importedImagePath_, std::filesystem::copy_options::overwrite_existing);
    return importedImagePath_;
}

void SettingsStore::ClearCustomImage() const
{
    std::error_code errorCode;
    std::filesystem::remove(importedImagePath_, errorCode);
}

std::wstring SettingsStore::GetSettingsPath() const
{
    return settingsPath_.wstring();
}

std::wstring SettingsStore::GetAssetsDirectory() const
{
    return assetsDirectory_.wstring();
}

std::wstring SettingsStore::GetImportedImagePath() const
{
    return importedImagePath_.wstring();
}

void SettingsStore::EnsureDirectories() const
{
    std::error_code errorCode;
    std::filesystem::create_directories(assetsDirectory_, errorCode);
}

void SettingsStore::MigrateLegacySettingsIfNeeded() const
{
    if (FileExists(settingsPath_))
    {
        return;
    }

    const auto legacyIniPath = GetLegacyIniPath();
    if (!FileExists(legacyIniPath))
    {
        return;
    }

    std::error_code errorCode;
    std::filesystem::copy_file(legacyIniPath, settingsPath_, std::filesystem::copy_options::overwrite_existing, errorCode);
}

std::filesystem::path SettingsStore::GetLegacyIniPath() const
{
    return moduleDirectory_ / L"crosshair.ini";
}
} // namespace potato

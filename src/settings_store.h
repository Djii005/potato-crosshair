#pragma once

#include "app_types.h"

#include <filesystem>
#include <string>

namespace potato
{
class SettingsStore
{
public:
    bool Initialize(HINSTANCE instance);
    Settings Load() const;
    void Save(const Settings& settings) const;

    std::filesystem::path ImportCustomImage(const std::wstring& sourcePath) const;
    void ClearCustomImage() const;

    std::wstring GetSettingsPath() const;
    std::wstring GetAssetsDirectory() const;
    std::wstring GetImportedImagePath() const;

private:
    void EnsureDirectories() const;
    void MigrateLegacySettingsIfNeeded() const;
    std::filesystem::path GetLegacyIniPath() const;

    std::filesystem::path moduleDirectory_;
    std::filesystem::path appDataDirectory_;
    std::filesystem::path assetsDirectory_;
    std::filesystem::path settingsPath_;
    std::filesystem::path importedImagePath_;
};
} // namespace potato

#pragma once

#include "settings_model.h"

#include <windows.h>

#include <string>
#include <vector>

namespace refrain {

inline constexpr GUID kPreferencesPageGuid{
    0x0b88eb5f, 0x579d, 0x477c, {0x8b, 0x16, 0x22, 0x41, 0x34, 0x00, 0x25, 0xb8}};
inline constexpr GUID kSettingsCommandGuid{
    0xab4e6816, 0x2610, 0x4286, {0x86, 0x2e, 0xc6, 0xec, 0x68, 0x57, 0xbb, 0xc9}};
inline constexpr UINT kSettingsChangedMessage = WM_APP + 0x422;

struct DependencyStatus {
    std::wstring displayName;
    std::wstring version;
    bool detected{};
    std::wstring officialUrl;
};

[[nodiscard]] SettingsValues readSettings();
void writeSettings(const SettingsValues& values);
void registerSettingsWindow(HWND window);
void unregisterSettingsWindow(HWND window) noexcept;
void showRefrainPreferences() noexcept;
[[nodiscard]] std::vector<DependencyStatus> detectDependencies();

} // namespace refrain

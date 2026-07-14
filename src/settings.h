#pragma once

#include "settings_model.h"
#include "playlist_config_model.h"

#include <windows.h>

#include <string>
#include <vector>

namespace refrain {

inline constexpr GUID kPreferencesPageGuid{
    0x0b88eb5f, 0x579d, 0x477c, {0x8b, 0x16, 0x22, 0x41, 0x34, 0x00, 0x25, 0xb8}};
inline constexpr GUID kPlaylistPreferencesPageGuid{
    0x650df2ab, 0x8c92, 0x41bb, {0x9d, 0x37, 0x5c, 0x5c, 0xf0, 0xb9, 0x7d, 0xa3}};
inline constexpr GUID kPlaylistBrowserPreferencesPageGuid{
    0xa4c91363, 0x86da, 0x42af, {0x9b, 0x17, 0x37, 0x4f, 0x3a, 0x02, 0x66, 0x8c}};
inline constexpr GUID kSettingsCommandGuid{
    0xab4e6816, 0x2610, 0x4286, {0x86, 0x2e, 0xc6, 0xec, 0x68, 0x57, 0xbb, 0xc9}};
inline constexpr UINT kSettingsChangedMessage = WM_APP + 0x422;

struct DependencyStatus {
    std::wstring displayName;
    std::wstring version;
    bool detected{};
    std::wstring officialUrl;
};

struct AlbumBrowserSettings {
    bool mediaLibrary{};
    GUID playlistGuid{};
    GUID bridgeGuid{};
    int splitPermille{780};
    std::string albumKey;
};

struct PlaylistBrowserSettings {
    GUID defaultPlaylistGuid{};
    int splitPermille{150};
};

[[nodiscard]] SettingsValues readSettings();
void writeSettings(const SettingsValues& values);
[[nodiscard]] PlaylistViewSettings readPlaylistViewSettings();
void writePlaylistViewSettings(const PlaylistViewSettings& values);
[[nodiscard]] AlbumBrowserSettings readAlbumBrowserSettings();
void writeAlbumBrowserSettings(const AlbumBrowserSettings& values);
[[nodiscard]] PlaylistBrowserSettings readPlaylistBrowserSettings();
void writePlaylistBrowserSettings(const PlaylistBrowserSettings& values);
void notifySettingsChanged();
void registerSettingsWindow(HWND window);
void unregisterSettingsWindow(HWND window) noexcept;
void showRefrainPreferences() noexcept;
[[nodiscard]] std::vector<DependencyStatus> detectDependencies();

} // namespace refrain

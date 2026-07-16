#pragma once

#include "settings_model.h"
#include "playlist_config_model.h"

#include <windows.h>

#include <string>
#include <cstdint>
#include <vector>

namespace refrain {

inline constexpr GUID kPreferencesPageGuid{
    0x0b88eb5f, 0x579d, 0x477c, {0x8b, 0x16, 0x22, 0x41, 0x34, 0x00, 0x25, 0xb8}};
inline constexpr GUID kPlaylistPreferencesPageGuid{
    0x650df2ab, 0x8c92, 0x41bb, {0x9d, 0x37, 0x5c, 0x5c, 0xf0, 0xb9, 0x7d, 0xa3}};
inline constexpr GUID kNowPlayingPreferencesPageGuid{
    0x39e11789, 0xd101, 0x448d, {0xa6, 0xa9, 0xa5, 0x3e, 0x34, 0x3a, 0xbd, 0xb1}};
inline constexpr GUID kDependenciesPreferencesPageGuid{
    0xd512aa74, 0xc8d1, 0x4f44, {0xb6, 0x35, 0x89, 0xb0, 0x94, 0x75, 0xda, 0x03}};
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

struct PlaylistViewRestoreState {
    GUID playlistGuid{};
    std::string path;
    std::uint32_t subsong{};
    std::size_t playlistItem{static_cast<std::size_t>(-1)};
    std::string groupKey;
    std::size_t viewportAnchor{};
    double playbackPosition{};

    [[nodiscard]] bool valid() const noexcept { return !path.empty(); }
};

[[nodiscard]] SettingsValues readSettings();
[[nodiscard]] SettingsValues readEffectiveSettings();
void writeSettings(const SettingsValues& values);
void setSettingsPreview(HWND owner, const SettingsValues& values);
void clearSettingsPreview(HWND owner) noexcept;
[[nodiscard]] PlaylistViewSettings readPlaylistViewSettings();
void writePlaylistViewSettings(const PlaylistViewSettings& values);
[[nodiscard]] AlbumBrowserSettings readAlbumBrowserSettings();
void writeAlbumBrowserSettings(const AlbumBrowserSettings& values);
[[nodiscard]] PlaylistBrowserSettings readPlaylistBrowserSettings();
void writePlaylistBrowserSettings(const PlaylistBrowserSettings& values);
[[nodiscard]] PlaylistViewRestoreState readPlaylistViewRestoreState();
void writePlaylistViewRestoreState(const PlaylistViewRestoreState& value);
void notifySettingsChanged();
void registerSettingsWindow(HWND window);
void unregisterSettingsWindow(HWND window) noexcept;
void showRefrainPreferences() noexcept;
[[nodiscard]] std::vector<DependencyStatus> detectDependencies();

} // namespace refrain

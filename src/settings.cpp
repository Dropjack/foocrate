#include "settings.h"
#include "playlist_browser_model.h"

#include <mmsystem.h>
#include <objbase.h>
#include <ole2.h>
#include <foobar2000/SDK/foobar2000.h>

#include <algorithm>
#include <array>
#include <cwctype>
#include <mutex>
#include <limits>
#include <optional>

namespace foocrate {
namespace {

constexpr GUID kConfigVersionGuid{
    0x6b7e5304, 0xcd2a, 0x4d7e, {0xa2, 0x28, 0xd0, 0x66, 0xef, 0x9d, 0x01, 0x90}};
constexpr GUID kTimeDisplayGuid{
    0xf2ae2fe9, 0xa2cc, 0x4960, {0x9f, 0x93, 0xbc, 0xee, 0x16, 0x24, 0x9d, 0x29}};
constexpr GUID kShowTooltipsGuid{
    0x39608393, 0xf962, 0x46b0, {0x86, 0xbf, 0x8c, 0xe9, 0x2e, 0x46, 0xbc, 0xd1}};
constexpr GUID kShowSettingsButtonGuid{
    0x70e58186, 0xa678, 0x4b86, {0xa5, 0x70, 0x1e, 0x7f, 0x2f, 0x58, 0x39, 0x84}};
constexpr GUID kLyricsAutoSwitchGuid{
    0xeaf337c3, 0x7b42, 0x40ba, {0xb8, 0x49, 0x95, 0x31, 0xb6, 0xe8, 0xf5, 0xc1}};
constexpr GUID kLowerRightViewGuid{
    0x477edb2d, 0x0155, 0x43bc, {0xb0, 0x57, 0x45, 0x98, 0xd7, 0x21, 0xc3, 0x54}};
constexpr GUID kShowReplayGainGuid{
    0x26b186af, 0x9781, 0x45c0, {0x93, 0x91, 0x95, 0x32, 0xa4, 0x0c, 0x86, 0x1d}};
constexpr GUID kRightHeaderPermilleGuid{
    0x6434bf69, 0x3e63, 0x4f17, {0x86, 0x44, 0xa1, 0xd1, 0xe3, 0x20, 0xf1, 0x4c}};
constexpr GUID kRightColumnPermilleGuid{
    0x522cd19f, 0x70dc, 0x4b8b, {0x9b, 0x65, 0x28, 0xe2, 0xd2, 0x3d, 0xf7, 0xa1}};
constexpr GUID kThemePresetGuid{
    0xb2c4668f, 0x124c, 0x4f8b, {0xa0, 0x28, 0x6a, 0xea, 0xbb, 0xc3, 0xde, 0xe7}};
constexpr GUID kColourModeGuid{
    0xac92cbe4, 0xd155, 0x486e, {0x8b, 0x7f, 0xd6, 0x1d, 0x7c, 0xa1, 0x65, 0xc5}};
constexpr GUID kTrackActivationGuid{0x52c7b43b, 0xf0cd, 0x4660, {0x92, 0x70, 0x29, 0xd6, 0xd2, 0xac, 0x7f, 0xf7}};
constexpr GUID kRightPanelFollowGuid{0xc380723d, 0xd36c, 0x4cca, {0x85, 0xe7, 0x7d, 0xb0, 0xd1, 0xef, 0x31, 0x93}};
constexpr GUID kDetailsMetadataGuid{0x36c68ed2, 0x4d94, 0x4315, {0x93, 0xea, 0xea, 0x0d, 0x38, 0xd0, 0x17, 0x1a}};
constexpr GUID kDetailsLocationGuid{0x8f6effc0, 0x0c6c, 0x4a29, {0xa2, 0x90, 0x4c, 0x29, 0x41, 0x90, 0xa2, 0x31}};
constexpr GUID kDetailsTechnicalGuid{0x80f59694, 0xaf9d, 0x44c1, {0xb2, 0xbc, 0xd7, 0x17, 0xb4, 0x5d, 0x21, 0xf3}};
constexpr GUID kDetailsStatisticsGuid{0xa2cd8fb4, 0x486b, 0x4c5b, {0x9e, 0x58, 0xa9, 0x74, 0xd5, 0x1d, 0x81, 0x4a}};
constexpr GUID kArtworkRotationEnabledGuid{0xdfcf21fa, 0xcdce, 0x4f8d, {0xa3, 0x3d, 0x70, 0xe5, 0x8f, 0x89, 0xb9, 0x56}};
constexpr GUID kArtworkSourcesGuid{0xbb6b7810, 0xda38, 0x4c68, {0xa0, 0x64, 0x60, 0x50, 0x7d, 0x5c, 0x1a, 0x06}};
constexpr GUID kArtworkIntervalGuid{0x659edc79, 0x3dc1, 0x446d, {0xb0, 0xd5, 0x9b, 0x61, 0x1d, 0x8f, 0xe8, 0xd7}};
constexpr GUID kAlbumTileSizeGuid{0x14da9817, 0xc551, 0x41ca, {0xbc, 0x2c, 0x8d, 0xf0, 0x08, 0x1a, 0xe9, 0xca}};
constexpr GUID kNowPlayingSummaryGuid{0x82d6553c, 0x1cdf, 0x4899, {0xa9, 0xf1, 0x78, 0x87, 0x2a, 0xbd, 0xcb, 0x79}};
constexpr GUID kPlaylistViewSettingsGuid{
    0xd8a6f36a, 0x474c, 0x4fd6, {0x89, 0x3f, 0x08, 0x8e, 0xcc, 0x83, 0xa4, 0xa2}};
constexpr GUID kAlbumSourceKindGuid{
    0xdbb3c92f, 0xbd40, 0x4e5c, {0xa4, 0xed, 0xa8, 0x8d, 0xe1, 0x79, 0xa7, 0xe4}};
constexpr GUID kAlbumSourcePlaylistGuid{
    0x3e1727b4, 0xefca, 0x4e7c, {0xa3, 0x04, 0x57, 0xd9, 0x23, 0xb6, 0xba, 0x37}};
constexpr GUID kAlbumSplitPermilleGuid{
    0xc7df192c, 0xad7e, 0x45bb, {0xa0, 0x3b, 0xc4, 0xe5, 0x9c, 0xfe, 0x56, 0x43}};
constexpr GUID kAlbumBridgePlaylistGuid{
    0xed20fbc8, 0xbce0, 0x49d8, {0xa6, 0xd4, 0xe6, 0xf6, 0xe1, 0x3a, 0x4e, 0xfb}};
constexpr GUID kAlbumSelectionKeyGuid{
    0x69b9b165, 0xc9e9, 0x42aa, {0x80, 0xc8, 0xb9, 0x77, 0xbe, 0x2e, 0x02, 0x14}};
constexpr GUID kDefaultPlaylistGuid{
    0x09a54027, 0x3884, 0x45c1, {0x89, 0x8c, 0xd5, 0xa9, 0x84, 0x94, 0x43, 0xb2}};
constexpr GUID kPlaylistBrowserSplitGuid{
    0x83f11fdb, 0xc4d9, 0x46b8, {0x95, 0x20, 0x30, 0x99, 0xf4, 0x3d, 0x0e, 0x1a}};
constexpr GUID kRestorePlaylistViewGuid{
    0xa49d0135, 0x87c8, 0x4d45, {0x84, 0xf5, 0xb9, 0x11, 0x13, 0x89, 0x78, 0xd1}};
constexpr GUID kRestorePlaylistGuidGuid{
    0xcb6655d7, 0x9769, 0x438f, {0x81, 0xd2, 0x16, 0xb4, 0xcc, 0x7a, 0xb8, 0x33}};
constexpr GUID kRestorePathGuid{
    0x45e2a9ab, 0xe529, 0x41e6, {0xa4, 0x3c, 0xd6, 0x38, 0x22, 0x7d, 0x4a, 0x90}};
constexpr GUID kRestoreSubsongGuid{
    0xd652c194, 0x3858, 0x40ca, {0x8a, 0x90, 0x44, 0x79, 0x9d, 0x7b, 0x71, 0x4f}};
constexpr GUID kRestoreGroupKeyGuid{
    0x81495cb8, 0xf5f5, 0x4034, {0x86, 0xc4, 0x3a, 0x91, 0x71, 0x26, 0x13, 0xe2}};
constexpr GUID kRestoreAnchorGuid{
    0x2b4434b2, 0x8d22, 0x4781, {0x8b, 0xc4, 0x23, 0x76, 0x82, 0x3f, 0x1f, 0x1c}};
constexpr GUID kStartupBehaviorGuid{
    0xc80bc715, 0xdfae, 0x4e3f, {0xa0, 0xa9, 0x83, 0x6a, 0xd8, 0xf5, 0xbb, 0x75}};
constexpr GUID kRestorePlaybackPositionGuid{
    0x30b18575, 0x04f1, 0x4d65, {0x93, 0x7a, 0x6f, 0xc0, 0x2f, 0x0a, 0x2d, 0x5e}};
constexpr GUID kRestorePlaylistItemGuid{
    0x9ee27c80, 0xb544, 0x44c5, {0xb0, 0xa7, 0xc0, 0xc4, 0xce, 0x46, 0x04, 0xa0}};
constexpr GUID kRestoreStateVersionGuid{
    0xc71e50bc, 0x32dc, 0x4bd9, {0xb6, 0x9e, 0x7a, 0xe5, 0x9c, 0xec, 0x5a, 0xc9}};
constexpr t_int32 kCurrentRestoreStateVersion = 1;

cfg_int g_configVersion(kConfigVersionGuid, 0);
cfg_int g_timeDisplay(kTimeDisplayGuid, static_cast<std::int64_t>(TimeDisplayMode::total));
cfg_bool g_showTooltips(kShowTooltipsGuid, true);
cfg_bool g_showSettingsButton(kShowSettingsButtonGuid, true);
cfg_bool g_lyricsAutoSwitch(kLyricsAutoSwitchGuid, true);
cfg_int g_lowerRightView(kLowerRightViewGuid, static_cast<std::int64_t>(LowerRightView::lyrics));
cfg_bool g_showReplayGain(kShowReplayGainGuid, false);
cfg_int g_rightHeaderPermille(kRightHeaderPermilleGuid, 500);
cfg_int g_rightColumnPermille(kRightColumnPermilleGuid, 230);
cfg_int g_themePreset(kThemePresetGuid, static_cast<std::int64_t>(ThemePreset::mist));
// Zero is the version-4 FooCrate value. First-run version-0 migration rewrites it to
// the version-5 FooCrate preset value without ever selecting Windows accidentally.
cfg_int g_colourMode(kColourModeGuid, 0);
cfg_int g_trackActivation(kTrackActivationGuid, 0);
cfg_int g_rightPanelFollow(kRightPanelFollowGuid, 0);
cfg_bool g_detailsMetadata(kDetailsMetadataGuid, true);
cfg_bool g_detailsLocation(kDetailsLocationGuid, true);
cfg_bool g_detailsTechnical(kDetailsTechnicalGuid, true);
cfg_bool g_detailsStatistics(kDetailsStatisticsGuid, true);
cfg_bool g_artworkRotationEnabled(kArtworkRotationEnabledGuid, true);
cfg_int g_artworkSources(kArtworkSourcesGuid, kAllArtworkSources);
cfg_int g_artworkInterval(kArtworkIntervalGuid, 20);
cfg_int g_albumTileSize(kAlbumTileSizeGuid, static_cast<std::int64_t>(AlbumTileSize::medium));
cfg_string g_nowPlayingSummary(kNowPlayingSummaryGuid,
    "$if2(%title%,$filename_ext(%path%))\n$if2(%artist%,'Unknown artist') | $if2(%album%,'Unknown album')");
cfg_string g_playlistViewSettings(kPlaylistViewSettingsGuid, "");
cfg_bool g_albumSourceLibrary(kAlbumSourceKindGuid, false);
cfg_guid g_albumSourcePlaylist(kAlbumSourcePlaylistGuid, GUID{});
cfg_int g_albumSplitPermille(kAlbumSplitPermilleGuid, 780);
cfg_guid g_albumBridgePlaylist(kAlbumBridgePlaylistGuid, GUID{});
cfg_string g_albumSelectionKey(kAlbumSelectionKeyGuid, "");
cfg_guid g_defaultPlaylist(kDefaultPlaylistGuid, GUID{});
cfg_int g_playlistBrowserSplit(kPlaylistBrowserSplitGuid, 150);
cfg_bool g_restorePlaylistView(kRestorePlaylistViewGuid, true);
cfg_int g_startupBehavior(kStartupBehaviorGuid, static_cast<t_int32>(StartupBehavior::startAtHome));
cfg_guid g_restorePlaylistGuid(kRestorePlaylistGuidGuid, GUID{});
cfg_string g_restorePath(kRestorePathGuid, "");
cfg_int g_restoreSubsong(kRestoreSubsongGuid, 0);
cfg_string g_restoreGroupKey(kRestoreGroupKeyGuid, "");
cfg_int g_restoreAnchor(kRestoreAnchorGuid, 0);
cfg_float g_restorePlaybackPosition(kRestorePlaybackPositionGuid, 0.0);
cfg_int g_restorePlaylistItem(kRestorePlaylistItemGuid, -1);
cfg_int g_restoreStateVersion(kRestoreStateVersionGuid, 0);

std::mutex g_windowsMutex;
std::vector<HWND> g_settingsWindows;
std::mutex g_previewMutex;
std::optional<std::pair<HWND, SettingsValues>> g_settingsPreview;

[[nodiscard]] std::wstring utf8ToWide(const char* text) {
    if (text == nullptr || *text == '\0') {
        return {};
    }
    const auto length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, nullptr, 0);
    if (length <= 1) {
        return {};
    }
    std::wstring result(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, result.data(), length);
    result.pop_back();
    return result;
}

[[nodiscard]] std::wstring lower(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](wchar_t character) { return static_cast<wchar_t>(std::towlower(character)); });
    return value;
}

void notifySettingsWindows() {
    std::scoped_lock lock(g_windowsMutex);
    std::erase_if(g_settingsWindows, [](HWND window) { return !IsWindow(window); });
    for (const auto window : g_settingsWindows) {
        PostMessage(window, kSettingsChangedMessage, 0, 0);
    }
}

} // namespace

SettingsValues readSettings() {
    StoredSettings stored{g_configVersion.get(), g_timeDisplay.get(), g_showTooltips.get(),
        g_showSettingsButton.get(), g_lyricsAutoSwitch.get(), g_lowerRightView.get(),
        g_showReplayGain.get(), g_rightHeaderPermille.get(), g_rightColumnPermille.get(),
        g_themePreset.get(), g_colourMode.get()};
    stored.trackActivation = g_trackActivation.get();
    stored.restorePlaylistView = g_restorePlaylistView.get();
    stored.startupBehavior = g_startupBehavior.get();
    stored.rightPanelFollow = g_rightPanelFollow.get();
    stored.detailsMetadata = g_detailsMetadata.get();
    stored.detailsLocation = g_detailsLocation.get();
    stored.detailsTechnical = g_detailsTechnical.get();
    stored.detailsPlaybackStatistics = g_detailsStatistics.get();
    stored.detailsReplayGain = g_showReplayGain.get();
    stored.artworkRotationEnabled = g_artworkRotationEnabled.get();
    stored.artworkSourceMask = g_artworkSources.get();
    stored.artworkRotationSeconds = g_artworkInterval.get();
    stored.albumTileSize = g_albumTileSize.get();
    stored.nowPlayingSummaryFormat = g_nowPlayingSummary.get().c_str();
    const auto migration = migrateSettings(stored);
    if (migration.rewriteKnownValues) {
        g_timeDisplay = static_cast<t_int32>(migration.values.timeDisplay);
        g_showTooltips = migration.values.showTooltips;
        g_showSettingsButton = migration.values.showSettingsButton;
        g_lyricsAutoSwitch = migration.values.lyricsAutoSwitch;
        g_lowerRightView = static_cast<t_int32>(migration.values.lowerRightView);
        g_showReplayGain = migration.values.showReplayGain;
        g_rightHeaderPermille = static_cast<t_int32>(migration.values.rightHeaderPermille);
        g_rightColumnPermille = static_cast<t_int32>(migration.values.rightColumnPermille);
        g_themePreset = static_cast<t_int32>(migration.values.themePreset);
        g_colourMode = static_cast<t_int32>(migration.values.colourMode);
        g_trackActivation = static_cast<t_int32>(migration.values.trackActivation);
        g_restorePlaylistView = migration.values.startupBehavior != StartupBehavior::startAtHome;
        g_startupBehavior = static_cast<t_int32>(migration.values.startupBehavior);
        g_rightPanelFollow = static_cast<t_int32>(migration.values.rightPanelFollow);
        g_detailsMetadata = migration.values.detailsMetadata;
        g_detailsLocation = migration.values.detailsLocation;
        g_detailsTechnical = migration.values.detailsTechnical;
        g_detailsStatistics = migration.values.detailsPlaybackStatistics;
        g_showReplayGain = migration.values.detailsReplayGain;
        g_artworkRotationEnabled = migration.values.artworkRotationEnabled;
        g_artworkSources = static_cast<t_int32>(migration.values.artworkSourceMask);
        g_artworkInterval = static_cast<t_int32>(migration.values.artworkRotationSeconds);
        g_albumTileSize = static_cast<t_int32>(migration.values.albumTileSize);
        g_nowPlayingSummary.set(migration.values.nowPlayingSummaryFormat.c_str());
        g_configVersion = static_cast<t_int32>(migration.versionToKeep);
    }
    return migration.values;
}

SettingsValues readEffectiveSettings() {
    {
        std::scoped_lock lock(g_previewMutex);
        if (g_settingsPreview && IsWindow(g_settingsPreview->first)) return g_settingsPreview->second;
        if (g_settingsPreview) g_settingsPreview.reset();
    }
    return readSettings();
}

void writeSettings(const SettingsValues& values) {
    StoredSettings stored{kCurrentSettingsVersion,
        static_cast<std::int64_t>(values.timeDisplay), values.showTooltips, values.showSettingsButton,
        values.lyricsAutoSwitch, static_cast<std::int64_t>(values.lowerRightView),
        values.showReplayGain, values.rightHeaderPermille, values.rightColumnPermille,
        static_cast<std::int64_t>(values.themePreset), static_cast<std::int64_t>(values.colourMode)};
    stored.trackActivation = static_cast<std::int64_t>(values.trackActivation);
    stored.restorePlaylistView = values.startupBehavior != StartupBehavior::startAtHome;
    stored.startupBehavior = static_cast<std::int64_t>(values.startupBehavior);
    stored.rightPanelFollow = static_cast<std::int64_t>(values.rightPanelFollow);
    stored.detailsMetadata = values.detailsMetadata;
    stored.detailsLocation = values.detailsLocation;
    stored.detailsTechnical = values.detailsTechnical;
    stored.detailsPlaybackStatistics = values.detailsPlaybackStatistics;
    stored.detailsReplayGain = values.detailsReplayGain;
    stored.artworkRotationEnabled = values.artworkRotationEnabled;
    stored.artworkSourceMask = values.artworkSourceMask;
    stored.artworkRotationSeconds = values.artworkRotationSeconds;
    stored.albumTileSize = static_cast<std::int64_t>(values.albumTileSize);
    stored.nowPlayingSummaryFormat = values.nowPlayingSummaryFormat;
    const auto normalized = migrateSettings(std::move(stored));
    g_timeDisplay = static_cast<t_int32>(normalized.values.timeDisplay);
    g_showTooltips = normalized.values.showTooltips;
    g_showSettingsButton = normalized.values.showSettingsButton;
    g_lyricsAutoSwitch = normalized.values.lyricsAutoSwitch;
    g_lowerRightView = static_cast<t_int32>(normalized.values.lowerRightView);
    g_showReplayGain = normalized.values.showReplayGain;
    g_rightHeaderPermille = static_cast<t_int32>(normalized.values.rightHeaderPermille);
    g_rightColumnPermille = static_cast<t_int32>(normalized.values.rightColumnPermille);
    g_themePreset = static_cast<t_int32>(normalized.values.themePreset);
    g_colourMode = static_cast<t_int32>(normalized.values.colourMode);
    g_trackActivation = static_cast<t_int32>(normalized.values.trackActivation);
    g_restorePlaylistView = normalized.values.startupBehavior != StartupBehavior::startAtHome;
    g_startupBehavior = static_cast<t_int32>(normalized.values.startupBehavior);
    g_rightPanelFollow = static_cast<t_int32>(normalized.values.rightPanelFollow);
    g_detailsMetadata = normalized.values.detailsMetadata;
    g_detailsLocation = normalized.values.detailsLocation;
    g_detailsTechnical = normalized.values.detailsTechnical;
    g_detailsStatistics = normalized.values.detailsPlaybackStatistics;
    g_showReplayGain = normalized.values.detailsReplayGain;
    g_artworkRotationEnabled = normalized.values.artworkRotationEnabled;
    g_artworkSources = static_cast<t_int32>(normalized.values.artworkSourceMask);
    g_artworkInterval = static_cast<t_int32>(normalized.values.artworkRotationSeconds);
    g_albumTileSize = static_cast<t_int32>(normalized.values.albumTileSize);
    g_nowPlayingSummary.set(normalized.values.nowPlayingSummaryFormat.c_str());
    g_configVersion = static_cast<t_int32>(std::max<std::int64_t>(g_configVersion.get(), kCurrentSettingsVersion));
    notifySettingsWindows();
}

void setSettingsPreview(HWND owner, const SettingsValues& values) {
    if (!owner || !IsWindow(owner)) return;
    auto normalized = values;
    normalized.artworkSourceMask = isValidArtworkSourceMask(values.artworkSourceMask)
        ? values.artworkSourceMask : kAllArtworkSources;
    normalized.artworkRotationSeconds = isValidArtworkRotationSeconds(values.artworkRotationSeconds)
        ? values.artworkRotationSeconds : 20;
    if (normalized.nowPlayingSummaryFormat.empty()) normalized.nowPlayingSummaryFormat =
        "$if2(%title%,$filename_ext(%path%))\n$if2(%artist%,'Unknown artist') | $if2(%album%,'Unknown album')";
    {
        std::scoped_lock lock(g_previewMutex);
        g_settingsPreview = std::pair{owner, normalized};
    }
    notifySettingsWindows();
}

void clearSettingsPreview(HWND owner) noexcept {
    bool changed{};
    {
        std::scoped_lock lock(g_previewMutex);
        if (g_settingsPreview && g_settingsPreview->first == owner) {
            g_settingsPreview.reset();
            changed = true;
        }
    }
    if (changed) notifySettingsWindows();
}

PlaylistViewSettings readPlaylistViewSettings() {
    const auto stored = g_playlistViewSettings.get();
    if (stored.is_empty()) return defaultPlaylistViewSettings();
    auto value = deserializePlaylistViewSettings(stored.c_str());
    auto compiler = titleformat_compiler::get();
    auto valid = [&](const std::string& text, bool allowEmpty) {
        titleformat_object::ptr script;
        return (allowEmpty && text.empty()) || compiler->compile(script, text.c_str());
    };
    bool repaired{};
    for (auto& group : value.groups) {
        if (!valid(group.keyFormat, false)) { group.keyFormat = "$if2(%album%,'Single')"; repaired = true; }
        if (!valid(group.sortFormat, false)) { group.sortFormat = "%album% | %discnumber% | %tracknumber% | %title%"; repaired = true; }
        for (auto* field : {&group.leftPrimary, &group.rightPrimary, &group.leftSecondary, &group.rightSecondary}) {
            if (!valid(*field, true)) { field->clear(); repaired = true; }
        }
    }
    for (auto& column : value.columns) {
        if (!column.cover && !valid(column.displayFormat, false)) {
            column.displayFormat = "$if2(%title%,$filename_ext(%path%))"; repaired = true;
        }
        if (!valid(column.sortFormat, true)) { column.sortFormat.clear(); repaired = true; }
    }
    if (repaired) {
        const auto serialized = serializePlaylistViewSettings(value);
        g_playlistViewSettings.set(serialized.c_str());
    }
    return value;
}

void writePlaylistViewSettings(const PlaylistViewSettings& values) {
    const auto serialized = serializePlaylistViewSettings(values);
    g_playlistViewSettings.set(serialized.c_str());
    notifySettingsWindows();
}

AlbumBrowserSettings readAlbumBrowserSettings() {
    return {g_albumSourceLibrary.get(), g_albumSourcePlaylist.get(), g_albumBridgePlaylist.get(),
        std::clamp(static_cast<int>(g_albumSplitPermille.get()), 350, 800), g_albumSelectionKey.get().c_str()};
}

void writeAlbumBrowserSettings(const AlbumBrowserSettings& values) {
    g_albumSourceLibrary = values.mediaLibrary;
    g_albumSourcePlaylist = values.playlistGuid;
    g_albumBridgePlaylist = values.bridgeGuid;
    g_albumSplitPermille = std::clamp(values.splitPermille, 350, 800);
    g_albumSelectionKey.set(values.albumKey.c_str());
}

PlaylistBrowserSettings readPlaylistBrowserSettings() {
    return {g_defaultPlaylist.get(),
        normalizePlaylistBrowserSplitPermille(static_cast<int>(g_playlistBrowserSplit.get()))};
}

void writePlaylistBrowserSettings(const PlaylistBrowserSettings& values) {
    g_defaultPlaylist = values.defaultPlaylistGuid;
    g_playlistBrowserSplit = normalizePlaylistBrowserSplitPermille(values.splitPermille);
    notifySettingsWindows();
}

PlaylistViewRestoreState readPlaylistViewRestoreState() {
    PlaylistViewRestoreState value;
    if (g_restoreStateVersion.get() != kCurrentRestoreStateVersion) return value;
    value.playlistGuid = g_restorePlaylistGuid.get();
    value.path = g_restorePath.get().c_str();
    value.subsong = static_cast<std::uint32_t>(std::max<t_int32>(0, g_restoreSubsong.get()));
    value.playlistItem = g_restorePlaylistItem.get() < 0
        ? static_cast<std::size_t>(-1)
        : static_cast<std::size_t>(g_restorePlaylistItem.get());
    value.groupKey = g_restoreGroupKey.get().c_str();
    value.viewportAnchor = static_cast<std::size_t>(std::max<t_int32>(0, g_restoreAnchor.get()));
    value.playbackPosition = std::max(0.0, static_cast<double>(g_restorePlaybackPosition.get()));
    return value;
}

void writePlaylistViewRestoreState(const PlaylistViewRestoreState& value) {
    g_restorePlaylistGuid = value.playlistGuid;
    g_restorePath.set(value.path.c_str());
    g_restoreSubsong = static_cast<t_int32>(std::min<std::uint32_t>(value.subsong,
        static_cast<std::uint32_t>(std::numeric_limits<t_int32>::max())));
    g_restorePlaylistItem = value.playlistItem == static_cast<std::size_t>(-1)
        ? -1
        : static_cast<t_int32>(std::min<std::size_t>(value.playlistItem,
            static_cast<std::size_t>(std::numeric_limits<t_int32>::max())));
    g_restoreGroupKey.set(value.groupKey.c_str());
    g_restoreAnchor = static_cast<t_int32>(std::min<std::size_t>(value.viewportAnchor,
        static_cast<std::size_t>(std::numeric_limits<t_int32>::max())));
    g_restorePlaybackPosition = static_cast<float>(std::max(0.0, value.playbackPosition));
    g_restoreStateVersion = kCurrentRestoreStateVersion;
}

void notifySettingsChanged() {
    notifySettingsWindows();
}

void registerSettingsWindow(HWND window) {
    if (!window) {
        return;
    }
    std::scoped_lock lock(g_windowsMutex);
    if (std::find(g_settingsWindows.begin(), g_settingsWindows.end(), window) == g_settingsWindows.end()) {
        g_settingsWindows.push_back(window);
    }
}

void unregisterSettingsWindow(HWND window) noexcept {
    std::scoped_lock lock(g_windowsMutex);
    std::erase(g_settingsWindows, window);
}

void showFooCratePreferences() noexcept {
    try {
        ui_control::get()->show_preferences(kPreferencesPageGuid);
    } catch (...) {
    }
}

std::vector<DependencyStatus> detectDependencies() {
    struct Target {
        const wchar_t* displayName;
        const wchar_t* match;
        const wchar_t* officialUrl;
    };
    constexpr std::array targets{
        Target{L"Columns UI", L"columns ui", L"https://www.foobar2000.org/components/view/foo_ui_columns"},
        Target{L"ESLyric", L"eslyric", L"https://github.com/ESLyric/release/releases/latest"},
        Target{L"Playback Statistics", L"playback statistics", L"https://www.foobar2000.org/components/view/foo_playcount"},
    };

    std::vector<DependencyStatus> result;
    result.reserve(targets.size());
    for (const auto& target : targets) {
        result.push_back({target.displayName, {}, false, target.officialUrl});
    }

    for (auto component : componentversion::enumerate()) {
        pfc::string8 name;
        pfc::string8 version;
        component->get_component_name(name);
        component->get_component_version(version);
        const auto normalizedName = lower(utf8ToWide(name.c_str()));
        for (std::size_t index = 0; index < targets.size(); ++index) {
            if (!result[index].detected && normalizedName.find(targets[index].match) != std::wstring::npos) {
                result[index].detected = true;
                result[index].version = utf8ToWide(version.c_str());
            }
        }
    }
    return result;
}

} // namespace foocrate

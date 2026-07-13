#include "settings.h"

#include <mmsystem.h>
#include <objbase.h>
#include <ole2.h>
#include <foobar2000/SDK/foobar2000.h>

#include <algorithm>
#include <array>
#include <cwctype>
#include <mutex>

namespace refrain {
namespace {

constexpr GUID kConfigVersionGuid{
    0x6b7e5304, 0xcd2a, 0x4d7e, {0xa2, 0x28, 0xd0, 0x66, 0xef, 0x9d, 0x01, 0x90}};
constexpr GUID kTimeDisplayGuid{
    0xf2ae2fe9, 0xa2cc, 0x4960, {0x9f, 0x93, 0xbc, 0xee, 0x16, 0x24, 0x9d, 0x29}};
constexpr GUID kShowTooltipsGuid{
    0x39608393, 0xf962, 0x46b0, {0x86, 0xbf, 0x8c, 0xe9, 0x2e, 0x46, 0xbc, 0xd1}};
constexpr GUID kShowSettingsButtonGuid{
    0x70e58186, 0xa678, 0x4b86, {0xa5, 0x70, 0x1e, 0x7f, 0x2f, 0x58, 0x39, 0x84}};

cfg_int g_configVersion(kConfigVersionGuid, 0);
cfg_int g_timeDisplay(kTimeDisplayGuid, static_cast<std::int64_t>(TimeDisplayMode::total));
cfg_bool g_showTooltips(kShowTooltipsGuid, true);
cfg_bool g_showSettingsButton(kShowSettingsButtonGuid, true);

std::mutex g_windowsMutex;
std::vector<HWND> g_settingsWindows;

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
    const StoredSettings stored{
        g_configVersion.get(), g_timeDisplay.get(), g_showTooltips.get(), g_showSettingsButton.get()};
    const auto migration = migrateSettings(stored);
    if (migration.rewriteKnownValues) {
        g_timeDisplay = static_cast<t_int32>(migration.values.timeDisplay);
        g_showTooltips = migration.values.showTooltips;
        g_showSettingsButton = migration.values.showSettingsButton;
        g_configVersion = static_cast<t_int32>(migration.versionToKeep);
    }
    return migration.values;
}

void writeSettings(const SettingsValues& values) {
    const auto normalized = migrateSettings({kCurrentSettingsVersion,
        static_cast<std::int64_t>(values.timeDisplay), values.showTooltips, values.showSettingsButton});
    g_timeDisplay = static_cast<t_int32>(normalized.values.timeDisplay);
    g_showTooltips = normalized.values.showTooltips;
    g_showSettingsButton = normalized.values.showSettingsButton;
    g_configVersion = static_cast<t_int32>(std::max<std::int64_t>(g_configVersion.get(), kCurrentSettingsVersion));
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

void showRefrainPreferences() noexcept {
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

} // namespace refrain

#include "settings.h"

#include <windows.h>
#include <mmsystem.h>
#include <objbase.h>
#include <ole2.h>
#include <shellapi.h>

#include <foobar2000/SDK/foobar2000.h>

#include <array>
#include <string>
#include <utility>
#include <vector>

namespace refrain {
namespace {

enum ControlId : int {
    timeMode = 1001,
    showTooltips = 1002,
    showSettingsButton = 1003,
    lyricsAutoSwitch = 1004,
    lowerRightView = 1005,
    showReplayGain = 1006,
    colourMode = 1007,
    themePreset = 1008,
    dependencyStatusBase = 1100,
    dependencyButtonBase = 1200,
    trackActivation = 1300,
};

class SettingsPageInstance : public preferences_page_instance {
public:
    SettingsPageInstance(HWND parent, preferences_page_callback::ptr callback)
        : m_callback(std::move(callback)), m_initial(readSettings()), m_draft(m_initial),
          m_dependencies(detectDependencies()) {
        m_window = CreateWindowExW(0, L"STATIC", nullptr,
            WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
            0, 0, 0, 0, parent, nullptr, core_api::get_my_instance(), nullptr);
        if (!m_window) {
            throw exception_win32(GetLastError());
        }
        SetWindowLongPtrW(m_window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
        m_originalProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(
            m_window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&SettingsPageInstance::windowProc)));
        m_dpi = GetDpiForWindow(m_window);
        createFont();
        createControls();
        writeDraftToControls();
        layoutControls();
    }

    ~SettingsPageInstance() {
        clearSettingsPreview(m_window);
        if (m_window && IsWindow(m_window)) {
            SetWindowLongPtrW(m_window, GWLP_USERDATA, 0);
            if (m_originalProc) {
                SetWindowLongPtrW(m_window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(m_originalProc));
            }
        }
        if (m_ownedFont) {
            DeleteObject(m_ownedFont);
        }
    }

    t_uint32 get_state() override {
        auto state = static_cast<t_uint32>(preferences_state::resettable | preferences_state::dark_mode_supported);
        if (!(readDraftFromControls() == m_initial)) {
            state |= preferences_state::changed;
        }
        return state;
    }

    HWND get_wnd() override { return m_window; }

    void apply() override {
        m_draft = readDraftFromControls();
        auto merged = readSettings();
        merged.timeDisplay = m_draft.timeDisplay; merged.showTooltips = m_draft.showTooltips;
        merged.showSettingsButton = m_draft.showSettingsButton; merged.trackActivation = m_draft.trackActivation;
        merged.colourMode = m_draft.colourMode; merged.themePreset = m_draft.themePreset;
        writeSettings(merged); m_draft = merged;
        clearSettingsPreview(m_window);
        m_initial = m_draft;
        notifyChanged();
    }

    void reset() override {
        const auto defaults = defaultSettings();
        m_draft.timeDisplay = defaults.timeDisplay; m_draft.showTooltips = defaults.showTooltips;
        m_draft.showSettingsButton = defaults.showSettingsButton; m_draft.trackActivation = defaults.trackActivation;
        m_draft.colourMode = defaults.colourMode; m_draft.themePreset = defaults.themePreset;
        writeDraftToControls();
        previewDraft();
        notifyChanged();
    }

private:
    static LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wp, LPARAM lp) {
        auto* self = reinterpret_cast<SettingsPageInstance*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (!self || !self->m_originalProc) {
            return DefWindowProcW(window, message, wp, lp);
        }
        switch (message) {
        case WM_SIZE:
            self->layoutControls();
            return 0;
        case WM_DPICHANGED:
        case WM_DPICHANGED_AFTERPARENT:
            self->m_dpi = GetDpiForWindow(window);
            self->createFont();
            EnumChildWindows(window, [](HWND child, LPARAM parameter) -> BOOL {
                SendMessageW(child, WM_SETFONT, static_cast<WPARAM>(parameter), TRUE);
                return TRUE;
            }, reinterpret_cast<LPARAM>(self->m_font));
            self->layoutControls();
            return 0;
        case WM_COMMAND:
            self->onCommand(LOWORD(wp), HIWORD(wp));
            return 0;
        case WM_NCDESTROY: {
            const auto original = self->m_originalProc;
            clearSettingsPreview(window);
            SetWindowLongPtrW(window, GWLP_USERDATA, 0);
            self->m_window = nullptr;
            return CallWindowProcW(original, window, message, wp, lp);
        }
        default:
            return CallWindowProcW(self->m_originalProc, window, message, wp, lp);
        }
    }

    HWND addControl(const wchar_t* className, const wchar_t* text, DWORD style, int id) {
        const auto control = CreateWindowExW(0, className, text, WS_CHILD | WS_VISIBLE | style,
            0, 0, 0, 0, m_window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
            core_api::get_my_instance(), nullptr);
        if (!control) {
            throw exception_win32(GetLastError());
        }
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(m_font), TRUE);
        return control;
    }

    [[nodiscard]] int scaled(int value) const noexcept {
        return MulDiv(value, static_cast<int>(m_dpi), 96);
    }

    void createFont() {
        if (m_ownedFont) {
            DeleteObject(m_ownedFont);
            m_ownedFont = nullptr;
        }
        NONCLIENTMETRICSW metrics{sizeof(metrics)};
        if (SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0, m_dpi)) {
            m_ownedFont = CreateFontIndirectW(&metrics.lfMessageFont);
        }
        m_font = m_ownedFont ? m_ownedFont : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    }

    void createControls() {
        m_generalGroup = addControl(L"BUTTON", L"Playback controls", BS_GROUPBOX, 0);
        m_timeLabel = addControl(L"STATIC", L"Right-side time:", SS_LEFT, 0);
        m_timeMode = addControl(L"COMBOBOX", nullptr, CBS_DROPDOWNLIST | WS_TABSTOP, ControlId::timeMode);
        SendMessageW(m_timeMode, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Total"));
        SendMessageW(m_timeMode, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Remaining"));
        m_tooltips = addControl(L"BUTTON", L"Show control tooltips",
            BS_AUTOCHECKBOX | WS_TABSTOP, ControlId::showTooltips);
        m_settingsButton = addControl(L"BUTTON", L"Show settings button",
            BS_AUTOCHECKBOX | WS_TABSTOP, ControlId::showSettingsButton);
        m_trackActivationLabel = addControl(L"STATIC", L"Activate track (double-click or Enter):", SS_LEFT, 0);
        m_trackActivation = addControl(L"COMBOBOX", nullptr,
            CBS_DROPDOWNLIST | WS_TABSTOP, ControlId::trackActivation);
        for (const auto* value : {L"Play", L"Add to queue"})
            SendMessageW(m_trackActivation, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(value));

        m_lyricsGroup = addControl(L"BUTTON", L"Lyrics and track details", BS_GROUPBOX, 0);
        m_lyricsAutoSwitch = addControl(L"BUTTON", L"Automatically show lyrics while playing",
            BS_AUTOCHECKBOX | WS_TABSTOP, ControlId::lyricsAutoSwitch);
        m_lowerViewLabel = addControl(L"STATIC", L"Manual/default view:", SS_LEFT, 0);
        m_lowerRightView = addControl(L"COMBOBOX", nullptr,
            CBS_DROPDOWNLIST | WS_TABSTOP, ControlId::lowerRightView);
        SendMessageW(m_lowerRightView, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Lyrics"));
        SendMessageW(m_lowerRightView, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Track details"));
        m_showReplayGain = addControl(L"BUTTON", L"Show ReplayGain section in track details",
            BS_AUTOCHECKBOX | WS_TABSTOP, ControlId::showReplayGain);

        m_appearanceGroup = addControl(L"BUTTON", L"Appearance", BS_GROUPBOX, 0);
        m_colourModeLabel = addControl(L"STATIC", L"Colour mode:", SS_LEFT, 0);
        m_colourMode = addControl(L"COMBOBOX", nullptr,
            CBS_DROPDOWNLIST | WS_TABSTOP, ControlId::colourMode);
        for (const auto* value : {L"Follow Windows", L"Refrain preset", L"Follow album artwork"}) {
            SendMessageW(m_colourMode, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(value));
        }
        m_themeLabel = addControl(L"STATIC", L"Preset:", SS_LEFT, 0);
        m_themePreset = addControl(L"COMBOBOX", nullptr,
            CBS_DROPDOWNLIST | WS_TABSTOP, ControlId::themePreset);
        for (const auto* value : {L"漫雾青 (Mist)", L"暖纸橙 (Paper)", L"松影深色 (Pine)", L"靛夜琥珀 (Ink)"}) {
            SendMessageW(m_themePreset, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(value));
        }

        m_dependenciesGroup = addControl(L"BUTTON", L"Dependencies (read-only)", BS_GROUPBOX, 0);
        for (std::size_t index = 0; index < m_dependencies.size(); ++index) {
            const auto& dependency = m_dependencies[index];
            auto status = dependency.displayName + L": ";
            if (dependency.detected) {
                status += L"Detected";
                if (!dependency.version.empty()) {
                    status += L" ";
                    status += dependency.version;
                }
            } else {
                status += L"Not detected";
            }
            m_dependencyLabels.push_back(addControl(L"STATIC", status.c_str(), SS_LEFT,
                ControlId::dependencyStatusBase + static_cast<int>(index)));
            m_dependencyButtons.push_back(addControl(L"BUTTON", L"Open official page",
                BS_PUSHBUTTON | WS_TABSTOP, ControlId::dependencyButtonBase + static_cast<int>(index)));
        }
        m_dependencyNote = addControl(L"STATIC",
            L"Refrain does not download or update these components. Newer detected versions are not treated as errors.",
            SS_LEFT, 0);
    }

    void layoutControls() const {
        if (!m_window) {
            return;
        }
        RECT client{};
        GetClientRect(m_window, &client);
        const auto width = client.right - client.left;
        const auto margin8 = scaled(8);
        const auto margin16 = scaled(16);
        const auto margin24 = scaled(24);
        const auto contentWidth = (width > scaled(32)) ? width - scaled(32) : 1;
        MoveWindow(m_generalGroup, margin8, margin8, width > margin16 ? width - margin16 : 1, scaled(170), TRUE);
        MoveWindow(m_timeLabel, margin24, scaled(34), scaled(120), scaled(22), TRUE);
        MoveWindow(m_timeMode, scaled(150), scaled(30), scaled(170), scaled(200), TRUE);
        MoveWindow(m_tooltips, margin24, scaled(66), scaled(250), scaled(24), TRUE);
        MoveWindow(m_settingsButton, margin24, scaled(98), scaled(250), scaled(24), TRUE);
        MoveWindow(m_trackActivationLabel, margin24, scaled(130), scaled(240), scaled(22), TRUE);
        MoveWindow(m_trackActivation, scaled(270), scaled(126), scaled(150), scaled(180), TRUE);

        for (const auto control : {m_lyricsGroup, m_lyricsAutoSwitch, m_lowerViewLabel, m_lowerRightView,
                m_showReplayGain, m_dependenciesGroup, m_dependencyNote}) ShowWindow(control, SW_HIDE);
        for (const auto control : m_dependencyLabels) ShowWindow(control, SW_HIDE);
        for (const auto control : m_dependencyButtons) ShowWindow(control, SW_HIDE);
        MoveWindow(m_lyricsGroup, margin8, scaled(148), width > margin16 ? width - margin16 : 1, scaled(132), TRUE);
        MoveWindow(m_lyricsAutoSwitch, margin24, scaled(174), scaled(330), scaled(24), TRUE);
        MoveWindow(m_lowerViewLabel, margin24, scaled(208), scaled(130), scaled(22), TRUE);
        MoveWindow(m_lowerRightView, scaled(160), scaled(204), scaled(170), scaled(200), TRUE);
        MoveWindow(m_showReplayGain, margin24, scaled(240), scaled(340), scaled(24), TRUE);

        MoveWindow(m_appearanceGroup, margin8, scaled(188), width > margin16 ? width - margin16 : 1, scaled(110), TRUE);
        MoveWindow(m_colourModeLabel, margin24, scaled(216), scaled(128), scaled(22), TRUE);
        MoveWindow(m_colourMode, scaled(160), scaled(212), scaled(220), scaled(200), TRUE);
        MoveWindow(m_themeLabel, margin24, scaled(250), scaled(128), scaled(22), TRUE);
        MoveWindow(m_themePreset, scaled(160), scaled(246), scaled(220), scaled(200), TRUE);

        MoveWindow(m_dependenciesGroup, margin8, scaled(410), width > margin16 ? width - margin16 : 1, scaled(184), TRUE);
        for (std::size_t index = 0; index < m_dependencyLabels.size(); ++index) {
            const auto y = scaled(438 + static_cast<int>(index) * 38);
            MoveWindow(m_dependencyLabels[index], margin24, y + scaled(5),
                contentWidth > scaled(190) ? contentWidth - scaled(190) : 1, scaled(22), TRUE);
            MoveWindow(m_dependencyButtons[index], width > scaled(174) ? width - scaled(166) : margin8,
                y, scaled(142), scaled(28), TRUE);
        }
        MoveWindow(m_dependencyNote, margin24, scaled(554),
            contentWidth > margin16 ? contentWidth - margin16 : 1, scaled(34), TRUE);
    }

    void onCommand(int id, int notification) {
        if (id >= ControlId::dependencyButtonBase
            && id < ControlId::dependencyButtonBase + static_cast<int>(m_dependencies.size())
            && notification == BN_CLICKED) {
            const auto index = static_cast<std::size_t>(id - ControlId::dependencyButtonBase);
            ShellExecuteW(m_window, L"open", m_dependencies[index].officialUrl.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            return;
        }
        if ((id == ControlId::timeMode && notification == CBN_SELCHANGE)
            || (id == ControlId::lowerRightView && notification == CBN_SELCHANGE)
            || (id == ControlId::trackActivation && notification == CBN_SELCHANGE)
            || ((id == ControlId::colourMode || id == ControlId::themePreset)
                && notification == CBN_SELCHANGE)
            || ((id == ControlId::showTooltips || id == ControlId::showSettingsButton
                    || id == ControlId::lyricsAutoSwitch || id == ControlId::showReplayGain)
                && notification == BN_CLICKED)) {
            if (id == ControlId::colourMode || id == ControlId::themePreset) {
                previewDraft();
            }
            notifyChanged();
            return;
        }
    }

    [[nodiscard]] SettingsValues readDraftFromControls() const {
        auto values = m_draft;
        const auto selection = SendMessageW(m_timeMode, CB_GETCURSEL, 0, 0);
        values.timeDisplay = selection == 1 ? TimeDisplayMode::remaining : TimeDisplayMode::total;
        values.showTooltips = SendMessageW(m_tooltips, BM_GETCHECK, 0, 0) == BST_CHECKED;
        values.showSettingsButton = SendMessageW(m_settingsButton, BM_GETCHECK, 0, 0) == BST_CHECKED;
        values.lyricsAutoSwitch = SendMessageW(m_lyricsAutoSwitch, BM_GETCHECK, 0, 0) == BST_CHECKED;
        values.lowerRightView = SendMessageW(m_lowerRightView, CB_GETCURSEL, 0, 0) == 1
            ? LowerRightView::trackDetails : LowerRightView::lyrics;
        values.showReplayGain = SendMessageW(m_showReplayGain, BM_GETCHECK, 0, 0) == BST_CHECKED;
        values.detailsReplayGain = values.showReplayGain;
        values.trackActivation = SendMessageW(m_trackActivation, CB_GETCURSEL, 0, 0) == 1
            ? TrackActivationAction::addToQueue : TrackActivationAction::play;
        values.rightHeaderPermille = m_draft.rightHeaderPermille;
        values.rightColumnPermille = m_draft.rightColumnPermille;
        const auto colourMode = SendMessageW(m_colourMode, CB_GETCURSEL, 0, 0);
        values.colourMode = colourMode >= 0 && colourMode <= 2
            ? static_cast<ColourMode>(colourMode) : ColourMode::refrainPreset;
        const auto preset = SendMessageW(m_themePreset, CB_GETCURSEL, 0, 0);
        values.themePreset = preset >= 0 && preset <= 3
            ? static_cast<ThemePreset>(preset) : ThemePreset::mist;
        return values;
    }

    void writeDraftToControls() const {
        SendMessageW(m_timeMode, CB_SETCURSEL,
            m_draft.timeDisplay == TimeDisplayMode::remaining ? 1 : 0, 0);
        SendMessageW(m_tooltips, BM_SETCHECK, m_draft.showTooltips ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessageW(m_settingsButton, BM_SETCHECK,
            m_draft.showSettingsButton ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessageW(m_lyricsAutoSwitch, BM_SETCHECK,
            m_draft.lyricsAutoSwitch ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessageW(m_lowerRightView, CB_SETCURSEL,
            m_draft.lowerRightView == LowerRightView::trackDetails ? 1 : 0, 0);
        SendMessageW(m_showReplayGain, BM_SETCHECK,
            m_draft.showReplayGain ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessageW(m_trackActivation, CB_SETCURSEL,
            m_draft.trackActivation == TrackActivationAction::addToQueue ? 1 : 0, 0);
        SendMessageW(m_colourMode, CB_SETCURSEL,
            static_cast<WPARAM>(static_cast<std::int64_t>(m_draft.colourMode)), 0);
        SendMessageW(m_themePreset, CB_SETCURSEL,
            static_cast<WPARAM>(static_cast<std::int64_t>(m_draft.themePreset)), 0);
        updateAppearanceControls();
    }

    void updateAppearanceControls() const {
        EnableWindow(m_themePreset, m_draft.colourMode == ColourMode::refrainPreset);
        EnableWindow(m_themeLabel, m_draft.colourMode == ColourMode::refrainPreset);
    }

    void previewDraft() {
        m_draft = readDraftFromControls();
        setSettingsPreview(m_window, m_draft);
        updateAppearanceControls();
    }

    void notifyChanged() const {
        if (m_callback.is_valid()) {
            m_callback->on_state_changed();
        }
    }

    preferences_page_callback::ptr m_callback;
    SettingsValues m_initial;
    SettingsValues m_draft;
    std::vector<DependencyStatus> m_dependencies;
    HWND m_window{};
    WNDPROC m_originalProc{};
    HFONT m_font{};
    HFONT m_ownedFont{};
    UINT m_dpi{96};
    HWND m_generalGroup{};
    HWND m_timeLabel{};
    HWND m_timeMode{};
    HWND m_tooltips{};
    HWND m_settingsButton{};
    HWND m_trackActivationLabel{};
    HWND m_trackActivation{};
    HWND m_lyricsGroup{};
    HWND m_lyricsAutoSwitch{};
    HWND m_lowerViewLabel{};
    HWND m_lowerRightView{};
    HWND m_showReplayGain{};
    HWND m_appearanceGroup{};
    HWND m_colourModeLabel{};
    HWND m_colourMode{};
    HWND m_themeLabel{};
    HWND m_themePreset{};
    HWND m_dependenciesGroup{};
    std::vector<HWND> m_dependencyLabels;
    std::vector<HWND> m_dependencyButtons;
    HWND m_dependencyNote{};
};

class SettingsPage : public preferences_page_v3 {
public:
    const char* get_name() override { return "Refrain"; }
    GUID get_guid() override { return kPreferencesPageGuid; }
    GUID get_parent_guid() override { return preferences_page::guid_display; }
    preferences_page_instance::ptr instantiate(HWND parent, preferences_page_callback::ptr callback) override {
        return fb2k::service_new<SettingsPageInstance>(parent, std::move(callback));
    }
};

class SettingsCommand : public mainmenu_commands {
public:
    t_uint32 get_command_count() override { return 1; }
    GUID get_command(t_uint32) override { return kSettingsCommandGuid; }
    void get_name(t_uint32, pfc::string_base& out) override { out = "Refrain Settings..."; }
    bool get_description(t_uint32, pfc::string_base& out) override {
        out = "Open Refrain settings in foobar2000 Preferences";
        return true;
    }
    GUID get_parent() override { return mainmenu_groups::view; }
    t_uint32 get_sort_priority() override { return sort_priority_base; }
    void execute(t_uint32, ctx_t) override { showRefrainPreferences(); }
};

preferences_page_factory_t<SettingsPage> g_settingsPageFactory;
mainmenu_commands_factory_t<SettingsCommand> g_settingsCommandFactory;

} // namespace
} // namespace refrain

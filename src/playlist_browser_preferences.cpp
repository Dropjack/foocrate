#include "playlist_browser_model.h"
#include "settings.h"

#include <windows.h>
#include <mmsystem.h>
#include <objbase.h>
#include <ole2.h>
#include <foobar2000/SDK/foobar2000.h>

#include <algorithm>
#include <utility>
#include <vector>

namespace refrain {
namespace {

enum ControlId : int {
    defaultPlaylist = 3001,
};

class PlaylistBrowserSettingsPageInstance : public preferences_page_instance {
public:
    PlaylistBrowserSettingsPageInstance(HWND parent, preferences_page_callback::ptr callback)
        : m_callback(std::move(callback)), m_initial(readPlaylistBrowserSettings()), m_draft(m_initial) {
        m_window = CreateWindowExW(0, L"STATIC", nullptr,
            WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
            0, 0, 0, 0, parent, nullptr, core_api::get_my_instance(), nullptr);
        if (!m_window) throw exception_win32(GetLastError());
        SetWindowLongPtrW(m_window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
        m_originalProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(m_window, GWLP_WNDPROC,
            reinterpret_cast<LONG_PTR>(&PlaylistBrowserSettingsPageInstance::windowProc)));
        m_dpi = GetDpiForWindow(m_window);
        createFont();
        m_label = addControl(L"STATIC", L"Default playlist:", SS_LEFT, 0);
        m_combo = addControl(L"COMBOBOX", nullptr,
            CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, defaultPlaylist);
        m_explanation = addControl(L"STATIC",
            L"Refrain activates this playlist once when foobar2000 starts. Both workspaces use it as their initial source.\r\n\r\n"
            L"The Playlist Browser divider is saved when you drag it. Reset restores the 15% default.",
            SS_LEFT, 0);
        rebuildPlaylistChoices();
        layoutControls();
    }

    ~PlaylistBrowserSettingsPageInstance() {
        if (m_window && IsWindow(m_window)) {
            SetWindowLongPtrW(m_window, GWLP_USERDATA, 0);
            if (m_originalProc) SetWindowLongPtrW(m_window, GWLP_WNDPROC,
                reinterpret_cast<LONG_PTR>(m_originalProc));
        }
        if (m_ownedFont) DeleteObject(m_ownedFont);
    }

    t_uint32 get_state() override {
        readChoice();
        auto state = static_cast<t_uint32>(preferences_state::resettable);
        if (!sameGuid(m_draft.defaultPlaylistGuid, m_initial.defaultPlaylistGuid)
            || m_draft.splitPermille != m_initial.splitPermille) state |= preferences_state::changed;
        return state;
    }
    HWND get_wnd() override { return m_window; }
    void apply() override {
        readChoice();
        writePlaylistBrowserSettings(m_draft);
        m_initial = m_draft;
        notifyChanged();
    }
    void reset() override {
        m_draft = {};
        m_draft.splitPermille = 150;
        selectDraft();
        notifyChanged();
    }

private:
    static LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wp, LPARAM lp) {
        auto* self = reinterpret_cast<PlaylistBrowserSettingsPageInstance*>(
            GetWindowLongPtrW(window, GWLP_USERDATA));
        if (!self || !self->m_originalProc) return DefWindowProcW(window, message, wp, lp);
        if (message == WM_SIZE) {
            self->layoutControls();
            return 0;
        }
        if (message == WM_COMMAND && LOWORD(wp) == defaultPlaylist && HIWORD(wp) == CBN_SELCHANGE) {
            self->readChoice();
            self->notifyChanged();
            return 0;
        }
        if (message == WM_DPICHANGED || message == WM_DPICHANGED_AFTERPARENT) {
            self->m_dpi = GetDpiForWindow(window);
            self->createFont();
            for (const auto child : {self->m_label, self->m_combo, self->m_explanation}) {
                SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(self->m_font), TRUE);
            }
            self->layoutControls();
            return 0;
        }
        if (message == WM_NCDESTROY) {
            const auto original = self->m_originalProc;
            SetWindowLongPtrW(window, GWLP_USERDATA, 0);
            self->m_window = nullptr;
            return CallWindowProcW(original, window, message, wp, lp);
        }
        return CallWindowProcW(self->m_originalProc, window, message, wp, lp);
    }

    HWND addControl(const wchar_t* className, const wchar_t* text, DWORD style, int id) {
        const auto control = CreateWindowExW(0, className, text, WS_CHILD | WS_VISIBLE | style,
            0, 0, 0, 0, m_window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
            core_api::get_my_instance(), nullptr);
        if (!control) throw exception_win32(GetLastError());
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(m_font), TRUE);
        return control;
    }

    [[nodiscard]] int scaled(int value) const noexcept {
        return MulDiv(value, static_cast<int>(m_dpi), 96);
    }
    void createFont() {
        NONCLIENTMETRICSW metrics{sizeof(metrics)};
        SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0);
        metrics.lfMessageFont.lfHeight = MulDiv(metrics.lfMessageFont.lfHeight,
            static_cast<int>(m_dpi), 96);
        const auto next = CreateFontIndirectW(&metrics.lfMessageFont);
        if (m_ownedFont) DeleteObject(m_ownedFont);
        m_ownedFont = next;
        m_font = next ? next : reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    }
    void layoutControls() const {
        if (!m_window) return;
        RECT client{};
        GetClientRect(m_window, &client);
        const auto width = std::max(scaled(320), static_cast<int>(client.right));
        MoveWindow(m_label, scaled(12), scaled(18), scaled(120), scaled(24), TRUE);
        MoveWindow(m_combo, scaled(136), scaled(14), width - scaled(152), scaled(240), TRUE);
        MoveWindow(m_explanation, scaled(12), scaled(58), width - scaled(24), scaled(110), TRUE);
    }

    void rebuildPlaylistChoices() {
        m_guids.clear();
        SendMessageW(m_combo, CB_RESETCONTENT, 0, 0);
        const auto bridge = readAlbumBrowserSettings().bridgeGuid;
        auto api = playlist_manager_v5::get();
        for (t_size index = 0; index < api->get_playlist_count(); ++index) {
            const auto guid = api->playlist_get_guid(index);
            if (!nullGuid(bridge) && sameGuid(bridge, guid)) continue;
            pfc::string8 name;
            if (!api->playlist_get_name(index, name)) continue;
            const auto wide = [&] {
                const auto count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                    name.c_str(), -1, nullptr, 0);
                if (count <= 1) return std::wstring{};
                std::wstring value(static_cast<std::size_t>(count), L'\0');
                MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name.c_str(), -1,
                    value.data(), count);
                value.pop_back();
                return value;
            }();
            SendMessageW(m_combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(wide.c_str()));
            m_guids.push_back(guid);
        }
        if (m_guids.empty()) {
            SendMessageW(m_combo, CB_ADDSTRING, 0,
                reinterpret_cast<LPARAM>(L"No playlist available (Refrain will create one)"));
            EnableWindow(m_combo, FALSE);
        }
        selectDraft();
    }
    void selectDraft() {
        auto selected = 0;
        for (std::size_t index = 0; index < m_guids.size(); ++index) {
            if (sameGuid(m_guids[index], m_draft.defaultPlaylistGuid)) {
                selected = static_cast<int>(index);
                break;
            }
        }
        SendMessageW(m_combo, CB_SETCURSEL, selected, 0);
        if (!m_guids.empty() && nullGuid(m_draft.defaultPlaylistGuid)) {
            m_draft.defaultPlaylistGuid = m_guids[static_cast<std::size_t>(selected)];
        }
    }
    void readChoice() {
        const auto selected = SendMessageW(m_combo, CB_GETCURSEL, 0, 0);
        if (selected >= 0 && static_cast<std::size_t>(selected) < m_guids.size()) {
            m_draft.defaultPlaylistGuid = m_guids[static_cast<std::size_t>(selected)];
        }
    }
    void notifyChanged() const {
        if (m_callback.is_valid()) m_callback->on_state_changed();
    }

    preferences_page_callback::ptr m_callback;
    PlaylistBrowserSettings m_initial;
    PlaylistBrowserSettings m_draft;
    std::vector<GUID> m_guids;
    HWND m_window{};
    WNDPROC m_originalProc{};
    HWND m_label{};
    HWND m_combo{};
    HWND m_explanation{};
    HFONT m_font{};
    HFONT m_ownedFont{};
    UINT m_dpi{96};
};

class PlaylistBrowserSettingsPage : public preferences_page_v3 {
public:
    const char* get_name() override { return "Playlist Browser"; }
    GUID get_guid() override { return kPlaylistBrowserPreferencesPageGuid; }
    GUID get_parent_guid() override { return kPreferencesPageGuid; }
    preferences_page_instance::ptr instantiate(HWND parent,
        preferences_page_callback::ptr callback) override {
        return fb2k::service_new<PlaylistBrowserSettingsPageInstance>(parent, std::move(callback));
    }
};

preferences_page_factory_t<PlaylistBrowserSettingsPage> g_playlistBrowserSettingsPageFactory;

} // namespace
} // namespace refrain

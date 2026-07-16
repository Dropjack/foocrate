#include "settings.h"

#include <windows.h>
#include <mmsystem.h>
#include <objbase.h>
#include <ole2.h>
#include <foobar2000/SDK/foobar2000.h>

#include <algorithm>
#include <array>
#include <string>
#include <utility>

namespace foocrate {
namespace {

enum Id : int { follow = 5001, rotate, front, back, disc, artist, interval, autoLyrics,
    defaultView, metadata, location, technical, statistics, replayGain, summary };

class PageInstance : public preferences_page_instance {
public:
    PageInstance(HWND parent, preferences_page_callback::ptr callback)
        : m_callback(std::move(callback)), m_initial(readSettings()), m_draft(m_initial) {
        m_window = CreateWindowExW(0, L"STATIC", nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_VSCROLL,
            0, 0, 0, 0, parent, nullptr, core_api::get_my_instance(), nullptr);
        if (!m_window) throw exception_win32(GetLastError());
        SetWindowLongPtrW(m_window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
        m_original = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(m_window, GWLP_WNDPROC,
            reinterpret_cast<LONG_PTR>(&PageInstance::proc)));
        m_dpi = GetDpiForWindow(m_window); createFont(); createControls(); writeControls(); layout();
    }
    ~PageInstance() {
        if (m_window && IsWindow(m_window) && m_original) SetWindowLongPtrW(m_window, GWLP_WNDPROC,
            reinterpret_cast<LONG_PTR>(m_original));
        if (m_fontOwned) DeleteObject(m_fontOwned);
    }
    t_uint32 get_state() override {
        readControls();
        auto state = static_cast<t_uint32>(preferences_state::resettable | preferences_state::dark_mode_supported);
        if (!(m_draft == m_initial)) state |= preferences_state::changed;
        return state;
    }
    HWND get_wnd() override { return m_window; }
    void apply() override {
        readControls();
        titleformat_object::ptr script;
        if (m_draft.nowPlayingSummaryFormat.empty()
            || !titleformat_compiler::get()->compile(script, m_draft.nowPlayingSummaryFormat.c_str())) {
            SetWindowTextW(m_summaryHint, L"Error: Now Playing summary format is invalid.");
            SetFocus(m_summary); return;
        }
        SetWindowTextW(m_summaryHint, L"Format is valid.");
        auto merged = readSettings();
        merged.rightPanelFollow = m_draft.rightPanelFollow;
        merged.artworkRotationEnabled = m_draft.artworkRotationEnabled;
        merged.artworkSourceMask = m_draft.artworkSourceMask;
        merged.artworkRotationSeconds = m_draft.artworkRotationSeconds;
        merged.lyricsAutoSwitch = m_draft.lyricsAutoSwitch; merged.lowerRightView = m_draft.lowerRightView;
        merged.detailsMetadata = m_draft.detailsMetadata; merged.detailsLocation = m_draft.detailsLocation;
        merged.detailsTechnical = m_draft.detailsTechnical;
        merged.detailsPlaybackStatistics = m_draft.detailsPlaybackStatistics;
        merged.detailsReplayGain = m_draft.detailsReplayGain; merged.showReplayGain = m_draft.detailsReplayGain;
        merged.nowPlayingSummaryFormat = m_draft.nowPlayingSummaryFormat;
        writeSettings(merged); m_draft = merged; m_initial = merged; changed();
    }
    void reset() override {
        const auto defaults = defaultSettings();
        m_draft.rightPanelFollow = defaults.rightPanelFollow;
        m_draft.artworkRotationEnabled = defaults.artworkRotationEnabled;
        m_draft.artworkSourceMask = defaults.artworkSourceMask;
        m_draft.artworkRotationSeconds = defaults.artworkRotationSeconds;
        m_draft.lyricsAutoSwitch = defaults.lyricsAutoSwitch;
        m_draft.lowerRightView = defaults.lowerRightView;
        m_draft.detailsMetadata = defaults.detailsMetadata; m_draft.detailsLocation = defaults.detailsLocation;
        m_draft.detailsTechnical = defaults.detailsTechnical;
        m_draft.detailsPlaybackStatistics = defaults.detailsPlaybackStatistics;
        m_draft.detailsReplayGain = defaults.detailsReplayGain;
        m_draft.showReplayGain = defaults.detailsReplayGain;
        m_draft.nowPlayingSummaryFormat = defaults.nowPlayingSummaryFormat;
        writeControls(); changed();
    }
private:
    static LRESULT CALLBACK proc(HWND window, UINT msg, WPARAM wp, LPARAM lp) {
        auto* self = reinterpret_cast<PageInstance*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (!self || !self->m_original) return DefWindowProcW(window, msg, wp, lp);
        if (msg == WM_SIZE) { self->layout(); return 0; }
        if (msg == WM_DPICHANGED || msg == WM_DPICHANGED_AFTERPARENT) {
            self->updateDpi(GetDpiForWindow(window)); return 0;
        }
        if (msg == WM_SETTINGCHANGE) { self->updateDpi(GetDpiForWindow(window)); return 0; }
        if (msg == WM_MOUSEWHEEL) {
            self->scrollBy(-GET_WHEEL_DELTA_WPARAM(wp) / WHEEL_DELTA * self->s(48));
            return 0;
        }
        if (msg == WM_VSCROLL && lp == 0) { self->onVerticalScroll(LOWORD(wp)); return 0; }
        if (msg == WM_COMMAND) { self->readControls(); self->enableRotation(); self->changed(); return 0; }
        if (msg == WM_NCDESTROY) { const auto original = self->m_original; self->m_window = nullptr;
            SetWindowLongPtrW(window, GWLP_USERDATA, 0); return CallWindowProcW(original, window, msg, wp, lp); }
        return CallWindowProcW(self->m_original, window, msg, wp, lp);
    }
    int s(int value) const { return MulDiv(value, static_cast<int>(m_dpi), 96); }
    void createFont() {
        if (m_fontOwned) { DeleteObject(m_fontOwned); m_fontOwned = nullptr; }
        NONCLIENTMETRICSW metrics{sizeof(metrics)};
        SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0, m_dpi);
        m_fontOwned = CreateFontIndirectW(&metrics.lfMessageFont);
        m_font = m_fontOwned ? m_fontOwned : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    }
    void updateDpi(UINT dpi) {
        const auto oldDpi = std::max(1U, m_dpi);
        m_dpi = dpi ? dpi : 96U;
        m_scrollY = MulDiv(m_scrollY, static_cast<int>(m_dpi), static_cast<int>(oldDpi));
        createFont();
        EnumChildWindows(m_window, [](HWND child, LPARAM font) -> BOOL {
            SendMessageW(child, WM_SETFONT, static_cast<WPARAM>(font), TRUE); return TRUE;
        }, reinterpret_cast<LPARAM>(m_font));
        layout();
        RedrawWindow(m_window, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
    }
    HWND add(const wchar_t* cls, const wchar_t* text, DWORD style, int id = 0) {
        const auto window = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 0, 0,
            m_window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), core_api::get_my_instance(), nullptr);
        SendMessageW(window, WM_SETFONT, reinterpret_cast<WPARAM>(m_font), TRUE); return window;
    }
    void createControls() {
        m_followBox = add(L"BUTTON", L"Right panel", BS_GROUPBOX);
        m_followLabel = add(L"STATIC", L"Right panel follows:", SS_LEFT);
        m_follow = add(L"COMBOBOX", nullptr, CBS_DROPDOWNLIST | WS_TABSTOP, Id::follow);
        for (const auto* value : {L"Playing track", L"Playlist selection"}) SendMessageW(m_follow, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(value));
        m_artBox = add(L"BUTTON", L"Artwork rotation", BS_GROUPBOX);
        m_rotate = add(L"BUTTON", L"Rotate artwork", BS_AUTOCHECKBOX | WS_TABSTOP, Id::rotate);
        for (const auto [label, id] : std::array<std::pair<const wchar_t*, int>, 4>{{{L"Front", Id::front}, {L"Back", Id::back}, {L"Disc", Id::disc}, {L"Artist", Id::artist}}})
            m_sources[static_cast<std::size_t>(id - Id::front)] = add(L"BUTTON", label, BS_AUTOCHECKBOX | WS_TABSTOP, id);
        m_intervalLabel = add(L"STATIC", L"Interval:", SS_LEFT);
        m_interval = add(L"COMBOBOX", nullptr, CBS_DROPDOWNLIST | WS_TABSTOP, Id::interval);
        for (const auto* value : {L"10 seconds", L"20 seconds", L"30 seconds", L"60 seconds"}) SendMessageW(m_interval, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(value));
        m_detailsBox = add(L"BUTTON", L"Lyrics / Track Details", BS_GROUPBOX);
        m_autoLyrics = add(L"BUTTON", L"Automatically show lyrics while playing", BS_AUTOCHECKBOX | WS_TABSTOP, Id::autoLyrics);
        m_viewLabel = add(L"STATIC", L"Manual/default view:", SS_LEFT);
        m_defaultView = add(L"COMBOBOX", nullptr, CBS_DROPDOWNLIST | WS_TABSTOP, Id::defaultView);
        for (const auto* value : {L"Lyrics", L"Track details"}) SendMessageW(m_defaultView, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(value));
        for (const auto [label, id] : std::array<std::pair<const wchar_t*, int>, 5>{{{L"Metadata", Id::metadata}, {L"Location", Id::location}, {L"Technical", Id::technical}, {L"Playback statistics", Id::statistics}, {L"ReplayGain", Id::replayGain}}})
            m_sections[static_cast<std::size_t>(id - Id::metadata)] = add(L"BUTTON", label, BS_AUTOCHECKBOX | WS_TABSTOP, id);
        m_advancedBox = add(L"BUTTON", L"Advanced", BS_GROUPBOX);
        m_summaryLabel = add(L"STATIC", L"Artist / album line format:", SS_LEFT);
        m_summary = add(L"EDIT", nullptr, WS_BORDER | WS_TABSTOP | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL, Id::summary);
        m_summaryHint = add(L"STATIC", L"Syntax errors are reported here when Apply is pressed.", SS_LEFT);
    }
    void layout() {
        RECT r{}; GetClientRect(m_window, &r);
        const auto viewportHeight = std::max(1, static_cast<int>(r.bottom));
        const auto contentHeight = s(482);
        SCROLLINFO info{sizeof(info), SIF_RANGE | SIF_PAGE | SIF_POS};
        info.nMin = 0; info.nMax = std::max(0, contentHeight - 1);
        info.nPage = static_cast<UINT>(viewportHeight); info.nPos = m_scrollY;
        SetScrollInfo(m_window, SB_VERT, &info, TRUE);
        m_scrollY = std::clamp(m_scrollY, 0, std::max(0, contentHeight - viewportHeight));
        const auto y = [this](int value) { return s(value) - m_scrollY; };
        const auto width = std::max(s(430), static_cast<int>(r.right));
        const auto boxWidth = width - s(16);
        MoveWindow(m_followBox, s(8), y(6), boxWidth, s(54), TRUE);
        MoveWindow(m_followLabel, s(24), y(28), s(132), s(22), TRUE); MoveWindow(m_follow, s(160), y(24), s(210), s(180), TRUE);
        MoveWindow(m_artBox, s(8), y(64), boxWidth, s(96), TRUE); MoveWindow(m_rotate, s(24), y(86), s(130), s(22), TRUE);
        for (std::size_t i = 0; i < m_sources.size(); ++i) MoveWindow(m_sources[i], s(160 + static_cast<int>(i) * 72), y(86), s(70), s(22), TRUE);
        MoveWindow(m_intervalLabel, s(24), y(122), s(90), s(22), TRUE); MoveWindow(m_interval, s(112), y(118), s(150), s(180), TRUE);
        MoveWindow(m_detailsBox, s(8), y(164), boxWidth, s(150), TRUE); MoveWindow(m_autoLyrics, s(24), y(186), s(310), s(22), TRUE);
        MoveWindow(m_viewLabel, s(24), y(216), s(130), s(22), TRUE); MoveWindow(m_defaultView, s(158), y(212), s(170), s(180), TRUE);
        constexpr std::array<int, 5> sectionX{24, 174, 324, 24, 224};
        constexpr std::array<int, 5> sectionY{246, 246, 246, 274, 274};
        constexpr std::array<int, 5> sectionWidth{140, 140, 140, 190, 140};
        for (std::size_t i = 0; i < m_sections.size(); ++i)
            MoveWindow(m_sections[i], s(sectionX[i]), y(sectionY[i]), s(sectionWidth[i]), s(22), TRUE);
        MoveWindow(m_advancedBox, s(8), y(318), boxWidth, s(158), TRUE); MoveWindow(m_summaryLabel, s(24), y(340), s(220), s(22), TRUE);
        MoveWindow(m_summary, s(24), y(362), width - s(48), s(68), TRUE); MoveWindow(m_summaryHint, s(24), y(436), width - s(48), s(22), TRUE);
    }
    void scrollBy(int delta) {
        RECT r{}; GetClientRect(m_window, &r);
        const auto maximum = std::max(0, s(482) - static_cast<int>(r.bottom));
        const auto next = std::clamp(m_scrollY + delta, 0, maximum);
        if (next == m_scrollY) return;
        m_scrollY = next; layout(); InvalidateRect(m_window, nullptr, TRUE);
    }
    void onVerticalScroll(int action) {
        SCROLLINFO info{sizeof(info), SIF_ALL}; GetScrollInfo(m_window, SB_VERT, &info);
        auto next = m_scrollY;
        if (action == SB_LINEUP) next -= s(24);
        else if (action == SB_LINEDOWN) next += s(24);
        else if (action == SB_PAGEUP) next -= static_cast<int>(info.nPage);
        else if (action == SB_PAGEDOWN) next += static_cast<int>(info.nPage);
        else if (action == SB_THUMBTRACK || action == SB_THUMBPOSITION) next = info.nTrackPos;
        scrollBy(next - m_scrollY);
    }
    void readControls() {
        m_draft.rightPanelFollow = SendMessageW(m_follow, CB_GETCURSEL, 0, 0) == 1 ? RightPanelFollow::playlistSelection : RightPanelFollow::playingTrack;
        m_draft.artworkRotationEnabled = SendMessageW(m_rotate, BM_GETCHECK, 0, 0) == BST_CHECKED;
        m_draft.artworkSourceMask = 0;
        constexpr std::array<std::int64_t, 4> bits{kArtworkFront, kArtworkBack, kArtworkDisc, kArtworkArtist};
        for (std::size_t i = 0; i < m_sources.size(); ++i) if (SendMessageW(m_sources[i], BM_GETCHECK, 0, 0) == BST_CHECKED) m_draft.artworkSourceMask |= bits[i];
        if (!m_draft.artworkSourceMask) m_draft.artworkSourceMask = kArtworkFront;
        constexpr std::array<std::int64_t, 4> seconds{10, 20, 30, 60}; const auto selected = SendMessageW(m_interval, CB_GETCURSEL, 0, 0);
        m_draft.artworkRotationSeconds = selected >= 0 && selected < 4 ? seconds[static_cast<std::size_t>(selected)] : 20;
        m_draft.lyricsAutoSwitch = SendMessageW(m_autoLyrics, BM_GETCHECK, 0, 0) == BST_CHECKED;
        m_draft.lowerRightView = SendMessageW(m_defaultView, CB_GETCURSEL, 0, 0) == 1 ? LowerRightView::trackDetails : LowerRightView::lyrics;
        bool* values[]{&m_draft.detailsMetadata, &m_draft.detailsLocation, &m_draft.detailsTechnical, &m_draft.detailsPlaybackStatistics, &m_draft.detailsReplayGain};
        for (std::size_t i = 0; i < m_sections.size(); ++i) *values[i] = SendMessageW(m_sections[i], BM_GETCHECK, 0, 0) == BST_CHECKED;
        m_draft.showReplayGain = m_draft.detailsReplayGain;
        const auto length = GetWindowTextLengthW(m_summary); std::wstring wide(static_cast<std::size_t>(length) + 1, L'\0'); GetWindowTextW(m_summary, wide.data(), length + 1); wide.resize(static_cast<std::size_t>(length));
        const auto count = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide.c_str(), length, nullptr, 0, nullptr, nullptr); m_draft.nowPlayingSummaryFormat.assign(static_cast<std::size_t>(std::max(0, count)), '\0');
        if (count > 0) WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide.c_str(), length, m_draft.nowPlayingSummaryFormat.data(), count, nullptr, nullptr);
    }
    void writeControls() {
        SendMessageW(m_follow, CB_SETCURSEL, m_draft.rightPanelFollow == RightPanelFollow::playlistSelection ? 1 : 0, 0);
        SendMessageW(m_rotate, BM_SETCHECK, m_draft.artworkRotationEnabled ? BST_CHECKED : BST_UNCHECKED, 0);
        constexpr std::array<std::int64_t, 4> bits{kArtworkFront, kArtworkBack, kArtworkDisc, kArtworkArtist}; for (std::size_t i = 0; i < 4; ++i) SendMessageW(m_sources[i], BM_SETCHECK, (m_draft.artworkSourceMask & bits[i]) ? BST_CHECKED : BST_UNCHECKED, 0);
        const auto seconds = m_draft.artworkRotationSeconds; SendMessageW(m_interval, CB_SETCURSEL, seconds == 10 ? 0 : seconds == 30 ? 2 : seconds == 60 ? 3 : 1, 0);
        SendMessageW(m_autoLyrics, BM_SETCHECK, m_draft.lyricsAutoSwitch ? BST_CHECKED : BST_UNCHECKED, 0); SendMessageW(m_defaultView, CB_SETCURSEL, m_draft.lowerRightView == LowerRightView::trackDetails ? 1 : 0, 0);
        const bool values[]{m_draft.detailsMetadata, m_draft.detailsLocation, m_draft.detailsTechnical, m_draft.detailsPlaybackStatistics, m_draft.detailsReplayGain}; for (std::size_t i = 0; i < 5; ++i) SendMessageW(m_sections[i], BM_SETCHECK, values[i] ? BST_CHECKED : BST_UNCHECKED, 0);
        const auto count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, m_draft.nowPlayingSummaryFormat.c_str(), -1, nullptr, 0); std::wstring wide(static_cast<std::size_t>(std::max(1, count)), L'\0'); if (count > 0) MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, m_draft.nowPlayingSummaryFormat.c_str(), -1, wide.data(), count); SetWindowTextW(m_summary, wide.c_str()); enableRotation();
    }
    void enableRotation() const { const auto enabled = SendMessageW(m_rotate, BM_GETCHECK, 0, 0) == BST_CHECKED; for (const auto item : m_sources) EnableWindow(item, enabled); EnableWindow(m_interval, enabled); EnableWindow(m_intervalLabel, enabled); }
    void changed() const { if (m_callback.is_valid()) m_callback->on_state_changed(); }
    preferences_page_callback::ptr m_callback; SettingsValues m_initial, m_draft; HWND m_window{}; WNDPROC m_original{}; UINT m_dpi{96}; HFONT m_font{}, m_fontOwned{};
    HWND m_followBox{}, m_followLabel{}, m_follow{}, m_artBox{}, m_rotate{}, m_intervalLabel{}, m_interval{}; std::array<HWND,4> m_sources{};
    HWND m_detailsBox{}, m_autoLyrics{}, m_viewLabel{}, m_defaultView{}; std::array<HWND,5> m_sections{}; HWND m_advancedBox{}, m_summaryLabel{}, m_summary{}, m_summaryHint{};
    int m_scrollY{};
};

class Page : public preferences_page_v3 {
public:
    const char* get_name() override { return "Now Playing & Details"; }
    GUID get_guid() override { return kNowPlayingPreferencesPageGuid; }
    GUID get_parent_guid() override { return kPreferencesPageGuid; }
    preferences_page_instance::ptr instantiate(HWND parent, preferences_page_callback::ptr callback) override { return fb2k::service_new<PageInstance>(parent, std::move(callback)); }
};
preferences_page_factory_t<Page> g_factory;
}
}

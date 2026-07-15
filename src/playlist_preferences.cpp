#include "settings.h"

#include <windows.h>
#include <windowsx.h>
#include <mmsystem.h>
#include <objbase.h>
#include <ole2.h>
#include <commctrl.h>
#include <foobar2000/SDK/foobar2000.h>

#include <algorithm>
#include <array>
#include <string>
#include <utility>

namespace refrain {
namespace {

enum ControlId : int {
    groupPreset = 4001, autoCollapse, collapseDefault, newGroup, copyGroup, groupUp, groupDown, deleteGroup,
    groupKey, groupSort, groupLeftPrimary, groupRightPrimary, groupLeftSecondary,
    groupRightSecondary, groupArtwork, columnList, newColumn, columnUp, columnDown, copyColumn,
    deleteColumn, columnLabel, columnDisplay, columnSecondary, columnSort, columnAlignment, columnWidth,
    profilePreset, profileName, newProfile, copyProfile, profileUp, profileDown, deleteProfile, trackRows, headerStyle, tabs,
};

[[nodiscard]] std::wstring utf8ToWide(const std::string& value) {
    if (value.empty()) return {};
    const auto count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
        value.c_str(), static_cast<int>(value.size()), nullptr, 0);
    if (count <= 0) return {};
    std::wstring output(static_cast<std::size_t>(count), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.c_str(),
        static_cast<int>(value.size()), output.data(), count);
    return output;
}

[[nodiscard]] std::string wideToUtf8(const std::wstring& value) {
    if (value.empty()) return {};
    const auto count = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
        value.c_str(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (count <= 0) return {};
    std::string output(static_cast<std::size_t>(count), '\0');
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.c_str(),
        static_cast<int>(value.size()), output.data(), count, nullptr, nullptr);
    return output;
}

class PlaylistSettingsPageInstance : public preferences_page_instance {
public:
    PlaylistSettingsPageInstance(HWND parent, preferences_page_callback::ptr callback)
        : m_callback(std::move(callback)), m_initial(readPlaylistViewSettings()), m_draft(m_initial) {
        INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_LISTVIEW_CLASSES};
        InitCommonControlsEx(&controls);
        m_window = CreateWindowExW(0, L"STATIC", nullptr,
            WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VSCROLL,
            0, 0, 0, 0, parent, nullptr, core_api::get_my_instance(), nullptr);
        if (!m_window) throw exception_win32(GetLastError());
        SetWindowLongPtrW(m_window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
        m_originalProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(m_window, GWLP_WNDPROC,
            reinterpret_cast<LONG_PTR>(&PlaylistSettingsPageInstance::windowProc)));
        m_dpi = GetDpiForWindow(m_window);
        createFont();
        createControls();
        rebuildProfileCombo();
        loadProfile(assignedProfileIndex());
        rebuildGroupCombo();
        rebuildColumnList();
        const auto active = std::find_if(m_draft.groups.begin(), m_draft.groups.end(), [&](const auto& group) {
            return group.id == m_draft.activeGroupId;
        });
        loadGroup(active == m_draft.groups.end() ? 0
            : static_cast<std::size_t>(active - m_draft.groups.begin()));
        loadColumn(0);
        layoutControls();
    }

    ~PlaylistSettingsPageInstance() {
        if (m_window && IsWindow(m_window)) {
            SetWindowLongPtrW(m_window, GWLP_USERDATA, 0);
            if (m_originalProc) SetWindowLongPtrW(m_window, GWLP_WNDPROC,
                reinterpret_cast<LONG_PTR>(m_originalProc));
        }
        if (m_ownedFont) DeleteObject(m_ownedFont);
    }

    t_uint32 get_state() override {
        captureControls();
        auto state = static_cast<t_uint32>(preferences_state::resettable);
        if (!(normalizePlaylistViewSettings(m_draft) == normalizePlaylistViewSettings(m_initial))) {
            state |= preferences_state::changed;
        }
        return state;
    }
    HWND get_wnd() override { return m_window; }
    void apply() override {
        captureControls();
        if (!validateFormats()) return;
        m_draft = normalizePlaylistViewSettings(m_draft);
        const auto oldGroup = std::find_if(m_initial.groups.begin(), m_initial.groups.end(), [&](const auto& group) {
            return group.id == m_initial.activeGroupId;
        });
        const auto newGroup = std::find_if(m_draft.groups.begin(), m_draft.groups.end(), [&](const auto& group) {
            return group.id == m_draft.activeGroupId;
        });
        const auto groupingChanged = newGroup != m_draft.groups.end()
            && (oldGroup == m_initial.groups.end() || oldGroup->id != newGroup->id
                || oldGroup->keyFormat != newGroup->keyFormat || oldGroup->sortFormat != newGroup->sortFormat);
        if (groupingChanged && newGroup != m_draft.groups.end()) {
            auto api = playlist_manager::get();
            const auto playlist = api->get_active_playlist();
            const auto itemCount = playlist == SIZE_MAX ? 0 : api->playlist_get_item_count(playlist);
            const auto canReorder = playlist != SIZE_MAX && (!api->playlist_lock_is_present(playlist)
                || (api->playlist_lock_get_filter_mask(playlist) & playlist_lock::filter_reorder) == 0);
            if (shouldPhysicallySortForGroupChange(true, canReorder, itemCount)) {
                api->playlist_undo_backup(playlist);
                if (!api->playlist_sort_by_format(playlist, newGroup->sortFormat.c_str(), false)) {
                    MessageBoxW(m_window, L"foobar2000 could not sort the active playlist, so the grouping change was not applied.",
                        L"Refrain", MB_OK | MB_ICONWARNING);
                    return;
                }
            }
        }
        writePlaylistViewSettings(m_draft);
        m_initial = m_draft;
        notifyChanged();
    }
    void reset() override {
        m_draft = defaultPlaylistViewSettings();
        m_profileIndex = m_groupIndex = m_columnIndex = static_cast<std::size_t>(-1);
        rebuildProfileCombo(); loadProfile(0);
        rebuildGroupCombo(); rebuildColumnList(); loadGroup(0); loadColumn(0);
        notifyChanged();
    }

private:
    static LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wp, LPARAM lp) {
        auto* self = reinterpret_cast<PlaylistSettingsPageInstance*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (!self || !self->m_originalProc) return DefWindowProcW(window, message, wp, lp);
        if (message == WM_SIZE) { self->layoutControls(); return 0; }
        if (message == WM_DPICHANGED || message == WM_DPICHANGED_AFTERPARENT) {
            self->updateDpi(GetDpiForWindow(window)); return 0;
        }
        if (message == WM_SETTINGCHANGE) { self->updateDpi(GetDpiForWindow(window)); return 0; }
        if (message == WM_MOUSEWHEEL) {
            const auto point = POINT{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            if (const auto target = WindowFromPoint(point); target == self->m_profileList) {
                SendMessageW(target, message, wp, lp); return 0;
            }
            self->scrollBy(-GET_WHEEL_DELTA_WPARAM(wp) / WHEEL_DELTA * self->scaled(48));
            return 0;
        }
        if (message == WM_VSCROLL && lp == 0) { self->onVerticalScroll(LOWORD(wp)); return 0; }
        if (message == WM_ERASEBKGND) {
            RECT client{}; GetClientRect(window, &client);
            FillRect(reinterpret_cast<HDC>(wp), &client, GetSysColorBrush(COLOR_WINDOW));
            return 1;
        }
        if (message == WM_COMMAND) { self->onCommand(LOWORD(wp), HIWORD(wp)); return 0; }
        if (message == WM_NOTIFY && self->onNotify(*reinterpret_cast<NMHDR*>(lp))) return 0;
        if (message == WM_NCDESTROY) {
            const auto original = self->m_originalProc;
            SetWindowLongPtrW(window, GWLP_USERDATA, 0); self->m_window = nullptr;
            return CallWindowProcW(original, window, message, wp, lp);
        }
        return CallWindowProcW(self->m_originalProc, window, message, wp, lp);
    }

    [[nodiscard]] int scaled(int value) const { return MulDiv(value, static_cast<int>(m_dpi), 96); }
    void createFont() {
        if (m_ownedFont) { DeleteObject(m_ownedFont); m_ownedFont = nullptr; }
        NONCLIENTMETRICSW metrics{sizeof(metrics)};
        if (SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0, m_dpi))
            m_ownedFont = CreateFontIndirectW(&metrics.lfMessageFont);
        m_font = m_ownedFont ? m_ownedFont : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    }
    void updateDpi(UINT dpi) {
        const auto oldDpi = std::max(1U, m_dpi);
        m_dpi = dpi ? dpi : 96U;
        m_scrollY = MulDiv(m_scrollY, static_cast<int>(m_dpi), static_cast<int>(oldDpi));
        createFont();
        EnumChildWindows(m_window, [](HWND child, LPARAM font) -> BOOL {
            SendMessageW(child, WM_SETFONT, static_cast<WPARAM>(font), TRUE); return TRUE;
        }, reinterpret_cast<LPARAM>(m_font));
        ListView_SetColumnWidth(m_columnList, 0, scaled(180));
        ListView_SetColumnWidth(m_columnList, 1, scaled(64));
        layoutControls();
        RedrawWindow(m_window, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
    }
    HWND add(const wchar_t* type, const wchar_t* text, DWORD style, int id) {
        auto control = CreateWindowExW(0, type, text, WS_CHILD | WS_VISIBLE | style,
            0, 0, 0, 0, m_window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
            core_api::get_my_instance(), nullptr);
        if (!control) throw exception_win32(GetLastError());
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(m_font), TRUE);
        return control;
    }
    void createControls() {
        m_groupBox = add(L"BUTTON", L"Grouping", BS_GROUPBOX, 0);
        m_groupCombo = add(L"COMBOBOX", nullptr, CBS_DROPDOWN | CBS_AUTOHSCROLL | WS_TABSTOP, groupPreset);
        m_autoCollapse = add(L"BUTTON", L"Auto-collapse", BS_AUTOCHECKBOX | WS_TABSTOP, autoCollapse);
        m_collapseDefault = add(L"BUTTON", L"Collapse groups by default", BS_AUTOCHECKBOX | WS_TABSTOP, collapseDefault);
        m_newGroup = add(L"BUTTON", L"New", BS_PUSHBUTTON | WS_TABSTOP, newGroup);
        m_copyGroup = add(L"BUTTON", L"Duplicate", BS_PUSHBUTTON | WS_TABSTOP, copyGroup);
        m_groupUp = add(L"BUTTON", L"Move up", BS_PUSHBUTTON | WS_TABSTOP, groupUp);
        m_groupDown = add(L"BUTTON", L"Move down", BS_PUSHBUTTON | WS_TABSTOP, groupDown);
        m_deleteGroup = add(L"BUTTON", L"Delete", BS_PUSHBUTTON | WS_TABSTOP, deleteGroup);
        constexpr std::array<const wchar_t*, 7> groupLabels{L"Group key", L"Sort", L"Title left", L"Title right", L"Subtitle left", L"Subtitle right", L"Artwork"};
        for (const auto* label : groupLabels) m_groupLabels.push_back(add(L"STATIC", label, SS_LEFT, 0));
        m_groupEdits = {add(L"EDIT", nullptr, WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, groupKey),
            add(L"EDIT", nullptr, WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, groupSort),
            add(L"EDIT", nullptr, WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, groupLeftPrimary),
            add(L"EDIT", nullptr, WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, groupRightPrimary),
            add(L"EDIT", nullptr, WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, groupLeftSecondary),
            add(L"EDIT", nullptr, WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, groupRightSecondary)};
        m_groupArtwork = add(L"COMBOBOX", nullptr, CBS_DROPDOWNLIST | WS_TABSTOP, groupArtwork);
        SendMessageW(m_groupArtwork, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Front cover"));
        SendMessageW(m_groupArtwork, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Artist artwork"));
        SendMessageW(m_groupArtwork, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Placeholder"));

        m_columnsBox = add(L"BUTTON", L"Columns", BS_GROUPBOX, 0);
        m_columnList = add(WC_LISTVIEWW, nullptr, LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | WS_BORDER | WS_TABSTOP, columnList);
        ListView_SetExtendedListViewStyle(m_columnList, LVS_EX_FULLROWSELECT | LVS_EX_CHECKBOXES | LVS_EX_DOUBLEBUFFER);
        LVCOLUMNW listColumn{LVCF_TEXT | LVCF_WIDTH}; listColumn.cx = scaled(180); listColumn.pszText = const_cast<wchar_t*>(L"Column");
        ListView_InsertColumn(m_columnList, 0, &listColumn); listColumn.cx = scaled(64); listColumn.pszText = const_cast<wchar_t*>(L"Width");
        ListView_InsertColumn(m_columnList, 1, &listColumn);
        m_newColumn = add(L"BUTTON", L"New", BS_PUSHBUTTON | WS_TABSTOP, newColumn);
        m_columnUp = add(L"BUTTON", L"Move up", BS_PUSHBUTTON | WS_TABSTOP, columnUp);
        m_columnDown = add(L"BUTTON", L"Move down", BS_PUSHBUTTON | WS_TABSTOP, columnDown);
        m_copyColumn = add(L"BUTTON", L"Duplicate", BS_PUSHBUTTON | WS_TABSTOP, copyColumn);
        m_deleteColumn = add(L"BUTTON", L"Delete", BS_PUSHBUTTON | WS_TABSTOP, deleteColumn);
        constexpr std::array<const wchar_t*, 6> columnLabels{L"Label", L"Display format", L"Secondary display format", L"Sort format", L"Alignment", L"Width weight"};
        for (const auto* label : columnLabels) m_columnLabels.push_back(add(L"STATIC", label, SS_LEFT, 0));
        m_columnLabel = add(L"EDIT", nullptr, WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, columnLabel);
        m_columnDisplay = add(L"EDIT", nullptr, WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | WS_TABSTOP, columnDisplay);
        m_columnSecondary = add(L"EDIT", nullptr, WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | WS_TABSTOP, columnSecondary);
        m_columnSort = add(L"EDIT", nullptr, WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | WS_TABSTOP, columnSort);
        m_columnAlignment = add(L"COMBOBOX", nullptr, CBS_DROPDOWNLIST | WS_TABSTOP, columnAlignment);
        for (const auto* text : {L"Left", L"Center", L"Right"}) SendMessageW(m_columnAlignment, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text));
        m_columnWidth = add(L"EDIT", nullptr, WS_BORDER | ES_NUMBER | ES_AUTOHSCROLL | WS_TABSTOP, columnWidth);

        m_tabs = add(WC_TABCONTROLW, nullptr, WS_TABSTOP, tabs);
        for (const auto* text : {L"Layout profiles", L"Groups"}) {
            TCITEMW item{TCIF_TEXT}; item.pszText = const_cast<wchar_t*>(text); TabCtrl_InsertItem(m_tabs, TabCtrl_GetItemCount(m_tabs), &item);
        }
        m_profilesBox = add(L"BUTTON", L"Layout profiles", BS_GROUPBOX, 0);
        m_profileList = add(L"LISTBOX", nullptr,
            LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | WS_BORDER | WS_VSCROLL | WS_TABSTOP, profilePreset);
        SendMessageW(m_profileList, LB_SETITEMHEIGHT, 0, scaled(20));
        m_profileNameLabel = add(L"STATIC", L"Profile name:", SS_LEFT, 0);
        m_profileName = add(L"EDIT", nullptr, WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, profileName);
        m_newProfile = add(L"BUTTON", L"New", BS_PUSHBUTTON | WS_TABSTOP, newProfile);
        m_copyProfile = add(L"BUTTON", L"Duplicate", BS_PUSHBUTTON | WS_TABSTOP, copyProfile);
        m_profileUp = add(L"BUTTON", L"Move up", BS_PUSHBUTTON | WS_TABSTOP, profileUp);
        m_profileDown = add(L"BUTTON", L"Move down", BS_PUSHBUTTON | WS_TABSTOP, profileDown);
        m_deleteProfile = add(L"BUTTON", L"Delete", BS_PUSHBUTTON | WS_TABSTOP, deleteProfile);
        m_trackRowsLabel = add(L"STATIC", L"Track row layout:", SS_LEFT, 0);
        m_trackRows = add(L"COMBOBOX", nullptr, CBS_DROPDOWNLIST | WS_TABSTOP, trackRows);
        for (const auto* text : {L"Compact", L"Standard", L"Two-line"}) SendMessageW(m_trackRows, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text));
        m_headerStyleLabel = add(L"STATIC", L"Group header style:", SS_LEFT, 0);
        m_headerStyle = add(L"COMBOBOX", nullptr, CBS_DROPDOWNLIST | WS_TABSTOP, headerStyle);
        for (const auto* text : {L"Detailed", L"Compact line"}) SendMessageW(m_headerStyle, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text));
        m_formatError = add(L"STATIC", L"", SS_LEFT, 0);
    }

    void layoutControls() {
        if (!m_window) return; RECT client{}; GetClientRect(m_window, &client);
        const auto width = std::max(1, static_cast<int>(client.right));
        const auto left = scaled(18); const auto right = std::max(left + 1, width - scaled(18));
        const auto innerWidth = std::max(1, right - left); const auto gap = scaled(6);
        const auto buttonWidth = std::max(1, (innerWidth - gap * 2) / 3);
        const auto placeButtons = [&](const std::array<HWND, 5>& buttons, int firstY) {
            for (std::size_t index = 0; index < buttons.size(); ++index) {
                const auto row = index / 3; const auto column = index % 3;
                MoveWindow(buttons[index], left + static_cast<int>(column) * (buttonWidth + gap),
                    firstY + static_cast<int>(row) * scaled(32), buttonWidth, scaled(26), TRUE);
            }
        };
        const auto contentHeight = selectedTab() == 0 ? scaled(438) : scaled(426);
        const auto viewportHeight = std::max(1, static_cast<int>(client.bottom));
        SCROLLINFO info{sizeof(info), SIF_RANGE | SIF_PAGE | SIF_POS};
        info.nMin = 0; info.nMax = std::max(0, contentHeight - 1);
        info.nPage = static_cast<UINT>(viewportHeight); info.nPos = m_scrollY;
        SetScrollInfo(m_window, SB_VERT, &info, TRUE);
        m_scrollY = std::clamp(m_scrollY, 0, std::max(0, contentHeight - viewportHeight));
        const auto y = [this](int value) { return scaled(value) - m_scrollY; };
        MoveWindow(m_tabs, scaled(6), y(6), std::max(1, width - scaled(12)), scaled(28), TRUE);
        const auto top = y(40);
        const auto panelHeight = selectedTab() == 0 ? scaled(368) : scaled(356);
        const auto errorY = selectedTab() == 0 ? y(412) : y(400);
        MoveWindow(m_formatError, scaled(12), errorY, std::max(1, width - scaled(24)), scaled(22), TRUE);

        MoveWindow(m_profilesBox, scaled(6), top, std::max(1, width - scaled(12)), panelHeight, TRUE);
        MoveWindow(m_profileList, left, top + scaled(22), innerWidth, scaled(102), TRUE);
        const std::array<HWND, 5> profileButtons{m_newProfile, m_copyProfile, m_profileUp, m_profileDown, m_deleteProfile};
        placeButtons(profileButtons, top + scaled(130));
        MoveWindow(m_profileNameLabel, left, top + scaled(200), scaled(104), scaled(22), TRUE);
        MoveWindow(m_profileName, left + scaled(110), top + scaled(196), std::max(1, innerWidth - scaled(110)), scaled(26), TRUE);
        MoveWindow(m_trackRowsLabel, left, top + scaled(232), scaled(132), scaled(22), TRUE);
        MoveWindow(m_trackRows, left + scaled(138), top + scaled(228), std::max(1, innerWidth - scaled(138)), scaled(180), TRUE);
        MoveWindow(m_headerStyleLabel, left, top + scaled(264), scaled(132), scaled(22), TRUE);
        MoveWindow(m_headerStyle, left + scaled(138), top + scaled(260), std::max(1, innerWidth - scaled(138)), scaled(180), TRUE);
        MoveWindow(m_autoCollapse, left, top + scaled(298), innerWidth, scaled(22), TRUE);
        MoveWindow(m_collapseDefault, left, top + scaled(326), innerWidth, scaled(22), TRUE);

        const auto panelTop = top;
        MoveWindow(m_groupBox, scaled(6), panelTop, std::max(1, width - scaled(12)), panelHeight, TRUE);
        MoveWindow(m_groupCombo, left, panelTop + scaled(20), innerWidth, scaled(220), TRUE);
        const std::array<HWND, 5> groupButtons{m_newGroup, m_copyGroup, m_groupUp, m_groupDown, m_deleteGroup};
        placeButtons(groupButtons, panelTop + scaled(50));
        for (std::size_t index = 0; index < m_groupLabels.size(); ++index) {
            const auto fieldY = panelTop + scaled(114 + static_cast<int>(index) * 34);
            MoveWindow(m_groupLabels[index], left, fieldY, scaled(112), scaled(20), TRUE);
            const auto control = index < m_groupEdits.size() ? m_groupEdits[index] : m_groupArtwork;
            const auto controlLeft = left + scaled(118);
            MoveWindow(control, controlLeft, fieldY - scaled(3), std::max(1, right - controlLeft),
                index < m_groupEdits.size() ? scaled(25) : scaled(180), TRUE);
        }
        MoveWindow(m_columnsBox, scaled(6), panelTop, std::max(1, width - scaled(12)), panelHeight, TRUE);
        MoveWindow(m_columnList, left, panelTop + scaled(22), innerWidth, scaled(96), TRUE);
        const std::array<HWND, 5> columnButtons{m_newColumn, m_copyColumn, m_columnUp, m_columnDown, m_deleteColumn};
        placeButtons(columnButtons, panelTop + scaled(124));
        const std::array<HWND, 6> fields{m_columnLabel, m_columnDisplay, m_columnSecondary, m_columnSort, m_columnAlignment, m_columnWidth};
        const std::array<int, 6> labelY{190, 226, 292, 358, 424, 460};
        for (std::size_t index = 0; index < fields.size(); ++index) {
            const auto fieldY = panelTop + scaled(labelY[index]);
            if (index == 0 || index >= 4) {
                MoveWindow(m_columnLabels[index], left, fieldY, scaled(170), scaled(20), TRUE);
                const auto fieldLeft = left + scaled(176);
                MoveWindow(fields[index], fieldLeft, fieldY - scaled(3), std::max(1, right - fieldLeft),
                    index == 4 ? scaled(160) : scaled(25), TRUE);
            } else {
                MoveWindow(m_columnLabels[index], left, fieldY, innerWidth, scaled(20), TRUE);
                MoveWindow(fields[index], left, fieldY + scaled(20), innerWidth, scaled(42), TRUE);
            }
        }
        updateTabVisibility();
    }

    void scrollBy(int delta) {
        RECT client{}; GetClientRect(m_window, &client);
        const auto contentHeight = selectedTab() == 0 ? scaled(438) : scaled(426);
        const auto maximum = std::max(0, contentHeight - static_cast<int>(client.bottom));
        const auto next = std::clamp(m_scrollY + delta, 0, maximum);
        if (next == m_scrollY) return;
        m_scrollY = next; layoutControls(); InvalidateRect(m_window, nullptr, TRUE);
    }

    void onVerticalScroll(int action) {
        SCROLLINFO info{sizeof(info), SIF_ALL}; GetScrollInfo(m_window, SB_VERT, &info);
        auto next = m_scrollY;
        if (action == SB_LINEUP) next -= scaled(24);
        else if (action == SB_LINEDOWN) next += scaled(24);
        else if (action == SB_PAGEUP) next -= static_cast<int>(info.nPage);
        else if (action == SB_PAGEDOWN) next += static_cast<int>(info.nPage);
        else if (action == SB_THUMBTRACK || action == SB_THUMBPOSITION) next = info.nTrackPos;
        scrollBy(next - m_scrollY);
    }

    [[nodiscard]] std::size_t selectedTab() const noexcept {
        const auto selected = TabCtrl_GetCurSel(m_tabs);
        return static_cast<std::size_t>(std::clamp(selected, 0, 1));
    }

    void updateTabVisibility() const {
        const auto selected = selectedTab();
        const auto setVisible = [](HWND control, bool visible) {
            SetWindowPos(control, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER
                | SWP_NOACTIVATE | (visible ? SWP_SHOWWINDOW : SWP_HIDEWINDOW));
        };
        const auto profileControls = {m_profilesBox, m_profileList, m_profileNameLabel, m_profileName,
            m_newProfile, m_copyProfile, m_profileUp, m_profileDown, m_deleteProfile, m_trackRowsLabel, m_trackRows,
            m_headerStyleLabel, m_headerStyle, m_autoCollapse, m_collapseDefault};
        const auto groupControls = {m_groupBox, m_groupCombo, m_newGroup, m_copyGroup, m_groupUp,
            m_groupDown, m_deleteGroup, m_groupArtwork};
        const auto columnControls = {m_columnsBox, m_columnList, m_newColumn, m_columnUp,
            m_columnDown, m_copyColumn, m_deleteColumn, m_columnLabel, m_columnDisplay,
            m_columnSecondary, m_columnSort, m_columnAlignment, m_columnWidth};
        for (const auto control : profileControls) setVisible(control, false);
        for (const auto control : groupControls) setVisible(control, false);
        for (const auto control : m_groupLabels) setVisible(control, false);
        for (const auto control : m_groupEdits) setVisible(control, false);
        for (const auto control : columnControls) setVisible(control, false);
        for (const auto control : m_columnLabels) setVisible(control, false);
        if (selected == 0) for (const auto control : profileControls) setVisible(control, true);
        if (selected == 1) {
            for (const auto control : groupControls) setVisible(control, true);
            for (const auto control : m_groupLabels) setVisible(control, true);
            for (const auto control : m_groupEdits) setVisible(control, true);
        }
    }

    [[nodiscard]] std::wstring editText(HWND edit) const {
        const auto length = GetWindowTextLengthW(edit);
        std::wstring value(static_cast<std::size_t>(length) + 1, L'\0');
        if (length) GetWindowTextW(edit, value.data(), length + 1);
        value.resize(static_cast<std::size_t>(length));
        return value;
    }
    void setEdit(HWND edit, const std::string& value) { SetWindowTextW(edit, utf8ToWide(value).c_str()); }

    void rebuildGroupCombo() {
        SendMessageW(m_groupCombo, CB_RESETCONTENT, 0, 0);
        for (const auto& group : m_draft.groups) {
            const auto label = utf8ToWide(group.label); SendMessageW(m_groupCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
        }
    }
    [[nodiscard]] std::string activePlaylistGuid() const {
        const auto api = playlist_manager_v5::get(); const auto playlist = api->get_active_playlist();
        if (playlist == SIZE_MAX) return {};
        wchar_t text[40]{}; if (!StringFromGUID2(api->playlist_get_guid(playlist), text, 40)) return {};
        return wideToUtf8(text);
    }
    [[nodiscard]] std::size_t assignedProfileIndex() const {
        const auto guid = activePlaylistGuid();
        const auto assignment = std::find_if(m_draft.assignments.begin(), m_draft.assignments.end(), [&](const auto& value) { return value.playlistGuid == guid; });
        const auto id = assignment == m_draft.assignments.end() ? std::string{"builtin.default"} : assignment->profileId;
        const auto profile = std::find_if(m_draft.profiles.begin(), m_draft.profiles.end(), [&](const auto& value) { return value.id == id; });
        return profile == m_draft.profiles.end() ? 0 : static_cast<std::size_t>(profile - m_draft.profiles.begin());
    }
    void rebuildProfileCombo() {
        m_updating = true; SendMessageW(m_profileList, LB_RESETCONTENT, 0, 0);
        for (const auto& profile : m_draft.profiles) {
            const auto label = utf8ToWide(profile.label);
            SendMessageW(m_profileList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
        }
        m_updating = false;
    }
    void storeProfile() {
        if (m_updating || m_profileIndex >= m_draft.profiles.size()) return;
        auto& profile = m_draft.profiles[m_profileIndex];
        if (!profile.builtIn) profile.label = wideToUtf8(editText(m_profileName));
        profile.groupId = m_draft.activeGroupId; profile.autoCollapse = m_draft.autoCollapse;
        profile.collapseByDefault = m_draft.collapseByDefault;
        const auto rows = SendMessageW(m_trackRows, CB_GETCURSEL, 0, 0);
        profile.trackRowLayout = rows >= 0 && rows <= 2 ? static_cast<TrackRowLayout>(rows) : TrackRowLayout::standard;
        profile.groupHeaderStyle = SendMessageW(m_headerStyle, CB_GETCURSEL, 0, 0) == 1
            ? GroupHeaderStyle::compactLine : GroupHeaderStyle::detailed;
        profile.columns.clear(); for (const auto& column : m_draft.columns)
            profile.columns.push_back({column.id, column.visible, column.widthWeight});
        const auto guid = activePlaylistGuid(); if (!guid.empty()) {
            std::erase_if(m_draft.assignments, [&](const auto& value) { return value.playlistGuid == guid; });
            if (profile.id != "builtin.default") m_draft.assignments.push_back({guid, profile.id});
        }
    }
    void loadProfile(std::size_t index) {
        if (index >= m_draft.profiles.size()) return; m_updating = true; m_profileIndex = index;
        SendMessageW(m_profileList, LB_SETCURSEL, static_cast<WPARAM>(index), 0);
        const auto& profile = m_draft.profiles[index];
        SetWindowTextW(m_profileName, utf8ToWide(profile.label).c_str());
        SendMessageW(m_profileName, EM_SETREADONLY, profile.builtIn, 0);
        const auto effective = applyLayoutProfile(m_draft, profile.id);
        m_draft.activeGroupId = effective.activeGroupId; m_draft.autoCollapse = effective.autoCollapse;
        m_draft.collapseByDefault = effective.collapseByDefault; m_draft.columns = effective.columns;
        SendMessageW(m_trackRows, CB_SETCURSEL, static_cast<WPARAM>(profile.trackRowLayout), 0);
        SendMessageW(m_headerStyle, CB_SETCURSEL, static_cast<WPARAM>(profile.groupHeaderStyle), 0);
        EnableWindow(m_deleteProfile, !profile.builtIn); m_updating = false;
    }
    void rebuildColumnList() {
        m_updating = true; ListView_DeleteAllItems(m_columnList);
        for (std::size_t index = 0; index < m_draft.columns.size(); ++index) {
            const auto name = utf8ToWide(m_draft.columns[index].label); LVITEMW item{LVIF_TEXT};
            item.iItem = static_cast<int>(index); item.pszText = const_cast<wchar_t*>(name.c_str()); ListView_InsertItem(m_columnList, &item);
            const auto width = std::to_wstring(m_draft.columns[index].widthWeight); ListView_SetItemText(m_columnList, static_cast<int>(index), 1, const_cast<wchar_t*>(width.c_str()));
            ListView_SetCheckState(m_columnList, static_cast<int>(index), m_draft.columns[index].visible);
        }
        m_updating = false;
    }
    void captureControls() {
        storeGroup(); storeColumn();
        m_draft.autoCollapse = SendMessageW(m_autoCollapse, BM_GETCHECK, 0, 0) == BST_CHECKED;
        m_draft.collapseByDefault = SendMessageW(m_collapseDefault, BM_GETCHECK, 0, 0) == BST_CHECKED;
        for (std::size_t index = 0; index < m_draft.columns.size(); ++index)
            m_draft.columns[index].visible = ListView_GetCheckState(m_columnList, static_cast<int>(index)) != FALSE;
        storeProfile();
    }
    void storeGroup() {
        if (m_updating || m_groupIndex >= m_draft.groups.size()) return; auto& group = m_draft.groups[m_groupIndex];
        if (!group.builtIn) {
            group.keyFormat = wideToUtf8(editText(m_groupEdits[0])); group.sortFormat = wideToUtf8(editText(m_groupEdits[1]));
            group.leftPrimary = wideToUtf8(editText(m_groupEdits[2])); group.rightPrimary = wideToUtf8(editText(m_groupEdits[3]));
            group.leftSecondary = wideToUtf8(editText(m_groupEdits[4])); group.rightSecondary = wideToUtf8(editText(m_groupEdits[5]));
            group.artwork = static_cast<GroupArtworkSource>(std::max<LRESULT>(0, SendMessageW(m_groupArtwork, CB_GETCURSEL, 0, 0)));
        }
    }
    void loadGroup(std::size_t index) {
        if (index >= m_draft.groups.size()) return; m_updating = true; m_groupIndex = index; const auto& group = m_draft.groups[index];
        SendMessageW(m_groupCombo, CB_SETCURSEL, index, 0); SendMessageW(m_autoCollapse, BM_SETCHECK, m_draft.autoCollapse ? BST_CHECKED : BST_UNCHECKED, 0);
        if (const auto comboEdit = GetWindow(m_groupCombo, GW_CHILD)) SendMessageW(comboEdit, EM_SETREADONLY, group.builtIn, 0);
        SendMessageW(m_collapseDefault, BM_SETCHECK, m_draft.collapseByDefault ? BST_CHECKED : BST_UNCHECKED, 0);
        const std::array<std::string, 6> values{group.keyFormat, group.sortFormat, group.leftPrimary, group.rightPrimary, group.leftSecondary, group.rightSecondary};
        for (std::size_t field = 0; field < values.size(); ++field) { setEdit(m_groupEdits[field], values[field]); SendMessageW(m_groupEdits[field], EM_SETREADONLY, group.builtIn, 0); }
        SendMessageW(m_groupArtwork, CB_SETCURSEL, static_cast<int>(group.artwork), 0); EnableWindow(m_groupArtwork, !group.builtIn); EnableWindow(m_deleteGroup, !group.builtIn);
        m_updating = false;
    }
    void storeColumn() {
        if (m_updating || m_columnIndex >= m_draft.columns.size()) return; auto& column = m_draft.columns[m_columnIndex];
        if (!column.builtIn) {
            column.label = wideToUtf8(editText(m_columnLabel)); column.displayFormat = wideToUtf8(editText(m_columnDisplay));
            column.secondaryDisplayFormat = wideToUtf8(editText(m_columnSecondary));
            column.sortFormat = wideToUtf8(editText(m_columnSort)); column.alignment = static_cast<ColumnAlignment>(std::max<LRESULT>(0, SendMessageW(m_columnAlignment, CB_GETCURSEL, 0, 0)));
        }
        const auto width = _wtoi(editText(m_columnWidth).c_str()); if (width > 0) column.widthWeight = width;
    }
    void loadColumn(std::size_t index) {
        if (index >= m_draft.columns.size()) return; m_updating = true; m_columnIndex = index; const auto& column = m_draft.columns[index];
        ListView_SetItemState(m_columnList, static_cast<int>(index), LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        setEdit(m_columnLabel, column.label); setEdit(m_columnDisplay, column.displayFormat);
        setEdit(m_columnSecondary, column.secondaryDisplayFormat); setEdit(m_columnSort, column.sortFormat); SetWindowTextW(m_columnWidth, std::to_wstring(column.widthWeight).c_str());
        SendMessageW(m_columnAlignment, CB_SETCURSEL, static_cast<int>(column.alignment), 0);
        for (const auto edit : {m_columnLabel, m_columnDisplay, m_columnSecondary, m_columnSort}) SendMessageW(edit, EM_SETREADONLY, column.builtIn, 0);
        EnableWindow(m_columnAlignment, !column.builtIn); EnableWindow(m_deleteColumn, !column.builtIn && !column.cover); m_updating = false;
    }

    [[nodiscard]] bool validateFormats() {
        SetWindowTextW(m_formatError, L"");
        auto compiler = titleformat_compiler::get(); titleformat_object::ptr script;
        auto valid = [&](const std::string& text, bool allowEmpty) { return (allowEmpty && text.empty()) || compiler->compile(script, text.c_str()); };
        for (const auto& group : m_draft.groups) {
            if (group.label.empty()) {
                SetWindowTextW(m_formatError, L"Group error: a preset name is empty."); return false;
            }
            if (!valid(group.keyFormat, false) || !valid(group.sortFormat, false) || !valid(group.leftPrimary, true)
                || !valid(group.rightPrimary, true) || !valid(group.leftSecondary, true) || !valid(group.rightSecondary, true)) {
                SetWindowTextW(m_formatError, L"Group error: a title-format expression is invalid."); TabCtrl_SetCurSel(m_tabs, 1); updateTabVisibility(); return false;
            }
        }
        for (const auto& column : m_draft.columns) {
            if (column.label.empty()) {
                SetWindowTextW(m_formatError, L"Column error: a label is empty."); return false;
            }
            if ((!column.cover && column.id != "builtin.state" && !valid(column.displayFormat, false))
                || !valid(column.secondaryDisplayFormat, true) || !valid(column.sortFormat, true)) {
                SetWindowTextW(m_formatError, L"Stored column format is invalid; reset the page to repair it."); return false;
            }
        }
        return true;
    }
    void onCommand(int id, int notification) {
        if (m_updating) return;
        if (id == profilePreset && notification == LBN_SELCHANGE) {
            const auto selected = SendMessageW(m_profileList, LB_GETCURSEL, 0, 0);
            if (selected != LB_ERR) {
                storeProfile(); rebuildProfileCombo(); loadProfile(static_cast<std::size_t>(selected));
                rebuildGroupCombo(); rebuildColumnList(); loadGroup(0); loadColumn(0);
            }
        }
        else if (id == profileName && notification == EN_CHANGE) notifyChanged();
        else if (id == newProfile && notification == BN_CLICKED) { storeProfile(); LayoutProfile profile; profile.id = "custom.profile." + std::to_string(GetTickCount64()); profile.label = "New profile"; profile.groupId = m_draft.activeGroupId; for (const auto& column : m_draft.columns) profile.columns.push_back({column.id, column.visible, column.widthWeight}); m_draft.profiles.push_back(std::move(profile)); rebuildProfileCombo(); loadProfile(m_draft.profiles.size() - 1); }
        else if (id == copyProfile && notification == BN_CLICKED && m_profileIndex < m_draft.profiles.size()) { storeProfile(); auto copy = m_draft.profiles[m_profileIndex]; copy.id = "custom.profile." + std::to_string(GetTickCount64()); copy.label += " (copy)"; copy.builtIn = false; m_draft.profiles.push_back(std::move(copy)); rebuildProfileCombo(); loadProfile(m_draft.profiles.size() - 1); }
        else if ((id == profileUp || id == profileDown) && m_profileIndex < m_draft.profiles.size()) { storeProfile(); const auto target = static_cast<std::ptrdiff_t>(m_profileIndex) + (id == profileUp ? -1 : 1); if (target >= 0 && target < static_cast<std::ptrdiff_t>(m_draft.profiles.size())) { std::swap(m_draft.profiles[m_profileIndex], m_draft.profiles[static_cast<std::size_t>(target)]); m_profileIndex = static_cast<std::size_t>(target); rebuildProfileCombo(); loadProfile(m_profileIndex); } }
        else if (id == deleteProfile && notification == BN_CLICKED && m_profileIndex < m_draft.profiles.size() && !m_draft.profiles[m_profileIndex].builtIn) { if (MessageBoxW(m_window, L"Delete this layout profile? Assigned playlists will use Default.", L"Refrain", MB_YESNO | MB_ICONWARNING) == IDYES) { const auto removed = m_draft.profiles[m_profileIndex].id; std::erase_if(m_draft.assignments, [&](const auto& a){ return a.profileId == removed; }); m_draft.profiles.erase(m_draft.profiles.begin() + static_cast<std::ptrdiff_t>(m_profileIndex)); rebuildProfileCombo(); loadProfile(0); } }
        else if (id == groupPreset && notification == CBN_SELCHANGE) { storeGroup(); const auto index = static_cast<std::size_t>(SendMessageW(m_groupCombo, CB_GETCURSEL, 0, 0)); m_draft.activeGroupId = m_draft.groups[index].id; loadGroup(index); }
        else if (id == newGroup && notification == BN_CLICKED) { auto group = defaultGroupDefinitions().front(); group.id = "custom.group." + std::to_string(GetTickCount64()); group.label = "New group"; group.builtIn = false; m_draft.groups.push_back(std::move(group)); rebuildGroupCombo(); loadGroup(m_draft.groups.size() - 1); }
        else if (id == copyGroup && notification == BN_CLICKED) { storeGroup(); auto copy = m_draft.groups[m_groupIndex]; copy.id = "custom.group." + std::to_string(GetTickCount64()); copy.label += " (copy)"; copy.builtIn = false; m_draft.groups.push_back(std::move(copy)); m_draft.activeGroupId = m_draft.groups.back().id; rebuildGroupCombo(); loadGroup(m_draft.groups.size() - 1); }
        else if ((id == groupUp || id == groupDown) && m_groupIndex < m_draft.groups.size()) { storeGroup(); const auto target = static_cast<std::ptrdiff_t>(m_groupIndex) + (id == groupUp ? -1 : 1); if (target >= 0 && target < static_cast<std::ptrdiff_t>(m_draft.groups.size())) { std::swap(m_draft.groups[m_groupIndex], m_draft.groups[static_cast<std::size_t>(target)]); m_groupIndex = static_cast<std::size_t>(target); rebuildGroupCombo(); loadGroup(m_groupIndex); } }
        else if (id == deleteGroup && notification == BN_CLICKED && m_groupIndex < m_draft.groups.size() && !m_draft.groups[m_groupIndex].builtIn) { if (MessageBoxW(m_window, L"Delete this group definition?", L"Refrain", MB_YESNO | MB_ICONWARNING) == IDYES) { const auto removed = m_draft.groups[m_groupIndex].id; m_draft.groups.erase(m_draft.groups.begin() + static_cast<std::ptrdiff_t>(m_groupIndex)); if (m_draft.activeGroupId == removed) m_draft.activeGroupId = "builtin.album.simple"; for (auto& p : m_draft.profiles) if (p.groupId == removed) p.groupId = "builtin.album.simple"; rebuildGroupCombo(); loadGroup(0); } }
        else if ((id == autoCollapse || id == collapseDefault) && notification == BN_CLICKED) notifyChanged();
        else if (id == groupPreset && notification == CBN_EDITCHANGE) {
            if (m_groupIndex < m_draft.groups.size() && !m_draft.groups[m_groupIndex].builtIn)
                m_draft.groups[m_groupIndex].label = wideToUtf8(editText(m_groupCombo));
            notifyChanged();
        }
        else if (id == newColumn && notification == BN_CLICKED) { ColumnDefinition column{"custom.column." + std::to_string(GetTickCount64()), "New column", "$if2(%title%,$filename_ext(%path%))", "", "", ColumnAlignment::leading, true, 800, false, false}; m_draft.columns.push_back(std::move(column)); rebuildColumnList(); loadColumn(m_draft.columns.size() - 1); }
        else if (id == columnUp || id == columnDown) { storeColumn(); const auto direction = id == columnUp ? -1 : 1; const auto target = static_cast<std::ptrdiff_t>(m_columnIndex) + direction; if (target >= 0 && target < static_cast<std::ptrdiff_t>(m_draft.columns.size()) && !m_draft.columns[m_columnIndex].cover && !m_draft.columns[static_cast<std::size_t>(target)].cover) { std::swap(m_draft.columns[m_columnIndex], m_draft.columns[static_cast<std::size_t>(target)]); m_columnIndex = static_cast<std::size_t>(target); rebuildColumnList(); loadColumn(m_columnIndex); } }
        else if (id == copyColumn && notification == BN_CLICKED) { storeColumn(); auto copy = m_draft.columns[m_columnIndex]; copy.id = "custom.column." + std::to_string(GetTickCount64()); copy.label += " (copy)"; copy.builtIn = false; copy.cover = false; m_draft.columns.push_back(std::move(copy)); rebuildColumnList(); loadColumn(m_draft.columns.size() - 1); }
        else if (id == deleteColumn && notification == BN_CLICKED && m_columnIndex < m_draft.columns.size() && !m_draft.columns[m_columnIndex].builtIn) { if (MessageBoxW(m_window, L"Delete this column definition?", L"Refrain", MB_YESNO | MB_ICONWARNING) == IDYES) { const auto removed = m_draft.columns[m_columnIndex].id; m_draft.columns.erase(m_draft.columns.begin() + static_cast<std::ptrdiff_t>(m_columnIndex)); for (auto& p : m_draft.profiles) std::erase_if(p.columns, [&](const auto& c){ return c.columnId == removed; }); rebuildColumnList(); loadColumn(0); } }
        else if (notification == EN_CHANGE || notification == CBN_SELCHANGE) notifyChanged();
    }
    bool onNotify(const NMHDR& header) {
        if (header.hwndFrom == m_tabs && header.code == TCN_SELCHANGE) {
            m_scrollY = 0; updateTabVisibility(); layoutControls();
            RedrawWindow(m_window, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
            return true;
        }
        if (header.hwndFrom != m_columnList || m_updating || header.code != LVN_ITEMCHANGED) return false;
        const auto* change = reinterpret_cast<const NMLISTVIEW*>(&header);
        if ((change->uNewState & LVIS_SELECTED) != 0 && change->iItem >= 0 && static_cast<std::size_t>(change->iItem) != m_columnIndex) { storeColumn(); loadColumn(static_cast<std::size_t>(change->iItem)); }
        notifyChanged(); return true;
    }
    void notifyChanged() const { if (m_callback.is_valid()) m_callback->on_state_changed(); }

    preferences_page_callback::ptr m_callback; PlaylistViewSettings m_initial, m_draft;
    HWND m_window{}; WNDPROC m_originalProc{}; HFONT m_font{}, m_ownedFont{}; UINT m_dpi{96}; bool m_updating{};
    std::size_t m_profileIndex{static_cast<std::size_t>(-1)}, m_groupIndex{static_cast<std::size_t>(-1)}, m_columnIndex{static_cast<std::size_t>(-1)};
    HWND m_profilesBox{}, m_profileList{}, m_profileNameLabel{}, m_profileName{}, m_newProfile{}, m_copyProfile{}, m_profileUp{}, m_profileDown{}, m_deleteProfile{};
    HWND m_tabs{};
    HWND m_formatError{};
    HWND m_trackRowsLabel{}, m_trackRows{}, m_headerStyleLabel{}, m_headerStyle{};
    HWND m_groupBox{}, m_groupCombo{}, m_autoCollapse{}, m_collapseDefault{}, m_newGroup{}, m_copyGroup{}, m_groupUp{}, m_groupDown{}, m_deleteGroup{}, m_groupArtwork{};
    std::vector<HWND> m_groupLabels; std::array<HWND, 6> m_groupEdits{};
    HWND m_columnsBox{}, m_columnList{}, m_newColumn{}, m_columnUp{}, m_columnDown{}, m_copyColumn{}, m_deleteColumn{};
    std::vector<HWND> m_columnLabels; HWND m_columnLabel{}, m_columnDisplay{}, m_columnSecondary{}, m_columnSort{}, m_columnAlignment{}, m_columnWidth{};
    int m_scrollY{};
};

class PlaylistSettingsPage : public preferences_page_v3 {
public:
    const char* get_name() override { return "Playlist View"; }
    GUID get_guid() override { return kPlaylistPreferencesPageGuid; }
    GUID get_parent_guid() override { return kPreferencesPageGuid; }
    preferences_page_instance::ptr instantiate(HWND parent, preferences_page_callback::ptr callback) override {
        return fb2k::service_new<PlaylistSettingsPageInstance>(parent, std::move(callback));
    }
};

preferences_page_factory_t<PlaylistSettingsPage> g_playlistSettingsPageFactory;

} // namespace
} // namespace refrain

#include "settings.h"

#include <windows.h>
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
    groupPreset = 4001, autoCollapse, collapseDefault, copyGroup, deleteGroup,
    groupKey, groupSort, groupLeftPrimary, groupRightPrimary, groupLeftSecondary,
    groupRightSecondary, groupArtwork, columnList, columnUp, columnDown, copyColumn,
    deleteColumn, columnLabel, columnDisplay, columnSort, columnAlignment, columnWidth,
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
            WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
            0, 0, 0, 0, parent, nullptr, core_api::get_my_instance(), nullptr);
        if (!m_window) throw exception_win32(GetLastError());
        SetWindowLongPtrW(m_window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
        m_originalProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(m_window, GWLP_WNDPROC,
            reinterpret_cast<LONG_PTR>(&PlaylistSettingsPageInstance::windowProc)));
        m_dpi = GetDpiForWindow(m_window);
        createFont();
        createControls();
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
        if (groupingChanged) {
            auto api = playlist_manager::get();
            const auto playlist = api->get_active_playlist();
            if (playlist != SIZE_MAX && api->playlist_get_item_count(playlist) > 1) {
                if (api->playlist_lock_is_present(playlist)
                    && (api->playlist_lock_get_filter_mask(playlist) & playlist_lock::filter_reorder) != 0) {
                    MessageBoxW(m_window, L"The active playlist does not allow reordering, so the grouping change was not applied.",
                        L"Refrain", MB_OK | MB_ICONINFORMATION);
                    return;
                }
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
        m_groupIndex = m_columnIndex = static_cast<std::size_t>(-1);
        rebuildGroupCombo(); rebuildColumnList(); loadGroup(0); loadColumn(0);
        notifyChanged();
    }

private:
    static LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wp, LPARAM lp) {
        auto* self = reinterpret_cast<PlaylistSettingsPageInstance*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (!self || !self->m_originalProc) return DefWindowProcW(window, message, wp, lp);
        if (message == WM_SIZE) { self->layoutControls(); return 0; }
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
        NONCLIENTMETRICSW metrics{sizeof(metrics)};
        if (SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0, m_dpi))
            m_ownedFont = CreateFontIndirectW(&metrics.lfMessageFont);
        m_font = m_ownedFont ? m_ownedFont : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
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
        m_copyGroup = add(L"BUTTON", L"Copy", BS_PUSHBUTTON | WS_TABSTOP, copyGroup);
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
        m_columnUp = add(L"BUTTON", L"Up", BS_PUSHBUTTON | WS_TABSTOP, columnUp);
        m_columnDown = add(L"BUTTON", L"Down", BS_PUSHBUTTON | WS_TABSTOP, columnDown);
        m_copyColumn = add(L"BUTTON", L"Copy", BS_PUSHBUTTON | WS_TABSTOP, copyColumn);
        m_deleteColumn = add(L"BUTTON", L"Delete", BS_PUSHBUTTON | WS_TABSTOP, deleteColumn);
        constexpr std::array<const wchar_t*, 5> columnLabels{L"Label", L"Display format", L"Sort format", L"Alignment", L"Width weight"};
        for (const auto* label : columnLabels) m_columnLabels.push_back(add(L"STATIC", label, SS_LEFT, 0));
        m_columnLabel = add(L"EDIT", nullptr, WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, columnLabel);
        m_columnDisplay = add(L"EDIT", nullptr, WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, columnDisplay);
        m_columnSort = add(L"EDIT", nullptr, WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, columnSort);
        m_columnAlignment = add(L"COMBOBOX", nullptr, CBS_DROPDOWNLIST | WS_TABSTOP, columnAlignment);
        for (const auto* text : {L"Left", L"Center", L"Right"}) SendMessageW(m_columnAlignment, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text));
        m_columnWidth = add(L"EDIT", nullptr, WS_BORDER | ES_NUMBER | ES_AUTOHSCROLL | WS_TABSTOP, columnWidth);
    }

    void layoutControls() const {
        if (!m_window) return; RECT client{}; GetClientRect(m_window, &client);
        const auto width = static_cast<int>(client.right); const auto half = std::max(scaled(280), width / 2);
        MoveWindow(m_groupBox, scaled(6), scaled(6), half - scaled(9), scaled(430), TRUE);
        MoveWindow(m_groupCombo, scaled(18), scaled(28), half - scaled(166), scaled(220), TRUE);
        MoveWindow(m_copyGroup, half - scaled(140), scaled(28), scaled(58), scaled(26), TRUE);
        MoveWindow(m_deleteGroup, half - scaled(76), scaled(28), scaled(58), scaled(26), TRUE);
        MoveWindow(m_autoCollapse, scaled(18), scaled(62), scaled(130), scaled(22), TRUE);
        MoveWindow(m_collapseDefault, scaled(154), scaled(62), scaled(190), scaled(22), TRUE);
        for (std::size_t index = 0; index < m_groupLabels.size(); ++index) {
            const auto y = scaled(94 + static_cast<int>(index) * 44);
            MoveWindow(m_groupLabels[index], scaled(18), y, scaled(92), scaled(20), TRUE);
            const auto control = index < m_groupEdits.size() ? m_groupEdits[index] : m_groupArtwork;
            MoveWindow(control, scaled(112), y - scaled(3), half - scaled(130), index < m_groupEdits.size() ? scaled(25) : scaled(180), TRUE);
        }
        const auto left = half + scaled(3); const auto rightWidth = std::max(scaled(280), width - left - scaled(6));
        MoveWindow(m_columnsBox, left, scaled(6), rightWidth, scaled(430), TRUE);
        MoveWindow(m_columnList, left + scaled(12), scaled(28), rightWidth - scaled(104), scaled(196), TRUE);
        const auto buttonsX = left + rightWidth - scaled(84);
        for (const auto [control, y] : std::array<std::pair<HWND, int>, 4>{{{m_columnUp, 28}, {m_columnDown, 62}, {m_copyColumn, 116}, {m_deleteColumn, 150}}})
            MoveWindow(control, buttonsX, scaled(y), scaled(70), scaled(27), TRUE);
        const std::array<HWND, 5> fields{m_columnLabel, m_columnDisplay, m_columnSort, m_columnAlignment, m_columnWidth};
        for (std::size_t index = 0; index < fields.size(); ++index) {
            const auto y = scaled(238 + static_cast<int>(index) * 36);
            MoveWindow(m_columnLabels[index], left + scaled(12), y, scaled(100), scaled(20), TRUE);
            MoveWindow(fields[index], left + scaled(116), y - scaled(3), rightWidth - scaled(130), index == 3 ? scaled(160) : scaled(25), TRUE);
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
            column.sortFormat = wideToUtf8(editText(m_columnSort)); column.alignment = static_cast<ColumnAlignment>(std::max<LRESULT>(0, SendMessageW(m_columnAlignment, CB_GETCURSEL, 0, 0)));
        }
        const auto width = _wtoi(editText(m_columnWidth).c_str()); if (width > 0) column.widthWeight = width;
    }
    void loadColumn(std::size_t index) {
        if (index >= m_draft.columns.size()) return; m_updating = true; m_columnIndex = index; const auto& column = m_draft.columns[index];
        ListView_SetItemState(m_columnList, static_cast<int>(index), LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        setEdit(m_columnLabel, column.label); setEdit(m_columnDisplay, column.displayFormat); setEdit(m_columnSort, column.sortFormat); SetWindowTextW(m_columnWidth, std::to_wstring(column.widthWeight).c_str());
        SendMessageW(m_columnAlignment, CB_SETCURSEL, static_cast<int>(column.alignment), 0);
        for (const auto edit : {m_columnLabel, m_columnDisplay, m_columnSort}) SendMessageW(edit, EM_SETREADONLY, column.builtIn, 0);
        EnableWindow(m_columnAlignment, !column.builtIn); EnableWindow(m_deleteColumn, !column.builtIn && !column.cover); m_updating = false;
    }

    [[nodiscard]] bool validateFormats() const {
        auto compiler = titleformat_compiler::get(); titleformat_object::ptr script;
        auto valid = [&](const std::string& text, bool allowEmpty) { return (allowEmpty && text.empty()) || compiler->compile(script, text.c_str()); };
        for (const auto& group : m_draft.groups) {
            if (group.label.empty()) {
                MessageBoxW(m_window, L"A grouping preset name is empty.", L"Refrain", MB_OK | MB_ICONWARNING); return false;
            }
            if (!valid(group.keyFormat, false) || !valid(group.sortFormat, false) || !valid(group.leftPrimary, true)
                || !valid(group.rightPrimary, true) || !valid(group.leftSecondary, true) || !valid(group.rightSecondary, true)) {
                MessageBoxW(m_window, L"A grouping title-format expression is invalid. Fix it before applying.", L"Refrain", MB_OK | MB_ICONWARNING); return false;
            }
        }
        for (const auto& column : m_draft.columns) {
            if (column.label.empty()) {
                MessageBoxW(m_window, L"A column label is empty.", L"Refrain", MB_OK | MB_ICONWARNING); return false;
            }
            if ((!column.cover && column.id != "builtin.state" && !valid(column.displayFormat, false))
                || !valid(column.sortFormat, true)) {
                MessageBoxW(m_window, L"A column title-format expression is invalid. Fix it before applying.", L"Refrain", MB_OK | MB_ICONWARNING); return false;
            }
        }
        return true;
    }
    void onCommand(int id, int notification) {
        if (m_updating) return;
        if (id == groupPreset && notification == CBN_SELCHANGE) { storeGroup(); const auto index = static_cast<std::size_t>(SendMessageW(m_groupCombo, CB_GETCURSEL, 0, 0)); m_draft.activeGroupId = m_draft.groups[index].id; loadGroup(index); }
        else if (id == copyGroup && notification == BN_CLICKED) { storeGroup(); auto copy = m_draft.groups[m_groupIndex]; copy.id = "custom.group." + std::to_string(GetTickCount64()); copy.label += " (copy)"; copy.builtIn = false; m_draft.groups.push_back(std::move(copy)); m_draft.activeGroupId = m_draft.groups.back().id; rebuildGroupCombo(); loadGroup(m_draft.groups.size() - 1); }
        else if (id == deleteGroup && notification == BN_CLICKED && m_groupIndex < m_draft.groups.size() && !m_draft.groups[m_groupIndex].builtIn) { const auto removed = m_draft.groups[m_groupIndex].id; m_draft.groups.erase(m_draft.groups.begin() + static_cast<std::ptrdiff_t>(m_groupIndex)); if (m_draft.activeGroupId == removed) m_draft.activeGroupId = "builtin.album.simple"; rebuildGroupCombo(); loadGroup(0); }
        else if ((id == autoCollapse || id == collapseDefault) && notification == BN_CLICKED) notifyChanged();
        else if (id == groupPreset && notification == CBN_EDITCHANGE) {
            if (m_groupIndex < m_draft.groups.size() && !m_draft.groups[m_groupIndex].builtIn)
                m_draft.groups[m_groupIndex].label = wideToUtf8(editText(m_groupCombo));
            notifyChanged();
        }
        else if (id == columnUp || id == columnDown) { storeColumn(); const auto direction = id == columnUp ? -1 : 1; const auto target = static_cast<std::ptrdiff_t>(m_columnIndex) + direction; if (target >= 0 && target < static_cast<std::ptrdiff_t>(m_draft.columns.size()) && !m_draft.columns[m_columnIndex].cover && !m_draft.columns[static_cast<std::size_t>(target)].cover) { std::swap(m_draft.columns[m_columnIndex], m_draft.columns[static_cast<std::size_t>(target)]); m_columnIndex = static_cast<std::size_t>(target); rebuildColumnList(); loadColumn(m_columnIndex); } }
        else if (id == copyColumn && notification == BN_CLICKED) { storeColumn(); auto copy = m_draft.columns[m_columnIndex]; copy.id = "custom.column." + std::to_string(GetTickCount64()); copy.label += " (copy)"; copy.builtIn = false; copy.cover = false; m_draft.columns.push_back(std::move(copy)); rebuildColumnList(); loadColumn(m_draft.columns.size() - 1); }
        else if (id == deleteColumn && notification == BN_CLICKED && m_columnIndex < m_draft.columns.size() && !m_draft.columns[m_columnIndex].builtIn) { m_draft.columns.erase(m_draft.columns.begin() + static_cast<std::ptrdiff_t>(m_columnIndex)); rebuildColumnList(); loadColumn(0); }
        else if (notification == EN_CHANGE || notification == CBN_SELCHANGE) notifyChanged();
    }
    bool onNotify(const NMHDR& header) {
        if (header.hwndFrom != m_columnList || m_updating || header.code != LVN_ITEMCHANGED) return false;
        const auto* change = reinterpret_cast<const NMLISTVIEW*>(&header);
        if ((change->uNewState & LVIS_SELECTED) != 0 && change->iItem >= 0 && static_cast<std::size_t>(change->iItem) != m_columnIndex) { storeColumn(); loadColumn(static_cast<std::size_t>(change->iItem)); }
        notifyChanged(); return true;
    }
    void notifyChanged() const { if (m_callback.is_valid()) m_callback->on_state_changed(); }

    preferences_page_callback::ptr m_callback; PlaylistViewSettings m_initial, m_draft;
    HWND m_window{}; WNDPROC m_originalProc{}; HFONT m_font{}, m_ownedFont{}; UINT m_dpi{96}; bool m_updating{};
    std::size_t m_groupIndex{static_cast<std::size_t>(-1)}, m_columnIndex{static_cast<std::size_t>(-1)};
    HWND m_groupBox{}, m_groupCombo{}, m_autoCollapse{}, m_collapseDefault{}, m_copyGroup{}, m_deleteGroup{}, m_groupArtwork{};
    std::vector<HWND> m_groupLabels; std::array<HWND, 6> m_groupEdits{};
    HWND m_columnsBox{}, m_columnList{}, m_columnUp{}, m_columnDown{}, m_copyColumn{}, m_deleteColumn{};
    std::vector<HWND> m_columnLabels; HWND m_columnLabel{}, m_columnDisplay{}, m_columnSort{}, m_columnAlignment{}, m_columnWidth{};
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

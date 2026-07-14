#include "playback_panel.h"
#include "artwork_loader.h"
#include "lyrics_host.h"
#include "now_playing_state.h"
#include "playback_state.h"
#include "playlist_view_model.h"
#include "playlist_config_model.h"
#include "playlist_grouping_model.h"
#include "settings.h"

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <mmsystem.h>
#include <d2d1.h>
#include <d2d1helper.h>
#include <dwrite.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <cwctype>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include <columns_ui-sdk-8.1.0/ui_extension.h>
#include <foobar2000/SDK/foobar2000.h>

namespace refrain {
namespace {

using Microsoft::WRL::ComPtr;

constexpr GUID kPlaybackPanelGuid{0x9e26e4b0, 0x41a9, 0x4b08, {0xa3, 0x55, 0x74, 0x55, 0x10, 0x82, 0xc5, 0xf2}};
constexpr UINT kRefreshMessage = WM_APP + 0x421;
constexpr UINT kArtworkReadyMessage = WM_APP + 0x423;
constexpr UINT kLyricsMiddleClickMessage = WM_APP + 0x424;
constexpr UINT kGroupArtworkReadyMessage = WM_APP + 0x425;

enum class ControlId {
    none,
    seek,
    previous,
    playPause,
    next,
    stop,
    mute,
    volume,
    settings,
    artwork,
    trackTitle,
    artistAlbum,
    codecBitrate,
    rating1,
    rating2,
    rating3,
    rating4,
    rating5,
    rightDivider,
    trackDetails,
    playlistHeader,
    playlistBody,
    playlistScrollbar,
};

struct Layout {
    D2D1_RECT_F seek{};
    D2D1_RECT_F previous{};
    D2D1_RECT_F playPause{};
    D2D1_RECT_F next{};
    D2D1_RECT_F stop{};
    D2D1_RECT_F mute{};
    D2D1_RECT_F volume{};
    D2D1_RECT_F settings{};
    D2D1_RECT_F elapsed{};
    D2D1_RECT_F total{};
    D2D1_RECT_F header{};
    D2D1_RECT_F rightColumn{};
    D2D1_RECT_F rightDivider{};
    D2D1_RECT_F lowerRight{};
    D2D1_RECT_F playlistArea{};
    D2D1_RECT_F playlistHeader{};
    D2D1_RECT_F playlistBody{};
    D2D1_RECT_F playlistScrollTrack{};
    D2D1_RECT_F playlistScrollThumb{};
    D2D1_RECT_F artwork{};
    std::array<D2D1_RECT_F, 5> ratings{};
    D2D1_RECT_F trackTitle{};
    D2D1_RECT_F artistAlbum{};
    D2D1_RECT_F codecBitrate{};
    float surfaceTop{};
};

struct DetailRow {
    std::wstring field;
    std::wstring value;
    bool heading{};
};

struct PlaylistRowText {
    std::vector<std::wstring> columns;
};

struct ArtworkAsyncState {
    std::atomic_bool alive{true};
    std::atomic_uint64_t generation{};
    HWND window{};
    std::mutex abortMutex;
    std::shared_ptr<abort_callback_impl> aborter;
};

struct ArtworkCompletion {
    std::uint64_t generation{};
    ArtworkPixels pixels;
};

struct GroupArtworkRequest {
    std::uint64_t generation{};
    std::string key;
    metadb_handle_list items;
    GUID artworkId{};
};

struct GroupArtworkCompletion {
    std::uint64_t generation{};
    std::string key;
    ArtworkPixels pixels;
};

struct GroupArtworkCacheEntry {
    bool loading{};
    ArtworkPixels pixels{ArtworkStatus::unavailable};
    ComPtr<ID2D1Bitmap> bitmap;
};

[[nodiscard]] bool contains(const D2D1_RECT_F& rect, D2D1_POINT_2F point) noexcept {
    return rect.right > rect.left && rect.bottom > rect.top
        && point.x >= rect.left && point.x <= rect.right && point.y >= rect.top && point.y <= rect.bottom;
}

[[nodiscard]] D2D1_POINT_2F pointFromLParam(LPARAM value, float scale) noexcept {
    return D2D1::Point2F(static_cast<float>(GET_X_LPARAM(value)) / scale,
        static_cast<float>(GET_Y_LPARAM(value)) / scale);
}

[[nodiscard]] float fractionAtX(float x, const D2D1_RECT_F& rect) noexcept {
    const auto width = rect.right - rect.left;
    return width > 0.0F ? static_cast<float>(clampUnit((x - rect.left) / width)) : 0.0F;
}

[[nodiscard]] std::wstring utf8ToWide(const char* text) {
    if (!text || *text == '\0') return {};
    const auto count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, nullptr, 0);
    if (count <= 1) return {};
    std::wstring result(static_cast<std::size_t>(count), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, result.data(), count) <= 0) return {};
    result.pop_back();
    return result;
}

class PlaybackPanel : public uie::container_uie_window_v3,
                      public play_callback,
                      private playlist_callback_impl_base,
                      private metadb_io_callback_dynamic_impl_base {
public:
    PlaybackPanel()
        : playlist_callback_impl_base(playlist_callback::flag_item_ops
              | playlist_callback::flag_on_playlist_activate
              | playlist_callback::flag_on_playlist_locked
              | playlist_callback::flag_on_default_format_changed) {}
    ~PlaybackPanel() {
        stopGroupArtworkWorker();
        stopArtworkWork();
        unregisterPlaybackCallback();
        releaseDeviceResources();
    }

    const GUID& get_extension_guid() const override { return kPlaybackPanelGuid; }
    void get_name(pfc::string_base& out) const override { out = "Refrain"; }
    void get_category(pfc::string_base& out) const override { out = "Panels"; }
    bool get_short_name(pfc::string_base& out) const override {
        out = "Refrain";
        return true;
    }
    bool get_description(pfc::string_base& out) const override {
        out = "Native Refrain playback shell and synchronized playback controls";
        return true;
    }
    unsigned get_type() const override { return uie::type_panel | uie::type_layout; }

    uie::container_window_v3_config get_window_config() override {
        auto config = uie::container_window_v3_config{L"Refrain.PlaybackShell", false, CS_DBLCLKS};
        config.window_styles |= WS_TABSTOP;
        config.class_cursor = IDC_ARROW;
        return config;
    }

    LRESULT on_message(HWND wnd, UINT msg, WPARAM wp, LPARAM lp) override {
        switch (msg) {
        case WM_CREATE:
            if (!m_artworkAsync->alive.load()) m_artworkAsync = std::make_shared<ArtworkAsyncState>();
            m_artworkAsync->window = wnd;
            m_dpi = static_cast<float>(GetDpiForWindow(wnd));
            createIndependentResources();
            reloadPlaylistViewSettings();
            startGroupArtworkWorker();
            registerPlaybackCallback();
            rebuildPlaylistSnapshot(true);
            refreshFromCore();
            createTooltip(wnd);
            registerSettingsWindow(wnd);
            initializeLowerRightView();
            createLyricsHost(wnd);
            return 0;
        case WM_DESTROY:
            m_lyricsHost.destroy();
            stopGroupArtworkWorker();
            stopArtworkWork();
            unregisterSettingsWindow(wnd);
            unregisterPlaybackCallback();
            if (GetCapture() == wnd) {
                ReleaseCapture();
            }
            m_active = ControlId::none;
            m_previewing = false;
            releaseDeviceResources();
            m_tooltip = nullptr;
            return 0;
        case WM_SIZE:
            if (m_renderTarget) {
                m_renderTarget->Resize(D2D1::SizeU(LOWORD(lp), HIWORD(lp)));
            }
            clampPlaylistScroll();
            syncLyricsHost(wnd);
            InvalidateRect(wnd, nullptr, FALSE);
            return 0;
        case WM_DPICHANGED:
            m_dpi = static_cast<float>(LOWORD(wp));
            releaseTextResources();
            createTextResources();
            if (m_renderTarget) {
                m_renderTarget->SetDpi(m_dpi, m_dpi);
            }
            InvalidateRect(wnd, nullptr, FALSE);
            return 0;
        case WM_GETMINMAXINFO: {
            const auto scale = dpiScale();
            auto* info = reinterpret_cast<MINMAXINFO*>(lp);
            info->ptMinTrackSize.x = static_cast<LONG>(std::lround(420.0F * scale));
            info->ptMinTrackSize.y = static_cast<LONG>(std::lround(104.0F * scale));
            return 0;
        }
        case WM_PAINT:
            paint(wnd);
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_MOUSEMOVE:
            onMouseMove(wnd, lp);
            return 0;
        case WM_MOUSELEAVE:
            m_trackingMouse = false;
            if (m_active == ControlId::none) {
                setHover(ControlId::none);
            }
            return 0;
        case WM_LBUTTONDOWN:
            onLeftButtonDown(wnd, lp);
            return 0;
        case WM_LBUTTONUP:
            onLeftButtonUp(wnd, lp);
            return 0;
        case WM_LBUTTONDBLCLK:
            onLeftButtonDoubleClick(wnd, lp);
            return 0;
        case WM_MBUTTONUP:
            if (contains(calculateLayout(wnd).rightColumn, pointFromLParam(lp, dpiScale()))) {
                toggleLowerRightView();
            }
            return 0;
        case WM_RBUTTONUP:
            onRightButtonUp(wnd, pointFromLParam(lp, dpiScale()),
                POINT{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)});
            return 0;
        case WM_CAPTURECHANGED:
            if (reinterpret_cast<HWND>(lp) != wnd) {
                cancelInteraction();
            }
            return 0;
        case WM_MOUSEWHEEL:
            onMouseWheel(wnd, wp, lp);
            return 0;
        case WM_SETFOCUS:
            if (m_focus == ControlId::none) {
                m_focus = ControlId::playPause;
            }
            InvalidateRect(wnd, nullptr, FALSE);
            return 0;
        case WM_KILLFOCUS:
            cancelInteraction();
            InvalidateRect(wnd, nullptr, FALSE);
            return 0;
        case WM_GETDLGCODE:
            return DLGC_WANTARROWS | DLGC_WANTCHARS;
        case WM_KEYDOWN:
            if (onKeyDown(wnd, wp)) {
                return 0;
            }
            break;
        case WM_SETCURSOR:
            if (LOWORD(lp) == HTCLIENT && m_hover == ControlId::rightDivider) {
                SetCursor(LoadCursor(nullptr, IDC_SIZENS));
                return TRUE;
            }
            if (LOWORD(lp) == HTCLIENT && (m_hover == ControlId::seek || m_hover == ControlId::volume
                    || isRatingControl(m_hover))) {
                SetCursor(LoadCursor(nullptr, IDC_HAND));
                return TRUE;
            }
            break;
        case kRefreshMessage:
            refreshFromCore();
            return 0;
        case kSettingsChangedMessage:
            applySettingsChange();
            return 0;
        case kArtworkReadyMessage:
            applyArtworkCompletion(*reinterpret_cast<ArtworkCompletion*>(lp));
            return 0;
        case kGroupArtworkReadyMessage:
            applyGroupArtworkCompletion(reinterpret_cast<GroupArtworkCompletion*>(lp));
            return 0;
        case kLyricsMiddleClickMessage:
            toggleLowerRightView();
            return 0;
        }
        return DefWindowProc(wnd, msg, wp, lp);
    }

    void on_playback_starting(play_control::t_track_command, bool paused) override {
        m_state.playing = true;
        m_state.paused = paused;
        requestRefresh();
    }
    void on_playback_new_track(metadb_handle_ptr) override {
        if (readSettings().lyricsAutoSwitch) setLowerRightView(LowerRightView::lyrics, false);
        requestRefresh();
    }
    void on_playback_stop(play_control::t_stop_reason reason) override {
        m_state.playing = false;
        m_state.paused = false;
        m_state.seekable = false;
        m_state.position = 0.0;
        m_state.length = 0.0;
        m_previewing = false;
        if (reason != play_control::stop_reason_starting_another && readSettings().lyricsAutoSwitch) {
            setLowerRightView(LowerRightView::trackDetails, false);
        }
        requestRefresh();
    }
    void on_playback_seek(double time) override {
        m_state.position = std::max(0.0, time);
        invalidate();
    }
    void on_playback_pause(bool paused) override {
        m_state.paused = paused;
        invalidate();
    }
    void on_playback_edited(metadb_handle_ptr) override { requestRefresh(); }
    void on_playback_dynamic_info(const file_info&) override { requestRefresh(); }
    void on_playback_dynamic_info_track(const file_info&) override { requestRefresh(); }
    void on_playback_time(double time) override {
        if (!m_previewing) {
            m_state.position = std::max(0.0, time);
            invalidate();
        }
    }
    void on_volume_change(float volume) override {
        m_state.setVolume(volume);
        invalidate();
    }

    void on_items_added(t_size playlist, t_size, metadb_handle_list_cref, const bit_array&) override {
        if (playlist == m_activePlaylist) rebuildPlaylistSnapshot(false);
    }
    void on_items_reordered(t_size playlist, const t_size*, t_size) override {
        if (playlist == m_activePlaylist) rebuildPlaylistSnapshot(false);
    }
    void on_items_removed(t_size playlist, const bit_array&, t_size, t_size) override {
        if (playlist == m_activePlaylist) rebuildPlaylistSnapshot(false);
    }
    void on_items_selection_change(t_size playlist, const bit_array&, const bit_array&) override {
        if (playlist == m_activePlaylist) refreshPlaylistSelection();
    }
    void on_item_focus_change(t_size playlist, t_size, t_size to) override {
        if (playlist == m_activePlaylist) {
            m_playlistFocus = to;
            const auto group = groupForTrack(m_playlistGroups, to);
            if (group != SIZE_MAX) applyAutoCollapse(group);
            ensurePlaylistRowVisible(to);
            invalidate();
        }
        if (!playback_control::get()->is_playing()) requestRefresh();
    }
    void on_playlist_activate(t_size, t_size) override {
        rebuildPlaylistSnapshot(true);
        if (!playback_control::get()->is_playing()) requestRefresh();
    }
    void on_items_modified(t_size playlist, const bit_array&) override {
        if (playlist == m_activePlaylist) rebuildPlaylistSnapshot(false);
        requestRefresh();
    }
    void on_items_modified_fromplayback(t_size playlist, const bit_array&, play_control::t_display_level) override {
        if (playlist == m_activePlaylist) rebuildPlaylistSnapshot(false);
        requestRefresh();
    }
    void on_items_replaced(t_size playlist, const bit_array&,
        const pfc::list_base_const_t<playlist_callback::t_on_items_replaced_entry>&) override {
        if (playlist == m_activePlaylist) rebuildPlaylistSnapshot(false);
    }
    void on_item_ensure_visible(t_size playlist, t_size item) override {
        if (playlist == m_activePlaylist) ensurePlaylistRowVisible(item);
    }
    void on_playlist_locked(t_size playlist, bool) override {
        if (playlist == m_activePlaylist) invalidate();
    }
    void on_default_format_changed() override {
        clearPlaylistTextCache();
    }
    void on_changed_sorted(metadb_handle_list_cref items, bool fromHook) override {
        if (m_target.is_empty()) return;
        for (const auto& item : items) {
            if (item == m_target) {
                refreshNowPlayingText(false);
                if (!fromHook) startArtworkWork(m_target);
                invalidate();
                return;
            }
        }
    }

private:
    [[nodiscard]] static bool isRatingControl(ControlId id) noexcept {
        return id >= ControlId::rating1 && id <= ControlId::rating5;
    }

    [[nodiscard]] static int ratingForControl(ControlId id) noexcept {
        return isRatingControl(id) ? static_cast<int>(id) - static_cast<int>(ControlId::rating1) + 1 : 0;
    }

    [[nodiscard]] float dpiScale() const noexcept { return m_dpi / 96.0F; }

    [[nodiscard]] RECT lowerRightPixels(HWND wnd) const {
        const auto rect = calculateLayout(wnd).lowerRight;
        const auto scale = dpiScale();
        return {static_cast<LONG>(std::lround(rect.left * scale)),
            static_cast<LONG>(std::lround(rect.top * scale)),
            static_cast<LONG>(std::lround(rect.right * scale)),
            static_cast<LONG>(std::lround(rect.bottom * scale))};
    }

    void initializeLowerRightView() {
        const auto settings = readSettings();
        m_autoSwitchEnabled = settings.lyricsAutoSwitch;
        m_lowerRightView = settings.lyricsAutoSwitch
            ? (playback_control::get()->is_playing() ? LowerRightView::lyrics : LowerRightView::trackDetails)
            : settings.lowerRightView;
    }

    void createLyricsHost(HWND wnd) {
        (void)m_lyricsHost.create(wnd, get_host(), lowerRightPixels(wnd), kLyricsMiddleClickMessage);
        syncLyricsHost(wnd);
    }

    void syncLyricsHost(HWND wnd) {
        if (!wnd || !IsWindow(wnd)) return;
        m_lyricsHost.resize(lowerRightPixels(wnd));
        m_lyricsHost.setVisible(m_lowerRightView == LowerRightView::lyrics);
        InvalidateRect(wnd, nullptr, FALSE);
    }

    void setLowerRightView(LowerRightView view, bool persist) {
        if (m_lowerRightView == view && !persist) return;
        m_lowerRightView = view;
        if (persist) {
            auto settings = readSettings();
            settings.lowerRightView = view;
            writeSettings(settings);
        }
        if (const auto wnd = get_wnd()) syncLyricsHost(wnd);
    }

    void toggleLowerRightView() {
        setLowerRightView(m_lowerRightView == LowerRightView::lyrics
                ? LowerRightView::trackDetails : LowerRightView::lyrics,
            !readSettings().lyricsAutoSwitch);
    }

    void registerPlaybackCallback() {
        if (!m_callbackRegistered) {
            constexpr unsigned flags = play_callback::flag_on_playback_all | play_callback::flag_on_volume_change;
            play_callback_manager::get()->register_callback(this, flags, true);
            m_callbackRegistered = true;
        }
    }

    void unregisterPlaybackCallback() noexcept {
        if (m_callbackRegistered && core_api::is_main_thread()) {
            play_callback_manager::get()->unregister_callback(this);
            m_callbackRegistered = false;
        }
    }

    void requestRefresh() const noexcept {
        if (const auto wnd = get_wnd()) {
            PostMessage(wnd, kRefreshMessage, 0, 0);
        }
    }

    void invalidate() const noexcept {
        if (const auto wnd = get_wnd()) {
            InvalidateRect(wnd, nullptr, FALSE);
        }
    }

    void clearPlaylistTextCache() {
        m_playlistTextCache.clear();
        invalidate();
    }

    [[nodiscard]] const GroupDefinition& activeGroupDefinition() const {
        const auto found = std::find_if(m_playlistSettings.groups.begin(), m_playlistSettings.groups.end(),
            [&](const GroupDefinition& value) { return value.id == m_playlistSettings.activeGroupId; });
        if (found != m_playlistSettings.groups.end()) return *found;
        return m_playlistSettings.groups.front();
    }

    void reloadPlaylistViewSettings() {
        m_playlistSettings = readPlaylistViewSettings();
        auto compiler = titleformat_compiler::get();
        const auto& group = activeGroupDefinition();
        const std::array<const std::string*, 5> groupFormats{
            &group.keyFormat, &group.leftPrimary, &group.rightPrimary,
            &group.leftSecondary, &group.rightSecondary};
        constexpr std::array<const char*, 5> groupFallbacks{
            "$if2(%album%,'Single')", "$if2(%album%,'Single')", "", "[%album artist%]", ""};
        for (std::size_t index = 0; index < groupFormats.size(); ++index) {
            if (!compiler->compile(m_playlistGroupScripts[index], groupFormats[index]->c_str())) {
                compiler->compile_safe(m_playlistGroupScripts[index], groupFallbacks[index]);
            }
        }
        m_playlistColumnScripts.clear();
        m_playlistColumnScripts.reserve(m_playlistSettings.columns.size());
        for (const auto& column : m_playlistSettings.columns) {
            titleformat_object::ptr script;
            if (!column.cover && !compiler->compile(script, column.displayFormat.c_str())) {
                compiler->compile_safe(script, "$if2(%title%,$filename_ext(%path%))");
            }
            m_playlistColumnScripts.push_back(std::move(script));
        }
        m_playlistTextCache.clear();
    }

    [[nodiscard]] std::string formatPlaylistItemUtf8(
        std::size_t index, const titleformat_object::ptr& script) const {
        if (m_activePlaylist == SIZE_MAX || index >= m_playlistItems.get_count() || script.is_empty()) return {};
        pfc::string8 output;
        playlist_manager::get()->playlist_item_format_title(m_activePlaylist, index, nullptr,
            output, script, nullptr, playback_control::display_level_all);
        return output.c_str();
    }

    void rebuildPlaylistGroups() {
        ++m_groupArtworkGeneration;
        {
            std::scoped_lock lock(m_groupArtworkMutex);
            m_groupArtworkRequests.clear();
            if (m_groupArtworkAborter) m_groupArtworkAborter->abort();
        }
        m_groupArtworkCache.clear();
        m_groupArtworkBytes = 0;
        std::unordered_map<std::string, bool> priorCollapsed;
        for (const auto& group : m_playlistGroups) priorCollapsed[group.key] = group.collapsed;
        std::vector<PlaylistGroupInput> inputs;
        inputs.reserve(m_playlistItems.get_count());
        for (std::size_t index = 0; index < m_playlistItems.get_count(); ++index) {
            inputs.push_back({formatPlaylistItemUtf8(index, m_playlistGroupScripts[0]),
                formatPlaylistItemUtf8(index, m_playlistGroupScripts[1]),
                formatPlaylistItemUtf8(index, m_playlistGroupScripts[2]),
                formatPlaylistItemUtf8(index, m_playlistGroupScripts[3]),
                formatPlaylistItemUtf8(index, m_playlistGroupScripts[4])});
        }
        m_playlistGroups = buildPlaylistGroups(inputs, m_playlistSettings.collapseByDefault);
        for (auto& group : m_playlistGroups) {
            if (const auto found = priorCollapsed.find(group.key); found != priorCollapsed.end()) {
                group.collapsed = found->second;
            }
        }
        m_playlistDisplayRows = buildPlaylistDisplayRows(m_playlistGroups, 4);
    }

    void rebuildPlaylistDisplayRows() {
        m_playlistDisplayRows = buildPlaylistDisplayRows(m_playlistGroups, 4);
        clampPlaylistScroll();
        invalidate();
    }

    void refreshPlayingLocation() {
        m_playingPlaylist = SIZE_MAX;
        m_playingItem = SIZE_MAX;
        playlist_manager::get()->get_playing_item_location(&m_playingPlaylist, &m_playingItem);
    }

    void refreshPlaylistSelection() {
        if (m_activePlaylist == SIZE_MAX) {
            m_playlistSelection.resize(0);
        } else {
            m_playlistSelection.resize(playlist_manager::get()->playlist_get_item_count(m_activePlaylist));
            playlist_manager::get()->playlist_get_selection_mask(m_activePlaylist, m_playlistSelection);
        }
        invalidate();
    }

    void rebuildPlaylistSnapshot(bool revealFocus) {
        auto api = playlist_manager::get();
        m_activePlaylist = api->get_active_playlist();
        m_playlistItems.remove_all();
        m_playlistSelection.resize(0);
        m_playlistFocus = SIZE_MAX;
        m_playlistAnchor = SIZE_MAX;
        m_playlistTextCache.clear();
        if (m_activePlaylist != SIZE_MAX) {
            api->playlist_get_all_items(m_activePlaylist, m_playlistItems);
            m_playlistSelection.resize(m_playlistItems.get_count());
            api->playlist_get_selection_mask(m_activePlaylist, m_playlistSelection);
            m_playlistFocus = api->playlist_get_focus_item(m_activePlaylist);
        }
        rebuildPlaylistGroups();
        if (revealFocus && m_playlistFocus != SIZE_MAX) {
            ensurePlaylistRowVisible(m_playlistFocus);
        } else {
            clampPlaylistScroll();
        }
        refreshPlayingLocation();
        invalidate();
    }

    void clampPlaylistScroll() {
        const auto wnd = get_wnd();
        if (!wnd) {
            m_playlistTopRow = 0;
            return;
        }
        const auto body = calculateLayout(wnd).playlistBody;
        const auto capacity = visibleRowCapacity(body.bottom - body.top, 26.0F);
        m_playlistTopRow = clampTopRow(m_playlistTopRow, m_playlistDisplayRows.size(), capacity);
    }

    void ensurePlaylistRowVisible(std::size_t row) {
        const auto wnd = get_wnd();
        if (!wnd || row == SIZE_MAX || row >= m_playlistItems.get_count()) return;
        const auto groupIndex = groupForTrack(m_playlistGroups, row);
        if (groupIndex != SIZE_MAX && m_playlistGroups[groupIndex].collapsed) {
            m_playlistGroups[groupIndex].collapsed = false;
            m_playlistDisplayRows = buildPlaylistDisplayRows(m_playlistGroups, 4);
        }
        const auto displayRow = displayRowForTrack(m_playlistDisplayRows, row);
        if (displayRow == SIZE_MAX) return;
        const auto body = calculateLayout(wnd).playlistBody;
        const auto capacity = visibleRowCapacity(body.bottom - body.top, 26.0F);
        m_playlistTopRow = ensureRowVisible(m_playlistTopRow, displayRow, m_playlistDisplayRows.size(), capacity);
        invalidate();
    }

    [[nodiscard]] std::optional<std::size_t> playlistRowAt(HWND wnd, D2D1_POINT_2F point) const {
        const auto body = calculateLayout(wnd).playlistBody;
        if (!contains(body, point)) return std::nullopt;
        constexpr float rowHeight = 26.0F;
        const auto relative = std::max(0.0F, point.y - body.top);
        const auto row = m_playlistTopRow + static_cast<std::size_t>(relative / rowHeight);
        return row < m_playlistDisplayRows.size() ? std::optional<std::size_t>{row} : std::nullopt;
    }

    void scrollPlaylistRows(long long delta) {
        const auto wnd = get_wnd();
        if (!wnd) return;
        const auto body = calculateLayout(wnd).playlistBody;
        const auto capacity = visibleRowCapacity(body.bottom - body.top, 26.0F);
        const auto maximum = maximumTopRow(m_playlistDisplayRows.size(), capacity);
        const auto candidate = static_cast<long long>(m_playlistTopRow) + delta;
        m_playlistTopRow = static_cast<std::size_t>(std::clamp(candidate, 0LL,
            static_cast<long long>(maximum)));
        invalidate();
    }

    void updatePlaylistScrollbar(HWND wnd, float pointerY) {
        const auto layout = calculateLayout(wnd);
        const auto bodyHeight = layout.playlistBody.bottom - layout.playlistBody.top;
        const auto capacity = visibleRowCapacity(bodyHeight, 26.0F);
        const auto maximum = maximumTopRow(m_playlistDisplayRows.size(), capacity);
        const auto trackHeight = layout.playlistScrollTrack.bottom - layout.playlistScrollTrack.top;
        const auto thumbHeight = layout.playlistScrollThumb.bottom - layout.playlistScrollThumb.top;
        m_playlistTopRow = topRowFromScrollbar(pointerY, m_playlistScrollbarGrabOffset,
            layout.playlistScrollTrack.top, trackHeight, thumbHeight, maximum);
        invalidate();
    }

    void selectPlaylistRow(std::size_t row, bool ctrl, bool shift) {
        if (m_activePlaylist == SIZE_MAX || row >= m_playlistItems.get_count()) return;
        auto api = playlist_manager::get();
        m_playlistSelection.resize(m_playlistItems.get_count());
        api->playlist_get_selection_mask(m_activePlaylist, m_playlistSelection);
        const auto anchor = m_playlistAnchor != SIZE_MAX && m_playlistAnchor < m_playlistItems.get_count()
            ? m_playlistAnchor
            : (m_playlistFocus != SIZE_MAX && m_playlistFocus < m_playlistItems.get_count()
                  ? m_playlistFocus : row);
        if (shift) {
            if (!ctrl) api->playlist_set_selection(m_activePlaylist, pfc::bit_array_true(), pfc::bit_array_false());
            const auto [first, last] = inclusiveRange(anchor, row);
            api->playlist_set_selection(m_activePlaylist, pfc::bit_array_range(first, last - first + 1),
                pfc::bit_array_true());
        } else if (ctrl) {
            api->playlist_set_selection_single(m_activePlaylist, row, !m_playlistSelection.get(row));
            m_playlistAnchor = row;
        } else {
            api->playlist_set_selection(m_activePlaylist, pfc::bit_array_true(), pfc::bit_array_false());
            api->playlist_set_selection_single(m_activePlaylist, row, true);
            m_playlistAnchor = row;
        }
        api->playlist_set_focus_item(m_activePlaylist, row);
        m_playlistFocus = row;
        ensurePlaylistRowVisible(row);
    }

    void clearPlaylistSelection() {
        if (m_activePlaylist == SIZE_MAX) return;
        playlist_manager::get()->playlist_set_selection(m_activePlaylist,
            pfc::bit_array_true(), pfc::bit_array_false());
    }

    void selectPlaylistGroup(std::size_t groupIndex, bool ctrl, bool shift) {
        if (m_activePlaylist == SIZE_MAX || groupIndex >= m_playlistGroups.size()) return;
        const auto& group = m_playlistGroups[groupIndex];
        auto api = playlist_manager::get();
        m_playlistSelection.resize(m_playlistItems.get_count());
        api->playlist_get_selection_mask(m_activePlaylist, m_playlistSelection);
        if (shift) {
            if (!ctrl) api->playlist_set_selection(m_activePlaylist, pfc::bit_array_true(), pfc::bit_array_false());
            const auto anchor = m_playlistAnchor < m_playlistItems.get_count() ? m_playlistAnchor : group.start;
            const auto [first, last] = inclusiveRange(anchor, group.start + group.count - 1);
            api->playlist_set_selection(m_activePlaylist, pfc::bit_array_range(first, last - first + 1), pfc::bit_array_true());
        } else {
            bool allSelected = true;
            for (std::size_t offset = 0; offset < group.count; ++offset)
                allSelected = allSelected && m_playlistSelection.get(group.start + offset);
            if (!ctrl) api->playlist_set_selection(m_activePlaylist, pfc::bit_array_true(), pfc::bit_array_false());
            if (ctrl && allSelected) {
                api->playlist_set_selection(m_activePlaylist,
                    pfc::bit_array_range(group.start, group.count), pfc::bit_array_false());
            } else {
                api->playlist_set_selection(m_activePlaylist,
                    pfc::bit_array_range(group.start, group.count), pfc::bit_array_true());
            }
        }
        api->playlist_set_focus_item(m_activePlaylist, group.start);
        m_playlistFocus = group.start;
        m_playlistAnchor = group.start;
    }

    void applyAutoCollapse(std::size_t keepGroup) {
        if (!m_playlistSettings.autoCollapse) return;
        bool changed{};
        for (std::size_t index = 0; index < m_playlistGroups.size(); ++index) {
            const auto collapse = index != keepGroup;
            changed = changed || m_playlistGroups[index].collapsed != collapse;
            m_playlistGroups[index].collapsed = collapse;
        }
        if (changed) rebuildPlaylistDisplayRows();
    }

    void onPlaylistLeftButtonDown(HWND wnd, D2D1_POINT_2F point) {
        m_focus = ControlId::playlistBody;
        const auto ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        const auto shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        if (const auto row = playlistRowAt(wnd, point)) {
            const auto& display = m_playlistDisplayRows[*row];
            if (display.kind == PlaylistDisplayRowKind::groupHeader) {
                selectPlaylistGroup(display.group, ctrl, shift);
                applyAutoCollapse(display.group);
            } else if (display.kind == PlaylistDisplayRowKind::track) {
                selectPlaylistRow(display.track, ctrl, shift);
                applyAutoCollapse(display.group);
            }
        } else if (!ctrl && !shift) clearPlaylistSelection();
        invalidate();
    }

    void onLeftButtonDoubleClick(HWND wnd, LPARAM lp) {
        const auto point = pointFromLParam(lp, dpiScale());
        if (const auto row = playlistRowAt(wnd, point); row && m_activePlaylist != SIZE_MAX) {
            auto& display = m_playlistDisplayRows[*row];
            if (display.kind == PlaylistDisplayRowKind::groupHeader) {
                m_playlistGroups[display.group].collapsed = !m_playlistGroups[display.group].collapsed;
                rebuildPlaylistDisplayRows();
            } else if (display.kind == PlaylistDisplayRowKind::track) {
                selectPlaylistRow(display.track, false, false);
                playlist_manager::get()->playlist_execute_default_action(m_activePlaylist, display.track);
            }
        }
    }

    [[nodiscard]] std::optional<std::size_t> playlistColumnAt(HWND wnd, float pointerX) const {
        const auto layout = calculateLayout(wnd);
        if (pointerX < layout.playlistHeader.left || pointerX > layout.playlistHeader.right) return std::nullopt;
        auto totalWeight = 0;
        for (const auto& column : m_playlistSettings.columns) if (column.visible) totalWeight += column.widthWeight;
        auto x = layout.playlistHeader.left;
        for (std::size_t index = 0; index < m_playlistSettings.columns.size(); ++index) {
            const auto& column = m_playlistSettings.columns[index];
            if (!column.visible) continue;
            const auto right = x + (layout.playlistBody.right - layout.playlistBody.left)
                * static_cast<float>(column.widthWeight) / static_cast<float>(std::max(1, totalWeight));
            if (pointerX >= x && pointerX < right) return index;
            x = right;
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<std::pair<std::size_t, std::size_t>> playlistBoundaryAt(
        HWND wnd, float pointerX) const {
        const auto layout = calculateLayout(wnd);
        auto totalWeight = 0;
        for (const auto& column : m_playlistSettings.columns) if (column.visible) totalWeight += column.widthWeight;
        auto x = layout.playlistHeader.left;
        std::optional<std::size_t> previous;
        for (std::size_t index = 0; index < m_playlistSettings.columns.size(); ++index) {
            const auto& column = m_playlistSettings.columns[index];
            if (!column.visible) continue;
            if (previous && std::abs(pointerX - x) <= 4.0F) return std::pair{*previous, index};
            x += (layout.playlistBody.right - layout.playlistBody.left)
                * static_cast<float>(column.widthWeight) / static_cast<float>(std::max(1, totalWeight));
            previous = index;
        }
        return std::nullopt;
    }

    void updatePlaylistHeaderDrag(HWND wnd, float pointerX) {
        if (!m_headerPressedColumn) return;
        const auto delta = pointerX - m_headerPressX;
        m_headerMoved = m_headerMoved || std::abs(delta) >= 4.0F;
        if (!m_headerResizeColumns) return;
        const auto layout = calculateLayout(wnd);
        auto totalWeight = 0;
        for (const auto& column : m_headerStartSettings.columns) if (column.visible) totalWeight += column.widthWeight;
        const auto width = std::max(1.0F, layout.playlistBody.right - layout.playlistBody.left);
        const auto weightDelta = static_cast<int>(std::lround(delta / width * static_cast<float>(totalWeight)));
        const auto [left, right] = *m_headerResizeColumns;
        const auto combined = m_headerStartSettings.columns[left].widthWeight
            + m_headerStartSettings.columns[right].widthWeight;
        const auto newLeft = std::clamp(m_headerStartSettings.columns[left].widthWeight + weightDelta,
            100, combined - 100);
        m_playlistSettings.columns[left].widthWeight = newLeft;
        m_playlistSettings.columns[right].widthWeight = combined - newLeft;
        invalidate();
    }

    [[nodiscard]] bool playlistCanReorder() const {
        if (m_activePlaylist == SIZE_MAX) return false;
        const auto api = playlist_manager::get();
        return !api->playlist_lock_is_present(m_activePlaylist)
            || (api->playlist_lock_get_filter_mask(m_activePlaylist) & playlist_lock::filter_reorder) == 0;
    }

    bool sortActivePlaylist(const std::string& format, bool descending = false) {
        if (format.empty() || m_activePlaylist == SIZE_MAX || m_playlistItems.get_count() < 2) return true;
        if (!playlistCanReorder()) {
            MessageBoxW(get_wnd(), L"This playlist does not allow item reordering.", L"Refrain", MB_OK | MB_ICONINFORMATION);
            return false;
        }
        auto api = playlist_manager::get();
        api->playlist_undo_backup(m_activePlaylist);
        bool sorted{};
        if (descending) {
            titleformat_object::ptr script;
            if (titleformat_compiler::get()->compile(script, format.c_str())) {
                std::vector<t_size> order(m_playlistItems.get_count());
                metadb_handle_list_helper::sort_by_format_get_order(
                    m_playlistItems, order.data(), script, nullptr, -1);
                sorted = api->playlist_reorder_items(m_activePlaylist, order.data(), order.size());
            }
        } else {
            sorted = api->playlist_sort_by_format(m_activePlaylist, format.c_str(), false);
        }
        if (!sorted) {
            MessageBoxW(get_wnd(), L"foobar2000 could not sort this playlist.", L"Refrain", MB_OK | MB_ICONWARNING);
            return false;
        }
        rebuildPlaylistSnapshot(true);
        return true;
    }

    void activatePlaylistGroup(std::size_t index) {
        if (index >= m_playlistSettings.groups.size()) return;
        const auto& group = m_playlistSettings.groups[index];
        if (group.id == m_playlistSettings.activeGroupId) return;
        if (!sortActivePlaylist(group.sortFormat)) return;
        m_sortColumnId.clear();
        m_sortDescending = false;
        m_playlistSettings.activeGroupId = group.id;
        writePlaylistViewSettings(m_playlistSettings);
    }

    void showPlaylistHeaderMenu(HWND wnd, POINT clientPoint) {
        const auto menu = CreatePopupMenu();
        const auto groups = CreatePopupMenu();
        const auto columns = CreatePopupMenu();
        if (!menu || !groups || !columns) {
            if (columns) DestroyMenu(columns); if (groups) DestroyMenu(groups); if (menu) DestroyMenu(menu);
            return;
        }
        constexpr UINT groupBase = 1000;
        constexpr UINT columnBase = 2000;
        for (std::size_t index = 0; index < m_playlistSettings.groups.size(); ++index) {
            const auto label = utf8ToWide(m_playlistSettings.groups[index].label.c_str());
            AppendMenuW(groups, MF_STRING | (m_playlistSettings.groups[index].id == m_playlistSettings.activeGroupId
                ? MF_CHECKED : 0), groupBase + static_cast<UINT>(index), label.c_str());
        }
        for (std::size_t index = 0; index < m_playlistSettings.columns.size(); ++index) {
            const auto label = utf8ToWide(m_playlistSettings.columns[index].label.c_str());
            AppendMenuW(columns, MF_STRING | (m_playlistSettings.columns[index].visible ? MF_CHECKED : 0),
                columnBase + static_cast<UINT>(index), label.c_str());
        }
        AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(groups), L"Group by");
        AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(columns), L"Columns");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, 3001, L"Collapse all groups");
        AppendMenuW(menu, MF_STRING, 3002, L"Expand all groups");
        AppendMenuW(menu, MF_STRING | (m_playlistSettings.autoCollapse ? MF_CHECKED : 0),
            3003, L"Auto-collapse");
        AppendMenuW(menu, MF_STRING | (m_playlistSettings.collapseByDefault ? MF_CHECKED : 0),
            3004, L"Collapse groups by default");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, 3005, L"Refresh group artwork");
        AppendMenuW(menu, MF_STRING, 3006, L"Restore default playlist layout");
        AppendMenuW(menu, MF_STRING, 3007, L"Playlist View preferences...");
        ClientToScreen(wnd, &clientPoint);
        const auto command = TrackPopupMenuEx(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
            clientPoint.x, clientPoint.y, wnd, nullptr);
        if (command >= groupBase && command < groupBase + m_playlistSettings.groups.size()) {
            activatePlaylistGroup(command - groupBase);
        } else if (command >= columnBase && command < columnBase + m_playlistSettings.columns.size()) {
            auto& column = m_playlistSettings.columns[command - columnBase];
            column.visible = !column.visible;
            m_playlistSettings = normalizePlaylistViewSettings(m_playlistSettings);
            writePlaylistViewSettings(m_playlistSettings);
        } else if (command == 3001 || command == 3002) {
            for (auto& group : m_playlistGroups) group.collapsed = command == 3001;
            rebuildPlaylistDisplayRows();
        } else if (command == 3003 || command == 3004) {
            if (command == 3003) m_playlistSettings.autoCollapse = !m_playlistSettings.autoCollapse;
            else m_playlistSettings.collapseByDefault = !m_playlistSettings.collapseByDefault;
            writePlaylistViewSettings(m_playlistSettings);
        } else if (command == 3005) {
            ++m_groupArtworkGeneration;
            {
                std::scoped_lock lock(m_groupArtworkMutex);
                m_groupArtworkRequests.clear();
                if (m_groupArtworkAborter) m_groupArtworkAborter->abort();
            }
            m_groupArtworkCache.clear();
            m_groupArtworkBytes = 0;
            invalidate();
        } else if (command == 3006) {
            m_playlistSettings = defaultPlaylistViewSettings();
            writePlaylistViewSettings(m_playlistSettings);
        } else if (command == 3007) {
            ui_control::get()->show_preferences(kPlaylistPreferencesPageGuid);
        }
        DestroyMenu(menu);
    }

    void onRightButtonUp(HWND wnd, D2D1_POINT_2F point, POINT clientPoint) {
        const auto layout = calculateLayout(wnd);
        if (contains(layout.playlistHeader, point)) {
            showPlaylistHeaderMenu(wnd, clientPoint);
            return;
        }
        if (!contains(layout.playlistArea, point)) {
            showTrackDetailsMenu(wnd, point, clientPoint);
            return;
        }
        if (m_activePlaylist == SIZE_MAX) return;
        auto api = playlist_manager::get();
        if (const auto row = playlistRowAt(wnd, point)) {
            const auto display = m_playlistDisplayRows[*row];
            if (display.kind == PlaylistDisplayRowKind::spacer) return;
            if (display.kind == PlaylistDisplayRowKind::groupHeader) {
                selectPlaylistGroup(display.group, false, false);
            }
            const auto track = display.kind == PlaylistDisplayRowKind::track
                ? display.track : m_playlistGroups[display.group].start;
            m_playlistSelection.resize(m_playlistItems.get_count());
            api->playlist_get_selection_mask(m_activePlaylist, m_playlistSelection);
            if (!m_playlistSelection.get(track)) {
                selectPlaylistRow(track, false, false);
            } else {
                api->playlist_set_focus_item(m_activePlaylist, track);
                m_playlistFocus = track;
            }
            metadb_handle_list frozenSelection;
            api->playlist_get_selected_items(m_activePlaylist, frozenSelection);
            if (frozenSelection.get_count() == 0) return;
            ClientToScreen(wnd, &clientPoint);
            contextmenu_manager::win32_run_menu_context(wnd, frozenSelection, &clientPoint);
        } else {
            ClientToScreen(wnd, &clientPoint);
            contextmenu_manager::win32_run_menu_context_playlist(wnd, &clientPoint);
        }
        rebuildPlaylistSnapshot(false);
    }

    bool onPlaylistKeyDown(HWND wnd, WPARAM key) {
        if (m_activePlaylist == SIZE_MAX) return false;
        if (key == VK_F5) {
            ++m_groupArtworkGeneration;
            {
                std::scoped_lock lock(m_groupArtworkMutex);
                m_groupArtworkRequests.clear();
                if (m_groupArtworkAborter) m_groupArtworkAborter->abort();
            }
            m_groupArtworkCache.clear();
            m_groupArtworkBytes = 0;
            invalidate();
            return true;
        }
        auto api = playlist_manager::get();
        const auto count = m_playlistItems.get_count();
        const auto ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        const auto shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        if (ctrl && key == 'A') {
            api->playlist_set_selection(m_activePlaylist, pfc::bit_array_true(), pfc::bit_array_true());
            return true;
        }
        if (key == VK_F2) {
            refreshPlayingLocation();
            if (m_playingPlaylist == m_activePlaylist && m_playingItem < count) {
                api->playlist_set_focus_item(m_activePlaylist, m_playingItem);
                m_playlistFocus = m_playingItem;
                ensurePlaylistRowVisible(m_playingItem);
            }
            return true;
        }
        if (key == VK_DELETE) {
            if (count == 0) return true;
            if (api->playlist_lock_is_present(m_activePlaylist)
                && (api->playlist_lock_get_filter_mask(m_activePlaylist) & playlist_lock::filter_remove) != 0) return true;
            m_playlistSelection.resize(count);
            api->playlist_get_selection_mask(m_activePlaylist, m_playlistSelection);
            if (api->playlist_get_selection_count(m_activePlaylist, count) == 0) return true;
            api->playlist_undo_backup(m_activePlaylist);
            api->playlist_remove_items(m_activePlaylist, m_playlistSelection);
            return true;
        }
        if (count == 0) return false;
        auto focus = m_playlistFocus < count ? m_playlistFocus : std::size_t{};
        if (key == VK_SPACE) {
            m_playlistSelection.resize(count);
            api->playlist_get_selection_mask(m_activePlaylist, m_playlistSelection);
            api->playlist_set_selection_single(m_activePlaylist, focus, !m_playlistSelection.get(focus));
            m_playlistAnchor = focus;
            return true;
        }
        if (key == VK_RETURN) {
            api->playlist_execute_default_action(m_activePlaylist, focus);
            return true;
        }
        const auto layout = calculateLayout(wnd);
        const auto page = std::max<std::size_t>(1,
            visibleRowCapacity(layout.playlistBody.bottom - layout.playlistBody.top, 26.0F) - 1);
        auto target = focus;
        if (key == VK_UP) target = focus > 0 ? focus - 1 : 0;
        else if (key == VK_DOWN) target = std::min(count - 1, focus + 1);
        else if (key == VK_PRIOR) target = focus > page ? focus - page : 0;
        else if (key == VK_NEXT) target = std::min(count - 1, focus + page);
        else if (key == VK_HOME) target = 0;
        else if (key == VK_END) target = count - 1;
        else return false;
        if (ctrl && !shift) {
            api->playlist_set_focus_item(m_activePlaylist, target);
            m_playlistFocus = target;
            ensurePlaylistRowVisible(target);
        } else {
            selectPlaylistRow(target, ctrl, shift);
        }
        return true;
    }

    void refreshFromCore() {
        auto api = playback_control::get();
        m_state.playing = api->is_playing();
        m_state.paused = m_state.playing && api->is_paused();
        m_state.position = m_state.playing ? std::max(0.0, api->playback_get_position()) : 0.0;
        m_state.length = m_state.playing ? std::max(0.0, api->playback_get_length_ex()) : 0.0;
        m_state.seekable = m_state.playing && api->playback_can_seek();
        m_state.setVolume(api->get_volume());
        auto apiV3 = playback_control_v3::get();
        m_state.customVolume = apiV3->custom_volume_is_active();
        refreshPlayingLocation();
        refreshNowPlayingTarget();
        invalidate();
    }

    void createIndependentResources() {
        if (!m_d2dFactory) {
            D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, m_d2dFactory.ReleaseAndGetAddressOf());
        }
        if (!m_dwriteFactory) {
            DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                reinterpret_cast<IUnknown**>(m_dwriteFactory.ReleaseAndGetAddressOf()));
        }
        if (m_titleScript.is_empty()) {
            auto compiler = titleformat_compiler::get();
            compiler->compile_safe(m_titleScript, "$if2(%title%,$if2($filename(%path%),Unknown title))");
            compiler->compile_safe(m_artistScript, "$if2(%artist%,Unknown artist)");
            compiler->compile_safe(m_albumScript, "$if2(%album%,Unknown album)");
            compiler->compile_safe(m_codecScript, "$if2(%codec%,Unknown codec)");
            compiler->compile_safe(m_bitrateScript, "$if2(%bitrate%,Unknown bitrate)");
            compiler->compile_safe(m_ratingScript, "$if2(%rating%,0)");
        }
        createTextResources();
    }

    void createTextResources() {
        if (!m_dwriteFactory || m_textFormat) {
            return;
        }
        if (SUCCEEDED(m_dwriteFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
                DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12.0F, L"", m_textFormat.ReleaseAndGetAddressOf()))) {
            m_textFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            m_textFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        }
        m_dwriteFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD,
            DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 14.0F, L"", m_titleFormat.ReleaseAndGetAddressOf());
        m_dwriteFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 11.0F, L"", m_secondaryFormat.ReleaseAndGetAddressOf());
        configureTextFormat(m_titleFormat.Get());
        configureTextFormat(m_secondaryFormat.Get());
    }

    bool createDeviceResources(HWND wnd) {
        if (m_renderTarget) {
            return true;
        }
        if (!m_d2dFactory) {
            return false;
        }
        RECT rect{};
        GetClientRect(wnd, &rect);
        const auto size = D2D1::SizeU(static_cast<UINT32>(std::max(0L, rect.right - rect.left)),
            static_cast<UINT32>(std::max(0L, rect.bottom - rect.top)));
        const auto result = m_d2dFactory->CreateHwndRenderTarget(D2D1::RenderTargetProperties(),
            D2D1::HwndRenderTargetProperties(wnd, size), m_renderTarget.ReleaseAndGetAddressOf());
        if (FAILED(result)) {
            return false;
        }
        m_renderTarget->SetDpi(m_dpi, m_dpi);
        m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0xF4F1EA), m_background.ReleaseAndGetAddressOf());
        m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0xDDD7CC), m_surface.ReleaseAndGetAddressOf());
        m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0x252A2E), m_foreground.ReleaseAndGetAddressOf());
        m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0xA5A099), m_disabled.ReleaseAndGetAddressOf());
        m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0xE7A43B), m_accent.ReleaseAndGetAddressOf());
        m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0xFFFFFF), m_light.ReleaseAndGetAddressOf());
        createArtworkBitmap();
        return m_background && m_surface && m_foreground && m_disabled && m_accent && m_light;
    }

    void releaseTextResources() noexcept {
        m_secondaryFormat.Reset();
        m_titleFormat.Reset();
        m_textFormat.Reset();
    }

    void releaseDeviceResources() noexcept {
        for (auto& [key, entry] : m_groupArtworkCache) {
            (void)key;
            entry.bitmap.Reset();
        }
        m_light.Reset();
        m_artworkBitmap.Reset();
        m_accent.Reset();
        m_disabled.Reset();
        m_foreground.Reset();
        m_surface.Reset();
        m_background.Reset();
        m_renderTarget.Reset();
        m_textFormat.Reset();
        m_dwriteFactory.Reset();
        m_d2dFactory.Reset();
    }

    [[nodiscard]] Layout calculateLayout(HWND wnd) const {
        RECT client{};
        GetClientRect(wnd, &client);
        const auto width = static_cast<float>(client.right - client.left) / dpiScale();
        const auto height = static_cast<float>(client.bottom - client.top) / dpiScale();
        const auto top = std::max(0.0F, height - 104.0F);
        const auto seekY = top + 19.0F;
        const auto buttonY = top + 50.0F;
        const auto center = width * 0.5F;

        Layout layout;
        layout.surfaceTop = top;
        layout.elapsed = D2D1::RectF(10.0F, top + 7.0F, 58.0F, top + 31.0F);
        layout.total = D2D1::RectF(width - 58.0F, top + 7.0F, width - 10.0F, top + 31.0F);
        layout.seek = D2D1::RectF(64.0F, seekY - 7.0F, std::max(65.0F, width - 64.0F), seekY + 7.0F);
        layout.previous = D2D1::RectF(center - 92.0F, buttonY, center - 56.0F, buttonY + 36.0F);
        layout.playPause = D2D1::RectF(center - 46.0F, buttonY - 3.0F, center - 4.0F, buttonY + 39.0F);
        layout.next = D2D1::RectF(center + 6.0F, buttonY, center + 42.0F, buttonY + 36.0F);
        layout.stop = D2D1::RectF(center + 52.0F, buttonY, center + 88.0F, buttonY + 36.0F);
        layout.mute = D2D1::RectF(std::max(center + 100.0F, width - 190.0F), buttonY, std::max(center + 136.0F, width - 154.0F), buttonY + 36.0F);
        layout.volume = D2D1::RectF(std::max(center + 142.0F, width - 146.0F), buttonY + 8.0F,
            width - 16.0F, buttonY + 28.0F);
        if (readSettings().showSettingsButton) {
            layout.settings = D2D1::RectF(16.0F, buttonY, 52.0F, buttonY + 36.0F);
        }

        if (top > 170.0F && width > 220.0F) {
            const auto panelWidth = std::min(width, std::clamp(width * 0.23F, 280.0F, 440.0F));
            const auto left = width - panelWidth;
            const auto storedPermille = m_dragHeaderPermille >= 0
                ? m_dragHeaderPermille : readSettings().rightHeaderPermille;
            const auto headerRatio = static_cast<float>(storedPermille) / 1000.0F;
            const auto dividerY = std::clamp(top * headerRatio, 150.0F, std::max(151.0F, top - 80.0F));
            layout.rightColumn = D2D1::RectF(left, 0.0F, width, top);
            layout.rightDivider = D2D1::RectF(left, dividerY - 4.0F, width, dividerY + 4.0F);
            layout.lowerRight = D2D1::RectF(left, dividerY + 4.0F, width, top);
            const auto artworkSize = std::max(32.0F,
                std::min(panelWidth - 32.0F, dividerY - 112.0F));
            const auto artLeft = left + (panelWidth - artworkSize) * 0.5F;
            layout.artwork = D2D1::RectF(artLeft, 16.0F, artLeft + artworkSize, 16.0F + artworkSize);
            constexpr float starSize = 20.0F;
            constexpr float starGap = 6.0F;
            const auto starsWidth = starSize * 5.0F + starGap * 4.0F;
            const auto starsLeft = left + (panelWidth - starsWidth) * 0.5F;
            const auto starsTop = layout.artwork.bottom + 8.0F;
            for (std::size_t index = 0; index < layout.ratings.size(); ++index) {
                const auto x = starsLeft + static_cast<float>(index) * (starSize + starGap);
                layout.ratings[index] = D2D1::RectF(x, starsTop, x + starSize, starsTop + starSize);
            }
            layout.trackTitle = D2D1::RectF(left + 12.0F, starsTop + 25.0F, width - 12.0F, starsTop + 49.0F);
            layout.artistAlbum = D2D1::RectF(left + 12.0F, starsTop + 49.0F, width - 12.0F, starsTop + 69.0F);
            layout.codecBitrate = D2D1::RectF(left + 12.0F, starsTop + 69.0F, width - 12.0F, starsTop + 89.0F);
            layout.header = D2D1::RectF(left, 0.0F, width, dividerY - 4.0F);
        }

        const auto playlistRight = layout.rightColumn.right > layout.rightColumn.left
            ? layout.rightColumn.left : width;
        if (playlistRight > 120.0F && top > 60.0F) {
            constexpr float headerHeight = 30.0F;
            constexpr float scrollbarWidth = 12.0F;
            layout.playlistArea = D2D1::RectF(0.0F, 0.0F, playlistRight, top);
            layout.playlistHeader = D2D1::RectF(0.0F, 0.0F, playlistRight, headerHeight);
            layout.playlistBody = D2D1::RectF(0.0F, headerHeight,
                playlistRight - scrollbarWidth, top);
            layout.playlistScrollTrack = D2D1::RectF(playlistRight - scrollbarWidth,
                headerHeight, playlistRight, top);
            constexpr float rowHeight = 26.0F;
            const auto capacity = visibleRowCapacity(layout.playlistBody.bottom - layout.playlistBody.top, rowHeight);
            const auto maximum = maximumTopRow(m_playlistDisplayRows.size(), capacity);
            const auto trackHeight = layout.playlistScrollTrack.bottom - layout.playlistScrollTrack.top;
            const auto thumbHeight = scrollbarThumbHeight(trackHeight, m_playlistDisplayRows.size(), capacity);
            const auto thumbTop = scrollbarThumbTop(layout.playlistScrollTrack.top, trackHeight,
                thumbHeight, m_playlistTopRow, maximum);
            layout.playlistScrollThumb = D2D1::RectF(layout.playlistScrollTrack.left + 2.0F,
                thumbTop, layout.playlistScrollTrack.right - 2.0F, thumbTop + thumbHeight);
        }
        return layout;
    }

    [[nodiscard]] ControlId hitTest(HWND wnd, D2D1_POINT_2F point) const {
        const auto layout = calculateLayout(wnd);
        for (std::size_t index = 0; index < layout.ratings.size(); ++index) {
            if (contains(layout.ratings[index], point)) {
                return static_cast<ControlId>(static_cast<int>(ControlId::rating1) + static_cast<int>(index));
            }
        }
        if (contains(layout.artwork, point)) return ControlId::artwork;
        if (contains(layout.trackTitle, point)) return ControlId::trackTitle;
        if (contains(layout.artistAlbum, point)) return ControlId::artistAlbum;
        if (contains(layout.codecBitrate, point)) return ControlId::codecBitrate;
        if (contains(layout.rightDivider, point)) return ControlId::rightDivider;
        if (m_lowerRightView == LowerRightView::trackDetails && contains(layout.lowerRight, point)) {
            return ControlId::trackDetails;
        }
        if (contains(layout.playlistScrollTrack, point)) return ControlId::playlistScrollbar;
        if (contains(layout.playlistHeader, point)) return ControlId::playlistHeader;
        if (contains(layout.playlistBody, point)) return ControlId::playlistBody;
        if (contains(layout.seek, point)) return m_state.canSeek() ? ControlId::seek : ControlId::none;
        if (contains(layout.previous, point)) return ControlId::previous;
        if (contains(layout.playPause, point)) return ControlId::playPause;
        if (contains(layout.next, point)) return ControlId::next;
        if (contains(layout.stop, point)) return m_state.playing ? ControlId::stop : ControlId::none;
        if (contains(layout.mute, point)) return ControlId::mute;
        if (contains(layout.volume, point)) return m_state.customVolume ? ControlId::none : ControlId::volume;
        if (contains(layout.settings, point)) return ControlId::settings;
        return ControlId::none;
    }

    [[nodiscard]] bool isEnabled(ControlId id) const noexcept {
        switch (id) {
        case ControlId::seek: return m_state.canSeek();
        case ControlId::stop: return m_state.playing;
        case ControlId::volume: return !m_state.customVolume;
        case ControlId::previous:
        case ControlId::playPause:
        case ControlId::next:
        case ControlId::mute: return true;
        case ControlId::settings: return readSettings().showSettingsButton;
        case ControlId::rating1:
        case ControlId::rating2:
        case ControlId::rating3:
        case ControlId::rating4:
        case ControlId::rating5: return m_ratingAvailable && m_target.is_valid();
        case ControlId::rightDivider:
        case ControlId::trackDetails: return true;
        case ControlId::playlistBody:
        case ControlId::playlistScrollbar: return true;
        default: return false;
        }
    }

    void paint(HWND wnd) {
        PAINTSTRUCT paintStruct{};
        BeginPaint(wnd, &paintStruct);
        if (!createDeviceResources(wnd) || !m_textFormat) {
            RECT client{};
            GetClientRect(wnd, &client);
            FillRect(paintStruct.hdc, &client, reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1));
            SetBkMode(paintStruct.hdc, TRANSPARENT);
            SetTextColor(paintStruct.hdc, RGB(64, 64, 64));
            DrawTextW(paintStruct.hdc, L"Refrain rendering unavailable", -1, &client,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            EndPaint(wnd, &paintStruct);
            return;
        }

        const auto layout = calculateLayout(wnd);
        const auto size = m_renderTarget->GetSize();
        m_renderTarget->BeginDraw();
        m_renderTarget->Clear(D2D1::ColorF(0xF4F1EA));
        m_renderTarget->FillRectangle(D2D1::RectF(0.0F, layout.surfaceTop, size.width, size.height), m_surface.Get());
        m_renderTarget->DrawLine(D2D1::Point2F(0.0F, layout.surfaceTop), D2D1::Point2F(size.width, layout.surfaceTop),
            m_disabled.Get(), 1.0F);

        drawPlaylist(layout);
        drawNowPlayingHeader(layout);
        if (layout.rightDivider.right > layout.rightDivider.left) {
            m_renderTarget->FillRectangle(layout.rightDivider,
                m_active == ControlId::rightDivider || m_hover == ControlId::rightDivider
                    ? m_accent.Get() : m_surface.Get());
            m_renderTarget->DrawLine(D2D1::Point2F(layout.rightDivider.left, (layout.rightDivider.top + layout.rightDivider.bottom) * 0.5F),
                D2D1::Point2F(layout.rightDivider.right, (layout.rightDivider.top + layout.rightDivider.bottom) * 0.5F),
                m_disabled.Get(), 1.0F);
        }
        if (m_lowerRightView == LowerRightView::trackDetails) {
            drawTrackDetails(layout);
        } else if (!m_lyricsHost.available()) {
            drawTextWith(m_lyricsHost.statusText(), layout.lowerRight, m_secondaryFormat.Get(),
                DWRITE_TEXT_ALIGNMENT_CENTER, m_disabled.Get());
        }
        drawSeek(layout);
        drawButton(layout.previous, ControlId::previous);
        drawButton(layout.playPause, ControlId::playPause);
        drawButton(layout.next, ControlId::next);
        drawButton(layout.stop, ControlId::stop);
        drawButton(layout.mute, ControlId::mute);
        if (readSettings().showSettingsButton) {
            drawButton(layout.settings, ControlId::settings);
        }
        drawVolume(layout);

        const auto displayPosition = m_previewing ? m_previewPosition : m_state.position;
        drawText(formatPlaybackTime(displayPosition, m_state.playing), layout.elapsed, DWRITE_TEXT_ALIGNMENT_CENTER,
            m_state.playing ? m_foreground.Get() : m_disabled.Get());
        const auto settings = readSettings();
        const auto rightTime = settings.timeDisplay == TimeDisplayMode::remaining
            ? formatRemainingTime(displayPosition, m_state.length)
            : formatPlaybackTime(m_state.length, m_state.hasKnownLength());
        drawText(rightTime, layout.total,
            DWRITE_TEXT_ALIGNMENT_CENTER, m_state.hasKnownLength() ? m_foreground.Get() : m_disabled.Get());

        const auto result = m_renderTarget->EndDraw();
        if (result == D2DERR_RECREATE_TARGET) {
            for (auto& [key, entry] : m_groupArtworkCache) {
                (void)key;
                entry.bitmap.Reset();
            }
            m_light.Reset();
            m_accent.Reset();
            m_disabled.Reset();
            m_foreground.Reset();
            m_surface.Reset();
            m_background.Reset();
            m_artworkBitmap.Reset();
            m_renderTarget.Reset();
        }
        EndPaint(wnd, &paintStruct);
    }

    [[nodiscard]] PlaylistRowText formatPlaylistRow(std::size_t index) {
        if (const auto found = m_playlistTextCache.find(index); found != m_playlistTextCache.end()) {
            return found->second;
        }
        PlaylistRowText row;
        row.columns.reserve(m_playlistSettings.columns.size());
        for (std::size_t column = 0; column < m_playlistSettings.columns.size(); ++column) {
            row.columns.push_back(!m_playlistSettings.columns[column].visible
                    || m_playlistSettings.columns[column].cover
                ? std::wstring{} : utf8ToWide(formatPlaylistItemUtf8(index, m_playlistColumnScripts[column]).c_str()));
        }
        if (m_playlistTextCache.size() >= 512) m_playlistTextCache.clear();
        m_playlistTextCache.emplace(index, row);
        return row;
    }

    void drawPlaylist(const Layout& layout) {
        if (layout.playlistArea.right <= layout.playlistArea.left) return;
        m_renderTarget->FillRectangle(layout.playlistHeader, m_surface.Get());
        m_renderTarget->DrawLine(D2D1::Point2F(layout.playlistHeader.left, layout.playlistHeader.bottom),
            D2D1::Point2F(layout.playlistHeader.right, layout.playlistHeader.bottom), m_disabled.Get(), 1.0F);

        const auto contentRight = layout.playlistBody.right;
        struct DrawColumn { std::size_t index{}; D2D1_RECT_F rect{}; };
        std::vector<DrawColumn> columns;
        auto totalWeight = 0;
        for (const auto& column : m_playlistSettings.columns) if (column.visible) totalWeight += column.widthWeight;
        auto x = layout.playlistBody.left;
        for (std::size_t index = 0; index < m_playlistSettings.columns.size(); ++index) {
            const auto& column = m_playlistSettings.columns[index];
            if (!column.visible) continue;
            const auto next = index + 1 == m_playlistSettings.columns.size()
                ? contentRight : x + (contentRight - layout.playlistBody.left)
                    * static_cast<float>(column.widthWeight) / static_cast<float>(std::max(1, totalWeight));
            columns.push_back({index, D2D1::RectF(x, layout.playlistHeader.top, std::min(next, contentRight), layout.playlistHeader.bottom)});
            x = next;
        }
        auto alignment = [](ColumnAlignment value) {
            if (value == ColumnAlignment::center) return DWRITE_TEXT_ALIGNMENT_CENTER;
            if (value == ColumnAlignment::trailing) return DWRITE_TEXT_ALIGNMENT_TRAILING;
            return DWRITE_TEXT_ALIGNMENT_LEADING;
        };
        for (const auto& column : columns) {
            const auto& definition = m_playlistSettings.columns[column.index];
            auto rect = column.rect; rect.left += 6.0F; rect.right -= 6.0F;
            auto label = utf8ToWide(definition.label.c_str());
            if (definition.id == m_sortColumnId) label += m_sortDescending ? L"  \x2193" : L"  \x2191";
            drawTextWith(label, rect, m_secondaryFormat.Get(),
                alignment(definition.alignment), m_foreground.Get());
            m_renderTarget->DrawLine(D2D1::Point2F(column.rect.right, column.rect.top + 5.0F),
                D2D1::Point2F(column.rect.right, column.rect.bottom - 5.0F), m_disabled.Get(), 0.5F);
        }

        if (m_activePlaylist == SIZE_MAX) {
            drawTextWith(L"No active playlist", layout.playlistBody, m_secondaryFormat.Get(),
                DWRITE_TEXT_ALIGNMENT_CENTER, m_disabled.Get());
            return;
        }
        if (m_playlistItems.get_count() == 0) {
            drawTextWith(L"Playlist is empty", layout.playlistBody, m_secondaryFormat.Get(),
                DWRITE_TEXT_ALIGNMENT_CENTER, m_disabled.Get());
            return;
        }

        constexpr float rowHeight = 26.0F;
        std::vector<std::pair<std::size_t, float>> visibleGroupCovers;
        const auto visible = visibleRows(m_playlistTopRow, m_playlistDisplayRows.size(),
            layout.playlistBody.bottom - layout.playlistBody.top, rowHeight, 0);
        m_renderTarget->PushAxisAlignedClip(layout.playlistBody, D2D1_ANTIALIAS_MODE_ALIASED);
        for (auto displayIndex = visible.first; displayIndex < visible.end; ++displayIndex) {
            const auto y = layout.playlistBody.top
                + static_cast<float>(displayIndex - m_playlistTopRow) * rowHeight;
            const auto rowRect = D2D1::RectF(layout.playlistBody.left, y, contentRight, y + rowHeight);
            const auto& display = m_playlistDisplayRows[displayIndex];
            if (display.kind == PlaylistDisplayRowKind::groupHeader) {
                const auto& group = m_playlistGroups[display.group];
                if (!group.collapsed) visibleGroupCovers.emplace_back(display.group, y + rowHeight);
                const auto groupPlaying = m_playingPlaylist == m_activePlaylist
                    && m_playingItem >= group.start && m_playingItem < group.start + group.count;
                m_renderTarget->FillRectangle(rowRect, m_surface.Get());
                const auto triangleX = rowRect.left + 11.0F;
                const auto triangleY = rowRect.top + rowHeight * 0.5F;
                if (group.collapsed) drawTriangle(D2D1::Point2F(triangleX - 3.0F, triangleY - 5.0F),
                    D2D1::Point2F(triangleX + 4.0F, triangleY), D2D1::Point2F(triangleX - 3.0F, triangleY + 5.0F), m_foreground.Get());
                else drawTriangle(D2D1::Point2F(triangleX - 5.0F, triangleY - 3.0F),
                    D2D1::Point2F(triangleX + 5.0F, triangleY - 3.0F), D2D1::Point2F(triangleX, triangleY + 4.0F), m_foreground.Get());
                auto leftText = utf8ToWide(group.leftPrimary.c_str());
                const auto leftSecondary = utf8ToWide(group.leftSecondary.c_str());
                if (!leftSecondary.empty()) leftText += L"  |  " + leftSecondary;
                auto rightText = utf8ToWide(group.rightPrimary.c_str());
                const auto rightSecondary = utf8ToWide(group.rightSecondary.c_str());
                if (!rightSecondary.empty()) rightText = rightSecondary + L"  |  " + rightText;
                if (groupPlaying) drawTriangle(D2D1::Point2F(rowRect.right - 16.0F, triangleY - 5.0F),
                    D2D1::Point2F(rowRect.right - 8.0F, triangleY),
                    D2D1::Point2F(rowRect.right - 16.0F, triangleY + 5.0F), m_foreground.Get());
                drawTextWith(leftText,
                    D2D1::RectF(rowRect.left + 24.0F, y, rowRect.right * 0.72F, y + rowHeight),
                    m_secondaryFormat.Get(), DWRITE_TEXT_ALIGNMENT_LEADING,
                    groupPlaying ? m_accent.Get() : m_foreground.Get());
                drawTextWith(rightText,
                    D2D1::RectF(rowRect.right * 0.72F, y, rowRect.right - (groupPlaying ? 22.0F : 8.0F), y + rowHeight),
                    m_secondaryFormat.Get(), DWRITE_TEXT_ALIGNMENT_TRAILING, m_disabled.Get());
                continue;
            }
            if (display.kind == PlaylistDisplayRowKind::spacer) continue;
            const auto index = display.track;
            const auto selected = m_playlistSelection.get(index);
            const auto focused = index == m_playlistFocus;
            const auto playing = m_playingPlaylist == m_activePlaylist && index == m_playingItem;
            if (selected) m_renderTarget->FillRectangle(rowRect, m_surface.Get());
            if (playing) {
                m_renderTarget->FillRectangle(D2D1::RectF(rowRect.left, rowRect.top,
                    rowRect.left + 4.0F, rowRect.bottom), m_accent.Get());
            }
            if (focused) {
                const auto focusRect = D2D1::RectF(rowRect.left + 1.0F, rowRect.top + 1.0F,
                    rowRect.right - 1.0F, rowRect.bottom - 1.0F);
                m_renderTarget->DrawRectangle(focusRect, m_foreground.Get(), 1.0F);
            }
            const auto row = formatPlaylistRow(index);
            const auto textBrush = playing ? m_accent.Get() : m_foreground.Get();
            for (const auto& column : columns) {
                const auto& definition = m_playlistSettings.columns[column.index];
                if (definition.cover || column.index >= row.columns.size()) continue;
                auto rect = D2D1::RectF(column.rect.left + 6.0F, y, column.rect.right - 6.0F, y + rowHeight);
                auto text = row.columns[column.index];
                if (definition.id == "builtin.rating") {
                    const auto rating = std::clamp(_wtoi(text.c_str()), 0, 5);
                    text.assign(static_cast<std::size_t>(rating), L'\x2605');
                    text.append(static_cast<std::size_t>(5 - rating), L'\x2606');
                }
                drawTextWith(text, rect, m_secondaryFormat.Get(),
                    alignment(definition.alignment), textBrush);
            }
            m_renderTarget->DrawLine(D2D1::Point2F(rowRect.left, rowRect.bottom),
                D2D1::Point2F(rowRect.right, rowRect.bottom), m_surface.Get(), 1.0F);
        }
        const auto coverColumn = std::find_if(columns.begin(), columns.end(), [&](const DrawColumn& column) {
            return m_playlistSettings.columns[column.index].cover;
        });
        if (coverColumn != columns.end()) {
            for (const auto& [groupIndex, top] : visibleGroupCovers) {
                auto rect = D2D1::RectF(coverColumn->rect.left + 4.0F, top + 4.0F,
                    coverColumn->rect.right - 4.0F, top + rowHeight * 4.0F - 4.0F);
                m_renderTarget->FillRectangle(rect, m_surface.Get());
                auto& artwork = requestGroupArtwork(groupIndex);
                createGroupArtworkBitmap(artwork);
                if (artwork.bitmap) {
                    const auto bitmapSize = artwork.bitmap->GetSize();
                    const auto scale = std::min((rect.right - rect.left) / bitmapSize.width,
                        (rect.bottom - rect.top) / bitmapSize.height);
                    const auto width = bitmapSize.width * scale;
                    const auto height = bitmapSize.height * scale;
                    const auto target = D2D1::RectF(rect.left + (rect.right - rect.left - width) * 0.5F,
                        rect.top + (rect.bottom - rect.top - height) * 0.5F,
                        rect.left + (rect.right - rect.left + width) * 0.5F,
                        rect.top + (rect.bottom - rect.top + height) * 0.5F);
                    m_renderTarget->DrawBitmap(artwork.bitmap.Get(), target, 1.0F,
                        D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
                } else {
                    m_renderTarget->DrawRectangle(rect, m_disabled.Get(), 1.0F);
                    drawTextWith(artwork.loading ? L"Loading" : L"No cover", rect,
                        m_secondaryFormat.Get(), DWRITE_TEXT_ALIGNMENT_CENTER, m_disabled.Get());
                }
            }
        }
        m_renderTarget->PopAxisAlignedClip();

        const auto capacity = visibleRowCapacity(layout.playlistBody.bottom - layout.playlistBody.top, rowHeight);
        if (m_playlistDisplayRows.size() > capacity) {
            m_renderTarget->FillRectangle(layout.playlistScrollTrack, m_surface.Get());
            m_renderTarget->FillRoundedRectangle(D2D1::RoundedRect(layout.playlistScrollThumb, 4.0F, 4.0F),
                m_active == ControlId::playlistScrollbar ? m_accent.Get() : m_disabled.Get());
        }
    }

    void configureTextFormat(IDWriteTextFormat* format) {
        if (!format || !m_dwriteFactory) return;
        format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        DWRITE_TRIMMING trimming{DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0};
        ComPtr<IDWriteInlineObject> ellipsis;
        if (SUCCEEDED(m_dwriteFactory->CreateEllipsisTrimmingSign(format, ellipsis.ReleaseAndGetAddressOf()))) {
            format->SetTrimming(&trimming, ellipsis.Get());
        }
    }

    void createArtworkBitmap() {
        if (!m_renderTarget || m_artworkBitmap || m_artworkPixels.status != ArtworkStatus::ready
            || m_artworkPixels.width == 0 || m_artworkPixels.height == 0 || m_artworkPixels.bgra.empty()) {
            return;
        }
        const auto stride = m_artworkPixels.width * 4U;
        m_renderTarget->CreateBitmap(D2D1::SizeU(m_artworkPixels.width, m_artworkPixels.height),
            m_artworkPixels.bgra.data(), stride,
            D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,
                D2D1_ALPHA_MODE_PREMULTIPLIED), m_dpi, m_dpi), m_artworkBitmap.ReleaseAndGetAddressOf());
    }

    void drawNowPlayingHeader(const Layout& layout) {
        if (!contains(layout.header, D2D1::Point2F(layout.header.left + 1.0F, layout.header.top + 1.0F))) return;
        m_renderTarget->DrawLine(D2D1::Point2F(layout.header.left, layout.header.top),
            D2D1::Point2F(layout.header.left, layout.header.bottom), m_disabled.Get(), 1.0F);
        m_renderTarget->DrawLine(D2D1::Point2F(layout.header.left, layout.header.bottom),
            D2D1::Point2F(layout.header.right, layout.header.bottom), m_disabled.Get(), 1.0F);

        createArtworkBitmap();
        if (m_artworkBitmap) {
            const auto bitmapSize = m_artworkBitmap->GetSize();
            const auto scale = std::min((layout.artwork.right - layout.artwork.left) / bitmapSize.width,
                (layout.artwork.bottom - layout.artwork.top) / bitmapSize.height);
            const auto drawWidth = bitmapSize.width * scale;
            const auto drawHeight = bitmapSize.height * scale;
            const auto left = (layout.artwork.left + layout.artwork.right - drawWidth) * 0.5F;
            const auto top = (layout.artwork.top + layout.artwork.bottom - drawHeight) * 0.5F;
            m_renderTarget->DrawBitmap(m_artworkBitmap.Get(), D2D1::RectF(left, top, left + drawWidth, top + drawHeight),
                1.0F, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
        } else {
            m_renderTarget->FillRectangle(layout.artwork, m_surface.Get());
            m_renderTarget->DrawRectangle(layout.artwork, m_disabled.Get(), 1.0F);
            const wchar_t* status = L"No artwork";
            if (m_artworkLoading) status = L"Loading artwork...";
            else if (m_artworkPixels.status == ArtworkStatus::unavailable) status = L"Artwork unavailable";
            drawTextWith(status, layout.artwork, m_secondaryFormat.Get(), DWRITE_TEXT_ALIGNMENT_CENTER, m_disabled.Get());
        }

        const auto hoverRating = ratingForControl(m_hover);
        const auto shownRating = hoverRating > 0 && m_ratingAvailable ? hoverRating : m_rating;
        for (std::size_t index = 0; index < layout.ratings.size(); ++index) {
            const auto& rect = layout.ratings[index];
            const auto center = D2D1::Point2F((rect.left + rect.right) * 0.5F, (rect.top + rect.bottom) * 0.5F);
            const auto active = static_cast<int>(index) < shownRating;
            const auto brush = m_ratingAvailable && m_target.is_valid()
                ? (active ? m_accent.Get() : m_disabled.Get()) : m_disabled.Get();
            drawStar(center, 9.0F, brush, active);
        }

        drawTextWith(m_trackTitle, layout.trackTitle, m_titleFormat.Get(), DWRITE_TEXT_ALIGNMENT_CENTER,
            m_target.is_valid() ? m_foreground.Get() : m_disabled.Get());
        drawTextWith(m_artistAlbum, layout.artistAlbum, m_secondaryFormat.Get(), DWRITE_TEXT_ALIGNMENT_CENTER,
            m_target.is_valid() ? m_foreground.Get() : m_disabled.Get());
        drawTextWith(m_codecBitrate, layout.codecBitrate, m_secondaryFormat.Get(), DWRITE_TEXT_ALIGNMENT_CENTER,
            m_target.is_valid() ? m_foreground.Get() : m_disabled.Get());
    }

    void drawStar(D2D1_POINT_2F center, float radius, ID2D1Brush* brush, bool filled) {
        ComPtr<ID2D1PathGeometry> geometry;
        ComPtr<ID2D1GeometrySink> sink;
        if (FAILED(m_d2dFactory->CreatePathGeometry(geometry.ReleaseAndGetAddressOf()))
            || FAILED(geometry->Open(sink.ReleaseAndGetAddressOf()))) return;
        std::array<D2D1_POINT_2F, 10> points{};
        for (std::size_t index = 0; index < points.size(); ++index) {
            const auto angle = -3.14159265F / 2.0F + static_cast<float>(index) * 3.14159265F / 5.0F;
            const auto pointRadius = index % 2 == 0 ? radius : radius * 0.43F;
            points[index] = D2D1::Point2F(center.x + std::cos(angle) * pointRadius,
                center.y + std::sin(angle) * pointRadius);
        }
        sink->BeginFigure(points[0], filled ? D2D1_FIGURE_BEGIN_FILLED : D2D1_FIGURE_BEGIN_HOLLOW);
        sink->AddLines(points.data() + 1, static_cast<UINT32>(points.size() - 1));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
        if (SUCCEEDED(sink->Close())) {
            if (filled) m_renderTarget->FillGeometry(geometry.Get(), brush);
            else m_renderTarget->DrawGeometry(geometry.Get(), brush, 1.3F);
        }
    }

    void drawTrackDetails(const Layout& layout) {
        if (layout.lowerRight.right <= layout.lowerRight.left) return;
        m_renderTarget->DrawLine(D2D1::Point2F(layout.lowerRight.left, layout.lowerRight.top),
            D2D1::Point2F(layout.lowerRight.left, layout.lowerRight.bottom), m_disabled.Get(), 1.0F);
        constexpr float rowHeight = 22.0F;
        auto y = layout.lowerRight.top + 4.0F - m_detailScroll;
        for (std::size_t index = 0; index < m_detailRows.size(); ++index) {
            const auto& row = m_detailRows[index];
            const auto rowRect = D2D1::RectF(layout.lowerRight.left + 8.0F, y,
                layout.lowerRight.right - 8.0F, y + rowHeight);
            if (rowRect.bottom >= layout.lowerRight.top && rowRect.top <= layout.lowerRight.bottom) {
                if (index < m_detailSelected.size() && m_detailSelected[index]) {
                    m_renderTarget->FillRectangle(rowRect, m_surface.Get());
                }
                if (row.heading) {
                    drawTextWith(row.field, rowRect, m_titleFormat.Get(), DWRITE_TEXT_ALIGNMENT_LEADING,
                        m_accent.Get());
                } else {
                    const auto split = layout.lowerRight.left + (layout.lowerRight.right - layout.lowerRight.left) * 0.38F;
                    drawTextWith(row.field, D2D1::RectF(rowRect.left, rowRect.top, split - 4.0F, rowRect.bottom),
                        m_secondaryFormat.Get(), DWRITE_TEXT_ALIGNMENT_LEADING, m_disabled.Get());
                    drawTextWith(row.value, D2D1::RectF(split, rowRect.top, rowRect.right, rowRect.bottom),
                        m_secondaryFormat.Get(), DWRITE_TEXT_ALIGNMENT_LEADING, m_foreground.Get());
                }
            }
            y += rowHeight;
        }
        if (m_detailRows.empty()) {
            drawTextWith(L"No track details", layout.lowerRight, m_secondaryFormat.Get(),
                DWRITE_TEXT_ALIGNMENT_CENTER, m_disabled.Get());
        }
    }

    void drawTextWith(const std::wstring& text, D2D1_RECT_F rect, IDWriteTextFormat* format,
        DWRITE_TEXT_ALIGNMENT alignment, ID2D1Brush* brush) {
        if (!format) return;
        format->SetTextAlignment(alignment);
        m_renderTarget->DrawTextW(text.c_str(), static_cast<UINT32>(text.size()), format, rect, brush,
            D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }

    [[nodiscard]] std::wstring formatTargetField(const titleformat_object::ptr& script,
        const wchar_t* fallback) const {
        if (m_target.is_empty() || script.is_empty()) return fallback;
        pfc::string8 output;
        bool formatted = false;
        metadb_handle_ptr nowPlaying;
        const auto playback = playback_control::get();
        if (m_state.playing && playback->get_now_playing(nowPlaying) && nowPlaying == m_target) {
            formatted = playback->playback_format_title(nullptr, output, script, nullptr,
                playback_control::display_level_all);
        } else {
            formatted = m_target->format_title(nullptr, output, script, nullptr);
        }
        auto converted = formatted ? utf8ToWide(output.c_str()) : std::wstring{};
        return converted.empty() ? fallback : converted;
    }

    [[nodiscard]] std::wstring formatTargetExpression(const char* expression) const {
        titleformat_object::ptr script;
        titleformat_compiler::get()->compile_safe(script, expression);
        return formatTargetField(script, L"");
    }

    void addDetailSection(const wchar_t* name) {
        m_detailRows.push_back({name, {}, true});
    }

    void addDetailRow(std::wstring field, std::wstring value) {
        if (!value.empty() && value != L"?") m_detailRows.push_back({std::move(field), std::move(value), false});
    }

    void rebuildTrackDetails() {
        m_detailRows.clear();
        m_detailSelected.clear();
        m_detailScroll = 0.0F;
        if (m_target.is_empty()) return;

        const auto container = m_target->get_info_ref();
        if (container.is_empty()) return;
        const auto& info = container->info();

        addDetailSection(L"Metadata");
        for (t_size index = 0; index < info.meta_get_count(); ++index) {
            const auto name = utf8ToWide(info.meta_enum_name(index));
            auto normalized = name;
            std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                [](wchar_t value) { return static_cast<wchar_t>(std::towupper(value)); });
            if (normalized.find(L"LYRICS") != std::wstring::npos
                || normalized.rfind(L"MUSICBRAINZ", 0) == 0
                || normalized.find(L"ARTWORK") != std::wstring::npos) continue;
            std::wstring values;
            for (t_size valueIndex = 0; valueIndex < info.meta_enum_value_count(index); ++valueIndex) {
                if (!values.empty()) values += L"; ";
                values += utf8ToWide(info.meta_enum_value(index, valueIndex));
            }
            addDetailRow(name, std::move(values));
        }

        addDetailSection(L"Location");
        addDetailRow(L"File name", formatTargetExpression("%filename_ext%"));
        addDetailRow(L"Folder", formatTargetExpression("$directory(%path%)"));
        addDetailRow(L"Path", utf8ToWide(m_target->get_path()));
        addDetailRow(L"Subsong", std::to_wstring(m_target->get_subsong_index()));
        addDetailRow(L"File size", formatTargetExpression("%filesize_natural%"));
        addDetailRow(L"Last modified", formatTargetExpression("%last_modified%"));

        addDetailSection(L"Technical");
        addDetailRow(L"Duration", formatPlaybackTime(info.get_length(), info.get_length() > 0.0));
        for (t_size index = 0; index < info.info_get_count(); ++index) {
            const auto name = utf8ToWide(info.info_enum_name(index));
            auto normalized = name;
            std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                [](wchar_t value) { return static_cast<wchar_t>(std::towupper(value)); });
            if (normalized == L"MD5") continue;
            addDetailRow(name, utf8ToWide(info.info_enum_value(index)));
        }

        addDetailSection(L"Playback Statistics");
        const auto statisticsStart = m_detailRows.size();
        addDetailRow(L"Rating", formatTargetExpression("[%rating%]"));
        addDetailRow(L"Play count", formatTargetExpression("[%play_count%]"));
        addDetailRow(L"First played", formatTargetExpression("[%first_played%]"));
        addDetailRow(L"Last played", formatTargetExpression("[%last_played%]"));
        addDetailRow(L"Added", formatTargetExpression("[%added%]"));
        if (m_detailRows.size() == statisticsStart && !m_ratingAvailable) {
            addDetailRow(L"Status", L"Playback Statistics unavailable");
        }

        if (readSettings().showReplayGain) {
            addDetailSection(L"ReplayGain");
            addDetailRow(L"Track gain", formatTargetExpression("[%replaygain_track_gain%]"));
            addDetailRow(L"Track peak", formatTargetExpression("[%replaygain_track_peak%]"));
            addDetailRow(L"Album gain", formatTargetExpression("[%replaygain_album_gain%]"));
            addDetailRow(L"Album peak", formatTargetExpression("[%replaygain_album_peak%]"));
        }
        m_detailSelected.resize(m_detailRows.size());
    }

    void refreshNowPlayingTarget() {
        metadb_handle_ptr target;
        if (m_state.playing) {
            playback_control::get()->get_now_playing(target);
        } else {
            playlist_manager::get()->activeplaylist_get_focus_item_handle(target);
        }
        const auto changed = target != m_target;
        if (changed) {
            m_target = target;
            startArtworkWork(m_target);
        }
        refreshNowPlayingText(changed);
        if (changed) rebuildTrackDetails();
    }

    void refreshNowPlayingText(bool targetChanged) {
        if (m_target.is_empty()) {
            m_trackTitle = L"Nothing selected";
            m_artistAlbum = L"Unknown artist | Unknown album";
            m_codecBitrate = L"Unknown codec | Unknown bitrate";
            m_rating = 0;
            m_ratingAvailable = false;
            rebuildTrackDetails();
            return;
        }
        m_trackTitle = formatTargetField(m_titleScript, L"Unknown title");
        m_artistAlbum = formatTargetField(m_artistScript, L"Unknown artist") + L" | "
            + formatTargetField(m_albumScript, L"Unknown album");
        const auto codec = formatTargetField(m_codecScript, L"Unknown codec");
        auto bitrate = formatTargetField(m_bitrateScript, L"Unknown bitrate");
        if (bitrate != L"Unknown bitrate" && bitrate.find(L"kbps") == std::wstring::npos) bitrate += L" kbps";
        m_codecBitrate = codec + L" | " + bitrate;
        const auto ratingText = formatTargetField(m_ratingScript, L"0");
        wchar_t* end{};
        m_rating = normalizeRating(std::wcstod(ratingText.c_str(), &end));
        if (targetChanged) m_ratingAvailable = haveAllRatingCommands(m_target);
        rebuildTrackDetails();
    }

    [[nodiscard]] static std::string appendMenuNodeName(std::string path, contextmenu_node* node) {
        if (!node) return path;
        const auto* name = node->get_name();
        if (!name || *name == '\0' || std::strcmp(name, "Root") == 0) return path;
        if (!path.empty()) path += '/';
        path += name;
        return path;
    }

    [[nodiscard]] static contextmenu_node* findContextCommand(contextmenu_node* node, const char* fullName,
        std::string treePath = {}) {
        if (!node) return nullptr;
        treePath = appendMenuNodeName(std::move(treePath), node);
        if (node->get_type() == contextmenu_item_node::TYPE_COMMAND) {
            pfc::string8 name;
            if (node->get_full_name(name) && menuPathMatches(name.c_str(), fullName)) return node;
            if (menuPathMatches(treePath, fullName)) return node;
            return nullptr;
        }
        if (node->get_type() == contextmenu_item_node::TYPE_POPUP) {
            for (t_size index = 0; index < node->get_num_children(); ++index) {
                if (auto* found = findContextCommand(node->get_child(index), fullName, treePath)) return found;
            }
        }
        return nullptr;
    }

    [[nodiscard]] static service_ptr_t<contextmenu_manager> createRatingMenu(const metadb_handle_ptr& target) {
        if (target.is_empty()) return {};
        auto manager = contextmenu_manager::g_create();
        metadb_handle_list items;
        items.add_item(target);
        manager->init_context(items, contextmenu_manager::flag_view_full);
        return manager;
    }

    [[nodiscard]] static bool haveAllRatingCommands(const metadb_handle_ptr& target) {
        try {
            const auto manager = createRatingMenu(target);
            if (manager.is_empty()) return false;
            if (!findContextCommand(manager->get_root(), "Playback Statistics/Rating/<not set>")) return false;
            for (int value = 1; value <= 5; ++value) {
                const auto name = std::string("Playback Statistics/Rating/") + std::to_string(value);
                if (!findContextCommand(manager->get_root(), name.c_str())) return false;
            }
            return true;
        } catch (...) {
            return false;
        }
    }

    void executeRating(int clickedRating) {
        const auto target = m_target;
        if (target.is_empty() || !m_ratingAvailable || clickedRating < 1 || clickedRating > 5) return;
        try {
            const auto value = ratingCommandValue(m_rating, clickedRating);
            const auto commandName = value == 0
                ? std::string("Playback Statistics/Rating/<not set>")
                : std::string("Playback Statistics/Rating/") + std::to_string(value);
            const auto manager = createRatingMenu(target);
            if (manager.is_empty()) return;
            if (auto* command = findContextCommand(manager->get_root(), commandName.c_str())) {
                command->execute();
                requestRefresh();
            } else {
                m_ratingAvailable = false;
                updateTooltip(m_hover);
                invalidate();
            }
        } catch (...) {
            m_ratingAvailable = false;
            updateTooltip(m_hover);
            invalidate();
        }
    }

    void startGroupArtworkWorker() {
        if (m_groupArtworkThread.joinable()) return;
        m_groupArtworkThread = std::jthread([this](std::stop_token stop) {
            while (!stop.stop_requested()) {
                GroupArtworkRequest request;
                std::shared_ptr<abort_callback_impl> aborter;
                {
                    std::unique_lock lock(m_groupArtworkMutex);
                    m_groupArtworkCondition.wait(lock, [&] {
                        return stop.stop_requested() || !m_groupArtworkRequests.empty();
                    });
                    if (stop.stop_requested()) break;
                    request = std::move(m_groupArtworkRequests.front());
                    m_groupArtworkRequests.erase(m_groupArtworkRequests.begin());
                    aborter = std::make_shared<abort_callback_impl>();
                    m_groupArtworkAborter = aborter;
                }
                auto pixels = loadGroupArtwork(request.items, request.artworkId, *aborter);
                {
                    std::scoped_lock lock(m_groupArtworkMutex);
                    if (m_groupArtworkAborter == aborter) m_groupArtworkAborter.reset();
                }
                if (pixels.status == ArtworkStatus::aborted || stop.stop_requested()) continue;
                auto* completion = new GroupArtworkCompletion{
                    request.generation, std::move(request.key), std::move(pixels)};
                const auto wnd = get_wnd();
                if (!wnd || !PostMessage(wnd, kGroupArtworkReadyMessage, 0,
                        reinterpret_cast<LPARAM>(completion))) delete completion;
            }
        });
    }

    void stopGroupArtworkWorker() noexcept {
        if (!m_groupArtworkThread.joinable()) return;
        m_groupArtworkThread.request_stop();
        {
            std::scoped_lock lock(m_groupArtworkMutex);
            if (m_groupArtworkAborter) m_groupArtworkAborter->abort();
            m_groupArtworkRequests.clear();
        }
        m_groupArtworkCondition.notify_all();
        m_groupArtworkThread.join();
        MSG message{};
        while (PeekMessageW(&message, get_wnd(), kGroupArtworkReadyMessage,
                kGroupArtworkReadyMessage, PM_REMOVE)) {
            delete reinterpret_cast<GroupArtworkCompletion*>(message.lParam);
        }
    }

    void applyGroupArtworkCompletion(GroupArtworkCompletion* raw) {
        const std::unique_ptr<GroupArtworkCompletion> completion(raw);
        if (!completion || completion->generation != m_groupArtworkGeneration) return;
        const auto found = m_groupArtworkCache.find(completion->key);
        if (found == m_groupArtworkCache.end()) return;
        m_groupArtworkBytes -= std::min(m_groupArtworkBytes, found->second.pixels.bgra.size());
        constexpr std::size_t maximumBytes = 64U * 1024U * 1024U;
        const auto incoming = completion->pixels.bgra.size();
        while (m_groupArtworkBytes + incoming > maximumBytes) {
            const auto victim = std::find_if(m_groupArtworkCache.begin(), m_groupArtworkCache.end(), [&](const auto& item) {
                return item.first != completion->key && !item.second.loading;
            });
            if (victim == m_groupArtworkCache.end()) break;
            m_groupArtworkBytes -= std::min(m_groupArtworkBytes, victim->second.pixels.bgra.size());
            m_groupArtworkCache.erase(victim);
        }
        found->second.loading = false;
        if (m_groupArtworkBytes + incoming <= maximumBytes) {
            found->second.pixels = std::move(completion->pixels);
            m_groupArtworkBytes += incoming;
        } else {
            found->second.pixels = {ArtworkStatus::unavailable};
        }
        found->second.bitmap.Reset();
        invalidate();
    }

    [[nodiscard]] std::string groupArtworkKey(std::size_t groupIndex) const {
        return std::to_string(groupIndex) + '\x1f' + m_playlistGroups[groupIndex].key;
    }

    GroupArtworkCacheEntry& requestGroupArtwork(std::size_t groupIndex) {
        const auto key = groupArtworkKey(groupIndex);
        if (const auto found = m_groupArtworkCache.find(key); found != m_groupArtworkCache.end()) return found->second;
        if (m_groupArtworkCache.size() >= 64) {
            const auto victim = m_groupArtworkCache.begin();
            m_groupArtworkBytes -= std::min(m_groupArtworkBytes, victim->second.pixels.bgra.size());
            m_groupArtworkCache.erase(victim);
        }
        auto [entry, inserted] = m_groupArtworkCache.emplace(key, GroupArtworkCacheEntry{});
        (void)inserted;
        const auto& source = activeGroupDefinition().artwork;
        if (source == GroupArtworkSource::placeholder) {
            entry->second.pixels.status = ArtworkStatus::missing;
            return entry->second;
        }
        const auto& group = m_playlistGroups[groupIndex];
        GroupArtworkRequest request;
        request.generation = m_groupArtworkGeneration;
        request.key = key;
        request.artworkId = source == GroupArtworkSource::artist
            ? album_art_ids::artist : album_art_ids::cover_front;
        for (std::size_t offset = 0; offset < group.count; ++offset) {
            request.items.add_item(m_playlistItems[group.start + offset]);
        }
        entry->second.loading = true;
        {
            std::scoped_lock lock(m_groupArtworkMutex);
            m_groupArtworkRequests.push_back(std::move(request));
        }
        m_groupArtworkCondition.notify_one();
        return entry->second;
    }

    void createGroupArtworkBitmap(GroupArtworkCacheEntry& entry) {
        if (!m_renderTarget || entry.bitmap || entry.pixels.status != ArtworkStatus::ready
            || entry.pixels.width == 0 || entry.pixels.height == 0 || entry.pixels.bgra.empty()) return;
        m_renderTarget->CreateBitmap(D2D1::SizeU(entry.pixels.width, entry.pixels.height),
            entry.pixels.bgra.data(), entry.pixels.width * 4U,
            D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,
                D2D1_ALPHA_MODE_PREMULTIPLIED)), entry.bitmap.ReleaseAndGetAddressOf());
    }

    void startArtworkWork(const metadb_handle_ptr& target) {
        const auto state = m_artworkAsync;
        const auto generation = state->generation.fetch_add(1) + 1;
        auto aborter = std::make_shared<abort_callback_impl>();
        {
            std::scoped_lock lock(state->abortMutex);
            if (state->aborter) state->aborter->abort();
            state->aborter = aborter;
        }
        m_artworkBitmap.Reset();
        m_artworkPixels = {target.is_valid() ? ArtworkStatus::unavailable : ArtworkStatus::missing};
        m_artworkLoading = target.is_valid();
        if (target.is_empty()) return;

        fb2k::splitTask([state, target, generation, aborter] {
            auto pixels = loadFrontArtwork(target, *aborter);
            if (pixels.status == ArtworkStatus::aborted) return;
            fb2k::inMainThread([state, generation, pixels = std::move(pixels)]() mutable {
                if (!state->alive.load() || state->generation.load() != generation || !IsWindow(state->window)) return;
                ArtworkCompletion completion{generation, std::move(pixels)};
                SendMessage(state->window, kArtworkReadyMessage, 0, reinterpret_cast<LPARAM>(&completion));
            });
        });
    }

    void applyArtworkCompletion(ArtworkCompletion& completion) {
        if (!m_artworkAsync->alive.load() || completion.generation != m_artworkAsync->generation.load()) return;
        m_artworkLoading = false;
        m_artworkPixels = std::move(completion.pixels);
        m_artworkBitmap.Reset();
        createArtworkBitmap();
        updateTooltip(m_hover);
        rebuildTrackDetails();
        if (const auto wnd = get_wnd()) syncLyricsHost(wnd);
        invalidate();
    }

    void stopArtworkWork() noexcept {
        const auto state = m_artworkAsync;
        state->alive.store(false);
        state->window = nullptr;
        state->generation.fetch_add(1);
        std::scoped_lock lock(state->abortMutex);
        if (state->aborter) state->aborter->abort();
        state->aborter.reset();
    }

    void drawSeek(const Layout& layout) {
        const auto centerY = (layout.seek.top + layout.seek.bottom) * 0.5F;
        const auto rail = D2D1::RectF(layout.seek.left, centerY - 2.0F, layout.seek.right, centerY + 2.0F);
        m_renderTarget->FillRoundedRectangle(D2D1::RoundedRect(rail, 2.0F, 2.0F), m_disabled.Get());
        const auto position = m_previewing ? m_previewPosition : m_state.position;
        const auto fraction = static_cast<float>(seekFraction(position, m_state.length));
        const auto x = rail.left + (rail.right - rail.left) * fraction;
        if (m_state.canSeek()) {
            auto progress = rail;
            progress.right = x;
            m_renderTarget->FillRoundedRectangle(D2D1::RoundedRect(progress, 2.0F, 2.0F), m_accent.Get());
            m_renderTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x, centerY), 5.5F, 5.5F), m_accent.Get());
        }
    }

    void drawButton(const D2D1_RECT_F& rect, ControlId id) {
        const auto enabled = isEnabled(id);
        const auto hot = m_hover == id;
        const auto pressed = m_active == id;
        const auto primary = id == ControlId::playPause;
        auto fill = primary ? m_accent.Get() : m_surface.Get();
        if (pressed) {
            fill = m_disabled.Get();
        } else if (hot && !primary) {
            fill = m_light.Get();
        }
        if (primary || hot || pressed) {
            m_renderTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F((rect.left + rect.right) * 0.5F,
                (rect.top + rect.bottom) * 0.5F), (rect.right - rect.left) * 0.5F, (rect.bottom - rect.top) * 0.5F), fill);
        }
        const auto brush = enabled ? (primary ? m_light.Get() : m_foreground.Get()) : m_disabled.Get();
        drawIcon(rect, id, brush);
        if (m_focus == id && GetFocus() == get_wnd()) {
            auto focusRect = rect;
            focusRect.left += 2.0F;
            focusRect.top += 2.0F;
            focusRect.right -= 2.0F;
            focusRect.bottom -= 2.0F;
            m_renderTarget->DrawEllipse(D2D1::Ellipse(D2D1::Point2F((focusRect.left + focusRect.right) * 0.5F,
                (focusRect.top + focusRect.bottom) * 0.5F), (focusRect.right - focusRect.left) * 0.5F,
                (focusRect.bottom - focusRect.top) * 0.5F), brush, 1.0F);
        }
    }

    void drawIcon(const D2D1_RECT_F& rect, ControlId id, ID2D1Brush* brush) {
        const auto cx = (rect.left + rect.right) * 0.5F;
        const auto cy = (rect.top + rect.bottom) * 0.5F;
        switch (id) {
        case ControlId::previous:
            m_renderTarget->DrawLine(D2D1::Point2F(cx - 7.0F, cy - 7.0F), D2D1::Point2F(cx - 7.0F, cy + 7.0F), brush, 2.0F);
            drawTriangle(D2D1::Point2F(cx + 6.0F, cy - 8.0F), D2D1::Point2F(cx - 5.0F, cy),
                D2D1::Point2F(cx + 6.0F, cy + 8.0F), brush);
            break;
        case ControlId::playPause:
            if (m_state.playing && !m_state.paused) {
                m_renderTarget->FillRectangle(D2D1::RectF(cx - 6.0F, cy - 8.0F, cx - 2.0F, cy + 8.0F), brush);
                m_renderTarget->FillRectangle(D2D1::RectF(cx + 2.0F, cy - 8.0F, cx + 6.0F, cy + 8.0F), brush);
            } else {
                drawTriangle(D2D1::Point2F(cx - 5.0F, cy - 9.0F), D2D1::Point2F(cx + 9.0F, cy),
                    D2D1::Point2F(cx - 5.0F, cy + 9.0F), brush);
            }
            break;
        case ControlId::next:
            m_renderTarget->DrawLine(D2D1::Point2F(cx + 7.0F, cy - 7.0F), D2D1::Point2F(cx + 7.0F, cy + 7.0F), brush, 2.0F);
            drawTriangle(D2D1::Point2F(cx - 6.0F, cy - 8.0F), D2D1::Point2F(cx + 5.0F, cy),
                D2D1::Point2F(cx - 6.0F, cy + 8.0F), brush);
            break;
        case ControlId::stop:
            m_renderTarget->FillRectangle(D2D1::RectF(cx - 6.0F, cy - 6.0F, cx + 6.0F, cy + 6.0F), brush);
            break;
        case ControlId::mute:
            m_renderTarget->FillRectangle(D2D1::RectF(cx - 8.0F, cy - 4.0F, cx - 3.0F, cy + 4.0F), brush);
            m_renderTarget->DrawLine(D2D1::Point2F(cx - 3.0F, cy - 4.0F), D2D1::Point2F(cx + 2.0F, cy - 8.0F), brush, 2.0F);
            m_renderTarget->DrawLine(D2D1::Point2F(cx - 3.0F, cy + 4.0F), D2D1::Point2F(cx + 2.0F, cy + 8.0F), brush, 2.0F);
            m_renderTarget->DrawLine(D2D1::Point2F(cx + 2.0F, cy - 8.0F), D2D1::Point2F(cx + 2.0F, cy + 8.0F), brush, 2.0F);
            if (m_state.muted()) {
                m_renderTarget->DrawLine(D2D1::Point2F(cx + 5.0F, cy - 6.0F), D2D1::Point2F(cx + 11.0F, cy + 6.0F), brush, 2.0F);
                m_renderTarget->DrawLine(D2D1::Point2F(cx + 11.0F, cy - 6.0F), D2D1::Point2F(cx + 5.0F, cy + 6.0F), brush, 2.0F);
            } else {
                m_renderTarget->DrawLine(D2D1::Point2F(cx + 5.0F, cy - 5.0F), D2D1::Point2F(cx + 9.0F, cy - 2.0F), brush, 1.5F);
                m_renderTarget->DrawLine(D2D1::Point2F(cx + 9.0F, cy - 2.0F), D2D1::Point2F(cx + 9.0F, cy + 2.0F), brush, 1.5F);
                m_renderTarget->DrawLine(D2D1::Point2F(cx + 9.0F, cy + 2.0F), D2D1::Point2F(cx + 5.0F, cy + 5.0F), brush, 1.5F);
            }
            break;
        case ControlId::settings: {
            const auto radius = 8.0F;
            m_renderTarget->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), radius, radius), brush, 2.0F);
            m_renderTarget->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), 2.5F, 2.5F), brush, 2.0F);
            for (int index = 0; index < 8; ++index) {
                const auto angle = static_cast<float>(index) * 3.14159265F / 4.0F;
                m_renderTarget->DrawLine(
                    D2D1::Point2F(cx + std::cos(angle) * 9.0F, cy + std::sin(angle) * 9.0F),
                    D2D1::Point2F(cx + std::cos(angle) * 12.0F, cy + std::sin(angle) * 12.0F), brush, 2.0F);
            }
            break;
        }
        default:
            break;
        }
    }

    void drawTriangle(D2D1_POINT_2F first, D2D1_POINT_2F second, D2D1_POINT_2F third, ID2D1Brush* brush) {
        ComPtr<ID2D1PathGeometry> geometry;
        ComPtr<ID2D1GeometrySink> sink;
        if (FAILED(m_d2dFactory->CreatePathGeometry(geometry.ReleaseAndGetAddressOf()))
            || FAILED(geometry->Open(sink.ReleaseAndGetAddressOf()))) {
            return;
        }
        sink->BeginFigure(first, D2D1_FIGURE_BEGIN_FILLED);
        const std::array<D2D1_POINT_2F, 2> points{second, third};
        sink->AddLines(points.data(), static_cast<UINT32>(points.size()));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
        if (SUCCEEDED(sink->Close())) {
            m_renderTarget->FillGeometry(geometry.Get(), brush);
        }
    }

    void drawVolume(const Layout& layout) {
        if (m_state.customVolume) {
            drawText(L"Device volume", layout.volume, DWRITE_TEXT_ALIGNMENT_CENTER, m_disabled.Get());
            return;
        }
        const auto y = (layout.volume.top + layout.volume.bottom) * 0.5F;
        const auto rail = D2D1::RectF(layout.volume.left, y - 2.0F, layout.volume.right, y + 2.0F);
        m_renderTarget->FillRoundedRectangle(D2D1::RoundedRect(rail, 2.0F, 2.0F), m_disabled.Get());
        const auto effectiveDb = m_state.muted() ? m_state.lastAudibleVolumeDb : m_state.volumeDb;
        const auto fraction = static_cast<float>(volumeFractionFromDb(effectiveDb));
        const auto x = rail.left + (rail.right - rail.left) * fraction;
        auto level = rail;
        level.right = x;
        m_renderTarget->FillRoundedRectangle(D2D1::RoundedRect(level, 2.0F, 2.0F), m_accent.Get());
        m_renderTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x, y), 5.0F, 5.0F),
            m_state.muted() ? m_disabled.Get() : m_accent.Get());
    }

    void drawText(const std::wstring& text, D2D1_RECT_F rect, DWRITE_TEXT_ALIGNMENT alignment, ID2D1Brush* brush) {
        m_textFormat->SetTextAlignment(alignment);
        m_renderTarget->DrawTextW(text.c_str(), static_cast<UINT32>(text.size()), m_textFormat.Get(), rect, brush,
            D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }

    [[nodiscard]] std::optional<std::size_t> detailRowAt(HWND wnd, D2D1_POINT_2F point) const {
        const auto layout = calculateLayout(wnd);
        if (m_lowerRightView != LowerRightView::trackDetails || !contains(layout.lowerRight, point)) return {};
        const auto relative = point.y - layout.lowerRight.top - 4.0F + m_detailScroll;
        if (relative < 0.0F) return {};
        const auto index = static_cast<std::size_t>(relative / 22.0F);
        if (index >= m_detailRows.size() || m_detailRows[index].heading) return {};
        return index;
    }

    void selectDetailRow(std::size_t index) {
        if (index >= m_detailRows.size() || m_detailRows[index].heading) return;
        if (m_detailSelected.size() != m_detailRows.size()) m_detailSelected.resize(m_detailRows.size());
        const auto ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        const auto shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        if (shift && m_detailAnchor < m_detailRows.size()) {
            if (!ctrl) std::fill(m_detailSelected.begin(), m_detailSelected.end(), false);
            const auto first = std::min(index, m_detailAnchor);
            const auto last = std::max(index, m_detailAnchor);
            for (auto current = first; current <= last; ++current) {
                if (!m_detailRows[current].heading) m_detailSelected[current] = true;
            }
        } else if (ctrl) {
            m_detailSelected[index] = !m_detailSelected[index];
            m_detailAnchor = index;
        } else {
            std::fill(m_detailSelected.begin(), m_detailSelected.end(), false);
            m_detailSelected[index] = true;
            m_detailAnchor = index;
        }
        invalidate();
    }

    [[nodiscard]] std::wstring selectedDetailsText(bool valuesOnly = false) const {
        std::wstring text;
        for (std::size_t index = 0; index < m_detailRows.size(); ++index) {
            if (index >= m_detailSelected.size() || !m_detailSelected[index] || m_detailRows[index].heading) continue;
            if (!text.empty()) text += L"\r\n";
            if (!valuesOnly) text += m_detailRows[index].field + L": ";
            text += m_detailRows[index].value;
        }
        return text;
    }

    static void copyToClipboard(HWND owner, const std::wstring& text) {
        if (text.empty() || !OpenClipboard(owner)) return;
        EmptyClipboard();
        const auto bytes = (text.size() + 1) * sizeof(wchar_t);
        if (const auto memory = GlobalAlloc(GMEM_MOVEABLE, bytes)) {
            if (auto* destination = GlobalLock(memory)) {
                std::memcpy(destination, text.c_str(), bytes);
                GlobalUnlock(memory);
                if (!SetClipboardData(CF_UNICODETEXT, memory)) GlobalFree(memory);
            } else {
                GlobalFree(memory);
            }
        }
        CloseClipboard();
    }

    void showTrackDetailsMenu(HWND wnd, D2D1_POINT_2F point, POINT clientPoint) {
        if (m_lowerRightView != LowerRightView::trackDetails
            || !contains(calculateLayout(wnd).lowerRight, point)) return;
        if (const auto row = detailRowAt(wnd, point); row &&
            (*row >= m_detailSelected.size() || !m_detailSelected[*row])) {
            std::fill(m_detailSelected.begin(), m_detailSelected.end(), false);
            m_detailSelected[*row] = true;
            m_detailAnchor = *row;
        }
        const auto menu = CreatePopupMenu();
        if (!menu) return;
        AppendMenuW(menu, MF_STRING, 1, L"Copy value");
        AppendMenuW(menu, MF_STRING, 2, L"Copy row");
        AppendMenuW(menu, MF_STRING, 3, L"Select all");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, 4, L"Properties");
        ClientToScreen(wnd, &clientPoint);
        const auto command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
            clientPoint.x, clientPoint.y, 0, wnd, nullptr);
        DestroyMenu(menu);
        if (command == 1) copyToClipboard(wnd, selectedDetailsText(true));
        else if (command == 2) copyToClipboard(wnd, selectedDetailsText(false));
        else if (command == 3) {
            m_detailSelected.resize(m_detailRows.size());
            for (std::size_t index = 0; index < m_detailRows.size(); ++index) {
                m_detailSelected[index] = !m_detailRows[index].heading;
            }
            invalidate();
        } else if (command == 4 && m_target.is_valid()) {
            try {
                const auto manager = createRatingMenu(m_target);
                if (manager.is_valid()) {
                    if (auto* properties = findContextCommand(manager->get_root(), "Properties")) properties->execute();
                }
            } catch (...) {
            }
        }
    }

    void onMouseMove(HWND wnd, LPARAM lp) {
        if (!m_trackingMouse) {
            TRACKMOUSEEVENT event{sizeof(event), TME_LEAVE, wnd, 0};
            TrackMouseEvent(&event);
            m_trackingMouse = true;
        }
        const auto point = pointFromLParam(lp, dpiScale());
        const auto hit = hitTest(wnd, point);
        setHover(hit);
        if (m_active == ControlId::seek && m_previewing) {
            updateSeekPreview(wnd, point.x);
        } else if (m_active == ControlId::volume && !m_state.customVolume) {
            updateVolume(wnd, point.x);
        } else if (m_active == ControlId::rightDivider) {
            updateRightDivider(wnd, point.y);
        } else if (m_active == ControlId::playlistScrollbar) {
            updatePlaylistScrollbar(wnd, point.y);
        } else if (m_active == ControlId::playlistHeader) {
            updatePlaylistHeaderDrag(wnd, point.x);
        }
    }

    void onLeftButtonDown(HWND wnd, LPARAM lp) {
        SetFocus(wnd);
        const auto point = pointFromLParam(lp, dpiScale());
        const auto hit = hitTest(wnd, point);
        if (hit == ControlId::trackDetails) {
            if (const auto row = detailRowAt(wnd, point)) selectDetailRow(*row);
            m_focus = ControlId::trackDetails;
            return;
        }
        if (hit == ControlId::playlistBody) {
            onPlaylistLeftButtonDown(wnd, point);
            return;
        }
        if (hit == ControlId::playlistHeader) {
            m_headerPressedColumn = playlistColumnAt(wnd, point.x);
            m_headerResizeColumns = playlistBoundaryAt(wnd, point.x);
            m_headerStartSettings = m_playlistSettings;
            m_headerPressX = point.x;
            m_headerMoved = false;
            m_active = ControlId::playlistHeader;
            SetCapture(wnd);
            return;
        }
        if (!isEnabled(hit)) {
            return;
        }
        m_focus = hit;
        m_active = hit;
        SetCapture(wnd);
        if (hit == ControlId::seek) {
            m_previewing = true;
            updateSeekPreview(wnd, point.x);
        } else if (hit == ControlId::volume) {
            updateVolume(wnd, point.x);
        } else if (hit == ControlId::rightDivider) {
            m_dragHeaderPermille = static_cast<int>(readSettings().rightHeaderPermille);
            updateRightDivider(wnd, point.y);
        } else if (hit == ControlId::playlistScrollbar) {
            const auto layout = calculateLayout(wnd);
            if (contains(layout.playlistScrollThumb, point)) {
                m_playlistScrollbarGrabOffset = point.y - layout.playlistScrollThumb.top;
            } else {
                m_playlistScrollbarGrabOffset = (layout.playlistScrollThumb.bottom
                    - layout.playlistScrollThumb.top) * 0.5F;
                updatePlaylistScrollbar(wnd, point.y);
            }
        }
        invalidate();
    }

    void onLeftButtonUp(HWND wnd, LPARAM lp) {
        if (m_active == ControlId::none) {
            return;
        }
        const auto active = m_active;
        const auto point = pointFromLParam(lp, dpiScale());
        const auto hit = hitTest(wnd, point);
        const auto seekTarget = active == ControlId::seek
            ? committedSeekTarget(m_previewing, hit == ControlId::seek, m_state.canSeek(),
                  m_previewPosition, m_state.length)
            : std::nullopt;
        const auto committedHeaderPermille = active == ControlId::rightDivider
            ? std::clamp(m_dragHeaderPermille, 250, 800)
            : -1;
        m_active = ControlId::none;
        if (active == ControlId::seek) {
            m_previewing = false;
        }
        if (GetCapture() == wnd) {
            ReleaseCapture();
        }
        if (active == ControlId::seek) {
            if (seekTarget) {
                playback_control::get()->playback_seek(*seekTarget);
            }
        } else if (active == ControlId::rightDivider) {
            auto settings = readSettings();
            settings.rightHeaderPermille = committedHeaderPermille;
            m_dragHeaderPermille = -1;
            writeSettings(settings);
            syncLyricsHost(wnd);
        } else if (active == ControlId::playlistHeader) {
            if (m_headerResizeColumns) {
                writePlaylistViewSettings(m_playlistSettings);
            } else if (m_headerPressedColumn) {
                const auto source = *m_headerPressedColumn;
                if (m_headerMoved) {
                    if (const auto target = playlistColumnAt(wnd, point.x);
                        target && *target != source && !m_playlistSettings.columns[source].cover
                            && !m_playlistSettings.columns[*target].cover) {
                        std::swap(m_playlistSettings.columns[source], m_playlistSettings.columns[*target]);
                        writePlaylistViewSettings(m_playlistSettings);
                    }
                } else {
                    const auto& definition = m_playlistSettings.columns[source];
                    if (definition.cover) {
                        m_headerPressedColumn.reset();
                        m_headerResizeColumns.reset();
                        invalidate();
                        return;
                    }
                    const auto descending = m_sortColumnId == definition.id ? !m_sortDescending : false;
                    if (sortActivePlaylist(definition.sortFormat.empty()
                            ? definition.displayFormat : definition.sortFormat, descending)) {
                        m_sortColumnId = definition.id;
                        m_sortDescending = descending;
                    }
                }
            }
            m_headerPressedColumn.reset();
            m_headerResizeColumns.reset();
        } else if (active != ControlId::volume && active != ControlId::playlistScrollbar && active == hit) {
            activate(active);
        }
        invalidate();
    }

    void onMouseWheel(HWND wnd, WPARAM wp, LPARAM lp) {
        POINT pixels{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        ScreenToClient(wnd, &pixels);
        const auto point = D2D1::Point2F(static_cast<float>(pixels.x) / dpiScale(),
            static_cast<float>(pixels.y) / dpiScale());
        const auto layout = calculateLayout(wnd);
        if (contains(layout.playlistArea, point)) {
            const auto notches = GET_WHEEL_DELTA_WPARAM(wp) / WHEEL_DELTA;
            scrollPlaylistRows(static_cast<long long>(-notches) * 3LL);
        } else if (contains(layout.volume, point) || contains(layout.mute, point)) {
            if (GET_WHEEL_DELTA_WPARAM(wp) > 0) {
                playback_control::get()->volume_up();
            } else {
                playback_control::get()->volume_down();
            }
        } else if (m_lowerRightView == LowerRightView::trackDetails && contains(layout.lowerRight, point)) {
            const auto contentHeight = static_cast<float>(m_detailRows.size()) * 22.0F + 8.0F;
            const auto viewportHeight = layout.lowerRight.bottom - layout.lowerRight.top;
            const auto maximum = std::max(0.0F, contentHeight - viewportHeight);
            m_detailScroll = std::clamp(m_detailScroll
                    + (GET_WHEEL_DELTA_WPARAM(wp) > 0 ? -66.0F : 66.0F), 0.0F, maximum);
            invalidate();
        }
    }

    bool onKeyDown(HWND wnd, WPARAM key) {
        const std::array<ControlId, 6> allButtons{
            ControlId::settings, ControlId::previous, ControlId::playPause,
            ControlId::next, ControlId::stop, ControlId::mute};
        std::array<ControlId, 6> buttons{};
        const auto settingsVisible = readSettings().showSettingsButton;
        auto buttonCount = std::size_t{};
        for (const auto button : allButtons) {
            if (button != ControlId::settings || settingsVisible) {
                buttons[buttonCount++] = button;
            }
        }
        if (key == VK_TAB) {
            uie::window::g_on_tab(wnd);
            return true;
        }
        if (m_focus == ControlId::playlistBody && onPlaylistKeyDown(wnd, key)) return true;
        if (m_focus == ControlId::trackDetails) {
            if (key == 'C' && (GetKeyState(VK_CONTROL) & 0x8000) != 0) {
                copyToClipboard(wnd, selectedDetailsText(false));
                return true;
            }
            if (key == VK_PRIOR || key == VK_NEXT || key == VK_HOME || key == VK_END) {
                const auto layout = calculateLayout(wnd);
                const auto contentHeight = static_cast<float>(m_detailRows.size()) * 22.0F + 8.0F;
                const auto viewportHeight = layout.lowerRight.bottom - layout.lowerRight.top;
                const auto maximum = std::max(0.0F, contentHeight - viewportHeight);
                if (key == VK_HOME) m_detailScroll = 0.0F;
                else if (key == VK_END) m_detailScroll = maximum;
                else m_detailScroll = std::clamp(m_detailScroll
                        + (key == VK_PRIOR ? -viewportHeight : viewportHeight), 0.0F, maximum);
                invalidate();
                return true;
            }
        }
        if (key == VK_LEFT || key == VK_RIGHT) {
            auto current = std::find(buttons.begin(), buttons.begin() + static_cast<std::ptrdiff_t>(buttonCount), m_focus);
            auto index = current == buttons.begin() + static_cast<std::ptrdiff_t>(buttonCount)
                ? (settingsVisible ? 2LL : 1LL) : static_cast<long long>(current - buttons.begin());
            index += key == VK_RIGHT ? 1LL : -1LL;
            if (index < 0) index = static_cast<long long>(buttonCount) - 1LL;
            if (index >= static_cast<long long>(buttonCount)) index = 0;
            m_focus = buttons[static_cast<std::size_t>(index)];
            invalidate();
            return true;
        }
        if ((key == VK_RETURN || key == VK_SPACE) && isEnabled(m_focus)) {
            activate(m_focus);
            return true;
        }
        if (key == VK_ESCAPE && m_active != ControlId::none) {
            cancelInteraction();
            return true;
        }
        if (get_host()->get_keyboard_shortcuts_enabled()) {
            return uie::window::g_process_keydown_keyboard_shortcuts(key);
        }
        return false;
    }

    void activate(ControlId id) {
        if (isRatingControl(id)) {
            executeRating(ratingForControl(id));
            return;
        }
        auto api = playback_control::get();
        switch (id) {
        case ControlId::previous: api->userPrev(); break;
        case ControlId::playPause: api->play_or_pause(); break;
        case ControlId::next: api->userNext(); break;
        case ControlId::stop: api->userStop(); break;
        case ControlId::mute: api->userMute(); break;
        case ControlId::settings: showRefrainPreferences(); break;
        default: break;
        }
    }

    void updateSeekPreview(HWND wnd, float x) {
        const auto layout = calculateLayout(wnd);
        m_previewPosition = seekPosition(fractionAtX(x, layout.seek), m_state.length);
        invalidate();
    }

    void updateVolume(HWND wnd, float x) {
        const auto layout = calculateLayout(wnd);
        playback_control::get()->set_volume(volumeDbFromFraction(fractionAtX(x, layout.volume)));
    }

    void updateRightDivider(HWND wnd, float y) {
        const auto top = calculateLayout(wnd).surfaceTop;
        if (top <= 0.0F) return;
        m_dragHeaderPermille = std::clamp(static_cast<int>(std::lround(y / top * 1000.0F)), 250, 800);
        syncLyricsHost(wnd);
    }

    void cancelInteraction() {
        if (m_active == ControlId::playlistHeader && m_headerResizeColumns) {
            m_playlistSettings = m_headerStartSettings;
        }
        m_active = ControlId::none;
        m_previewing = false;
        m_dragHeaderPermille = -1;
        m_playlistScrollbarGrabOffset = 0.0F;
        m_headerPressedColumn.reset();
        m_headerResizeColumns.reset();
        if (GetCapture() == get_wnd()) {
            ReleaseCapture();
        }
        invalidate();
    }

    void setHover(ControlId id) {
        if (m_hover != id) {
            m_hover = id;
            updateTooltip(id);
            invalidate();
        }
    }

    void createTooltip(HWND wnd) {
        INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_WIN95_CLASSES};
        InitCommonControlsEx(&controls);
        m_tooltip = CreateWindowEx(WS_EX_TOPMOST, TOOLTIPS_CLASS, nullptr, WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX,
            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, wnd, nullptr, core_api::get_my_instance(), nullptr);
        if (!m_tooltip) {
            return;
        }
        TOOLINFO info{sizeof(info)};
        info.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
        info.hwnd = wnd;
        info.uId = reinterpret_cast<UINT_PTR>(wnd);
        info.lpszText = const_cast<wchar_t*>(L"");
        SendMessage(m_tooltip, TTM_ADDTOOL, 0, reinterpret_cast<LPARAM>(&info));
        SendMessage(m_tooltip, TTM_SETMAXTIPWIDTH, 0, 320);
        SendMessage(m_tooltip, TTM_ACTIVATE, readSettings().showTooltips ? TRUE : FALSE, 0);
    }

    void updateTooltip(ControlId id) {
        if (!m_tooltip || !get_wnd()) {
            return;
        }
        if (!readSettings().showTooltips) {
            m_tooltipText.clear();
            SendMessage(m_tooltip, TTM_ACTIVATE, FALSE, 0);
        } else {
            SendMessage(m_tooltip, TTM_ACTIVATE, TRUE, 0);
            switch (id) {
            case ControlId::seek: m_tooltipText = L"Seek"; break;
            case ControlId::previous: m_tooltipText = L"Previous"; break;
            case ControlId::playPause: m_tooltipText = m_state.playing && !m_state.paused ? L"Pause" : L"Play"; break;
            case ControlId::next: m_tooltipText = L"Next"; break;
            case ControlId::stop: m_tooltipText = L"Stop"; break;
            case ControlId::mute: m_tooltipText = m_state.muted() ? L"Unmute" : L"Mute"; break;
            case ControlId::volume: m_tooltipText = L"Volume (mouse wheel supported)"; break;
            case ControlId::settings: m_tooltipText = L"Refrain settings"; break;
            case ControlId::artwork:
                if (m_artworkLoading) m_tooltipText = L"Loading artwork...";
                else if (m_artworkPixels.status == ArtworkStatus::ready) m_tooltipText = L"Front cover";
                else if (m_artworkPixels.status == ArtworkStatus::missing) m_tooltipText = L"No artwork";
                else m_tooltipText = L"Artwork unavailable";
                break;
            case ControlId::trackTitle: m_tooltipText = m_trackTitle; break;
            case ControlId::artistAlbum: m_tooltipText = m_artistAlbum; break;
            case ControlId::codecBitrate: m_tooltipText = m_codecBitrate; break;
            default: m_tooltipText.clear(); break;
            }
            if (isRatingControl(id)) {
                if (!m_ratingAvailable) {
                    m_tooltipText = L"Playback Statistics rating command unavailable";
                } else {
                    const auto value = ratingForControl(id);
                    m_tooltipText = value == m_rating
                        ? L"Clear rating" : L"Set rating to " + std::to_wstring(value);
                }
            }
        }
        TOOLINFO info{sizeof(info)};
        info.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
        info.hwnd = get_wnd();
        info.uId = reinterpret_cast<UINT_PTR>(get_wnd());
        info.lpszText = m_tooltipText.data();
        SendMessage(m_tooltip, TTM_UPDATETIPTEXT, 0, reinterpret_cast<LPARAM>(&info));
    }

    void applySettingsChange() {
        const auto settings = readSettings();
        reloadPlaylistViewSettings();
        rebuildPlaylistGroups();
        clampPlaylistScroll();
        if (settings.lyricsAutoSwitch != m_autoSwitchEnabled) {
            m_autoSwitchEnabled = settings.lyricsAutoSwitch;
            m_lowerRightView = settings.lyricsAutoSwitch
                ? (playback_control::get()->is_playing() ? LowerRightView::lyrics : LowerRightView::trackDetails)
                : settings.lowerRightView;
        } else if (!settings.lyricsAutoSwitch) {
            m_lowerRightView = settings.lowerRightView;
        }
        SendMessage(m_tooltip, TTM_ACTIVATE, settings.showTooltips ? TRUE : FALSE, 0);
        if (!settings.showSettingsButton) {
            if (m_hover == ControlId::settings) m_hover = ControlId::none;
            if (m_active == ControlId::settings) m_active = ControlId::none;
            if (m_focus == ControlId::settings) m_focus = ControlId::playPause;
        }
        updateTooltip(m_hover);
        invalidate();
    }

    PlaybackState m_state;
    metadb_handle_ptr m_target;
    std::wstring m_trackTitle{L"Nothing selected"};
    std::wstring m_artistAlbum{L"Unknown artist | Unknown album"};
    std::wstring m_codecBitrate{L"Unknown codec | Unknown bitrate"};
    int m_rating{};
    bool m_ratingAvailable{};
    ArtworkPixels m_artworkPixels{ArtworkStatus::missing};
    bool m_artworkLoading{};
    std::shared_ptr<ArtworkAsyncState> m_artworkAsync{std::make_shared<ArtworkAsyncState>()};
    bool m_callbackRegistered{};
    bool m_trackingMouse{};
    bool m_previewing{};
    LowerRightView m_lowerRightView{LowerRightView::lyrics};
    bool m_autoSwitchEnabled{true};
    int m_dragHeaderPermille{-1};
    float m_detailScroll{};
    std::vector<DetailRow> m_detailRows;
    std::vector<bool> m_detailSelected;
    std::size_t m_detailAnchor{static_cast<std::size_t>(-1)};
    t_size m_activePlaylist{SIZE_MAX};
    metadb_handle_list m_playlistItems;
    pfc::bit_array_bittable m_playlistSelection;
    t_size m_playlistFocus{SIZE_MAX};
    t_size m_playlistAnchor{SIZE_MAX};
    t_size m_playingPlaylist{SIZE_MAX};
    t_size m_playingItem{SIZE_MAX};
    std::size_t m_playlistTopRow{};
    float m_playlistScrollbarGrabOffset{};
    std::unordered_map<std::size_t, PlaylistRowText> m_playlistTextCache;
    std::optional<std::size_t> m_headerPressedColumn;
    std::optional<std::pair<std::size_t, std::size_t>> m_headerResizeColumns;
    PlaylistViewSettings m_headerStartSettings{defaultPlaylistViewSettings()};
    float m_headerPressX{};
    bool m_headerMoved{};
    std::string m_sortColumnId;
    bool m_sortDescending{};
    PlaylistViewSettings m_playlistSettings{defaultPlaylistViewSettings()};
    std::vector<PlaylistGroup> m_playlistGroups;
    std::vector<PlaylistDisplayRow> m_playlistDisplayRows;
    std::vector<titleformat_object::ptr> m_playlistColumnScripts;
    std::array<titleformat_object::ptr, 5> m_playlistGroupScripts;
    std::uint64_t m_groupArtworkGeneration{};
    std::size_t m_groupArtworkBytes{};
    std::unordered_map<std::string, GroupArtworkCacheEntry> m_groupArtworkCache;
    std::vector<GroupArtworkRequest> m_groupArtworkRequests;
    std::mutex m_groupArtworkMutex;
    std::condition_variable m_groupArtworkCondition;
    std::shared_ptr<abort_callback_impl> m_groupArtworkAborter;
    std::jthread m_groupArtworkThread;
    LyricsHost m_lyricsHost;
    double m_previewPosition{};
    float m_dpi{96.0F};
    ControlId m_hover{ControlId::none};
    ControlId m_active{ControlId::none};
    ControlId m_focus{ControlId::none};
    HWND m_tooltip{};
    std::wstring m_tooltipText;

    ComPtr<ID2D1Factory> m_d2dFactory;
    ComPtr<IDWriteFactory> m_dwriteFactory;
    ComPtr<ID2D1HwndRenderTarget> m_renderTarget;
    ComPtr<ID2D1Bitmap> m_artworkBitmap;
    ComPtr<IDWriteTextFormat> m_textFormat;
    ComPtr<IDWriteTextFormat> m_titleFormat;
    ComPtr<IDWriteTextFormat> m_secondaryFormat;
    titleformat_object::ptr m_titleScript;
    titleformat_object::ptr m_artistScript;
    titleformat_object::ptr m_albumScript;
    titleformat_object::ptr m_codecScript;
    titleformat_object::ptr m_bitrateScript;
    titleformat_object::ptr m_ratingScript;
    ComPtr<ID2D1SolidColorBrush> m_background;
    ComPtr<ID2D1SolidColorBrush> m_surface;
    ComPtr<ID2D1SolidColorBrush> m_foreground;
    ComPtr<ID2D1SolidColorBrush> m_disabled;
    ComPtr<ID2D1SolidColorBrush> m_accent;
    ComPtr<ID2D1SolidColorBrush> m_light;
};

uie::window_factory<PlaybackPanel> g_playbackPanelFactory;

} // namespace

void ensurePlaybackPanelLinked() noexcept {}

} // namespace refrain

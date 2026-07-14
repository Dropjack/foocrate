#include "playback_panel.h"
#include "artwork_loader.h"
#include "album_browser_model.h"
#include "compact_track_list_model.h"
#include "lyrics_host.h"
#include "now_playing_state.h"
#include "playback_state.h"
#include "playlist_view_model.h"
#include "playlist_config_model.h"
#include "playlist_grouping_model.h"
#include "playlist_interaction_model.h"
#include "playlist_browser_model.h"
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
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <cwctype>
#include <random>
#include <memory>
#include <limits>
#include <mutex>
#include <optional>
#include <stdexcept>
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
constexpr UINT kQueueChangedMessage = WM_APP + 0x426;
constexpr UINT kDropReadyMessage = WM_APP + 0x427;
constexpr UINT kRestoreAlbumSourceMessage = WM_APP + 0x428;
constexpr UINT kCommitPlaylistRenameMessage = WM_APP + 0x429;
constexpr UINT kCancelPlaylistRenameMessage = WM_APP + 0x42A;
constexpr UINT_PTR kPlaylistDragTimer = 0x5271;
constexpr UINT_PTR kStatusTimer = 0x5272;
constexpr UINT_PTR kPlaylistFilterEditId = 0x5273;
constexpr UINT_PTR kPlaylistFilterClearId = 0x5274;

std::vector<HWND> g_queueWindows;
std::atomic_bool g_defaultPlaylistApplied{};

class QueueChangeCallback : public playback_queue_callback {
public:
    void on_changed(t_change_origin) override {
        std::erase_if(g_queueWindows, [](HWND window) { return !IsWindow(window); });
        for (const auto window : g_queueWindows) PostMessage(window, kQueueChangedMessage, 0, 0);
    }
};

service_factory_single_t<QueueChangeCallback> g_queueChangeCallbackFactory;

class AlbumViewPlaylistLock : public playlist_lock {
public:
    bool query_items_add(t_size, const pfc::list_base_const_t<metadb_handle_ptr>&, const bit_array&) override { return false; }
    bool query_items_reorder(const t_size*, t_size) override { return false; }
    bool query_items_remove(const bit_array&, bool force) override { return force; }
    bool query_item_replace(t_size, const metadb_handle_ptr&, const metadb_handle_ptr&) override { return false; }
    bool query_playlist_rename(const char*, t_size) override { return false; }
    bool query_playlist_remove() override { return false; }
    bool execute_default_action(t_size) override { return false; }
    void on_playlist_index_change(t_size index) override { m_index = index; }
    void on_playlist_remove() override { m_index = SIZE_MAX; }
    void get_lock_name(pfc::string_base& out) override { out = "Refrain Album View"; }
    void show_ui() override {}
    t_uint32 get_filter_mask() override {
        return filter_add | filter_remove | filter_reorder | filter_replace | filter_rename | filter_remove_playlist;
    }
    t_size m_index{SIZE_MAX};
};

enum class ControlId {
    none,
    seek,
    open,
    previous,
    playPause,
    next,
    stop,
    mute,
    volume,
    settings,
    playlistWorkspace,
    albumWorkspace,
    albumList,
    queueToggle,
    playbackOrder,
    outputDevice,
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
    playlistBrowser,
    playlistBrowserDivider,
    playlistRating,
    queueHeader,
    queueBody,
    queueScrollbar,
    albumSource,
    albumGrid,
    albumTrackList,
    albumDivider,
    albumLibraryPreferences,
    albumGridScrollbar,
    albumTrackScrollbar,
};

enum class Workspace { playlist, album };
enum class AlbumSourceKind { playlist, mediaLibrary };

enum class DropDestination {
    none,
    playlist,
    queue,
    playlistBrowser,
};

struct Layout {
    D2D1_RECT_F seek{};
    D2D1_RECT_F open{};
    D2D1_RECT_F previous{};
    D2D1_RECT_F playPause{};
    D2D1_RECT_F next{};
    D2D1_RECT_F stop{};
    D2D1_RECT_F mute{};
    D2D1_RECT_F volume{};
    D2D1_RECT_F settings{};
    D2D1_RECT_F playlistWorkspace{};
    D2D1_RECT_F albumWorkspace{};
    D2D1_RECT_F albumList{};
    D2D1_RECT_F queueToggle{};
    D2D1_RECT_F playbackOrder{};
    D2D1_RECT_F outputDevice{};
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
    D2D1_RECT_F playlistBrowser{};
    D2D1_RECT_F playlistBrowserHeader{};
    D2D1_RECT_F playlistBrowserFilter{};
    D2D1_RECT_F playlistBrowserFilterClear{};
    D2D1_RECT_F playlistBrowserBody{};
    D2D1_RECT_F playlistBrowserDivider{};
    D2D1_RECT_F artwork{};
    std::array<D2D1_RECT_F, 5> ratings{};
    D2D1_RECT_F trackTitle{};
    D2D1_RECT_F artistAlbum{};
    D2D1_RECT_F codecBitrate{};
    D2D1_RECT_F queuePanel{};
    D2D1_RECT_F queueHeader{};
    D2D1_RECT_F queueListHeader{};
    D2D1_RECT_F queueBody{};
    D2D1_RECT_F queueScrollTrack{};
    D2D1_RECT_F queueScrollThumb{};
    D2D1_RECT_F albumSource{};
    D2D1_RECT_F albumGrid{};
    D2D1_RECT_F albumDivider{};
    D2D1_RECT_F albumHeader{};
    D2D1_RECT_F albumTrackHeader{};
    D2D1_RECT_F albumTrackBody{};
    D2D1_RECT_F albumLibraryPreferences{};
    D2D1_RECT_F albumGridScrollTrack{};
    D2D1_RECT_F albumGridScrollThumb{};
    D2D1_RECT_F albumTrackScrollTrack{};
    D2D1_RECT_F albumTrackScrollThumb{};
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

struct QueueRowText {
    std::wstring title;
    std::wstring artist;
    std::wstring source;
    std::wstring length;
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
    bool album{};
};

struct GroupArtworkCompletion {
    std::uint64_t generation{};
    std::string key;
    ArtworkPixels pixels;
    bool album{};
};

struct GroupArtworkCacheEntry {
    bool loading{};
    ArtworkPixels pixels{ArtworkStatus::unavailable};
    ComPtr<ID2D1Bitmap> bitmap;
    std::uint64_t lastUse{};
};

struct DropAsyncState {
    std::atomic_bool alive{true};
    std::atomic_uint64_t generation{};
    HWND window{};
};

struct DropCompletion {
    std::uint64_t generation{};
    GUID playlistGuid{};
    std::size_t insertion{};
    metadb_handle_list targetSnapshot;
    metadb_handle_list items;
    bool createPlaylist{};
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

[[nodiscard]] std::string wideToUtf8(const std::wstring& text) {
    if (text.empty()) return {};
    const auto count = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(),
        static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    if (count <= 0) return {};
    std::string result(static_cast<std::size_t>(count), '\0');
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(),
            static_cast<int>(text.size()), result.data(), count, nullptr, nullptr) <= 0) return {};
    return result;
}

class PlaybackPanel : public uie::container_uie_window_v3,
                      public play_callback,
                      private playlist_callback_impl_base,
                      private metadb_io_callback_dynamic_impl_base,
                      private library_callback_v2_dynamic_impl_base,
                      private output_config_change_callback {
public:
    PlaybackPanel()
        : playlist_callback_impl_base(playlist_callback::flag_item_ops
              | playlist_callback::flag_playlist_ops
              | playlist_callback::flag_on_default_format_changed
              | playlist_callback::flag_on_playback_order_changed) {}
    ~PlaybackPanel() {
        if (m_outputCallbackRegistered) {
            output_manager_v2::get()->removeCallback(this);
            m_outputCallbackRegistered = false;
        }
        endAlbumPlaybackSession();
        releaseAlbumBridgeLock();
        stopAlbumArtworkWorker();
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
            m_albumSplitPermille = readAlbumBrowserSettings().splitPermille;
            m_playlistBrowserSplitPermille = readPlaylistBrowserSettings().splitPermille;
            restoreAlbumBridgeLock();
            refreshPlaylistBrowserSnapshot();
            applyDefaultPlaylistOnce();
            reloadPlaylistViewSettings();
            startGroupArtworkWorker();
            startAlbumArtworkWorker();
            registerPlaybackCallback();
            output_manager_v2::get()->addCallback(this);
            m_outputCallbackRegistered = true;
            rebuildPlaylistSnapshot(true);
            refreshQueueSnapshot();
            refreshFromCore();
            createTooltip(wnd);
            createPlaylistFilterControls(wnd);
            registerSettingsWindow(wnd);
            initializeLowerRightView();
            createLyricsHost(wnd);
            m_dropAsync->alive.store(true);
            m_dropAsync->window = wnd;
            m_dropTarget = new DropTargetImpl(this);
            if (FAILED(RegisterDragDrop(wnd, m_dropTarget.get_ptr()))) m_dropTarget.release();
            if (std::find(g_queueWindows.begin(), g_queueWindows.end(), wnd) == g_queueWindows.end()) {
                g_queueWindows.push_back(wnd);
            }
            return 0;
        case WM_DESTROY: {
            cancelPlaylistBrowserRename();
            std::erase(g_queueWindows, wnd);
            KillTimer(wnd, kPlaylistDragTimer);
            KillTimer(wnd, kStatusTimer);
            m_dropAsync->alive.store(false);
            m_dropAsync->window = nullptr;
            m_dropAsync->generation.fetch_add(1);
            MSG pendingDrop{};
            while (PeekMessageW(&pendingDrop, wnd, kDropReadyMessage, kDropReadyMessage, PM_REMOVE)) {
                delete reinterpret_cast<DropCompletion*>(pendingDrop.lParam);
            }
            RevokeDragDrop(wnd);
            if (m_dropTarget.is_valid()) m_dropTarget->detach();
            m_dropTarget.release();
            m_lyricsHost.destroy();
            stopAlbumArtworkWorker();
            stopGroupArtworkWorker();
            stopArtworkWork();
            unregisterSettingsWindow(wnd);
            if (m_outputCallbackRegistered) {
                output_manager_v2::get()->removeCallback(this);
                m_outputCallbackRegistered = false;
            }
            unregisterPlaybackCallback();
            m_playlistFilterEdit = nullptr;
            m_playlistFilterClear = nullptr;
            if (GetCapture() == wnd) {
                ReleaseCapture();
            }
            m_active = ControlId::none;
            m_previewing = false;
            releaseDeviceResources();
            m_tooltip = nullptr;
            return 0;
        }
        case WM_SIZE:
            if (m_renderTarget) {
                m_renderTarget->Resize(D2D1::SizeU(LOWORD(lp), HIWORD(lp)));
            }
            clampPlaylistScroll();
            clampAlbumScroll();
            syncPlaylistFilterControls(wnd);
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
            syncPlaylistFilterControls(wnd);
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
            m_albumHover.reset();
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
            if (!m_queueMode && contains(calculateLayout(wnd).rightColumn, pointFromLParam(lp, dpiScale()))) {
                toggleLowerRightView();
            }
            return 0;
        case WM_RBUTTONUP:
            onRightButtonUp(wnd, pointFromLParam(lp, dpiScale()),
                POINT{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)});
            return 0;
        case WM_CAPTURECHANGED:
            if (!m_ignoreCaptureChanged && reinterpret_cast<HWND>(lp) != wnd) {
                cancelInteraction();
            }
            return 0;
        case WM_MOUSEWHEEL:
            onMouseWheel(wnd, wp, lp);
            return 0;
        case WM_TIMER:
            if (wp == kPlaylistDragTimer) {
                updatePlaylistDragAutoscroll(wnd);
                return 0;
            }
            if (wp == kStatusTimer) {
                KillTimer(wnd, kStatusTimer);
                m_statusText.clear();
                invalidate();
                return 0;
            }
            break;
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
        case WM_COMMAND:
            if (LOWORD(wp) == kPlaylistFilterEditId && HIWORD(wp) == EN_CHANGE) {
                updatePlaylistFilterFromEditor();
                return 0;
            }
            if (LOWORD(wp) == kPlaylistFilterClearId && HIWORD(wp) == BN_CLICKED) {
                clearPlaylistFilter(true);
                return 0;
            }
            break;
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
            if (LOWORD(lp) == HTCLIENT && m_hover == ControlId::albumDivider) {
                SetCursor(LoadCursor(nullptr, IDC_SIZEWE));
                return TRUE;
            }
            if (LOWORD(lp) == HTCLIENT && m_hover == ControlId::playlistBrowserDivider) {
                SetCursor(LoadCursor(nullptr, IDC_SIZEWE));
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
            if (!m_queueMode) toggleLowerRightView();
            return 0;
        case kQueueChangedMessage:
            refreshQueueSnapshot();
            return 0;
        case kDropReadyMessage:
            applyDropCompletion(reinterpret_cast<DropCompletion*>(lp));
            return 0;
        case kRestoreAlbumSourceMessage:
            restoreAlbumSourceAfterPlaybackStart();
            return 0;
        case kCommitPlaylistRenameMessage:
            commitPlaylistBrowserRename();
            return 0;
        case kCancelPlaylistRenameMessage:
            cancelPlaylistBrowserRename();
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
        if (m_albumSessionActive) {
            t_size playlist = SIZE_MAX, item = SIZE_MAX;
            playlist_manager::get()->get_playing_item_location(&playlist, &item);
            (void)item;
            if (playlist != m_albumBridgePlaylist) {
                endAlbumPlaybackSession();
            }
            if (m_albumSourceRestorePending) {
                if (const auto wnd = get_wnd()) PostMessage(wnd, kRestoreAlbumSourceMessage, 0, 0);
            }
        }
        if (readSettings().lyricsAutoSwitch) setLowerRightView(LowerRightView::lyrics, false);
        requestRefresh();
    }
    void on_playback_stop(play_control::t_stop_reason reason) override {
        if (!m_startingAlbumSession) endAlbumPlaybackSession();
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

    void outputConfigChanged() override { requestRefresh(); }

    void on_items_added(t_size playlist, t_size, metadb_handle_list_cref, const bit_array&) override {
        if (playlist == m_activePlaylist) rebuildPlaylistSnapshot(false);
        refreshPlaylistBrowserSnapshot();
        refreshAlbumSourceIfPlaylist(playlist);
        refreshQueueSnapshot();
    }
    void on_items_reordered(t_size playlist, const t_size*, t_size) override {
        if (playlist == m_activePlaylist) rebuildPlaylistSnapshot(false);
        refreshAlbumSourceIfPlaylist(playlist);
        refreshQueueSnapshot();
    }
    void on_items_removed(t_size playlist, const bit_array&, t_size, t_size) override {
        if (playlist == m_activePlaylist) rebuildPlaylistSnapshot(false);
        refreshPlaylistBrowserSnapshot();
        refreshAlbumSourceIfPlaylist(playlist);
        refreshQueueSnapshot();
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
        refreshPlaylistBrowserSnapshot();
        rebuildPlaylistSnapshot(true);
        if (!playback_control::get()->is_playing()) requestRefresh();
    }
    void on_items_modified(t_size playlist, const bit_array&) override {
        if (playlist == m_activePlaylist) rebuildPlaylistSnapshot(false);
        refreshAlbumSourceIfPlaylist(playlist);
        requestRefresh();
    }
    void on_items_modified_fromplayback(
        t_size playlist, const bit_array& affected, play_control::t_display_level) override {
        if (playlist == m_activePlaylist) {
            for (t_size index = 0; index < m_playlistItems.get_count(); ++index) {
                if (affected.get(index)) m_playlistTextCache.erase(index);
            }
            invalidate();
        }
        requestRefresh();
    }
    void on_items_replaced(t_size playlist, const bit_array&,
        const pfc::list_base_const_t<playlist_callback::t_on_items_replaced_entry>&) override {
        if (playlist == m_activePlaylist) rebuildPlaylistSnapshot(false);
        refreshAlbumSourceIfPlaylist(playlist);
    }
    void on_item_ensure_visible(t_size playlist, t_size item) override {
        if (playlist == m_activePlaylist) ensurePlaylistRowVisible(item);
    }
    void on_playlist_locked(t_size playlist, bool) override {
        refreshPlaylistBrowserSnapshot();
        if (playlist == m_activePlaylist) invalidate();
    }
    void on_playback_order_changed(t_size order) override {
        if (m_albumSessionActive && !m_restoringAlbumPlaybackOrder) {
            m_albumPriorPlaybackOrder = order;
            if (playlist_manager::get()->playback_order_get_count() > 0 && order != 0) {
                m_restoringAlbumPlaybackOrder = true;
                playlist_manager::get()->playback_order_set_active(0);
                m_restoringAlbumPlaybackOrder = false;
            }
        }
        invalidate();
    }
    void on_playlist_renamed(t_size playlist, const char*, t_size) override {
        refreshPlaylistBrowserSnapshot();
        refreshAlbumSourceIfPlaylist(playlist);
    }
    void on_playlists_removed(const bit_array&, t_size, t_size) override {
        refreshPlaylistBrowserSnapshot();
        repairDefaultPlaylist();
        if (m_workspace == Workspace::album && m_albumSourceKind == AlbumSourceKind::playlist) {
            loadAlbumSourcePlaylist(m_albumSourcePlaylistGuid);
        }
    }
    void on_playlist_created(t_size, const char*, t_size) override { refreshPlaylistBrowserSnapshot(); }
    void on_playlists_reorder(const t_size*, t_size) override { refreshPlaylistBrowserSnapshot(); }
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

    void on_items_added(metadb_handle_list_cref) override { refreshAlbumLibrarySource(); }
    void on_items_removed(metadb_handle_list_cref) override { refreshAlbumLibrarySource(); }
    void on_items_modified_v2(metadb_handle_list_cref, metadb_io_callback_v2_data&) override {
        refreshAlbumLibrarySource();
    }
    void on_library_initialized() override { refreshAlbumLibrarySource(); }

private:
    class DropTargetImpl : public ImplementCOMRefCounter<IDropTarget> {
    public:
        explicit DropTargetImpl(PlaybackPanel* owner) : m_owner(owner) {}
        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void** out) override {
            if (!out) return E_POINTER;
            *out = nullptr;
            if (id == IID_IUnknown || id == IID_IDropTarget) {
                *out = static_cast<IDropTarget*>(this);
                AddRef();
                return S_OK;
            }
            return E_NOINTERFACE;
        }

        void detach() noexcept { m_owner = nullptr; m_data.release(); }

        HRESULT STDMETHODCALLTYPE DragEnter(IDataObject* data, DWORD keys, POINTL point, DWORD* effect) override {
            if (!m_owner || !data || !effect) return E_INVALIDARG;
            m_data = data;
            return m_owner->dragEnter(data, keys, point, effect);
        }
        HRESULT STDMETHODCALLTYPE DragOver(DWORD keys, POINTL point, DWORD* effect) override {
            if (!m_owner || !effect) return E_INVALIDARG;
            return m_owner->dragOver(m_data.get_ptr(), keys, point, effect);
        }
        HRESULT STDMETHODCALLTYPE DragLeave() override {
            if (m_owner) m_owner->dragLeave();
            m_data.release();
            return S_OK;
        }
        HRESULT STDMETHODCALLTYPE Drop(IDataObject* data, DWORD keys, POINTL point, DWORD* effect) override {
            if (!m_owner || !data || !effect) return E_INVALIDARG;
            const auto result = m_owner->dropData(data, keys, point, effect);
            m_data.release();
            return result;
        }
    private:
        PlaybackPanel* m_owner{};
        pfc::com_ptr_t<IDataObject> m_data;
    };

    class DropSourceImpl : public ImplementCOMRefCounter<IDropSource> {
    public:
        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void** out) override {
            if (!out) return E_POINTER;
            *out = nullptr;
            if (id == IID_IUnknown || id == IID_IDropSource) {
                *out = static_cast<IDropSource*>(this);
                AddRef();
                return S_OK;
            }
            return E_NOINTERFACE;
        }
        HRESULT STDMETHODCALLTYPE GiveFeedback(DWORD) override { return DRAGDROP_S_USEDEFAULTCURSORS; }
        HRESULT STDMETHODCALLTYPE QueryContinueDrag(BOOL escape, DWORD keys) override {
            if (escape || (keys & MK_RBUTTON) != 0) return DRAGDROP_S_CANCEL;
            return (keys & MK_LBUTTON) != 0 ? S_OK : DRAGDROP_S_DROP;
        }
    };

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
        m_lyricsHost.setVisible(m_workspace == Workspace::playlist && !m_queueMode
            && m_lowerRightView == LowerRightView::lyrics);
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

    void toggleQueueMode() {
        if (m_workspace == Workspace::album) {
            setWorkspace(Workspace::playlist);
            m_queueMode = true;
            if (const auto wnd = get_wnd()) syncLyricsHost(wnd);
            invalidate();
            return;
        }
        m_queueMode = !m_queueMode;
        m_queueSelected.assign(m_queueItems.size(), false);
        m_queueFocus = m_queueItems.empty() ? SIZE_MAX : 0;
        m_queueTopRow = 0;
        if (const auto wnd = get_wnd()) syncLyricsHost(wnd);
        invalidate();
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

    void showStatus(std::wstring text) {
        m_statusText = std::move(text);
        if (const auto wnd = get_wnd()) SetTimer(wnd, kStatusTimer, 4000, nullptr);
        invalidate();
    }

    void clearPlaylistTextCache() {
        m_playlistTextCache.clear();
        invalidate();
    }

    void refreshQueueSnapshot() {
        pfc::list_t<t_playback_queue_item> contents;
        playlist_manager::get()->queue_get_contents(contents);
        m_queueItems.clear();
        m_queueItems.reserve(contents.get_count());
        for (t_size index = 0; index < contents.get_count(); ++index) m_queueItems.push_back(contents[index]);
        m_queueTextCache.clear();
        m_queueSelected.resize(m_queueItems.size(), false);
        if (m_queueFocus >= m_queueItems.size()) m_queueFocus = m_queueItems.empty() ? SIZE_MAX : m_queueItems.size() - 1;
        clampQueueScroll();
        invalidate();
    }

    [[nodiscard]] bool queueMatches(const std::vector<t_playback_queue_item>& expected) const {
        pfc::list_t<t_playback_queue_item> contents;
        playlist_manager::get()->queue_get_contents(contents);
        if (contents.get_count() != expected.size()) return false;
        for (t_size index = 0; index < contents.get_count(); ++index) {
            if (contents[index] != expected[index]) return false;
        }
        return true;
    }

    [[nodiscard]] std::vector<std::size_t> queuePositionsForTrack(std::size_t track) const {
        std::vector<std::size_t> result;
        if (track >= m_playlistItems.get_count()) return result;
        for (std::size_t index = 0; index < m_queueItems.size(); ++index) {
            const auto& item = m_queueItems[index];
            if (item.m_playlist == m_activePlaylist && item.m_item == track
                && item.m_handle == m_playlistItems[track]) result.push_back(index);
        }
        return result;
    }

    [[nodiscard]] std::wstring formatQueueHandle(
        const metadb_handle_ptr& handle, const titleformat_object::ptr& script, const wchar_t* fallback = L"") const {
        if (handle.is_empty() || script.is_empty()) return fallback;
        pfc::string8 output;
        if (!handle->format_title(nullptr, output, script, nullptr)) return fallback;
        auto text = utf8ToWide(output.c_str());
        return text.empty() ? fallback : text;
    }

    [[nodiscard]] QueueRowText formatQueueRow(std::size_t index) {
        if (const auto found = m_queueTextCache.find(index); found != m_queueTextCache.end()) return found->second;
        QueueRowText row;
        if (index < m_queueItems.size()) {
            const auto& item = m_queueItems[index];
            row.title = formatQueueHandle(item.m_handle, m_titleScript, L"Unknown title");
            row.artist = formatQueueHandle(item.m_handle, m_artistScript, L"Unknown artist");
            row.length = formatQueueHandle(item.m_handle, m_queueLengthScript, L"");
            pfc::string8 name;
            if (item.m_playlist != SIZE_MAX && item.m_playlist < playlist_manager::get()->get_playlist_count()
                && playlist_manager::get()->playlist_get_name(item.m_playlist, name)) {
                row.source = utf8ToWide(name.c_str());
            } else {
                row.source = L"External item";
            }
        }
        if (m_queueTextCache.size() >= 256) m_queueTextCache.clear();
        m_queueTextCache.emplace(index, row);
        return row;
    }

    void clampQueueScroll() {
        const auto wnd = get_wnd();
        if (!wnd) { m_queueTopRow = 0; return; }
        const auto body = calculateLayout(wnd).queueBody;
        const auto capacity = visibleRowCapacity(body.bottom - body.top, kCompactTrackRowHeight);
        m_queueTopRow = clampTopRow(m_queueTopRow, m_queueItems.size(), capacity);
    }

    void removeSelectedQueueItems() {
        if (m_queueItems.empty() || m_queueSelected.empty()) return;
        pfc::bit_array_bittable mask;
        mask.resize(m_queueItems.size());
        bool any{};
        for (std::size_t index = 0; index < m_queueItems.size(); ++index) {
            if (index < m_queueSelected.size() && m_queueSelected[index]) {
                mask.set(index, true);
                any = true;
            }
        }
        if (any) playlist_manager::get()->queue_remove_mask(mask);
    }

    bool rebuildQueueInOrder(const std::vector<std::size_t>& order,
        const std::vector<t_playback_queue_item>& snapshot) {
        if (order.size() != snapshot.size() || !queueMatches(snapshot)) return false;
        auto api = playlist_manager::get();
        api->queue_flush();
        for (const auto source : order) {
            if (source >= snapshot.size()) continue;
            const auto& item = snapshot[source];
            bool addedByLocation{};
            if (item.m_playlist != SIZE_MAX && item.m_playlist < api->get_playlist_count()
                && item.m_item < api->playlist_get_item_count(item.m_playlist)) {
                metadb_handle_ptr current;
                api->playlist_get_item_handle(current, item.m_playlist, item.m_item);
                if (current == item.m_handle) {
                    api->queue_add_item_playlist(item.m_playlist, item.m_item);
                    addedByLocation = true;
                }
            }
            if (!addedByLocation) api->queue_add_item(item.m_handle);
        }
        refreshQueueSnapshot();
        return true;
    }

    void playQueueItemNow(std::size_t index) {
        if (index >= m_queueItems.size()) return;
        const auto snapshot = m_queueItems;
        const auto order = prioritizeIndexOrder(snapshot.size(), index);
        if (!rebuildQueueInOrder(order, snapshot)) {
            showStatus(L"The playback queue changed before this item could be played.");
            return;
        }
        playback_control::get()->userNext();
    }

    void addPlaylistSelectionToQueue() {
        if (m_activePlaylist == SIZE_MAX || !playlistSnapshotMatches(m_playlistDragSnapshot)) return;
        auto api = playlist_manager::get();
        for (std::size_t index = 0; index < m_playlistDragSelection.size(); ++index) {
            if (m_playlistDragSelection[index]) api->queue_add_item_playlist(m_activePlaylist, index);
        }
        refreshQueueSnapshot();
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
            if (!column.cover && column.id != "builtin.state"
                && !compiler->compile(script, column.displayFormat.c_str())) {
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
        auto nextGroups = buildPlaylistGroups(inputs, m_playlistSettings.collapseByDefault);
        const auto artworkSource = activeGroupDefinition().artwork;
        const auto artworkIdentityChanged = artworkSource != m_groupArtworkSource
            || nextGroups.size() != m_playlistGroups.size()
            || !std::equal(nextGroups.begin(), nextGroups.end(), m_playlistGroups.begin(), m_playlistGroups.end(),
                [](const PlaylistGroup& left, const PlaylistGroup& right) {
                    return left.key == right.key && left.start == right.start && left.count == right.count;
                });
        if (artworkIdentityChanged) {
            ++m_groupArtworkGeneration;
            {
                std::scoped_lock lock(m_groupArtworkMutex);
                m_groupArtworkRequests.clear();
                if (m_groupArtworkAborter && !m_groupArtworkAborterAlbum) m_groupArtworkAborter->abort();
            }
            m_groupArtworkCache.clear();
            m_groupArtworkBytes = 0;
            m_groupArtworkSource = artworkSource;
        }
        m_playlistGroups = std::move(nextGroups);
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

    [[nodiscard]] std::string formatHandleUtf8(
        const metadb_handle_ptr& handle, const titleformat_object::ptr& script) const {
        if (handle.is_empty() || script.is_empty()) return {};
        pfc::string8 output;
        return handle->format_title(nullptr, output, script, nullptr) ? std::string(output.c_str()) : std::string{};
    }

    void rebuildAlbumModel() {
        const auto previousKey = m_albumSelection < m_albums.size() ? m_albums[m_albumSelection].key : std::string{};
        std::vector<AlbumSourceTrack> input;
        input.reserve(m_albumSourceItems.get_count());
        for (std::size_t index = 0; index < m_albumSourceItems.get_count(); ++index) {
            const auto& item = m_albumSourceItems[index];
            const auto parseNumber = [&](const titleformat_object::ptr& script) {
                const auto text = formatHandleUtf8(item, script);
                const auto value = std::strtoul(text.c_str(), nullptr, 10);
                return value > std::numeric_limits<unsigned>::max() ? 0U : static_cast<unsigned>(value);
            };
            input.push_back({index, formatHandleUtf8(item, m_albumNameScript),
                formatHandleUtf8(item, m_albumArtistScript), formatHandleUtf8(item, m_titleScript),
                formatHandleUtf8(item, m_artistScript), formatHandleUtf8(item, m_albumDateScript),
                parseNumber(m_albumDiscScript), parseNumber(m_albumTrackScript)});
        }
        m_albumInput = std::move(input);
        m_albums = buildAlbumBrowserAlbums(m_albumInput);
        m_albumSelection = 0;
        if (!previousKey.empty()) {
            const auto found = std::find_if(m_albums.begin(), m_albums.end(), [&](const auto& album) {
                return album.key == previousKey;
            });
            if (found != m_albums.end()) m_albumSelection = static_cast<std::size_t>(found - m_albums.begin());
        }
        m_albumTrackSelection = 0;
        m_albumGridTopRow = 0;
        ++m_albumArtworkGeneration;
        {
            std::scoped_lock lock(m_albumArtworkMutex);
            m_albumArtworkRequests.clear();
            if (m_albumArtworkAborter) m_albumArtworkAborter->abort();
        }
        m_albumArtworkCache.clear();
        m_albumArtworkBytes = 0;
        invalidate();
    }

    [[nodiscard]] std::string albumSourceIdentity() const {
        if (m_albumSourceKind == AlbumSourceKind::mediaLibrary) return "library";
        return std::string("playlist:") + std::string(reinterpret_cast<const char*>(&m_albumSourcePlaylistGuid),
            sizeof(m_albumSourcePlaylistGuid));
    }

    void rememberAlbumSourceState() {
        if (m_albums.empty() || m_albumSelection >= m_albums.size()) return;
        m_albumSourceStates[albumSourceIdentity()] = {m_albums[m_albumSelection].key, m_albumGridTopRow};
    }

    void persistAlbumBrowserState() {
        auto saved = readAlbumBrowserSettings();
        saved.mediaLibrary = m_albumSourceKind == AlbumSourceKind::mediaLibrary;
        saved.playlistGuid = m_albumSourcePlaylistGuid;
        saved.splitPermille = m_albumSplitPermille;
        saved.albumKey = m_albumSelection < m_albums.size() ? m_albums[m_albumSelection].key : std::string{};
        writeAlbumBrowserSettings(saved);
    }

    void restoreAlbumSourceState() {
        const auto found = m_albumSourceStates.find(albumSourceIdentity());
        if (found == m_albumSourceStates.end()) return;
        const auto album = std::find_if(m_albums.begin(), m_albums.end(), [&](const auto& value) {
            return value.key == found->second.first;
        });
        if (album != m_albums.end()) m_albumSelection = static_cast<std::size_t>(album - m_albums.begin());
        m_albumGridTopRow = found->second.second;
        if (const auto wnd = get_wnd()) {
            const auto layout = calculateLayout(wnd);
            const auto columns = albumGridColumns(layout);
            const auto rows = (m_albums.size() + columns - 1) / columns;
            const auto visible = std::max<std::size_t>(1, static_cast<std::size_t>(
                std::max(0.0F, layout.albumGrid.bottom - layout.albumGrid.top) / 184.0F));
            m_albumGridTopRow = std::min(m_albumGridTopRow, rows > visible ? rows - visible : 0);
        }
    }

    void resetAlbumTrackSelection() {
        const auto count = m_albumSelection < m_albums.size()
            ? m_albums[m_albumSelection].sourceIndices.size() : 0;
        m_albumTrackSelected.assign(count, false);
        m_albumTrackSelection = count > 0 ? 0 : SIZE_MAX;
        m_albumTrackAnchor = m_albumTrackSelection;
        if (count > 0) m_albumTrackSelected[0] = true;
        m_albumTrackTopRow = 0;
    }

    void clampAlbumScroll() {
        const auto wnd = get_wnd();
        if (!wnd || m_workspace != Workspace::album) return;
        const auto layout = calculateLayout(wnd);
        const auto columns = albumGridColumns(layout);
        const auto rows = (m_albums.size() + columns - 1) / columns;
        const auto visible = std::max<std::size_t>(1, static_cast<std::size_t>(
            (layout.albumGrid.bottom - layout.albumGrid.top) / 184.0F));
        m_albumGridTopRow = std::min(m_albumGridTopRow, rows > visible ? rows - visible : 0);
        const auto count = m_albumSelection < m_albums.size()
            ? m_albums[m_albumSelection].sourceIndices.size() : 0;
        const auto capacity = visibleRowCapacity(layout.albumTrackBody.bottom - layout.albumTrackBody.top,
            kCompactTrackRowHeight);
        m_albumTrackTopRow = clampTopRow(m_albumTrackTopRow, count, capacity);
    }

    void selectAlbumTrack(std::size_t row, bool ctrl, bool shift) {
        if (m_albumSelection >= m_albums.size()) return;
        const auto count = m_albums[m_albumSelection].sourceIndices.size();
        if (row >= count) return;
        if (m_albumTrackSelected.size() != count) m_albumTrackSelected.assign(count, false);
        if (shift && m_albumTrackAnchor < count) {
            if (!ctrl) std::fill(m_albumTrackSelected.begin(), m_albumTrackSelected.end(), false);
            const auto first = std::min(row, m_albumTrackAnchor);
            const auto last = std::max(row, m_albumTrackAnchor);
            for (auto index = first; index <= last; ++index) m_albumTrackSelected[index] = true;
        } else if (ctrl) {
            m_albumTrackSelected[row] = !m_albumTrackSelected[row];
            m_albumTrackAnchor = row;
        } else {
            std::fill(m_albumTrackSelected.begin(), m_albumTrackSelected.end(), false);
            m_albumTrackSelected[row] = true;
            m_albumTrackAnchor = row;
        }
        m_albumTrackSelection = row;
        invalidate();
    }

    void loadAlbumSourcePlaylist(const GUID& guid) {
        rememberAlbumSourceState();
        m_albumSourceKind = AlbumSourceKind::playlist;
        m_albumSourcePlaylistGuid = guid;
        m_albumReturnPlaylistGuid = guid;
        m_albumSourceItems.remove_all();
        const auto index = playlist_manager_v5::get()->find_playlist_by_guid(guid);
        pfc::string8 name;
        if (index != SIZE_MAX) {
            playlist_manager::get()->playlist_get_all_items(index, m_albumSourceItems);
            if (playlist_manager::get()->playlist_get_name(index, name)) m_albumSourceName = utf8ToWide(name.c_str());
        } else {
            loadAlbumSourceLibrary();
            return;
        }
        m_albumSelection = SIZE_MAX;
        rebuildAlbumModel();
        restoreAlbumSourceState();
        const auto saved = readAlbumBrowserSettings();
        if (!saved.mediaLibrary && InlineIsEqualGUID(saved.playlistGuid, guid) && !saved.albumKey.empty()) {
            const auto album = std::find_if(m_albums.begin(), m_albums.end(), [&](const auto& value) {
                return value.key == saved.albumKey;
            });
            if (album != m_albums.end()) m_albumSelection = static_cast<std::size_t>(album - m_albums.begin());
        }
        resetAlbumTrackSelection();
        persistAlbumBrowserState();
    }

    void loadAlbumSourceLibrary() {
        rememberAlbumSourceState();
        m_albumSourceKind = AlbumSourceKind::mediaLibrary;
        m_albumSourceName = library_manager::get()->is_library_enabled()
            ? L"Media Library" : L"Media Library unavailable";
        m_albumSourceItems.remove_all();
        if (library_manager::get()->is_library_enabled()) library_manager::get()->get_all_items(m_albumSourceItems);
        m_albumSelection = SIZE_MAX;
        rebuildAlbumModel();
        restoreAlbumSourceState();
        const auto saved = readAlbumBrowserSettings();
        if (saved.mediaLibrary && !saved.albumKey.empty()) {
            const auto album = std::find_if(m_albums.begin(), m_albums.end(), [&](const auto& value) {
                return value.key == saved.albumKey;
            });
            if (album != m_albums.end()) m_albumSelection = static_cast<std::size_t>(album - m_albums.begin());
        }
        resetAlbumTrackSelection();
        persistAlbumBrowserState();
    }

    void refreshAlbumSourceIfPlaylist(t_size playlist) {
        if (m_workspace != Workspace::album || m_albumSourceKind != AlbumSourceKind::playlist) return;
        const auto api = playlist_manager_v5::get();
        if (playlist < api->get_playlist_count()
            && InlineIsEqualGUID(api->playlist_get_guid(playlist), m_albumSourcePlaylistGuid)) {
            loadAlbumSourcePlaylist(m_albumSourcePlaylistGuid);
        }
    }

    void refreshAlbumLibrarySource() {
        if (m_workspace == Workspace::album && m_albumSourceKind == AlbumSourceKind::mediaLibrary) {
            loadAlbumSourceLibrary();
        }
    }

    void setWorkspace(Workspace workspace) {
        if (m_workspace == workspace) return;
        if (workspace == Workspace::album) {
            auto api = playlist_manager_v5::get();
            m_playlistQueueModeBeforeAlbum = m_queueMode;
            const auto active = api->get_active_playlist();
            const auto initialAlbumSource = !m_albumWorkspaceEntered
                ? api->find_playlist_by_guid(m_initialAlbumSourceGuid) : SIZE_MAX;
            if (initialAlbumSource != SIZE_MAX) {
                m_albumReturnPlaylistGuid = active != SIZE_MAX
                    ? api->playlist_get_guid(active) : m_initialAlbumSourceGuid;
                loadAlbumSourcePlaylist(m_initialAlbumSourceGuid);
            } else if (active != SIZE_MAX) {
                m_albumReturnPlaylistGuid = api->playlist_get_guid(active);
                loadAlbumSourcePlaylist(m_albumReturnPlaylistGuid);
            }
            else {
                const auto saved = readAlbumBrowserSettings();
                if (!saved.mediaLibrary && playlist_manager_v5::get()->find_playlist_by_guid(saved.playlistGuid) != SIZE_MAX) {
                    loadAlbumSourcePlaylist(saved.playlistGuid);
                } else loadAlbumSourceLibrary();
            }
            m_albumWorkspaceEntered = true;
            m_queueMode = false;
        } else {
            m_queueMode = m_playlistQueueModeBeforeAlbum;
            const auto source = playlist_manager_v5::get()->find_playlist_by_guid(m_albumReturnPlaylistGuid);
            if (source != SIZE_MAX) playlist_manager::get()->set_active_playlist(source);
        }
        m_workspace = workspace;
        if (workspace == Workspace::playlist) rebuildPlaylistSnapshot(false);
        if (const auto wnd = get_wnd()) {
            syncPlaylistFilterControls(wnd);
            syncLyricsHost(wnd);
        }
        invalidate();
    }

    void releaseAlbumBridgeLock() noexcept {
        if (m_albumLock.is_empty() || !core_api::is_main_thread()) return;
        try {
            if (m_albumLock->m_index != SIZE_MAX) {
                playlist_manager::get()->playlist_lock_uninstall(m_albumLock->m_index, m_albumLock);
            }
        } catch (...) {
        }
        m_albumLock.release();
    }

    void restoreAlbumBridgeLock() {
        const auto saved = readAlbumBrowserSettings();
        const auto playlist = playlist_manager_v5::get()->find_playlist_by_guid(saved.bridgeGuid);
        if (playlist == SIZE_MAX || playlist_manager::get()->playlist_lock_is_present(playlist)) return;
        if (m_albumLock.is_empty()) m_albumLock = new service_impl_t<AlbumViewPlaylistLock>();
        if (playlist_manager::get()->playlist_lock_install(playlist, m_albumLock)) {
            m_albumLock->m_index = playlist;
            m_albumBridgePlaylist = playlist;
        }
    }

    [[nodiscard]] t_size ensureAlbumBridgePlaylist() {
        auto api = playlist_manager::get();
        auto saved = readAlbumBrowserSettings();
        t_size playlist = playlist_manager_v5::get()->find_playlist_by_guid(saved.bridgeGuid);
        pfc::string8 name;
        if (playlist == SIZE_MAX) {
            for (t_size index = 0; index < api->get_playlist_count(); ++index) {
                if (api->playlist_get_name(index, name) && std::strcmp(name.c_str(), "Refrain Album View") == 0
                    && !api->playlist_lock_is_present(index)) {
                    playlist = index;
                    break;
                }
            }
        }
        if (playlist == SIZE_MAX) playlist = api->create_playlist("Refrain Album View", SIZE_MAX, SIZE_MAX);
        if (playlist == SIZE_MAX) return SIZE_MAX;
        saved.bridgeGuid = playlist_manager_v5::get()->playlist_get_guid(playlist);
        writeAlbumBrowserSettings(saved);
        if (m_albumLock.is_empty()) m_albumLock = new service_impl_t<AlbumViewPlaylistLock>();
        if (m_albumLock->m_index != SIZE_MAX && m_albumLock->m_index != playlist) {
            api->playlist_lock_uninstall(m_albumLock->m_index, m_albumLock);
            m_albumLock->m_index = SIZE_MAX;
        }
        if (api->playlist_lock_is_present(playlist)) {
            if (m_albumLock->m_index != playlist) return SIZE_MAX;
            if (!api->playlist_lock_uninstall(playlist, m_albumLock)) return SIZE_MAX;
            m_albumLock->m_index = SIZE_MAX;
        }
        api->playlist_clear(playlist);
        return playlist;
    }

    [[nodiscard]] bool playbackOrderIsShuffle() const {
        const auto api = playlist_manager::get();
        const auto active = api->playback_order_get_active();
        if (active >= api->playback_order_get_count()) return false;
        auto name = std::string(api->playback_order_get_name(active));
        std::transform(name.begin(), name.end(), name.begin(), [](unsigned char value) {
            return static_cast<char>(std::tolower(value));
        });
        return name.find("shuffle") != std::string::npos || name.find("random") != std::string::npos;
    }

    void endAlbumPlaybackSession() {
        if (!m_albumSessionActive) return;
        m_albumSessionActive = false;
        auto api = playlist_manager::get();
        if (m_albumPriorPlaybackOrder < api->playback_order_get_count()) {
            m_restoringAlbumPlaybackOrder = true;
            api->playback_order_set_active(m_albumPriorPlaybackOrder);
            m_restoringAlbumPlaybackOrder = false;
        }
        m_albumPriorPlaybackOrder = SIZE_MAX;
    }

    void restoreAlbumSourceAfterPlaybackStart() {
        if (!m_albumSourceRestorePending) return;
        m_albumSourceRestorePending = false;
        const auto source = playlist_manager_v5::get()->find_playlist_by_guid(m_albumPendingReturnPlaylistGuid);
        if (source != SIZE_MAX) playlist_manager::get()->set_active_playlist(source);
        rebuildPlaylistSnapshot(false);
    }

    void playAlbumTrack(std::size_t row) {
        if (m_albumSelection >= m_albums.size()) return;
        const auto& album = m_albums[m_albumSelection];
        if (row >= album.sourceIndices.size()) return;
        const auto sourceGuid = m_albumReturnPlaylistGuid;
        const auto shuffle = playbackOrderIsShuffle();
        auto order = album.sourceIndices;
        std::size_t start = row;
        if (shuffle) {
            const auto clicked = order[row];
            order.erase(order.begin() + static_cast<std::ptrdiff_t>(row));
            std::mt19937 generator(std::random_device{}());
            std::shuffle(order.begin(), order.end(), generator);
            order.insert(order.begin(), clicked);
            start = 0;
        }
        metadb_handle_list handles;
        for (const auto source : order) {
            if (source < m_albumSourceItems.get_count()) handles.add_item(m_albumSourceItems[source]);
        }
        if (handles.get_count() == 0) return;
        auto api = playlist_manager::get();
        const auto bridge = ensureAlbumBridgePlaylist();
        if (bridge == SIZE_MAX) {
            showStatus(L"Refrain Album View is locked by another component.");
            return;
        }
        if (!api->playlist_add_items(bridge, handles, pfc::bit_array_false())) {
            showStatus(L"foobar2000 could not prepare Refrain Album View.");
            return;
        }
        m_albumBridgePlaylist = bridge;
        m_albumPriorPlaybackOrder = api->playback_order_get_active();
        // foobar2000 defines index zero as its built-in Default order; using it prevents
        // Repeat/Shuffle modes from escaping the finite album session.
        if (api->playback_order_get_count() > 0) api->playback_order_set_active(0);
        m_albumSessionActive = true;
        m_startingAlbumSession = true;
        m_albumSourceRestorePending = true;
        m_albumPendingReturnPlaylistGuid = sourceGuid;
        api->set_active_playlist(bridge);
        const auto started = api->playlist_execute_default_action(bridge, start);
        const auto locked = started && api->playlist_lock_install(bridge, m_albumLock);
        if (locked) m_albumLock->m_index = bridge;
        m_startingAlbumSession = false;
        if (!started || !locked) {
            if (started) playback_control::get()->stop();
            endAlbumPlaybackSession();
            showStatus(started
                ? L"Refrain could not protect its album playback list."
                : L"foobar2000 could not start this album.");
            restoreAlbumSourceAfterPlaybackStart();
        }
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
            // Selection and auto-collapse callbacks may rebuild m_playlistDisplayRows.
            // Keep a value copy so the drag identity cannot become a dangling row reference.
            const auto display = m_playlistDisplayRows[*row];
            if (display.kind == PlaylistDisplayRowKind::groupHeader) {
                selectPlaylistGroup(display.group, ctrl, shift);
                applyAutoCollapse(display.group);
            } else if (display.kind == PlaylistDisplayRowKind::track) {
                refreshPlaylistSelection();
                const auto preserveForPossibleDrag = !ctrl && !shift
                    && display.track < m_playlistItems.get_count() && m_playlistSelection.get(display.track);
                if (preserveForPossibleDrag) {
                    playlist_manager::get()->playlist_set_focus_item(m_activePlaylist, display.track);
                    m_playlistFocus = display.track;
                    m_playlistCollapseSelectionOnClick = true;
                } else {
                    selectPlaylistRow(display.track, ctrl, shift);
                    m_playlistCollapseSelectionOnClick = false;
                }
                applyAutoCollapse(display.group);
                m_playlistDragCandidate = display.track;
                m_playlistDragSnapshot = m_playlistItems;
                m_playlistDragSelection.assign(m_playlistItems.get_count(), false);
                refreshPlaylistSelection();
                for (std::size_t index = 0; index < m_playlistItems.get_count(); ++index) {
                    m_playlistDragSelection[index] = m_playlistSelection.get(index);
                }
                m_dragStartPoint = point;
                m_playlistDragPoint = point;
                m_playlistInsertion = display.track;
                m_active = ControlId::playlistBody;
                SetCapture(wnd);
            }
        } else if (!ctrl && !shift) clearPlaylistSelection();
        invalidate();
    }

    [[nodiscard]] std::size_t playlistInsertionAt(HWND wnd, D2D1_POINT_2F point) const {
        const auto body = calculateLayout(wnd).playlistBody;
        if (point.y <= body.top) return m_playlistTopRow < m_playlistDisplayRows.size()
            ? m_playlistDisplayRows[m_playlistTopRow].track : 0;
        if (point.y >= body.bottom) return m_playlistItems.get_count();
        const auto displayIndex = m_playlistTopRow
            + static_cast<std::size_t>((point.y - body.top) / 26.0F);
        if (displayIndex >= m_playlistDisplayRows.size()) return m_playlistItems.get_count();
        const auto& display = m_playlistDisplayRows[displayIndex];
        const auto lower = std::fmod(point.y - body.top, 26.0F) >= 13.0F;
        if (display.kind == PlaylistDisplayRowKind::groupHeader) {
            const auto& group = m_playlistGroups[display.group];
            return lower ? group.start + group.count : group.start;
        }
        if (display.kind == PlaylistDisplayRowKind::spacer) {
            const auto& group = m_playlistGroups[display.group];
            return group.start + group.count;
        }
        return insertionBoundaryForRow(display.track, lower, m_playlistItems.get_count());
    }

    [[nodiscard]] std::size_t playlistDisplayBoundaryAt(HWND wnd, D2D1_POINT_2F point) const {
        const auto body = calculateLayout(wnd).playlistBody;
        if (point.y <= body.top) return m_playlistTopRow;
        if (point.y >= body.bottom) return m_playlistDisplayRows.size();
        const auto displayIndex = m_playlistTopRow
            + static_cast<std::size_t>((point.y - body.top) / 26.0F);
        if (displayIndex >= m_playlistDisplayRows.size()) return m_playlistDisplayRows.size();
        const auto lower = std::fmod(point.y - body.top, 26.0F) >= 13.0F;
        const auto& display = m_playlistDisplayRows[displayIndex];
        if (display.kind == PlaylistDisplayRowKind::groupHeader && lower) {
            const auto& group = m_playlistGroups[display.group];
            return std::min(m_playlistDisplayRows.size(), displayIndex + 1
                + (group.collapsed ? 0U : std::max<std::size_t>(group.count, 4)));
        }
        return std::min(m_playlistDisplayRows.size(), displayIndex + (lower ? 1U : 0U));
    }

    [[nodiscard]] bool playlistSnapshotMatches(const metadb_handle_list& snapshot) const {
        if (m_activePlaylist == SIZE_MAX || snapshot.get_count() != m_playlistItems.get_count()) return false;
        auto api = playlist_manager::get();
        metadb_handle_list current;
        api->playlist_get_all_items(m_activePlaylist, current);
        if (current.get_count() != snapshot.get_count()) return false;
        for (t_size index = 0; index < current.get_count(); ++index) {
            if (current[index] != snapshot[index]) return false;
        }
        return true;
    }

    void updatePlaylistDrag(HWND wnd, D2D1_POINT_2F point) {
        if (!m_playlistDragCandidate) return;
        m_playlistDragPoint = point;
        const auto dx = point.x - m_dragStartPoint.x;
        const auto dy = point.y - m_dragStartPoint.y;
        if (!m_playlistDragging && std::hypot(dx, dy) < 4.0F) return;
        m_playlistDragging = true;
        const auto area = calculateLayout(wnd).playlistArea;
        // The right column is directly adjacent to the playlist. Start OLE as soon as
        // the pointer crosses that boundary so Playback Queue is a reachable target.
        // Outgoing copy/queue operations do not require the source playlist to allow reorder.
        if (point.x < area.left || point.x > area.right
            || point.y < area.top - 8.0F || point.y > area.bottom + 8.0F) {
            beginOutgoingPlaylistDrag(wnd);
            return;
        }
        if (!playlistCanReorder()) {
            cancelInteraction();
            showStatus(L"This playlist does not allow item reordering.");
            return;
        }
        m_playlistInsertion = playlistInsertionAt(wnd, point);
        m_playlistDropDisplayBoundary = playlistDisplayBoundaryAt(wnd, point);
        SetTimer(wnd, kPlaylistDragTimer, 75, nullptr);
        invalidate();
    }

    void updatePlaylistDragAutoscroll(HWND wnd) {
        if (!m_playlistDragging || m_active != ControlId::playlistBody) {
            KillTimer(wnd, kPlaylistDragTimer);
            return;
        }
        const auto body = calculateLayout(wnd).playlistBody;
        if (m_playlistDragPoint.y < body.top + 22.0F) scrollPlaylistRows(-1);
        else if (m_playlistDragPoint.y > body.bottom - 22.0F) scrollPlaylistRows(1);
        m_playlistInsertion = playlistInsertionAt(wnd, m_playlistDragPoint);
        m_playlistDropDisplayBoundary = playlistDisplayBoundaryAt(wnd, m_playlistDragPoint);
        invalidate();
    }

    void commitPlaylistDrag() {
        if (!m_playlistDragging || !m_playlistDragCandidate || !playlistSnapshotMatches(m_playlistDragSnapshot)) return;
        std::vector<std::size_t> selected;
        for (std::size_t index = 0; index < m_playlistDragSelection.size(); ++index) {
            if (m_playlistDragSelection[index]) selected.push_back(index);
        }
        const auto plan = planPlaylistMove(m_playlistDragSnapshot.get_count(), selected, m_playlistInsertion);
        if (!plan.changed) return;
        std::vector<t_size> order(plan.order.begin(), plan.order.end());
        auto api = playlist_manager::get();
        api->playlist_undo_backup(m_activePlaylist);
        if (!api->playlist_reorder_items(m_activePlaylist, order.data(), order.size())) {
            MessageBoxW(get_wnd(), L"foobar2000 could not reorder this playlist.", L"Refrain", MB_OK | MB_ICONWARNING);
        }
        rebuildPlaylistSnapshot(true);
    }

    [[nodiscard]] bool playlistCanAdd(std::size_t playlist) const {
        const auto api = playlist_manager::get();
        if (playlist == SIZE_MAX || playlist >= api->get_playlist_count()) return false;
        return !api->playlist_lock_is_present(playlist)
            || (api->playlist_lock_get_filter_mask(playlist) & playlist_lock::filter_add) == 0;
    }

    [[nodiscard]] bool playlistCanRemove(std::size_t playlist) const {
        const auto api = playlist_manager::get();
        if (playlist == SIZE_MAX || playlist >= api->get_playlist_count()) return false;
        return !api->playlist_lock_is_present(playlist)
            || (api->playlist_lock_get_filter_mask(playlist) & playlist_lock::filter_remove) == 0;
    }

    [[nodiscard]] GUID activePlaylistGuid() const {
        if (m_activePlaylist == SIZE_MAX) return GUID_NULL;
        return playlist_manager_v5::get()->playlist_get_guid(m_activePlaylist);
    }

    [[nodiscard]] std::size_t playlistByGuid(const GUID& guid) const {
        if (InlineIsEqualGUID(guid, GUID_NULL)) return SIZE_MAX;
        return playlist_manager_v5::get()->find_playlist_by_guid(guid);
    }

    [[nodiscard]] bool isAlbumBridgeGuid(const GUID& guid) const {
        const auto bridge = readAlbumBrowserSettings().bridgeGuid;
        return !nullGuid(bridge) && sameGuid(bridge, guid);
    }

    static LRESULT CALLBACK playlistFilterSubclass(HWND window, UINT message, WPARAM wp, LPARAM lp,
        UINT_PTR, DWORD_PTR reference) {
        auto* self = reinterpret_cast<PlaybackPanel*>(reference);
        if (message == WM_KEYDOWN && wp == 'F' && self
            && (GetKeyState(VK_CONTROL) & 0x8000) != 0
            && self->get_host()->get_keyboard_shortcuts_enabled()
            && uie::window::g_process_keydown_keyboard_shortcuts(wp)) {
            return 0;
        }
        if (message == WM_NCDESTROY) RemoveWindowSubclass(window, playlistFilterSubclass, 1);
        return DefSubclassProc(window, message, wp, lp);
    }

    void createPlaylistFilterControls(HWND wnd) {
        m_playlistFilterEdit = CreateWindowExW(0, L"EDIT", L"",
            WS_CHILD | WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP,
            0, 0, 0, 0, wnd, reinterpret_cast<HMENU>(kPlaylistFilterEditId),
            core_api::get_my_instance(), nullptr);
        m_playlistFilterClear = CreateWindowExW(0, L"BUTTON", L"×",
            WS_CHILD | BS_PUSHBUTTON | WS_TABSTOP,
            0, 0, 0, 0, wnd, reinterpret_cast<HMENU>(kPlaylistFilterClearId),
            core_api::get_my_instance(), nullptr);
        if (m_playlistFilterEdit) {
            SendMessageW(m_playlistFilterEdit, WM_SETFONT,
                reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
            SendMessageW(m_playlistFilterEdit, EM_SETCUEBANNER, TRUE,
                reinterpret_cast<LPARAM>(L"Filter playlists"));
            SetWindowSubclass(m_playlistFilterEdit, playlistFilterSubclass, 1,
                reinterpret_cast<DWORD_PTR>(this));
        }
        if (m_playlistFilterClear) {
            SendMessageW(m_playlistFilterClear, WM_SETFONT,
                reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        }
        syncPlaylistFilterControls(wnd);
    }

    void syncPlaylistFilterControls(HWND wnd) const {
        if (!m_playlistFilterEdit || !m_playlistFilterClear) return;
        const auto visible = m_workspace == Workspace::playlist;
        if (!visible) {
            ShowWindow(m_playlistFilterEdit, SW_HIDE);
            ShowWindow(m_playlistFilterClear, SW_HIDE);
            return;
        }
        const auto layout = calculateLayout(wnd);
        const auto scale = dpiScale();
        const auto place = [&](HWND child, const D2D1_RECT_F& rect) {
            SetWindowPos(child, nullptr,
                static_cast<int>(std::lround(rect.left * scale)),
                static_cast<int>(std::lround(rect.top * scale)),
                std::max(0, static_cast<int>(std::lround((rect.right - rect.left) * scale))),
                std::max(0, static_cast<int>(std::lround((rect.bottom - rect.top) * scale))),
                SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);
        };
        place(m_playlistFilterEdit, layout.playlistBrowserFilter);
        place(m_playlistFilterClear, layout.playlistBrowserFilterClear);
        EnableWindow(m_playlistFilterClear, !m_playlistFilterText.empty());
    }

    void rebuildFilteredPlaylistBrowserRows() {
        GUID focusGuid{};
        if (m_playlistBrowserFocus < m_playlistBrowserRows.size()) {
            focusGuid = m_playlistBrowserRows[m_playlistBrowserFocus].guid;
        }
        m_playlistBrowserRows = filterPlaylistBrowserRows(m_playlistBrowserAllRows, m_playlistFilterText);
        const auto active = playlist_manager_v5::get()->get_active_playlist();
        const auto activeGuid = active != SIZE_MAX && active < playlist_manager_v5::get()->get_playlist_count()
            ? playlist_manager_v5::get()->playlist_get_guid(active) : GUID{};
        m_playlistBrowserSelection = reconcilePlaylistSelection(
            m_playlistBrowserRows, m_playlistBrowserSelection, activeGuid);
        m_playlistBrowserFocus = findPlaylistRowByGuid(m_playlistBrowserRows, focusGuid);
        if (m_playlistBrowserFocus == static_cast<std::size_t>(-1)) {
            m_playlistBrowserFocus = findPlaylistRowByGuid(m_playlistBrowserRows, activeGuid);
        }
        m_playlistBrowserAnchor = m_playlistBrowserFocus;
        clampPlaylistBrowserScroll();
        invalidate();
    }

    void updatePlaylistFilterFromEditor() {
        if (!m_playlistFilterEdit) return;
        const auto length = GetWindowTextLengthW(m_playlistFilterEdit);
        std::wstring value(static_cast<std::size_t>(length) + 1, L'\0');
        GetWindowTextW(m_playlistFilterEdit, value.data(), length + 1);
        value.resize(static_cast<std::size_t>(length));
        if (value == m_playlistFilterText) return;
        if (m_playlistRenameEdit && IsWindow(m_playlistRenameEdit)) {
            cancelPlaylistBrowserRename();
        }
        m_playlistFilterText = std::move(value);
        m_playlistBrowserTopRow = 0;
        rebuildFilteredPlaylistBrowserRows();
        if (const auto wnd = get_wnd()) syncPlaylistFilterControls(wnd);
    }

    void clearPlaylistFilter(bool focusEditor) {
        if (m_playlistFilterEdit && GetWindowTextLengthW(m_playlistFilterEdit) > 0) {
            SetWindowTextW(m_playlistFilterEdit, L"");
        } else if (!m_playlistFilterText.empty()) {
            m_playlistFilterText.clear();
            rebuildFilteredPlaylistBrowserRows();
        }
        if (focusEditor && m_playlistFilterEdit) SetFocus(m_playlistFilterEdit);
    }

    void openAlbumList() {
        GUID command{};
        const auto found = mainmenu_commands::g_find_by_name("Album List", command)
            || mainmenu_commands::g_find_by_name("Library/Album List", command);
        if (!found || !mainmenu_commands::g_execute(command)) {
            MessageBoxW(get_wnd(),
                L"foobar2000's Album List command is unavailable. Check that the Album List component is installed.",
                L"Refrain", MB_OK | MB_ICONINFORMATION);
        }
    }

    [[nodiscard]] POINT popupOrigin(const D2D1_RECT_F& rect) const noexcept {
        POINT point{static_cast<LONG>(std::lround(rect.left * dpiScale())),
            static_cast<LONG>(std::lround(rect.bottom * dpiScale()))};
        if (const auto wnd = get_wnd()) ClientToScreen(wnd, &point);
        return point;
    }

    [[nodiscard]] std::wstring playbackOrderName() const {
        auto api = playlist_manager::get();
        const auto count = api->playback_order_get_count();
        const auto active = api->playback_order_get_active();
        return active < count ? utf8ToWide(api->playback_order_get_name(active)) : L"Unavailable";
    }

    void showPlaybackOrderMenu() {
        const auto wnd = get_wnd();
        if (!wnd) return;
        auto api = playlist_manager::get();
        const auto count = api->playback_order_get_count();
        if (count == 0) {
            showStatus(L"Playback Order is unavailable.");
            return;
        }
        const auto menu = CreatePopupMenu();
        if (!menu) return;
        const auto active = api->playback_order_get_active();
        const auto maximum = std::min<t_size>(count, 0xEFFF);
        for (t_size index = 0; index < maximum; ++index) {
            const auto flags = MF_STRING | (index == active ? MF_CHECKED : MF_UNCHECKED);
            AppendMenuW(menu, flags, static_cast<UINT_PTR>(index + 1),
                utf8ToWide(api->playback_order_get_name(index)).c_str());
        }
        const auto origin = popupOrigin(calculateLayout(wnd).playbackOrder);
        const auto command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN,
            origin.x, origin.y, 0, wnd, nullptr);
        DestroyMenu(menu);
        if (command > 0 && static_cast<t_size>(command) <= maximum) {
            api->playback_order_set_active(static_cast<t_size>(command - 1));
            invalidate();
        }
    }

    struct OutputDeviceItem {
        std::wstring name;
        GUID output{};
        GUID device{};
    };

    [[nodiscard]] std::vector<OutputDeviceItem> outputDevices() const {
        std::vector<OutputDeviceItem> result;
        output_manager_v2::get()->listDevices([&](const char* name, const GUID& output, const GUID& device) {
            result.push_back(OutputDeviceItem{utf8ToWide(name), output, device});
        });
        return result;
    }

    [[nodiscard]] std::wstring outputDeviceName() const {
        try {
            const auto config = output_manager::get()->getCoreConfig();
            for (const auto& item : outputDevices()) {
                if (IsEqualGUID(item.output, config.m_output) && IsEqualGUID(item.device, config.m_device)) {
                    return item.name;
                }
            }
        } catch (...) {
        }
        return L"Unavailable";
    }

    void showOutputDeviceMenu() {
        const auto wnd = get_wnd();
        if (!wnd) return;
        try {
            const auto devices = outputDevices();
            if (devices.empty()) {
                showStatus(L"No output devices are available.");
                return;
            }
            const auto config = output_manager::get()->getCoreConfig();
            const auto menu = CreatePopupMenu();
            if (!menu) return;
            const auto maximum = std::min<std::size_t>(devices.size(), 0xEFFF);
            for (std::size_t index = 0; index < maximum; ++index) {
                const auto selected = IsEqualGUID(devices[index].output, config.m_output)
                    && IsEqualGUID(devices[index].device, config.m_device);
                AppendMenuW(menu, MF_STRING | (selected ? MF_CHECKED : MF_UNCHECKED),
                    static_cast<UINT_PTR>(index + 1), devices[index].name.c_str());
            }
            const auto origin = popupOrigin(calculateLayout(wnd).outputDevice);
            const auto command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN,
                origin.x, origin.y, 0, wnd, nullptr);
            DestroyMenu(menu);
            if (command > 0 && static_cast<std::size_t>(command) <= maximum) {
                const auto& selected = devices[static_cast<std::size_t>(command - 1)];
                output_manager_v2::get()->setCoreConfigDevice(selected.output, selected.device);
                invalidate();
            }
        } catch (const std::exception& error) {
            const auto message = L"foobar2000 could not switch the output device.\r\n\r\n"
                + utf8ToWide(error.what());
            MessageBoxW(wnd, message.c_str(), L"Refrain - Output Device", MB_OK | MB_ICONERROR);
        } catch (...) {
            MessageBoxW(wnd, L"foobar2000 could not switch the output device.",
                L"Refrain - Output Device", MB_OK | MB_ICONERROR);
        }
    }

    void refreshPlaylistBrowserSnapshot() {
        auto api = playlist_manager_v5::get();
        refreshPlayingLocation();
        const auto active = api->get_active_playlist();
        std::vector<PlaylistBrowserRow> rows;
        rows.reserve(api->get_playlist_count());
        for (t_size index = 0; index < api->get_playlist_count(); ++index) {
            const auto guid = api->playlist_get_guid(index);
            if (isAlbumBridgeGuid(guid)) continue;
            pfc::string8 name;
            api->playlist_get_name(index, name);
            const auto automatic = autoplaylist_manager::get()->is_client_present(index);
            const auto lockMask = api->playlist_lock_is_present(index)
                ? api->playlist_lock_get_filter_mask(index) : 0U;
            rows.push_back({guid, utf8ToWide(name.c_str()), api->playlist_get_item_count(index),
                automatic ? PlaylistBrowserKind::automatic : PlaylistBrowserKind::manual,
                lockMask, index == active, index == m_playingPlaylist});
        }
        m_playlistBrowserAllRows = std::move(rows);
        rebuildFilteredPlaylistBrowserRows();
    }

    [[nodiscard]] std::wstring uniqueBrowserName(const std::wstring& base) const {
        std::vector<std::wstring> names;
        names.reserve(m_playlistBrowserAllRows.size());
        for (const auto& row : m_playlistBrowserAllRows) names.push_back(row.name);
        return uniquePlaylistName(names, base);
    }

    [[nodiscard]] t_size createManualPlaylist(const std::wstring& requested, bool activate) {
        const auto name = uniqueBrowserName(requested);
        const auto utf8 = wideToUtf8(name);
        if (utf8.empty()) return SIZE_MAX;
        auto api = playlist_manager_v5::get();
        const auto index = api->create_playlist(utf8.c_str(), utf8.size(), SIZE_MAX);
        if (index != SIZE_MAX && activate) api->set_active_playlist(index);
        refreshPlaylistBrowserSnapshot();
        return index;
    }

    [[nodiscard]] t_size ensureDefaultPlaylist(bool persistRepair) {
        auto settings = readPlaylistBrowserSettings();
        auto row = chooseDefaultPlaylistRow(m_playlistBrowserAllRows, settings.defaultPlaylistGuid);
        if (row == static_cast<std::size_t>(-1)) {
            const auto created = createManualPlaylist(L"Default Playlist", false);
            if (created == SIZE_MAX) return SIZE_MAX;
            refreshPlaylistBrowserSnapshot();
            row = findPlaylistRowByGuid(m_playlistBrowserAllRows,
                playlist_manager_v5::get()->playlist_get_guid(created));
        }
        if (row == static_cast<std::size_t>(-1)) return SIZE_MAX;
        const auto& chosen = m_playlistBrowserAllRows[row];
        if (persistRepair && !sameGuid(settings.defaultPlaylistGuid, chosen.guid)) {
            settings.defaultPlaylistGuid = chosen.guid;
            writePlaylistBrowserSettings(settings);
        }
        return playlistByGuid(chosen.guid);
    }

    void applyDefaultPlaylistOnce() {
        if (g_defaultPlaylistApplied.exchange(true)) {
            const auto configured = readPlaylistBrowserSettings().defaultPlaylistGuid;
            if (playlistByGuid(configured) != SIZE_MAX) m_initialAlbumSourceGuid = configured;
            return;
        }
        const auto playlist = ensureDefaultPlaylist(true);
        if (playlist == SIZE_MAX) return;
        auto api = playlist_manager_v5::get();
        api->set_active_playlist(playlist);
        m_initialAlbumSourceGuid = api->playlist_get_guid(playlist);
        refreshPlaylistBrowserSnapshot();
    }

    void repairDefaultPlaylist() {
        const auto settings = readPlaylistBrowserSettings();
        if (findPlaylistRowByGuid(m_playlistBrowserAllRows, settings.defaultPlaylistGuid)
            == static_cast<std::size_t>(-1)) {
            (void)ensureDefaultPlaylist(true);
        }
    }

    [[nodiscard]] std::optional<std::size_t> playlistBrowserRowAt(
        const Layout& layout, D2D1_POINT_2F point) const {
        if (!contains(layout.playlistBrowserBody, point)) return std::nullopt;
        constexpr float rowHeight = 30.0F;
        const auto row = m_playlistBrowserTopRow + static_cast<std::size_t>(
            (point.y - layout.playlistBrowserBody.top) / rowHeight);
        return row < m_playlistBrowserRows.size() ? std::optional<std::size_t>{row} : std::nullopt;
    }

    void clampPlaylistBrowserScroll() {
        const auto wnd = get_wnd();
        if (!wnd) return;
        const auto layout = calculateLayout(wnd);
        const auto capacity = visibleRowCapacity(layout.playlistBrowserBody.bottom
            - layout.playlistBrowserBody.top, 30.0F);
        m_playlistBrowserTopRow = clampTopRow(
            m_playlistBrowserTopRow, m_playlistBrowserRows.size(), capacity);
    }

    void selectPlaylistBrowserRow(std::size_t row, bool ctrl, bool shift, bool activate) {
        if (row >= m_playlistBrowserRows.size()) return;
        if (!ctrl && !shift) {
            m_playlistBrowserSelection.assign(1, m_playlistBrowserRows[row].guid);
            m_playlistBrowserAnchor = row;
        } else if (shift) {
            const auto anchor = m_playlistBrowserAnchor < m_playlistBrowserRows.size()
                ? m_playlistBrowserAnchor : row;
            if (!ctrl) m_playlistBrowserSelection.clear();
            const auto first = std::min(anchor, row);
            const auto last = std::max(anchor, row);
            for (auto index = first; index <= last; ++index) {
                const auto guid = m_playlistBrowserRows[index].guid;
                if (std::none_of(m_playlistBrowserSelection.begin(), m_playlistBrowserSelection.end(),
                        [&](const GUID& value) { return sameGuid(value, guid); })) {
                    m_playlistBrowserSelection.push_back(guid);
                }
            }
        } else {
            const auto guid = m_playlistBrowserRows[row].guid;
            const auto found = std::find_if(m_playlistBrowserSelection.begin(), m_playlistBrowserSelection.end(),
                [&](const GUID& value) { return sameGuid(value, guid); });
            if (found == m_playlistBrowserSelection.end()) m_playlistBrowserSelection.push_back(guid);
            else m_playlistBrowserSelection.erase(found);
            m_playlistBrowserAnchor = row;
        }
        m_playlistBrowserFocus = row;
        if (activate) {
            const auto playlist = playlistByGuid(m_playlistBrowserRows[row].guid);
            if (playlist != SIZE_MAX) playlist_manager::get()->set_active_playlist(playlist);
        }
        invalidate();
    }

    void updatePlaylistBrowserDrag(HWND wnd, D2D1_POINT_2F point) {
        if (!m_playlistBrowserDragCandidate || !m_playlistFilterText.empty()) return;
        const auto dx = point.x - m_playlistBrowserDragStart.x;
        const auto dy = point.y - m_playlistBrowserDragStart.y;
        if (!m_playlistBrowserDragging && std::hypot(dx, dy) < 4.0F) return;
        m_playlistBrowserDragging = true;
        const auto layout = calculateLayout(wnd);
        constexpr float rowHeight = 30.0F;
        if (point.y < layout.playlistBrowserBody.top + 18.0F && m_playlistBrowserTopRow > 0) {
            --m_playlistBrowserTopRow;
        } else {
            const auto capacity = visibleRowCapacity(layout.playlistBrowserBody.bottom
                - layout.playlistBrowserBody.top, rowHeight);
            const auto maximum = maximumTopRow(m_playlistBrowserRows.size(), capacity);
            if (point.y > layout.playlistBrowserBody.bottom - 18.0F
                && m_playlistBrowserTopRow < maximum) ++m_playlistBrowserTopRow;
        }
        const auto relative = std::clamp(point.y - layout.playlistBrowserBody.top, 0.0F,
            std::max(0.0F, layout.playlistBrowserBody.bottom - layout.playlistBrowserBody.top));
        const auto row = m_playlistBrowserTopRow + static_cast<std::size_t>(relative / rowHeight);
        const auto lower = std::fmod(relative, rowHeight) >= rowHeight * 0.5F;
        m_playlistBrowserInsertion = std::min(m_playlistBrowserRows.size(), row + (lower ? 1U : 0U));
        invalidate();
    }

    void commitPlaylistBrowserDrag() {
        if (m_playlistBrowserDragSnapshot.size() != m_playlistBrowserRows.size()) return;
        for (std::size_t index = 0; index < m_playlistBrowserRows.size(); ++index) {
            if (!sameGuid(m_playlistBrowserRows[index].guid, m_playlistBrowserDragSnapshot[index])) return;
        }
        std::vector<std::size_t> selected;
        for (std::size_t index = 0; index < m_playlistBrowserDragSnapshot.size(); ++index) {
            if (std::any_of(m_playlistBrowserSelection.begin(), m_playlistBrowserSelection.end(),
                    [&](const GUID& guid) { return sameGuid(guid, m_playlistBrowserDragSnapshot[index]); })) {
                selected.push_back(index);
            }
        }
        const auto plan = planPlaylistMove(m_playlistBrowserDragSnapshot.size(), selected,
            m_playlistBrowserInsertion);
        if (!plan.changed) return;
        std::vector<GUID> desiredVisible;
        desiredVisible.reserve(plan.order.size());
        for (const auto index : plan.order) desiredVisible.push_back(m_playlistBrowserDragSnapshot[index]);

        auto api = playlist_manager_v5::get();
        std::vector<t_size> order(api->get_playlist_count());
        std::size_t visible{};
        for (t_size destination = 0; destination < api->get_playlist_count(); ++destination) {
            const auto currentGuid = api->playlist_get_guid(destination);
            if (isAlbumBridgeGuid(currentGuid)) {
                order[destination] = destination;
                continue;
            }
            if (visible >= desiredVisible.size()) return;
            const auto source = api->find_playlist_by_guid(desiredVisible[visible++]);
            if (source == SIZE_MAX) return;
            order[destination] = source;
        }
        if (!api->reorder(order.data(), order.size())) {
            MessageBoxW(get_wnd(), L"foobar2000 could not reorder these playlists.",
                L"Refrain", MB_OK | MB_ICONWARNING);
        }
        refreshPlaylistBrowserSnapshot();
    }

    static LRESULT CALLBACK playlistRenameSubclass(HWND window, UINT message, WPARAM wp, LPARAM lp,
        UINT_PTR, DWORD_PTR reference) {
        auto* self = reinterpret_cast<PlaybackPanel*>(reference);
        if (message == WM_GETDLGCODE) return DLGC_WANTALLKEYS;
        if (message == WM_KEYDOWN && wp == VK_RETURN) {
            PostMessageW(GetParent(window), kCommitPlaylistRenameMessage, 0, 0);
            return 0;
        }
        if (message == WM_KEYDOWN && wp == VK_ESCAPE) {
            if (self) self->m_playlistRenameCancelPending = true;
            PostMessageW(GetParent(window), kCancelPlaylistRenameMessage, 0, 0);
            return 0;
        }
        if (message == WM_KILLFOCUS && self && !self->m_playlistRenameCommitting
            && !self->m_playlistRenameCancelPending) {
            PostMessageW(GetParent(window), kCommitPlaylistRenameMessage, 0, 0);
        }
        if (message == WM_NCDESTROY) RemoveWindowSubclass(window, playlistRenameSubclass, 1);
        return DefSubclassProc(window, message, wp, lp);
    }

    void beginPlaylistBrowserRename() {
        if (m_playlistBrowserFocus >= m_playlistBrowserRows.size()) return;
        const auto& row = m_playlistBrowserRows[m_playlistBrowserFocus];
        const auto playlist = playlistByGuid(row.guid);
        auto api = playlist_manager::get();
        if (playlist == SIZE_MAX || (api->playlist_lock_is_present(playlist)
                && (api->playlist_lock_get_filter_mask(playlist) & playlist_lock::filter_rename) != 0)) {
            showStatus(L"This playlist does not allow renaming.");
            return;
        }
        cancelPlaylistBrowserRename();
        const auto layout = calculateLayout(get_wnd());
        const auto y = layout.playlistBrowserBody.top
            + static_cast<float>(m_playlistBrowserFocus - m_playlistBrowserTopRow) * 30.0F;
        if (y < layout.playlistBrowserBody.top || y + 30.0F > layout.playlistBrowserBody.bottom) return;
        const auto scale = dpiScale();
        m_playlistRenameEdit = CreateWindowExW(0, L"EDIT", row.name.c_str(),
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
            static_cast<int>(std::lround((layout.playlistBrowserBody.left + 18.0F) * scale)),
            static_cast<int>(std::lround((y + 2.0F) * scale)),
            static_cast<int>(std::lround((layout.playlistBrowserBody.right
                - layout.playlistBrowserBody.left - 64.0F) * scale)),
            static_cast<int>(std::lround(26.0F * scale)), get_wnd(), nullptr,
            core_api::get_my_instance(), nullptr);
        if (!m_playlistRenameEdit) return;
        m_playlistRenameGuid = row.guid;
        m_playlistRenameOriginal = row.name;
        m_playlistRenameCancelPending = false;
        m_playlistRenameCommitting = false;
        SendMessageW(m_playlistRenameEdit, WM_SETFONT,
            reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        SetWindowSubclass(m_playlistRenameEdit, playlistRenameSubclass, 1,
            reinterpret_cast<DWORD_PTR>(this));
        SendMessageW(m_playlistRenameEdit, EM_SETSEL, 0, -1);
        SetFocus(m_playlistRenameEdit);
    }

    void destroyPlaylistBrowserRenameEdit() {
        const auto edit = std::exchange(m_playlistRenameEdit, nullptr);
        if (edit && IsWindow(edit)) DestroyWindow(edit);
        m_playlistRenameGuid = GUID{};
        m_playlistRenameOriginal.clear();
        m_playlistRenameCommitting = false;
        m_playlistRenameCancelPending = false;
    }

    void cancelPlaylistBrowserRename() {
        m_playlistRenameCancelPending = true;
        destroyPlaylistBrowserRenameEdit();
        if (const auto wnd = get_wnd(); wnd && IsWindow(wnd)) SetFocus(wnd);
    }

    void commitPlaylistBrowserRename() {
        if (!m_playlistRenameEdit || !IsWindow(m_playlistRenameEdit) || m_playlistRenameCommitting) return;
        m_playlistRenameCommitting = true;
        const auto length = GetWindowTextLengthW(m_playlistRenameEdit);
        std::wstring name(static_cast<std::size_t>(length) + 1, L'\0');
        GetWindowTextW(m_playlistRenameEdit, name.data(), length + 1);
        name.resize(static_cast<std::size_t>(length));
        const auto first = name.find_first_not_of(L" \t\r\n");
        const auto last = name.find_last_not_of(L" \t\r\n");
        name = first == std::wstring::npos ? std::wstring{} : name.substr(first, last - first + 1);
        const auto target = playlistByGuid(m_playlistRenameGuid);
        auto api = playlist_manager::get();
        const auto utf8 = wideToUtf8(name);
        const auto duplicate = utf8.empty() ? SIZE_MAX : api->find_playlist(utf8.c_str(), utf8.size());
        if (name.empty() || (duplicate != SIZE_MAX && duplicate != target)) {
            MessageBoxW(get_wnd(), name.empty() ? L"Playlist name cannot be empty."
                : L"A playlist with this name already exists.", L"Refrain", MB_OK | MB_ICONWARNING);
            m_playlistRenameCommitting = false;
            SetFocus(m_playlistRenameEdit);
            SendMessageW(m_playlistRenameEdit, EM_SETSEL, 0, -1);
            return;
        }
        if (target == SIZE_MAX || (api->playlist_lock_is_present(target)
                && (api->playlist_lock_get_filter_mask(target) & playlist_lock::filter_rename) != 0)
            || !api->playlist_rename(target, utf8.c_str(), utf8.size())) {
            MessageBoxW(get_wnd(), L"The playlist changed or rejected this name.",
                L"Refrain", MB_OK | MB_ICONWARNING);
            m_playlistRenameCommitting = false;
            SetFocus(m_playlistRenameEdit);
            return;
        }
        destroyPlaylistBrowserRenameEdit();
        SetFocus(get_wnd());
        refreshPlaylistBrowserSnapshot();
    }

    void deleteSelectedPlaylists() {
        std::vector<GUID> targets = m_playlistBrowserSelection;
        if (targets.empty() && m_playlistBrowserFocus < m_playlistBrowserRows.size()) {
            targets.push_back(m_playlistBrowserRows[m_playlistBrowserFocus].guid);
        }
        if (targets.empty()) return;
        const auto settings = readPlaylistBrowserSettings();
        std::wstring message = L"Delete " + std::to_wstring(targets.size()) + L" playlist";
        if (targets.size() != 1) message += L"s";
        message += L"?\r\n\r\n";
        bool warning{};
        for (const auto& guid : targets) {
            const auto row = findPlaylistRowByGuid(m_playlistBrowserRows, guid);
            if (row == static_cast<std::size_t>(-1)) continue;
            const auto& value = m_playlistBrowserRows[row];
            message += L"• " + value.name + (value.kind == PlaylistBrowserKind::automatic
                ? L"  [Autoplaylist]" : L"  [Manual]") + L"  ("
                + std::to_wstring(value.itemCount) + L" tracks)\r\n";
            warning = warning || value.active || value.playing || sameGuid(value.guid, settings.defaultPlaylistGuid);
        }
        if (warning) message += L"\r\nThis includes the active, playing, or default playlist.";
        message += L"\r\nAudio files will not be deleted.";
        if (MessageBoxW(get_wnd(), message.c_str(), L"Refrain", MB_YESNO | MB_ICONWARNING
                | MB_DEFBUTTON2) != IDYES) return;
        auto api = playlist_manager_v5::get();
        pfc::bit_array_bittable mask;
        mask.resize(api->get_playlist_count());
        bool any{};
        for (const auto& guid : targets) {
            const auto playlist = api->find_playlist_by_guid(guid);
            if (playlist == SIZE_MAX) continue;
            if (api->playlist_lock_is_present(playlist)
                && (api->playlist_lock_get_filter_mask(playlist) & playlist_lock::filter_remove_playlist) != 0) {
                continue;
            }
            mask.set(playlist, true);
            any = true;
        }
        if (!any || !api->remove_playlists(mask)) {
            MessageBoxW(get_wnd(), L"No selected playlist could be deleted. A lock or external change may prevent it.",
                L"Refrain", MB_OK | MB_ICONINFORMATION);
        }
        refreshPlaylistBrowserSnapshot();
        repairDefaultPlaylist();
    }

    void setDefaultPlaylist(const GUID& guid) {
        if (playlistByGuid(guid) == SIZE_MAX || isAlbumBridgeGuid(guid)) return;
        auto settings = readPlaylistBrowserSettings();
        settings.defaultPlaylistGuid = guid;
        writePlaylistBrowserSettings(settings);
        showStatus(L"Default playlist saved. It will be applied on the next foobar2000 start.");
    }

    [[nodiscard]] t_size duplicateManualPlaylist(const GUID& guid, const std::wstring& suffix) {
        const auto source = playlistByGuid(guid);
        if (source == SIZE_MAX) return SIZE_MAX;
        auto api = playlist_manager::get();
        pfc::string8 sourceName;
        metadb_handle_list content;
        if (!api->playlist_get_name(source, sourceName)) return SIZE_MAX;
        api->playlist_get_all_items(source, content);
        const auto created = createManualPlaylist(utf8ToWide(sourceName.c_str()) + suffix, false);
        if (created == SIZE_MAX) return SIZE_MAX;
        if (content.get_count() > 0 && !api->playlist_add_items(created, content, pfc::bit_array_false())) {
            api->remove_playlist(created);
            return SIZE_MAX;
        }
        api->set_active_playlist(created);
        refreshPlaylistBrowserSnapshot();
        return created;
    }

    bool duplicateAutoplaylist(const GUID& guid) {
        const auto source = playlistByGuid(guid);
        if (source == SIZE_MAX || !autoplaylist_manager::get()->is_client_present(source)) return false;
        try {
            const auto client = autoplaylist_manager::get()->query_client(source);
            autoplaylist_client_factory::ptr factory;
            for (auto candidate : autoplaylist_client_factory::enumerate()) {
                if (sameGuid(candidate->get_guid(), client->get_guid())) {
                    factory = candidate;
                    break;
                }
            }
            if (factory.is_empty()) {
                MessageBoxW(get_wnd(), L"This autoplaylist provider cannot recreate its rules, so Duplicate Rules is unavailable.",
                    L"Refrain", MB_OK | MB_ICONINFORMATION);
                return false;
            }
            pfc::array_t<t_uint8> configuration;
            client->get_configuration(configuration);
            stream_reader_memblock_ref reader(configuration);
            const auto clone = factory->instantiate(&reader, configuration.get_size(), fb2k::noAbort);
            pfc::string8 name;
            playlist_manager::get()->playlist_get_name(source, name);
            const auto created = createManualPlaylist(utf8ToWide(name.c_str()) + L" (Rules Copy)", false);
            if (created == SIZE_MAX) return false;
            try {
                const auto flags = autoplaylist_manager_v2::get()->get_client_flags(source);
                autoplaylist_manager::get()->add_client(clone, created, flags);
            } catch (...) {
                playlist_manager::get()->remove_playlist(created);
                throw;
            }
            playlist_manager::get()->set_active_playlist(created);
            refreshPlaylistBrowserSnapshot();
            return true;
        } catch (const std::exception& error) {
            MessageBoxW(get_wnd(), utf8ToWide(error.what()).c_str(), L"Refrain - Duplicate Rules",
                MB_OK | MB_ICONWARNING);
            return false;
        }
    }

    bool freezeAutoplaylist(const GUID& guid) {
        const auto source = playlistByGuid(guid);
        if (source == SIZE_MAX || !autoplaylist_manager::get()->is_client_present(source)) return false;
        pfc::string8 name;
        playlist_manager::get()->playlist_get_name(source, name);
        const auto created = duplicateManualPlaylist(guid, L" (Manual)");
        if (created == SIZE_MAX) return false;
        const auto currentSource = playlistByGuid(guid);
        if (currentSource == SIZE_MAX) return true;
        try {
            autoplaylist_manager::get()->remove_client(currentSource);
            if (!playlist_manager::get()->remove_playlist(currentSource)) {
                MessageBoxW(get_wnd(), L"The manual snapshot was created, but the original autoplaylist could not be removed.",
                    L"Refrain", MB_OK | MB_ICONINFORMATION);
            }
        } catch (const std::exception& error) {
            MessageBoxW(get_wnd(), utf8ToWide(error.what()).c_str(), L"Refrain - Freeze as Manual Playlist",
                MB_OK | MB_ICONWARNING);
        }
        playlist_manager::get()->set_active_playlist(created);
        refreshPlaylistBrowserSnapshot();
        repairDefaultPlaylist();
        return true;
    }

    bool createAutoplaylist(const std::wstring& requestedName, const char* query) {
        constexpr const char* sort = "%album% | %discnumber% | %tracknumber% | %title%";
        try {
            (void)search_filter_manager::get()->create(query);
            const auto name = uniqueBrowserName(requestedName);
            const auto utf8 = wideToUtf8(name);
            auto api = playlist_manager::get();
            const auto created = api->create_playlist(utf8.c_str(), utf8.size(), SIZE_MAX);
            if (created == SIZE_MAX) return false;
            try {
                autoplaylist_manager::get()->add_client_simple(query, sort, created, autoplaylist_flag_sort);
            } catch (...) {
                api->remove_playlist(created);
                throw;
            }
            api->set_active_playlist(created);
            refreshPlaylistBrowserSnapshot();
            return true;
        } catch (const std::exception& error) {
            MessageBoxW(get_wnd(), utf8ToWide(error.what()).c_str(), L"Refrain - Autoplaylist",
                MB_OK | MB_ICONWARNING);
            return false;
        }
    }

    void savePlaylistByGuid(const GUID& guid) {
        auto api = playlist_manager::get();
        const auto target = playlistByGuid(guid);
        if (target == SIZE_MAX) return;
        const auto prior = api->get_active_playlist();
        const auto priorGuid = prior != SIZE_MAX ? playlist_manager_v5::get()->playlist_get_guid(prior) : GUID{};
        api->set_active_playlist(target);
        standard_commands::main_save_playlist();
        const auto restore = playlistByGuid(priorGuid);
        if (restore != SIZE_MAX) api->set_active_playlist(restore);
    }

    void showPlaylistBrowserMenu(HWND wnd, D2D1_POINT_2F point, POINT clientPoint) {
        const auto layout = calculateLayout(wnd);
        const auto rowAtPoint = playlistBrowserRowAt(layout, point);
        if (rowAtPoint) {
            const auto row = *rowAtPoint;
            const auto guid = m_playlistBrowserRows[row].guid;
            const auto selected = std::any_of(m_playlistBrowserSelection.begin(),
                m_playlistBrowserSelection.end(), [&](const GUID& value) { return sameGuid(value, guid); });
            if (!selected) selectPlaylistBrowserRow(row, false, false, false);
            else m_playlistBrowserFocus = row;
            const auto playlist = playlistByGuid(guid);
            if (playlist == SIZE_MAX) return;
            const auto automatic = autoplaylist_manager::get()->is_client_present(playlist);
            const auto lockMask = playlist_manager::get()->playlist_lock_is_present(playlist)
                ? playlist_manager::get()->playlist_lock_get_filter_mask(playlist) : 0U;
            const auto menu = CreatePopupMenu();
            if (!menu) return;
            AppendMenuW(menu, MF_STRING, 1, L"Activate");
            AppendMenuW(menu, MF_STRING, 2, L"Set as Default Playlist");
            AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
            if (automatic) {
                autoplaylist_client_v2::ptr clientV2;
                const auto client = autoplaylist_manager::get()->query_client(playlist);
                const auto canEdit = client->service_query_t(clientV2) && clientV2->show_ui_available();
                bool canDuplicate{};
                for (auto factory : autoplaylist_client_factory::enumerate()) {
                    if (sameGuid(factory->get_guid(), client->get_guid())) { canDuplicate = true; break; }
                }
                AppendMenuW(menu, MF_STRING | (canEdit ? 0 : MF_GRAYED), 7, L"Edit Rules...");
                AppendMenuW(menu, MF_STRING | (canDuplicate ? 0 : MF_GRAYED), 8, L"Duplicate Rules");
                AppendMenuW(menu, MF_STRING, 9, L"Freeze as Manual Playlist");
            } else {
                AppendMenuW(menu, MF_STRING, 4, L"Duplicate");
            }
            AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(menu, MF_STRING | ((lockMask & playlist_lock::filter_rename) ? MF_GRAYED : 0), 3, L"Rename");
            AppendMenuW(menu, MF_STRING, 5, L"Save This Playlist...");
            AppendMenuW(menu, MF_STRING | ((lockMask & playlist_lock::filter_remove_playlist) ? MF_GRAYED : 0), 6, L"Delete");
            ClientToScreen(wnd, &clientPoint);
            const auto command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                clientPoint.x, clientPoint.y, 0, wnd, nullptr);
            DestroyMenu(menu);
            if (command == 1) playlist_manager::get()->set_active_playlist(playlistByGuid(guid));
            else if (command == 2) setDefaultPlaylist(guid);
            else if (command == 3) beginPlaylistBrowserRename();
            else if (command == 4) (void)duplicateManualPlaylist(guid, L" (Copy)");
            else if (command == 5) savePlaylistByGuid(guid);
            else if (command == 6) deleteSelectedPlaylists();
            else if (command == 7) {
                const auto current = playlistByGuid(guid);
                if (current != SIZE_MAX) autoplaylist_manager::get()->query_client(current)->show_ui(current);
            } else if (command == 8) (void)duplicateAutoplaylist(guid);
            else if (command == 9) (void)freezeAutoplaylist(guid);
            return;
        }

        const auto menu = CreatePopupMenu();
        const auto add = CreatePopupMenu();
        const auto presets = CreatePopupMenu();
        if (!menu || !add || !presets) {
            if (presets) DestroyMenu(presets); if (add) DestroyMenu(add); if (menu) DestroyMenu(menu);
            return;
        }
        AppendMenuW(add, MF_STRING, 100, L"New Playlist");
        AppendMenuW(add, MF_STRING, 101, L"New Autoplaylist...");
        constexpr std::array<const wchar_t*, 11> presetNames{
            L"Library (full)", L"Never played", L"History - last week", L"Played often",
            L"Recently added - 12 weeks", L"Unrated", L"Rated 1", L"Rated 2", L"Rated 3", L"Rated 4", L"Rated 5"};
        for (std::size_t index = 0; index < presetNames.size(); ++index) {
            AppendMenuW(presets, MF_STRING, 200 + static_cast<UINT>(index), presetNames[index]);
        }
        AppendMenuW(add, MF_POPUP, reinterpret_cast<UINT_PTR>(presets), L"Preset Autoplaylists");
        AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(add), L"Add Playlist...");
        AppendMenuW(menu, MF_STRING, 102, L"Load Playlist...");
        AppendMenuW(menu, MF_STRING, 103, L"Save All Playlists...");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, 104, L"Playlist Browser preferences...");
        ClientToScreen(wnd, &clientPoint);
        const auto command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
            clientPoint.x, clientPoint.y, 0, wnd, nullptr);
        DestroyMenu(menu);
        if (command == 100) {
            const auto created = createManualPlaylist(L"New Playlist", true);
            if (created != SIZE_MAX) {
                refreshPlaylistBrowserSnapshot();
                m_playlistBrowserFocus = findPlaylistRowByGuid(m_playlistBrowserRows,
                    playlist_manager_v5::get()->playlist_get_guid(created));
                beginPlaylistBrowserRename();
            }
        } else if (command == 101) {
            if (createAutoplaylist(L"New Autoplaylist", "ALL")) {
                const auto created = playlist_manager::get()->get_active_playlist();
                if (created != SIZE_MAX && autoplaylist_manager::get()->is_client_present(created)) {
                    const auto client = autoplaylist_manager::get()->query_client(created);
                    autoplaylist_client_v2::ptr clientV2;
                    if (client->service_query_t(clientV2) && clientV2->show_ui_available()) {
                        client->show_ui(created);
                    }
                }
            }
        } else if (command == 102) standard_commands::main_load_playlist();
        else if (command == 103) standard_commands::main_save_all_playlists();
        else if (command == 104) ui_control::get()->show_preferences(kPlaylistBrowserPreferencesPageGuid);
        else if (command >= 200 && command < 211) {
            constexpr std::array<const char*, 11> queries{
                "ALL", "%play_count% IS 0",
                "%last_played% DURING LAST 1 WEEK SORT DESCENDING BY %last_played%",
                "%play_count% GREATER 0 SORT DESCENDING BY %play_count%",
                "%added% DURING LAST 12 WEEKS SORT DESCENDING BY %added%", "%rating% MISSING",
                "%rating% IS 1", "%rating% IS 2", "%rating% IS 3", "%rating% IS 4", "%rating% IS 5"};
            const auto index = static_cast<std::size_t>(command - 200);
            (void)createAutoplaylist(presetNames[index], queries[index]);
        }
    }

    [[nodiscard]] bool playlistSnapshotMatchesAt(
        std::size_t playlist, const metadb_handle_list& snapshot) const {
        const auto api = playlist_manager::get();
        if (playlist == SIZE_MAX || playlist >= api->get_playlist_count()
            || api->playlist_get_item_count(playlist) != snapshot.get_count()) return false;
        metadb_handle_list current;
        api->playlist_get_all_items(playlist, current);
        for (t_size index = 0; index < current.get_count(); ++index) {
            if (current[index] != snapshot[index]) return false;
        }
        return true;
    }

    [[nodiscard]] D2D1_POINT_2F clientPointFromScreen(POINTL screen) const {
        POINT point{screen.x, screen.y};
        ScreenToClient(get_wnd(), &point);
        return D2D1::Point2F(static_cast<float>(point.x) / dpiScale(), static_cast<float>(point.y) / dpiScale());
    }

    HRESULT dragEnter(IDataObject* data, DWORD keys, POINTL point, DWORD* effect) {
        if (m_oleDragFromThisPanel) {
            // We created this object from a frozen metadb selection immediately before
            // DoDragDrop. Do not make the same-window queue target depend on a second,
            // unrelated incoming-file format probe.
            m_externalDropAccepts = true;
            m_externalDropNative = true;
        } else {
            DWORD suggested = *effect;
            m_externalDropAccepts = playlist_incoming_item_filter::get()->process_dropped_files_check_ex(data, &suggested);
            m_externalDropNative = m_externalDropAccepts
                && playlist_incoming_item_filter::get()->process_dropped_files_check_if_native(data);
        }
        return dragOver(data, keys, point, effect);
    }

    HRESULT dragOver(IDataObject*, DWORD keys, POINTL point, DWORD* effect) {
        if (!effect) return E_INVALIDARG;
        const auto allowed = *effect;
        const auto client = clientPointFromScreen(point);
        const auto layout = calculateLayout(get_wnd());
        const auto queueTarget = m_queueMode && contains(layout.queuePanel, client) && m_externalDropNative;
        const auto playlistTarget = contains(layout.playlistArea, client) && playlistCanAdd(m_activePlaylist);
        bool browserTarget{};
        m_externalDropBrowserBlank = false;
        m_externalDropBrowserGuid = GUID{};
        if (m_workspace == Workspace::playlist && contains(layout.playlistBrowserBody, client)) {
            if (const auto row = playlistBrowserRowAt(layout, client)) {
                const auto playlist = playlistByGuid(m_playlistBrowserRows[*row].guid);
                browserTarget = playlist != SIZE_MAX && playlistCanAdd(playlist)
                    && !autoplaylist_manager::get()->is_client_present(playlist);
                if (browserTarget) m_externalDropBrowserGuid = m_playlistBrowserRows[*row].guid;
            } else {
                browserTarget = m_playlistFilterText.empty();
                m_externalDropBrowserBlank = browserTarget;
            }
        }
        if (!m_externalDropAccepts || (!queueTarget && !playlistTarget && !browserTarget)) {
            *effect = DROPEFFECT_NONE;
            m_externalDropActive = false;
            m_externalDropDestination = DropDestination::none;
            invalidate();
            return S_OK;
        }
        m_externalDropActive = true;
        m_externalDropDestination = queueTarget ? DropDestination::queue
            : (browserTarget ? DropDestination::playlistBrowser : DropDestination::playlist);
        if (queueTarget) {
            *effect = (allowed & DROPEFFECT_COPY) != 0 ? DROPEFFECT_COPY : DROPEFFECT_NONE;
            invalidate();
            return S_OK;
        }
        if (browserTarget) {
            *effect = (allowed & DROPEFFECT_COPY) != 0 ? DROPEFFECT_COPY : DROPEFFECT_NONE;
            invalidate();
            return S_OK;
        }
        m_externalDropInsertion = playlistInsertionAt(get_wnd(), client);
        m_externalDropDisplayBoundary = playlistDisplayBoundaryAt(get_wnd(), client);
        const auto wantsCopy = (keys & MK_CONTROL) != 0;
        if (m_externalDropNative && !wantsCopy && (allowed & DROPEFFECT_MOVE) != 0) *effect = DROPEFFECT_MOVE;
        else if ((allowed & DROPEFFECT_COPY) != 0) *effect = DROPEFFECT_COPY;
        else *effect = DROPEFFECT_NONE;
        invalidate();
        return S_OK;
    }

    void dragLeave() {
        m_externalDropActive = false;
        m_externalDropAccepts = false;
        m_externalDropDestination = DropDestination::none;
        m_externalDropBrowserGuid = GUID{};
        m_externalDropBrowserBlank = false;
        invalidate();
    }

    bool insertDroppedItems(const GUID& guid, const metadb_handle_list& snapshot,
        std::size_t insertion, metadb_handle_list_cref items, bool* insertedAll = nullptr) {
        if (insertedAll) *insertedAll = false;
        const auto playlist = playlistByGuid(guid);
        if (items.get_count() == 0 || !playlistCanAdd(playlist)
            || !playlistSnapshotMatchesAt(playlist, snapshot)) return false;
        auto api = playlist_manager::get();
        std::vector<metadb_handle_ptr> existing;
        existing.reserve(snapshot.get_count());
        for (const auto& item : snapshot) existing.push_back(item);
        std::vector<metadb_handle_ptr> incoming;
        incoming.reserve(items.get_count());
        for (const auto& item : items) incoming.push_back(item);
        const auto filteredValues = uniqueValuesNotIn(existing, incoming);
        if (insertedAll) *insertedAll = filteredValues.size() == incoming.size();
        metadb_handle_list filtered;
        for (const auto& item : filteredValues) filtered.add_item(item);
        if (filtered.get_count() == 0) {
            showStatus(L"All dropped tracks are already in this playlist.");
            return true;
        }
        api->playlist_undo_backup(playlist);
        const auto inserted = api->playlist_insert_items(playlist,
            std::min<std::size_t>(insertion, snapshot.get_count()), filtered, pfc::bit_array_true());
        return inserted != SIZE_MAX;
    }

    void beginAsyncDrop(IDataObject* data, const GUID& guid, const metadb_handle_list& snapshot,
        std::size_t insertion, bool createPlaylist = false) {
        if (!data || (!createPlaylist && nullGuid(guid))) return;
        const auto state = m_dropAsync;
        const auto generation = state->generation.fetch_add(1) + 1;
        const auto notify = process_locations_notify::create(
            [state, generation, guid, insertion, snapshot, createPlaylist](metadb_handle_list_cref items) mutable {
                auto* completion = new DropCompletion();
                completion->generation = generation;
                completion->playlistGuid = guid;
                completion->insertion = insertion;
                completion->targetSnapshot = snapshot;
                completion->items = items;
                completion->createPlaylist = createPlaylist;
                fb2k::inMainThread([state, completion] {
                    if (!state->alive.load() || state->generation.load() != completion->generation
                        || !IsWindow(state->window)) {
                        delete completion;
                        return;
                    }
                    if (!PostMessage(state->window, kDropReadyMessage, 0,
                            reinterpret_cast<LPARAM>(completion))) delete completion;
                });
            });
        playlist_incoming_item_filter_v2::get()->process_dropped_files_async(data,
            playlist_incoming_item_filter_v2::op_flag_delay_ui, get_wnd(), notify);
    }

    void applyDropCompletion(DropCompletion* raw) {
        const std::unique_ptr<DropCompletion> completion(raw);
        if (!completion || completion->generation != m_dropAsync->generation.load()) return;
        auto guid = completion->playlistGuid;
        auto snapshot = completion->targetSnapshot;
        if (completion->createPlaylist && completion->items.get_count() > 0) {
            const auto created = createManualPlaylist(L"Dragged Items", true);
            if (created != SIZE_MAX) guid = playlist_manager_v5::get()->playlist_get_guid(created);
            snapshot.remove_all();
        }
        if (nullGuid(guid) || !insertDroppedItems(guid, snapshot,
                completion->insertion, completion->items)) {
            showStatus(L"The target playlist changed or rejected these items.");
        }
        rebuildPlaylistSnapshot(false);
        refreshPlaylistBrowserSnapshot();
    }

    HRESULT dropData(IDataObject* data, DWORD keys, POINTL point, DWORD* effect) {
        if (!data || !effect) return E_INVALIDARG;
        const auto allowed = *effect;
        dragOver(data, keys, point, effect);
        if (*effect == DROPEFFECT_NONE) { dragLeave(); return S_OK; }
        const auto destination = m_externalDropDestination;
        if (destination == DropDestination::queue) {
            bool added{};
            if (m_oleDragFromThisPanel) {
                if (playlistSnapshotMatches(m_playlistDragSnapshot)) {
                    addPlaylistSelectionToQueue();
                    added = true;
                }
                m_oleDropHandledByThisPanel = true;
            } else if (m_externalDropNative) {
                metadb_handle_list items;
                const pfc::com_ptr_t<IDataObject> object(data);
                if (SUCCEEDED(ole_interaction::get()->parse_dataobject_immediate(object, items))) {
                    for (const auto& item : items) playlist_manager::get()->queue_add_item(item);
                    added = items.get_count() > 0;
                }
            }
            *effect = added ? DROPEFFECT_COPY : DROPEFFECT_NONE;
            dragLeave();
            return S_OK;
        }
        if (destination == DropDestination::playlistBrowser) {
            const auto targetGuid = m_externalDropBrowserGuid;
            const auto createOnBlank = m_externalDropBrowserBlank;
            metadb_handle_list targetSnapshot;
            if (!createOnBlank) {
                const auto target = playlistByGuid(targetGuid);
                if (target != SIZE_MAX) playlist_manager::get()->playlist_get_all_items(target, targetSnapshot);
            }
            bool inserted{};
            if (m_externalDropNative) {
                metadb_handle_list items;
                const pfc::com_ptr_t<IDataObject> object(data);
                if (SUCCEEDED(ole_interaction::get()->parse_dataobject_immediate(object, items))
                    && items.get_count() > 0) {
                    auto guid = targetGuid;
                    if (createOnBlank) {
                        const auto created = createManualPlaylist(L"Dragged Items", true);
                        if (created != SIZE_MAX) guid = playlist_manager_v5::get()->playlist_get_guid(created);
                    }
                    inserted = !nullGuid(guid) && insertDroppedItems(guid, targetSnapshot,
                        targetSnapshot.get_count(), items);
                }
            } else {
                beginAsyncDrop(data, targetGuid, targetSnapshot, targetSnapshot.get_count(), createOnBlank);
                inserted = true;
            }
            if (m_oleDragFromThisPanel) m_oleDropHandledByThisPanel = true;
            *effect = inserted ? DROPEFFECT_COPY : DROPEFFECT_NONE;
            dragLeave();
            refreshPlaylistBrowserSnapshot();
            return S_OK;
        }
        const auto insertion = m_externalDropInsertion;
        const auto guid = activePlaylistGuid();
        const auto snapshot = m_playlistItems;
        if (m_oleDragFromThisPanel && InlineIsEqualGUID(guid, m_playlistDragGuid)) {
            m_oleDropHandledByThisPanel = true;
            *effect = DROPEFFECT_NONE;
            if (playlistSnapshotMatches(m_playlistDragSnapshot)) {
                std::vector<std::size_t> selected;
                for (std::size_t index = 0; index < m_playlistDragSelection.size(); ++index) {
                    if (m_playlistDragSelection[index]) selected.push_back(index);
                }
                const auto plan = planPlaylistMove(m_playlistDragSnapshot.get_count(), selected, insertion);
                if (plan.changed && playlistCanReorder()) {
                    std::vector<t_size> order(plan.order.begin(), plan.order.end());
                    auto api = playlist_manager::get();
                    api->playlist_undo_backup(m_activePlaylist);
                    *effect = api->playlist_reorder_items(m_activePlaylist, order.data(), order.size())
                        ? DROPEFFECT_MOVE : DROPEFFECT_NONE;
                }
            }
            dragLeave();
            return S_OK;
        }
        if (m_externalDropNative) {
            metadb_handle_list items;
            const pfc::com_ptr_t<IDataObject> object(data);
            bool insertedAll{};
            if (SUCCEEDED(ole_interaction::get()->parse_dataobject_immediate(object, items))
                && insertDroppedItems(guid, snapshot, insertion, items, &insertedAll)) {
                *effect = (insertedAll && (keys & MK_CONTROL) == 0 && (allowed & DROPEFFECT_MOVE) != 0)
                    ? DROPEFFECT_MOVE : DROPEFFECT_COPY;
            } else {
                *effect = DROPEFFECT_NONE;
            }
        } else {
            beginAsyncDrop(data, guid, snapshot, insertion);
            *effect = (allowed & DROPEFFECT_COPY) != 0 ? DROPEFFECT_COPY : DROPEFFECT_NONE;
        }
        dragLeave();
        return S_OK;
    }

    void beginOutgoingPlaylistDrag(HWND wnd) {
        if (!m_playlistDragging || m_playlistDragSnapshot.get_count() == 0) return;
        metadb_handle_list selectedHandles;
        for (std::size_t index = 0; index < m_playlistDragSelection.size(); ++index) {
            if (m_playlistDragSelection[index]) selectedHandles.add_item(m_playlistDragSnapshot[index]);
        }
        if (selectedHandles.get_count() == 0) return;
        const auto sourceGuid = activePlaylistGuid();
        m_playlistDragGuid = sourceGuid;
        const auto sourceSnapshot = m_playlistDragSnapshot;
        const auto sourceSelection = m_playlistDragSelection;
        auto object = ole_interaction::get()->create_dataobject(selectedHandles);
        pfc::com_ptr_t<DropSourceImpl> source = new DropSourceImpl();
        KillTimer(wnd, kPlaylistDragTimer);
        releaseCapturePreservingInteraction(wnd);
        m_active = ControlId::none;
        m_playlistDragging = false;
        DWORD outEffect = DROPEFFECT_NONE;
        m_oleDropHandledByThisPanel = false;
        m_oleDragFromThisPanel = true;
        const auto status = DoDragDrop(object.get_ptr(), source.get_ptr(), DROPEFFECT_COPY | DROPEFFECT_MOVE, &outEffect);
        m_oleDragFromThisPanel = false;
        if (!m_oleDropHandledByThisPanel && status == DRAGDROP_S_DROP && outEffect == DROPEFFECT_MOVE) {
            const auto playlist = playlistByGuid(sourceGuid);
            if (playlistCanRemove(playlist) && playlistSnapshotMatchesAt(playlist, sourceSnapshot)) {
                pfc::bit_array_bittable mask;
                mask.resize(sourceSnapshot.get_count());
                for (std::size_t index = 0; index < sourceSelection.size(); ++index) mask.set(index, sourceSelection[index]);
                auto api = playlist_manager::get();
                api->playlist_undo_backup(playlist);
                api->playlist_remove_items(playlist, mask);
            }
        }
        m_playlistDragCandidate.reset();
        m_playlistDragSnapshot.remove_all();
        m_playlistDragSelection.clear();
        m_playlistCollapseSelectionOnClick = false;
        rebuildPlaylistSnapshot(false);
    }

    void onLeftButtonDoubleClick(HWND wnd, LPARAM lp) {
        const auto point = pointFromLParam(lp, dpiScale());
        if (m_workspace == Workspace::album) {
            const auto layout = calculateLayout(wnd);
            if (const auto album = albumAt(layout, point)) {
                m_albumSelection = *album;
                resetAlbumTrackSelection();
                persistAlbumBrowserState();
                playAlbumTrack(0);
            } else if (const auto row = albumTrackAt(layout, point)) {
                selectAlbumTrack(*row, false, false);
                playAlbumTrack(*row);
            }
            invalidate();
            return;
        }
        if (m_queueMode) {
            if (const auto row = queueRowAt(wnd, point)) {
                selectQueueRow(*row, false, false);
                playQueueItemNow(*row);
                return;
            }
        }
        if (const auto row = playlistRowAt(wnd, point); row && m_activePlaylist != SIZE_MAX) {
            // Selecting a row can synchronously notify this panel and rebuild display rows.
            const auto display = m_playlistDisplayRows[*row];
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

    [[nodiscard]] std::optional<std::pair<std::size_t, int>> playlistRatingAt(
        HWND wnd, D2D1_POINT_2F point) const {
        const auto displayRow = playlistRowAt(wnd, point);
        if (!displayRow) return std::nullopt;
        const auto& display = m_playlistDisplayRows[*displayRow];
        if (display.kind != PlaylistDisplayRowKind::track) return std::nullopt;
        const auto layout = calculateLayout(wnd);
        auto totalWeight = 0;
        for (const auto& column : m_playlistSettings.columns) if (column.visible) totalWeight += column.widthWeight;
        auto left = layout.playlistBody.left;
        for (const auto& column : m_playlistSettings.columns) {
            if (!column.visible) continue;
            const auto right = left + (layout.playlistBody.right - layout.playlistBody.left)
                * static_cast<float>(column.widthWeight) / static_cast<float>(std::max(1, totalWeight));
            if (column.id == "builtin.rating" && point.x >= left && point.x <= right) {
                const auto usableLeft = left + 5.0F;
                const auto usableRight = right - 5.0F;
                if (usableRight <= usableLeft) return std::nullopt;
                const auto value = std::clamp(static_cast<int>((point.x - usableLeft)
                    / (usableRight - usableLeft) * 5.0F) + 1, 1, 5);
                return std::pair{display.track, value};
            }
            left = right;
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
                if (m_groupArtworkAborter && !m_groupArtworkAborterAlbum) m_groupArtworkAborter->abort();
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

    void showQueueMenu(HWND wnd, D2D1_POINT_2F point, POINT clientPoint) {
        const auto layout = calculateLayout(wnd);
        if (!m_queueMode || !contains(layout.queuePanel, point)) return;
        if (const auto row = queueRowAt(wnd, point)) {
            if (*row >= m_queueSelected.size() || !m_queueSelected[*row]) {
                selectQueueRow(*row, false, false);
            } else {
                m_queueFocus = *row;
            }
        }
        const auto hasSelection = std::any_of(
            m_queueSelected.begin(), m_queueSelected.end(), [](bool selected) { return selected; });
        const auto canPlay = m_queueFocus < m_queueItems.size();
        const auto menu = CreatePopupMenu();
        if (!menu) return;
        AppendMenuW(menu, MF_STRING | (canPlay ? 0 : MF_GRAYED), 1, L"Play now");
        AppendMenuW(menu, MF_STRING | (hasSelection ? 0 : MF_GRAYED), 2, L"Remove selected");
        AppendMenuW(menu, MF_STRING | (!m_queueItems.empty() ? 0 : MF_GRAYED), 3, L"Clear playback queue");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING | (hasSelection ? 0 : MF_GRAYED), 4, L"Track actions...");
        ClientToScreen(wnd, &clientPoint);
        const auto command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
            clientPoint.x, clientPoint.y, 0, wnd, nullptr);
        DestroyMenu(menu);
        if (command == 1 && canPlay) {
            playQueueItemNow(m_queueFocus);
        } else if (command == 2 && hasSelection) {
            removeSelectedQueueItems();
        } else if (command == 3 && !m_queueItems.empty()) {
            playlist_manager::get()->queue_flush();
        } else if (command == 4 && hasSelection) {
            metadb_handle_list handles;
            for (std::size_t index = 0; index < m_queueItems.size(); ++index) {
                if (index < m_queueSelected.size() && m_queueSelected[index]) {
                    handles.add_item(m_queueItems[index].m_handle);
                }
            }
            if (handles.get_count() > 0) {
                contextmenu_manager::win32_run_menu_context(wnd, handles, &clientPoint);
            }
        }
    }

    void onRightButtonUp(HWND wnd, D2D1_POINT_2F point, POINT clientPoint) {
        const auto layout = calculateLayout(wnd);
        if (m_workspace == Workspace::album) {
            if (const auto row = albumTrackAt(layout, point); row && m_albumSelection < m_albums.size()) {
                if (*row >= m_albumTrackSelected.size() || !m_albumTrackSelected[*row]) {
                    selectAlbumTrack(*row, false, false);
                }
                const auto source = m_albums[m_albumSelection].sourceIndices[*row];
                if (source < m_albumSourceItems.get_count()) {
                    metadb_handle_list items;
                    for (std::size_t index = 0; index < m_albumTrackSelected.size(); ++index) {
                        if (!m_albumTrackSelected[index]) continue;
                        const auto selectedSource = m_albums[m_albumSelection].sourceIndices[index];
                        if (selectedSource < m_albumSourceItems.get_count()) {
                            items.add_item(m_albumSourceItems[selectedSource]);
                        }
                    }
                    ClientToScreen(wnd, &clientPoint);
                    contextmenu_manager::win32_run_menu_context(wnd, items, &clientPoint);
                }
                invalidate();
            }
            return;
        }
        if (m_queueMode && contains(layout.queuePanel, point)) {
            showQueueMenu(wnd, point, clientPoint);
            return;
        }
        if (contains(layout.playlistBrowser, point)) {
            showPlaylistBrowserMenu(wnd, point, clientPoint);
            return;
        }
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
                if (m_groupArtworkAborter && !m_groupArtworkAborterAlbum) m_groupArtworkAborter->abort();
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

    bool onPlaylistBrowserKeyDown(HWND wnd, WPARAM key) {
        const auto count = m_playlistBrowserRows.size();
        const auto ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        const auto shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        if (ctrl && key == 'A') {
            m_playlistBrowserSelection.clear();
            for (const auto& row : m_playlistBrowserRows) m_playlistBrowserSelection.push_back(row.guid);
            invalidate();
            return true;
        }
        if (key == VK_DELETE) {
            deleteSelectedPlaylists();
            return true;
        }
        if (key == VK_F2) {
            beginPlaylistBrowserRename();
            return true;
        }
        if (count == 0) return false;
        auto focus = m_playlistBrowserFocus < count ? m_playlistBrowserFocus : 0;
        const auto layout = calculateLayout(wnd);
        const auto page = std::max<std::size_t>(1, visibleRowCapacity(
            layout.playlistBrowserBody.bottom - layout.playlistBrowserBody.top, 30.0F) - 1);
        auto target = focus;
        if (key == VK_UP) target = focus > 0 ? focus - 1 : 0;
        else if (key == VK_DOWN) target = std::min(count - 1, focus + 1);
        else if (key == VK_PRIOR) target = focus > page ? focus - page : 0;
        else if (key == VK_NEXT) target = std::min(count - 1, focus + page);
        else if (key == VK_HOME) target = 0;
        else if (key == VK_END) target = count - 1;
        else if (key == VK_RETURN || key == VK_SPACE) {
            const auto playlist = playlistByGuid(m_playlistBrowserRows[focus].guid);
            if (playlist != SIZE_MAX) playlist_manager::get()->set_active_playlist(playlist);
            return true;
        } else return false;
        selectPlaylistBrowserRow(target, ctrl, shift, !ctrl && !shift);
        m_playlistBrowserTopRow = ensureRowVisible(m_playlistBrowserTopRow, target, count,
            visibleRowCapacity(layout.playlistBrowserBody.bottom
                - layout.playlistBrowserBody.top, 30.0F));
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
        refreshPlaylistBrowserSnapshot();
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
            compiler->compile_safe(m_queueLengthScript, "[%length%]");
            compiler->compile_safe(m_albumNameScript, "[%album%]");
            compiler->compile_safe(m_albumArtistScript, "$if2(%album artist%,%artist%)");
            compiler->compile_safe(m_albumDateScript, "[%date%]");
            compiler->compile_safe(m_albumDiscScript, "[%discnumber%]");
            compiler->compile_safe(m_albumTrackScript, "[%tracknumber%]");
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
        m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0x5F7495), m_defaultPlaylist.ReleaseAndGetAddressOf());
        m_renderTarget->CreateSolidColorBrush(D2D1::ColorF(0xFFFFFF), m_light.ReleaseAndGetAddressOf());
        createArtworkBitmap();
        return m_background && m_surface && m_foreground && m_disabled && m_accent
            && m_defaultPlaylist && m_light;
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
        for (auto& [key, entry] : m_albumArtworkCache) {
            (void)key;
            entry.bitmap.Reset();
        }
        m_light.Reset();
        m_artworkBitmap.Reset();
        m_defaultPlaylist.Reset();
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
        layout.open = D2D1::RectF(center - 138.0F, buttonY, center - 102.0F, buttonY + 36.0F);
        layout.previous = D2D1::RectF(center - 92.0F, buttonY, center - 56.0F, buttonY + 36.0F);
        layout.playPause = D2D1::RectF(center - 46.0F, buttonY - 3.0F, center - 4.0F, buttonY + 39.0F);
        layout.next = D2D1::RectF(center + 6.0F, buttonY, center + 42.0F, buttonY + 36.0F);
        layout.stop = D2D1::RectF(center + 52.0F, buttonY, center + 88.0F, buttonY + 36.0F);
        layout.mute = D2D1::RectF(std::max(center + 100.0F, width - 190.0F), buttonY, std::max(center + 136.0F, width - 154.0F), buttonY + 36.0F);
        layout.volume = D2D1::RectF(std::max(center + 142.0F, width - 146.0F), buttonY + 8.0F,
            width - 16.0F, buttonY + 28.0F);
        layout.outputDevice = D2D1::RectF(layout.mute.left - 42.0F, buttonY,
            layout.mute.left - 6.0F, buttonY + 36.0F);
        layout.playbackOrder = D2D1::RectF(layout.mute.left - 84.0F, buttonY,
            layout.mute.left - 48.0F, buttonY + 36.0F);
        layout.queueToggle = D2D1::RectF(layout.mute.left - 126.0F, buttonY,
            layout.mute.left - 90.0F, buttonY + 36.0F);
        layout.playlistWorkspace = D2D1::RectF(16.0F, buttonY, 52.0F, buttonY + 36.0F);
        layout.albumWorkspace = D2D1::RectF(58.0F, buttonY, 94.0F, buttonY + 36.0F);
        layout.albumList = D2D1::RectF(100.0F, buttonY, 136.0F, buttonY + 36.0F);
        if (readSettings().showSettingsButton) layout.settings = D2D1::RectF(142.0F, buttonY, 178.0F, buttonY + 36.0F);

        if (m_workspace == Workspace::album && top > 100.0F && width > 420.0F) {
            const auto split = std::clamp(width * static_cast<float>(m_albumSplitPermille) / 1000.0F,
                240.0F, std::max(241.0F, width - 280.0F));
            layout.albumSource = D2D1::RectF(0.0F, 0.0F, split - 4.0F, 42.0F);
            layout.albumGrid = D2D1::RectF(0.0F, 42.0F, split - 16.0F, top);
            layout.albumGridScrollTrack = D2D1::RectF(split - 16.0F, 42.0F, split - 4.0F, top);
            layout.albumDivider = D2D1::RectF(split - 4.0F, 0.0F, split + 4.0F, top);
            layout.albumHeader = D2D1::RectF(split + 4.0F, 0.0F, width, 70.0F);
            layout.albumTrackHeader = D2D1::RectF(split + 4.0F, 70.0F, width, 98.0F);
            layout.albumTrackBody = D2D1::RectF(split + 4.0F, 98.0F, width - 12.0F, top);
            layout.albumTrackScrollTrack = D2D1::RectF(width - 12.0F, 98.0F, width, top);
            const auto columns = albumGridColumns(layout);
            const auto albumRows = (m_albums.size() + columns - 1) / columns;
            const auto visibleAlbumRows = std::max<std::size_t>(1, static_cast<std::size_t>(
                (layout.albumGrid.bottom - layout.albumGrid.top) / 184.0F));
            const auto albumMaximum = albumRows > visibleAlbumRows ? albumRows - visibleAlbumRows : 0;
            const auto albumTrackHeight = layout.albumGridScrollTrack.bottom - layout.albumGridScrollTrack.top;
            const auto albumThumbHeight = scrollbarThumbHeight(albumTrackHeight, albumRows, visibleAlbumRows);
            const auto albumThumbTop = scrollbarThumbTop(layout.albumGridScrollTrack.top, albumTrackHeight,
                albumThumbHeight, m_albumGridTopRow, albumMaximum);
            layout.albumGridScrollThumb = D2D1::RectF(layout.albumGridScrollTrack.left + 2.0F,
                albumThumbTop, layout.albumGridScrollTrack.right - 2.0F, albumThumbTop + albumThumbHeight);
            const auto trackCount = m_albumSelection < m_albums.size()
                ? m_albums[m_albumSelection].sourceIndices.size() : 0;
            const auto trackCapacity = visibleRowCapacity(layout.albumTrackBody.bottom
                - layout.albumTrackBody.top, kCompactTrackRowHeight);
            const auto trackMaximum = maximumTopRow(trackCount, trackCapacity);
            const auto trackHeight = layout.albumTrackScrollTrack.bottom - layout.albumTrackScrollTrack.top;
            const auto trackThumbHeight = scrollbarThumbHeight(trackHeight, trackCount, trackCapacity);
            const auto trackThumbTop = scrollbarThumbTop(layout.albumTrackScrollTrack.top, trackHeight,
                trackThumbHeight, m_albumTrackTopRow, trackMaximum);
            layout.albumTrackScrollThumb = D2D1::RectF(layout.albumTrackScrollTrack.left + 2.0F,
                trackThumbTop, layout.albumTrackScrollTrack.right - 2.0F, trackThumbTop + trackThumbHeight);
            const auto centerX = (layout.albumGrid.left + layout.albumGrid.right) * 0.5F;
            const auto centerY = (layout.albumGrid.top + layout.albumGrid.bottom) * 0.5F;
            layout.albumLibraryPreferences = D2D1::RectF(centerX - 110.0F, centerY + 18.0F,
                centerX + 110.0F, centerY + 54.0F);
            return layout;
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
            layout.queuePanel = layout.rightColumn;
            constexpr float queueHeaderHeight = 42.0F;
            constexpr float queueListHeaderHeight = kCompactTrackHeaderHeight;
            constexpr float queueScrollbarWidth = 12.0F;
            layout.queueHeader = D2D1::RectF(left, 0.0F, width, queueHeaderHeight);
            layout.queueListHeader = D2D1::RectF(left, queueHeaderHeight, width, queueHeaderHeight + queueListHeaderHeight);
            layout.queueBody = D2D1::RectF(left, queueHeaderHeight + queueListHeaderHeight,
                width - queueScrollbarWidth, top);
            layout.queueScrollTrack = D2D1::RectF(width - queueScrollbarWidth,
                queueHeaderHeight + queueListHeaderHeight, width, top);
            constexpr float queueRowHeight = kCompactTrackRowHeight;
            const auto queueCapacity = visibleRowCapacity(
                layout.queueBody.bottom - layout.queueBody.top, queueRowHeight);
            const auto queueMaximum = maximumTopRow(m_queueItems.size(), queueCapacity);
            const auto queueTrackHeight = layout.queueScrollTrack.bottom - layout.queueScrollTrack.top;
            const auto queueThumbHeight = scrollbarThumbHeight(queueTrackHeight, m_queueItems.size(), queueCapacity);
            const auto queueThumbTop = scrollbarThumbTop(layout.queueScrollTrack.top, queueTrackHeight,
                queueThumbHeight, m_queueTopRow, queueMaximum);
            layout.queueScrollThumb = D2D1::RectF(layout.queueScrollTrack.left + 2.0F,
                queueThumbTop, layout.queueScrollTrack.right - 2.0F, queueThumbTop + queueThumbHeight);
        }

        const auto playlistRight = layout.rightColumn.right > layout.rightColumn.left
            ? layout.rightColumn.left : width;
        if (playlistRight > 120.0F && top > 60.0F) {
            constexpr float headerHeight = 30.0F;
            constexpr float filterHeight = 38.0F;
            constexpr float scrollbarWidth = 12.0F;
            constexpr float browserDividerWidth = 8.0F;
            const auto maxBrowser = std::max(0.0F,
                std::min(width * 0.35F, playlistRight - 120.0F - browserDividerWidth));
            const auto minBrowser = std::min(180.0F, maxBrowser);
            const auto storedBrowser = width * static_cast<float>(m_playlistBrowserSplitPermille) / 1000.0F;
            const auto browserWidth = std::clamp(storedBrowser, minBrowser, maxBrowser);
            layout.playlistBrowser = D2D1::RectF(0.0F, 0.0F, browserWidth, top);
            layout.playlistBrowserHeader = D2D1::RectF(0.0F, 0.0F, browserWidth, headerHeight);
            layout.playlistBrowserFilter = D2D1::RectF(8.0F, headerHeight + 5.0F,
                std::max(9.0F, browserWidth - 38.0F), headerHeight + filterHeight - 5.0F);
            layout.playlistBrowserFilterClear = D2D1::RectF(std::max(9.0F, browserWidth - 34.0F),
                headerHeight + 5.0F, std::max(10.0F, browserWidth - 8.0F), headerHeight + filterHeight - 5.0F);
            layout.playlistBrowserBody = D2D1::RectF(0.0F, headerHeight + filterHeight, browserWidth, top);
            layout.playlistBrowserDivider = D2D1::RectF(browserWidth, 0.0F,
                browserWidth + browserDividerWidth, top);
            const auto playlistLeft = browserWidth + browserDividerWidth;
            layout.playlistArea = D2D1::RectF(playlistLeft, 0.0F, playlistRight, top);
            layout.playlistHeader = D2D1::RectF(playlistLeft, 0.0F, playlistRight, headerHeight);
            layout.playlistBody = D2D1::RectF(playlistLeft, headerHeight,
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
        if (m_workspace == Workspace::album) {
            if (contains(layout.albumDivider, point)) return ControlId::albumDivider;
            if (contains(layout.albumSource, point)) return ControlId::albumSource;
            if (contains(layout.albumGridScrollTrack, point)) return ControlId::albumGridScrollbar;
            if (contains(layout.albumTrackScrollTrack, point)) return ControlId::albumTrackScrollbar;
            if (m_albumSourceKind == AlbumSourceKind::mediaLibrary
                && !library_manager::get()->is_library_enabled()
                && contains(layout.albumLibraryPreferences, point)) return ControlId::albumLibraryPreferences;
            if (contains(layout.albumGrid, point)) return ControlId::albumGrid;
            if (contains(layout.albumTrackBody, point)) return ControlId::albumTrackList;
        }
        if (m_queueMode) {
            if (contains(layout.queueScrollTrack, point)) return ControlId::queueScrollbar;
            if (contains(layout.queueBody, point)) return ControlId::queueBody;
            if (contains(layout.queueHeader, point) || contains(layout.queueListHeader, point)) return ControlId::queueHeader;
        } else {
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
        }
        if (m_workspace == Workspace::playlist && contains(layout.playlistBrowserDivider, point)) {
            return ControlId::playlistBrowserDivider;
        }
        if (m_workspace == Workspace::playlist && contains(layout.playlistBrowser, point)) {
            return ControlId::playlistBrowser;
        }
        if (contains(layout.playlistScrollTrack, point)) return ControlId::playlistScrollbar;
        if (contains(layout.playlistHeader, point)) return ControlId::playlistHeader;
        if (contains(layout.playlistBody, point)) return ControlId::playlistBody;
        if (contains(layout.seek, point)) return m_state.canSeek() ? ControlId::seek : ControlId::none;
        if (contains(layout.open, point)) return ControlId::open;
        if (contains(layout.previous, point)) return ControlId::previous;
        if (contains(layout.playPause, point)) return ControlId::playPause;
        if (contains(layout.next, point)) return ControlId::next;
        if (contains(layout.stop, point)) return m_state.playing ? ControlId::stop : ControlId::none;
        if (contains(layout.mute, point)) return ControlId::mute;
        if (contains(layout.volume, point)) return m_state.customVolume ? ControlId::none : ControlId::volume;
        if (contains(layout.settings, point)) return ControlId::settings;
        if (contains(layout.playlistWorkspace, point)) return ControlId::playlistWorkspace;
        if (contains(layout.albumWorkspace, point)) return ControlId::albumWorkspace;
        if (contains(layout.albumList, point)) return ControlId::albumList;
        if (contains(layout.queueToggle, point)) return ControlId::queueToggle;
        if (contains(layout.playbackOrder, point)) return ControlId::playbackOrder;
        if (contains(layout.outputDevice, point)) return ControlId::outputDevice;
        return ControlId::none;
    }

    [[nodiscard]] bool isEnabled(ControlId id) const noexcept {
        switch (id) {
        case ControlId::seek: return m_state.canSeek();
        case ControlId::stop: return m_state.playing;
        case ControlId::volume: return !m_state.customVolume;
        case ControlId::open: return true;
        case ControlId::previous:
        case ControlId::playPause:
        case ControlId::next:
        case ControlId::mute: return true;
        case ControlId::settings: return readSettings().showSettingsButton;
        case ControlId::playlistWorkspace:
        case ControlId::albumWorkspace:
        case ControlId::albumList:
        case ControlId::albumSource:
        case ControlId::albumGrid:
        case ControlId::albumTrackList:
        case ControlId::albumDivider: return true;
        case ControlId::albumLibraryPreferences: return true;
        case ControlId::albumGridScrollbar:
        case ControlId::albumTrackScrollbar: return true;
        case ControlId::queueToggle: return true;
        case ControlId::playbackOrder: return playlist_manager::get()->playback_order_get_count() > 0;
        case ControlId::outputDevice: return true;
        case ControlId::rating1:
        case ControlId::rating2:
        case ControlId::rating3:
        case ControlId::rating4:
        case ControlId::rating5: return m_ratingAvailable && m_target.is_valid();
        case ControlId::rightDivider:
        case ControlId::trackDetails: return true;
        case ControlId::playlistBody:
        case ControlId::playlistScrollbar: return true;
        case ControlId::playlistBrowser:
        case ControlId::playlistBrowserDivider: return true;
        case ControlId::queueBody:
        case ControlId::queueScrollbar: return true;
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

        if (m_workspace == Workspace::album) drawAlbumWorkspace(layout);
        else {
            drawPlaylistBrowser(layout);
            drawPlaylist(layout);
        }
        if (!m_statusText.empty() && (layout.playlistArea.right > layout.playlistArea.left
                || layout.albumGrid.right > layout.albumGrid.left)) {
            const auto area = m_workspace == Workspace::album ? layout.albumGrid : layout.playlistArea;
            const auto status = D2D1::RectF(area.left + 12.0F,
                std::max(area.top, area.bottom - 38.0F), area.right - 12.0F, area.bottom - 8.0F);
            m_renderTarget->FillRoundedRectangle(D2D1::RoundedRect(status, 4.0F, 4.0F), m_surface.Get());
            drawTextWith(m_statusText, status, m_secondaryFormat.Get(), DWRITE_TEXT_ALIGNMENT_CENTER, m_foreground.Get());
        }
        if (m_workspace == Workspace::playlist && m_queueMode) {
            drawPlaybackQueue(layout);
        } else if (m_workspace == Workspace::playlist) {
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
        }
        drawSeek(layout);
        drawButton(layout.open, ControlId::open);
        drawButton(layout.previous, ControlId::previous);
        drawButton(layout.playPause, ControlId::playPause);
        drawButton(layout.next, ControlId::next);
        drawButton(layout.stop, ControlId::stop);
        drawButton(layout.mute, ControlId::mute);
        if (readSettings().showSettingsButton) {
            drawButton(layout.settings, ControlId::settings);
        }
        drawButton(layout.playlistWorkspace, ControlId::playlistWorkspace);
        drawButton(layout.albumWorkspace, ControlId::albumWorkspace);
        drawButton(layout.albumList, ControlId::albumList);
        drawButton(layout.queueToggle, ControlId::queueToggle);
        drawButton(layout.playbackOrder, ControlId::playbackOrder);
        drawButton(layout.outputDevice, ControlId::outputDevice);
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
            for (auto& [key, entry] : m_albumArtworkCache) {
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

    [[nodiscard]] std::size_t albumGridColumns(const Layout& layout) const noexcept {
        return std::max<std::size_t>(1, static_cast<std::size_t>(
            std::max(1.0F, layout.albumGrid.right - layout.albumGrid.left) / 160.0F));
    }

    [[nodiscard]] std::optional<std::size_t> albumAt(const Layout& layout, D2D1_POINT_2F point) const {
        if (!contains(layout.albumGrid, point)) return {};
        const auto columns = albumGridColumns(layout);
        const auto cellWidth = (layout.albumGrid.right - layout.albumGrid.left) / static_cast<float>(columns);
        const auto column = std::min(columns - 1,
            static_cast<std::size_t>((point.x - layout.albumGrid.left) / cellWidth));
        const auto row = m_albumGridTopRow + static_cast<std::size_t>((point.y - layout.albumGrid.top) / 184.0F);
        const auto index = row * columns + column;
        return index < m_albums.size() ? std::optional<std::size_t>(index) : std::nullopt;
    }

    [[nodiscard]] std::optional<std::size_t> albumTrackAt(const Layout& layout, D2D1_POINT_2F point) const {
        if (!contains(layout.albumTrackBody, point) || m_albumSelection >= m_albums.size()) return {};
        const auto relative = point.y - layout.albumTrackBody.top;
        const auto index = m_albumTrackTopRow + static_cast<std::size_t>(relative / kCompactTrackRowHeight);
        return index < m_albums[m_albumSelection].sourceIndices.size()
            ? std::optional<std::size_t>(index) : std::nullopt;
    }

    void drawAlbumWorkspace(const Layout& layout) {
        if (layout.albumGrid.right <= layout.albumGrid.left) return;
        m_renderTarget->FillRectangle(layout.albumSource, m_surface.Get());
        m_renderTarget->DrawLine(D2D1::Point2F(layout.albumSource.left, layout.albumSource.bottom),
            D2D1::Point2F(layout.albumSource.right, layout.albumSource.bottom), m_disabled.Get());
        drawTextWith(L"Current source: " + m_albumSourceName + L"  ▾",
            D2D1::RectF(layout.albumSource.left + 14.0F, layout.albumSource.top,
                layout.albumSource.right - 10.0F, layout.albumSource.bottom),
            m_textFormat.Get(), DWRITE_TEXT_ALIGNMENT_LEADING, m_foreground.Get());
        m_renderTarget->FillRectangle(layout.albumDivider,
            m_active == ControlId::albumDivider || m_hover == ControlId::albumDivider
                ? m_accent.Get() : m_surface.Get());

        const auto columns = albumGridColumns(layout);
        const auto cellWidth = (layout.albumGrid.right - layout.albumGrid.left) / static_cast<float>(columns);
        const auto visibleRows = static_cast<std::size_t>(std::ceil(
            (layout.albumGrid.bottom - layout.albumGrid.top) / 184.0F)) + 1;
        const auto first = m_albumGridTopRow * columns;
        const auto last = std::min(m_albums.size(), first + visibleRows * columns);
        m_renderTarget->PushAxisAlignedClip(layout.albumGrid, D2D1_ANTIALIAS_MODE_ALIASED);
        for (auto index = first; index < last; ++index) {
            const auto local = index - first;
            const auto row = local / columns;
            const auto column = local % columns;
            const auto left = layout.albumGrid.left + static_cast<float>(column) * cellWidth;
            const auto top = layout.albumGrid.top + static_cast<float>(row) * 184.0F;
            const auto cell = D2D1::RectF(left + 4.0F, top + 4.0F, left + cellWidth - 4.0F, top + 180.0F);
            if (index == m_albumSelection) {
                m_renderTarget->FillRectangle(cell, m_surface.Get());
                m_renderTarget->DrawRectangle(cell, m_foreground.Get(), 1.0F);
            }
            const auto artSize = std::max(40.0F, std::min(132.0F, cellWidth - 20.0F));
            const auto art = D2D1::RectF(left + (cellWidth - artSize) * 0.5F, top + 10.0F,
                left + (cellWidth + artSize) * 0.5F, top + 10.0F + artSize);
            auto& artwork = requestAlbumArtwork(index);
            createGroupArtworkBitmap(artwork);
            if (artwork.bitmap) m_renderTarget->DrawBitmap(artwork.bitmap.Get(), art, 1.0F,
                D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
            else {
                m_renderTarget->FillRectangle(art, m_surface.Get());
                m_renderTarget->DrawRectangle(art, m_disabled.Get());
                drawTextWith(artwork.loading ? L"Loading..." : L"No cover", art,
                    m_secondaryFormat.Get(), DWRITE_TEXT_ALIGNMENT_CENTER, m_disabled.Get());
            }
            drawTextWith(utf8ToWide(m_albums[index].album.c_str()),
                D2D1::RectF(left + 8.0F, art.bottom + 5.0F, left + cellWidth - 8.0F, art.bottom + 25.0F),
                m_textFormat.Get(), DWRITE_TEXT_ALIGNMENT_CENTER, m_foreground.Get());
            drawTextWith(utf8ToWide(m_albums[index].albumArtist.c_str()),
                D2D1::RectF(left + 8.0F, art.bottom + 25.0F, left + cellWidth - 8.0F, art.bottom + 43.0F),
                m_secondaryFormat.Get(), DWRITE_TEXT_ALIGNMENT_CENTER, m_disabled.Get());
        }
        m_renderTarget->PopAxisAlignedClip();
        m_renderTarget->FillRectangle(layout.albumGridScrollTrack, m_surface.Get());
        if (layout.albumGridScrollThumb.bottom > layout.albumGridScrollThumb.top) {
            m_renderTarget->FillRoundedRectangle(D2D1::RoundedRect(layout.albumGridScrollThumb, 4.0F, 4.0F),
                m_disabled.Get());
        }
        if (m_albumSourceKind == AlbumSourceKind::mediaLibrary
            && !library_manager::get()->is_library_enabled()) {
            const auto message = layout.albumLibraryPreferences;
            drawTextWith(L"Media Library is not configured.", D2D1::RectF(message.left,
                message.top - 38.0F, message.right, message.top - 6.0F), m_textFormat.Get(),
                DWRITE_TEXT_ALIGNMENT_CENTER, m_disabled.Get());
            m_renderTarget->FillRoundedRectangle(D2D1::RoundedRect(message, 4.0F, 4.0F),
                m_hover == ControlId::albumLibraryPreferences ? m_light.Get() : m_surface.Get());
            m_renderTarget->DrawRoundedRectangle(D2D1::RoundedRect(message, 4.0F, 4.0F), m_disabled.Get());
            drawTextWith(L"Open Media Library Preferences", message, m_secondaryFormat.Get(),
                DWRITE_TEXT_ALIGNMENT_CENTER, m_foreground.Get());
        }

        m_renderTarget->FillRectangle(layout.albumHeader, m_surface.Get());
        if (m_albumSelection < m_albums.size()) {
            const auto& album = m_albums[m_albumSelection];
            drawTextWith(utf8ToWide(album.album.c_str()), D2D1::RectF(layout.albumHeader.left + 14.0F,
                8.0F, layout.albumHeader.right - 10.0F, 36.0F), m_titleFormat.Get(),
                DWRITE_TEXT_ALIGNMENT_LEADING, m_foreground.Get());
            drawTextWith(utf8ToWide(album.albumArtist.c_str()) + L"  ·  "
                    + std::to_wstring(album.sourceIndices.size()) + L" tracks",
                D2D1::RectF(layout.albumHeader.left + 14.0F,
                38.0F, layout.albumHeader.right - 10.0F, 64.0F), m_secondaryFormat.Get(),
                DWRITE_TEXT_ALIGNMENT_LEADING, m_disabled.Get());
        } else {
            drawTextWith(m_albumSourceItems.get_count() == 0 ? L"No albums in this source" : L"No album selected",
                layout.albumHeader, m_titleFormat.Get(), DWRITE_TEXT_ALIGNMENT_CENTER, m_disabled.Get());
        }
        m_renderTarget->FillRectangle(layout.albumTrackHeader, m_surface.Get());
        constexpr std::array<const wchar_t*, 4> labels{L"#", L"Title", L"Artist", L"Length"};
        constexpr std::array<float, 5> edges{0.0F, 0.10F, 0.55F, 0.84F, 1.0F};
        const auto width = layout.albumTrackHeader.right - layout.albumTrackHeader.left;
        for (std::size_t column = 0; column < labels.size(); ++column) {
            const auto rect = D2D1::RectF(layout.albumTrackHeader.left + edges[column] * width,
                layout.albumTrackHeader.top, layout.albumTrackHeader.left + edges[column + 1] * width,
                layout.albumTrackHeader.bottom);
            drawTextWith(labels[column], rect, m_secondaryFormat.Get(), column == 0 || column == 3
                ? DWRITE_TEXT_ALIGNMENT_CENTER : DWRITE_TEXT_ALIGNMENT_LEADING, m_foreground.Get());
        }
        if (m_albumSelection >= m_albums.size()) return;
        const auto& album = m_albums[m_albumSelection];
        const auto capacity = visibleRowCapacity(layout.albumTrackBody.bottom - layout.albumTrackBody.top,
            kCompactTrackRowHeight);
        const auto end = std::min(album.sourceIndices.size(), m_albumTrackTopRow + capacity + 1);
        m_renderTarget->PushAxisAlignedClip(layout.albumTrackBody, D2D1_ANTIALIAS_MODE_ALIASED);
        refreshPlayingLocation();
        for (auto row = m_albumTrackTopRow; row < end; ++row) {
            const auto source = album.sourceIndices[row];
            if (source >= m_albumSourceItems.get_count()) continue;
            const auto top = layout.albumTrackBody.top + static_cast<float>(row - m_albumTrackTopRow)
                * kCompactTrackRowHeight;
            const auto rect = D2D1::RectF(layout.albumTrackBody.left, top, layout.albumTrackBody.right,
                top + kCompactTrackRowHeight);
            if (row < m_albumTrackSelected.size() && m_albumTrackSelected[row]) {
                m_renderTarget->FillRectangle(rect, m_surface.Get());
            }
            if (row == m_albumTrackSelection && GetFocus() == get_wnd()) {
                m_renderTarget->DrawRectangle(D2D1::RectF(rect.left + 1.0F, rect.top + 1.0F,
                    rect.right - 1.0F, rect.bottom - 1.0F), m_foreground.Get(), 1.0F);
            }
            if (m_albumSourceItems[source] == m_target) m_renderTarget->FillRectangle(
                D2D1::RectF(rect.left, rect.top, rect.left + 3.0F, rect.bottom), m_accent.Get());
            const auto& meta = m_albumInput[source];
            std::wstring ordinal = L"—";
            if (meta.track != 0) {
                if (meta.disc != 0) {
                    wchar_t buffer[32]{};
                    swprintf_s(buffer, L"%u.%02u", meta.disc, meta.track);
                    ordinal = buffer;
                } else ordinal = std::to_wstring(meta.track);
            }
            const std::array<std::wstring, 4> values{
                ordinal, utf8ToWide(meta.title.c_str()),
                utf8ToWide(meta.artist.c_str()), formatQueueHandle(m_albumSourceItems[source], m_queueLengthScript)};
            for (std::size_t column = 0; column < values.size(); ++column) {
                const auto field = D2D1::RectF(rect.left + edges[column] * (rect.right - rect.left) + 5.0F,
                    rect.top, rect.left + edges[column + 1] * (rect.right - rect.left) - 5.0F, rect.bottom);
                drawTextWith(values[column], field, m_secondaryFormat.Get(), column == 0 || column == 3
                    ? DWRITE_TEXT_ALIGNMENT_CENTER : DWRITE_TEXT_ALIGNMENT_LEADING, m_foreground.Get());
            }
            m_renderTarget->DrawLine(D2D1::Point2F(rect.left, rect.bottom),
                D2D1::Point2F(rect.right, rect.bottom), m_surface.Get());
        }
        m_renderTarget->PopAxisAlignedClip();
        m_renderTarget->FillRectangle(layout.albumTrackScrollTrack, m_surface.Get());
        if (layout.albumTrackScrollThumb.bottom > layout.albumTrackScrollThumb.top) {
            m_renderTarget->FillRoundedRectangle(D2D1::RoundedRect(layout.albumTrackScrollThumb, 4.0F, 4.0F),
                m_disabled.Get());
        }
    }

    void drawPlaylistBrowser(const Layout& layout) {
        if (layout.playlistBrowser.right <= layout.playlistBrowser.left) return;
        const auto defaultGuid = readPlaylistBrowserSettings().defaultPlaylistGuid;
        m_renderTarget->FillRectangle(layout.playlistBrowser, m_background.Get());
        m_renderTarget->FillRectangle(layout.playlistBrowserHeader, m_surface.Get());
        m_renderTarget->DrawLine(D2D1::Point2F(layout.playlistBrowserHeader.left,
            layout.playlistBrowserHeader.bottom), D2D1::Point2F(layout.playlistBrowserHeader.right,
            layout.playlistBrowserHeader.bottom), m_disabled.Get(), 1.0F);
        drawTextWith(L"Playlists", D2D1::RectF(layout.playlistBrowserHeader.left + 12.0F,
            layout.playlistBrowserHeader.top, layout.playlistBrowserHeader.right - 12.0F,
            layout.playlistBrowserHeader.bottom), m_titleFormat.Get(),
            DWRITE_TEXT_ALIGNMENT_LEADING, m_foreground.Get());

        constexpr float rowHeight = 30.0F;
        const auto visible = visibleRows(m_playlistBrowserTopRow, m_playlistBrowserRows.size(),
            layout.playlistBrowserBody.bottom - layout.playlistBrowserBody.top, rowHeight, 0);
        m_renderTarget->PushAxisAlignedClip(layout.playlistBrowserBody, D2D1_ANTIALIAS_MODE_ALIASED);
        for (auto index = visible.first; index < visible.end; ++index) {
            const auto& row = m_playlistBrowserRows[index];
            const auto y = layout.playlistBrowserBody.top
                + static_cast<float>(index - m_playlistBrowserTopRow) * rowHeight;
            const auto rect = D2D1::RectF(layout.playlistBrowserBody.left, y,
                layout.playlistBrowserBody.right, y + rowHeight);
            const auto selected = std::any_of(m_playlistBrowserSelection.begin(),
                m_playlistBrowserSelection.end(), [&](const GUID& value) { return sameGuid(value, row.guid); });
            const auto isDefault = sameGuid(row.guid, defaultGuid);
            if (row.active) m_renderTarget->FillRectangle(rect, m_surface.Get());
            if (selected) m_renderTarget->DrawRectangle(D2D1::RectF(rect.left + 1.0F, rect.top + 1.0F,
                rect.right - 1.0F, rect.bottom - 1.0F), m_foreground.Get(), 1.0F);
            if (m_externalDropActive && m_externalDropDestination == DropDestination::playlistBrowser
                && !m_externalDropBrowserBlank && sameGuid(row.guid, m_externalDropBrowserGuid)) {
                m_renderTarget->DrawRectangle(D2D1::RectF(rect.left + 1.0F, rect.top + 1.0F,
                    rect.right - 1.0F, rect.bottom - 1.0F), m_accent.Get(), 2.0F);
            }
            if (row.playing) {
                drawTriangle(D2D1::Point2F(rect.left + 8.0F, y + 10.0F),
                    D2D1::Point2F(rect.left + 8.0F, y + 20.0F),
                    D2D1::Point2F(rect.left + 15.0F, y + 15.0F), m_accent.Get());
            }
            const auto type = row.kind == PlaylistBrowserKind::automatic
                ? (row.lockMask != 0 ? L"A·L" : L"A") : (row.lockMask != 0 ? L"L" : L"");
            if (*type) drawTextWith(type, D2D1::RectF(rect.left + 18.0F, y, rect.left + 36.0F,
                y + rowHeight), m_secondaryFormat.Get(), DWRITE_TEXT_ALIGNMENT_CENTER, m_disabled.Get());
            const auto textLeft = *type ? rect.left + 42.0F : rect.left + 20.0F;
            drawTextWith(row.name, D2D1::RectF(textLeft, y, rect.right - 48.0F, y + rowHeight),
                m_textFormat.Get(), DWRITE_TEXT_ALIGNMENT_LEADING,
                isDefault ? m_defaultPlaylist.Get() : (row.active ? m_accent.Get() : m_foreground.Get()));
            drawTextWith(std::to_wstring(row.itemCount), D2D1::RectF(rect.right - 44.0F, y,
                rect.right - 8.0F, y + rowHeight), m_secondaryFormat.Get(),
                DWRITE_TEXT_ALIGNMENT_TRAILING, isDefault ? m_defaultPlaylist.Get() : m_disabled.Get());
        }
        if (m_playlistBrowserRows.empty() && !m_playlistFilterText.empty()) {
            drawTextWith(L"No matching playlists", layout.playlistBrowserBody,
                m_secondaryFormat.Get(), DWRITE_TEXT_ALIGNMENT_CENTER, m_disabled.Get());
        }
        if (m_playlistBrowserDragging && m_playlistBrowserInsertion >= m_playlistBrowserTopRow) {
            const auto y = layout.playlistBrowserBody.top
                + static_cast<float>(m_playlistBrowserInsertion - m_playlistBrowserTopRow) * rowHeight;
            if (y >= layout.playlistBrowserBody.top && y <= layout.playlistBrowserBody.bottom) {
                m_renderTarget->DrawLine(D2D1::Point2F(layout.playlistBrowserBody.left, y),
                    D2D1::Point2F(layout.playlistBrowserBody.right, y), m_accent.Get(), 2.0F);
            }
        }
        m_renderTarget->PopAxisAlignedClip();
        if (m_externalDropActive && m_externalDropDestination == DropDestination::playlistBrowser
            && m_externalDropBrowserBlank) {
            m_renderTarget->DrawRectangle(D2D1::RectF(layout.playlistBrowserBody.left + 1.0F,
                layout.playlistBrowserBody.top + 1.0F, layout.playlistBrowserBody.right - 1.0F,
                layout.playlistBrowserBody.bottom - 1.0F), m_accent.Get(), 2.0F);
        }
        m_renderTarget->FillRectangle(layout.playlistBrowserDivider,
            m_active == ControlId::playlistBrowserDivider || m_hover == ControlId::playlistBrowserDivider
                ? m_accent.Get() : m_surface.Get());
        m_renderTarget->DrawLine(D2D1::Point2F((layout.playlistBrowserDivider.left
                + layout.playlistBrowserDivider.right) * 0.5F, layout.playlistBrowserDivider.top),
            D2D1::Point2F((layout.playlistBrowserDivider.left + layout.playlistBrowserDivider.right) * 0.5F,
                layout.playlistBrowserDivider.bottom), m_disabled.Get(), 1.0F);
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
                if (definition.id == "builtin.state") {
                    text = compactQueuePositions(queuePositionsForTrack(index));
                    if (text.empty() && playing) text = L"\x25B6";
                } else if (definition.id == "builtin.rating") {
                    const auto rating = m_playlistRatingHover && m_playlistRatingHover->first == index
                        ? m_playlistRatingHover->second : std::clamp(_wtoi(text.c_str()), 0, 5);
                    text.assign(static_cast<std::size_t>(rating), L'\x2605');
                    text.append(static_cast<std::size_t>(5 - rating), L'\x2606');
                }
                drawTextWith(text, rect, m_secondaryFormat.Get(),
                    alignment(definition.alignment), textBrush);
            }
            m_renderTarget->DrawLine(D2D1::Point2F(rowRect.left, rowRect.bottom),
                D2D1::Point2F(rowRect.right, rowRect.bottom), m_surface.Get(), 1.0F);
        }
        if ((m_playlistDragging || (m_externalDropActive
                && m_externalDropDestination == DropDestination::playlist))
            && (m_playlistDragging ? m_playlistInsertion : m_externalDropInsertion) <= m_playlistItems.get_count()) {
            const auto displayBoundary = m_playlistDragging
                ? m_playlistDropDisplayBoundary : m_externalDropDisplayBoundary;
            if (displayBoundary >= m_playlistTopRow && displayBoundary <= m_playlistDisplayRows.size()) {
                const auto lineY = layout.playlistBody.top
                    + static_cast<float>(displayBoundary - m_playlistTopRow) * rowHeight;
                if (lineY >= layout.playlistBody.top && lineY <= layout.playlistBody.bottom) {
                    m_renderTarget->DrawLine(D2D1::Point2F(layout.playlistBody.left, lineY),
                        D2D1::Point2F(layout.playlistBody.right, lineY), m_accent.Get(), 2.0F);
                }
            }
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

    void drawPlaybackQueue(const Layout& layout) {
        if (layout.queuePanel.right <= layout.queuePanel.left) return;
        m_renderTarget->FillRectangle(layout.queuePanel, m_background.Get());
        m_renderTarget->DrawLine(D2D1::Point2F(layout.queuePanel.left, layout.queuePanel.top),
            D2D1::Point2F(layout.queuePanel.left, layout.queuePanel.bottom), m_disabled.Get(), 1.0F);
        if (m_externalDropActive && m_externalDropDestination == DropDestination::queue) {
            m_renderTarget->DrawRectangle(D2D1::RectF(layout.queuePanel.left + 1.0F, layout.queuePanel.top + 1.0F,
                layout.queuePanel.right - 1.0F, layout.queuePanel.bottom - 1.0F), m_accent.Get(), 2.0F);
        }
        m_renderTarget->FillRectangle(layout.queueHeader, m_surface.Get());
        const auto title = L"Playback Queue  (" + std::to_wstring(m_queueItems.size()) + L")";
        drawTextWith(title, D2D1::RectF(layout.queueHeader.left + 10.0F, layout.queueHeader.top,
            layout.queueHeader.right - 10.0F, layout.queueHeader.bottom), m_titleFormat.Get(),
            DWRITE_TEXT_ALIGNMENT_LEADING, m_foreground.Get());

        m_renderTarget->FillRectangle(layout.queueListHeader, m_surface.Get());
        const auto width = layout.queueBody.right - layout.queueBody.left;
        const auto compactColumns = compactTrackColumns();
        const std::array<const wchar_t*, 5> labels{L"#", L"Title", L"Artist", L"Source", L"Length"};
        for (std::size_t column = 0; column < labels.size(); ++column) {
            const auto left = compactTrackColumnX(layout.queueBody.left, width, compactColumns[column].leftFraction);
            const auto right = compactTrackColumnX(layout.queueBody.left, width, compactColumns[column].rightFraction);
            drawTextWith(labels[column], D2D1::RectF(left + 5.0F, layout.queueListHeader.top,
                right - 5.0F, layout.queueListHeader.bottom), m_secondaryFormat.Get(),
                compactColumns[column].centered ? DWRITE_TEXT_ALIGNMENT_CENTER : DWRITE_TEXT_ALIGNMENT_LEADING,
                m_foreground.Get());
        }
        m_renderTarget->DrawLine(D2D1::Point2F(layout.queueListHeader.left, layout.queueListHeader.bottom),
            D2D1::Point2F(layout.queueListHeader.right, layout.queueListHeader.bottom), m_disabled.Get(), 1.0F);

        if (m_queueItems.empty()) {
            drawTextWith(L"Playback queue is empty", layout.queueBody, m_secondaryFormat.Get(),
                DWRITE_TEXT_ALIGNMENT_CENTER, m_disabled.Get());
            return;
        }
        constexpr float rowHeight = kCompactTrackRowHeight;
        const auto visible = visibleRows(m_queueTopRow, m_queueItems.size(),
            layout.queueBody.bottom - layout.queueBody.top, rowHeight, 0);
        m_renderTarget->PushAxisAlignedClip(layout.queueBody, D2D1_ANTIALIAS_MODE_ALIASED);
        for (auto index = visible.first; index < visible.end; ++index) {
            const auto y = layout.queueBody.top + static_cast<float>(index - m_queueTopRow) * rowHeight;
            const auto rect = D2D1::RectF(layout.queueBody.left, y, layout.queueBody.right, y + rowHeight);
            if (index < m_queueSelected.size() && m_queueSelected[index]) m_renderTarget->FillRectangle(rect, m_surface.Get());
            if (index == m_queueFocus) m_renderTarget->DrawRectangle(
                D2D1::RectF(rect.left + 1.0F, rect.top + 1.0F, rect.right - 1.0F, rect.bottom - 1.0F),
                m_foreground.Get(), 1.0F);
            const auto row = formatQueueRow(index);
            const std::array<std::wstring, 5> values{std::to_wstring(index + 1), row.title, row.artist, row.source, row.length};
            for (std::size_t column = 0; column < values.size(); ++column) {
                const auto left = compactTrackColumnX(layout.queueBody.left, width, compactColumns[column].leftFraction);
                const auto right = compactTrackColumnX(layout.queueBody.left, width, compactColumns[column].rightFraction);
                drawTextWith(values[column], D2D1::RectF(left + 5.0F, y, right - 5.0F, y + rowHeight),
                    m_secondaryFormat.Get(), compactColumns[column].centered
                        ? DWRITE_TEXT_ALIGNMENT_CENTER : DWRITE_TEXT_ALIGNMENT_LEADING, m_foreground.Get());
            }
            m_renderTarget->DrawLine(D2D1::Point2F(rect.left, rect.bottom),
                D2D1::Point2F(rect.right, rect.bottom), m_surface.Get(), 1.0F);
        }
        if (m_queueDragging && m_queueInsertion >= m_queueTopRow
            && m_queueInsertion <= m_queueItems.size()) {
            const auto lineY = layout.queueBody.top
                + static_cast<float>(m_queueInsertion - m_queueTopRow) * rowHeight;
            if (lineY >= layout.queueBody.top && lineY <= layout.queueBody.bottom) {
                m_renderTarget->DrawLine(D2D1::Point2F(layout.queueBody.left, lineY),
                    D2D1::Point2F(layout.queueBody.right, lineY), m_accent.Get(), 2.0F);
            }
        }
        m_renderTarget->PopAxisAlignedClip();
        const auto capacity = visibleRowCapacity(layout.queueBody.bottom - layout.queueBody.top, rowHeight);
        if (m_queueItems.size() > capacity) {
            m_renderTarget->FillRectangle(layout.queueScrollTrack, m_surface.Get());
            m_renderTarget->FillRoundedRectangle(D2D1::RoundedRect(layout.queueScrollThumb, 4.0F, 4.0F),
                m_active == ControlId::queueScrollbar ? m_accent.Get() : m_disabled.Get());
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

    void executePlaylistRating(std::size_t track, int clickedRating) {
        if (track >= m_playlistItems.get_count() || clickedRating < 1 || clickedRating > 5) return;
        const auto target = m_playlistItems[track];
        if (target.is_empty()) return;
        try {
            const auto currentText = formatPlaylistItemUtf8(track, m_ratingScript);
            const auto current = std::clamp(std::atoi(currentText.c_str()), 0, 5);
            const auto value = ratingCommandValue(current, clickedRating);
            const auto commandName = value == 0
                ? std::string("Playback Statistics/Rating/<not set>")
                : std::string("Playback Statistics/Rating/") + std::to_string(value);
            const auto manager = createRatingMenu(target);
            if (manager.is_empty()) throw std::runtime_error("rating menu unavailable");
            if (auto* command = findContextCommand(manager->get_root(), commandName.c_str())) {
                command->execute();
                m_playlistTextCache.erase(track);
                requestRefresh();
                invalidate();
            } else {
                throw std::runtime_error("rating command unavailable");
            }
        } catch (...) {
            showStatus(L"Playback Statistics could not update this rating.");
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
                    m_groupArtworkAborterAlbum = request.album;
                }
                auto pixels = loadGroupArtwork(request.items, request.artworkId, *aborter);
                {
                    std::scoped_lock lock(m_groupArtworkMutex);
                    if (m_groupArtworkAborter == aborter) {
                        m_groupArtworkAborter.reset();
                        m_groupArtworkAborterAlbum = false;
                    }
                }
                if (pixels.status == ArtworkStatus::aborted || stop.stop_requested()) continue;
                auto* completion = new GroupArtworkCompletion{
                    request.generation, std::move(request.key), std::move(pixels), request.album};
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
        if (completion && completion->album) {
            applyAlbumArtworkCompletion(*completion);
            return;
        }
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

    void applyAlbumArtworkCompletion(GroupArtworkCompletion& completion) {
        if (completion.generation != m_albumArtworkGeneration) return;
        const auto found = m_albumArtworkCache.find(completion.key);
        if (found == m_albumArtworkCache.end()) return;
        m_albumArtworkBytes -= std::min(m_albumArtworkBytes, found->second.pixels.bgra.size());
        constexpr std::size_t maximumBytes = 64U * 1024U * 1024U;
        const auto incoming = completion.pixels.bgra.size();
        while (m_albumArtworkBytes + incoming > maximumBytes) {
            const auto victim = std::min_element(m_albumArtworkCache.begin(), m_albumArtworkCache.end(),
                [&](const auto& left, const auto& right) {
                    const auto leftEligible = left.first != completion.key && !left.second.loading;
                    const auto rightEligible = right.first != completion.key && !right.second.loading;
                    if (leftEligible != rightEligible) return leftEligible;
                    if (!leftEligible) return false;
                    return left.second.lastUse < right.second.lastUse;
                });
            if (victim == m_albumArtworkCache.end()
                || victim->first == completion.key || victim->second.loading) break;
            m_albumArtworkBytes -= std::min(m_albumArtworkBytes, victim->second.pixels.bgra.size());
            m_albumArtworkCache.erase(victim);
        }
        found->second.loading = false;
        if (m_albumArtworkBytes + incoming <= maximumBytes) {
            found->second.pixels = std::move(completion.pixels);
            m_albumArtworkBytes += incoming;
        } else {
            found->second.pixels = {ArtworkStatus::unavailable};
        }
        found->second.bitmap.Reset();
        invalidate();
    }

    void startAlbumArtworkWorker() {
        if (m_albumArtworkThread.joinable()) return;
        m_albumArtworkThread = std::jthread([this](std::stop_token stop) {
            while (!stop.stop_requested()) {
                GroupArtworkRequest request;
                std::shared_ptr<abort_callback_impl> aborter;
                {
                    std::unique_lock lock(m_albumArtworkMutex);
                    m_albumArtworkCondition.wait(lock, [&] {
                        return stop.stop_requested() || !m_albumArtworkRequests.empty();
                    });
                    if (stop.stop_requested()) break;
                    request = std::move(m_albumArtworkRequests.front());
                    m_albumArtworkRequests.erase(m_albumArtworkRequests.begin());
                    aborter = std::make_shared<abort_callback_impl>();
                    m_albumArtworkAborter = aborter;
                }
                // Album Grid cells are thumbnails. Decoding them at Now Playing size would
                // exhaust the bounded cache after only a few covers and cause visible reload loops.
                auto pixels = loadGroupArtwork(request.items, request.artworkId, *aborter, 320U);
                {
                    std::scoped_lock lock(m_albumArtworkMutex);
                    if (m_albumArtworkAborter == aborter) m_albumArtworkAborter.reset();
                }
                if (pixels.status == ArtworkStatus::aborted || stop.stop_requested()) continue;
                auto* completion = new GroupArtworkCompletion{
                    request.generation, std::move(request.key), std::move(pixels), true};
                const auto wnd = get_wnd();
                if (!wnd || !PostMessage(wnd, kGroupArtworkReadyMessage, 0,
                        reinterpret_cast<LPARAM>(completion))) delete completion;
            }
        });
    }

    void stopAlbumArtworkWorker() noexcept {
        if (!m_albumArtworkThread.joinable()) return;
        m_albumArtworkThread.request_stop();
        {
            std::scoped_lock lock(m_albumArtworkMutex);
            if (m_albumArtworkAborter) m_albumArtworkAborter->abort();
            m_albumArtworkRequests.clear();
        }
        m_albumArtworkCondition.notify_all();
        m_albumArtworkThread.join();
    }

    GroupArtworkCacheEntry& requestAlbumArtwork(std::size_t albumIndex) {
        const auto& album = m_albums[albumIndex];
        if (const auto found = m_albumArtworkCache.find(album.key); found != m_albumArtworkCache.end()) {
            found->second.lastUse = ++m_albumArtworkUseCounter;
            return found->second;
        }
        while (m_albumArtworkCache.size() >= 160) {
            const auto victim = std::min_element(m_albumArtworkCache.begin(), m_albumArtworkCache.end(),
                [](const auto& left, const auto& right) {
                    if (left.second.loading != right.second.loading) return !left.second.loading;
                    if (left.second.loading) return false;
                    return left.second.lastUse < right.second.lastUse;
                });
            if (victim == m_albumArtworkCache.end() || victim->second.loading) break;
            m_albumArtworkBytes -= std::min(m_albumArtworkBytes, victim->second.pixels.bgra.size());
            m_albumArtworkCache.erase(victim);
        }
        auto [entry, inserted] = m_albumArtworkCache.emplace(album.key, GroupArtworkCacheEntry{});
        (void)inserted;
        entry->second.lastUse = ++m_albumArtworkUseCounter;
        GroupArtworkRequest request;
        request.generation = m_albumArtworkGeneration;
        request.key = album.key;
        request.artworkId = album_art_ids::cover_front;
        request.album = true;
        for (const auto source : album.sourceIndices) {
            if (source < m_albumSourceItems.get_count()) request.items.add_item(m_albumSourceItems[source]);
        }
        entry->second.loading = true;
        {
            std::scoped_lock lock(m_albumArtworkMutex);
            m_albumArtworkRequests.push_back(std::move(request));
        }
        m_albumArtworkCondition.notify_one();
        return entry->second;
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
        const auto selectedWorkspace = (id == ControlId::playlistWorkspace && m_workspace == Workspace::playlist)
            || (id == ControlId::albumWorkspace && m_workspace == Workspace::album);
        const auto selectedToggle = id == ControlId::queueToggle && m_queueMode;
        auto fill = primary ? m_accent.Get() : m_surface.Get();
        if (pressed) {
            fill = m_disabled.Get();
        } else if (hot && !primary) {
            fill = m_light.Get();
        }
        if (selectedWorkspace || selectedToggle) {
            m_renderTarget->FillRoundedRectangle(D2D1::RoundedRect(rect, 4.0F, 4.0F), m_disabled.Get());
        } else if (primary || hot || pressed) {
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
        case ControlId::open:
            m_renderTarget->DrawLine(D2D1::Point2F(cx - 9.0F, cy + 8.0F),
                D2D1::Point2F(cx + 9.0F, cy + 8.0F), brush, 1.8F);
            m_renderTarget->DrawLine(D2D1::Point2F(cx - 9.0F, cy + 8.0F),
                D2D1::Point2F(cx - 9.0F, cy + 3.0F), brush, 1.8F);
            m_renderTarget->DrawLine(D2D1::Point2F(cx + 9.0F, cy + 8.0F),
                D2D1::Point2F(cx + 9.0F, cy + 3.0F), brush, 1.8F);
            drawTriangle(D2D1::Point2F(cx, cy - 10.0F), D2D1::Point2F(cx - 6.0F, cy - 2.0F),
                D2D1::Point2F(cx + 6.0F, cy - 2.0F), brush);
            m_renderTarget->DrawLine(D2D1::Point2F(cx, cy - 2.0F),
                D2D1::Point2F(cx, cy + 4.0F), brush, 1.8F);
            break;
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
        case ControlId::playlistWorkspace:
            for (int row = -1; row <= 1; ++row) {
                const auto y = cy + static_cast<float>(row) * 6.0F;
                m_renderTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx - 8.0F, y), 1.3F, 1.3F), brush);
                m_renderTarget->DrawLine(D2D1::Point2F(cx - 4.0F, y), D2D1::Point2F(cx + 9.0F, y), brush, 1.6F);
            }
            break;
        case ControlId::albumWorkspace:
            m_renderTarget->DrawRectangle(D2D1::RectF(cx - 10.0F, cy - 9.0F, cx - 1.0F, cy), brush, 1.5F);
            m_renderTarget->DrawRectangle(D2D1::RectF(cx + 2.0F, cy - 9.0F, cx + 11.0F, cy), brush, 1.5F);
            m_renderTarget->DrawRectangle(D2D1::RectF(cx - 10.0F, cy + 3.0F, cx - 1.0F, cy + 12.0F), brush, 1.5F);
            m_renderTarget->DrawRectangle(D2D1::RectF(cx + 2.0F, cy + 3.0F, cx + 11.0F, cy + 12.0F), brush, 1.5F);
            break;
        case ControlId::albumList:
            m_renderTarget->DrawRectangle(D2D1::RectF(cx - 10.0F, cy - 10.0F, cx + 10.0F, cy + 10.0F), brush, 1.4F);
            m_renderTarget->DrawLine(D2D1::Point2F(cx - 6.0F, cy - 5.0F),
                D2D1::Point2F(cx + 6.0F, cy - 5.0F), brush, 1.5F);
            m_renderTarget->DrawLine(D2D1::Point2F(cx - 6.0F, cy),
                D2D1::Point2F(cx + 6.0F, cy), brush, 1.5F);
            m_renderTarget->DrawLine(D2D1::Point2F(cx - 6.0F, cy + 5.0F),
                D2D1::Point2F(cx + 6.0F, cy + 5.0F), brush, 1.5F);
            break;
        case ControlId::queueToggle: {
            const auto iconBrush = m_queueMode ? m_accent.Get() : brush;
            for (int row = -1; row <= 1; ++row) {
                const auto y = cy + static_cast<float>(row) * 6.0F;
                m_renderTarget->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx - 8.0F, y), 1.5F, 1.5F), iconBrush);
                m_renderTarget->DrawLine(D2D1::Point2F(cx - 4.0F, y), D2D1::Point2F(cx + 9.0F, y), iconBrush, 1.7F);
            }
            break;
        }
        case ControlId::playbackOrder:
            m_renderTarget->DrawLine(D2D1::Point2F(cx - 8.0F, cy - 6.0F),
                D2D1::Point2F(cx + 7.0F, cy - 6.0F), brush, 1.8F);
            drawTriangle(D2D1::Point2F(cx + 10.0F, cy - 6.0F),
                D2D1::Point2F(cx + 5.0F, cy - 10.0F), D2D1::Point2F(cx + 5.0F, cy - 2.0F), brush);
            m_renderTarget->DrawLine(D2D1::Point2F(cx + 8.0F, cy + 6.0F),
                D2D1::Point2F(cx - 7.0F, cy + 6.0F), brush, 1.8F);
            drawTriangle(D2D1::Point2F(cx - 10.0F, cy + 6.0F),
                D2D1::Point2F(cx - 5.0F, cy + 2.0F), D2D1::Point2F(cx - 5.0F, cy + 10.0F), brush);
            break;
        case ControlId::outputDevice:
            m_renderTarget->DrawRoundedRectangle(D2D1::RoundedRect(
                D2D1::RectF(cx - 10.0F, cy - 8.0F, cx + 10.0F, cy + 6.0F), 2.0F, 2.0F), brush, 1.7F);
            m_renderTarget->DrawLine(D2D1::Point2F(cx, cy + 6.0F),
                D2D1::Point2F(cx, cy + 10.0F), brush, 1.7F);
            m_renderTarget->DrawLine(D2D1::Point2F(cx - 5.0F, cy + 10.0F),
                D2D1::Point2F(cx + 5.0F, cy + 10.0F), brush, 1.7F);
            break;
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

    [[nodiscard]] std::optional<std::size_t> queueRowAt(HWND wnd, D2D1_POINT_2F point) const {
        const auto body = calculateLayout(wnd).queueBody;
        if (!m_queueMode || !contains(body, point)) return std::nullopt;
        const auto index = m_queueTopRow + static_cast<std::size_t>((point.y - body.top) / kCompactTrackRowHeight);
        return index < m_queueItems.size() ? std::optional<std::size_t>{index} : std::nullopt;
    }

    void selectQueueRow(std::size_t index, bool ctrl, bool shift) {
        if (index >= m_queueItems.size()) return;
        m_queueSelected.resize(m_queueItems.size(), false);
        const auto anchor = m_queueAnchor < m_queueItems.size() ? m_queueAnchor : index;
        if (shift) {
            if (!ctrl) std::fill(m_queueSelected.begin(), m_queueSelected.end(), false);
            const auto [first, last] = inclusiveRange(anchor, index);
            for (auto current = first; current <= last; ++current) m_queueSelected[current] = true;
        } else if (ctrl) {
            m_queueSelected[index] = !m_queueSelected[index];
            m_queueAnchor = index;
        } else {
            std::fill(m_queueSelected.begin(), m_queueSelected.end(), false);
            m_queueSelected[index] = true;
            m_queueAnchor = index;
        }
        m_queueFocus = index;
        invalidate();
    }

    void updateQueueScrollbar(HWND wnd, float pointerY) {
        const auto layout = calculateLayout(wnd);
        const auto capacity = visibleRowCapacity(layout.queueBody.bottom - layout.queueBody.top, kCompactTrackRowHeight);
        const auto maximum = maximumTopRow(m_queueItems.size(), capacity);
        const auto trackHeight = layout.queueScrollTrack.bottom - layout.queueScrollTrack.top;
        const auto thumbHeight = layout.queueScrollThumb.bottom - layout.queueScrollThumb.top;
        m_queueTopRow = topRowFromScrollbar(pointerY, m_queueScrollbarGrabOffset,
            layout.queueScrollTrack.top, trackHeight, thumbHeight, maximum);
        invalidate();
    }

    void updateAlbumGridScrollbar(HWND wnd, float pointerY) {
        const auto layout = calculateLayout(wnd);
        const auto columns = albumGridColumns(layout);
        const auto rows = (m_albums.size() + columns - 1) / columns;
        const auto visible = std::max<std::size_t>(1, static_cast<std::size_t>(
            (layout.albumGrid.bottom - layout.albumGrid.top) / 184.0F));
        const auto maximum = rows > visible ? rows - visible : 0;
        const auto trackHeight = layout.albumGridScrollTrack.bottom - layout.albumGridScrollTrack.top;
        const auto thumbHeight = layout.albumGridScrollThumb.bottom - layout.albumGridScrollThumb.top;
        m_albumGridTopRow = topRowFromScrollbar(pointerY, m_albumGridScrollbarGrabOffset,
            layout.albumGridScrollTrack.top, trackHeight, thumbHeight, maximum);
        invalidate();
    }

    void updateAlbumTrackScrollbar(HWND wnd, float pointerY) {
        const auto layout = calculateLayout(wnd);
        const auto count = m_albumSelection < m_albums.size()
            ? m_albums[m_albumSelection].sourceIndices.size() : 0;
        const auto capacity = visibleRowCapacity(layout.albumTrackBody.bottom - layout.albumTrackBody.top,
            kCompactTrackRowHeight);
        const auto maximum = maximumTopRow(count, capacity);
        const auto trackHeight = layout.albumTrackScrollTrack.bottom - layout.albumTrackScrollTrack.top;
        const auto thumbHeight = layout.albumTrackScrollThumb.bottom - layout.albumTrackScrollThumb.top;
        m_albumTrackTopRow = topRowFromScrollbar(pointerY, m_albumTrackScrollbarGrabOffset,
            layout.albumTrackScrollTrack.top, trackHeight, thumbHeight, maximum);
        invalidate();
    }

    void updateQueueDrag(HWND wnd, D2D1_POINT_2F point) {
        if (!m_queueDragCandidate) return;
        const auto dx = point.x - m_dragStartPoint.x;
        const auto dy = point.y - m_dragStartPoint.y;
        if (!m_queueDragging && std::hypot(dx, dy) < 4.0F) return;
        m_queueDragging = true;
        const auto body = calculateLayout(wnd).queueBody;
        if (point.y <= body.top) m_queueInsertion = m_queueTopRow;
        else if (point.y >= body.bottom) m_queueInsertion = m_queueItems.size();
        else {
            const auto relative = point.y - body.top;
            const auto row = m_queueTopRow + static_cast<std::size_t>(relative / kCompactTrackRowHeight);
            m_queueInsertion = insertionBoundaryForRow(std::min(row, m_queueItems.size()),
                std::fmod(relative, kCompactTrackRowHeight) >= kCompactTrackRowHeight * 0.5F, m_queueItems.size());
        }
        invalidate();
    }

    void commitQueueDrag() {
        if (!m_queueDragging || !m_queueDragCandidate || m_queueDragSnapshot.empty()) return;
        std::vector<std::size_t> selected;
        for (std::size_t index = 0; index < m_queueDragSelection.size(); ++index) {
            if (m_queueDragSelection[index]) selected.push_back(index);
        }
        const auto plan = planPlaylistMove(m_queueDragSnapshot.size(), selected, m_queueInsertion);
        if (plan.changed) rebuildQueueInOrder(plan.order, m_queueDragSnapshot);
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
        const auto priorAlbumHover = m_albumHover;
        m_albumHover = m_workspace == Workspace::album ? albumAt(calculateLayout(wnd), point) : std::nullopt;
        if (priorAlbumHover != m_albumHover && m_hover == ControlId::albumGrid) updateTooltip(ControlId::albumGrid);
        const auto priorRatingHover = m_playlistRatingHover;
        m_playlistRatingHover = playlistRatingAt(wnd, point);
        if (priorRatingHover != m_playlistRatingHover) invalidate();
        const auto hit = hitTest(wnd, point);
        setHover(hit);
        if (m_active == ControlId::seek && m_previewing) {
            updateSeekPreview(wnd, point.x);
        } else if (m_active == ControlId::volume && !m_state.customVolume) {
            updateVolume(wnd, point.x);
        } else if (m_active == ControlId::rightDivider) {
            updateRightDivider(wnd, point.y);
        } else if (m_active == ControlId::albumDivider) {
            RECT client{};
            GetClientRect(wnd, &client);
            const auto width = std::max(1.0F, static_cast<float>(client.right - client.left) / dpiScale());
            m_albumSplitPermille = std::clamp(static_cast<int>(std::lround(point.x / width * 1000.0F)), 350, 800);
            clampAlbumScroll();
            invalidate();
        } else if (m_active == ControlId::playlistBrowserDivider) {
            updatePlaylistBrowserDivider(wnd, point.x);
        } else if (m_active == ControlId::playlistBrowser) {
            updatePlaylistBrowserDrag(wnd, point);
        } else if (m_active == ControlId::albumGridScrollbar) {
            updateAlbumGridScrollbar(wnd, point.y);
        } else if (m_active == ControlId::albumTrackScrollbar) {
            updateAlbumTrackScrollbar(wnd, point.y);
        } else if (m_active == ControlId::playlistScrollbar) {
            updatePlaylistScrollbar(wnd, point.y);
        } else if (m_active == ControlId::playlistHeader) {
            updatePlaylistHeaderDrag(wnd, point.x);
        } else if (m_active == ControlId::playlistBody) {
            updatePlaylistDrag(wnd, point);
        } else if (m_active == ControlId::queueScrollbar) {
            updateQueueScrollbar(wnd, point.y);
        } else if (m_active == ControlId::queueBody) {
            updateQueueDrag(wnd, point);
        }
    }

    void onLeftButtonDown(HWND wnd, LPARAM lp) {
        SetFocus(wnd);
        const auto point = pointFromLParam(lp, dpiScale());
        const auto hit = hitTest(wnd, point);
        if (m_workspace == Workspace::album && hit == ControlId::albumGrid) {
            if (const auto album = albumAt(calculateLayout(wnd), point)) {
                m_albumSelection = *album;
                resetAlbumTrackSelection();
                persistAlbumBrowserState();
                m_focus = ControlId::albumGrid;
                invalidate();
            }
            return;
        }
        if (m_workspace == Workspace::album && hit == ControlId::albumTrackList) {
            if (const auto row = albumTrackAt(calculateLayout(wnd), point)) {
                selectAlbumTrack(*row, (GetKeyState(VK_CONTROL) & 0x8000) != 0,
                    (GetKeyState(VK_SHIFT) & 0x8000) != 0);
                m_focus = ControlId::albumTrackList;
                invalidate();
            }
            return;
        }
        if (m_workspace == Workspace::playlist && hit == ControlId::playlistBrowser) {
            if (const auto row = playlistBrowserRowAt(calculateLayout(wnd), point)) {
                const auto ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
                const auto shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                selectPlaylistBrowserRow(*row, ctrl, shift, !ctrl && !shift);
                if (m_playlistFilterText.empty()) {
                    m_playlistBrowserDragCandidate = *row;
                    m_playlistBrowserDragStart = point;
                    m_playlistBrowserDragSnapshot.clear();
                    for (const auto& value : m_playlistBrowserRows) m_playlistBrowserDragSnapshot.push_back(value.guid);
                    m_active = ControlId::playlistBrowser;
                    SetCapture(wnd);
                }
            } else if ((GetKeyState(VK_CONTROL) & 0x8000) == 0
                && (GetKeyState(VK_SHIFT) & 0x8000) == 0) {
                m_playlistBrowserSelection.clear();
                invalidate();
            }
            m_focus = ControlId::playlistBrowser;
            return;
        }
        if (const auto rating = playlistRatingAt(wnd, point)) {
            m_playlistRatingPress = rating;
            m_active = ControlId::playlistRating;
            m_focus = ControlId::playlistBody;
            SetCapture(wnd);
            invalidate();
            return;
        }
        if (hit == ControlId::queueBody) {
            const auto ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            const auto shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            if (const auto row = queueRowAt(wnd, point)) {
                if (*row >= m_queueSelected.size() || !m_queueSelected[*row]) selectQueueRow(*row, ctrl, shift);
                m_queueDragCandidate = *row;
                m_queueDragSnapshot = m_queueItems;
                m_queueDragSelection = m_queueSelected;
                m_dragStartPoint = point;
                m_queueInsertion = *row;
                m_active = ControlId::queueBody;
                m_focus = ControlId::queueBody;
                SetCapture(wnd);
            } else if (!ctrl && !shift) {
                std::fill(m_queueSelected.begin(), m_queueSelected.end(), false);
                invalidate();
            }
            return;
        }
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
        } else if (hit == ControlId::playlistBrowserDivider) {
            updatePlaylistBrowserDivider(wnd, point.x);
        } else if (hit == ControlId::albumGridScrollbar) {
            const auto layout = calculateLayout(wnd);
            if (contains(layout.albumGridScrollThumb, point)) {
                m_albumGridScrollbarGrabOffset = point.y - layout.albumGridScrollThumb.top;
            } else {
                m_albumGridScrollbarGrabOffset = (layout.albumGridScrollThumb.bottom
                    - layout.albumGridScrollThumb.top) * 0.5F;
                updateAlbumGridScrollbar(wnd, point.y);
            }
        } else if (hit == ControlId::albumTrackScrollbar) {
            const auto layout = calculateLayout(wnd);
            if (contains(layout.albumTrackScrollThumb, point)) {
                m_albumTrackScrollbarGrabOffset = point.y - layout.albumTrackScrollThumb.top;
            } else {
                m_albumTrackScrollbarGrabOffset = (layout.albumTrackScrollThumb.bottom
                    - layout.albumTrackScrollThumb.top) * 0.5F;
                updateAlbumTrackScrollbar(wnd, point.y);
            }
        } else if (hit == ControlId::playlistScrollbar) {
            const auto layout = calculateLayout(wnd);
            if (contains(layout.playlistScrollThumb, point)) {
                m_playlistScrollbarGrabOffset = point.y - layout.playlistScrollThumb.top;
            } else {
                m_playlistScrollbarGrabOffset = (layout.playlistScrollThumb.bottom
                    - layout.playlistScrollThumb.top) * 0.5F;
                updatePlaylistScrollbar(wnd, point.y);
            }
        } else if (hit == ControlId::queueScrollbar) {
            const auto layout = calculateLayout(wnd);
            if (contains(layout.queueScrollThumb, point)) {
                m_queueScrollbarGrabOffset = point.y - layout.queueScrollThumb.top;
            } else {
                m_queueScrollbarGrabOffset = (layout.queueScrollThumb.bottom - layout.queueScrollThumb.top) * 0.5F;
                updateQueueScrollbar(wnd, point.y);
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
        const auto commitQueue = active == ControlId::queueBody && m_queueDragging;
        const auto commitPlaylist = active == ControlId::playlistBody && m_playlistDragging;
        const auto commitBrowser = active == ControlId::playlistBrowser && m_playlistBrowserDragging;
        const auto ratingRelease = active == ControlId::playlistRating ? playlistRatingAt(wnd, point) : std::nullopt;
        m_active = ControlId::none;
        if (active == ControlId::seek) {
            m_previewing = false;
        }
        releaseCapturePreservingInteraction(wnd);
        if (active == ControlId::seek) {
            if (seekTarget) {
                playback_control::get()->playback_seek(*seekTarget);
            }
        } else if (active == ControlId::playlistRating) {
            if (m_playlistRatingPress && ratingRelease && *m_playlistRatingPress == *ratingRelease) {
                executePlaylistRating(ratingRelease->first, ratingRelease->second);
            }
            m_playlistRatingPress.reset();
        } else if (active == ControlId::rightDivider) {
            auto settings = readSettings();
            settings.rightHeaderPermille = committedHeaderPermille;
            m_dragHeaderPermille = -1;
            writeSettings(settings);
            syncLyricsHost(wnd);
        } else if (active == ControlId::albumDivider) {
            persistAlbumBrowserState();
        } else if (active == ControlId::playlistBrowserDivider) {
            auto settings = readPlaylistBrowserSettings();
            settings.splitPermille = m_playlistBrowserSplitPermille;
            writePlaylistBrowserSettings(settings);
        } else if (active == ControlId::playlistBrowser) {
            if (commitBrowser) commitPlaylistBrowserDrag();
            m_playlistBrowserDragCandidate.reset();
            m_playlistBrowserDragging = false;
            m_playlistBrowserDragSnapshot.clear();
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
        } else if (active == ControlId::queueBody) {
            if (commitQueue) commitQueueDrag();
            m_queueDragCandidate.reset();
            m_queueDragging = false;
            m_queueDragSnapshot.clear();
            m_queueDragSelection.clear();
        } else if (active == ControlId::playlistBody) {
            if (commitPlaylist) commitPlaylistDrag();
            else if (m_playlistCollapseSelectionOnClick && m_playlistDragCandidate) {
                selectPlaylistRow(*m_playlistDragCandidate, false, false);
            }
            KillTimer(wnd, kPlaylistDragTimer);
            m_playlistDragCandidate.reset();
            m_playlistDragging = false;
            m_playlistDragSnapshot.remove_all();
            m_playlistDragSelection.clear();
            m_playlistCollapseSelectionOnClick = false;
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
        if (m_workspace == Workspace::album && contains(layout.albumGrid, point)) {
            const auto columns = albumGridColumns(layout);
            const auto rows = (m_albums.size() + columns - 1) / columns;
            const auto visible = std::max<std::size_t>(1, static_cast<std::size_t>(
                (layout.albumGrid.bottom - layout.albumGrid.top) / 184.0F));
            const auto maximum = rows > visible ? rows - visible : 0;
            const auto notches = GET_WHEEL_DELTA_WPARAM(wp) / WHEEL_DELTA;
            const auto candidate = static_cast<long long>(m_albumGridTopRow) - static_cast<long long>(notches) * 2LL;
            m_albumGridTopRow = static_cast<std::size_t>(std::clamp(candidate, 0LL, static_cast<long long>(maximum)));
            invalidate();
        } else if (m_workspace == Workspace::album && contains(layout.albumTrackBody, point)
            && m_albumSelection < m_albums.size()) {
            const auto count = m_albums[m_albumSelection].sourceIndices.size();
            const auto capacity = visibleRowCapacity(layout.albumTrackBody.bottom - layout.albumTrackBody.top,
                kCompactTrackRowHeight);
            const auto maximum = maximumTopRow(count, capacity);
            const auto notches = GET_WHEEL_DELTA_WPARAM(wp) / WHEEL_DELTA;
            const auto candidate = static_cast<long long>(m_albumTrackTopRow) - static_cast<long long>(notches) * 3LL;
            m_albumTrackTopRow = static_cast<std::size_t>(std::clamp(candidate, 0LL, static_cast<long long>(maximum)));
            invalidate();
        } else if (m_queueMode && contains(layout.queuePanel, point)) {
            const auto capacity = visibleRowCapacity(layout.queueBody.bottom - layout.queueBody.top, kCompactTrackRowHeight);
            const auto maximum = maximumTopRow(m_queueItems.size(), capacity);
            const auto notches = GET_WHEEL_DELTA_WPARAM(wp) / WHEEL_DELTA;
            const auto candidate = static_cast<long long>(m_queueTopRow) - static_cast<long long>(notches) * 3LL;
            m_queueTopRow = static_cast<std::size_t>(std::clamp(candidate, 0LL, static_cast<long long>(maximum)));
            invalidate();
        } else if (m_workspace == Workspace::playlist && contains(layout.playlistBrowser, point)) {
            const auto capacity = visibleRowCapacity(layout.playlistBrowserBody.bottom
                - layout.playlistBrowserBody.top, 30.0F);
            const auto maximum = maximumTopRow(m_playlistBrowserRows.size(), capacity);
            const auto notches = GET_WHEEL_DELTA_WPARAM(wp) / WHEEL_DELTA;
            const auto candidate = static_cast<long long>(m_playlistBrowserTopRow)
                - static_cast<long long>(notches) * 3LL;
            m_playlistBrowserTopRow = static_cast<std::size_t>(std::clamp(candidate, 0LL,
                static_cast<long long>(maximum)));
            invalidate();
        } else if (contains(layout.playlistArea, point)) {
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

    bool onQueueKeyDown(HWND wnd, WPARAM key) {
        if (!m_queueMode) return false;
        const auto count = m_queueItems.size();
        const auto ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        const auto shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        if (ctrl && key == 'A') {
            m_queueSelected.assign(count, true);
            invalidate();
            return true;
        }
        if (key == VK_DELETE) {
            removeSelectedQueueItems();
            return true;
        }
        if (count == 0) return false;
        auto focus = m_queueFocus < count ? m_queueFocus : 0;
        const auto layout = calculateLayout(wnd);
        const auto page = std::max<std::size_t>(1,
            visibleRowCapacity(layout.queueBody.bottom - layout.queueBody.top, kCompactTrackRowHeight) - 1);
        auto target = focus;
        if (key == VK_UP) target = focus > 0 ? focus - 1 : 0;
        else if (key == VK_DOWN) target = std::min(count - 1, focus + 1);
        else if (key == VK_PRIOR) target = focus > page ? focus - page : 0;
        else if (key == VK_NEXT) target = std::min(count - 1, focus + page);
        else if (key == VK_HOME) target = 0;
        else if (key == VK_END) target = count - 1;
        else if (key == VK_RETURN) {
            playQueueItemNow(focus);
            return true;
        } else return false;
        selectQueueRow(target, ctrl, shift);
        m_queueTopRow = ensureRowVisible(m_queueTopRow, target, count,
            visibleRowCapacity(layout.queueBody.bottom - layout.queueBody.top, kCompactTrackRowHeight));
        return true;
    }

    void showAlbumSourceMenu() {
        const auto wnd = get_wnd();
        if (!wnd) return;
        const auto menu = CreatePopupMenu();
        if (!menu) return;
        AppendMenuW(menu, MF_STRING | (m_albumSourceKind == AlbumSourceKind::mediaLibrary ? MF_CHECKED : 0),
            1, L"Media Library");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        std::vector<GUID> playlistGuids;
        auto api = playlist_manager_v5::get();
        pfc::string8 name;
        for (t_size index = 0; index < api->get_playlist_count(); ++index) {
            if (!api->playlist_get_name(index, name) || std::strcmp(name.c_str(), "Refrain Album View") == 0) continue;
            playlistGuids.push_back(api->playlist_get_guid(index));
            const auto checked = m_albumSourceKind == AlbumSourceKind::playlist
                && InlineIsEqualGUID(playlistGuids.back(), m_albumSourcePlaylistGuid);
            const auto label = utf8ToWide(name.c_str());
            AppendMenuW(menu, MF_STRING | (checked ? MF_CHECKED : 0),
                100 + static_cast<UINT>(playlistGuids.size() - 1), label.c_str());
        }
        const auto layout = calculateLayout(wnd);
        POINT point{static_cast<LONG>(layout.albumSource.left * dpiScale()),
            static_cast<LONG>(layout.albumSource.bottom * dpiScale())};
        ClientToScreen(wnd, &point);
        const auto command = TrackPopupMenuEx(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
            point.x, point.y, wnd, nullptr);
        DestroyMenu(menu);
        if (command == 1) loadAlbumSourceLibrary();
        else if (command >= 100 && command - 100 < playlistGuids.size()) {
            loadAlbumSourcePlaylist(playlistGuids[command - 100]);
        }
    }

    bool onKeyDown(HWND wnd, WPARAM key) {
        if (m_workspace == Workspace::album && (m_focus == ControlId::albumGrid
                || m_focus == ControlId::albumTrackList)) {
            const auto ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            const auto shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            if (key == VK_RETURN && m_focus == ControlId::albumTrackList) {
                playAlbumTrack(m_albumTrackSelection);
                return true;
            }
            if (m_focus == ControlId::albumGrid && !m_albums.empty()) {
                const auto columns = albumGridColumns(calculateLayout(wnd));
                auto next = m_albumSelection;
                if (key == VK_LEFT && next > 0) --next;
                else if (key == VK_RIGHT) next = std::min(m_albums.size() - 1, next + 1);
                else if (key == VK_UP) next = next >= columns ? next - columns : next;
                else if (key == VK_DOWN) next = std::min(m_albums.size() - 1, next + columns);
                else if (key == VK_RETURN) { playAlbumTrack(0); return true; }
                else if (key == VK_HOME) next = 0;
                else if (key == VK_END) next = m_albums.size() - 1;
                else next = SIZE_MAX;
                if (next != SIZE_MAX) {
                    m_albumSelection = next;
                    resetAlbumTrackSelection();
                    persistAlbumBrowserState();
                    const auto row = next / columns;
                    const auto layout = calculateLayout(wnd);
                    const auto visible = std::max<std::size_t>(1, static_cast<std::size_t>(
                        (layout.albumGrid.bottom - layout.albumGrid.top) / 184.0F));
                    if (row < m_albumGridTopRow) m_albumGridTopRow = row;
                    else if (row >= m_albumGridTopRow + visible) m_albumGridTopRow = row - visible + 1;
                    invalidate();
                    return true;
                }
            } else if (m_focus == ControlId::albumTrackList && m_albumSelection < m_albums.size()) {
                const auto count = m_albums[m_albumSelection].sourceIndices.size();
                if (key == VK_DELETE) return true;
                if (count > 0) {
                    if (ctrl && key == 'A') {
                        m_albumTrackSelected.assign(count, true);
                        invalidate();
                        return true;
                    }
                    if (key == VK_SPACE && m_albumTrackSelection < count) {
                        selectAlbumTrack(m_albumTrackSelection, true, false);
                        return true;
                    }
                    auto next = m_albumTrackSelection;
                    if (key == VK_UP && next > 0) --next;
                    else if (key == VK_DOWN) next = std::min(count - 1, next + 1);
                    else if (key == VK_HOME) next = 0;
                    else if (key == VK_END) next = count - 1;
                    else next = SIZE_MAX;
                    if (next != SIZE_MAX) {
                        if (ctrl && !shift) m_albumTrackSelection = next;
                        else selectAlbumTrack(next, ctrl, shift);
                        const auto layout = calculateLayout(wnd);
                        const auto capacity = visibleRowCapacity(layout.albumTrackBody.bottom
                            - layout.albumTrackBody.top, kCompactTrackRowHeight);
                        m_albumTrackTopRow = ensureRowVisible(m_albumTrackTopRow, next, count, capacity);
                        invalidate();
                        return true;
                    }
                }
            }
        }
        const std::array<ControlId, 13> allButtons{
            ControlId::playlistWorkspace, ControlId::albumWorkspace, ControlId::albumList, ControlId::settings,
            ControlId::open, ControlId::previous, ControlId::playPause, ControlId::next,
            ControlId::stop, ControlId::queueToggle, ControlId::playbackOrder,
            ControlId::outputDevice, ControlId::mute};
        std::array<ControlId, 13> buttons{};
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
        if (m_focus == ControlId::playlistBrowser && onPlaylistBrowserKeyDown(wnd, key)) return true;
        if (m_focus == ControlId::queueBody && onQueueKeyDown(wnd, key)) return true;
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
        case ControlId::open:
            if (!standard_commands::main_open()) showStatus(L"foobar2000's Open command is unavailable.");
            break;
        case ControlId::previous: api->userPrev(); break;
        case ControlId::playPause: api->play_or_pause(); break;
        case ControlId::next: api->userNext(); break;
        case ControlId::stop: api->userStop(); break;
        case ControlId::mute: api->userMute(); break;
        case ControlId::settings: showRefrainPreferences(); break;
        case ControlId::playlistWorkspace: setWorkspace(Workspace::playlist); break;
        case ControlId::albumWorkspace: setWorkspace(Workspace::album); break;
        case ControlId::albumList: openAlbumList(); break;
        case ControlId::albumSource: showAlbumSourceMenu(); break;
        case ControlId::albumLibraryPreferences: library_manager::get()->show_preferences(); break;
        case ControlId::queueToggle: toggleQueueMode(); break;
        case ControlId::playbackOrder: showPlaybackOrderMenu(); break;
        case ControlId::outputDevice: showOutputDeviceMenu(); break;
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

    void updatePlaylistBrowserDivider(HWND wnd, float x) {
        RECT client{};
        GetClientRect(wnd, &client);
        const auto width = std::max(1.0F, static_cast<float>(client.right - client.left) / dpiScale());
        const auto maxWidth = width * 0.35F;
        const auto requested = std::clamp(x, std::min(180.0F, maxWidth), maxWidth);
        m_playlistBrowserSplitPermille = normalizePlaylistBrowserSplitPermille(
            static_cast<int>(std::lround(requested / width * 1000.0F)));
        clampPlaylistScroll();
        syncPlaylistFilterControls(wnd);
        invalidate();
    }

    void cancelInteraction() {
        if (m_active == ControlId::playlistHeader && m_headerResizeColumns) {
            m_playlistSettings = m_headerStartSettings;
        }
        m_active = ControlId::none;
        m_previewing = false;
        m_dragHeaderPermille = -1;
        m_playlistScrollbarGrabOffset = 0.0F;
        m_playlistBrowserDragCandidate.reset();
        m_playlistBrowserDragging = false;
        m_playlistBrowserDragSnapshot.clear();
        m_albumGridScrollbarGrabOffset = 0.0F;
        m_albumTrackScrollbarGrabOffset = 0.0F;
        m_headerPressedColumn.reset();
        m_headerResizeColumns.reset();
        m_queueDragCandidate.reset();
        m_queueDragging = false;
        m_queueDragSnapshot.clear();
        m_queueDragSelection.clear();
        m_queueScrollbarGrabOffset = 0.0F;
        m_playlistDragging = false;
        m_playlistDragCandidate.reset();
        m_playlistDragSnapshot.remove_all();
        m_playlistDragSelection.clear();
        m_playlistCollapseSelectionOnClick = false;
        m_playlistRatingPress.reset();
        KillTimer(get_wnd(), kPlaylistDragTimer);
        if (GetCapture() == get_wnd()) {
            ReleaseCapture();
        }
        invalidate();
    }

    void releaseCapturePreservingInteraction(HWND wnd) {
        if (GetCapture() != wnd) return;
        m_ignoreCaptureChanged = true;
        ReleaseCapture();
        m_ignoreCaptureChanged = false;
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
            case ControlId::playlistWorkspace: m_tooltipText = L"Playlist Workspace"; break;
            case ControlId::albumWorkspace: m_tooltipText = L"Album Workspace"; break;
            case ControlId::albumList: m_tooltipText = L"Open foobar2000 Album List"; break;
            case ControlId::albumSource: m_tooltipText = L"Choose album source"; break;
            case ControlId::albumLibraryPreferences: m_tooltipText = L"Open Media Library Preferences"; break;
            case ControlId::albumGrid:
                if (m_albumHover && *m_albumHover < m_albums.size()) {
                    const auto& album = m_albums[*m_albumHover];
                    m_tooltipText = utf8ToWide(album.album.c_str()) + L"\r\n"
                        + utf8ToWide(album.albumArtist.c_str());
                    if (!album.date.empty()) m_tooltipText += L"\r\n" + utf8ToWide(album.date.c_str());
                    m_tooltipText += L"\r\n" + std::to_wstring(album.sourceIndices.size()) + L" tracks";
                }
                break;
            case ControlId::seek: m_tooltipText = L"Seek"; break;
            case ControlId::open: m_tooltipText = L"Open audio files"; break;
            case ControlId::previous: m_tooltipText = L"Previous"; break;
            case ControlId::playPause: m_tooltipText = m_state.playing && !m_state.paused ? L"Pause" : L"Play"; break;
            case ControlId::next: m_tooltipText = L"Next"; break;
            case ControlId::stop: m_tooltipText = L"Stop"; break;
            case ControlId::mute: m_tooltipText = m_state.muted() ? L"Unmute" : L"Mute"; break;
            case ControlId::volume: m_tooltipText = L"Volume (mouse wheel supported)"; break;
            case ControlId::settings: m_tooltipText = L"Refrain settings"; break;
            case ControlId::queueToggle: m_tooltipText = m_queueMode ? L"Show now playing" : L"Show playback queue"; break;
            case ControlId::playbackOrder: m_tooltipText = L"Playback Order: " + playbackOrderName(); break;
            case ControlId::outputDevice: m_tooltipText = L"Output Device: " + outputDeviceName(); break;
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
        m_playlistBrowserSplitPermille = readPlaylistBrowserSettings().splitPermille;
        reloadPlaylistViewSettings();
        rebuildPlaylistGroups();
        clampPlaylistScroll();
        clampPlaylistBrowserScroll();
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
    bool m_ignoreCaptureChanged{};
    bool m_previewing{};
    bool m_queueMode{};
    bool m_playlistQueueModeBeforeAlbum{};
    Workspace m_workspace{Workspace::playlist};
    AlbumSourceKind m_albumSourceKind{AlbumSourceKind::playlist};
    GUID m_albumSourcePlaylistGuid{};
    GUID m_albumReturnPlaylistGuid{};
    GUID m_albumPendingReturnPlaylistGuid{};
    GUID m_initialAlbumSourceGuid{};
    bool m_albumWorkspaceEntered{};
    std::wstring m_albumSourceName{L"Current playlist"};
    metadb_handle_list m_albumSourceItems;
    std::vector<AlbumSourceTrack> m_albumInput;
    std::vector<AlbumBrowserAlbum> m_albums;
    std::size_t m_albumSelection{};
    std::size_t m_albumTrackSelection{};
    std::vector<bool> m_albumTrackSelected;
    std::size_t m_albumTrackAnchor{SIZE_MAX};
    std::size_t m_albumGridTopRow{};
    std::size_t m_albumTrackTopRow{};
    float m_albumGridScrollbarGrabOffset{};
    float m_albumTrackScrollbarGrabOffset{};
    std::unordered_map<std::string, std::pair<std::string, std::size_t>> m_albumSourceStates;
    int m_albumSplitPermille{780};
    std::uint64_t m_albumArtworkGeneration{};
    std::size_t m_albumArtworkBytes{};
    std::uint64_t m_albumArtworkUseCounter{};
    std::unordered_map<std::string, GroupArtworkCacheEntry> m_albumArtworkCache;
    std::vector<GroupArtworkRequest> m_albumArtworkRequests;
    std::mutex m_albumArtworkMutex;
    std::condition_variable m_albumArtworkCondition;
    std::shared_ptr<abort_callback_impl> m_albumArtworkAborter;
    std::jthread m_albumArtworkThread;
    service_ptr_t<AlbumViewPlaylistLock> m_albumLock;
    t_size m_albumBridgePlaylist{SIZE_MAX};
    t_size m_albumPriorPlaybackOrder{SIZE_MAX};
    bool m_albumSessionActive{};
    bool m_startingAlbumSession{};
    bool m_albumSourceRestorePending{};
    bool m_restoringAlbumPlaybackOrder{};
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
    std::vector<PlaylistBrowserRow> m_playlistBrowserAllRows;
    std::vector<PlaylistBrowserRow> m_playlistBrowserRows;
    std::wstring m_playlistFilterText;
    HWND m_playlistFilterEdit{};
    HWND m_playlistFilterClear{};
    std::vector<GUID> m_playlistBrowserSelection;
    std::size_t m_playlistBrowserFocus{SIZE_MAX};
    std::size_t m_playlistBrowserAnchor{SIZE_MAX};
    std::size_t m_playlistBrowserTopRow{};
    int m_playlistBrowserSplitPermille{150};
    HWND m_playlistRenameEdit{};
    GUID m_playlistRenameGuid{};
    std::wstring m_playlistRenameOriginal;
    bool m_playlistRenameCommitting{};
    bool m_playlistRenameCancelPending{};
    std::optional<std::size_t> m_playlistBrowserDragCandidate;
    bool m_playlistBrowserDragging{};
    std::size_t m_playlistBrowserInsertion{};
    D2D1_POINT_2F m_playlistBrowserDragStart{};
    std::vector<GUID> m_playlistBrowserDragSnapshot;
    std::vector<t_playback_queue_item> m_queueItems;
    std::vector<bool> m_queueSelected;
    std::size_t m_queueFocus{SIZE_MAX};
    std::size_t m_queueAnchor{SIZE_MAX};
    std::size_t m_queueTopRow{};
    float m_queueScrollbarGrabOffset{};
    std::unordered_map<std::size_t, QueueRowText> m_queueTextCache;
    std::optional<std::size_t> m_queueDragCandidate;
    bool m_queueDragging{};
    std::size_t m_queueInsertion{};
    std::vector<t_playback_queue_item> m_queueDragSnapshot;
    std::vector<bool> m_queueDragSelection;
    D2D1_POINT_2F m_dragStartPoint{};
    std::optional<std::size_t> m_playlistDragCandidate;
    bool m_playlistDragging{};
    bool m_playlistCollapseSelectionOnClick{};
    std::size_t m_playlistInsertion{};
    std::size_t m_playlistDropDisplayBoundary{};
    metadb_handle_list m_playlistDragSnapshot;
    std::vector<bool> m_playlistDragSelection;
    D2D1_POINT_2F m_playlistDragPoint{};
    GUID m_playlistDragGuid{};
    bool m_oleDragFromThisPanel{};
    bool m_oleDropHandledByThisPanel{};
    std::optional<std::pair<std::size_t, int>> m_playlistRatingHover;
    std::optional<std::size_t> m_albumHover;
    std::optional<std::pair<std::size_t, int>> m_playlistRatingPress;
    bool m_externalDropAccepts{};
    bool m_externalDropNative{};
    bool m_externalDropActive{};
    DropDestination m_externalDropDestination{DropDestination::none};
    GUID m_externalDropBrowserGuid{};
    bool m_externalDropBrowserBlank{};
    std::size_t m_externalDropInsertion{};
    std::size_t m_externalDropDisplayBoundary{};
    std::shared_ptr<DropAsyncState> m_dropAsync{std::make_shared<DropAsyncState>()};
    pfc::com_ptr_t<DropTargetImpl> m_dropTarget;
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
    GroupArtworkSource m_groupArtworkSource{GroupArtworkSource::placeholder};
    std::size_t m_groupArtworkBytes{};
    std::unordered_map<std::string, GroupArtworkCacheEntry> m_groupArtworkCache;
    std::vector<GroupArtworkRequest> m_groupArtworkRequests;
    std::mutex m_groupArtworkMutex;
    std::condition_variable m_groupArtworkCondition;
    std::shared_ptr<abort_callback_impl> m_groupArtworkAborter;
    bool m_groupArtworkAborterAlbum{};
    std::jthread m_groupArtworkThread;
    LyricsHost m_lyricsHost;
    double m_previewPosition{};
    float m_dpi{96.0F};
    ControlId m_hover{ControlId::none};
    ControlId m_active{ControlId::none};
    ControlId m_focus{ControlId::none};
    HWND m_tooltip{};
    std::wstring m_tooltipText;
    std::wstring m_statusText;
    bool m_outputCallbackRegistered{};

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
    titleformat_object::ptr m_queueLengthScript;
    titleformat_object::ptr m_albumNameScript;
    titleformat_object::ptr m_albumArtistScript;
    titleformat_object::ptr m_albumDateScript;
    titleformat_object::ptr m_albumDiscScript;
    titleformat_object::ptr m_albumTrackScript;
    ComPtr<ID2D1SolidColorBrush> m_background;
    ComPtr<ID2D1SolidColorBrush> m_surface;
    ComPtr<ID2D1SolidColorBrush> m_foreground;
    ComPtr<ID2D1SolidColorBrush> m_disabled;
    ComPtr<ID2D1SolidColorBrush> m_accent;
    ComPtr<ID2D1SolidColorBrush> m_defaultPlaylist;
    ComPtr<ID2D1SolidColorBrush> m_light;
};

uie::window_factory<PlaybackPanel> g_playbackPanelFactory;

} // namespace

void ensurePlaybackPanelLinked() noexcept {}

} // namespace refrain

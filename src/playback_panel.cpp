#include "playback_panel.h"
#include "playback_state.h"
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
#include <cmath>
#include <memory>
#include <string>

#include <columns_ui-sdk-8.1.0/ui_extension.h>
#include <foobar2000/SDK/foobar2000.h>

namespace refrain {
namespace {

using Microsoft::WRL::ComPtr;

constexpr GUID kPlaybackPanelGuid{0x9e26e4b0, 0x41a9, 0x4b08, {0xa3, 0x55, 0x74, 0x55, 0x10, 0x82, 0xc5, 0xf2}};
constexpr UINT kRefreshMessage = WM_APP + 0x421;

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
    float surfaceTop{};
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

class PlaybackPanel : public uie::container_uie_window_v3, public play_callback {
public:
    PlaybackPanel() = default;
    ~PlaybackPanel() {
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
            m_dpi = static_cast<float>(GetDpiForWindow(wnd));
            createIndependentResources();
            registerPlaybackCallback();
            refreshFromCore();
            createTooltip(wnd);
            registerSettingsWindow(wnd);
            return 0;
        case WM_DESTROY:
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
            if (LOWORD(lp) == HTCLIENT && (m_hover == ControlId::seek || m_hover == ControlId::volume)) {
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
        }
        return DefWindowProc(wnd, msg, wp, lp);
    }

    void on_playback_starting(play_control::t_track_command, bool paused) override {
        m_state.playing = true;
        m_state.paused = paused;
        requestRefresh();
    }
    void on_playback_new_track(metadb_handle_ptr) override { requestRefresh(); }
    void on_playback_stop(play_control::t_stop_reason) override {
        m_state.playing = false;
        m_state.paused = false;
        m_state.seekable = false;
        m_state.position = 0.0;
        m_state.length = 0.0;
        m_previewing = false;
        invalidate();
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

private:
    [[nodiscard]] float dpiScale() const noexcept { return m_dpi / 96.0F; }

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
        createTextResources();
    }

    void createTextResources() {
        if (!m_dwriteFactory || m_textFormat) {
            return;
        }
        if (SUCCEEDED(m_dwriteFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
                DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12.0F, L"", m_textFormat.ReleaseAndGetAddressOf()))) {
            m_textFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }
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
        return m_background && m_surface && m_foreground && m_disabled && m_accent && m_light;
    }

    void releaseTextResources() noexcept { m_textFormat.Reset(); }

    void releaseDeviceResources() noexcept {
        m_light.Reset();
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
        return layout;
    }

    [[nodiscard]] ControlId hitTest(HWND wnd, D2D1_POINT_2F point) const {
        const auto layout = calculateLayout(wnd);
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
            m_light.Reset();
            m_accent.Reset();
            m_disabled.Reset();
            m_foreground.Reset();
            m_surface.Reset();
            m_background.Reset();
            m_renderTarget.Reset();
        }
        EndPaint(wnd, &paintStruct);
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
        }
    }

    void onLeftButtonDown(HWND wnd, LPARAM lp) {
        SetFocus(wnd);
        const auto point = pointFromLParam(lp, dpiScale());
        const auto hit = hitTest(wnd, point);
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
        } else if (active != ControlId::volume && active == hit) {
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
        if (contains(layout.volume, point) || contains(layout.mute, point)) {
            if (GET_WHEEL_DELTA_WPARAM(wp) > 0) {
                playback_control::get()->volume_up();
            } else {
                playback_control::get()->volume_down();
            }
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

    void cancelInteraction() {
        m_active = ControlId::none;
        m_previewing = false;
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
            default: m_tooltipText.clear(); break;
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
    bool m_callbackRegistered{};
    bool m_trackingMouse{};
    bool m_previewing{};
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
    ComPtr<IDWriteTextFormat> m_textFormat;
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

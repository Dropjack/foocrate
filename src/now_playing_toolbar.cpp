#include "now_playing_state.h"

#include <windows.h>
#include <mmsystem.h>

#include <array>
#include <string>
#include <vector>

#include <columns_ui-sdk-8.1.0/ui_extension.h>
#include <foobar2000/SDK/foobar2000.h>

namespace refrain {
namespace {

constexpr GUID kNowPlayingToolbarGuid{0x61c1cd76, 0x9944, 0x4b63,
    {0xa1, 0x71, 0x2b, 0x70, 0x18, 0xe8, 0x65, 0x95}};

class NowPlayingToolbar : public uie::container_uie_window_v3,
                          private play_callback_impl_base {
public:
    NowPlayingToolbar()
        : play_callback_impl_base(play_callback::flag_on_playback_starting
              | play_callback::flag_on_playback_new_track
              | play_callback::flag_on_playback_stop
              | play_callback::flag_on_playback_pause
              | play_callback::flag_on_playback_edited
              | play_callback::flag_on_playback_dynamic_info
              | play_callback::flag_on_playback_dynamic_info_track) {}

    ~NowPlayingToolbar() { releaseFont(); }

    const GUID& get_extension_guid() const override { return kNowPlayingToolbarGuid; }
    void get_name(pfc::string_base& out) const override { out = "Refrain Now Playing Summary"; }
    void get_category(pfc::string_base& out) const override { out = "Toolbars"; }
    bool get_short_name(pfc::string_base& out) const override {
        out = "Refrain Summary";
        return true;
    }
    bool get_description(pfc::string_base& out) const override {
        out = "Single-line Refrain now-playing summary for the Columns UI toolbar area";
        return true;
    }
    unsigned get_type() const override { return uie::type_toolbar; }

    void get_size_limits(uie::size_limit_t& limits) const override {
        limits.min_width = 160;
        limits.min_height = 20;
        limits.max_height = 40;
    }

    uie::container_window_v3_config get_window_config() override {
        auto config = uie::container_window_v3_config{L"Refrain.NowPlayingSummary", false, 0};
        config.class_cursor = IDC_ARROW;
        return config;
    }

    LRESULT on_message(HWND wnd, UINT message, WPARAM wp, LPARAM lp) override {
        switch (message) {
        case WM_CREATE:
            compileScripts();
            createFont(wnd);
            rebuildText();
            return 0;
        case WM_DESTROY:
            releaseFont();
            m_text.clear();
            return 0;
        case WM_DPICHANGED:
        case WM_DPICHANGED_AFTERPARENT:
        case WM_SETTINGCHANGE:
            releaseFont();
            createFont(wnd);
            InvalidateRect(wnd, nullptr, TRUE);
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT:
            paint(wnd);
            return 0;
        }
        return DefWindowProcW(wnd, message, wp, lp);
    }

    void on_playback_starting(play_control::t_track_command, bool) override { refresh(); }
    void on_playback_new_track(metadb_handle_ptr) override { refresh(); }
    void on_playback_stop(play_control::t_stop_reason) override { refresh(); }
    void on_playback_pause(bool) override { refresh(); }
    void on_playback_edited(metadb_handle_ptr) override { refresh(); }
    void on_playback_dynamic_info(const file_info&) override { refresh(); }
    void on_playback_dynamic_info_track(const file_info&) override { refresh(); }

private:
    enum ScriptIndex : std::size_t {
        title,
        artist,
        album,
        codec,
        codecProfile,
        encoding,
        channelMode,
        bitDepth,
        bitrate,
        sampleRate,
        count,
    };

    void compileScripts() {
        constexpr std::array<const char*, count> expressions{
            "$if2(%title%,$if2($filename(%path%),Unknown title))",
            "[%artist%]",
            "[%album%]",
            "[%codec%]",
            "[%codec_profile%]",
            "[%__encoding%]",
            "$if2(%channel_mode%,[%channels%])",
            "[%bitspersample%]",
            "[%bitrate%]",
            "[%samplerate%]",
        };
        const auto compiler = titleformat_compiler::get();
        for (std::size_t index = 0; index < expressions.size(); ++index) {
            compiler->compile_safe(m_scripts[index], expressions[index]);
        }
    }

    [[nodiscard]] std::wstring formatField(std::size_t index) const {
        if (index >= m_scripts.size() || m_scripts[index].is_empty()) return {};
        pfc::string8 output;
        const auto playback = playback_control::get();
        if (!playback->playback_format_title(nullptr, output, m_scripts[index], nullptr,
                playback_control::display_level_all)) {
            return {};
        }
        if (output.is_empty()) return {};
        const auto length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
            output.c_str(), -1, nullptr, 0);
        if (length <= 1) return {};
        std::wstring result(static_cast<std::size_t>(length), L'\0');
        if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, output.c_str(), -1,
                result.data(), length) <= 0) {
            return {};
        }
        result.pop_back();
        return result;
    }

    void rebuildText() {
        const auto playback = playback_control::get();
        if (!playback->is_playing()) {
            m_text.clear();
            return;
        }
        std::vector<std::wstring> technical;
        const auto append = [&](std::wstring value) {
            if (!value.empty()) technical.push_back(std::move(value));
        };
        append(formatField(codec));
        append(formatField(codecProfile));
        append(formatField(encoding));
        append(formatField(channelMode));
        if (auto value = formatField(bitDepth); !value.empty()) append(std::move(value) + L" bits");
        if (auto value = formatField(bitrate); !value.empty()) append(std::move(value) + L" kbps");
        if (auto value = formatField(sampleRate); !value.empty()) append(std::move(value) + L" Hz");
        m_text = buildTopBarSummary(playback->is_paused()
                ? TopBarPlaybackState::paused : TopBarPlaybackState::playing,
            formatField(title), formatField(artist), formatField(album), technical);
    }

    void refresh() {
        rebuildText();
        if (const auto wnd = get_wnd()) InvalidateRect(wnd, nullptr, TRUE);
    }

    void createFont(HWND wnd) {
        NONCLIENTMETRICSW metrics{sizeof(metrics)};
        const auto dpi = GetDpiForWindow(wnd);
        if (SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0, dpi)) {
            m_font = CreateFontIndirectW(&metrics.lfMenuFont);
        }
        if (!m_font) m_font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    }

    void releaseFont() noexcept {
        if (m_font && m_font != GetStockObject(DEFAULT_GUI_FONT)) DeleteObject(m_font);
        m_font = nullptr;
    }

    void paint(HWND wnd) const {
        PAINTSTRUCT paint{};
        const auto dc = BeginPaint(wnd, &paint);
        RECT client{};
        GetClientRect(wnd, &client);
        FillRect(dc, &client, GetSysColorBrush(COLOR_MENU));
        const auto previousFont = SelectObject(dc,
            m_font ? m_font : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT)));
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, GetSysColor(COLOR_MENUTEXT));
        client.left += MulDiv(8, static_cast<int>(GetDpiForWindow(wnd)), 96);
        client.right -= MulDiv(8, static_cast<int>(GetDpiForWindow(wnd)), 96);
        DrawTextW(dc, m_text.c_str(), static_cast<int>(m_text.size()), &client,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        SelectObject(dc, previousFont);
        EndPaint(wnd, &paint);
    }

    std::array<titleformat_object::ptr, count> m_scripts;
    std::wstring m_text;
    HFONT m_font{};
};

uie::window_factory<NowPlayingToolbar> g_nowPlayingToolbarFactory;

} // namespace
} // namespace refrain

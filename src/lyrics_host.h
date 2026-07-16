#pragma once

#include <windows.h>
#include <mmsystem.h>
#include <objbase.h>
#include <ole2.h>

#include <columns_ui-sdk-8.1.0/ui_extension.h>

#include <cstdint>
#include <string>

namespace refrain {

class LyricsHost {
public:
    LyricsHost() = default;
    LyricsHost(const LyricsHost&) = delete;
    LyricsHost& operator=(const LyricsHost&) = delete;
    ~LyricsHost();

    [[nodiscard]] bool create(HWND parent, const uie::window_host_ptr& upstream, const RECT& bounds,
        UINT middleClickMessage);
    void destroy() noexcept;
    void resize(const RECT& bounds) noexcept;
    void setVisible(bool visible) noexcept;
    void setThemeColors(std::uint32_t backgroundArgb, std::uint32_t normalArgb,
        std::uint32_t highlightArgb) noexcept;
    void setInstanceConfig(const void* data, t_size size, bool portable);
    [[nodiscard]] bool getInstanceConfig(
        pfc::array_t<t_uint8>& output, abort_callback& abort, bool portable) const;
    void forwardMessage(UINT message, WPARAM wp, LPARAM lp) const noexcept;

    [[nodiscard]] bool available() const noexcept { return m_child != nullptr && m_panel.is_valid(); }
    [[nodiscard]] bool visible() const noexcept { return m_visible; }
    [[nodiscard]] HWND childWindow() const noexcept { return m_child; }
    [[nodiscard]] const std::wstring& statusText() const noexcept { return m_statusText; }
    [[nodiscard]] UINT middleClickMessage() const noexcept { return m_middleClickMessage; }

private:
    class HostBridge;
    friend class HostBridge;

    void relinquish(HWND child) noexcept;
    void childVisibilityRequested(HWND child, bool visible) noexcept;
    [[nodiscard]] bool childIsVisible(HWND child) const noexcept;
    [[nodiscard]] bool applyThemeColors(
        bool background = true, bool normal = true, bool highlight = true) const noexcept;

    HWND m_parent{};
    HWND m_child{};
    bool m_visible{true};
    bool m_destroying{};
    uie::window::ptr m_panel;
    uie::window_host_ptr m_host;
    HostBridge* m_hostBridge{};
    std::wstring m_statusText{L"ESLyric not detected"};
    UINT m_middleClickMessage{};
    std::uint32_t m_backgroundArgb{0xFFF4F1EAu};
    std::uint32_t m_normalArgb{0xFF6F6B64u};
    std::uint32_t m_highlightArgb{0xFF365F9Du};
    pfc::array_t<t_uint8> m_instanceConfig;
    bool m_instanceConfigPortable{};
};

} // namespace refrain

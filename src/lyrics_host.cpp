#include "lyrics_host.h"

#include <foobar2000/SDK/foobar2000.h>

#include <algorithm>
#include <commctrl.h>
#include <cwctype>
#include <utility>
#include <wrl/client.h>

namespace refrain {
namespace {

using Microsoft::WRL::ComPtr;

constexpr GUID kEsLyricControlClassId{
    0x9e089429, 0x58b3, 0x4f58, {0xbf, 0x47, 0xa7, 0x65, 0xec, 0x05, 0x38, 0xe6}};

using DllGetClassObjectProc = HRESULT(STDAPICALLTYPE*)(REFCLSID, REFIID, LPVOID*);

[[nodiscard]] HRESULT dispatchId(IDispatch* dispatch, const wchar_t* name, DISPID& result) noexcept {
    if (!dispatch || !name) return E_POINTER;
    auto mutableName = const_cast<LPOLESTR>(name);
    return dispatch->GetIDsOfNames(IID_NULL, &mutableName, 1, LOCALE_USER_DEFAULT, &result);
}

[[nodiscard]] HRESULT esLyricPanelCollection(IDispatch* control, ComPtr<IDispatch>& panels) noexcept {
    DISPID method{};
    auto result = dispatchId(control, L"GetAll", method);
    if (FAILED(result)) return result;

    DISPPARAMS parameters{};
    VARIANT value;
    VariantInit(&value);
    result = control->Invoke(method, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD,
        &parameters, &value, nullptr, nullptr);
    if (SUCCEEDED(result)) {
        if (value.vt == VT_DISPATCH && value.pdispVal) {
            panels.Attach(value.pdispVal);
            value.vt = VT_EMPTY;
        } else if (value.vt == VT_UNKNOWN && value.punkVal) {
            result = value.punkVal->QueryInterface(IID_PPV_ARGS(panels.ReleaseAndGetAddressOf()));
        } else {
            result = E_NOINTERFACE;
        }
    }
    VariantClear(&value);
    return result;
}

[[nodiscard]] HRESULT setEsLyricColor(
    IDispatch* panels, const wchar_t* methodName, std::uint32_t argb) noexcept {
    DISPID method{};
    auto result = dispatchId(panels, methodName, method);
    if (FAILED(result)) return result;

    VARIANTARG argument;
    VariantInit(&argument);
    argument.vt = VT_I4;
    argument.lVal = static_cast<LONG>(argb);
    DISPPARAMS parameters{&argument, nullptr, 1, 0};
    result = panels->Invoke(method, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD,
        &parameters, nullptr, nullptr, nullptr);
    VariantClear(&argument);
    return result;
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

[[nodiscard]] std::wstring lower(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](wchar_t character) { return static_cast<wchar_t>(std::towlower(character)); });
    return value;
}

[[nodiscard]] bool findEsLyricPanel(uie::window::ptr& output) {
    service_enum_t<uie::window> enumerator;
    uie::window::ptr candidate;
    if (!enumerator.first(candidate)) return false;
    do {
        pfc::string8 name;
        candidate->get_name(name);
        if (lower(utf8ToWide(name.c_str())) == L"eslyric") {
            FB2K_console_formatter() << "Refrain: detected ESLyric Columns UI panel GUID "
                                     << pfc::print_guid(candidate->get_extension_guid());
            output = candidate;
            return true;
        }
    } while (enumerator.next(candidate));
    return false;
}

} // namespace

LRESULT CALLBACK lyricsSubclassProc(HWND window, UINT message, WPARAM wp, LPARAM lp,
    UINT_PTR, DWORD_PTR reference) {
    auto* owner = reinterpret_cast<LyricsHost*>(reference);
    if (owner && message == WM_MBUTTONUP) {
        PostMessage(GetParent(window), owner->middleClickMessage(), wp, lp);
        return 0;
    }
    return DefSubclassProc(window, message, wp, lp);
}

class LyricsHost::HostBridge : public uie::window_host {
public:
    HostBridge(LyricsHost& owner, uie::window_host_ptr upstream)
        : m_owner(&owner), m_upstream(std::move(upstream)) {
        if (FAILED(CoCreateGuid(&m_guid))) {
            m_guid = {0xa105733e, 0xb639, 0x4979, {0x87, 0x52, 0x37, 0xea, 0x6f, 0xcd, 0x4c, 0x84}};
        }
    }

    void detach() noexcept { m_owner = nullptr; }

    const GUID& get_host_guid() const override { return m_guid; }
    void on_size_limit_change(HWND, unsigned) override {}
    unsigned is_resize_supported(HWND) const override { return 0; }
    bool request_resize(HWND, unsigned, unsigned, unsigned) override { return false; }
    bool override_status_text_create(service_ptr_t<ui_status_text_override>& output) override {
        return m_upstream.is_valid() && m_upstream->override_status_text_create(output);
    }
    bool get_keyboard_shortcuts_enabled() const override {
        return m_upstream.is_empty() || m_upstream->get_keyboard_shortcuts_enabled();
    }
    bool is_visible(HWND child) const override {
        return m_owner && m_owner->childIsVisible(child);
    }
    bool is_visibility_modifiable(HWND child, bool) const override {
        return m_owner && child == m_owner->m_child;
    }
    bool set_window_visibility(HWND child, bool visible) override {
        if (!m_owner || child != m_owner->m_child) return false;
        m_owner->childVisibilityRequested(child, visible);
        return true;
    }
    void relinquish_ownership(HWND child) override {
        if (m_owner) m_owner->relinquish(child);
    }

private:
    LyricsHost* m_owner{};
    uie::window_host_ptr m_upstream;
    GUID m_guid{};
};

LyricsHost::~LyricsHost() {
    destroy();
}

bool LyricsHost::create(HWND parent, const uie::window_host_ptr& upstream, const RECT& bounds,
    UINT middleClickMessage) {
    destroy();
    m_parent = parent;
    m_middleClickMessage = middleClickMessage;
    m_statusText = L"ESLyric not detected";

    uie::window::ptr panel;
    if (!findEsLyricPanel(panel)) return false;

    auto host = fb2k::service_new<HostBridge>(*this, upstream);
    if (!panel->is_available(host)) {
        m_statusText = L"ESLyric panel is already owned by another host";
        host->detach();
        return false;
    }

    try {
        if (m_instanceConfig.get_size() > 0) {
            abort_callback_dummy abort;
            if (m_instanceConfigPortable) {
                panel->import_config_from_ptr(
                    m_instanceConfig.get_ptr(), m_instanceConfig.get_size(), abort);
            } else {
                panel->set_config_from_ptr(
                    m_instanceConfig.get_ptr(), m_instanceConfig.get_size(), abort);
            }
        }
        const ui_helpers::window_position_t position(bounds);
        const auto child = panel->create_or_transfer_window(parent, host, position);
        if (!child || !IsWindow(child)) {
            m_statusText = L"ESLyric panel could not be created";
            host->detach();
            return false;
        }
        m_panel = panel;
        m_host = host;
        m_hostBridge = host.get_ptr();
        m_child = child;
        SetWindowSubclass(m_child, lyricsSubclassProc, 1, reinterpret_cast<DWORD_PTR>(this));
        m_statusText.clear();
        resize(bounds);
        setVisible(m_visible);
        if (!applyThemeColors()) {
            FB2K_console_formatter() << "Refrain: ESLyric background synchronization is unavailable; "
                                        "enable pref.script.expose or check ESLyric compatibility.";
        }
        return true;
    } catch (...) {
        m_statusText = L"ESLyric panel creation failed";
        host->detach();
        return false;
    }
}

void LyricsHost::setInstanceConfig(const void* data, t_size size, bool portable) {
    if (size > 0 && !data) throw pfc::exception_invalid_params();
    m_instanceConfig.set_data_fromptr(static_cast<const t_uint8*>(data), size);
    m_instanceConfigPortable = portable;
}

bool LyricsHost::getInstanceConfig(
    pfc::array_t<t_uint8>& output, abort_callback& abort, bool portable) const {
    output.set_size(0);
    if (m_panel.is_valid()) {
        if (portable) m_panel->export_config_to_array(output, abort, true);
        else m_panel->get_config_to_array(output, abort, true);
        return true;
    }
    if (m_instanceConfig.get_size() == 0 || m_instanceConfigPortable != portable) return false;
    output.set_data_fromptr(m_instanceConfig.get_ptr(), m_instanceConfig.get_size());
    return true;
}

void LyricsHost::destroy() noexcept {
    m_destroying = true;
    if (m_child && IsWindow(m_child)) RemoveWindowSubclass(m_child, lyricsSubclassProc, 1);
    if (m_panel.is_valid() && m_child && IsWindow(m_child)) {
        try {
            m_panel->destroy_window();
        } catch (...) {
        }
    }
    m_child = nullptr;
    m_panel.release();
    if (m_hostBridge) m_hostBridge->detach();
    m_hostBridge = nullptr;
    m_host.release();
    m_parent = nullptr;
    m_destroying = false;
}

void LyricsHost::resize(const RECT& bounds) noexcept {
    if (!m_child || !IsWindow(m_child)) return;
    const auto width = std::max<LONG>(0, bounds.right - bounds.left);
    const auto height = std::max<LONG>(0, bounds.bottom - bounds.top);
    SetWindowPos(m_child, nullptr, bounds.left, bounds.top, width, height,
        SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOCOPYBITS);
}

void LyricsHost::setVisible(bool visible) noexcept {
    m_visible = visible;
    if (m_child && IsWindow(m_child)) ShowWindow(m_child, visible ? SW_SHOWNA : SW_HIDE);
}

void LyricsHost::setThemeColors(std::uint32_t backgroundArgb, std::uint32_t normalArgb,
    std::uint32_t highlightArgb) noexcept {
    m_backgroundArgb = backgroundArgb;
    m_normalArgb = normalArgb;
    m_highlightArgb = highlightArgb;
    if (available()) (void)applyThemeColors();
}

bool LyricsHost::applyThemeColors() const noexcept {
    if (!m_child || !IsWindow(m_child)) return false;
    const auto module = reinterpret_cast<HMODULE>(GetWindowLongPtrW(m_child, GWLP_HINSTANCE));
    if (!module) return false;
    const auto entry = reinterpret_cast<DllGetClassObjectProc>(
        GetProcAddress(module, "DllGetClassObject"));
    if (!entry) return false;

    ComPtr<IClassFactory> factory;
    auto result = entry(kEsLyricControlClassId, IID_PPV_ARGS(factory.ReleaseAndGetAddressOf()));
    if (FAILED(result)) return false;

    ComPtr<IDispatch> control;
    result = factory->CreateInstance(nullptr, IID_PPV_ARGS(control.ReleaseAndGetAddressOf()));
    if (FAILED(result)) return false;

    ComPtr<IDispatch> panels;
    result = esLyricPanelCollection(control.Get(), panels);
    if (FAILED(result) || !panels) return false;
    const auto background = setEsLyricColor(panels.Get(), L"SetBackgroundColor", m_backgroundArgb);
    const auto normal = setEsLyricColor(panels.Get(), L"SetTextColor", m_normalArgb);
    const auto highlight = setEsLyricColor(panels.Get(), L"SetTextHighlightColor", m_highlightArgb);
    (void)normal;
    (void)highlight;
    return SUCCEEDED(background);
}

void LyricsHost::forwardMessage(UINT message, WPARAM wp, LPARAM lp) const noexcept {
    if (m_child && IsWindow(m_child)) SendMessage(m_child, message, wp, lp);
}

void LyricsHost::relinquish(HWND child) noexcept {
    if (m_destroying || child != m_child) return;
    if (IsWindow(child)) RemoveWindowSubclass(child, lyricsSubclassProc, 1);
    m_child = nullptr;
    m_panel.release();
    m_statusText = L"ESLyric panel ownership was transferred";
}

void LyricsHost::childVisibilityRequested(HWND child, bool visible) noexcept {
    if (child != m_child || !IsWindow(child)) return;
    m_visible = visible;
    ShowWindow(child, visible ? SW_SHOWNA : SW_HIDE);
}

bool LyricsHost::childIsVisible(HWND child) const noexcept {
    return child == m_child && m_visible && IsWindow(child) && IsWindowVisible(child);
}

} // namespace refrain

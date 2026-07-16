#include "settings.h"

#include <windows.h>
#include <mmsystem.h>
#include <objbase.h>
#include <ole2.h>
#include <shellapi.h>
#include <foobar2000/SDK/foobar2000.h>

#include <utility>
#include <vector>

namespace foocrate {
namespace {
class Instance : public preferences_page_instance {
public:
    Instance(HWND parent, preferences_page_callback::ptr callback) : m_callback(std::move(callback)), m_dependencies(detectDependencies()) {
        m_window = CreateWindowExW(0, L"STATIC", nullptr, WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, parent, nullptr, core_api::get_my_instance(), nullptr);
        SetWindowLongPtrW(m_window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this)); m_original = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(m_window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&Instance::proc)));
        m_dpi = GetDpiForWindow(m_window); createFont();
        for (std::size_t i = 0; i < m_dependencies.size(); ++i) { const auto& d = m_dependencies[i]; auto text = d.displayName + L": " + (d.detected ? L"Detected" : L"Not detected"); if (!d.version.empty()) text += L" " + d.version; m_labels.push_back(add(L"STATIC", text.c_str(), SS_LEFT, 0)); m_buttons.push_back(add(L"BUTTON", L"Open official page", BS_PUSHBUTTON | WS_TABSTOP, 6000 + static_cast<int>(i))); }
        m_note = add(L"STATIC", L"Read-only detection. FooCrate never downloads or updates these components.", SS_LEFT, 0); layout();
    }
    ~Instance() { if (m_ownedFont) DeleteObject(m_ownedFont); }
    t_uint32 get_state() override { return preferences_state::dark_mode_supported; } HWND get_wnd() override { return m_window; } void apply() override {} void reset() override {}
private:
    static LRESULT CALLBACK proc(HWND w, UINT m, WPARAM wp, LPARAM lp) { auto* s = reinterpret_cast<Instance*>(GetWindowLongPtrW(w, GWLP_USERDATA)); if (!s || !s->m_original) return DefWindowProcW(w,m,wp,lp); if (m == WM_SIZE) { s->layout(); return 0; } if (m == WM_DPICHANGED || m == WM_DPICHANGED_AFTERPARENT || m == WM_SETTINGCHANGE) { s->updateDpi(GetDpiForWindow(w)); return 0; } if (m == WM_COMMAND && HIWORD(wp) == BN_CLICKED) { const auto i = LOWORD(wp) - 6000; if (i >= 0 && static_cast<std::size_t>(i) < s->m_dependencies.size()) ShellExecuteW(w,L"open",s->m_dependencies[static_cast<std::size_t>(i)].officialUrl.c_str(),nullptr,nullptr,SW_SHOWNORMAL); return 0; } return CallWindowProcW(s->m_original,w,m,wp,lp); }
    HWND add(const wchar_t* c,const wchar_t* t,DWORD st,int id){auto w=CreateWindowExW(0,c,t,WS_CHILD|WS_VISIBLE|st,0,0,0,0,m_window,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),core_api::get_my_instance(),nullptr);SendMessageW(w,WM_SETFONT,reinterpret_cast<WPARAM>(m_font),TRUE);return w;}
    int scaled(int value) const { return MulDiv(value, static_cast<int>(m_dpi), 96); }
    void createFont() { if (m_ownedFont) { DeleteObject(m_ownedFont); m_ownedFont = nullptr; } NONCLIENTMETRICSW metrics{sizeof(metrics)}; if (SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0, m_dpi)) m_ownedFont = CreateFontIndirectW(&metrics.lfMessageFont); m_font = m_ownedFont ? m_ownedFont : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT)); }
    void updateDpi(UINT dpi) { m_dpi = dpi ? dpi : 96U; createFont(); EnumChildWindows(m_window, [](HWND child, LPARAM font) -> BOOL { SendMessageW(child, WM_SETFONT, static_cast<WPARAM>(font), TRUE); return TRUE; }, reinterpret_cast<LPARAM>(m_font)); layout(); RedrawWindow(m_window, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN); }
    void layout() const { RECT r{}; GetClientRect(m_window,&r); const auto width=static_cast<int>(r.right); for(std::size_t i=0;i<m_labels.size();++i){MoveWindow(m_labels[i],scaled(16),scaled(20+static_cast<int>(i)*42),std::max(1,width-scaled(190)),scaled(24),TRUE);MoveWindow(m_buttons[i],std::max(scaled(16),width-scaled(166)),scaled(14+static_cast<int>(i)*42),scaled(150),scaled(28),TRUE);}MoveWindow(m_note,scaled(16),scaled(154),std::max(1,width-scaled(32)),scaled(40),TRUE);}
    preferences_page_callback::ptr m_callback; std::vector<DependencyStatus> m_dependencies; std::vector<HWND> m_labels,m_buttons; HWND m_window{},m_note{}; WNDPROC m_original{}; HFONT m_font{},m_ownedFont{}; UINT m_dpi{96};
};
class Page : public preferences_page_v3 { public: const char* get_name() override{return "Dependencies";} GUID get_guid() override{return kDependenciesPreferencesPageGuid;} GUID get_parent_guid() override{return kPreferencesPageGuid;} preferences_page_instance::ptr instantiate(HWND p,preferences_page_callback::ptr c) override{return fb2k::service_new<Instance>(p,std::move(c));}};
preferences_page_factory_t<Page> g_factory;
}
}

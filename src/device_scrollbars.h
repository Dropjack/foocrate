#pragma once
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <algorithm>
#include "theme_model.h"
#include "playback_layout.h"

namespace foocrate {
// Keep native scrolling/accessibility, but replace non-client chrome and tracking.
class DeviceScrollbars final {
public:
    enum class Kind { tree, table, text };
    void attach(HWND window, Kind kind) {
        window_=window; kind_=kind;
        SetWindowSubclass(window_,proc,scrollId,reinterpret_cast<DWORD_PTR>(this));
    }
    void theme(UINT dpi, const ThemePalette& palette) {
        dpi_=dpi;
        background_=RGB(palette.backgroundBase.red,palette.backgroundBase.green,palette.backgroundBase.blue);
        thumb_=RGB(palette.textDisabled.red,palette.textDisabled.green,palette.textDisabled.blue);
        paint();
    }
private:
    static constexpr UINT_PTR scrollId=0x46504353;
    HWND window_{};
    Kind kind_{};
    UINT dpi_{96};
    COLORREF background_{},thumb_{};
    bool shown_{},dragging_{};
    int axis_{SB_VERT},offset_{};
    struct Bar { RECT track{},thumb{}; SCROLLINFO info{sizeof(SCROLLINFO),SIF_ALL}; };
    bool bar(int axis, Bar& value) const {
        SCROLLBARINFO native{sizeof(native)};
        if (!GetScrollBarInfo(window_,axis==SB_VERT?OBJID_VSCROLL:OBJID_HSCROLL,&native)
            || (native.rgstate[0] & (STATE_SYSTEM_INVISIBLE|STATE_SYSTEM_OFFSCREEN))) return false;
        if (!GetScrollInfo(window_,axis,&value.info)) return false;
        RECT window{}; GetWindowRect(window_,&window);
        value.track=native.rcScrollBar; OffsetRect(&value.track,-window.left,-window.top);
        value.thumb=value.track;
        const int length=axis==SB_VERT?value.track.bottom-value.track.top:value.track.right-value.track.left;
        const int range=value.info.nMax-value.info.nMin+1;
        if (length<=0) return false;
        if (range<=0 || range<=static_cast<int>(value.info.nPage)) { value.thumb={}; return true; }
        const int extent=std::min(length,std::max(MulDiv(24,static_cast<int>(dpi_),96),
            static_cast<int>(static_cast<double>(length)*value.info.nPage/range)));
        const int maximum=std::max(1,range-static_cast<int>(value.info.nPage));
        const int start=static_cast<int>(static_cast<double>(length-extent)*(value.info.nPos-value.info.nMin)/maximum);
        if (axis==SB_VERT) { value.thumb.top+=start; value.thumb.bottom=value.thumb.top+extent; }
        else { value.thumb.left+=start; value.thumb.right=value.thumb.left+extent; }
        return true;
    }
    void paint() const {
        if (!window_ || !IsWindowVisible(window_)) return;
        auto dc=GetWindowDC(window_); if (!dc) return;
        auto bg=CreateSolidBrush(background_); auto ink=CreateSolidBrush(thumb_);
        for (int axis : {SB_VERT,SB_HORZ}) {
            Bar b; if (!bar(axis,b)) continue;
            FillRect(dc,&b.track,bg);
            if (!shown_ || IsRectEmpty(&b.thumb)) continue;
            auto r=b.thumb;
            const int width=std::max(1,MulDiv(static_cast<int>(layout::InteriorChromeMetrics::scrollbarVisualWidth),static_cast<int>(dpi_),96));
            if (axis==SB_VERT) { const auto center=(r.left+r.right)/2; r.left=center-width/2; r.right=r.left+width; }
            else { const auto center=(r.top+r.bottom)/2; r.top=center-width/2; r.bottom=r.top+width; }
            FillRect(dc,&r,ink);
        }
        // Native size-box corner must not retain the old gray chrome.
        Bar v,h;
        if (bar(SB_VERT,v) && bar(SB_HORZ,h)) {
            RECT corner{v.track.left,h.track.top,v.track.right,h.track.bottom}; FillRect(dc,&corner,bg);
        }
        DeleteObject(ink); DeleteObject(bg); ReleaseDC(window_,dc);
    }
    void reveal() {
        shown_=true;
        SetTimer(window_,scrollId,layout::InteriorChromeMetrics::scrollbarHideDelayMs,nullptr);
    }
    void moveTo(int axis, int position) {
        Bar b; if (!bar(axis,b)) return;
        const int maximum=std::max(b.info.nMin,b.info.nMax-static_cast<int>(b.info.nPage)+1);
        position=std::clamp(position,b.info.nMin,maximum);
        const int delta=position-b.info.nPos;
        if (kind_==Kind::table) {
            if (axis==SB_VERT) {
                RECT row{}; int height=std::max(1,MulDiv(20,static_cast<int>(dpi_),96));
                if (ListView_GetItemRect(window_,ListView_GetTopIndex(window_),&row,LVIR_BOUNDS)) height=std::max(1L,row.bottom-row.top);
                ListView_Scroll(window_,0,delta*height);
            } else ListView_Scroll(window_,delta,0);
        } else if (kind_==Kind::text) {
            SendMessageW(window_,EM_LINESCROLL,axis==SB_HORZ?delta:0,axis==SB_VERT?delta:0);
        } else {
            SendMessageW(window_,axis==SB_VERT?WM_VSCROLL:WM_HSCROLL,MAKEWPARAM(SB_THUMBPOSITION,position),0);
        }
        paint();
    }
    POINT windowPoint() const {
        POINT p{}; GetCursorPos(&p); RECT r{}; GetWindowRect(window_,&r); p.x-=r.left; p.y-=r.top; return p;
    }
    static LRESULT CALLBACK proc(HWND wnd,UINT msg,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR data) {
        auto& self=*reinterpret_cast<DeviceScrollbars*>(data);
        if (msg==WM_NCDESTROY) {
            KillTimer(wnd,scrollId); RemoveWindowSubclass(wnd,proc,id); self.window_=nullptr;
            return DefSubclassProc(wnd,msg,wp,lp);
        }
        if (msg==WM_NCPAINT) { self.paint(); return 0; }
        if (msg==WM_TIMER && wp==scrollId) {
            if (!self.dragging_) { self.shown_=false; KillTimer(wnd,scrollId); self.paint(); }
            return 0;
        }
        if (msg==WM_NCMOUSEMOVE && (wp==HTVSCROLL || wp==HTHSCROLL)) { self.reveal(); self.paint(); return 0; }
        if ((msg==WM_NCLBUTTONDOWN || msg==WM_NCLBUTTONDBLCLK) && (wp==HTVSCROLL || wp==HTHSCROLL)) {
            self.axis_=wp==HTVSCROLL?SB_VERT:SB_HORZ;
            Bar b; if (!self.bar(self.axis_,b) || IsRectEmpty(&b.thumb)) return 0;
            const auto pt=self.windowPoint(); const int coord=self.axis_==SB_VERT?pt.y:pt.x;
            const int start=self.axis_==SB_VERT?b.thumb.top:b.thumb.left;
            const int end=self.axis_==SB_VERT?b.thumb.bottom:b.thumb.right;
            self.reveal();
            if (coord>=start && coord<end) { self.dragging_=true; self.offset_=coord-start; SetCapture(wnd); }
            else self.moveTo(self.axis_,b.info.nPos+(coord<start?-1:1)*std::max(1,static_cast<int>(b.info.nPage)));
            self.paint(); return 0;
        }
        if (msg==WM_MOUSEMOVE && self.dragging_) {
            Bar b; if (!self.bar(self.axis_,b)) return 0;
            auto pt=self.windowPoint();
            const int start=self.axis_==SB_VERT?b.track.top:b.track.left;
            const int length=self.axis_==SB_VERT?b.track.bottom-b.track.top:b.track.right-b.track.left;
            const int extent=self.axis_==SB_VERT?b.thumb.bottom-b.thumb.top:b.thumb.right-b.thumb.left;
            const int coord=(self.axis_==SB_VERT?pt.y:pt.x)-self.offset_-start;
            const int range=std::max(0,b.info.nMax-b.info.nMin-static_cast<int>(b.info.nPage)+1);
            self.moveTo(self.axis_,b.info.nMin+static_cast<int>(static_cast<double>(std::clamp(coord,0,std::max(0,length-extent)))*range/std::max(1,length-extent)));
            return 0;
        }
        if (msg==WM_LBUTTONUP && self.dragging_) { self.dragging_=false; ReleaseCapture(); self.reveal(); return 0; }
        if (msg==WM_CAPTURECHANGED) self.dragging_=false;
        if (msg==WM_MOUSEMOVE || msg==WM_MOUSEWHEEL || msg==WM_MOUSEHWHEEL || msg==WM_KEYDOWN) self.reveal();
        const auto result=DefSubclassProc(wnd,msg,wp,lp);
        if (msg==WM_PAINT || msg==WM_SIZE || msg==WM_VSCROLL || msg==WM_HSCROLL || msg==WM_MOUSEWHEEL
            || msg==WM_MOUSEHWHEEL || msg==WM_KEYDOWN || msg==WM_NCACTIVATE || msg==WM_MOUSEMOVE) self.paint();
        return result;
    }
};
}

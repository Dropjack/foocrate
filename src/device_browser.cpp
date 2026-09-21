#include <foopodbridge/service_readonly.h>
#include "device_browser.h"
#include "device_browser_model.h"
#include "device_scrollbars.h"
#include <commctrl.h>
#include <windowsx.h>
#include <algorithm>
#include <atomic>
#include <sstream>

namespace foocrate {
namespace {
namespace c = foopodbridge::contract;
constexpr UINT updateMessage = WM_APP + 0x450, projectMessage = WM_APP + 0x451;
constexpr GUID splitGuid{0x4a5fc64a,0x1336,0x4a58,{0x89,0x52,0xaf,0x64,0x83,0xd9,0x1f,0x27}};
cfg_int sidebarSplit(splitGuid,660);
std::wstring wide(const char* value) {
    if (!value || !*value) return {};
    const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value, -1, nullptr, 0);
    if (!count) return L"Text unavailable";
    std::wstring result(static_cast<std::size_t>(count), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value, -1, result.data(), count);
    result.pop_back(); return result;
}
struct Lifetime { HWND window{}; std::atomic<bool> pending{}, unavailable{}; };
class Callback : public c::device_event_callback_v1 {
public:
    explicit Callback(std::weak_ptr<Lifetime> value) : lifetime_(std::move(value)) {}
    void on_snapshot_generation_changed(std::uint64_t) noexcept override { post(); }
    void on_provider_unavailable() noexcept override { if (auto p=lifetime_.lock()) p->unavailable=true; post(); }
private:
    void post() noexcept {
        if (auto p = lifetime_.lock(); p && p->window && !p->pending.exchange(true)) PostMessageW(p->window, updateMessage, 0, 0);
    }
    std::weak_ptr<Lifetime> lifetime_;
};
COLORREF color(RgbColor c) { return RGB(c.red,c.green,c.blue); }
void position(HWND window, RECT r, bool visible) {
    if (!window) return;
    SetWindowPos(window, nullptr, r.left,r.top,std::max(0L,r.right-r.left),std::max(0L,r.bottom-r.top),
        SWP_NOZORDER|SWP_NOACTIVATE|(visible ? SWP_SHOWWINDOW : SWP_HIDEWINDOW));
}
LRESULT CALLBACK readOnlyKeys(HWND wnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR id, DWORD_PTR) {
    if (msg == WM_KEYDOWN) {
        if (wp == VK_TAB) {
            auto target = GetNextDlgTabItem(GetParent(wnd), wnd, (GetKeyState(VK_SHIFT)&0x8000)!=0);
            if (target) SetFocus(target); return 0;
        }
        if (wp == VK_DELETE || wp == VK_F2 || wp == VK_RETURN || wp == VK_SPACE
            || (wp == 'V' && (GetKeyState(VK_CONTROL)&0x8000))) return 0;
    }
    if (msg == WM_NCDESTROY) RemoveWindowSubclass(wnd, readOnlyKeys, id);
    return DefSubclassProc(wnd,msg,wp,lp);
}
}

struct DeviceBrowser::Impl {
    HWND parent{}, tree{}, table{}, text{}, divider{};
    DeviceScrollbars treeScroll, tableScroll, textScroll;
    LONG sidebarHeight{};
    HFONT font{}; HBRUSH brush{}; UINT dpi{}; COLORREF background{}, foreground{};
    std::function<void(int)> changed;
    std::shared_ptr<Lifetime> life;
    c::device_provider_readonly_v1::ptr provider;
    c::subscription_v1::ptr subscription;
    bool servicePresent{}, rebuilding{}, projecting{};
    devices::Context context;
    struct Node {
        c::device_snapshot_readonly_v1::ptr device;
        c::library_snapshot_v1::ptr library;
        std::string token;
        std::uint64_t generation{}, playlistId{};
        std::uint32_t playlistIndex{}, members{};
        bool playlist{};
    };
    std::vector<Node> nodes;
    std::optional<Node> selected;
    devices::LibraryIndex library;
    std::vector<std::size_t> rows;
    std::uint32_t trackCursor{}, memberCursor{};
    std::wstring status;
    ~Impl() { destroy(); }
    void destroy() {
        if (life) life->window = nullptr;
        if (subscription.is_valid()) subscription->cancel();
        subscription.release(); provider.release(); selected.reset(); nodes.clear();
        for (auto h : {tree,table,text,divider}) if (h && IsWindow(h)) DestroyWindow(h);
        tree=table=text=divider=nullptr;
        if (font) DeleteObject(font); font=nullptr;
        if (brush) DeleteObject(brush); brush=nullptr;
        life.reset(); parent=nullptr; servicePresent=false; context={}; library.clear(); rows.clear(); projecting=false;
    }
    void signal() { if (changed) changed(context.tab); }
    static LRESULT CALLBACK splitProc(HWND wnd,UINT msg,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR data) {
        auto* self=reinterpret_cast<Impl*>(data);
        if (msg==WM_SETCURSOR) { SetCursor(LoadCursorW(nullptr,IDC_SIZENS)); return TRUE; }
        if (msg==WM_LBUTTONDOWN) { SetCapture(wnd); return 0; }
        if (msg==WM_MOUSEMOVE && GetCapture()==wnd) {
            POINT pt{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)}; MapWindowPoints(wnd,self->parent,&pt,1);
            if (self->sidebarHeight>0) { sidebarSplit=std::clamp(static_cast<int>(pt.y*1000/self->sidebarHeight),200,850); self->signal(); }
            return 0;
        }
        if (msg==WM_LBUTTONUP && GetCapture()==wnd) { ReleaseCapture(); return 0; }
        if (msg==WM_NCDESTROY) RemoveWindowSubclass(wnd,splitProc,id);
        return DefSubclassProc(wnd,msg,wp,lp);
    }
    static LRESULT CALLBACK lowerMiddleClick(HWND window, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR id, DWORD_PTR data) {
        if (msg == WM_MBUTTONUP) {
            auto* self = reinterpret_cast<Impl*>(data);
            POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            MapWindowPoints(window, self->parent, &pt, 1);
            SendMessageW(self->parent, WM_MBUTTONUP, wp, MAKELPARAM(pt.x,pt.y));
            return 0;
        }
        if (msg == WM_NCDESTROY) RemoveWindowSubclass(window, lowerMiddleClick, id);
        return DefSubclassProc(window,msg,wp,lp);
    }
    void clearRows() { projecting=false; library.clear(); rows.clear(); if (table) ListView_SetItemCountEx(table,0,0); }
    HTREEITEM insert(HTREEITEM parentItem, const std::wstring& title, LPARAM data) {
        TVINSERTSTRUCTW item{}; item.hParent=parentItem; item.hInsertAfter=TVI_LAST;
        item.item.mask=TVIF_TEXT|TVIF_PARAM; item.item.pszText=const_cast<wchar_t*>(title.c_str()); item.item.lParam=data;
        return TreeView_InsertItem(tree,&item);
    }
    void overviewText() {
        std::wstring value=L"Device overview\r\n\r\n"+status;
        if (selected && provider.is_valid() && provider->is_current(selected->token.c_str(),selected->generation,selected->device->get_revision())) {
            pfc::string8 description; selected->device->get_status_description(description);
            std::uint64_t total{},free{};
            if (selected->device->get_capacity(total,free)) {
                std::wostringstream summary; summary.setf(std::ios::fixed); summary.precision(2);
                summary << L"\r\nCapacity: " << static_cast<double>(total)/1073741824.0 << L" GiB\r\nFree: " << static_cast<double>(free)/1073741824.0 << L" GiB";
                value+=summary.str();
            }
            value+=L"\r\n\r\n"+wide(description.c_str());
        }
        SetWindowTextW(text,value.c_str());
    }
    void refresh() {
        if (life && life->unavailable) {
            provider.release(); servicePresent=false; context.leave(); selected.reset();
        }
        rebuilding=true; clearRows(); nodes.clear(); TreeView_DeleteAllItems(tree);
        const auto root=insert(TVI_ROOT,L"Devices",-1);
        HTREEITEM restore{};
        if (!provider.is_valid()) { insert(root,L"Component version does not support browsing",-1); status=L"Device service unavailable"; }
        else {
            pfc::string8 providerStatus; provider->get_provider_status(providerStatus); status=wide(providerStatus.c_str());
            c::device_snapshot_list_v1::ptr list; provider->get_snapshots(list);
            if (list.is_valid()) for (std::uint32_t i=0; i<list->get_count(); ++i) {
                c::device_snapshot_v1::ptr base; Node node;
                if (!list->get_item(i,base) || !base->service_query_t(node.device)) continue;
                pfc::string8 name,token; node.device->get_display_name(name); node.device->get_stable_id(token);
                node.token=token.c_str(); node.generation=node.device->get_generation(); node.device->get_library(node.library);
                const auto deviceIndex=nodes.size(); nodes.push_back(node);
                const auto device=insert(root,wide(name.c_str()),static_cast<LPARAM>(deviceIndex));
                if (node.library.is_valid()) {
                    const auto libraryItem=insert(device,L"Library  ("+std::to_wstring(node.library->get_track_count())+L")",static_cast<LPARAM>(deviceIndex));
                    if (context.active && selected && selected->token==node.token && selected->generation==node.generation && !selected->playlist) restore=libraryItem;
                    const auto playlists=insert(device,L"Playlists",static_cast<LPARAM>(deviceIndex));
                    for (std::uint32_t p=0;p<node.library->get_playlist_count();++p) {
                        std::uint64_t id{}; c::read_playlist_kind kind{}; std::uint32_t count{}; pfc::string8 title;
                        if (!node.library->get_playlist(p,id,kind,count,title) || kind==c::read_playlist_kind::master) continue;
                        auto child=node; child.playlist=true; child.playlistId=id; child.playlistIndex=p; child.members=count;
                        auto label=wide(title.c_str())+L"  ("+std::to_wstring(count)+L")";
                        const auto item=insert(playlists,label,static_cast<LPARAM>(nodes.size())); nodes.push_back(child);
                        if (context.active && selected && selected->token==node.token && selected->generation==node.generation && selected->playlist && selected->playlistId==id) restore=item;
                    }
                    TreeView_Expand(tree,playlists,TVE_EXPAND);
                } else if (context.active && selected && selected->token==node.token && selected->generation==node.generation) restore=device;
                TreeView_Expand(tree,device,TVE_EXPAND);
            }
            if (nodes.empty()) insert(root,status,-1);
        }
        TreeView_Expand(tree,root,TVE_EXPAND);
        rebuilding=false;
        if (restore) TreeView_SelectItem(tree,restore);
        else if (context.active) {
            const bool scanning=provider.is_valid() && provider->is_scanning();
            if (!scanning) selected.reset();
            status=scanning?L"Reading devices...":L"Device disconnected or snapshot expired";
        }
        overviewText(); signal();
    }
    void choose(std::size_t index) {
        if (index>=nodes.size()) return;
        selected=nodes[index]; context.enter(); clearRows();
        status=L"Reading device library...";
        if (selected->library.is_valid()) {
            trackCursor=memberCursor=0; projecting=true;
            PostMessageW(parent,projectMessage,0,0);
        } else { pfc::string8 value; selected->device->get_status_description(value); status=wide(value.c_str()); }
        overviewText(); signal();
    }
    bool current() const {
        return selected && provider.is_valid() && provider->is_current(selected->token.c_str(),selected->generation,selected->device->get_revision());
    }
    void project() {
        if (!projecting || !selected) return;
        if (!current()) { clearRows(); status=L"Device disconnected or snapshot expired"; overviewText(); InvalidateRect(table,nullptr,TRUE); return; }
        const auto source=selected->library;
        const auto total=source->get_track_count();
        if (total>1000000) throw std::runtime_error("library limit");
        unsigned budget=128;
        while (trackCursor<total && budget--) {
            devices::Track track; std::uint64_t pid{}; c::read_media_kind kind{}; pfc::string8 title,artist,album;
            if (!source->get_track(trackCursor,track.id,pid,kind)
                || !source->get_track_text(trackCursor,c::track_text::title,title)
                || !source->get_track_text(trackCursor,c::track_text::artist,artist)
                || !source->get_track_text(trackCursor,c::track_text::album,album)) throw std::runtime_error("invalid track");
            track.title=wide(title.c_str()); track.artist=wide(artist.c_str()); track.album=wide(album.c_str());
            if (!library.append(std::move(track))) throw std::runtime_error("duplicate track ID");
            if (!selected->playlist) rows.push_back(trackCursor);
            ++trackCursor;
        }
        if (trackCursor==total && selected->playlist) {
            if (selected->members>1000000) throw std::runtime_error("playlist limit");
            budget=128;
            while (memberCursor<selected->members && budget--) {
                std::uint32_t id{};
                if (!source->get_playlist_member(selected->playlistIndex,memberCursor++,id)) throw std::runtime_error("invalid member");
                auto index=library.find(id); if (!index) throw std::runtime_error("unknown member"); rows.push_back(*index);
            }
        }
        if (trackCursor<total || (selected->playlist && memberCursor<selected->members)) { PostMessageW(parent,projectMessage,0,0); return; }
        projecting=false; status=rows.empty()?L"0 tracks":L"Read-only";
        ListView_SetItemCountEx(table,static_cast<int>(rows.size()),LVSICF_NOINVALIDATEALL);
        InvalidateRect(table,nullptr,TRUE); overviewText();
    }
};

DeviceBrowser::DeviceBrowser() : impl_(std::make_unique<Impl>()) {}
DeviceBrowser::~DeviceBrowser() = default;
void DeviceBrowser::create(HWND parent,std::function<void(int)> changed) {
    auto& p=*impl_; p.parent=parent; p.changed=std::move(changed); p.life=std::make_shared<Lifetime>(); p.life->window=parent;
    service_enum_t<c::service_v1> services; c::service_v1::ptr service;
    while (services.next(service)) {
        p.servicePresent=true;
        c::device_provider_v1::ptr base;
        if (service->get_contract_major()==1 && service->get_device_provider(base) && base.is_valid()) base->service_query_t(p.provider);
        break;
    }
    if (!p.servicePresent) return;
    const auto instance=core_api::get_my_instance();
    p.tree=CreateWindowExW(0,WC_TREEVIEWW,L"iPod devices",WS_CHILD|WS_TABSTOP|TVS_HASBUTTONS|TVS_HASLINES|TVS_LINESATROOT|TVS_SHOWSELALWAYS,0,0,0,0,parent,nullptr,instance,nullptr);
    p.table=CreateWindowExW(0,WC_LISTVIEWW,L"Device library",WS_CHILD|WS_TABSTOP|LVS_REPORT|LVS_OWNERDATA|LVS_SHOWSELALWAYS,0,0,0,0,parent,nullptr,instance,nullptr);
    p.text=CreateWindowExW(0,L"EDIT",L"",WS_CHILD|WS_TABSTOP|ES_READONLY|ES_MULTILINE|ES_AUTOVSCROLL|WS_VSCROLL,0,0,0,0,parent,nullptr,instance,nullptr);
    p.divider=CreateWindowExW(0,L"STATIC",L"",WS_CHILD|SS_NOTIFY,0,0,0,0,parent,nullptr,instance,nullptr);
    if (!p.tree || !p.table || !p.text || !p.divider) throw std::runtime_error("Device controls unavailable");
    SetWindowSubclass(p.divider,Impl::splitProc,1,reinterpret_cast<DWORD_PTR>(&p));
    ListView_SetExtendedListViewStyle(p.table,LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER);
    int i=0; for (const auto label : {L"Title",L"Artist",L"Album"}) {
        LVCOLUMNW col{}; col.mask=LVCF_TEXT|LVCF_WIDTH|LVCF_SUBITEM; col.pszText=const_cast<wchar_t*>(label); col.cx=200; col.iSubItem=i;
        ListView_InsertColumn(p.table,i++,&col);
    }
    p.treeScroll.attach(p.tree, DeviceScrollbars::Kind::tree);
    p.tableScroll.attach(p.table, DeviceScrollbars::Kind::table);
    p.textScroll.attach(p.text, DeviceScrollbars::Kind::text);
    SetWindowSubclass(p.text,Impl::lowerMiddleClick,2,reinterpret_cast<DWORD_PTR>(&p));
    SetWindowSubclass(p.table,readOnlyKeys,1,0);
    if (p.provider.is_valid()) p.provider->subscribe(new service_impl_t<Callback>(p.life),p.subscription);
    p.refresh();
}
void DeviceBrowser::destroy() { impl_->destroy(); }
bool DeviceBrowser::available() const { return impl_->servicePresent; }
bool DeviceBrowser::active() const { return impl_->context.active; }
bool DeviceBrowser::overview() const { return impl_->context.overview(); }
float DeviceBrowser::sidebarBottom(float height,float minimum) const {
    return std::max(minimum,height*static_cast<float>(std::clamp(static_cast<int>(sidebarSplit.get()),200,850))/1000.0F);
}
void DeviceBrowser::leave() {
    auto& p=*impl_; if (!p.context.active) return;
    p.context.leave(); p.selected.reset(); p.clearRows(); p.signal();
}
void DeviceBrowser::cycle(bool lyricsAvailable) { impl_->context.cycle(lyricsAvailable); impl_->signal(); }
void DeviceBrowser::baseTab(int tab) { if (!overview()) impl_->context.choose(tab); }
void DeviceBrowser::layout(RECT sidebar,RECT table,RECT lower,bool visible,UINT dpi,const ThemePalette& palette) {
    auto& p=*impl_; if (!p.tree) return;
    const auto bg=color(palette.backgroundBase), fg=color(palette.textPrimary);
    if (!p.brush || bg!=p.background || fg!=p.foreground) {
        if (p.brush) DeleteObject(p.brush); p.brush=CreateSolidBrush(bg); p.background=bg; p.foreground=fg;
        TreeView_SetBkColor(p.tree,bg); TreeView_SetTextColor(p.tree,fg);
        ListView_SetBkColor(p.table,bg); ListView_SetTextBkColor(p.table,bg); ListView_SetTextColor(p.table,fg);
        InvalidateRect(p.text,nullptr,TRUE);
    }
    if (dpi!=p.dpi) {
        auto old=p.font; NONCLIENTMETRICSW metrics{sizeof(metrics)};
        SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS,sizeof(metrics),&metrics,0,dpi);
        p.font=CreateFontIndirectW(&metrics.lfMessageFont); p.dpi=dpi;
        for (auto h : {p.tree,p.table,p.text}) SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(p.font),TRUE);
        if (old) DeleteObject(old);
    }
    visible=visible&&p.servicePresent;
    p.sidebarHeight=sidebar.bottom;
    auto divider=sidebar; divider.bottom=std::min(sidebar.bottom,sidebar.top+MulDiv(5,static_cast<int>(dpi),96));
    position(p.divider,divider,visible); sidebar.top=divider.bottom;
    position(p.tree,sidebar,visible);
    position(p.table,table,visible&&active());
    position(p.text,lower,visible&&overview());
    for (auto* scroll : {&p.treeScroll,&p.tableScroll,&p.textScroll}) scroll->theme(dpi,palette);
    const auto width=std::max(180L,table.right-table.left-GetSystemMetricsForDpi(SM_CXVSCROLL,dpi));
    ListView_SetColumnWidth(p.table,0,static_cast<int>(width*45/100));
    ListView_SetColumnWidth(p.table,1,static_cast<int>(width*27/100));
    ListView_SetColumnWidth(p.table,2,static_cast<int>(width*28/100));
}
bool DeviceBrowser::message(UINT msg,WPARAM wp,LPARAM lp,LRESULT& result) {
    auto& p=*impl_; if (!p.tree) return false; result=0;
    try {
        if (msg==updateMessage) { p.life->pending=false; p.refresh(); return true; }
        if (msg==projectMessage) { p.project(); return true; }
        if ((msg==WM_CTLCOLOREDIT || msg==WM_CTLCOLORSTATIC) && reinterpret_cast<HWND>(lp)==p.text) {
            auto dc=reinterpret_cast<HDC>(wp); SetTextColor(dc,p.foreground); SetBkColor(dc,p.background); result=reinterpret_cast<LRESULT>(p.brush); return true;
        }
        if (msg==WM_CONTEXTMENU && reinterpret_cast<HWND>(wp)==p.tree) {
            auto menu=CreatePopupMenu(); AppendMenuW(menu,MF_STRING|(p.provider.is_valid()&&!p.provider->is_scanning()?0:MF_GRAYED),1,L"Refresh");
            POINT pt{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)}; if (pt.x==-1) { RECT r{}; GetWindowRect(p.tree,&r); pt={r.left,r.top}; }
            const auto command=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTBUTTON,pt.x,pt.y,0,p.parent,nullptr); DestroyMenu(menu);
            if (command==1 && p.provider.is_valid()) p.provider->refresh(); return true;
        }
        if (msg!=WM_NOTIFY) return false;
        auto header=reinterpret_cast<NMHDR*>(lp);
        if (header->hwndFrom==ListView_GetHeader(p.table) && header->code==NM_CUSTOMDRAW) {
            auto draw=reinterpret_cast<NMCUSTOMDRAW*>(lp);
            if (draw->dwDrawStage==CDDS_PREPAINT) { result=CDRF_NOTIFYITEMDRAW; return true; }
            if (draw->dwDrawStage==CDDS_ITEMPREPAINT) {
                wchar_t label[128]{}; HDITEMW item{}; item.mask=HDI_TEXT; item.pszText=label; item.cchTextMax=128;
                Header_GetItem(header->hwndFrom,static_cast<int>(draw->dwItemSpec),&item);
                FillRect(draw->hdc,&draw->rc,p.brush); SetBkMode(draw->hdc,TRANSPARENT); SetTextColor(draw->hdc,p.foreground);
                auto r=draw->rc; r.left+=6; DrawTextW(draw->hdc,label,-1,&r,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
                result=CDRF_SKIPDEFAULT; return true;
            }
        }
        if (header->hwndFrom==p.tree && header->code==TVN_SELCHANGEDW) {
            auto item=reinterpret_cast<NMTREEVIEWW*>(lp);
            if (!p.rebuilding && item->itemNew.lParam>=0) p.choose(static_cast<std::size_t>(item->itemNew.lParam)); return true;
        }
        if (header->hwndFrom==p.table && header->code==LVN_GETDISPINFOW) {
            auto info=reinterpret_cast<NMLVDISPINFOW*>(lp);
            if (!p.current()) {
                if ((info->item.mask&LVIF_TEXT) && info->item.cchTextMax>0) info->item.pszText[0]=L'\0';
                if (!p.life->pending.exchange(true)) PostMessageW(p.parent,updateMessage,0,0);
                return true;
            }
            if ((info->item.mask&LVIF_TEXT) && info->item.iItem>=0 && static_cast<std::size_t>(info->item.iItem)<p.rows.size()) {
                const auto& track=p.library.tracks[p.rows[static_cast<std::size_t>(info->item.iItem)]];
                const auto& value=info->item.iSubItem==0?track.title:info->item.iSubItem==1?track.artist:track.album;
                lstrcpynW(info->item.pszText,value.c_str(),info->item.cchTextMax);
            } return true;
        }
        if (header->hwndFrom==p.table && header->code==NM_CUSTOMDRAW) {
            auto draw=reinterpret_cast<NMLVCUSTOMDRAW*>(lp);
            if (draw->nmcd.dwDrawStage==CDDS_PREPAINT && p.rows.empty()) {
                RECT r{}; GetClientRect(p.table,&r); r.top+=MulDiv(40,static_cast<int>(p.dpi),96); r.left+=12; r.right-=12;
                SetTextColor(draw->nmcd.hdc,p.foreground); SetBkMode(draw->nmcd.hdc,TRANSPARENT);
                DrawTextW(draw->nmcd.hdc,p.status.c_str(),-1,&r,DT_LEFT|DT_WORDBREAK|DT_NOPREFIX);
            }
        }
    } catch (...) { p.clearRows(); p.status=L"Device data unavailable. Refresh to retry."; p.overviewText(); }
    return false;
}
}

#include <windows.h>
#include <commctrl.h>
#include "luna/audio.hpp"
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace {
constexpr COLORREF background = RGB(245,247,250), ink = RGB(23,35,54), muted = RGB(91,105,123), teal = RGB(0,132,120);
enum { Microphone = 101, Output, Refresh, Suppress, OutputMode, DeepFilter, Strength, Start };
int scale = 96;
int px(int n) { return MulDiv(n, scale, 96); }
std::wstring wide(const std::string& s) {
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (!n) return L"";
    std::wstring w(static_cast<std::size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), n); w.pop_back(); return w;
}
struct App {
    HWND window{}, mic{}, output{}, refresh{}, suppress{}, outputMode{}, deepFilter{}, strength{}, start{};
    HFONT normal{}, heading{}, small{}, bold{};
    HBRUSH bg = CreateSolidBrush(background), white = CreateSolidBrush(RGB(255,255,255));
    std::unique_ptr<luna::LiveSession> session;
    luna::DeviceList devices;
    bool active = false;
    std::wstring status = L"Ready. Select your microphone, then start.", native;
    luna::MetricsSnapshot metrics;
    double cpu = 0;
    ULONGLONG lastCpu = 0, lastTick = 0;
    ~App() { if (session) session->stop(); for (auto f : {normal,heading,small,bold}) if(f) DeleteObject(f); DeleteObject(bg); DeleteObject(white); }
    HWND control(const wchar_t* kind, const wchar_t* text, DWORD style, int x, int y, int w, int h, int id) {
        auto c = CreateWindowExW(0, kind, text, WS_CHILD|WS_VISIBLE|WS_TABSTOP|style,
            px(x),px(y),px(w),px(h),window,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);
        SendMessageW(c,WM_SETFONT,reinterpret_cast<WPARAM>(normal),TRUE); return c;
    }
    void create(HWND w) {
        window = w;
        normal = CreateFontW(-px(15),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        small = CreateFontW(-px(13),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        heading = CreateFontW(-px(32),0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        bold = CreateFontW(-px(16),0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        mic = control(WC_COMBOBOXW,L"",CBS_DROPDOWNLIST|WS_VSCROLL,44,155,552,240,Microphone);
        refresh = control(L"BUTTON",L"Refresh",BS_PUSHBUTTON,610,154,122,32,Refresh);
        output = control(WC_COMBOBOXW,L"",CBS_DROPDOWNLIST|WS_VSCROLL,44,219,688,240,Output);
        suppress = control(L"BUTTON",L"Noise suppression",BS_AUTOCHECKBOX,44,281,260,28,Suppress);
        SendMessageW(suppress,BM_SETCHECK,BST_CHECKED,0);
        outputMode = control(WC_COMBOBOXW,L"",CBS_DROPDOWNLIST|WS_VSCROLL,398,281,310,120,OutputMode);
        for (const auto* label : {L"Muted", L"Headphones (-12 dB)", L"Virtual cable (full level)"})
            SendMessageW(outputMode,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label));
        SendMessageW(outputMode,CB_SETCURSEL,0,0);
        deepFilter = control(L"BUTTON",L"Use DeepFilterNet3 (external)",BS_AUTOCHECKBOX,398,315,310,28,DeepFilter);
        strength = control(TRACKBAR_CLASSW,L"",TBS_HORZ|TBS_NOTICKS,40,349,696,35,Strength);
        SendMessageW(strength,TBM_SETRANGE,TRUE,MAKELPARAM(0,100)); SendMessageW(strength,TBM_SETPOS,TRUE,100);
        start = control(L"BUTTON",L"Start audio",BS_DEFPUSHBUTTON,44,643,174,42,Start);
        enumerate(); SetTimer(window,1,100,nullptr);
    }
    void enumerate() {
        try {
            if (!session) session = std::make_unique<luna::LiveSession>();
            devices = session->enumerate();
            auto fill = [](HWND c, const std::vector<luna::AudioDevice>& list) {
                SendMessageW(c,CB_RESETCONTENT,0,0);
                std::size_t selected = 0;
                for (std::size_t i = 0; i < list.size(); ++i) {
                    auto label = wide(list[i].name) + (list[i].isDefault ? L"  (default)" : L"");
                    SendMessageW(c,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label.c_str()));
                    if(list[i].isDefault) selected = i;
                }
                if (!list.empty()) SendMessageW(c,CB_SETCURSEL,selected,0);
            };
            fill(mic,devices.microphones); fill(output,devices.outputs);
            bool available = !devices.microphones.empty() && !devices.outputs.empty();
            EnableWindow(start,available);
            status = available ? L"Ready. Audio starts only when you press Start." : L"No microphone/output found. Connect a device and refresh.";
        } catch (const std::exception& e) { status = wide(e.what()); EnableWindow(start,FALSE); session.reset(); }
        InvalidateRect(window,nullptr,FALSE);
    }
    void toggle() {
        try {
            if (active) {
                session->stop(); active = false; status = L"Stopped. Microphone released. No recording was created.";
            } else {
                auto m = SendMessageW(mic,CB_GETCURSEL,0,0), o = SendMessageW(output,CB_GETCURSEL,0,0);
                if (m < 0 || o < 0) return;
                std::unique_ptr<luna::NoiseSuppressionEngine> engine;
                if (SendMessageW(deepFilter,BM_GETCHECK,0,0)==BST_CHECKED) {
                    wchar_t filename[MAX_PATH]{};
                    auto n = GetModuleFileNameW(nullptr,filename,MAX_PATH);
                    if (!n || n >= MAX_PATH) throw std::runtime_error("Cannot locate the application folder");
                    auto folder = std::filesystem::path(filename).parent_path();
                    engine = luna::makeDeepFilterNet3(folder/L"deepfilter.dll",folder/L"DeepFilterNet3_onnx.tar.gz");
                } else engine = luna::makeRNNoise();
                session->start(&devices.microphones.at(static_cast<std::size_t>(m)), &devices.outputs.at(static_cast<std::size_t>(o)),
                    SendMessageW(suppress,BM_GETCHECK,0,0)==BST_CHECKED,
                    static_cast<float>(SendMessageW(strength,TBM_GETPOS,0,0))/100,
                    selectedMode(),std::move(engine));
                active = true; native = wide(session->streamInfo());
                status = L"Processing locally. Select the cable's recording end in your call app.";
            }
        } catch (const std::exception& e) { if(session) session->stop(); active=false; status=wide(e.what()); MessageBoxW(window,status.c_str(),L"Luna Audio AI",MB_OK|MB_ICONERROR); }
        updateControls();
    }
    void updateControls() {
        SetWindowTextW(start,active ? L"Stop audio" : L"Start audio");
        for(auto c:{mic,output,refresh,deepFilter}) EnableWindow(c,!active);
        InvalidateRect(window,nullptr,FALSE);
    }
    luna::MonitoringOutput::Mode selectedMode() const {
        auto index = SendMessageW(outputMode,CB_GETCURSEL,0,0);
        return index == 2 ? luna::MonitoringOutput::Mode::route
             : index == 1 ? luna::MonitoringOutput::Mode::headphones
                          : luna::MonitoringOutput::Mode::muted;
    }
    void tick() {
        if(active && session) {
            metrics=session->processor()->metrics().snapshot();
            if (!session->running()) { session->stop(); active=false; status=L"Audio device stopped. Refresh devices and start again."; updateControls(); }
            else if (session->interruptions()) status=L"Device interruption/reroute detected. Stop and reselect devices if needed.";
        }
        FILETIME create{},exit{},kernel{},user{};
        if(GetProcessTimes(GetCurrentProcess(),&create,&exit,&kernel,&user)) {
            ULARGE_INTEGER k{},u{}; k.LowPart=kernel.dwLowDateTime;k.HighPart=kernel.dwHighDateTime;
            u.LowPart=user.dwLowDateTime;u.HighPart=user.dwHighDateTime;
            auto now=GetTickCount64(), current=k.QuadPart+u.QuadPart;
            if(!lastTick || now-lastTick>=1000) {
                if(lastTick) cpu=double(current-lastCpu)/double(now-lastTick)/100.0;
                lastCpu=current;lastTick=now;
            }
        }
        InvalidateRect(window,nullptr,FALSE);
    }
    void text(HDC dc, int x, int y, int w, int h, const std::wstring& value, HFONT font, COLORREF color=ink) {
        SelectObject(dc,font);SetTextColor(dc,color);SetBkMode(dc,TRANSPARENT);
        RECT r{px(x),px(y),px(x+w),px(y+h)};
        DrawTextW(dc,value.c_str(),-1,&r,DT_LEFT|DT_TOP|DT_NOPREFIX|DT_WORDBREAK);
    }
    void meter(HDC dc,int x,int y,const wchar_t* label,float db,float peak) {
        text(dc,x,y,280,24,label,bold);
        std::wostringstream s;s<<std::fixed<<std::setprecision(1)<<db<<L" dBFS";
        text(dc,x+234,y,108,24,s.str(),small,muted);
        RECT track{px(x),px(y+32),px(x+324),px(y+42)};
        auto brush=CreateSolidBrush(RGB(222,229,237));FillRect(dc,&track,brush);DeleteObject(brush);
        track.right=track.left+px(static_cast<int>(324*std::clamp((db+60)/60,0.0f,1.0f)));
        brush=CreateSolidBrush(peak>=0.99f?RGB(191,56,52):teal);FillRect(dc,&track,brush);DeleteObject(brush);
    }
    void paint(HDC dc) {
        RECT all;GetClientRect(window,&all);FillRect(dc,&all,bg);
        text(dc,32,24,490,44,L"Luna Audio AI",heading);
        text(dc,34,75,650,24,L"A clearer microphone. Processed on your computer.",normal,muted);
        text(dc,575,35,190,24,active && SendMessageW(deepFilter,BM_GETCHECK,0,0)==BST_CHECKED
            ? L"LOCAL / DeepFilterNet3" : L"LOCAL / RNNoise",small,teal);
        RECT card{px(28),px(117),px(748),px(626)};FillRect(dc,&card,white);
        text(dc,44,128,500,22,L"MICROPHONE",small,muted);
        text(dc,44,194,650,22,L"OUTPUT: HEADPHONES OR VIRTUAL CABLE INPUT",small,muted);
        auto amount=SendMessageW(strength,TBM_GETPOS,0,0);
        text(dc,44,326,660,24,L"Suppression mix   "+std::to_wstring(amount)+L"%",normal);
        text(dc,44,388,685,34,L"Mix blends original + denoised audio. Cable route requires a virtual cable driver.",small,muted);
        meter(dc,44,431,L"Input",active?metrics.inputDb:-120,active?metrics.inputPeak:0);
        meter(dc,398,431,L"Processed output",active?metrics.outputDb:-120,active?metrics.outputPeak:0);
        std::wostringstream s;s<<std::fixed<<std::setprecision(2)<<L"DSP last / mean / max: "<<metrics.processUs/1000<<L" / "<<metrics.meanUs/1000<<L" / "<<metrics.maxUs/1000<<L" ms";
        text(dc,44,499,690,24,s.str(),normal);
        s.str(L"");s.clear();s<<std::fixed<<std::setprecision(1)<<L"Process CPU: "<<cpu<<L"% of one core   |   Late frames: "<<metrics.lateFrames<<L"   |   Late callbacks: "<<metrics.lateCallbacks;
        text(dc,44,529,690,24,s.str(),small,muted);
        auto pipelineMs = active && session && session->processor()
            ? session->processor()->latencySamples()*1000/luna::sample_rate
            : SendMessageW(deepFilter,BM_GETCHECK,0,0)==BST_CHECKED ? 40u : 20u;
        text(dc,44,559,690,40,L"Pipeline delay: " + std::to_wstring(pipelineMs)
            + L" ms + device buffering. Physical end-to-end latency: unmeasured.",small,muted);
        text(dc,44,594,690,22,active?native:L"48 kHz mono / RNNoise default / Optional DeepFilterNet3",small,muted);
        text(dc,238,645,495,43,status,normal,active?teal:ink);
        text(dc,32,711,720,22,L"No cloud processing. No audio telemetry. Live recording unavailable in V1.",small,muted);
    }
};
LRESULT CALLBACK procedure(HWND w,UINT msg,WPARAM wp,LPARAM lp) {
    auto* app=reinterpret_cast<App*>(GetWindowLongPtrW(w,GWLP_USERDATA));
    if(msg==WM_NCCREATE) { app=static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);SetWindowLongPtrW(w,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(app)); }
    if(!app) return DefWindowProcW(w,msg,wp,lp);
    switch(msg) {
    case WM_CREATE: app->create(w);return 0;
    case WM_TIMER: app->tick();return 0;
    case WM_COMMAND:
        if(LOWORD(wp)==Refresh) app->enumerate();
        else if(LOWORD(wp)==Start) app->toggle();
        else if(LOWORD(wp)==Suppress && app->session && app->session->processor()) app->session->processor()->setEnabled(SendMessageW(app->suppress,BM_GETCHECK,0,0)==BST_CHECKED);
        else if(LOWORD(wp)==OutputMode && HIWORD(wp)==CBN_SELCHANGE && app->session)
            app->session->output().setMode(app->selectedMode());
        return 0;
    case WM_HSCROLL:
        if(app->session && app->session->processor()) app->session->processor()->setStrength(static_cast<float>(SendMessageW(app->strength,TBM_GETPOS,0,0))/100);
        InvalidateRect(w,nullptr,FALSE);return 0;
    case WM_CTLCOLORSTATIC: case WM_CTLCOLORBTN:
        SetBkColor(reinterpret_cast<HDC>(wp),RGB(255,255,255));SetTextColor(reinterpret_cast<HDC>(wp),ink);return reinterpret_cast<LRESULT>(app->white);
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;auto dc=BeginPaint(w,&ps);RECT r;GetClientRect(w,&r);
        auto memory=CreateCompatibleDC(dc);auto bitmap=CreateCompatibleBitmap(dc,r.right,r.bottom);
        auto old=SelectObject(memory,bitmap);app->paint(memory);BitBlt(dc,0,0,r.right,r.bottom,memory,0,0,SRCCOPY);
        SelectObject(memory,old);DeleteObject(bitmap);DeleteDC(memory);EndPaint(w,&ps);return 0;
    }
    case WM_CLOSE: if(app->session)app->session->stop();DestroyWindow(w);return 0;
    case WM_DESTROY: KillTimer(w,1);PostQuitMessage(0);return 0;
    }
    return DefWindowProcW(w,msg,wp,lp);
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int show) {
    SetProcessDPIAware();auto dc=GetDC(nullptr);scale=GetDeviceCaps(dc,LOGPIXELSX);ReleaseDC(nullptr,dc);
    RECT work{};SystemParametersInfoW(SPI_GETWORKAREA,0,&work,0);
    scale=std::max(64,std::min(scale,static_cast<int>((work.bottom-work.top-80)*96/752)));
    INITCOMMONCONTROLSEX common{sizeof(common),ICC_BAR_CLASSES|ICC_STANDARD_CLASSES};InitCommonControlsEx(&common);
    App app;WNDCLASSW wc{};wc.lpfnWndProc=procedure;wc.hInstance=instance;wc.lpszClassName=L"LunaAudioAIWindow";
    wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hIcon=LoadIconW(nullptr,IDI_APPLICATION);RegisterClassW(&wc);
    DWORD style=WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX|WS_CLIPCHILDREN;
    RECT bounds{0,0,px(776),px(752)};AdjustWindowRect(&bounds,style,FALSE);
    auto window=CreateWindowExW(0,wc.lpszClassName,L"Luna Audio AI",style,CW_USEDEFAULT,CW_USEDEFAULT,
        bounds.right-bounds.left,bounds.bottom-bounds.top,nullptr,nullptr,instance,&app);
    if(!window)return 1;
    ShowWindow(window,show);UpdateWindow(window);
    MSG msg;while(GetMessageW(&msg,nullptr,0,0)>0) { if(!IsDialogMessageW(window,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);} }
    return static_cast<int>(msg.wParam);
}

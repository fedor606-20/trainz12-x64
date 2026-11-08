// Unit U11 of KernelStandard_7c: OS, Window, WindowPlacement (0x100368f0..0x10036c6b, 0x100420d0..0x1004332c,
// plus folded getters). Notes: notes/KernelStandard.md G13 and "U11 done". Behaviour follows the TS12 assembly,
// quirks included (see the comments).
#include <jet/KernelStandard.h>
#include <jet/Window.h>

#include <windows.h>

#include <cctype>
#include <cstdio>
#include <cstring>
#include <new>

namespace Jet {

// ---- OS (0x100368f0..0x10036c6b)

namespace {
// the 0x18 bytes behind OS's opaque block
struct OSData {
    void* instance;           // +0x00
    Window* window;           // +0x04
    const char* commandLine;  // +0x08 not copied
    Options* options;         // +0x0c
    HWND__* parent;           // +0x10
    AnsiString appName;       // +0x14
};
static_assert(sizeof(OSData) == 0x18);

OSData& D(OS* os) { return *reinterpret_cast<OSData*>(os); }
const OSData& D(const OS* os) { return *reinterpret_cast<const OSData*>(os); }

// the text of an AnsiString, null when it is empty (its shared data: {length, ..., text +8})
const char* TextOrNull(const AnsiString& s)
{
    int* const data = *reinterpret_cast<int* const*>(&s);
    return *data ? reinterpret_cast<const char*>(data + 2) : nullptr;
}
}  // namespace

const char* const OS::defaultOptions =  // 0x10072898 -> 0x10055b68
    " +InputKeyboard=Default,Win32,DirectX +InputMouse=Default,Win32,DirectX +InputJoystick=Default,DirectX,Win32"
    " +Input=Win32,DirectX +Render=OpenGL,DirectX +Network=Native -Sound=DirectX -Movie=DirectShow -ShowRenderAPI"
    " -ShowFPS -Disable3dfxSplash -aAllocIgnore=FastDynamicArray;FastDynamicArrayObj;Str;Bitmap;HashTable"
    " +Log=File:Err -StencilBits=8 -LogFlush-";

OS::OS(const char* appName, const char* commandLine, void* instance, HWND__* window)  // 0x10036a40
{
    OSData& d = D(this);
    ::new (&d.appName) AnsiString();
    d.instance = instance;
    d.commandLine = commandLine;
    d.parent = window;
    d.appName = appName;
    void* const mem = Alloc::New(0xc);
    d.options = mem ? ::new (mem) Options() : nullptr;
    d.window = nullptr;
}

OS::OS(const OS& o)  // 0x100368f0
{
    OSData& d = D(this);
    const OSData& s = D(&o);
    d.instance = s.instance;
    d.window = s.window;
    d.commandLine = s.commandLine;
    d.options = s.options;  // shared: both destructors free it
    d.parent = s.parent;
    ::new (&d.appName) AnsiString(s.appName);
}

OS& OS::operator=(const OS& o)  // 0x10036930
{
    OSData& d = D(this);
    const OSData& s = D(&o);
    d.instance = s.instance;
    d.window = s.window;
    d.commandLine = s.commandLine;
    d.options = s.options;
    d.parent = s.parent;
    d.appName = s.appName;
    return *this;
}

OS::~OS()  // 0x10036b10
{
    OSData& d = D(this);
    if (Options* const o = d.options) {
        o->~Options();
        Alloc::Delete(o);
    }
    d.options = nullptr;
    d.appName.~AnsiString();
}

bool OS::Init() { return ExceptionHandler::Main(InitCallBack, this, nullptr) != 0; }  // 0x10036b80

int __cdecl OS::InitCallBack(void* self)  // 0x10036ba0
{
    OSData& d = *static_cast<OSData*>(self);
    d.options->ProcessRaw(defaultOptions);
    d.options->Init(TextOrNull(d.appName), d.commandLine);
    return 1;
}

const char* __cdecl OS::GetDefaultOptions() { return defaultOptions; }                 // 0x10036970
const char* __cdecl OS::GetAppName() const { return TextOrNull(D(this).appName); }     // 0x10036980
const char* __cdecl OS::GetName() const { return "Windows"; }                          // 0x10036be0
const char* __cdecl OS::GetCmdLine() const { return D(this).commandLine; }             // 0x1000e670 (folded)
void* __cdecl OS::GetInstance() const { return D(this).instance; }                    // 0x10001870 (folded)
Options* __cdecl OS::GetOptions() const { return D(this).options; }                    // 0x100330e0 (folded)
Window* __cdecl OS::GetWindow() const { return D(this).window; }                       // 0x1000e7a0 (folded)
void __cdecl OS::SetWindow(Window* window) { D(this).window = window; }                // 0x10036c60
HWND__* __cdecl OS::GetParent() const { return D(this).parent; }                       // 0x10036c40
void __cdecl OS::SetParent(HWND__* parent) { D(this).parent = parent; }                // 0x10036c50
DynamicLibrary* __cdecl OS::CreateDynamicLibrary() const { return new DynamicLibrary; }  // 0x10036bf0

Window* __cdecl OS::CreateNewWindow() const  // 0x10036c10
{
    void* const mem = Alloc::New(sizeof(Window));
    return mem ? ::new (mem) Window(D(this).parent) : nullptr;
}

// ---- Window (0x100369a0..0x10036a35, 0x100420d0..0x100431c1)

static_assert(sizeof(Window) == 0x134);

const int Window::WM_END_OF_MESSAGES = WM_USER + 1;  // 0x10052210
const int Window::WM_MODE_CHANGED = WM_USER + 2;     // 0x10052214
Window* Window::mainWindow = nullptr;                // 0x100add48

namespace {
const char kClass[] = "JET";  // the class name and the default title (0x10055cd0)
Window* creating;             // 0x100add4c: WindowProc's object until the window's extra bytes hold it
int hookGeneration;           // 0x100add50: a hook added or cleared ends WindowProc's walk

// not in TS12: the "Borderless" option (e.g. -borderless in trainzoptions.txt) makes a top-level windowed host a
// popup without a frame covering its monitor
bool borderless;

// styles of a host window: windowed (with a caption, or a child of the parent) or full screen (a popup)
DWORD WindowedStyle(HWND parent) { return parent ? 0x44000000 : borderless ? WS_POPUP : 0x00c80000; }

// trainz.exe merges trainzoptions.txt (in the game folder, the working directory) only after the window opens
bool BorderlessInOptionsFile()
{
    char text[4096] = {};
    const HANDLE f = CreateFileA("trainzoptions.txt", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                 OPEN_EXISTING, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    DWORD n = 0;
    ReadFile(f, text, sizeof text - 1, &n, nullptr);
    CloseHandle(f);
    for (DWORD i = 0; i < n; ++i) text[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(text[i])));
    return std::strstr(text, "-borderless") != nullptr;
}

// a borderless window covers the whole monitor it would open on (the game's resolution is stretched to it)
void BorderlessPlace(int& x, int& y, int& w, int& h)
{
    POINT pt = {x == CW_USEDEFAULT ? 0 : x, x == CW_USEDEFAULT ? 0 : y};
    MONITORINFO mi = {sizeof mi};
    if (!GetMonitorInfoA(MonitorFromPoint(pt, MONITOR_DEFAULTTOPRIMARY), &mi)) return;
    const RECT& m = mi.rcMonitor;
    x = m.left;
    y = m.top;
    w = m.right - m.left;
    h = m.bottom - m.top;
}
DWORD FullscreenStyle(HWND parent) { return parent ? 0xc1000000 : 0x81000000; }

#if defined(_WIN64)
void SetObject(HWND h, Window* w) { SetWindowLongPtrA(h, 0, reinterpret_cast<LONG_PTR>(w)); }
Window* GetObject(HWND h) { return reinterpret_cast<Window*>(GetWindowLongPtrA(h, 0)); }
#else
void SetObject(HWND h, Window* w) { SetWindowLongA(h, 0, reinterpret_cast<LONG>(w)); }
Window* GetObject(HWND h) { return reinterpret_cast<Window*>(GetWindowLongA(h, 0)); }
#endif
}  // namespace

Window::Window(HWND__* parentWindow)  // 0x10042e90
{
    hooks.head = reinterpret_cast<Node*>(&hooks);
    hooks.count = 0;
    dispatchers.head = reinterpret_cast<Node*>(&dispatchers);
    dispatchers.count = 0;
    flags |= 8;
    if (!mainWindow) mainWindow = this;
    active &= 0xfc;
    parent = parentWindow;
    handle = nullptr;
    bitsPerPixel = 0;
    shutdown = nullptr;
    flags = (flags & 0xee) | 2;
    std::strcpy(title, kClass);
    flags &= 0xcb;  // bits 6 and 7 keep whatever the memory held
}

Window::~Window()  // 0x10042df0
{
    Close();
    if (mainWindow == this) mainWindow = nullptr;
    while (dispatchers.count) {
        Node* const n = dispatchers.head;
        dispatchers.head = n->next;
        n->next = nullptr;
        --dispatchers.count;
        Alloc::Delete(n);
    }
    while (hooks.count) {
        Node* const n = hooks.head;
        hooks.head = n->next;
        n->next = nullptr;
        --hooks.count;
        Alloc::Delete(n);
    }
}

// a plain copy of all 0x134 bytes: the rings then still point into the source
Window& Window::operator=(const Window& o)  // 0x100369e0
{
    std::memcpy(static_cast<void*>(this), &o, sizeof(Window));
    return *this;
}

// ??_F (the default constructor closure, exported under that name by implemented.txt)
void __fastcall Jet_WindowDefaultCtor(Window* p) { ::new (p) Window(nullptr); }  // 0x10036a00

bool __cdecl Window::Open(const char* text, const Options* options)  // 0x100420d0
{
    shutdown = ExceptionHandler::AddCriticalShutdown(CriticalShutdown, this);
    WNDCLASSEXA wc;
    std::memset(&wc, 0, sizeof wc);
    wc.cbSize = sizeof wc;
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = reinterpret_cast<WNDPROC>(WindowProc);
    wc.cbWndExtra = sizeof(Window*);
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.hIcon = LoadIconA(nullptr, MAKEINTRESOURCEA(0x7f05));  // IDI_WINLOGO
    wc.hCursor = nullptr;
    wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    wc.lpszClassName = kClass;
    if (!RegisterClassExA(&wc)) {
        Error(const_cast<char*>("RegisterClassEx() failed"));
        return false;  // the critical-shutdown entry stays
    }
    flags |= 0x20;
    SetTitle(text);
    if (options) {
        if (options->Get("HideWindow")) flags &= ~2;
        borderless = options->Get("Borderless");
        bool showFps;
        options->Get("ShowFPS", showFps, false);
        if (showFps) {
            flags |= 1;
            frames = 0;
            fpsTime = timeGetTime();
            fps = 0.0f;
        }
    }
    if (!borderless) borderless = BorderlessInOptionsFile();
    if (borderless) SetProcessDPIAware();  // real pixels, so the window matches the monitor under Windows scaling
    return true;
}

bool __cdecl Window::CreateDefault()  // 0x10042220
{
    return CreateWindowedHost(CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                              reinterpret_cast<void*>(parent));
}

// a child of the parent covering its client area, or a captioned window with a client area of w x h
bool __cdecl Window::CreateWindowedHost(int x, int y, int w, int h, void* param)  // 0x10042250
{
    DWORD style = WindowedStyle(parent);
    if (flags & 2) style |= WS_VISIBLE;
    int width, height;
    if (parent) {
        RECT rc;
        if (!GetClientRect(parent, &rc)) {
            Error(const_cast<char*>("GetClientRect() failed"));
            return false;
        }
        ClientToScreen(parent, reinterpret_cast<POINT*>(&rc.left));
        ClientToScreen(parent, reinterpret_cast<POINT*>(&rc.right));
        width = rc.right - rc.left;
        height = rc.bottom - rc.top;
    } else {
        RECT rc = {0, 0, w, h};
        AdjustWindowRectEx(&rc, style, FALSE, 0);
        width = rc.right - rc.left;
        height = rc.bottom - rc.top;
        if (borderless && w != CW_USEDEFAULT) BorderlessPlace(x, y, width, height);
    }
    creating = this;
    handle = CreateWindowExA(0, kClass, title, style, x, y, width, height, parent, nullptr,
                             GetModuleHandleA(nullptr), param);
    if (!handle) {
        Error(const_cast<char*>("CreateWindowEx() failed"));
        return false;
    }
    SetObject(handle, this);
    flags &= ~4;
    return HandleMessages();
}

bool __cdecl Window::Create2DWindow(int x, int y, int w, int h, void* param)  // 0x10042430: a jmp
{
    return CreateWindowedHost(x, y, w, h, param);
}

bool __cdecl Window::CreateFullscreenHost(int w, int h, void* param)  // 0x100423a0
{
    creating = this;
    handle = CreateWindowExA(WS_EX_TOPMOST, kClass, title, FullscreenStyle(parent), 0, 0, w, h, parent, nullptr,
                             GetModuleHandleA(nullptr), param);
    if (!handle) {
        Error(const_cast<char*>("CreateWindowEx() failed"));
        return false;
    }
    SetObject(handle, this);
    flags |= 4;
    return HandleMessages();
}

// restyles the window; listeners hear WM_MODE_CHANGED with wParam 1 for windowed
bool __cdecl Window::SetHostState(bool fullscreen)  // 0x10042440
{
    if (!handle) return false;
    SetLastError(0);
    DWORD style = fullscreen ? FullscreenStyle(parent) : WindowedStyle(parent);
    if (flags & 2) style |= WS_VISIBLE;
    if (!SetWindowLongA(handle, GWL_STYLE, style)) {
        Error(const_cast<char*>("SetWindowLong(GWL_STYLE) failed"));
        return false;
    }
    SetLastError(0);
    if (!SetWindowLongA(handle, GWL_EXSTYLE, fullscreen ? WS_EX_TOPMOST : 0) && GetLastError()) {
        Error(const_cast<char*>("SetWindowLong(GWL_EXSTYLE) failed"));
        return false;
    }
    flags = static_cast<unsigned char>((flags & ~4) | (fullscreen ? 4 : 0));
    SendMessageA(handle, WM_MODE_CHANGED, ~(flags >> 2) & 1, 0);
    return true;
}

// the class stays marked registered: a second Close fails to unregister it (debug output only)
void __cdecl Window::Close()  // 0x10042560
{
    if (shutdown) {
        ExceptionHandler::RemoveCriticalShutdown(shutdown);
        shutdown = nullptr;
    }
    if (handle && DestroyWindow(handle)) handle = nullptr;
    if ((flags & 0x20) && !UnregisterClassA(kClass, GetModuleHandleA(nullptr)))
        Error(const_cast<char*>("UnregisterClass() failed"));
}

void __cdecl Window::Show(bool show)  // 0x100425f0
{
    flags = static_cast<unsigned char>((flags & ~2) | (show ? 2 : 0));
    if (handle && !(flags & 0x10)) ShowWindow(handle, show ? SW_SHOW : SW_HIDE);
}

// r is the client area wanted: position and size
bool __cdecl Window::Move(const Rect& r, bool repaint)  // 0x10042630
{
    const DWORD style = GetWindowLongA(handle, GWL_STYLE);
    RECT rc = {0, 0, r.w, r.h};
    AdjustWindowRectEx(&rc, style, FALSE, 0);
    int x = r.x, y = r.y, w = rc.right - rc.left, h = rc.bottom - rc.top;
    if (borderless && !parent && !(flags & 4)) BorderlessPlace(x, y, w, h);  // the game's own placement is ignored
    return MoveWindow(handle, x, y, w, h, repaint) != 0;
}

bool __cdecl Window::Resize(const Size& s, bool repaint)  // 0x100426b0
{
    RECT wr;
    GetWindowRect(handle, &wr);  // the result is not checked
    const Rect r = {wr.left, wr.top, s.w, s.h};
    return Move(r, repaint);
}

Size __cdecl Window::CalculateWindowArea(const Size& client, bool windowedStyle) const  // 0x10042710
{
    DWORD style = GetWindowLongA(handle, GWL_STYLE);
    if (windowedStyle) style = WindowedStyle(parent);
    RECT rc = {0, 0, client.w, client.h};
    AdjustWindowRectEx(&rc, style, FALSE, 0);
    return Size{rc.right - rc.left, rc.bottom - rc.top};
}

void __cdecl Window::Message(const char* text)  // 0x10042790
{
    MessageBoxA(nullptr, text, kClass, MB_SETFOREGROUND | MB_TOPMOST);
}

bool __cdecl Window::MessageConfirm(const char* text)  // 0x100427b0
{
    return MessageBoxA(nullptr, text, kClass, MB_SETFOREGROUND | MB_TOPMOST | MB_YESNO) == IDYES;
}

// the text is not used: the debugger hears the system's message for GetLastError
void __cdecl Window::Error(char*)  // 0x100427e0
{
    const DWORD code = GetLastError();
    char* text = nullptr;
    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr,
                   code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<char*>(&text), 0, nullptr);
    char line[1024];
    wsprintfA(line, "GetLastError(%d) returned: %s\n", code, text);
    OutputDebugStringA(line);
    LocalFree(text);
}

// Empties the thread's queue (dispatchers first, else translate and dispatch), then posts WM_END_OF_MESSAGES to
// the window and dispatches one more message. False only when WM_QUIT came. With ShowFPS the title shows the
// frame rate, a running average updated every 250 ms.
bool __cdecl Window::HandleMessages()  // 0x10042860
{
    if (!(flags & 8)) return true;
    if (flags & 1) {
        ++frames;
        const DWORD now = timeGetTime();
        const int elapsed = static_cast<int>(now - fpsTime);
        if (elapsed > 250) {
            const float f = (static_cast<float>(frames) * 1000.0f / static_cast<float>(elapsed) + fps) * 0.5f;
            if (f != fps) {
                char line[0x108];
                std::snprintf(line, sizeof line, "%s  (%0.1f)", title, static_cast<double>(f));
                SetWindowTextA(handle, line);
                fps = f;
            }
            frames = 0;
            fpsTime = now;
        }
    }
    MSG msg;
    while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) return false;
        bool taken = false;
        Node* const end = reinterpret_cast<Node*>(&dispatchers);
        for (Node* n = dispatchers.head; n != end && !taken; n = n->next)
            taken = static_cast<Dispatcher>(n->fn)(n->context, &msg);
        if (!taken) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
    }
    PostMessageA(handle, WM_END_OF_MESSAGES, 0, 0);
    if (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) DispatchMessageA(&msg);
    return true;
}

void __cdecl Window::SetHandleMessages(bool on)  // 0x100369a0
{
    flags = static_cast<unsigned char>((flags & ~8) | (on ? 8 : 0));
}

namespace {
// appended after the last node (the ring's head is the sentinel's next)
template <class Ring, class Node>
void Append(Ring& ring, void* fn, void* context)
{
    Node* const n = static_cast<Node*>(Alloc::New(sizeof(Node)));
    if (n) n->next = nullptr;
    n->context = context;
    n->fn = fn;
    Node* const end = reinterpret_cast<Node*>(&ring);
    Node* last = end;
    for (Node* p = end->next; p != end; p = p->next) last = p;
    n->next = last->next;
    last->next = n;
    ++ring.count;
}

// the first node with that context
template <class Ring, class Node>
bool Remove(Ring& ring, void* context)
{
    Node* const end = reinterpret_cast<Node*>(&ring);
    Node* n = end->next;
    if (n == end) return false;
    while (n->context != context) {
        n = n->next;
        if (n == end) return false;
    }
    Node* prev = end;
    for (Node* p = end->next; p != end && p != n; p = p->next) prev = p;
    prev->next = n->next;
    n->next = nullptr;
    --ring.count;
    Alloc::Delete(n);
    return true;
}
}  // namespace

void __cdecl Window::RegisterWindowHook(Hook hook, void* context)  // 0x10042a10
{
    Append<Ring, Node>(hooks, reinterpret_cast<void*>(hook), context);
    ++hookGeneration;
}

void __cdecl Window::ClearWindowHook(void* context)  // 0x10042a70
{
    if (Remove<Ring, Node>(hooks, context)) ++hookGeneration;
}

void __cdecl Window::RegisterWindowDispatcher(Dispatcher dispatcher, void* context)  // 0x10042ad0
{
    Append<Ring, Node>(dispatchers, reinterpret_cast<void*>(dispatcher), context);
}

void __cdecl Window::ClearWindowDispatcher(void* context) { Remove<Ring, Node>(dispatchers, context); }  // 0x10042b20

// Hooks first (a hook added or cleared during the walk ends it); then the window's own handling: paint is
// validated, destroy quits (top-level windows only), activation is recorded, a child follows its size, some
// system commands are dropped, input goes to the parent; the rest to DefWindowProc.
long __stdcall Window::WindowProc(HWND__* hwnd, unsigned int msg, unsigned int wParam, long lParam)  // 0x10042b80
{
    Window* w = GetObject(hwnd);
    if (!w) w = creating;
    if (!w) return DefWindowProcA(hwnd, msg, wParam, lParam);
    Node* const end = reinterpret_cast<Node*>(&w->hooks);
    const int generation = hookGeneration;
    for (Node* n = w->hooks.head; n != end;) {
        HookMessage m = {hwnd, msg, wParam, lParam};
        int result = 0;
        if (static_cast<Hook>(n->fn)(n->context, &m, result)) return result;
        if (generation != hookGeneration) break;
        n = n->next;
    }
    switch (msg) {
    case WM_PAINT:
        ValidateRect(w->handle, nullptr);
        return 0;
    case WM_DESTROY:
        if (!w->parent && !(w->flags & 0x10)) PostQuitMessage(0);
        return 0;
    case WM_SIZE:
        if (w->parent) {
            SetWindowPos(w->handle, HWND_BOTTOM, 0, 0, LOWORD(lParam), HIWORD(lParam), SWP_NOMOVE | SWP_NOZORDER);
            return 0;
        }
        break;
    case WM_ACTIVATE:
        w->active = static_cast<unsigned char>((w->active & ~2) | (LOWORD(wParam) ? 2 : 0));
        return 0;
    case WM_ACTIVATEAPP:
        w->active = static_cast<unsigned char>((w->active & ~1) | (wParam ? 1 : 0));
        return 0;
    case WM_SYSCOMMAND:
        if (!w->parent) {
            switch (wParam) {
            case SC_SCREENSAVE:
            case SC_KEYMENU:
            case SC_MONITORPOWER:
                return 0;
            case SC_CLOSE:
                SendMessageA(w->handle, WM_CLOSE, 0, 0);
                return 0;
            }
        }
        break;
    }
    if (w->parent && ((msg >= WM_MOUSEFIRST && msg <= 0x20d) || msg - WM_KEYFIRST <= 9)) {
        PostMessageA(w->parent, msg, wParam, lParam);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

void* __cdecl Window::GetHandle() const { return handle; }                              // 0x10001870 (folded)
void __cdecl Window::SetHandle(void* h) { handle = static_cast<HWND>(h); }              // 0x10042de0 (folded)
HWND__* __cdecl Window::GetParent() const { return parent; }                            // 0x100369d0
const char* __cdecl Window::GetTitle() { return title; }                                // 0x10008590 (folded)
const char* __cdecl Window::GetInternalName() { return kClass; }                        // 0x100369c0
bool __cdecl Window::GetVisible() const { return (flags >> 1) & 1; }                    // 0x10036a10
bool __cdecl Window::GetHostState() const { return (flags >> 2) & 1; }                  // 0x10036a20
Window* __cdecl Window::GetMainWindow() { return mainWindow; }                          // 0x10036a30
bool __cdecl Window::IsActiveApp() { return active & 1; }                               // 0x10043030
bool __cdecl Window::IsActiveWnd() { return (active >> 1) & 1; }                        // 0x10043040
void __cdecl Window::LocalToGlobal(Point& p) const { ClientToScreen(handle, reinterpret_cast<POINT*>(&p)); }  // 0x10042fb0
bool __cdecl Window::IsIconic() const { return ::IsIconic(handle) != 0; }               // 0x10042fd0
bool __cdecl Window::HasFocus() const { return GetFocus() == handle; }                  // 0x10042ff0
void __cdecl Window::SetFocus() const { ::SetFocus(handle); }                           // 0x10043000

void __cdecl Window::SetCapture(bool on) const  // 0x10043010
{
    if (on)
        ::SetCapture(handle);
    else
        ReleaseCapture();
}

// a context-only window is never shown and never ends the message loop
void __cdecl Window::SetContextOnly(bool on)  // 0x10042f20
{
    flags = static_cast<unsigned char>((flags & ~0x10) | (on ? 0x10 : 0));
    if (on) flags &= ~2;
}

Rect __cdecl Window::GetClientArea() const  // 0x10042f50: all -1 on failure
{
    RECT rc;
    if (!GetClientRect(handle, &rc)) return Rect{-1, -1, -1, -1};
    return Rect{rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top};
}

Rect __cdecl Window::GetArea() const  // 0x10043050: the result of GetWindowRect is not checked
{
    RECT rc;
    GetWindowRect(handle, &rc);
    return Rect{rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top};
}

Rect __cdecl Window::GetParentArea() const  // 0x10043090: all 0 without a parent
{
    RECT rc;
    if (!parent || !GetWindowRect(parent, &rc)) return Rect{0, 0, 0, 0};
    return Rect{rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top};
}

int __cdecl Window::CriticalShutdown(void*)  // 0x100430f0
{
    UnregisterClassA(kClass, GetModuleHandleA(nullptr));
    return 0;
}

int __cdecl Window::GetHostBitsPerPixel()  // 0x10043110: the desktop's depth, read once
{
    if (bitsPerPixel) return bitsPerPixel;
    if (HWND const desktop = GetDesktopWindow()) {
        if (HDC const dc = GetDC(desktop)) {
            bitsPerPixel = GetDeviceCaps(dc, BITSPIXEL);
            ReleaseDC(desktop, dc);
        }
    }
    return bitsPerPixel;
}

bool __cdecl Window::SetTitle(const char* text)  // 0x10043170
{
    if (std::strlen(text) > 0x100) return false;
    std::strcpy(title, text);
    if (handle) SetWindowTextA(handle, title);
    return true;
}

// ---- WindowPlacement (0x100431d0..0x1004332c): every visible top-level window's placement, saved and put back

class WindowPlacement {
public:
    static void __cdecl SaveWindowPositions();
    static void __cdecl RestoreWindowPositions();
};

namespace {
struct Placement {
    Placement* next;
    Placement* prev;
    HWND window;
    bool hasRect;  // GetWindowRect succeeded
    RECT rect;
    WINDOWPLACEMENT placement;
};
static_assert(sizeof(Placement) == 0x4c);

// 0x1007421c: a ring {next, prev} and its count; saves add to it, it is emptied at exit only (0x100432e0)
struct Placements {
    Placement* next;
    Placement* prev;
    unsigned long count;
    Placement* End() { return reinterpret_cast<Placement*>(this); }
    Placements() : next(End()), prev(End()), count(0) {}
    ~Placements()
    {
        while (count) {
            Placement* const p = next;
            p->prev->next = p->next;
            p->next->prev = p->prev;
            p->prev = p->next = nullptr;
            --count;
            Alloc::Delete(p);
        }
    }
} placements;

BOOL CALLBACK SaveOne(HWND h, LPARAM)  // 0x100431d0
{
    if (!IsWindowVisible(h)) return TRUE;
    Placement* p = static_cast<Placement*>(Alloc::New(sizeof(Placement)));
    if (p) p->prev = p->next = nullptr;
    p->window = h;
    p->placement.length = sizeof(WINDOWPLACEMENT);
    if (!GetWindowPlacement(h, &p->placement)) {
        Alloc::Delete(p);
        return TRUE;
    }
    p->hasRect = GetWindowRect(h, &p->rect) != 0;
    p->next = placements.End();
    p->prev = placements.prev;
    placements.prev->next = p;
    placements.prev = p;
    ++placements.count;
    return TRUE;
}
}  // namespace

void __cdecl WindowPlacement::SaveWindowPositions() { EnumWindows(SaveOne, 0); }  // 0x100432d0

void __cdecl WindowPlacement::RestoreWindowPositions()  // 0x10043270
{
    for (Placement* p = placements.next; p != placements.End(); p = p->next) {
        p->placement.length = sizeof(WINDOWPLACEMENT);
        SetWindowPlacement(p->window, &p->placement);
        SetWindowPos(p->window, nullptr, p->rect.left, p->rect.top, p->rect.right - p->rect.left,
                     p->rect.bottom - p->rect.top, 0);
    }
}

}  // namespace Jet

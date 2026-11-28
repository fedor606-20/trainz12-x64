// Unit U12c of KernelStandard_7c: ExceptionHandler (0x1001cff0..0x1001dcae, 0x1001cc40, 0x1001cc70), AssertEx
// (0x1001cc90..0x1001ccbf, 0x1001d4e0) and HardwareException (0x10014380..0x10014445, 0x1001ccc0).
// Still the original's and called there by address: the crash report writers (the hardware-fault filter 0x1001e920
// with ERRORLOG.TXT / CRASH.DMP / XCrashReport.exe, and the assertion report 0x1001ee20) and
// HardwareException::GetCallStack (an x86 EBP walk, through its export). They move with the x64 rewrite; they keep
// their own state (the report buffer, ERRORLOG.TXT), so they need none of ours.
// The statics are ours since U14b: the exported ones, the report lock, the nesting count of Main, the re-entry flags
// of the critical lists and the "ExceptionHandler" log client. `file` / `reason` mangle differently in VS2005
// (const char* const, 0PBDB): they are exported under those names from ExceptionHandlerFile / ExceptionHandlerReason.
#include <jet/Config.h>
#include <jet/KernelStandard.h>

#include "Crt80.h"

#include <windows.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <new>

// thrown by ExceptionHandler::Assert; the class name is what the catchers in other modules match
class AssertEx : public Jet::Exception {
public:
    AssertEx(const char* file, int line, const char* reason) : Exception(file, line, reason) {}
    AssertEx(const AssertEx& o) : Exception(o) {}
    const char* __cdecl Class() const override { return "Assertion"; }
};

namespace Jet {

class ExceptionHandler::Shutdown;

char ExceptionHandler::corePrefix[0x10] = "core";                    // 0x100728d8
ExceptionHandler::Shutdown* ExceptionHandler::criticalShutdown;      // 0x100adba8
ExceptionHandler::Shutdown* ExceptionHandler::criticalStartup;       // 0x100adbac
bool ExceptionHandler::doCoreDump = true;                            // 0x100728d5
int ExceptionHandler::dumpOptions;                                   // 0x100adbbc
alignas(8) char ExceptionHandler::hardwareExceptionBlock[0x1118];    // 0x10075528: DoFilterException's HardwareException
bool ExceptionHandler::ignore;                                       // 0x100adb91
bool ExceptionHandler::isPromptAssertions = true;                    // 0x100728d4
int ExceptionHandler::line;                                          // 0x100adbb4
extern const char* ExceptionHandlerFile = nullptr;                                  // 0x100adbb0
extern const char* ExceptionHandlerReason = nullptr;                                // 0x100adbb8

char HardwareException::tmpDescription[0x400];  // 0x10075128 (the original's GetCallStack uses its own)
HardwareException::Type HardwareException::exceptionTypes[21] = {  // 0x100728e8
    {0xc0000005, "Access Violation"},
    {0xc000008c, "Array Bounds Exceeded"},
    {0x80000003, "Breakpoint"},
    {0x80000002, "Data Type Misalignment"},
    {0xc000008d, "Floating Point Denormal Operation"},
    {0xc000008e, "Floating Point Divide By Zero"},
    {0xc000008f, "Floating Point Inexact Result"},
    {0xc0000090, "Floating Point Invalid Operaton"},
    {0xc0000091, "Floating Point Overflow"},
    {0xc0000092, "Floating Point Stack Check"},
    {0xc0000093, "Floating Point Underflow"},
    {0xc000001d, "Illegal Instruction"},
    {0xc0000006, "In Page Error"},
    {0xc0000094, "Integer Divide By Zero"},
    {0xc0000095, "Integer Overflow"},
    {0xc0000026, "Invalid Disposition"},
    {0xc0000025, "Non-continuable Exception"},
    {0xc0000096, "Privileged Instruction"},
    {0x80000004, "Single Step"},
    {0xc00000fd, "Stack Overflow"},
    {0xe06d7363, "C++ Exception"},
};

namespace {
CRITICAL_SECTION reportLock;  // 0x1007663c, made by the outermost Main
int mainNesting;              // 0x100ae208
bool inShutdown, inStartup;   // 0x100ae20c / 0x100ae20d
LogClient* const handlerClient = Log::GetClient("ExceptionHandler");  // 0x100ae178 (0x100507b0)
}  // namespace

class ExceptionHandler::Shutdown {
public:
    void* arg;
    CallBack fn;
    Shutdown* next;
};

struct HandlerWork {
    static char* Base()
    {
        static char* const base = reinterpret_cast<char*>(GetModuleHandleA("KernelStandard_7c_orig.dll"));
        return base;
    }
    static CRITICAL_SECTION* Lock() { return &reportLock; }
    static int& Nesting() { return mainNesting; }
    static bool& InShutdown() { return inShutdown; }
    static bool& InStartup() { return inStartup; }
    static LogClient* Client() { return handlerClient; }

    // the original's crash report for a hardware fault (the filter of Main; 1: handled, 0: continue the search).
    // With no original module there is no reporter: returning 0 lets the OS record the real fault instead of
    // calling into an unmapped low address and recursing through Main until the stack is gone.
    static int CrashFilter(void* pointers)
    {
        char* const base = Base();
        if (!base) return 0;
        return reinterpret_cast<int(__cdecl*)(void*)>(base + 0x1e920)(pointers);
    }
    // the original's report of an Exception (ERRORLOG.TXT and XCrashReport.exe -assert); 1: ignore from now on
    static int Report(Exception& e)
    {
        char* const base = Base();
        if (!base) return 0;
        return reinterpret_cast<int(__fastcall*)(Exception*)>(base + 0x1ee20)(&e);
    }

    static const char*& File() { return ExceptionHandlerFile; }
    static const char*& Reason() { return ExceptionHandlerReason; }

    // 0x1001cc40: the channel takes a message
    static bool Open(LogClient* c)
    {
        return Log::initialized && c->level > 0 && *reinterpret_cast<const int*>(&c->outputs) != 0 &&
               !LogClient::disabled;
    }
    static void Say(LogClient* c, const char* fmt, const char* arg = nullptr)
    {
        c->kind = 1;
        c->OutputString(fmt, arg);
    }
    static void SayText(LogClient* c, const char* text)  // the text is the format, as in the original
    {
        c->kind = 1;
        c->OutputString(text);
    }
    // CorePrefix: at most 10 characters
    static const char* Clip(AnsiString& s)
    {
        int* d = static_cast<int*>(s.data);
        if (static_cast<unsigned int>(d[1]) > 10) {
            s.MakeBufferUnique(0, true);
            d = static_cast<int*>(s.data);
            d[1] = 10;
            reinterpret_cast<char*>(d + 2)[10] = 0;
        }
        return d[0] ? reinterpret_cast<const char*>(d + 2) : nullptr;
    }

    // 0x1001cc70: the rest of a critical list, front to back
    static void CallAll(ExceptionHandler::Shutdown* s)
    {
        s->fn(s->arg);
        if (s->next) CallAll(s->next);
    }

    // CriticalShutdown / CriticalStartup: once at a time unless the list is not empty; a failing log message
    // closes the log file, a failing callback is reported
    static void RunList(ExceptionHandler::Shutdown*& list, bool& running, const char* title, const char* failure)
    {
        if (running && !list) return;
        running = true;
        try {
            LogClient* c = Client();
            if (c && Log::initialized && c->level >= 1 && *reinterpret_cast<const int*>(&c->outputs) != 0 &&
                !LogClient::disabled)
                Say(c, title);
        } catch (...) {
            Log::Shutdown(true);
        }
        try {
            if (ExceptionHandler::Shutdown* s = list) {
                s->fn(s->arg);
                if (s->next) CallAll(s->next);
            }
        } catch (...) {
            LogClient* c = Client();
            if (c && Open(c)) Say(c, failure);
        }
        running = false;
    }

    static void* Add(ExceptionHandler::Shutdown*& list, ExceptionHandler::CallBack fn, void* arg)
    {
        auto* s = static_cast<ExceptionHandler::Shutdown*>(Alloc::New(sizeof(ExceptionHandler::Shutdown)));
        if (!s) {
            list = nullptr;
            return nullptr;
        }
        s->arg = arg;
        s->fn = fn;
        s->next = list;
        list = s;
        return s;
    }

    static void Remove(ExceptionHandler::Shutdown*& list, void* handle)
    {
        for (ExceptionHandler::Shutdown** at = &list; *at; at = &(*at)->next) {
            if (*at == handle) {
                ExceptionHandler::Shutdown* s = *at;
                *at = s->next;
                Alloc::Delete(s);
                return;
            }
        }
    }

    static int CallFilter(void* pointers) { return CrashFilter(pointers); }
};

ExceptionHandler& ExceptionHandler::operator=(const ExceptionHandler&) { return *this; }
void __cdecl ExceptionHandler::Break() {}

// ---- running a function

int __cdecl ExceptionHandler::Main(CallBack fn, void* arg, Thread*)
{
    if (ignore) return 0;
    int r = 0;
    if (HandlerWork::Nesting() == 0) InitializeCriticalSection(HandlerWork::Lock());
    ++HandlerWork::Nesting();
    __try {
        r = MainTryCatch(fn, arg);
    } __except (HandlerWork::CallFilter(GetExceptionInformation())) {
    }
    if (--HandlerWork::Nesting() == 0) DeleteCriticalSection(HandlerWork::Lock());
    return r;
}

int __cdecl ExceptionHandler::MainTryCatch(CallBack fn, void* arg)
{
    int r = 0;
    try {
        r = fn(arg);
    } catch (AssertEx&) {
        CriticalShutdown();
    } catch (Exception& e) {
        ProcessException(e);
        CriticalShutdown();
    }
    return r;
}

// The report of an exception, once at a time (prompting is off while it runs). 1 when not prompting or when the
// report asks to ignore every later one. With `ignore` already set it returns 1 holding the lock (as the original).
int __cdecl ExceptionHandler::ProcessException(Exception& e)
{
    if (!isPromptAssertions) return 1;
    isPromptAssertions = false;
    EnterCriticalSection(HandlerWork::Lock());
    if (ignore) return 1;
    int r = 1;
    __try {
        LogClient* c = HandlerWork::Client();
        if (c && HandlerWork::Open(c)) HandlerWork::Say(c, "** EXCEPTION **\n   %s\n", e.Description());
        r = HandlerWork::Report(e);
        if (r == 1) ignore = true;
    } __except (1) {
        MessageBoxA(nullptr, "An error occurred trying to display the fatal error window! (0)", "Fatal Error",
                    MB_ICONHAND);
        ignore = true;
    }
    if (!ignore) isPromptAssertions = true;
    LeaveCriticalSection(HandlerWork::Lock());
    if (!ignore) isPromptAssertions = true;
    return r;
}

int __cdecl ExceptionHandler::DoFilterException(void* pointers)
{
    int r = 1;
    __try {
        CriticalShutdown();
        HardwareException* h = ::new (hardwareExceptionBlock) HardwareException(pointers);
        r = ProcessException(*h);
    } __except (1) {
        MessageBoxA(nullptr, "An error occurred trying to display the fatal error window! (1)", "Fatal Error",
                    MB_ICONHAND);
    }
    return r;
}

// With `ignore` set it returns 1 holding the lock (as the original).
int __cdecl ExceptionHandler::FilterException(void* pointers)
{
    EnterCriticalSection(HandlerWork::Lock());
    if (ignore) return 1;
    const int r = DoFilterException(pointers);
    if (r == -1) CriticalStartup();
    LeaveCriticalSection(HandlerWork::Lock());
    return r;
}

// ---- assertions

bool __cdecl ExceptionHandler::Assert(const char* fmt, ...)
{
    if (!isPromptAssertions) {
        AssertEx e(HandlerWork::File(), line, HandlerWork::Reason());
        throw AssertEx(e);
    }
    AssertEx e(HandlerWork::File(), line, HandlerWork::Reason());
    va_list ap;
    va_start(ap, fmt);
    e.VSetDescriptionf(fmt, ap);
    va_end(ap);
    if (ProcessException(e)) throw AssertEx(e);
    return true;
}

void __cdecl ExceptionHandler::AssertInit(const char* file_, int line_, const char* reason_)
{
    HandlerWork::File() = file_;
    line = line_;
    HandlerWork::Reason() = reason_;
}

void __cdecl ExceptionHandler::SetAssertions(bool prompt) { isPromptAssertions = prompt; }

// ---- the critical lists

void __cdecl ExceptionHandler::CriticalShutdown()
{
    HandlerWork::RunList(criticalShutdown, HandlerWork::InShutdown(), "Critical Shutdown ...",
                         "Critical Shutdown failure!");
}

void __cdecl ExceptionHandler::CriticalStartup()
{
    HandlerWork::RunList(criticalStartup, HandlerWork::InStartup(), "Critical Startup ...", "Critical Startup failure!");
}

void* __cdecl ExceptionHandler::AddCriticalShutdown(CallBack fn, void* arg) { return HandlerWork::Add(criticalShutdown, fn, arg); }
void* __cdecl ExceptionHandler::AddCriticalStartup(CallBack fn, void* arg) { return HandlerWork::Add(criticalStartup, fn, arg); }
void __cdecl ExceptionHandler::RemoveCriticalShutdown(void* handle) { HandlerWork::Remove(criticalShutdown, handle); }
void __cdecl ExceptionHandler::RemoveCriticalStartup(void* handle) { HandlerWork::Remove(criticalStartup, handle); }

// CoreDumps (default on), CorePrefix (at most 10 characters), CoreOptions
void __cdecl ExceptionHandler::Init(const Options* options)
{
    AnsiString prefix(corePrefix);
    options->Get("CoreDumps", doCoreDump, true);
    options->Get("CorePrefix", static_cast<String&>(prefix), static_cast<const String&>(prefix));
    options->Get("CoreOptions", dumpOptions, dumpOptions);
    std::strcpy(corePrefix, HandlerWork::Clip(prefix));
}

// ---- the report dialog's list box (item 0x3ea)

void __cdecl ExceptionHandler::InitExceptionDialog(void* dialog, Exception& e)
{
    HWND d = static_cast<HWND>(dialog);
    HDC dc = GetDC(GetDlgItem(d, 0x3ea));
    if (!dc) {
        SetDlgItemTextA(d, 0x3ea, "Problem creating exception report");
        return;
    }
    if (e.File() && *e.File()) LogLine(d, dc, "File: %hs\r\n", e.File());
    if (e.Line() > 0) LogLine(d, dc, "Line: %d\r\n", e.Line());
    LogLine(d, dc, "Class: %s\r\n", e.Class());
    LogLine(d, dc, "Reason: %s\r\n", e.Reason());
    const char* stack = e.Description();
    if (stack && *stack) LogLine(d, dc, stack);
    Log::Shutdown(false);
    ReleaseDC(d, dc);  // the dialog's, as in the original (the DC is the list box's)
}

// The formatted text line by line into the list box (and the log), widening its horizontal extent to the widest
void __cdecl ExceptionHandler::LogLine(void* dialog, void* dc, const char* fmt, ...)
{
    struct Text : AnsiString {
        const int* Data() const { return static_cast<const int*>(data); }
        const char* Chars() const { return Data()[0] ? reinterpret_cast<const char*>(Data() + 2) : nullptr; }
        unsigned int Length() const { return static_cast<unsigned int>(Data()[1]); }
    };
    AnsiString all;
    va_list ap;
    va_start(ap, fmt);
    all.Set(fmt, ap);
    va_end(ap);
    HWND list = GetDlgItem(static_cast<HWND>(dialog), 0x3ea);
    HGDIOBJ font = reinterpret_cast<HGDIOBJ>(SendMessageA(list, WM_GETFONT, 0, 0));
    if (!font) {
        font = reinterpret_cast<HGDIOBJ>(SendMessageA(static_cast<HWND>(dialog), WM_GETFONT, 0, 0));
        if (!font) font = GetStockObject(DEFAULT_GUI_FONT);
    }
    LONG widest = 0;
    AnsiString part = all.Token("\r\n");
    while (static_cast<const Text&>(part).Length()) {
        const Text& t = static_cast<const Text&>(part);
        LogClient* c = HandlerWork::Client();
        if (c && HandlerWork::Open(c)) HandlerWork::SayText(c, t.Chars());
        SendMessageA(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(t.Chars()));
        if (font) {
            HDC hdc = static_cast<HDC>(dc);
            if (HGDIOBJ old = SelectObject(hdc, font)) {
                SIZE size;
                if (GetTextExtentPoint32A(hdc, t.Chars(), static_cast<int>(t.Length()), &size) && size.cx > widest) {
                    if (const int border = GetSystemMetrics(SM_CXBORDER)) {
                        SendMessageA(list, LB_SETHORIZONTALEXTENT, size.cx + border * 3, 0);
                        widest = size.cx;
                    }
                }
                SelectObject(hdc, old);
            }
        }
        part = all.Token("\r\n");
    }
}

// ---- HardwareException

// 0x1001cd60 (U16): a bounded x86 EBP walk for the crash report. The original also resolved each frame to a
// module and offset through its own image helper; the frame address and a zero offset are enough here, and the
// walk stops on any unreadable or non-increasing frame so a corrupt stack cannot loop.
void __cdecl HardwareException::GetCallStack(void* p)
{
    char* const out = tmpDescription;
    out[0] = '\0';
    if (!p) return;
    const auto* const ep = static_cast<const EXCEPTION_POINTERS*>(p);
    const CONTEXT* const c = ep->ContextRecord;
    if (!c) return;

    std::snprintf(out, sizeof tmpDescription,
                  " \r\nException @ %08X in %hs\r\n \r\nRecoverable exception: %hs\r\n \r\n"
                  "  EAX: %08X     EBX: %08X     ECX: %08X     EDX: %08X\r\n"
                  "  ESP: %08X     EBP: %08X     ESI: %08X     EDI: %08X\r\n \r\n"
                  "   DS: %08X      FS: %08X\r\n   ES: %08X      GS: %08X\r\n"
                  "   SS: %08X  CS:EIP: %04X:%08X\r\n \r\n"
                  "Flags: %08X (CF:%d ZF:%d SF:%d)\r\n \r\nCall Stack:\r\n \r\n",
                  c->Eip, "Hardware", reason, c->Eax, c->Ebx, c->Ecx, c->Edx, c->Esp, c->Ebp, c->Esi, c->Edi,
                  c->SegDs, c->SegFs, c->SegEs, c->SegGs, c->SegSs, c->SegCs, c->Eip, c->EFlags,
                  (c->EFlags >> 0) & 1, (c->EFlags >> 6) & 1, (c->EFlags >> 7) & 1);

    const auto* frame = reinterpret_cast<const unsigned*>(c->Ebp);
    for (unsigned depth = 0; depth < 50; ++depth) {
        if (!frame || (reinterpret_cast<uintptr_t>(frame) & 3) || IsBadReadPtr(frame, 8)) break;
        const unsigned ret = frame[1];
        const auto* const next = reinterpret_cast<const unsigned*>(frame[0]);
        if (!ret) break;
        const size_t used = std::strlen(out);
        if (used + 48 >= sizeof tmpDescription) break;
        std::snprintf(out + used, sizeof tmpDescription - used, " %p - %08I64X(+%I64X) %hs\r\n",
                      reinterpret_cast<const void*>(ret), static_cast<unsigned long long>(0),
                      static_cast<unsigned long long>(0), "?");
        if (next <= frame) break;
        frame = next;
    }
}

HardwareException::HardwareException(void* p) : Exception(), pointers(p)
{
    const unsigned long code = static_cast<EXCEPTION_POINTERS*>(p)->ExceptionRecord->ExceptionCode;
    int i = 0;
    for (; i < 21; ++i) {
        if (exceptionTypes[i].code == code) {
            std::strcpy(reason, exceptionTypes[i].name);
            break;
        }
    }
    if (i == 21) SetReasonf("Unknown Exception - %08xh", code);
    GetCallStack(p);
}

HardwareException::HardwareException(const HardwareException& o) : Exception(o), pointers(o.pointers) {}

HardwareException& HardwareException::operator=(const HardwareException& o)
{
    Exception::operator=(o);
    pointers = o.pointers;
    return *this;
}

HardwareException::~HardwareException() {}
const char* __cdecl HardwareException::Class() const { return "Hardware"; }

}  // namespace Jet

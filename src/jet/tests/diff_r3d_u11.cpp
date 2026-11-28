// Differential test of Render3DCore_7c unit U11: our BackgroundResource and BackgroundLoadingThread against
// Render3DCore_7c_orig.dll in one process, with hand-built objects whose virtual calls land in this file.
//
// The state machine, the observer protocol and the queues are driven through the exported entry points with the
// same fake resource, fake loading thread, fake observers and fake ResourceDatabase on both sides. Every visible
// field, return value and virtual call (with its arguments) is compared. The worker thread itself is only created
// and destroyed; the asynchronous load path (QueueResource success -> ProcessThread -> SignalComplete ->
// CheckForComplete) is not driven here (see notes/agents/u11_statics.md).
//
// usage: diff_r3d_u11.exe <dir with Render3DCore_7c.dll, Render3DCore_7c_orig.dll and the Jet modules> [rounds]
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

HMODULE mod[2];
HMODULE kernelStandard;
unsigned long long checks, bad;
int g_round;
void* g_lastFound;

LONG WINAPI OnCrash(EXCEPTION_POINTERS* info)
{
    const char* names[] = {"KernelStandard_7c.dll", "Kernel_7c.dll", "KernelResource_7c.dll", "RenderSystem_7c.dll"};
    std::printf("crash code %08lx at %p (ours %p orig %p)\n", info->ExceptionRecord->ExceptionCode,
                info->ExceptionRecord->ExceptionAddress, static_cast<void*>(mod[0]), static_cast<void*>(mod[1]));
    for (const char* name : names) {
        HMODULE m = GetModuleHandleA(name);
        std::printf("  %s at %p\n", name, static_cast<void*>(m));
    }
    CONTEXT* c = info->ContextRecord;
    std::printf("  eax %08lx ebx %08lx ecx %08lx edx %08lx esi %08lx edi %08lx esp %08lx ebp %08lx\n", c->Eax,
                c->Ebx, c->Ecx, c->Edx, c->Esi, c->Edi, c->Esp, c->Ebp);
    std::printf("  g_lastFound %p\n", g_lastFound);
    return EXCEPTION_EXECUTE_HANDLER;
}

void Check(bool ok, const char* what)
{
    ++checks;
    if (!ok) {
        ++bad;
        std::printf("MISMATCH %s (round %d)\n", what, g_round);
    }
}

void CheckInt(int a, int b, const char* what)
{
    if (a != b) std::printf(" ours %d orig %d\n", a, b);
    Check(a == b, what);
}

// ---- the fake objects -----------------------------------------------------------------------------------------

int g_log[1024];
int g_logN;
int g_hook6;          // BackgroundResource's main-thread initialize hook: 0 ok, 2 failed
int g_hook7;          // its background load hook: 0 ok, 2 failed, 3 retry
int g_mainComplete;   // MainThreadLoadComplete / ProcessFailure: 1 loaded, 2 failed
int g_queueResult;    // QueueResource / DequeueResource / SignalComplete result code
int g_observerResult[4];
int g_databaseResult;  // the fake ResourceDatabase::Get: 0 none, 1 the fake resource

void Log(int code)
{
    if (g_logN < static_cast<int>(sizeof g_log / sizeof g_log[0])) g_log[g_logN++] = code;
}

int TakeLog(int* out)
{
    const int n = g_logN;
    std::memcpy(out, g_log, sizeof(int) * static_cast<size_t>(n));
    g_logN = 0;
    return n;
}

bool SameLog(const int* a, int an, const int* b, int bn, const char* what)
{
    bool same = an == bn;
    if (!same) {
        std::printf(" log counts ours %d orig %d\n", an, bn);
        std::printf(" ours:");
        for (int i = 0; i < an; ++i) std::printf(" %d", a[i]);
        std::printf("\n orig:");
        for (int i = 0; i < bn; ++i) std::printf(" %d", b[i]);
        std::printf("\n");
    }
    if (same) {
        for (int i = 0; i < an; ++i) {
            if (a[i] != b[i]) {
                std::printf(" log[%d] ours %d orig %d\n", i, a[i], b[i]);
                same = false;
                break;
            }
        }
    }
    Check(same, what);
    return same;
}

enum FakeKind { FAKE_RESOURCE = 0, FAKE_THREAD = 1, FAKE_DATABASE = 2, FAKE_OBSERVER = 3 };

// one side's fake objects; the CriticalSection inside bkg+0x20 is a real KernelStandard object so both
// implementations can lock it
struct alignas(16) Fixture {
    unsigned char bkg[0x2c];
    unsigned char resource[0x5c];
    unsigned char thread[8];
    unsigned char observer[8];
    unsigned char database[8];
    unsigned char observerEntry[8];  // {BkgResObserver*, unsigned long flags}
    unsigned char name[4];           // a PString's storage, only its address is used
};

Fixture fixtures[2];

int FixtureSide(const void* object, FakeKind kind)
{
    for (int side = 0; side < 2; ++side) {
        const unsigned char* p = nullptr;
        switch (kind) {
        case FAKE_RESOURCE: p = fixtures[side].resource; break;
        case FAKE_THREAD: p = fixtures[side].thread; break;
        case FAKE_DATABASE: p = fixtures[side].database; break;
        default: p = fixtures[side].observer; break;
        }
        if (object == p) return side;
    }
    return -1;
}

// every virtual the two classes call on the embedded fake objects is __cdecl with `this` on the stack
void* __cdecl FakeResourceLock(void*, void* out)
{
    Log(100);
    if (out) *static_cast<int*>(out) = 0;
    return out;
}

void __cdecl FakeResourceUnlock(void*)
{
    Log(101);
}

void __cdecl FakeResourceAddObserver(void* self, const void* observer)
{
    const int side = FixtureSide(self, FAKE_RESOURCE);
    Log(102 + (side >= 0 && observer == fixtures[side].bkg ? 0 : 1000));
}

void __cdecl FakeResourceRemoveObserver(void* self, const void* observer)
{
    const int side = FixtureSide(self, FAKE_RESOURCE);
    Log(103 + (side >= 0 && observer == fixtures[side].bkg ? 0 : 1000));
}

bool __cdecl FakeResourceMustReset(void*)
{
    Log(104);
    return false;
}

// BackgroundResource slots: 0 OnLoad, 1 deleting dtor, 2 DeleteSelf, 3/4 LoadResource, 5 Abort, 6/7 hooks, 8/9
// main-thread status
void __cdecl FakeBkgOnLoad(void*, const void*)
{
    Log(200);
}

void* __fastcall FakeBkgDeletingDtor(void* self, unsigned int, unsigned int flags)
{
    Log(201 + static_cast<int>(flags & 0xf) * 16);
    return self;
}

void __cdecl FakeBkgDeleteSelf(void*)
{
    Log(202);
}

bool __cdecl FakeBkgLoadResourceDb(void*, const void*, const void*)
{
    Log(203);
    return true;
}

bool __cdecl FakeBkgLoadResource(void*, const void*)
{
    Log(204);
    return true;
}

void __cdecl FakeBkgAbort(void*, bool abort)
{
    Log(205 + (abort ? 1 : 0));
}

int __cdecl FakeBkgHook6(void*)
{
    Log(206);
    return g_hook6;
}

int __cdecl FakeBkgHook7(void*)
{
    Log(207);
    return g_hook7;
}

int __cdecl FakeBkgMainComplete(void*)
{
    Log(208);
    return g_mainComplete;
}

int __cdecl FakeBkgMainFailure(void*)
{
    Log(209);
    return g_mainComplete;
}

// BackgroundLoadingThread slots: 0 DeleteThis, 1 deleting dtor, 2 QueueResource, 3 DequeueResource, 4
// SignalComplete, 5 CheckForComplete, 6 QueueForDelete
void __cdecl FakeThreadDeleteThis(void*)
{
    Log(300);
}

void* __fastcall FakeThreadDeletingDtor(void* self, unsigned int, unsigned int flags)
{
    Log(301 + static_cast<int>(flags & 0xf) * 16);
    return self;
}

void* __cdecl FakeThreadQueue(void* self, void* out, const void* resource)
{
    const int side = FixtureSide(self, FAKE_THREAD);
    Log(302 + (side >= 0 && resource == fixtures[side].bkg ? 0 : 1000));
    if (out) *static_cast<int*>(out) = g_queueResult;
    return out;
}

void* __cdecl FakeThreadDequeue(void* self, void* out, const void* resource)
{
    const int side = FixtureSide(self, FAKE_THREAD);
    Log(303 + (side >= 0 && resource == fixtures[side].bkg ? 0 : 1000));
    if (out) *static_cast<int*>(out) = g_queueResult;
    return out;
}

void* __cdecl FakeThreadSignal(void* self, void* out, const void* resource)
{
    const int side = FixtureSide(self, FAKE_THREAD);
    Log(304 + (side >= 0 && resource == fixtures[side].bkg ? 0 : 1000));
    if (out) *static_cast<int*>(out) = g_queueResult;
    return out;
}

void __cdecl FakeThreadCheck(void*)
{
    Log(305);
}

void __cdecl FakeThreadQueueForDelete(void* self, const void* resource)
{
    const int side = FixtureSide(self, FAKE_THREAD);
    Log(306 + (side >= 0 && resource == fixtures[side].bkg ? 0 : 1000));
}

// observers: the four callbacks of BkgResObserver
int __cdecl FakeObserver0(void* self, void* resource)
{
    Log(400 + (FixtureSide(self, FAKE_OBSERVER) >= 0 && resource != nullptr ? 0 : 1000));
    return g_observerResult[0];
}

int __cdecl FakeObserver1(void* self, void* resource)
{
    Log(410 + (FixtureSide(self, FAKE_OBSERVER) >= 0 && resource != nullptr ? 0 : 1000));
    return g_observerResult[1];
}

int __cdecl FakeObserver2(void* self, void* resource)
{
    Log(420 + (FixtureSide(self, FAKE_OBSERVER) >= 0 && resource != nullptr ? 0 : 1000));
    return g_observerResult[2];
}

int __cdecl FakeObserver3(void* self, void* resource)
{
    Log(430 + (FixtureSide(self, FAKE_OBSERVER) >= 0 && resource != nullptr ? 0 : 1000));
    return g_observerResult[3];
}

// the fake ResourceDatabase: slot 1 Get(name, flags) hands back the side's fake resource
void* __cdecl FakeDatabaseGet(void* self, const void*, int flags)
{
    Log(500 + flags);
    const int side = FixtureSide(self, FAKE_DATABASE);
    g_lastFound = g_databaseResult && side >= 0 ? fixtures[side].resource : nullptr;
    return g_lastFound;
}

using Ptr = void*;

Ptr kVtableResource[0x18] = {
    reinterpret_cast<Ptr>(&FakeResourceLock),           // 0 Lock
    reinterpret_cast<Ptr>(&FakeResourceUnlock),         // 1 Unlock
    nullptr,                                            // 2 LoadAll
    nullptr,                                            // 3 IsAllLoaded
    nullptr,                                            // 4 SetPriority
    reinterpret_cast<Ptr>(&FakeResourceAddObserver),    // 5 AddObserver
    reinterpret_cast<Ptr>(&FakeResourceRemoveObserver), // 6 RemoveObserver
    reinterpret_cast<Ptr>(&FakeResourceMustReset),      // 7 GetMustResetWhenFinished
};

Ptr kVtableBkg[10] = {
    reinterpret_cast<Ptr>(&FakeBkgOnLoad),          // 0
    reinterpret_cast<Ptr>(&FakeBkgDeletingDtor),    // 1
    reinterpret_cast<Ptr>(&FakeBkgDeleteSelf),      // 2
    reinterpret_cast<Ptr>(&FakeBkgLoadResourceDb),  // 3
    reinterpret_cast<Ptr>(&FakeBkgLoadResource),    // 4
    reinterpret_cast<Ptr>(&FakeBkgAbort),           // 5
    reinterpret_cast<Ptr>(&FakeBkgHook6),           // 6
    reinterpret_cast<Ptr>(&FakeBkgHook7),           // 7
    reinterpret_cast<Ptr>(&FakeBkgMainComplete),    // 8
    reinterpret_cast<Ptr>(&FakeBkgMainFailure),     // 9
};

Ptr kVtableThread[7] = {
    reinterpret_cast<Ptr>(&FakeThreadDeleteThis),     // 0
    reinterpret_cast<Ptr>(&FakeThreadDeletingDtor),   // 1
    reinterpret_cast<Ptr>(&FakeThreadQueue),          // 2
    reinterpret_cast<Ptr>(&FakeThreadDequeue),        // 3
    reinterpret_cast<Ptr>(&FakeThreadSignal),         // 4
    reinterpret_cast<Ptr>(&FakeThreadCheck),          // 5
    reinterpret_cast<Ptr>(&FakeThreadQueueForDelete), // 6
};

Ptr kVtableDatabase[5] = {
    nullptr,                                 // 0 dtor
    reinterpret_cast<Ptr>(&FakeDatabaseGet), // 1 Get
    nullptr,                                 // 2 Exists
    nullptr,                                 // 3 GetFileName
    nullptr,                                 // 4 GetSearchHandle
};

Ptr kVtableObserver[4] = {
    reinterpret_cast<Ptr>(&FakeObserver0),
    reinterpret_cast<Ptr>(&FakeObserver1),
    reinterpret_cast<Ptr>(&FakeObserver2),
    reinterpret_cast<Ptr>(&FakeObserver3),
};

template <class T>
T Proc(int side, const char* name)
{
    auto p = reinterpret_cast<T>(GetProcAddress(mod[side], name));
    if (!p) {
        std::printf("missing export %s in %s\n", name, side ? "orig" : "ours");
        std::exit(2);
    }
    return p;
}

// ---- the exported entry points, one set per side --------------------------------------------------------------

struct Api {
    using CtorFn = void(__thiscall*)(void*, void*);
    using DtorFn = void(__thiscall*)(void*);
    CtorFn bkgCtor;
    DtorFn bkgDtor;
    int(__cdecl* doAction)(void*, int);
    int(__cdecl* getState)(const void*);
    const void*(__cdecl* getResource)(const void*);
    void(__cdecl* setBackgroundThread)(void*, void*);
    bool(__cdecl* checkComplete)(void*);
    void(__cdecl* loadFailed)(void*);
    void(__cdecl* backgroundLoadData)(void*);
    void(__cdecl* queueBackground)(void*);
    void(__cdecl* unloadResource)(void*);
    void(__cdecl* abort)(void*, bool);
    void(__cdecl* addObserver)(void*, void*, unsigned long);
    void(__cdecl* removeObserver)(void*, void*);
    int(__cdecl* callCallbacks)(const void*, unsigned long);
    void(__cdecl* deleteSelf)(void*);
    bool(__cdecl* loadResource)(void*, const void*);
    bool(__cdecl* loadResourceDb)(void*, const void*, const void*);

    using ThreadCtorFn = void(__thiscall*)(void*, int, int);
    ThreadCtorFn threadCtor;
    DtorFn threadDtor;
    unsigned long(__cdecl* getThreadID)(const void*);
    void(__cdecl* setThreadPriority)(void*, int);
    void(__cdecl* queueResource)(void*, int*, void*);
    void(__cdecl* dequeueResource)(void*, int*, void*);
    void(__cdecl* signalComplete)(void*, int*, void*);
    void(__cdecl* checkForComplete)(void*);
    void(__cdecl* queueForDelete)(void*, void*);
};

static Api api[2];

using CriticalCtor = void(__thiscall*)(void*, const char*);
using CriticalDtor = void(__thiscall*)(void*);
static CriticalCtor criticalCtor;
static CriticalDtor criticalDtor;

// BackgroundResource field offsets
enum {
    BKG_THREAD = 0x04,
    BKG_RESOURCE = 0x08,
    BKG_OBSERVERS = 0x0c,
    BKG_CAPACITY = 0x10,
    BKG_DATA = 0x14,
    BKG_LOCKED = 0x18,
    BKG_DELETE_ABORT = 0x19,
    BKG_DELETE_FAILURE = 0x1a,
    BKG_STATE = 0x1c,
    BKG_SECTION = 0x20,
};
enum { RES_PRIORITY = 0x14, RES_REFS = 0x1c, RES_STATUS = 0x28 };

static int ReadInt(const void* p, int offset)
{
    int v;
    std::memcpy(&v, static_cast<const unsigned char*>(p) + offset, sizeof v);
    return v;
}

static void WriteInt(void* p, int offset, int v)
{
    std::memcpy(static_cast<unsigned char*>(p) + offset, &v, sizeof v);
}

static void WritePtr(void* p, int offset, void* v)
{
    std::memcpy(static_cast<unsigned char*>(p) + offset, &v, sizeof v);
}

static void* ReadPtr(const void* p, int offset)
{
    void* v;
    std::memcpy(&v, static_cast<const unsigned char*>(p) + offset, sizeof v);
    return v;
}

// a fake BackgroundResource with a real CriticalSection at +0x20
static void InitFakeBkg(int side, int state, bool locked, bool deleteOnAbort, bool deleteOnFailure)
{
    Fixture& f = fixtures[side];
    std::memset(f.bkg, 0, sizeof f.bkg);
    WritePtr(f.bkg, 0, kVtableBkg);
    WritePtr(f.bkg, BKG_THREAD, f.thread);
    WritePtr(f.bkg, BKG_RESOURCE, f.resource);
    f.bkg[BKG_LOCKED] = locked ? 1 : 0;
    f.bkg[BKG_DELETE_ABORT] = deleteOnAbort ? 1 : 0;
    f.bkg[BKG_DELETE_FAILURE] = deleteOnFailure ? 1 : 0;
    WriteInt(f.bkg, BKG_STATE, state);
    criticalCtor(f.bkg + BKG_SECTION, nullptr);
}

static void FreeFakeBkg(int side)
{
    criticalDtor(fixtures[side].bkg + BKG_SECTION);
}

// a fake Resource: the non-virtual Resource members the state machine calls work on these fields
static void InitFakeResource(int side, int priority, int status, int refs)
{
    Fixture& f = fixtures[side];
    std::memset(f.resource, 0, sizeof f.resource);
    WritePtr(f.resource, 0, kVtableResource);
    WriteInt(f.resource, RES_PRIORITY, priority);
    WriteInt(f.resource, RES_REFS, refs);
    WriteInt(f.resource, RES_STATUS, status);
}

static void InitFakeThread(int side)
{
    Fixture& f = fixtures[side];
    std::memset(f.thread, 0, sizeof f.thread);
    WritePtr(f.thread, 0, kVtableThread);
}

static void InitFakeDatabase(int side)
{
    Fixture& f = fixtures[side];
    std::memset(f.database, 0, sizeof f.database);
    WritePtr(f.database, 0, kVtableDatabase);
}

static void InitFakeObserver(int side)
{
    Fixture& f = fixtures[side];
    std::memset(f.observer, 0, sizeof f.observer);
    WritePtr(f.observer, 0, kVtableObserver);
}

struct BkgFields {
    bool threadIsFake;
    bool resourceIsFake;
    int observerCount;
    int observerCapacity;
    bool hasData;
    unsigned char flags[3];
    int state;
    int resourceRefs;
};

static BkgFields FieldsOf(const void* bkg, int side)
{
    const Fixture& f = fixtures[side];
    BkgFields out{};
    out.threadIsFake = ReadPtr(bkg, BKG_THREAD) == static_cast<const void*>(f.thread);
    out.resourceIsFake = ReadPtr(bkg, BKG_RESOURCE) == static_cast<const void*>(f.resource);
    out.observerCount = ReadInt(bkg, BKG_OBSERVERS);
    out.observerCapacity = ReadInt(bkg, BKG_CAPACITY);
    out.hasData = ReadPtr(bkg, BKG_DATA) != nullptr;
    out.flags[0] = static_cast<const unsigned char*>(bkg)[BKG_LOCKED];
    out.flags[1] = static_cast<const unsigned char*>(bkg)[BKG_DELETE_ABORT];
    out.flags[2] = static_cast<const unsigned char*>(bkg)[BKG_DELETE_FAILURE];
    out.state = ReadInt(bkg, BKG_STATE);
    out.resourceRefs = ReadInt(f.resource, RES_REFS);
    return out;
}

static BkgFields Fields(int side)
{
    return FieldsOf(fixtures[side].bkg, side);
}

static bool SameFields(const BkgFields& a, const BkgFields& b)
{
    return a.threadIsFake == b.threadIsFake && a.resourceIsFake == b.resourceIsFake &&
           a.observerCount == b.observerCount && a.observerCapacity == b.observerCapacity &&
           a.hasData == b.hasData && a.flags[0] == b.flags[0] && a.flags[1] == b.flags[1] &&
           a.flags[2] == b.flags[2] && a.state == b.state && a.resourceRefs == b.resourceRefs;
}

static void PrintFields(const char* tag, const BkgFields& f)
{
    std::printf(" %s thread %d resource %d observers %d/%d data %d flags %d%d%d state %d refs %d\n", tag,
                f.threadIsFake, f.resourceIsFake, f.observerCount, f.observerCapacity, f.hasData, f.flags[0],
                f.flags[1], f.flags[2], f.state, f.resourceRefs);
}

static void CheckFields(const BkgFields& a, const BkgFields& b, const char* what)
{
    const bool same = SameFields(a, b);
    if (!same) {
        PrintFields("ours ", a);
        PrintFields("orig ", b);
    }
    Check(same, what);
}

// ---- the tests ------------------------------------------------------------------------------------------------

static void CheckVtableSlot(int side, const void* const* vtable, int slot, const char* exportName, const char* what)
{
    FARPROC const p = GetProcAddress(mod[side], exportName);
    if (!p) {
        std::printf("missing export %s\n", exportName);
        std::exit(2);
    }
    Check(vtable[slot] == reinterpret_cast<const void*>(p), what);
}

static void TestRealObject()
{
    // the exported default constructor closure must build the same object as the plain constructor
    for (int side = 0; side < 2; ++side) {
        unsigned char object[0x2c];
        std::memset(object, 0xa5, sizeof object);
        auto closure = Proc<void(__thiscall*)(void*)>(side, "??_FBackgroundResource@Jet@@QAEXXZ");
        closure(object);
        Check(ReadInt(object, BKG_OBSERVERS) == 0, "closure observer count");
        Check(ReadPtr(object, BKG_THREAD) == nullptr, "closure thread");
        Check(ReadPtr(object, BKG_RESOURCE) == nullptr, "closure resource");
        Check(ReadInt(object, BKG_STATE) == 0, "closure state");
        api[side].bkgDtor(object);
    }

    unsigned char objects[2][0x2c];
    for (int side = 0; side < 2; ++side) {
        std::memset(objects[side], 0xa5, sizeof objects[side]);
        api[side].bkgCtor(objects[side], nullptr);
        Check(ReadInt(objects[side], BKG_OBSERVERS) == 0, "ctor observer count");
        Check(ReadInt(objects[side], BKG_CAPACITY) == 0, "ctor observer capacity");
        Check(ReadPtr(objects[side], BKG_DATA) == nullptr, "ctor observer data");
        Check(ReadPtr(objects[side], BKG_THREAD) == nullptr, "ctor thread");
        Check(ReadPtr(objects[side], BKG_RESOURCE) == nullptr, "ctor resource");
        Check(objects[side][BKG_LOCKED] == 0 && objects[side][BKG_DELETE_ABORT] == 0 &&
                  objects[side][BKG_DELETE_FAILURE] == 0,
              "ctor flags");
        Check(ReadInt(objects[side], BKG_STATE) == 0, "ctor state");
        Check(api[side].getState(objects[side]) == 0, "GetCurrentState");
        Check(api[side].getResource(objects[side]) == nullptr, "GetResource");
        void* vtable = ReadPtr(objects[side], 0);
        Check(vtable != nullptr, "ctor vtable");
        api[side].setBackgroundThread(objects[side], fixtures[side].thread);
        Check(ReadPtr(objects[side], BKG_THREAD) == static_cast<void*>(fixtures[side].thread), "SetBackgroundThread");
        api[side].setBackgroundThread(objects[side], nullptr);
        Check(ReadPtr(objects[side], BKG_THREAD) == nullptr, "SetBackgroundThread(null)");
        // the vtable order must be the original's (the overloads are easy to put in the wrong order)
        const void* const* const vtbl = static_cast<const void* const*>(ReadPtr(objects[side], 0));
        CheckVtableSlot(side, vtbl, 0, "?OnLoad@BackgroundResource@Jet@@MBAXPBVResource@2@@Z", "vtable OnLoad");
        CheckVtableSlot(side, vtbl, 2, "?DeleteSelf@BackgroundResource@Jet@@UAAXXZ", "vtable DeleteSelf");
        CheckVtableSlot(side, vtbl, 5, "?Abort@BackgroundResource@Jet@@UAAX_N@Z", "vtable Abort");
        CheckVtableSlot(side, vtbl, 8, "?MainThreadLoadComplete@BackgroundResource@Jet@@MBA?AW4BackgroundLoadStatus@2@XZ",
                        "vtable MainThreadLoadComplete");
        CheckVtableSlot(side, vtbl, 9, "?MainThreadProcessFailure@BackgroundResource@Jet@@MBA?AW4BackgroundLoadStatus@2@XZ",
                        "vtable MainThreadProcessFailure");
    }
    for (int side = 0; side < 2; ++side) {
        const char* name = "?mainThreadId@BackgroundResource@Jet@@1KA";
        const unsigned long* id = reinterpret_cast<const unsigned long*>(GetProcAddress(mod[side], name));
        if (!id) {
            std::printf("missing export %s\n", name);
            std::exit(2);
        }
        Check(*id == GetCurrentThreadId(), "BackgroundResource::mainThreadId");
    }
    for (int side = 0; side < 2; ++side) api[side].bkgDtor(objects[side]);
}

static void RunDoActionCase(int state, int input, bool locked, bool deleteOnAbort, bool deleteOnFailure,
                            int resourceStatus, const char* what)
{
    int logA[1024];
    int logB[1024];
    InitFakeResource(0, 0x3f800000, resourceStatus, 2);
    InitFakeBkg(0, state, locked, deleteOnAbort, deleteOnFailure);
    g_logN = 0;
    const int a = api[0].doAction(fixtures[0].bkg, input);
    const int logAN = TakeLog(logA);
    const BkgFields fieldsA = Fields(0);
    FreeFakeBkg(0);

    InitFakeResource(1, 0x3f800000, resourceStatus, 2);
    InitFakeBkg(1, state, locked, deleteOnAbort, deleteOnFailure);
    g_logN = 0;
    const int b = api[1].doAction(fixtures[1].bkg, input);
    const int logBN = TakeLog(logB);
    const BkgFields fieldsB = Fields(1);
    FreeFakeBkg(1);

    CheckInt(a, b, what);
    CheckFields(fieldsA, fieldsB, what);
    SameLog(logA, logAN, logB, logBN, what);
}

static void TestStateMachine()
{
    char name[96];
    ++g_round;
    // A: healthy hooks and a healthy resource
    g_hook6 = 0;
    g_hook7 = 0;
    g_mainComplete = 1;
    g_queueResult = 0;
    for (int state = 0; state < 10; ++state) {
        for (int input = 0; input < 9; ++input) {
            std::snprintf(name, sizeof name, "DoAction state %d input %d", state, input);
            RunDoActionCase(state, input, false, false, false, 6, name);
        }
    }
    // B: failures, a full queue, delete-on-abort/failure and a locked resource
    g_hook6 = 2;
    g_hook7 = 2;
    g_mainComplete = 2;
    g_queueResult = 3;
    for (int state = 0; state < 10; ++state) {
        for (int input = 0; input < 9; ++input) {
            std::snprintf(name, sizeof name, "failing DoAction state %d input %d", state, input);
            RunDoActionCase(state, input, true, true, true, 6, name);
        }
    }
    // C: the background hook asks for a retry
    g_hook6 = 0;
    g_hook7 = 3;
    g_mainComplete = 1;
    g_queueResult = 0;
    for (int state = 0; state < 10; ++state) {
        std::snprintf(name, sizeof name, "retry DoAction state %d", state);
        RunDoActionCase(state, 4, false, false, false, 6, name);
    }
    // D: the resource is not available, so QueueRequest takes the Cache path instead of recursing
    for (int state = 0; state < 10; ++state) {
        std::snprintf(name, sizeof name, "unavailable resource DoAction state %d", state);
        RunDoActionCase(state, 0, false, false, false, 8, name);
    }
}

static void TestEntryPoints()
{
    struct Case {
        int state;
        int which;  // 0 CheckComplete, 1 LoadFailed, 2 BackgroundLoadData, 3 QueueBackground, 4 UnloadResource
    };
    const Case cases[] = {{0, 0}, {8, 0}, {0, 1}, {4, 2}, {0, 2}, {0, 3}, {4, 4}, {0, 4}};
    for (const Case& c : cases) {
        int logA[1024];
        int logB[1024];
        InitFakeResource(0, 0x3f800000, 6, 2);
        InitFakeBkg(0, c.state, false, false, false);
        int a = 0;
        switch (c.which) {
        case 0:
            a = api[0].checkComplete(fixtures[0].bkg);
            break;
        case 1:
            api[0].loadFailed(fixtures[0].bkg);
            break;
        case 2:
            api[0].backgroundLoadData(fixtures[0].bkg);
            break;
        case 3:
            api[0].queueBackground(fixtures[0].bkg);
            break;
        default:
            api[0].unloadResource(fixtures[0].bkg);
            break;
        }
        const int logAN = TakeLog(logA);
        const BkgFields fieldsA = Fields(0);
        FreeFakeBkg(0);

        InitFakeResource(1, 0x3f800000, 6, 2);
        InitFakeBkg(1, c.state, false, false, false);
        int b = 0;
        switch (c.which) {
        case 0:
            b = api[1].checkComplete(fixtures[1].bkg);
            break;
        case 1:
            api[1].loadFailed(fixtures[1].bkg);
            break;
        case 2:
            api[1].backgroundLoadData(fixtures[1].bkg);
            break;
        case 3:
            api[1].queueBackground(fixtures[1].bkg);
            break;
        default:
            api[1].unloadResource(fixtures[1].bkg);
            break;
        }
        const int logBN = TakeLog(logB);
        const BkgFields fieldsB = Fields(1);
        FreeFakeBkg(1);

        CheckInt(a, b, "entry point return");
        CheckFields(fieldsA, fieldsB, "entry point");
        SameLog(logA, logAN, logB, logBN, "entry point log");
    }
    // Abort sets its flag and runs the abort action; with delete-on-abort the thread sees QueueForDelete
    for (int state = 0; state < 10; ++state) {
        for (int flag = 0; flag < 2; ++flag) {
            int logA[1024];
            int logB[1024];
            InitFakeResource(0, 0x3f800000, 6, 2);
            InitFakeBkg(0, state, false, flag != 0, false);
            api[0].abort(fixtures[0].bkg, flag != 0);
            const int logAN = TakeLog(logA);
            const BkgFields fieldsA = Fields(0);
            FreeFakeBkg(0);
            InitFakeResource(1, 0x3f800000, 6, 2);
            InitFakeBkg(1, state, false, flag != 0, false);
            api[1].abort(fixtures[1].bkg, flag != 0);
            const int logBN = TakeLog(logB);
            const BkgFields fieldsB = Fields(1);
            FreeFakeBkg(1);
            CheckFields(fieldsA, fieldsB, "Abort fields");
            SameLog(logA, logAN, logB, logBN, "Abort log");
        }
    }
    // DeleteSelf goes through the deleting destructor
    for (int side = 0; side < 2; ++side) {
        InitFakeResource(side, 0x3f800000, 6, 2);
        InitFakeBkg(side, 0, false, false, false);
        g_logN = 0;
        api[side].deleteSelf(fixtures[side].bkg);
        Check(g_logN == 1 && g_log[0] == 201 + 16, "DeleteSelf");
        FreeFakeBkg(side);
    }
}

static void TestObservers()
{
    // AddObserver grows the array by fives and keeps the entries
    {
        unsigned char objects[2][0x2c];
        for (int side = 0; side < 2; ++side) {
            std::memset(objects[side], 0, sizeof objects[side]);
            api[side].bkgCtor(objects[side], nullptr);
        }
        for (int count = 1; count <= 12; ++count) {
            for (int side = 0; side < 2; ++side)
                api[side].addObserver(objects[side], fixtures[side].observer, static_cast<unsigned long>(count));
            CheckInt(ReadInt(objects[0], BKG_OBSERVERS), ReadInt(objects[1], BKG_OBSERVERS), "AddObserver count");
            CheckInt(ReadInt(objects[0], BKG_CAPACITY), ReadInt(objects[1], BKG_CAPACITY), "AddObserver capacity");
            const int expected = ((count + 4) / 5) * 5;
            Check(ReadInt(objects[0], BKG_CAPACITY) == expected, "AddObserver growth");
            const int* dataA = static_cast<const int*>(ReadPtr(objects[0], BKG_DATA));
            const int* dataB = static_cast<const int*>(ReadPtr(objects[1], BKG_DATA));
            Check(dataA != nullptr && dataB != nullptr, "AddObserver data");
            for (int i = 0; i < count; ++i) {
                Check(dataA[2 * i] == dataB[2 * i] && dataA[2 * i] == i + 1, "AddObserver flags");
                Check(dataA[2 * i + 1] != 0 && dataB[2 * i + 1] != 0, "AddObserver observer");
            }
        }
        for (int side = 0; side < 2; ++side) api[side].bkgDtor(objects[side]);
    }
    // RemoveObserver keeps the order of the rest
    {
        unsigned char objects[2][0x2c];
        for (int side = 0; side < 2; ++side) {
            std::memset(objects[side], 0, sizeof objects[side]);
            api[side].bkgCtor(objects[side], nullptr);
            api[side].addObserver(objects[side], fixtures[side].observer, 1);
            api[side].addObserver(objects[side], fixtures[side].observer + 4, 2);
            api[side].addObserver(objects[side], fixtures[side].observer, 3);
            api[side].removeObserver(objects[side], fixtures[side].observer);
        }
        CheckInt(ReadInt(objects[0], BKG_OBSERVERS), ReadInt(objects[1], BKG_OBSERVERS), "RemoveObserver count");
        const int* dataA = static_cast<const int*>(ReadPtr(objects[0], BKG_DATA));
        const int* dataB = static_cast<const int*>(ReadPtr(objects[1], BKG_DATA));
        Check(dataA[0] == dataB[0] && dataA[0] == 2, "RemoveObserver first flags");
        Check(dataA[2] == dataB[2] && dataA[2] == 3, "RemoveObserver second flags");        for (int side = 0; side < 2; ++side) api[side].bkgDtor(objects[side]);
    }
    // CallCallbacks: the first flag aborts on false, the others on true
    const int maskCombinations[] = {1, 2, 4, 8, 3, 15};
    for (int mask : maskCombinations) {
        for (int result = 0; result < 2; ++result) {
            unsigned char objects[2][0x2c];
            int logA[16];
            int logB[16];
            int nA = 0;
            int nB = 0;
            int a = 0;
            int b = 0;
            for (int side = 0; side < 2; ++side) {
                std::memset(objects[side], 0, sizeof objects[side]);
                api[side].bkgCtor(objects[side], nullptr);
                Fixture& f = fixtures[side];
                WriteInt(f.observerEntry, 0, mask);
                WritePtr(f.observerEntry, 4, f.observer);
                WriteInt(objects[side], BKG_OBSERVERS, 1);
                WriteInt(objects[side], BKG_CAPACITY, 1);
                WritePtr(objects[side], BKG_DATA, f.observerEntry);
                for (int i = 0; i < 4; ++i) g_observerResult[i] = result;
            }
            g_logN = 0;
            a = api[0].callCallbacks(objects[0], 3);
            nA = TakeLog(logA);
            b = api[1].callCallbacks(objects[1], 3);
            nB = TakeLog(logB);
            CheckInt(a, b, "CallCallbacks");
            SameLog(logA, nA, logB, nB, "CallCallbacks log");
            for (int side = 0; side < 2; ++side) {
                WriteInt(objects[side], BKG_OBSERVERS, 0);
                WriteInt(objects[side], BKG_CAPACITY, 0);
                WritePtr(objects[side], BKG_DATA, nullptr);
                api[side].bkgDtor(objects[side]);
            }
        }
    }
}

static void TestVtableOrder()
{
    // slots 3 and 4 are the two LoadResource overloads; the overloads must be in the original's order. They are
    // called through the vtable with the argument counts the original's vtable implies.
    using DbFn = bool(__cdecl*)(void*, const void*, const void*);
    using ResFn = bool(__cdecl*)(void*, const void*);
    for (int which = 0; which < 2; ++which) {
        unsigned char objects[2][0x2c];
        BkgFields fields[2];
        int results[2];
        for (int side = 0; side < 2; ++side) {
            std::memset(objects[side], 0, sizeof objects[side]);
            api[side].bkgCtor(objects[side], nullptr);
            api[side].setBackgroundThread(objects[side], fixtures[side].thread);
            InitFakeResource(side, 0x3f800000, 6, 1);
            InitFakeDatabase(side);
            g_databaseResult = 1;
            void* const* const vtbl = static_cast<void* const*>(ReadPtr(objects[side], 0));
            if (which == 0) {
                results[side] = reinterpret_cast<DbFn>(vtbl[3])(objects[side], fixtures[side].database,
                                                               fixtures[side].name)
                                    ? 1
                                    : 0;
            } else {
                results[side] = reinterpret_cast<ResFn>(vtbl[4])(objects[side], fixtures[side].resource) ? 1 : 0;
            }
            fields[side] = FieldsOf(objects[side], side);
            api[side].bkgDtor(objects[side]);
        }
        CheckInt(results[0], results[1], which == 0 ? "vtable slot 3 result" : "vtable slot 4 result");
        CheckInt(results[0], 1, which == 0 ? "vtable slot 3 loads" : "vtable slot 4 loads");
        CheckFields(fields[0], fields[1], which == 0 ? "vtable slot 3" : "vtable slot 4");
        CheckInt(fields[0].state, 6, "vtable LoadResource state");
    }
}

static void TestLoadResource()
{
    unsigned char objects[2][0x2c];
    for (int side = 0; side < 2; ++side) {
        std::memset(objects[side], 0, sizeof objects[side]);
        api[side].bkgCtor(objects[side], nullptr);
        api[side].setBackgroundThread(objects[side], fixtures[side].thread);
        InitFakeResource(side, 0x3f800000, 6, 1);
        InitFakeThread(side);
        InitFakeDatabase(side);
    }
    // LoadResource(const Resource*): takes a reference, adds the observer and queues a request
    {
        int logA[1024];
        int logB[1024];
        g_logN = 0;
        const int resultA = api[0].loadResource(objects[0], fixtures[0].resource) ? 1 : 0;
        const int logAN = TakeLog(logA);
        const BkgFields fieldsA = FieldsOf(objects[0], 0);
        g_logN = 0;
        const int resultB = api[1].loadResource(objects[1], fixtures[1].resource) ? 1 : 0;
        const int logBN = TakeLog(logB);
        const BkgFields fieldsB = FieldsOf(objects[1], 1);
        CheckInt(resultA, resultB, "LoadResource(Resource)");
        CheckFields(fieldsA, fieldsB, "LoadResource(Resource) fields");
        CheckInt(fieldsA.state, 6, "LoadResource queued");
        CheckInt(fieldsA.resourceRefs, 2, "LoadResource reference");
        SameLog(logA, logAN, logB, logBN, "LoadResource log");
    }
    // LoadResource(ResourceDatabase*, PString): a miss is false, a hit forwards to LoadResource(Resource*)
    for (int hit = 0; hit < 2; ++hit) {
        for (int side = 0; side < 2; ++side) {
            api[side].bkgDtor(objects[side]);
            std::memset(objects[side], 0, sizeof objects[side]);
            api[side].bkgCtor(objects[side], nullptr);
            api[side].setBackgroundThread(objects[side], fixtures[side].thread);
            InitFakeResource(side, 0x3f800000, 6, 1);
            InitFakeDatabase(side);
        }
        g_databaseResult = hit;
        int logA[1024];
        int logB[1024];
        int resultA = -1;
        int resultB = -1;
        g_logN = 0;
        resultA = api[0].loadResourceDb(objects[0], fixtures[0].database, fixtures[0].name) ? 1 : 0;
        const int logAN = TakeLog(logA);
        const BkgFields fieldsA = FieldsOf(objects[0], 0);
        resultB = api[1].loadResourceDb(objects[1], fixtures[1].database, fixtures[1].name) ? 1 : 0;
        const int logBN = TakeLog(logB);
        const BkgFields fieldsB = FieldsOf(objects[1], 1);
        CheckInt(resultA, resultB, "LoadResource(db,name)");
        CheckInt(resultA, hit, "LoadResource(db,name) result");
        CheckFields(fieldsA, fieldsB, "LoadResource(db,name) fields");
        SameLog(logA, logAN, logB, logBN, "LoadResource(db,name) log");
    }
    for (int side = 0; side < 2; ++side) api[side].bkgDtor(objects[side]);
}

static void TestThread()
{
    unsigned char threads[2][0x44];
    for (int side = 0; side < 2; ++side) {
        std::memset(threads[side], 0, sizeof threads[side]);
        api[side].threadCtor(threads[side], 8, 0);
        Check(ReadInt(threads[side], 0x08) == 0, "thread ctor queue count");
        Check(ReadInt(threads[side], 0x0c) == 8, "thread ctor queue capacity");
        Check(ReadPtr(threads[side], 0x10) != nullptr, "thread ctor queue");
        Check(ReadInt(threads[side], 0x14) == 0, "thread ctor complete count");
        Check(ReadInt(threads[side], 0x18) == 8, "thread ctor complete capacity");
        Check(ReadPtr(threads[side], 0x1c) != nullptr, "thread ctor complete");
        Check(ReadPtr(threads[side], 0x20) == nullptr, "thread ctor delete list");
        Check(ReadPtr(threads[side], 0x24) != nullptr, "thread ctor thread");
        Check(ReadPtr(threads[side], 0x28) == nullptr, "thread ctor current");
        Check(threads[side][0x34] == 0, "thread ctor done");
        WriteInt(threads[side], 0x38, 0x12345678);
        Check(api[side].getThreadID(threads[side]) == 0x12345678ul, "GetThreadID");
        const void* const* const vtable = static_cast<const void* const*>(ReadPtr(threads[side], 0));
        CheckVtableSlot(side, vtable, 2, "?QueueResource@BackgroundLoadingThread@Jet@@UAA?AV?$ResultClass@$1?results@BackgroundLoadingThread@Jet@@2QBUResultTable@2@BW4Results@BackgroundLoadingThread@Jet@@@2@PAVBackgroundResource@2@@Z",
                        "vtable QueueResource");
        CheckVtableSlot(side, vtable, 3, "?DequeueResource@BackgroundLoadingThread@Jet@@UAA?AV?$ResultClass@$1?results@BackgroundLoadingThread@Jet@@2QBUResultTable@2@BW4Results@BackgroundLoadingThread@Jet@@@2@PAVBackgroundResource@2@@Z",
                        "vtable DequeueResource");
        CheckVtableSlot(side, vtable, 4, "?SignalComplete@BackgroundLoadingThread@Jet@@UAA?AV?$ResultClass@$1?results@BackgroundLoadingThread@Jet@@2QBUResultTable@2@BW4Results@BackgroundLoadingThread@Jet@@@2@PAVBackgroundResource@2@@Z",
                        "vtable SignalComplete");
        CheckVtableSlot(side, vtable, 5, "?CheckForComplete@BackgroundLoadingThread@Jet@@UAAXXZ", "vtable CheckForComplete");
        CheckVtableSlot(side, vtable, 6, "?QueueForDelete@BackgroundLoadingThread@Jet@@UAAXPAVBackgroundResource@2@@Z",
                        "vtable QueueForDelete");
        if (side)
            Check(vtable[0] != nullptr, "vtable DeleteThis");
        const unsigned long* id = reinterpret_cast<const unsigned long*>(
            GetProcAddress(mod[side], "?mainThreadId@BackgroundLoadingThread@Jet@@1KA"));
        if (!id) {
            std::printf("missing export mainThreadId (thread)\n");
            std::exit(2);
        }
        Check(*id == GetCurrentThreadId(), "BackgroundLoadingThread::mainThreadId");
    }
    // SetThreadPriority reaches the Thread object; its priority is read back through KernelStandard
    using GetPriorityFn = int(__cdecl*)(const void*);
    GetPriorityFn getPriority = reinterpret_cast<GetPriorityFn>(
        GetProcAddress(kernelStandard, "?GetPriority@Thread@Jet@@QBA?AW4Priority@12@XZ"));
    if (!getPriority) {
        std::printf("missing KernelStandard Thread::GetPriority\n");
        std::exit(2);
    }
    for (int priority = 0; priority < 3; ++priority) {
        for (int side = 0; side < 2; ++side) api[side].setThreadPriority(threads[side], priority);
        CheckInt(getPriority(ReadPtr(threads[0], 0x24)), getPriority(ReadPtr(threads[1], 0x24)),
                 "SetThreadPriority");
    }
    // DequeueResource on an object that is not queued, and on the one the worker has claimed
    for (int side = 0; side < 2; ++side) {
        InitFakeResource(side, 0x3f800000, 6, 2);
        int result = -1;
        api[side].dequeueResource(threads[side], &result, fixtures[side].resource);
        Check(result == 2, "DequeueResource not queued");
        WritePtr(threads[side], 0x28, fixtures[side].resource);
        result = -1;
        api[side].dequeueResource(threads[side], &result, fixtures[side].resource);
        Check(result == 1, "DequeueResource is current");
        WritePtr(threads[side], 0x28, nullptr);
    }
    // QueueResource's full branch: no signal, so the worker stays asleep
    for (int side = 0; side < 2; ++side) {
        WriteInt(threads[side], 0x08, 8);
        int result = -1;
        api[side].queueResource(threads[side], &result, fixtures[side].resource);
        Check(result == 3, "QueueResource full");
        WriteInt(threads[side], 0x08, 0);
    }
    // SignalComplete pushes onto a priority heap keyed by Resource::GetPriority: five distinct objects so the
    // heap order is visible
    struct HeapEntry {
        alignas(16) unsigned char bkg[0x2c];
        alignas(16) unsigned char resource[0x5c];
    };
    static HeapEntry heap[2][5];
    const int priorities[] = {0x40000000, 0x41200000, 0x3f800000, 0x40400000, 0x41000000};
    for (int side = 0; side < 2; ++side) {
        for (int i = 0; i < 5; ++i) {
            HeapEntry& e = heap[side][i];
            std::memset(e.bkg, 0, sizeof e.bkg);
            std::memset(e.resource, 0, sizeof e.resource);
            WritePtr(e.resource, 0, kVtableResource);
            WriteInt(e.resource, RES_PRIORITY, priorities[i]);
            WriteInt(e.resource, RES_REFS, 2);
            WriteInt(e.resource, RES_STATUS, 6);
            WritePtr(e.bkg, 0, kVtableBkg);
            WritePtr(e.bkg, BKG_RESOURCE, e.resource);
            WriteInt(e.bkg, BKG_STATE, 8);
            criticalCtor(e.bkg + BKG_SECTION, nullptr);
        }
    }
    for (int i = 0; i < 5; ++i) {
        for (int side = 0; side < 2; ++side) {
            int result = -1;
            api[side].signalComplete(threads[side], &result, heap[side][i].bkg);
            Check(result == 0, "SignalComplete");
        }
        CheckInt(ReadInt(threads[0], 0x14), ReadInt(threads[1], 0x14), "SignalComplete count");
    }
    for (int side = 0; side < 2; ++side) {
        Check(ReadInt(threads[side], 0x14) == 5, "SignalComplete total");
        const void* const* data = static_cast<const void* const*>(ReadPtr(threads[side], 0x1c));
        void* const rootResource = ReadPtr(data[0], BKG_RESOURCE);
        Check(ReadInt(rootResource, RES_PRIORITY) == 0x41200000, "SignalComplete heap root");
    }
    // CheckForComplete pops the heap and dispatches the check-complete input: state 8 reaches Loaded
    int logA[1024];
    int logB[1024];
    g_logN = 0;
    api[0].checkForComplete(threads[0]);
    const int logAN = TakeLog(logA);
    api[1].checkForComplete(threads[1]);
    const int logBN = TakeLog(logB);
    SameLog(logA, logAN, logB, logBN, "CheckForComplete log");
    for (int i = 0; i < 5; ++i) {
        CheckInt(ReadInt(heap[0][i].bkg, BKG_STATE), ReadInt(heap[1][i].bkg, BKG_STATE),
                 "CheckForComplete state");
        CheckInt(ReadInt(heap[0][i].bkg, BKG_STATE), 9, "CheckForComplete state loaded");
    }
    Check(ReadInt(threads[0], 0x14) == 0 && ReadInt(threads[1], 0x14) == 0, "CheckForComplete drained");
    for (int side = 0; side < 2; ++side)
        for (int i = 0; i < 5; ++i) criticalDtor(heap[side][i].bkg + BKG_SECTION);
    // QueueForDelete pools 13 entries per block and CheckForComplete deletes them all
    for (int side = 0; side < 2; ++side) {
        InitFakeResource(side, 0x3f800000, 6, 2);
        InitFakeBkg(side, 0, false, false, false);
        for (int i = 0; i < 14; ++i) api[side].queueForDelete(threads[side], fixtures[side].bkg);
        Check(ReadPtr(threads[side], 0x20) != nullptr, "QueueForDelete list");
    }
    g_logN = 0;
    api[0].checkForComplete(threads[0]);
    const int deletedA = TakeLog(logA);
    api[1].checkForComplete(threads[1]);
    const int deletedB = TakeLog(logB);
    CheckInt(deletedA, deletedB, "QueueForDelete deleted");
    CheckInt(deletedA, 14, "QueueForDelete deleted count");
    for (int side = 0; side < 2; ++side) {
        Check(ReadPtr(threads[side], 0x20) == nullptr, "QueueForDelete cleared");
        FreeFakeBkg(side);
    }
    for (int side = 0; side < 2; ++side) api[side].threadDtor(threads[side]);
}

// ---- U11 remainder: Facility, the static tables and the plugin entry -------------------------------------------

struct NameCheckView {
    int value;
    const char* name;
};

struct ResultTableView {
    const char* name;
    const char* desc;
};

static bool SameText(const char* a, const char* b)
{
    if (!a || !b) return a == b;
    return std::strcmp(a, b) == 0;
}

static const void* DataExport(int side, const char* name)
{
    const void* const p = GetProcAddress(mod[side], name);
    if (!p) std::printf("missing data export %s in %s\n", name, side ? "orig" : "ours");
    return p;
}

static void TestStatics()
{
    static const char* const actionValues[12] = {
        "Error", "NoAction", "ProcessFailure", "NotifyFailure", "QueueRequest", "MainThreadInitialize",
        "QueueBackground", "BackgroundLoad", "SendLoadComplete", "LoadComplete", "UnloadResource", "AbortNow"};
    static const char* const inputValues[9] = {"QueueRequest", "ResourceLoaded", "QueueBackground", "LoadData",
                                               "LoadComplete", "CheckComplete", "Unload",       "LoadFailure",
                                               "Abort"};
    static const char* const stateValues[10] = {"Unloaded", "Aborting", "FailurePending", "Failed",   "Requesed",
                                                "Initializing", "Queued", "Loading", "LoadComplete", "Loaded"};
    static const char* const resultNames[3] = {"RES_PROCESSING", "RES_NOT_QUEUED", "RES_QUEUE_FULL"};
    static const char* const resultDescs[3] = {"The resource is currently being processed",
                                               "The resource is not in the queue",
                                               "The queue was full and the item could not be added"};

    struct TableCase {
        const char* name;
        int count;
        const char* const* values;
    };
    const TableCase tables[] = {
        {"?actionNames@BackgroundResource@Jet@@1QBUNameCheck@12@B", 12, actionValues},
        {"?inputNames@BackgroundResource@Jet@@1QBUNameCheck@12@B", 9, inputValues},
        {"?stateNames@BackgroundResource@Jet@@1QBUNameCheck@12@B", 10, stateValues},
    };
    for (const TableCase& c : tables) {
        const NameCheckView* const ours = static_cast<const NameCheckView*>(DataExport(0, c.name));
        const NameCheckView* const orig = static_cast<const NameCheckView*>(DataExport(1, c.name));
        if (!ours || !orig) {
            Check(false, c.name);
            continue;
        }
        for (int i = 0; i < c.count; ++i) {
            CheckInt(ours[i].value, orig[i].value, "static table code");
            Check(ours[i].value == i, "static table index");
            Check(SameText(ours[i].name, orig[i].name), "static table name");
            Check(SameText(ours[i].name, c.values[i]), "static table expected name");
        }
    }
    // fsm is a pointer variable in the original and is null there
    const unsigned int* const fsmA = static_cast<const unsigned int*>(
        DataExport(0, "?fsm@BackgroundResource@Jet@@1QAY08$$CBUStateTable@12@A"));
    const unsigned int* const fsmB = static_cast<const unsigned int*>(
        DataExport(1, "?fsm@BackgroundResource@Jet@@1QAY08$$CBUStateTable@12@A"));
    Check(fsmA && fsmB, "fsm export");
    if (fsmA && fsmB) {
        CheckInt(static_cast<int>(*fsmA), static_cast<int>(*fsmB), "fsm value");
        Check(*fsmA == 0, "fsm null");
    }
    // results is the three-entry ResultTable array (MSVC mangles an array of const as a pointer)
    const ResultTableView* const resultsA = static_cast<const ResultTableView*>(
        DataExport(0, "?results@BackgroundLoadingThread@Jet@@2QBUResultTable@2@B"));
    const ResultTableView* const resultsB = static_cast<const ResultTableView*>(
        DataExport(1, "?results@BackgroundLoadingThread@Jet@@2QBUResultTable@2@B"));
    Check(resultsA && resultsB, "results export");
    if (resultsA && resultsB) {
        for (int i = 0; i < 3; ++i) {
            Check(SameText(resultsA[i].name, resultsB[i].name), "results name");
            Check(SameText(resultsA[i].desc, resultsB[i].desc), "results description");
            Check(SameText(resultsA[i].name, resultNames[i]), "results expected name");
            Check(SameText(resultsA[i].desc, resultDescs[i]), "results expected description");
        }
    }
}

static void TestFacility()
{
    using CtorFn = void(__thiscall*)(void*);
    using CopyFn = void(__thiscall*)(void*, const void*);
    using AssignFn = void*(__thiscall*)(void*, const void*);
    using GetNameFn = const void*(__cdecl*)(const void*);

    const CtorFn ctor[2] = {Proc<CtorFn>(0, "??0Facility@Jet@@QAE@XZ"), Proc<CtorFn>(1, "??0Facility@Jet@@QAE@XZ")};
    const CopyFn copy[2] = {Proc<CopyFn>(0, "??0Facility@Jet@@QAE@ABV01@@Z"),
                            Proc<CopyFn>(1, "??0Facility@Jet@@QAE@ABV01@@Z")};
    const AssignFn assign[2] = {Proc<AssignFn>(0, "??4Facility@Jet@@QAEAAV01@ABV01@@Z"),
                                Proc<AssignFn>(1, "??4Facility@Jet@@QAEAAV01@ABV01@@Z")};
    void* const vtable[2] = {GetProcAddress(mod[0], "??_7Facility@Jet@@6B@"),
                             GetProcAddress(mod[1], "??_7Facility@Jet@@6B@")};
    Check(vtable[0] && vtable[1], "Facility vtable export");
    if (!vtable[0] || !vtable[1] || !ctor[0] || !ctor[1] || !copy[0] || !copy[1] || !assign[0] || !assign[1]) {
        Check(false, "Facility exports");
        return;
    }
    for (int side = 0; side < 2; ++side) {
        unsigned char object[4];
        std::memset(object, 0xa5, sizeof object);
        ctor[side](object);
        Check(ReadPtr(object, 0) == vtable[side], "Facility ctor vtable");

        unsigned char source[4];
        std::memset(source, 0, sizeof source);
        WritePtr(source, 0, reinterpret_cast<void*>(0xdeadbeefu));
        unsigned char copyObject[4];
        std::memset(copyObject, 0xa5, sizeof copyObject);
        copy[side](copyObject, source);
        Check(ReadPtr(copyObject, 0) == vtable[side], "Facility copy ctor vtable");

        Check(assign[side](object, source) == object, "Facility assignment returns this");
        Check(ReadPtr(object, 0) == vtable[side], "Facility assignment keeps vtable");

        const void* const* const table = static_cast<const void* const*>(vtable[side]);
        const void* const name = reinterpret_cast<GetNameFn>(table[0])(object);
        Check(name == nullptr, "Facility GetName null");
    }
}

static void TestPluginEntry()
{
    void(__cdecl* const entry)() =
        reinterpret_cast<void(__cdecl*)()>(GetProcAddress(mod[0], "JetGetPluginsRender3DCore"));
    Check(entry != nullptr, "plugin entry export");
    if (!entry) return;
    // The entry registers plugin types in the shared PluginSystem. Run it after the BackgroundResource /
    // BackgroundLoadingThread checks: the hand-built fake objects in those tests use their own CriticalSection
    // allocations, and the plugin registration rearranges the KernelStandard pools under them.
    entry();
    Check(true, "plugin entry returns");
}

}  // namespace

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::printf("usage: diff_r3d_u11 <bin dir> [rounds]\n");
        return 2;
    }
    const int rounds = argc > 2 ? std::atoi(argv[2]) : 100;
    SetDllDirectoryA(argv[1]);
    const std::string dir = argv[1];
    if (!LoadLibraryA((dir + "\\Render3DCore_7c_orig.dll").c_str()))
        std::printf("orig load failed (%lu)\n", GetLastError());
    mod[0] = LoadLibraryA((dir + "\\Render3DCore_7c.dll").c_str());
    if (!mod[0]) std::printf("ours load failed (%lu)\n", GetLastError());
    mod[1] = GetModuleHandleA("Render3DCore_7c_orig.dll");
    if (!mod[1]) std::printf("orig handle missing (%lu)\n", GetLastError());
    // KernelStandard is already in the process: both modules import it
    kernelStandard = GetModuleHandleA("KernelStandard_7c.dll");
    if (!kernelStandard || !mod[0] || !mod[1]) {
        std::printf("cannot load the modules from %s (%lu)\n", argv[1], GetLastError());
        return 2;
    }
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    SetUnhandledExceptionFilter(&OnCrash);
    criticalCtor = reinterpret_cast<CriticalCtor>(GetProcAddress(kernelStandard, "??0CriticalSection@Jet@@QAE@PBD@Z"));
    criticalDtor = reinterpret_cast<CriticalDtor>(GetProcAddress(kernelStandard, "??1CriticalSection@Jet@@UAE@XZ"));
    if (!criticalCtor || !criticalDtor) {
        std::printf("cannot find KernelStandard's CriticalSection (module %p, error %lu)\n",
                    static_cast<void*>(kernelStandard), GetLastError());
        FARPROC get = kernelStandard ? GetProcAddress(kernelStandard, "?GetCurrentId@Thread@Jet@@SAKXZ") : nullptr;
        std::printf("GetCurrentId at %p\n", reinterpret_cast<void*>(get));
        return 2;
    }
    for (int side = 0; side < 2; ++side) {
        Api& a = api[side];
        a.bkgCtor = Proc<Api::CtorFn>(side, "??0BackgroundResource@Jet@@QAE@PAVBackgroundLoadingThread@1@@Z");
        a.bkgDtor = Proc<Api::DtorFn>(side, "??1BackgroundResource@Jet@@UAE@XZ");
        a.doAction = Proc<decltype(a.doAction)>(
            side, "?DoAction@BackgroundResource@Jet@@IAA?AW4StateReturn@12@W4Input@12@@Z");
        a.getState = Proc<decltype(a.getState)>(side, "?GetCurrentState@BackgroundResource@Jet@@IBA?AW4State@12@XZ");
        a.getResource =
            Proc<decltype(a.getResource)>(side, "?GetResource@BackgroundResource@Jet@@QBAPBVResource@2@XZ");
        a.setBackgroundThread = Proc<decltype(a.setBackgroundThread)>(
            side, "?SetBackgroundThread@BackgroundResource@Jet@@QAAXPAVBackgroundLoadingThread@2@@Z");
        a.checkComplete = Proc<decltype(a.checkComplete)>(side, "?CheckComplete@BackgroundResource@Jet@@QAA_NXZ");
        a.loadFailed = Proc<decltype(a.loadFailed)>(side, "?LoadFailed@BackgroundResource@Jet@@IAAXXZ");
        a.backgroundLoadData =
            Proc<decltype(a.backgroundLoadData)>(side, "?BackgroundLoadData@BackgroundResource@Jet@@QAAXXZ");
        a.queueBackground =
            Proc<decltype(a.queueBackground)>(side, "?QueueBackground@BackgroundResource@Jet@@IAAXXZ");
        a.unloadResource =
            Proc<decltype(a.unloadResource)>(side, "?UnloadResource@BackgroundResource@Jet@@QAAXXZ");
        a.abort = Proc<decltype(a.abort)>(side, "?Abort@BackgroundResource@Jet@@UAAX_N@Z");
        a.addObserver = Proc<decltype(a.addObserver)>(
            side, "?AddObserver@BackgroundResource@Jet@@QAAXPAVBkgResObserver@2@K@Z");
        a.removeObserver = Proc<decltype(a.removeObserver)>(
            side, "?RemoveObserver@BackgroundResource@Jet@@QAAXPAVBkgResObserver@2@@Z");
        a.callCallbacks = Proc<decltype(a.callCallbacks)>(
            side, "?CallCallbacks@BackgroundResource@Jet@@IBA?AW4StateReturn@12@K@Z");
        a.deleteSelf = Proc<decltype(a.deleteSelf)>(side, "?DeleteSelf@BackgroundResource@Jet@@UAAXXZ");
        a.loadResource =
            Proc<decltype(a.loadResource)>(side, "?LoadResource@BackgroundResource@Jet@@UAA_NPBVResource@2@@Z");
        a.loadResourceDb = Proc<decltype(a.loadResourceDb)>(
            side, "?LoadResource@BackgroundResource@Jet@@UAA_NPAVResourceDatabase@2@ABVPString@2@@Z");

        a.threadCtor = Proc<Api::ThreadCtorFn>(side, "??0BackgroundLoadingThread@Jet@@QAE@HW4Priority@Thread@1@@Z");
        a.threadDtor = Proc<Api::DtorFn>(side, "??1BackgroundLoadingThread@Jet@@MAE@XZ");
        a.getThreadID = Proc<decltype(a.getThreadID)>(side, "?GetThreadID@BackgroundLoadingThread@Jet@@QBAKXZ");
        a.setThreadPriority = Proc<decltype(a.setThreadPriority)>(
            side, "?SetThreadPriority@BackgroundLoadingThread@Jet@@QAAXW4Priority@Thread@2@@Z");
        a.queueResource = Proc<decltype(a.queueResource)>(
            side,
            "?QueueResource@BackgroundLoadingThread@Jet@@UAA?AV?$ResultClass@$1?results@BackgroundLoadingThread@Jet@@2QBUResultTable@2@BW4Results@BackgroundLoadingThread@Jet@@@2@PAVBackgroundResource@2@@Z");
        a.dequeueResource = Proc<decltype(a.dequeueResource)>(
            side,
            "?DequeueResource@BackgroundLoadingThread@Jet@@UAA?AV?$ResultClass@$1?results@BackgroundLoadingThread@Jet@@2QBUResultTable@2@BW4Results@BackgroundLoadingThread@Jet@@@2@PAVBackgroundResource@2@@Z");
        a.signalComplete = Proc<decltype(a.signalComplete)>(
            side,
            "?SignalComplete@BackgroundLoadingThread@Jet@@UAA?AV?$ResultClass@$1?results@BackgroundLoadingThread@Jet@@2QBUResultTable@2@BW4Results@BackgroundLoadingThread@Jet@@@2@PAVBackgroundResource@2@@Z");
        a.checkForComplete =
            Proc<decltype(a.checkForComplete)>(side, "?CheckForComplete@BackgroundLoadingThread@Jet@@UAAXXZ");
        a.queueForDelete = Proc<decltype(a.queueForDelete)>(
            side, "?QueueForDelete@BackgroundLoadingThread@Jet@@UAAXPAVBackgroundResource@2@@Z");
    }
    for (int side = 0; side < 2; ++side) {
        InitFakeThread(side);
        InitFakeResource(side, 0x3f800000, 6, 2);
        InitFakeDatabase(side);
        InitFakeObserver(side);
    }

    TestRealObject();
    TestStateMachine();
    TestEntryPoints();
    TestObservers();
    TestVtableOrder();
    TestLoadResource();
    TestStatics();
    TestFacility();
    for (int round = 1; round < rounds; ++round) TestStateMachine();
    TestThread();
    TestPluginEntry();

    std::printf("%llu checks, %llu mismatches\n", checks, bad);
    return bad ? 1 : 0;
}

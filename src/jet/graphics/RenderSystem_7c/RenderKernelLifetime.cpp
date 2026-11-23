// U13: RenderKernel construction, destruction and its default-constructor closure.
// The layout, call order and unexported helpers come from the raw disassembly of
// 0x10039350, 0x100394e0 and 0x1000d1e0. Private helpers still live in the original module.
#include <windows.h>

#include <cstdlib>
#include <cstring>
#include <new>
#include <memory>

#include <jet/KernelStandard.h>
#include <jet/PString.h>
#include <jet/Task.h>
#include <jet/VCall.h>

extern unsigned char RsRsV008[8];  // LayerManager vftable, slot 0
extern unsigned char RsRsV009[24]; // Task vftable, slot 0

namespace Jet {

class SceneGraph;
class SystemResourceManager;
void __fastcall RsSceneGraphDefaultClosure(void*, void*);
void RsDestroyScene(SceneGraph*);
void* __fastcall RsLayerManagerCreate(void*, void*);
void __fastcall RsLayerManagerDrop(void*, void*);
void __cdecl RsClearSharedTextures();
extern "C" void RsDestroyRenderingCacheManager(void*);

// Only the imported methods are needed here. Including Bitmap.h would redefine IListNode from Task.h.
class Bitmap {
public:
    ~Bitmap();
    bool __cdecl Save(const char* fileName) const;
};

namespace {

using Byte = unsigned char;

enum class Dll { Kernel, Standard };

template<class F>
F Import(Dll dll, const char* name)
{
    static HMODULE const kernel = GetModuleHandleA("Kernel_7c.dll");
    static HMODULE const standard = GetModuleHandleA("KernelStandard_7c.dll");
    HMODULE const module = dll == Dll::Kernel ? kernel : standard;
    const auto fn = reinterpret_cast<F>(GetProcAddress(module, name));
    if (!fn) std::abort();
    return fn;
}

using VoidThis = void(__thiscall*)(void*);
using VoidCdecl = void(__cdecl*)();
using ResourceList = FastDynamicArray<SystemResourceManager*>;

struct DumpEntry {
    Bitmap* bitmap;
    String name;
};
static_assert(sizeof(DumpEntry) == 8);

void DeleteDumpBitmap(Bitmap* bitmap)
{
    if (!bitmap) return;
    bitmap->~Bitmap();
    Alloc::Delete(bitmap);
}

class QueueLock {
public:
    explicit QueueLock(CriticalSection& lock) : lock_(lock), held_(VCall<bool>(&lock, 8, INFINITE)) {}
    ~QueueLock() { Release(); }
    void Release()
    {
        if (held_) {
            VCall<bool>(&lock_, 0x10);
            held_ = false;
        }
    }
private:
    CriticalSection& lock_;
    bool held_;
};

struct DumpQueue {
    CriticalSection lock;
    WEvent available{false, true, nullptr};
    unsigned count = 0, capacity = 0;
    DumpEntry* entries = nullptr;
    unsigned write = 0, read = 0;
    bool open = true;

    ~DumpQueue()
    {
        for (unsigned i = 0; i < count; ++i) {
            auto& entry = entries[(read + i) % capacity];
            DeleteDumpBitmap(entry.bitmap);
            entry.~DumpEntry();
        }
        if (entries) Alloc::Delete(entries);
    }

    bool Grow()
    {
        constexpr unsigned block = 128;
        const unsigned next = capacity + block;
        auto* storage = static_cast<DumpEntry*>(Alloc::New(next * sizeof(DumpEntry)));
        if (!storage) return false;
        unsigned copied = 0;
        try {
            for (; copied < count; ++copied)
                ::new (storage + copied) DumpEntry(entries[(read + copied) % capacity]);
        } catch (...) {
            while (copied > 0) storage[--copied].~DumpEntry();
            Alloc::Delete(storage);
            throw;
        }
        for (unsigned i = 0; i < count; ++i) entries[(read + i) % capacity].~DumpEntry();
        if (entries) Alloc::Delete(entries);
        entries = storage;
        read = 0;
        write = count;
        capacity = next;
        return true;
    }

    bool Push(Bitmap* bitmap, const AnsiString& name)
    {
        QueueLock guard(lock);
        if (!open || (count == capacity && !Grow())) return false;
        ::new (entries + write) DumpEntry{bitmap, String(name)};
        if (++write == capacity) write = 0;
        ++count;
        available.Set();
        return true;
    }

    bool Pop(DumpEntry* output, bool wait)
    {
        for (;;) {
            QueueLock guard(lock);
            if (count) {
                String name;
                auto& entry = entries[read];
                auto* bitmap = entry.bitmap;
                static_cast<AnsiString&>(name) = static_cast<const AnsiString&>(entry.name);
                entry.~DumpEntry();
                if (++read == capacity) read = 0;
                if (--count) available.Set();
                ::new (output) DumpEntry{bitmap, String(static_cast<const AnsiString&>(name))};
                return true;
            }
            if (!open || !wait) return false;
            guard.Release();
            available.Wait(INFINITE);
        }
    }

    void Close()
    {
        {
            QueueLock guard(lock);
            open = false;
        }
        available.Set();
    }
};
static_assert(sizeof(DumpQueue) == 0x2c);
static_assert(offsetof(DumpQueue, count) == 0x14 && offsetof(DumpQueue, open) == 0x28);

DumpQueue* Queue(void* kernel)
{
    return reinterpret_cast<DumpQueue*>(static_cast<Byte*>(kernel) + 0xb8);
}

void* ConstructImpl(void* self, void* graph, float priority)
{
    Byte* const base = static_cast<Byte*>(self);

    PStringCache* const cache = PString::Cache();
    PStringCache::Node* node =
        cache->Create(reinterpret_cast<const unsigned char*>("JETRenderKernel"));
    PString name;
    name.node = node;
    ::new (base) Task(name, priority, true);
    cache->Destroy(node);
    name.node = nullptr;

    Byte* const caches = base + 0x6c;
    *reinterpret_cast<void**>(caches) = caches;
    *reinterpret_cast<void**>(caches + 4) = caches;
    *reinterpret_cast<unsigned*>(caches + 8) = 0;
    caches[0x0c] = 0;

    RsLayerManagerCreate(base + 0x28, nullptr);
    *reinterpret_cast<void**>(base) = RsRsV009;
    *reinterpret_cast<void**>(base + 0x28) = RsRsV008;

    base[0x84] = 1;
    ::new (base + 0x88) ResourceList;
    ::new (base + 0xac) String;
    *reinterpret_cast<unsigned*>(base + 0xb4) = 0;
    ::new (Queue(base)) DumpQueue;

    *reinterpret_cast<void**>(base + 0x7c) = nullptr;
    void* scene = graph;
    if (!scene) {
        scene = Alloc::New(0x0c);
        if (scene) RsSceneGraphDefaultClosure(scene, nullptr);
    }
    *reinterpret_cast<void**>(base + 0x80) = scene;
    *reinterpret_cast<unsigned*>(base + 0x8c) = 0;
    *reinterpret_cast<unsigned*>(base + 0x90) = 0;
    *reinterpret_cast<unsigned*>(base + 0x94) = 0;
    base[0x99] = 0;
    *reinterpret_cast<unsigned*>(base + 0x9c) = 0;
    *reinterpret_cast<unsigned*>(base + 0xa0) = 0;
    *reinterpret_cast<unsigned*>(base + 0xa4) = 0;
    *reinterpret_cast<unsigned*>(base + 0xa8) = 1;
    static_cast<AnsiString&>(*reinterpret_cast<String*>(base + 0xac)) = "JetScreen";
    base[0x98] = 1;
    base[0xb0] = 0;
    return self;
}

void DestroyImpl(void* self)
{
    Byte* const base = static_cast<Byte*>(self);
    *reinterpret_cast<void**>(base) = RsRsV009;
    *reinterpret_cast<void**>(base + 0x28) = RsRsV008;
    base[0x84] = 0;

    if (*reinterpret_cast<void**>(base + 0xb4)) {
        Queue(base)->Close();
        if (void* const live = *reinterpret_cast<void**>(base + 0xb4)) {
            using Stop = void(__thiscall*)(void*, int);
            const auto table = *reinterpret_cast<Stop**>(live);
            table[0](live, 1);
        }
        *reinterpret_cast<void**>(base + 0xb4) = nullptr;
    }

    auto& scene = *reinterpret_cast<SceneGraph**>(base + 0x80);
    if (scene) RsDestroyScene(scene);
    scene = nullptr;

    RsClearSharedTextures();

    if (void* const material = *reinterpret_cast<void**>(base + 0x90))
        static_cast<ReferenceCount*>(material)->ReleaseReference();
    if (void* const texture = *reinterpret_cast<void**>(base + 0x94))
        static_cast<ReferenceCount*>(texture)->ReleaseReference();

    auto& resources = *reinterpret_cast<ResourceList*>(base + 0x88);
    for (auto it = resources.First(); it != resources.End(); ++it)
        if (auto* resource = *it) VDelete(resource);
    resources.~ResourceList();

    if (void* const display = *reinterpret_cast<void**>(base + 0x7c)) {
        Byte* const displayBytes = static_cast<Byte*>(display);
        void* const plugin = *reinterpret_cast<void**>(displayBytes + 4);
        const unsigned offset = *reinterpret_cast<unsigned*>(static_cast<Byte*>(plugin) + 4);
        static_cast<ReferenceCount*>(static_cast<void*>(displayBytes + 4 + offset))->ReleaseReference();
    }

    Queue(base)->~DumpQueue();
    reinterpret_cast<String*>(base + 0xac)->~String();

    RsLayerManagerDrop(base + 0x28, nullptr);
    RsDestroyRenderingCacheManager(base + 0x6c);
    reinterpret_cast<Task*>(base)->~Task();
}

unsigned DumpMemberImpl(void* self)
{
    Byte* const base = static_cast<Byte*>(self);
    auto* const queue = Queue(base);
    const bool wait = base[0x84] != 0;
    for (;;) {
        alignas(DumpEntry) unsigned char storage[sizeof(DumpEntry)];
        auto* entry = reinterpret_cast<DumpEntry*>(storage);
        if (!queue->Pop(entry, wait)) break;
        if (entry->bitmap) {
            entry->bitmap->Save(static_cast<const char*>(entry->name));
            DeleteDumpBitmap(entry->bitmap);
        }
        entry->~DumpEntry();
    }
    return 0;
}

}  // namespace

unsigned __cdecl RsRenderKernelDumpMember(void* self)
{
    return DumpMemberImpl(self);
}

unsigned __stdcall RsRenderKernelDumpThread(void* self)
{
    return DumpMemberImpl(self);
}

void RsQueueScreenDump(void* self, Bitmap* bitmap, const AnsiString& name)
{
    std::unique_ptr<Bitmap, decltype(&DeleteDumpBitmap)> capture(bitmap, &DeleteDumpBitmap);
    class Worker final : public Thread {
    public:
        explicit Worker(void* kernel) : Thread(RsRenderKernelDumpThread, kernel, true) {}
    };
    static_assert(sizeof(Worker) == sizeof(Thread));
    auto& worker = *reinterpret_cast<Thread**>(static_cast<Byte*>(self) + 0xb4);
    if (!worker) {
        std::unique_ptr<void, decltype(&Alloc::Delete)> storage(Alloc::New(sizeof(Worker)), &Alloc::Delete);
        if (!storage) return;
        auto* started = ::new (storage.get()) Worker(self);
        if (!started->StartedOk()) {
            started->~Worker();
            return;
        }
        worker = started;
        storage.release();
    }
    if (Queue(self)->Push(bitmap, name)) capture.release();
}

void* RsRenderKernelCtorImpl(void* self, void* graph, float priority)
{
    return ConstructImpl(self, graph, priority);
}

void RsRenderKernelDtorImpl(void* self)
{
    DestroyImpl(self);
}

void RsRenderKernelClosureImpl(void* self)
{
    ConstructImpl(self, nullptr, 8.0f);
}

// __thiscall passes `this` in ECX and the remaining arguments on the stack. Compile the ABI shims as naked so
// the C++ implementations keep normal cdecl signatures.
__declspec(naked) void RsRenderKernelCtorThunk()
{
    __asm {
        mov     edx, [esp + 8]
        push    edx
        mov     edx, [esp + 8]
        push    edx
        push    ecx
        call    RsRenderKernelCtorImpl
        add     esp, 12
        ret     8
    }
}

__declspec(naked) void RsRenderKernelDtorThunk()
{
    __asm {
        push    ecx
        call    RsRenderKernelDtorImpl
        add     esp, 4
        ret
    }
}

__declspec(naked) void RsRenderKernelClosureThunk()
{
    __asm {
        push    ecx
        call    RsRenderKernelClosureImpl
        add     esp, 4
        ret
    }
}

}  // namespace Jet

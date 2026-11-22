// U13: RenderKernel construction, destruction and its default-constructor closure.
// The layout, call order and unexported helpers come from the raw disassembly of
// 0x10039350, 0x100394e0 and 0x1000d1e0. Private helpers still live in the original module.
#include <windows.h>

#include <cstdlib>
#include <cstring>
#include <new>

#include <jet/KernelStandard.h>
#include <jet/PString.h>
#include <jet/Task.h>

extern unsigned char RsRsV008[8];  // LayerManager vftable, slot 0
extern unsigned char RsRsV009[24]; // Task vftable, slot 0

namespace Jet {

// Only the imported methods are needed here. Including Bitmap.h would redefine IListNode from Task.h.
class Bitmap {
public:
    ~Bitmap();
    bool __cdecl Save(const char* fileName) const;
};

namespace {

using Byte = unsigned char;

HMODULE Original()
{
    static HMODULE const module = GetModuleHandleA("RenderSystem_7c_orig.dll");
    if (!module) std::abort();
    return module;
}

template<class F>
F At(unsigned rva)
{
    return reinterpret_cast<F>(reinterpret_cast<Byte*>(Original()) + rva);
}

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
using VoidCdeclArg = void(__cdecl*)(void*);
using VoidStdArg = void(__stdcall*)(void*);
using SceneCtor = void(__thiscall*)(void*, void*);

void WakeQueue(void* queue)
{
    void (*fn)() = At<void(*)()>(0x3a520);
    void* saved;
    __asm {
        mov saved, esi
        mov esi, queue
        call fn
        mov esi, saved
    }
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

    At<VoidThis>(0x31df0)(base + 0x28);
    *reinterpret_cast<void**>(base) = RsRsV009;
    *reinterpret_cast<void**>(base + 0x28) = RsRsV008;

    base[0x84] = 1;
    *reinterpret_cast<unsigned*>(base + 0x88) = 0;
    ::new (base + 0xac) String;
    *reinterpret_cast<unsigned*>(base + 0xb4) = 0;
    At<VoidStdArg>(0x3a1a0)(base + 0xb8);

    *reinterpret_cast<void**>(base + 0x7c) = nullptr;
    void* scene = graph;
    if (!scene) {
        scene = Alloc::New(0x0c);
        if (scene) At<SceneCtor>(0x3bc90)(scene, nullptr);
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

    if (void* const thread = *reinterpret_cast<void**>(base + 0xb4)) {
        WakeQueue(base + 0xb8);
        if (void* const live = *reinterpret_cast<void**>(base + 0xb4)) {
            using Stop = void(__thiscall*)(void*, int);
            const auto table = *reinterpret_cast<Stop**>(live);
            table[0](live, 1);
        }
        *reinterpret_cast<void**>(base + 0xb4) = nullptr;
    }

    if (void* const scene = *reinterpret_cast<void**>(base + 0x80)) {
        At<VoidThis>(0x3bd30)(scene);
        Alloc::Delete(scene);
    }
    *reinterpret_cast<void**>(base + 0x80) = nullptr;

    At<VoidCdecl>(0x3c040)();

    if (void* const material = *reinterpret_cast<void**>(base + 0x90))
        static_cast<ReferenceCount*>(material)->ReleaseReference();
    if (void* const texture = *reinterpret_cast<void**>(base + 0x94))
        static_cast<ReferenceCount*>(texture)->ReleaseReference();

    At<VoidCdeclArg>(0x31fd0)(base + 0x88);

    if (void* const display = *reinterpret_cast<void**>(base + 0x7c)) {
        Byte* const displayBytes = static_cast<Byte*>(display);
        void* const plugin = *reinterpret_cast<void**>(displayBytes + 4);
        const unsigned offset = *reinterpret_cast<unsigned*>(static_cast<Byte*>(plugin) + 4);
        static_cast<ReferenceCount*>(static_cast<void*>(displayBytes + 4 + offset))->ReleaseReference();
    }

    At<VoidStdArg>(0x3a210)(base + 0xb8);
    reinterpret_cast<String*>(base + 0xac)->~String();

    // FUN_10031fd0 already freed the list. The original dtor repeats the walk on the now-empty head.
    void*& head = *reinterpret_cast<void**>(base + 0x88);
    if (void* first = head) {
        void* node = *reinterpret_cast<void**>(first);
        void* last = node;
        if (node != first) {
            do {
                void* const next = *reinterpret_cast<void**>(node);
                Alloc::Delete(node);
                node = next;
                last = node;
            } while (node != first);
        }
        if (last) Alloc::Delete(last);
        head = nullptr;
    }

    At<VoidThis>(0x31ec0)(base + 0x28);
    At<VoidThis>(0x38dc0)(base + 0x6c);
    reinterpret_cast<Task*>(base)->~Task();
}

// 0x1003a350 pops one {Bitmap*, String} record. The queue arrives in ESI and the output pointer on the stack.
// The helper constructs the String in place, so the output storage must stay unconstructed until the call.
struct DumpEntry {
    Bitmap* bitmap;                  // +0x00
    unsigned char name[sizeof(String)];  // +0x04
};
static_assert(sizeof(DumpEntry) == 8, "dump queue entry layout");

void PopOne(void* queue, DumpEntry* result)
{
    void (*fn)(DumpEntry*) = At<void (*)(DumpEntry*)>(0x3a350);
    void* saved;
    __asm {
        mov     eax, result
        mov     saved, esi
        mov     esi, queue
        push    eax
        call    fn
        add     esp, 4
        mov     esi, saved
    }
}

unsigned DumpMemberImpl(void* self)
{
    Byte* const base = static_cast<Byte*>(self);
    while (base[0x84] != 0 || *reinterpret_cast<unsigned*>(base + 0xcc) != 0) {
        DumpEntry entry;
        PopOne(base + 0xb8, &entry);
        String* const name = reinterpret_cast<String*>(entry.name);
        if (entry.bitmap) {
            const char* const fileName = static_cast<const char*>(*name);
            entry.bitmap->Save(fileName);
            entry.bitmap->~Bitmap();
            Alloc::Delete(entry.bitmap);
        }
        name->~String();
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

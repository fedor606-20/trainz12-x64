// U16: Jet::DisplayNull, the private "NullRender" display (retail 0x1001e130, 0xf28 bytes) and the 29-entry
// null format table it fills (retail RVA 0xb8228, one 36-byte row per DFormatId). The display object, its two
// virtual tables and the descriptor are built here; nothing in this file calls the renamed original except the
// optional table mirror that keeps the original's readers correct while it is loaded.
#include "DisplayNull.h"

#include <jet/Geometry.h>
#include <jet/Texture.h>
#include <jet/VCall.h>

#include <windows.h>

#include <cstring>
#include <new>

namespace Jet {

// DisplayLifetime.cpp / LocalTables.cpp: the base Display lifetime and the generated local table filler.
void* __fastcall RsDisplayCtor(unsigned char* display, void*, bool createContext, int mostDerived);
extern "C" void __fastcall RsDisplayDtor(unsigned char* adjusted);
extern "C" bool RsFillLocalVtable(unsigned char* table, void** meta, const char* name);
extern "C" int __cdecl RsLocalStubZero();

namespace {

constexpr unsigned int kDisplayReferenceBase = 0xf04;  // Display's virtual base, relative to Display
constexpr unsigned int kDescriptorOffset = DisplayNull::kDescriptorOffset;
constexpr unsigned int kPluginOffset = DisplayNull::kPluginOffset;
constexpr unsigned int kReferenceOffset = DisplayNull::kReferenceOffset;
constexpr unsigned int kDisplayTableSlots = 152;

template <class T>
T& Field(void* object, unsigned int offset)
{
    return *reinterpret_cast<T*>(static_cast<unsigned char*>(object) + offset);
}

// ---- the null format table -------------------------------------------------------------------------------------

// The retail initialiser (0x1001e730) turns the zero-filled table into this: the rows it marks native (flags
// bit 0), the rows it leaves unsupported with a conversion link, and the canonical format each row describes.
// Extracted from the pristine module's runtime table (dump of 0x100b8228 after its initialiser).
constexpr unsigned char kSupported[kNullFormatCount] = {
    0, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0};
constexpr signed char kLink[kNullFormatCount] = {
    -1, 2, -1, 4, -1, 6, -1, 2, 6, 6, 2, 2, 2, -1, -1, -1, -1, 13, 13, 13, -1, -1, -1, -1, -1, 2, 2, 2, 6};
constexpr signed char kSource[kNullFormatCount] = {
    0, 2, 0, 4, 0, 6, 0, 2, 6, 6, 2, 2, 2, 0, 0, 0, 0, 13, 13, 13, 0, 0, 0, 0, 0, 2, 2, 2, 6};

NULLFormat g_nullFormats[kNullFormatCount];

}  // namespace

NULLFormat* __cdecl NullFormatTable() { return g_nullFormats; }

NULLFormat* __cdecl NullFormatEntry(unsigned int id)
{
    static bool initialized = false;
    if (!initialized) {
        initialized = true;
        NullFormatInitialize();
    }
    return id < kNullFormatCount ? g_nullFormats + id : g_nullFormats;
}

void __cdecl NullFormatInitialize()
{
    for (unsigned int i = 0; i < kNullFormatCount; ++i) {
        NULLFormat& row = g_nullFormats[i];
        row.flags = kSupported[i];
        std::memset(row.unused, 0, sizeof row.unused);
        row.blockBits = 0;
        row.link = kLink[i] < 0 ? nullptr : g_nullFormats + kLink[i];
        std::memset(&row.format, 0, sizeof row.format);
        if (kSource[i] > 0) row.format.Set(static_cast<DFormatId>(kSource[i]));
    }
}

namespace {

// ---- the display's own virtuals (retail DisplayNull table overrides) -------------------------------------------

const char* __cdecl NullName(const void*) { return "Null"; }                       // 0x1001df10, slot 0x18
float __cdecl NullMaxAnisotropy(const void*) { return 1.0f; }                      // 0x1001df20, slot 0x28
float __cdecl NullTexelOffset(const void*) { return 0.0f; }                        // 0x1001df30, slot 0x80

DisplayDescriptor* __cdecl NullDescriptor(const void* self)                        // 0x1001df00, slot 0x150
{
    return reinterpret_cast<DisplayDescriptor*>(
        const_cast<unsigned char*>(static_cast<const unsigned char*>(self)) + kDescriptorOffset);
}

DisplayTexture* __cdecl NullCreateTexture(const void* self)                        // 0x1001e6c0, slot 0x98
{
    auto* const memory = static_cast<unsigned char*>(Alloc::New(sizeof(DisplayTextureNull)));
    if (!memory) return nullptr;
    return ::new (memory) DisplayTextureNull(const_cast<Display*>(static_cast<const Display*>(self)));
}

bool __cdecl NullResetVideoMode(const void* self)                                  // 0x1001ded0, slot 0x24c
{
    auto* const words = reinterpret_cast<int*>(
        const_cast<unsigned char*>(static_cast<const unsigned char*>(self)) + 0x24c);
    words[0] = words[1] = words[2] = words[3] = 0;
    return true;
}

void __cdecl NullInitializeFormats(const void*, const Options*)                    // 0x1001e730, slot 0x23c
{
    NullFormatInitialize();
}

bool __cdecl NullOpen(const void* self, const Options* options, bool)               // 0x1001e4d0, slot 0x34
{
    auto* const display = const_cast<unsigned char*>(static_cast<const unsigned char*>(self));
    Field<int>(display, 0x50) = 1;
    auto* const context = Field<unsigned char*>(display, 0x58);
    VCall<bool>(self, 0x174, context, true, false, options);  // Display::InitializeState
    return true;                                              // the original ignores the result
}

// ---- the plugin/refcount virtual base (display table 0x70108) --------------------------------------------------

void __cdecl NullPluginDeleteThis(void* reference)
{
    DisplayNull::Destroy(reinterpret_cast<Display*>(static_cast<unsigned char*>(reference) - kReferenceOffset));
}

void* __fastcall NullPluginDelete(void* reference, void*, unsigned int)
{
    auto* const object = static_cast<unsigned char*>(reference) - kReferenceOffset;
    DisplayNull::Destroy(reinterpret_cast<Display*>(object));
    return object;
}

// ---- the private null vertex buffer manager (0x1001e500, 0x80 bytes) -------------------------------------------

class NullVertexBufferManager : public VertexBufferManager {
public:
    NullVertexBufferManager()
    {
        buffers[0] = new StaticBuffer;
        buffers[1] = new StaticBuffer;
    }

    ~NullVertexBufferManager() override
    {
        if (buffers[0]) buffers[0]->ReleaseReference();
        if (buffers[1]) buffers[1]->ReleaseReference();
    }

    int __cdecl Slot5() override { return BufferSlot(buffers[0]); }   // AddReference and answer it
    int __cdecl Slot6() override { return BufferSlot(buffers[1]); }
    int __cdecl Slot21() override { return ReleaseBuffer(buffers[0]); }
    int __cdecl Slot22() override { return ReleaseBuffer(buffers[1]); }

private:
    static int __cdecl BufferSlot(StaticBuffer* buffer)
    {
        if (!buffer) return 0;
        buffer->AddReference();
        return reinterpret_cast<int>(buffer);
    }
    static int __cdecl ReleaseBuffer(StaticBuffer* buffer)
    {
        if (buffer) buffer->ReleaseReference();
        return 0;
    }

    StaticBuffer* buffers[2];  // +0x78, +0x7c
};
JET_LAYOUT32(sizeof(NullVertexBufferManager) == 0x80);

// ---- the null display descriptor (0x1001e320 / NullDisplayDescriptor 0x6fe38) ---------------------------------

class NullDisplayDescriptor : public DisplayDescriptor {
public:
    NullDisplayDescriptor() : DisplayDescriptor(static_cast<APIType>(1))
    {
        const PString name("NoAPI");
        const PString version("0.0");
        const PString vendor("NoVendor");
        const PString card("NoCard");
        SetData(name, version, vendor, card, 0.0f);
    }
};

// ---- the local tables ------------------------------------------------------------------------------------------

struct DisplayNullTable {
    void* meta;
    void* slots[kDisplayTableSlots];
};

struct PluginTable {
    void* meta;
    void* slots[2];
};

struct Tables {
    DisplayNullTable display;
    PluginTable plugin;
};

Tables& LocalTables()
{
    static Tables tables = [] {
        Tables made{};
        // The base slots are the same local Display virtuals the exported table uses.
        if (!RsFillLocalVtable(reinterpret_cast<unsigned char*>(made.display.slots), &made.display.meta,
                               "??_7Display@Jet@@6B01@@")) {
            for (unsigned int i = 0; i < kDisplayTableSlots; ++i)
                made.display.slots[i] = reinterpret_cast<void*>(&RsLocalStubZero);
        }
        made.display.slots[0x18 / 4] = reinterpret_cast<void*>(&NullName);
        made.display.slots[0x28 / 4] = reinterpret_cast<void*>(&NullMaxAnisotropy);
        made.display.slots[0x34 / 4] = reinterpret_cast<void*>(&NullOpen);
        made.display.slots[0x80 / 4] = reinterpret_cast<void*>(&NullTexelOffset);
        made.display.slots[0x98 / 4] = reinterpret_cast<void*>(&NullCreateTexture);
        made.display.slots[0x150 / 4] = reinterpret_cast<void*>(&NullDescriptor);
        made.display.slots[0x23c / 4] = reinterpret_cast<void*>(&NullInitializeFormats);
        made.display.slots[0x24c / 4] = reinterpret_cast<void*>(&NullResetVideoMode);
        made.plugin.meta = made.display.meta;
        made.plugin.slots[0] = reinterpret_cast<void*>(&NullPluginDeleteThis);
        made.plugin.slots[1] = reinterpret_cast<void*>(&NullPluginDelete);
        return made;
    }();
    return tables;
}

constexpr int kNullVbtable[2] = {0, 0xf1c};

}  // namespace

Display* __cdecl DisplayNull::Create()
{
    auto* const object = static_cast<unsigned char*>(Alloc::New(kObjectSize));
    if (!object) return nullptr;
    std::memset(object, 0, kObjectSize);

    // DisplayNull's vbtable and the virtual base that Display's constructor leaves to the most-derived class.
    *reinterpret_cast<const int**>(object + 4) = kNullVbtable;
    Field<unsigned int>(object, 0xf24) = 1;

    RsDisplayCtor(object, nullptr, true, 0);  // Display(bool createContext = true, bool mostDerived = false)

    Tables& tables = LocalTables();
    *reinterpret_cast<void**>(object) = tables.display.slots;
    Field<void*>(object, kPluginOffset) = nullptr;
    Field<void*>(object, kReferenceOffset) = tables.plugin.slots;
    ::new (object + kDescriptorOffset) NullDisplayDescriptor;
    Field<int>(object, 0x28c) = 1;
    Field<int>(object, 0x290) = 1;
    Field<void*>(object, 0x20c) = new NullVertexBufferManager;
    return reinterpret_cast<Display*>(object);
}

void __cdecl DisplayNull::Destroy(Display* display)
{
    if (!display) return;
    auto* const object = reinterpret_cast<unsigned char*>(display);
    VCall<void>(display, 0x30);  // Display::Shutdown (releases the managers and the built-in vertex types)
    reinterpret_cast<DisplayDescriptor*>(object + kDescriptorOffset)->~DisplayDescriptor();
    RsDisplayDtor(object + kDisplayReferenceBase);
    Alloc::Delete(object);
}

}  // namespace Jet

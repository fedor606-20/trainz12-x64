// Render3DCore_7c U5b wave 5 (U5bFinal): MultiPassLightRender (destructor, light-pass construction, render loop)
// and the MultiPassBumpChunkBase copy assignment.
//
// The light-pass record is 0x104 bytes: an Array<MPTexture,4> at +0, two blocks of ten 0x0c-byte arrays at +0x0c
// and +0x84, the vertex program at +0xfc and the texture combiner at +0x100. The original's array-of-passes at
// +0x08 grows by one record each time (capacity = count + 1); a pass owns one reference on its program and one on
// its combiner.
#include "U5bFinal.h"

#include <jet/DynamicArray.h>
#include <jet/GfxLanguage.h>

#include <cstring>

namespace Jet {
namespace MultiPass {
namespace Final {

HMODULE SelfModule()
{
    static HMODULE module = [] {
        HMODULE m = nullptr;
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCSTR>(&SelfModule), &m);
        return m;
    }();
    return module;
}

void* Original(const char* name) { return OriginalExport(name); }

void* Local(const char* name)
{
    return static_cast<void*>(GetProcAddress(SelfModule(), name));
}

namespace {

constexpr unsigned int kPassStride = 0x104;
constexpr unsigned int kPassProgram = 0xfc;
constexpr unsigned int kPassCombiner = 0x100;

struct PassArrays {
    ArrayHeader textures;    // +0x00
    ArrayHeader lists[10];   // +0x0c
    ArrayHeader combiner[10];  // +0x84
    void* program;           // +0xfc
    void* combinerObject;    // +0x100
};
JET_LAYOUT32(sizeof(PassArrays) == kPassStride);
JET_LAYOUT32(offsetof(PassArrays, lists) == 0x0c && offsetof(PassArrays, combiner) == 0x84);
JET_LAYOUT32(offsetof(PassArrays, program) == 0xfc && offsetof(PassArrays, combinerObject) == 0x100);

void ReleaseArray(ArrayHeader& array)
{
    array.count = 0;
    if (array.data) {
        Alloc::Delete(array.data);
        array.data = nullptr;
    }
}

void DestroyPass(PassArrays* pass)
{
    for (unsigned int i = 0; i < 10; ++i) ReleaseArray(pass->combiner[i]);
    for (unsigned int i = 0; i < 10; ++i) ReleaseArray(pass->lists[i]);
    ReleaseArray(pass->textures);
    pass->program = nullptr;
    pass->combinerObject = nullptr;
}

// The FastDynamicArray heads the render state keeps: {Chunk* head}, each chunk {next, prev, count, data[13]}.
unsigned int ChunkListCount(const void* headAddress)
{
    const unsigned char* const head = *reinterpret_cast<unsigned char* const*>(headAddress);
    if (!head) return 0;
    unsigned int total = 0;
    const unsigned char* chunk = head;
    do {
        total += *reinterpret_cast<const unsigned int*>(chunk + 8);
        chunk = *reinterpret_cast<unsigned char* const*>(chunk);
    } while (chunk != head);
    return total;
}

}  // namespace

void* __thiscall Thunks::BumpChunkAssign(const void*)
{
    return this;
}

void __thiscall Thunks::CreateNewLightPass(void* program, void* combiner)
{
    PassArrays pass;
    std::memset(&pass, 0, sizeof pass);
    pass.program = program;
    pass.combinerObject = combiner;
    if (program) AddVBaseRef(program, 4);
    if (combiner) AddVBaseRef(combiner, 4);

    unsigned char* const base = Bytes(this);
    unsigned int& count = *reinterpret_cast<unsigned int*>(base + 0x08);
    unsigned int& capacity = *reinterpret_cast<unsigned int*>(base + 0x0c);
    void** const dataSlot = reinterpret_cast<void**>(base + 0x10);
    void* data = *dataSlot;

    if (count + 1 > capacity) {
        const unsigned int nextCapacity = count + 1;
        void* const next = Alloc::New(nextCapacity * kPassStride);
        if (data && count) std::memcpy(next, data, count * kPassStride);
        if (data) Alloc::Delete(data);
        data = next;
        *dataSlot = next;
        capacity = nextCapacity;
    }
    std::memcpy(static_cast<unsigned char*>(data) + count * kPassStride, &pass, kPassStride);
    ++count;
}

void __thiscall Thunks::LightRenderDtor()
{
    unsigned char* const base = Bytes(this);
    // The original writes the class vftable first.
    *reinterpret_cast<void**>(base) = Original("??_7MultiPassLightRender@Jet@@6B@");

    const unsigned int count = *reinterpret_cast<unsigned int*>(base + 0x08);
    unsigned char* const data = static_cast<unsigned char*>(*reinterpret_cast<void**>(base + 0x10));
    for (unsigned int i = 0; i < count; ++i) {
        PassArrays* const pass = reinterpret_cast<PassArrays*>(data + i * kPassStride);
        ReleaseVBaseRef(pass->program, 4);
        ReleaseVBaseRef(pass->combinerObject, 4);
    }

    ArrayHeader& scissor = *reinterpret_cast<ArrayHeader*>(base + 0x14);
    scissor.count = 0;
    if (scissor.data) {
        Alloc::Delete(scissor.data);
        scissor.data = nullptr;
    }

    for (unsigned int i = 0; i < count; ++i) DestroyPass(reinterpret_cast<PassArrays*>(data + i * kPassStride));
    *reinterpret_cast<unsigned int*>(base + 0x08) = 0;
    if (data) Alloc::Delete(data);

    // ReferenceCount is a virtual base of MultiPassLightRender: its destructor runs last and writes the base
    // vftable at +0.
    using BaseDtorFn = void(__thiscall*)(void*);
    HMODULE ks = GetModuleHandleA("KernelStandard_7c.dll");
    if (!ks) ks = LoadLibraryA("KernelStandard_7c.dll");
    if (ks)
        if (auto fn = reinterpret_cast<BaseDtorFn>(GetProcAddress(ks, "??1ReferenceCount@Jet@@MAE@XZ"))) fn(this);
}

void __thiscall Thunks::LightRenderRender(void* state, const void* lights) const
{
    const unsigned char* const base = Bytes(this);
    unsigned char* const stateBytes = Bytes(state);
    const unsigned char* const lightsBytes = Bytes(lights);
    if (ChunkListCount(lightsBytes) != *reinterpret_cast<const unsigned int*>(base + 0x34)) return;

    const ArrayHeader& scissor = *reinterpret_cast<const ArrayHeader*>(base + 0x14);
    if (scissor.count) StateSetScissor(state, lights, &scissor);

    const unsigned int passes = *reinterpret_cast<const unsigned int*>(base + 0x08);
    const unsigned char* const passData = static_cast<const unsigned char*>(*reinterpret_cast<void* const*>(base + 0x10));
    for (unsigned int p = 0; p < passes; ++p) {
        const PassArrays* const pass = reinterpret_cast<const PassArrays*>(passData + p * kPassStride);
        StateLoadTextures(state, &pass->textures);

        // Walk the lights the same way the original does: every item of every chunk.
        const unsigned char* const head = *reinterpret_cast<unsigned char* const*>(lightsBytes);
        if (head) {
            unsigned int which = 0;
            const unsigned char* chunk = head;
            do {
                const unsigned int n = *reinterpret_cast<const unsigned int*>(chunk + 8);
                for (unsigned int i = 0; i < n; ++i, ++which) {
                    void* const light = *reinterpret_cast<void* const*>(chunk + 0xc + i * 4);
                    const ArrayHeader& vp = *reinterpret_cast<const ArrayHeader*>(
                        reinterpret_cast<const unsigned char*>(&pass->lists[0]) + which * 0x0c);
                    const ArrayHeader& tc = *reinterpret_cast<const ArrayHeader*>(
                        reinterpret_cast<const unsigned char*>(&pass->combiner[0]) + which * 0x0c);
                    StateLoadPropertiesVP(state, light, &vp, pass->program);
                    StateLoadPropertiesTC(state, light, &tc, pass->combinerObject);
                }
                chunk = *reinterpret_cast<unsigned char* const*>(chunk);
            } while (chunk != head);
        }

        if (pass->combinerObject) {
            using ActivateFn = void(__cdecl*)(void*);
            reinterpret_cast<ActivateFn>((*reinterpret_cast<void***>(pass->combinerObject))[0x14 / 4])(
                pass->combinerObject);
        }
        if (!StateRenderGeometry(state, pass->program)) return;
        if (pass->combinerObject) {
            using DeactivateFn = void(__cdecl*)(void*);
            reinterpret_cast<DeactivateFn>((*reinterpret_cast<void***>(pass->combinerObject))[0x18 / 4])(
                pass->combinerObject);
        }
    }
    (void)stateBytes;
}

}  // namespace Final
}  // namespace MultiPass
}  // namespace Jet

// Unit U5b of Render3DCore_7c: MultiPassRenderState::MPLightRender and MultiPassLightRender, transcribed from the
// TS12 assembly (addresses per member), plus the two trivial MultiPassBumpChunkBase constructors.
//
// These classes do not derive from Jet::Material; they are reference-counted containers whose construction is plain
// field initialisation. They write the *original* class vftable, looked up from Render3DCore_7c_orig.dll, so an object
// built through an entry point of this DLL is byte-identical to one built by the original and its virtual calls still
// reach the original slots. The reference-counted arrays use Jet::Array's flat {count, capacity, data} block and its
// exact growth step (SIZ 10 for the texture array, SIZ 4 for the texture pair list of a light pass, SIZ 5 for the
// vertex/texture-combiner pair lists, SIZ 1 for the scissor light list), which the original's inlined appenders
// reproduce with the same allocation sizes.
#include <jet/Render3DCoreMultiPass.h>

#include <jet/DynamicArray.h>
#include <jet/KernelStandard.h>

#include <cstring>

namespace Jet {
namespace MultiPass {

namespace {

// The array at one offset of a light pass. The texture list grows in steps of four 8-byte pairs (0x33c70); the
// vertex and texture-combiner lists in steps of five (0x1000f720).
template <unsigned int Step>
void AppendProperty(void* address, unsigned int property, unsigned int value)
{
    Array<PropertyPair, Step>* array = reinterpret_cast<Array<PropertyPair, Step>*>(address);
    array->Reserve(array->count + 1);
    array->data[array->count].property = property;
    array->data[array->count].value = value;
    ++array->count;
}

// The base ReferenceCount destructor is called at the end of the original's MPLightRender destructor; the MSVC
// destructor prologue writes the base class vftable, which is the final value seen at object +0. It is resolved from
// KernelStandard_7c.dll like the original's import (the class in the local header has a protected destructor).
using BaseDtorFn = void(__thiscall*)(void*);
BaseDtorFn BaseDtor()
{
    static BaseDtorFn fn = [] {
        HMODULE ks = GetModuleHandleA("KernelStandard_7c.dll");
        return ks ? reinterpret_cast<BaseDtorFn>(GetProcAddress(ks, "??1ReferenceCount@Jet@@MAE@XZ")) : nullptr;
    }();
    return fn;
}

void* Vtable(const char* name) { return OriginalExport(name); }

}  // namespace

// The exported thiscall entry points are member functions of this local thunk class; implemented.d/u5b_multipass.txt
// maps each original mangled name to the thunk. A class of its own (not a stand-in for an original class) keeps the
// material classes, which U5a owns, out of this header.
class StateThunks {
public:
    void* __thiscall MPLightRenderCtor();
    void __thiscall MPLightRenderDtor();

    void* __thiscall LightRenderCtor(unsigned int a, unsigned int b, unsigned int c, unsigned int d, unsigned int e);
    void __thiscall AddTexture(int type, unsigned int value);
    void __thiscall AddVPProperty(unsigned int property, unsigned int value, unsigned int which);
    void __thiscall AddTCProperty(unsigned int property, unsigned int value, unsigned int which);
    void __thiscall AddScissorLight(unsigned int light);

    void* __thiscall BumpChunkBaseCtor();
    void* __thiscall BumpChunkBaseCopyCtor(const void* other);
};

// 0x2d360: six empty arrays; the ReferenceCount vftable written first is overwritten on the next store by the class
// vftable, so the final object only holds the class vftable, reference count 1 and zeros.
void* __thiscall StateThunks::MPLightRenderCtor()
{
    unsigned char* const self = reinterpret_cast<unsigned char*>(this);
    *reinterpret_cast<void**>(self) = Vtable("??_7MPLightRender@MultiPassRenderState@Jet@@6B@");
    *reinterpret_cast<int*>(self + 0x04) = 1;
    std::memset(self + 0x08, 0, 0x48);
    return this;
}

// 0x399a0: release every element (the original does not skip nulls), then free the six blocks in reverse order.
// ReferenceCount::~ReferenceCount is empty in KernelStandard, so the final base-destructor call has no effect.
void __thiscall StateThunks::MPLightRenderDtor()
{
    unsigned char* const self = reinterpret_cast<unsigned char*>(this);
    *reinterpret_cast<void**>(self) = Vtable("??_7MPLightRender@MultiPassRenderState@Jet@@6B@");

    static const unsigned int offsets[6] = {0x08, 0x14, 0x20, 0x2c, 0x38, 0x44};
    for (unsigned int a = 0; a < 6; ++a) {
        const unsigned int count = *reinterpret_cast<unsigned int*>(self + offsets[a]);
        void** const data = *reinterpret_cast<void***>(self + offsets[a] + 8);
        for (unsigned int i = 0; i < count; ++i)
            reinterpret_cast<const ReferenceCount*>(data[i])->ReleaseReference();
    }
    for (int a = 5; a >= 0; --a) {
        void* const data = *reinterpret_cast<void**>(self + offsets[a] + 8);
        *reinterpret_cast<unsigned int*>(self + offsets[a]) = 0;
        if (data) Alloc::Delete(data);
    }
    if (BaseDtorFn fn = BaseDtor()) fn(self);
}

// 0x33490: five light-type counts and their sum; the pass and scissor arrays stay empty.
void* __thiscall StateThunks::LightRenderCtor(unsigned int a, unsigned int b, unsigned int c, unsigned int d,
                                             unsigned int e)
{
    unsigned char* const self = reinterpret_cast<unsigned char*>(this);
    *reinterpret_cast<void**>(self) = Vtable("??_7MultiPassLightRender@Jet@@6B@");
    *reinterpret_cast<int*>(self + 0x04) = 1;
    std::memset(self + 0x08, 0, 0x18);
    *reinterpret_cast<unsigned int*>(self + 0x20) = a;
    *reinterpret_cast<unsigned int*>(self + 0x24) = c;
    *reinterpret_cast<unsigned int*>(self + 0x28) = d;
    *reinterpret_cast<unsigned int*>(self + 0x2c) = b;
    *reinterpret_cast<unsigned int*>(self + 0x30) = e;
    *reinterpret_cast<unsigned int*>(self + 0x34) = a + b + c + d + e;
    return this;
}

// 0x33800: append {type, value} to the texture list of the last pass.
void __thiscall StateThunks::AddTexture(int type, unsigned int value)
{
    unsigned char* const self = reinterpret_cast<unsigned char*>(this);
    const unsigned int passes = *reinterpret_cast<unsigned int*>(self + 0x08);
    unsigned char* const pass =
        static_cast<unsigned char*>(*reinterpret_cast<void**>(self + 0x10)) + (passes - 1) * kLightPassStride;
    AppendProperty<4>(pass + kLightPassTextures, static_cast<unsigned int>(type), value);
}

// 0x33850: append {property, value} to one of the three vertex-property lists of the last pass.
void __thiscall StateThunks::AddVPProperty(unsigned int property, unsigned int value, unsigned int which)
{
    unsigned char* const self = reinterpret_cast<unsigned char*>(this);
    const unsigned int passes = *reinterpret_cast<unsigned int*>(self + 0x08);
    unsigned char* const pass =
        static_cast<unsigned char*>(*reinterpret_cast<void**>(self + 0x10)) + (passes - 1) * kLightPassStride;
    AppendProperty<5>(pass + kLightPassVertexProperties + which * 0x0c, property, value);
}

// 0x338a0: the same for the texture-combiner lists.
void __thiscall StateThunks::AddTCProperty(unsigned int property, unsigned int value, unsigned int which)
{
    unsigned char* const self = reinterpret_cast<unsigned char*>(this);
    const unsigned int passes = *reinterpret_cast<unsigned int*>(self + 0x08);
    unsigned char* const pass =
        static_cast<unsigned char*>(*reinterpret_cast<void**>(self + 0x10)) + (passes - 1) * kLightPassStride;
    AppendProperty<5>(pass + kLightPassTextureCombiners + which * 0x0c, property, value);
}

// 0x338f0: Array<unsigned int, 1> at +0x14, one value (its growth step is one item).
void __thiscall StateThunks::AddScissorLight(unsigned int light)
{
    unsigned char* const self = reinterpret_cast<unsigned char*>(this);
    Array<unsigned int, 1>* const array = reinterpret_cast<Array<unsigned int, 1>*>(self + 0x14);
    array->Reserve(array->count + 1);
    array->data[array->count] = light;
    ++array->count;
}

// 0x2bfc0: vftable only.
void* __thiscall StateThunks::BumpChunkBaseCtor()
{
    *reinterpret_cast<void**>(this) = Vtable("??_7MultiPassBumpChunkBase@Jet@@6B@");
    return this;
}

// 0x2bfd0: the copy constructor also writes only the vftable (`RET 4`).
void* __thiscall StateThunks::BumpChunkBaseCopyCtor(const void*)
{
    *reinterpret_cast<void**>(this) = Vtable("??_7MultiPassBumpChunkBase@Jet@@6B@");
    return this;
}

}  // namespace MultiPass
}  // namespace Jet

// The three counters are ordinary cdecl const members (0x2f230, 0x2f240, 0x1140): `this` on the stack.
namespace Jet {
namespace MultiPass {
namespace Leaves {

unsigned int r3d_u5b_light_num_lights(const void* self, int type)
{
    return *reinterpret_cast<const unsigned int*>(static_cast<const unsigned char*>(self) + 0x20 + type * 4);
}
unsigned int r3d_u5b_light_num_passes(const void* self)
{
    return *reinterpret_cast<const unsigned int*>(static_cast<const unsigned char*>(self) + 0x08);
}
unsigned int r3d_u5b_light_total(const void* self)
{
    return *reinterpret_cast<const unsigned int*>(static_cast<const unsigned char*>(self) + 0x34);
}
}  // namespace Leaves
}  // namespace MultiPass
}  // namespace Jet

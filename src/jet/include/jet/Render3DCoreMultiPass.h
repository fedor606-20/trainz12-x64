// Render3DCore_7c unit U5b: the multipass material family.
//
// The material classes derive from Jet::Material, which belongs to U5a and is not reconstructed yet, so their
// constructors, destructors and every method that touches the base or the U4 caches keep forwarding to
// Render3DCore_7c_orig.dll. What is reconstructed here is the part that can be expressed from the object layouts in
// the TS12 assembly alone:
//
//   * MultiPassRenderState::MPLightRender: its constructor and destructor (six reference-counted arrays, 0x50 bytes);
//   * MultiPassLightRender: the constructor, the light counters and the four appenders (pass record 0x104 bytes);
//   * MultiPassBumpChunkBase: the two trivial constructors (vftable only);
//   * the material leaves: the four mode flags of MultiPassNoTextureMaterial (0xac..0xaf), GetType/GetTexture for the
//     six material types, the OneTexture alpha hint (0xc8), Activate, UpdateTextureArray (Array<Texture const*, 10> at
//     0xb8) and the four plugin IsSupported methods.
//
// Because the recorded entry points are cdecl (virtual and ordinary alike: `this` is pushed on the stack, as the
// assembly reads it at [esp+4]), the leaves are plain `extern "C"` functions exported under the original mangled
// names by implemented.d/u5b_multipass.txt. The thiscall entry points (constructors, MPLightRender::~MPLightRender and
// the four appenders) are member functions of MultiPass::StateThunks with the same ABI. No class that another
// workstream owns is defined here, so merging U5a/U4/U6 later cannot collide with this header.
//
// Static tables stay the original's: GetType and the IsSupported fallback names return the exported
// UNDECORATED_TYPE objects of Render3DCore_7c_orig.dll, looked up at run time, so the returned addresses, node
// reference counts and text are bit-identical to the original's.
#pragma once

#include <jet/Platform.h>

#include <windows.h>

#include <cstddef>

namespace Jet {
namespace MultiPass {

// Resolve a data or function export of the renamed original module (the one every forwarder in this DLL needs).
inline void* OriginalExport(const char* name)
{
    HMODULE module = GetModuleHandleA("Render3DCore_7c_orig.dll");
    if (!module) module = LoadLibraryA("Render3DCore_7c_orig.dll");
    return module ? reinterpret_cast<void*>(GetProcAddress(module, name)) : nullptr;
}

// A Jet::Array<T, SIZ> header: SIZ only changes the growth step.
struct ArrayHeader {
    unsigned int count;     // +0x00
    unsigned int capacity;  // +0x04
    void* data;             // +0x08
};
JET_LAYOUT32(sizeof(ArrayHeader) == 0x0c);

// The pair array inside one light pass (Array<MPTexture, 4> / Array<MPRenderProperty, 4>: 8-byte items).
struct PropertyPair {
    unsigned int property;
    unsigned int value;
};
struct PropertyArray {
    unsigned int count;
    unsigned int capacity;
    PropertyPair* data;
};
JET_LAYOUT32(sizeof(PropertyArray) == 0x0c && sizeof(PropertyPair) == 0x08);

// One MultiPassLightRender pass record: 0x104 bytes. The two arrays the appenders use sit at these offsets from the
// record start (B = passData + (passes - 1) * 0x104):
//   textures            B + 0x00  (AddTexture)
//   vertex properties   B + 0x0c + which * 0x0c (AddVPProperty, which 0..2)
//   texture combiners   B + 0x84 + which * 0x0c (AddTCProperty, which 0..2)
constexpr unsigned int kLightPassStride = 0x104;
constexpr unsigned int kLightPassTextures = 0x00;
constexpr unsigned int kLightPassVertexProperties = 0x0c;
constexpr unsigned int kLightPassTextureCombiners = 0x84;

// MultiPassLightRender: {vftable, referenceCount, passes, passesCapacity, passData, scissorLights Array<unsigned, 1>,
// five light-type counts, total}. 0x38 bytes.
struct LightRenderLayout {
    void* vftable;                  // +0x00
    int referenceCount;             // +0x04
    unsigned int passes;            // +0x08  GetNumPasses
    unsigned int passesCapacity;    // +0x0c
    void* passData;                 // +0x10
    ArrayHeader scissorLights;      // +0x14  AddScissorLight
    unsigned int ambientLights;     // +0x20  GetNumLights(0)
    unsigned int parallelLights;    // +0x24
    unsigned int pointLights;       // +0x28
    unsigned int spotLights;        // +0x2c
    unsigned int cubeMapLights;     // +0x30
    unsigned int totalLights;       // +0x34  GetTotalLights
};
JET_LAYOUT32(sizeof(LightRenderLayout) == 0x38);
JET_LAYOUT32(offsetof(LightRenderLayout, passes) == 0x08 && offsetof(LightRenderLayout, passData) == 0x10);
JET_LAYOUT32(offsetof(LightRenderLayout, scissorLights) == 0x14 && offsetof(LightRenderLayout, totalLights) == 0x34);

// MultiPassRenderState::MPLightRender: six {count, capacity, data} arrays of reference-counted objects. 0x50 bytes.
struct MPLightRenderLayout {
    void* vftable;      // +0x00
    int referenceCount; // +0x04
    ArrayHeader arrays[6];  // +0x08, +0x14, +0x20, +0x2c, +0x38, +0x44
};
JET_LAYOUT32(sizeof(MPLightRenderLayout) == 0x50);
JET_LAYOUT32(offsetof(MPLightRenderLayout, arrays) == 0x08);
JET_LAYOUT32(sizeof(MPLightRenderLayout::arrays) == 0x48);

// Material tail offsets read by the reconstructed leaves. The head is U5a's Jet::Material.
namespace MaterialLayout {
constexpr unsigned int textureA = 0xa4;        // AutoReference<Texture> (runtime texture slot 0xc)
constexpr unsigned int textureB = 0xa8;        // AutoReference<Texture> (runtime texture slot 0xd)
constexpr unsigned int smoothBlendMode = 0xac; // MultiPassNoTextureMaterial
constexpr unsigned int singlePassMode = 0xad;
constexpr unsigned int intensityMode = 0xae;
constexpr unsigned int debugPassMode = 0xaf;
constexpr unsigned int lightRender0 = 0xb0;    // RenderState picks one of these two
constexpr unsigned int lightRender1 = 0xb4;
constexpr unsigned int textureArray = 0xb8;    // Array<Texture const*, 10> {count, capacity, data}
constexpr unsigned int primaryTexture = 0xc4;  // OneTextureMaterial / BumpMaterial texture slot 1
constexpr unsigned int alphaHint = 0xc8;       // TextureAlpha (dword, -1 until set)
constexpr unsigned int bumpTexture = 0xd0;     // BumpMaterial texture slot 8
}  // namespace MaterialLayout

}  // namespace MultiPass
}  // namespace Jet

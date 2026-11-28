// Render3DCore_7c U5b wave 5 (U5bFinal): shared helpers for the multipass material, light-render, plugin and
// render-state exports. The unit keeps the original cdecl entry ABI: every exported member is a plain function
// whose first stack argument is `this`.
#pragma once

#include <jet/Render3DCoreMultiPass.h>

#include <jet/Geometry.h>
#include <jet/KernelStandard.h>
#include <jet/Math.h>
#include <jet/PString.h>
#include <jet/RenderSystem.h>
#include <jet/RenderTypes.h>
#include <jet/SmallTypes.h>

#include <cstring>

namespace Jet {
namespace MultiPass {
namespace Final {

inline unsigned char* Bytes(void* self) { return static_cast<unsigned char*>(self); }
inline const unsigned char* Bytes(const void* self) { return static_cast<const unsigned char*>(self); }

template <class T>
T& Field(void* self, unsigned int offset)
{
    return *reinterpret_cast<T*>(Bytes(self) + offset);
}
template <class T>
T Field(const void* self, unsigned int offset)
{
    return *reinterpret_cast<const T*>(Bytes(self) + offset);
}

template <class T>
T* FieldPtr(void* self, unsigned int offset)
{
    return reinterpret_cast<T*>(Bytes(self) + offset);
}

// A vtable call with an explicit cdecl argument list. All classes in this module are called with the object as
// the first stack argument, so the helper simply fetches the slot and casts it.
template <class F, class... A>
F VFn(void* object, unsigned int slot)
{
    return reinterpret_cast<F>((*reinterpret_cast<void***>(object))[slot / 4]);
}

// Resolve an export of this DLL (the constructed local implementation) or of the renamed original.
HMODULE SelfModule();
void* Original(const char* name);
void* Local(const char* name);

// Material tail helpers.
inline void* TextureAt(void* self, unsigned int offset) { return Field<void*>(self, offset); }
inline void* MaterialTexture(void* self, int slot)
{
    switch (slot) {
    case 1: return TextureAt(self, 0xc4);
    case 8: return TextureAt(self, 0xd0);
    case 0xc: return TextureAt(self, 0xa4);
    case 0xd: return TextureAt(self, 0xa8);
    default: return nullptr;
    }
}

// Display::State() (+0x54) and the blend state the multipass materials save at +0x14/+0x18/+0x1c.
inline unsigned char* DisplayState(void* display)
{
    return *reinterpret_cast<unsigned char**>(Bytes(display) + 0x54);
}

// Reference-count arithmetic for the virtual-base ReferenceCount subobject. `offset` is the offset of the vbptr
// inside the object (0xc for the rendering caches, 4 for the light-pass programs).
inline void AddVBaseRef(void* object, unsigned int offset)
{
    if (!object) return;
    unsigned char* const p = Bytes(object);
    void* const vbptr = *reinterpret_cast<void**>(p + offset);
    const int delta = *reinterpret_cast<int*>(Bytes(vbptr) + 4);
    reinterpret_cast<const ReferenceCount*>(p + offset + delta)->AddReference();
}
inline void ReleaseVBaseRef(void* object, unsigned int offset)
{
    if (!object) return;
    unsigned char* const p = Bytes(object);
    void* const vbptr = *reinterpret_cast<void**>(p + offset);
    const int delta = *reinterpret_cast<int*>(Bytes(vbptr) + 4);
    reinterpret_cast<const ReferenceCount*>(p + offset + delta)->ReleaseReference();
}

// The geometry chunk the bump material allocates: GeometryChunkTemplate<PNTA/PNTTA> plus the second vtable at
// +0x60. The allocation sizes are the original's (0x64 for the template + the extra vtable slot).
void* AllocateBumpChunkPNTA(unsigned int vertices, unsigned int primitives, unsigned int indices,
                            unsigned int faceNormals, unsigned int flags);
void* AllocateBumpChunkPNTTA(unsigned int vertices, unsigned int primitives, unsigned int indices,
                             unsigned int faceNormals, unsigned int flags);

// Shared tail of every CreateGeometryChunk: hint flags, dynamic/shared/index-dynamic calls.
void FinishChunkAllocation(void* chunk, void* hint, bool dynamic);

// The thiscall entry points are members of this local thunk class, so their exported mangled names and the
// ECX/stack ABI match the original. A constructed object is reinterpret_cast to Thunks and the method called on
// it; the method's `this` is the object.
class Thunks {
public:
    // MultiPassRenderState
    void* __thiscall StateCtor1(const void* material, void* display, void* view, void* chunk, const void* lights,
                                float opacity, bool opaque, const void* modelView, const void* textures,
                                bool smoothBlend, bool flag);
    void* __thiscall StateCtor2(const void* material, void* display, void* view, void* chunk, const void* lights,
                                float opacity, bool opaque, const void* modelView, const void* textures,
                                bool smoothBlend, bool flag, unsigned int numIndices, const unsigned short* indices,
                                unsigned int numPrimitives, int primitive);
    void* __thiscall StateCopyCtor(const void* source);
    void __thiscall StateDtor();
    void __thiscall StateSetBlendState();

    // MultiPassLightRender
    void __thiscall LightRenderDtor();
    void __thiscall CreateNewLightPass(void* program, void* combiner);
    void __thiscall LightRenderRender(void* state, const void* lights) const;

    // MultiPassBumpChunkBase
    void* __thiscall BumpChunkAssign(const void* other);
};

void __cdecl StateRenderSingleLightPasses(void* self, void* lights);
void __cdecl StateCleanUp(void* self);
bool __cdecl StateSetup(void* self, const void* lights, const void* modelView);
void __cdecl StateSetScissor(void* self, const void* lights, const void* scissor);
void __cdecl StateLoadTextures(void* self, const void* textures);
void __cdecl StateLoadPropertiesVP(void* self, void* light, const void* properties, void* program);
void __cdecl StateLoadPropertiesTC(void* self, void* light, const void* properties, void* combiner);
bool __cdecl StateRenderGeometry(void* self, void* program);
void __cdecl StateGetAmbientLights(void* self, float r, float g, float b, float a);
void* __cdecl StateGetAmbientLight0(void* self);
void __cdecl StateDebugRenderSingleLightPasses(void* self, void* lightRender);
void __cdecl StateDebugRenderMixedLightPasses(void* self, void* lightRender);

// ---- multipass material render/cache/chunk leaves (U5bFinalRender.cpp)
void __cdecl NoTextureBind(void* self, void* display, void* view, unsigned int pass);
void __cdecl NoTextureUnbind(void* self, void* display, void* view, unsigned int pass);
void __cdecl NoTextureSetDisplayState(void* self, void* display);
void __cdecl NoTextureClearDisplayState(void* self, void* display);
void __cdecl NoTextureRenderState(void* self, void* state);
void __cdecl NoTextureAddProperty(void* self, const PString& name, const PString& value);
void __cdecl NoTextureCacheChunk(void* self, void* display, void* view, void* chunk, const void* lights,
                                 const void* modelView);
void __cdecl NoTextureRenderChunk(void* self, void* display, void* view, void* chunk, const void* lights,
                                  const void* modelView, int pass);
void __cdecl NoTextureRenderNumChunks(void* self, void* display, void* view, void** chunks, const void** lights,
                                      const void** modelViews, unsigned int numChunks, int pass);
void __cdecl NoTextureRenderPrimitives(void* self, void* display, void* view, void* chunk, const void* lights,
                                       const void* modelView, unsigned int numIndices, const unsigned short* indices,
                                       unsigned int numPrimitives, int primitive, int pass);
void __cdecl OneTextureBind(void* self, void* display, void* view, unsigned int pass);
void __cdecl OneTextureUnbind(void* self, void* display, void* view, unsigned int pass);
void __cdecl OneTextureCacheChunk(void* self, void* display, void* view, void* chunk, const void* lights,
                                  const void* modelView);
void __cdecl OneTextureRenderChunk(void* self, void* display, void* view, void* chunk, const void* lights,
                                   const void* modelView, int pass);
void __cdecl OneTextureRenderNumChunks(void* self, void* display, void* view, void** chunks, const void** lights,
                                       const void** modelViews, unsigned int numChunks, int pass);
void __cdecl OneTextureRenderPrimitives(void* self, void* display, void* view, void* chunk, const void* lights,
                                        const void* modelView, unsigned int numIndices, const unsigned short* indices,
                                        unsigned int numPrimitives, int primitive, int pass);
void* __cdecl CreateGeometryChunkShared(void* self, unsigned int vertices, unsigned int primitives,
                                       unsigned int indices, unsigned int faceNormals, int type, void* hint);
void* __cdecl NoTextureCreateGeometryChunk(void* self, unsigned int vertices, unsigned int primitives,
                                          unsigned int indices, unsigned int faceNormals, int type, void* hint);
void* __cdecl OneTextureCreateGeometryChunk(void* self, unsigned int vertices, unsigned int primitives,
                                           unsigned int indices, unsigned int faceNormals, int type, void* hint);
void* __cdecl BumpCreateGeometryChunk(void* self, unsigned int vertices, unsigned int primitives,
                                     unsigned int indices, unsigned int faceNormals, int type, void* hint);
void* __cdecl OneTextureAllocateGeometryMemory(void* self, unsigned int vertices, unsigned int primitives,
                                               unsigned int indices, unsigned int faceNormals, unsigned int flags);
void* __cdecl BumpAllocateGeometryMemory(void* self, unsigned int vertices, unsigned int primitives,
                                          unsigned int indices, unsigned int faceNormals, unsigned int flags);
void* __cdecl Bump2AllocateGeometryMemory(void* self, unsigned int vertices, unsigned int primitives,
                                           unsigned int indices, unsigned int faceNormals, unsigned int flags);
void __cdecl FinishChunkAllocation(void* chunk, void* hint, bool dynamicHint);

// The six FastDynamicArray heads in a render state, and the per-pass light groups.
void SetAlphaBlendModeRaw(void* display, unsigned int value);
unsigned int ListCount(const void* head);

}  // namespace Final
}  // namespace MultiPass
}  // namespace Jet

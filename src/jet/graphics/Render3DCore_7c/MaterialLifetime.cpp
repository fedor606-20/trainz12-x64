// Unit U5a2 of Render3DCore_7c: the material constructors.
//
// Every constructor is __thiscall with the hidden virtual-base flag as its last stack argument (the original ends in
// `ret N+4`). Each body below is a member of the thunk object, so the entry ABI matches. The object is built by the
// RenderSystem_7c Material base constructor (called with the flag cleared, exactly as the original does), then the
// derived vtable/vbtable and the class fields are written. The virtual tables and UID objects are this module's own
// data (emitted by the U5a definitions), installed by export name.
#include <jet/Render3DCoreCaches.h>
#include <jet/Render3DCoreMaterials.h>

#include <windows.h>

#include <cstddef>
#include <cstring>

namespace Jet {

enum SharedTextureID {};
class SharedTextureCollection {
public:
    static bool __cdecl GetTexture(SharedTextureID id, TextureManager* manager, AutoReference<Texture>& out);
};

namespace U5a2 {

namespace {

using MaterialCtorFn = void(__thiscall*)(void*, MaterialManager*, bool);

template <class T>
T& Field(void* p, unsigned offset)
{
    return *reinterpret_cast<T*>(static_cast<unsigned char*>(p) + offset);
}

template <class R, class... A>
R DisplaySlot(const Display* display, unsigned offset, A... args)
{
    using F = R(__cdecl*)(const Display*, A...);
    return (*reinterpret_cast<F* const*>(display))[offset / sizeof(void*)](display, args...);
}

HMODULE RenderSystem()
{
    static HMODULE module = [] {
        HMODULE m = GetModuleHandleA("RenderSystem_7c.dll");
        return m ? m : LoadLibraryA("RenderSystem_7c.dll");
    }();
    return module;
}

MaterialCtorFn MaterialCtor()
{
    static MaterialCtorFn fn = reinterpret_cast<MaterialCtorFn>(
        GetProcAddress(RenderSystem(), "??0Material@Jet@@QAE@PAVMaterialManager@1@@Z"));
    return fn;
}

HMODULE ThisModule()
{
    static HMODULE module = [] {
        HMODULE h = nullptr;
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCSTR>(&ThisModule), &h);
        return h;
    }();
    return module;
}

// the vftables/vbtables/UIds are this module's own data since U5a wave 5; every name this file installs is an
// exported data symbol of this module.
void* LocalData(const char* name) { return reinterpret_cast<void*>(GetProcAddress(ThisModule(), name)); }

int VBDelta(const void* self)
{
    const int* const table = *reinterpret_cast<const int* const*>(static_cast<const unsigned char*>(self) + 4);
    return table[1];
}

// the common tail: install the class's two original vtables and the vtordisp, exactly like the original constructor
void Finish(void* self, const char* materialTable, const char* instanceTable)
{
    const int delta = VBDelta(self);
    *reinterpret_cast<void**>(self) = LocalData(materialTable);
    *reinterpret_cast<void**>(static_cast<unsigned char*>(self) + delta + 4) = LocalData(instanceTable);
    *reinterpret_cast<int*>(static_cast<unsigned char*>(self) + delta) = 0;
}

void Begin(void* self, MaterialManager* manager, bool flag, const char* vbtable)
{
    if (flag) {
        *reinterpret_cast<void**>(static_cast<unsigned char*>(self) + 4) = LocalData(vbtable);
        Field<int>(self, VBDelta(self) + 8) = 1;  // ReferenceCount value of the virtual base
    }
    MaterialCtor()(self, manager, false);
}

void FetchTexture(void* self, unsigned offset, MaterialManager* manager)
{
    TextureManager* const textures = Field<TextureManager*>(Field<void*>(manager, 8), 0x94);
    SharedTextureCollection::GetTexture(static_cast<SharedTextureID>(0), textures,
                                        Field<AutoReference<Texture>>(self, offset));
}

// The one-texture set: the default shared texture, the never-cached alpha hint and the empty texture stage.
void OneTextureFields(void* self, MaterialManager* manager)
{
    Field<void*>(self, 0x9c) = nullptr;
    FetchTexture(self, 0x9c, manager);
    Field<TextureAlpha>(self, 0xa0) = static_cast<TextureAlpha>(-1);
    Field<void*>(self, 0xa4) = nullptr;
}

void AddCacheReference(void* cache)
{
    if (!cache) return;
    void* const vbptr = Field<void*>(cache, 0xc);
    const int delta = *reinterpret_cast<int*>(static_cast<unsigned char*>(vbptr) + 4);
    reinterpret_cast<ReferenceCount*>(static_cast<unsigned char*>(cache) + 0xc + delta)->AddReference();
}

}  // namespace

struct MaterialThunks {
    // NoTextureMaterial(MaterialManager*, MaterialChunkCache*, AlphaBlendCache*)
    void* NoTexture(MaterialManager* manager, MaterialChunkCache*, AlphaBlendCache*, bool flag);

    // OneTextureMaterial/Material): (MaterialManager*, MaterialChunkCache*, AlphaMaskCache*, AlphaBlendCache*)
    void* OneTexture(MaterialManager* manager, MaterialChunkCache*, AlphaMaskCache*, AlphaBlendCache*, bool flag);
    void* OneTextureNoAlpha(MaterialManager* manager, MaterialChunkCache*, AlphaMaskCache*, AlphaBlendCache*, bool flag);
    void* Billboard(MaterialManager* manager, MaterialChunkCache*, AlphaMaskCache*, AlphaBlendCache*, bool flag);

    // ColorOneTextureMaterial: no alpha-mask argument in the original signature
    void* ColorOneTexture(MaterialManager* manager, MaterialChunkCache*, AlphaBlendCache*, bool flag);

    // AutoTexMaterial keeps the mask argument, and MVHSelectMaterial adds the context pointer
    void* AutoTex(MaterialManager* manager, MaterialChunkCache*, AlphaMaskCache*, AlphaBlendCache*, bool flag);
    void* MVHSelect(MaterialManager* manager, MaterialChunkCache*, AlphaMaskCache*, AlphaBlendCache*, void* context,
                    bool flag);

    // Reflective/Gloss: (MaterialManager*, MaterialChunkCache*, AlphaBlendCache*, contextCount, contextData)
    void* Reflective(MaterialManager* manager, MaterialChunkCache*, AlphaBlendCache*, unsigned int contextCount,
                     void** contextData, bool flag);
    void* Gloss(MaterialManager* manager, MaterialChunkCache*, AlphaBlendCache*, unsigned int contextCount,
                void** contextData, bool flag);

    // GenericShaderMaterial(MaterialManager*, MaterialChunkCache*, AlphaBlendCache*)
    void* GenericShader(MaterialManager* manager, MaterialChunkCache* chunkCache, AlphaBlendCache* blendCache,
                        bool flag);
};

void* MaterialThunks::NoTexture(MaterialManager* manager, MaterialChunkCache*, AlphaBlendCache*, bool flag)
{
    Begin(this, manager, flag, "??_8NoTextureMaterial@Jet@@7B@");
    Finish(this, "??_7NoTextureMaterial@Jet@@6BMaterial@1@@", "??_7NoTextureMaterial@Jet@@6BPluginInstance@1@@");
    Field<void*>(this, 0x9c) = nullptr;
    return this;
}

void* MaterialThunks::OneTexture(MaterialManager* manager, MaterialChunkCache*, AlphaMaskCache*, AlphaBlendCache*,
                                 bool flag)
{
    Begin(this, manager, flag, "??_8OneTextureMaterial@Jet@@7B@");
    OneTextureFields(this, manager);
    Finish(this, "??_7OneTextureMaterial@Jet@@6BMaterial@1@@", "??_7OneTextureMaterial@Jet@@6BPluginInstance@1@@");
    return this;
}

void* MaterialThunks::OneTextureNoAlpha(MaterialManager* manager, MaterialChunkCache*, AlphaMaskCache*,
                                        AlphaBlendCache*, bool flag)
{
    Begin(this, manager, flag, "??_8OneTextureNoAlphaMaterial@Jet@@7B@");
    OneTextureFields(this, manager);
    Field<TextureAlpha>(this, 0xa0) = static_cast<TextureAlpha>(0);
    Finish(this, "??_7OneTextureNoAlphaMaterial@Jet@@6BMaterial@1@@",
           "??_7OneTextureNoAlphaMaterial@Jet@@6BPluginInstance@1@@");
    return this;
}

void* MaterialThunks::Billboard(MaterialManager* manager, MaterialChunkCache*, AlphaMaskCache*, AlphaBlendCache*,
                                bool flag)
{
    Begin(this, manager, flag, "??_8BillboardMaterial@Jet@@7B@");
    OneTextureFields(this, manager);
    Finish(this, "??_7BillboardMaterial@Jet@@6BMaterial@1@@", "??_7BillboardMaterial@Jet@@6BPluginInstance@1@@");
    return this;
}

void* MaterialThunks::ColorOneTexture(MaterialManager* manager, MaterialChunkCache*, AlphaBlendCache*, bool flag)
{
    Begin(this, manager, flag, "??_8ColorOneTextureMaterial@Jet@@7B@");
    OneTextureFields(this, manager);
    Finish(this, "??_7ColorOneTextureMaterial@Jet@@6BMaterial@1@@",
           "??_7ColorOneTextureMaterial@Jet@@6BPluginInstance@1@@");
    return this;
}

void* MaterialThunks::AutoTex(MaterialManager* manager, MaterialChunkCache*, AlphaMaskCache*, AlphaBlendCache*,
                              bool flag)
{
    Begin(this, manager, flag, "??_8AutoTexMaterial@Jet@@7B@");
    OneTextureFields(this, manager);
    Finish(this, "??_7AutoTexMaterial@Jet@@6BMaterial@1@@", "??_7AutoTexMaterial@Jet@@6BPluginInstance@1@@");
    Field<float>(this, 0xa8) = 1.0f;
    Field<float>(this, 0xac) = 1.0f;
    Field<float>(this, 0xb0) = 0.0f;
    Field<float>(this, 0xb4) = 0.0f;
    return this;
}

void* MaterialThunks::MVHSelect(MaterialManager* manager, MaterialChunkCache*, AlphaMaskCache*, AlphaBlendCache*,
                                void* context, bool flag)
{
    Begin(this, manager, flag, "??_8MVHSelectMaterial@Jet@@7B@");
    OneTextureFields(this, manager);
    Finish(this, "??_7MVHSelectMaterial@Jet@@6BMaterial@1@@", "??_7MVHSelectMaterial@Jet@@6BPluginInstance@1@@");
    Field<float>(this, 0xa8) = 1.0f;
    Field<float>(this, 0xac) = 1.0f;
    Field<float>(this, 0xb0) = 1.0f;
    Field<float>(this, 0xb4) = 1.0f;
    Field<unsigned char>(this, 0x84) &= 0xfdu;
    Field<void*>(this, 0xb8) = context;
    Field<int>(this, 0x78) = 4;
    Field<int>(this, 0x7c) = 5;
    return this;
}

void* MaterialThunks::Reflective(MaterialManager* manager, MaterialChunkCache*, AlphaBlendCache*,
                                 unsigned int contextCount, void** contextData, bool flag)
{
    Begin(this, manager, flag, "??_8ReflectiveMaterial@Jet@@7B@");
    OneTextureFields(this, manager);
    Finish(this, "??_7ReflectiveMaterial@Jet@@6BMaterial@1@@",
           "??_7ReflectiveMaterial@Jet@@6BPluginInstance@1@@");
    Display* const display = Field<Display*>(Field<void*>(manager, 8), 0x7c);
    const bool cubeFormat = DisplaySlot<bool>(display, 0x1c, 0x1a);
    const bool cubeFormat2 = DisplaySlot<bool>(display, 0x1c, 0x1b);
    Field<void*>(this, 0xa8) = nullptr;
    Field<float>(this, 0xac) = 1.0f;
    Field<unsigned int>(this, 0xb0) = contextCount;
    Field<void**>(this, 0xb4) = contextData;
    unsigned char flags2 = static_cast<unsigned char>(Field<unsigned char>(this, 0xb8) & 0xf8);
    flags2 = static_cast<unsigned char>(flags2 | (cubeFormat ? 1 : 0) | (cubeFormat2 ? 2 : 0));
    Field<unsigned char>(this, 0xb8) = flags2;
    FetchTexture(this, 0xa8, manager);
    float* const matrix = &Field<float>(this, 0xbc);
    for (unsigned i = 0; i < 16; ++i) matrix[i] = 0.0f;
    matrix[0] = -1.0f;
    matrix[6] = -1.0f;
    matrix[9] = 1.0f;
    matrix[15] = 1.0f;
    return this;
}

void* MaterialThunks::Gloss(MaterialManager* manager, MaterialChunkCache* chunk, AlphaBlendCache* blend,
                            unsigned int contextCount, void** contextData, bool flag)
{
    Reflective(manager, chunk, blend, contextCount, contextData, flag);
    Finish(this, "??_7GlossMaterial@Jet@@6BMaterial@1@@", "??_7GlossMaterial@Jet@@6BPluginInstance@1@@");
    return this;
}

void* MaterialThunks::GenericShader(MaterialManager* manager, MaterialChunkCache* chunkCache,
                                    AlphaBlendCache* blendCache, bool flag)
{
    Begin(this, manager, flag, "??_8GenericShaderMaterial@Jet@@7B@");
    Finish(this, "??_7GenericShaderMaterial@Jet@@6BMaterial@1@@",
           "??_7GenericShaderMaterial@Jet@@6BPluginInstance@1@@");
    Field<void*>(this, 0xa4) = nullptr;
    AddCacheReference(chunkCache);
    Field<void*>(this, 0x9c) = chunkCache;
    AddCacheReference(blendCache);
    Field<void*>(this, 0xa0) = blendCache;
    Field<unsigned int>(this, 0xa8) = 0;
    Field<unsigned int>(this, 0xac) = 0;
    Field<TextureAlpha>(this, 0xb0) = static_cast<TextureAlpha>(0);
    return this;
}

// Used by the local five-argument AutoTexMaterial constructor (U5aFinal.cpp): the shared default texture that
// OneTextureMaterial installs at +0x9c.
void InitOneTextureFields(void* self, MaterialManager* manager)
{
    OneTextureFields(self, manager);
}

}  // namespace U5a2
}  // namespace Jet

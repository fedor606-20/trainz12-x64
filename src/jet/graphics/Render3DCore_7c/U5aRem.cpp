// Unit U5a (wave 4) of Render3DCore_7c: the render, cache, chunk-creation and display-state paths left forwarded by
// U5a/U5a2.
//
// Every entry here is __cdecl with `this` on the stack (the virtual and ordinary manglings are UBA/MBA/QAA), so the
// bodies are ordinary class members. They read and write the TS12 object offsets, call the same RenderSystem_7c
// exports and the same U4 cache methods the original calls. The U4 cache singletons and their Add methods are still
// the original module's, so they are resolved from Render3DCore_7c_orig.dll at run time (workstream rule: a unit may
// call a forwarded export by name). The U6 chunk templates are header-only in this tree and are constructed locally.
#include <jet/Render3DCoreCaches.h>
#include <jet/Render3DCoreCamera.h>
#include <jet/Render3DCoreGeometry.h>
#include <jet/Render3DCoreMaterials.h>
#include <jet/VertexFormats.h>
#include <jet/VCall.h>

#include <windows.h>

#include <cstddef>
#include <cstring>
#include <new>
#include <xmmintrin.h>

namespace Jet {
namespace {

template <class T>
T& Field(void* p, unsigned offset)
{
    return *reinterpret_cast<T*>(static_cast<unsigned char*>(p) + offset);
}
template <class T>
const T& Field(const void* p, unsigned offset)
{
    return *reinterpret_cast<const T*>(static_cast<const unsigned char*>(p) + offset);
}

float X87(float f)
{
    unsigned int u;
    std::memcpy(&u, &f, 4);
    if ((u & 0x7f800000u) == 0x7f800000u && (u & 0x007fffffu) && !(u & 0x00400000u)) u |= 0x00400000u;
    std::memcpy(&f, &u, 4);
    return f;
}

// ---------------------------------------------------------------------------------------------------------------
// original module lookups (forwarded U4 caches and the shader-chunk branches U6 has not reconstructed yet)

HMODULE OriginalModule()
{
    static HMODULE module = [] {
        HMODULE m = GetModuleHandleA("Render3DCore_7c_orig.dll");
        return m ? m : LoadLibraryA("Render3DCore_7c_orig.dll");
    }();
    return module;
}

template <class F>
F OriginalProc(const char* name)
{
    return reinterpret_cast<F>(GetProcAddress(OriginalModule(), name));
}

void* OriginalData(const char* name)
{
    void** const slot = reinterpret_cast<void**>(GetProcAddress(OriginalModule(), name));
    return slot ? *slot : nullptr;
}

constexpr const char* kMaterialAdd =
    "?Add@MaterialChunkCache@Jet@@QAAXPAVDisplay@2@PAVViewPort@2@PBVMaterial@2@PAVGeometryChunk@2@PBV?$"
    "FastDynamicArray@PAVDisplayLight@Jet@@$0N@U?$Less@PAVDisplayLight@Jet@@PAV12@@2@@2@PBVMatrix4@2@@Z";
constexpr const char* kAlphaBlendAdd =
    "?Add@AlphaBlendCache@Jet@@UAAXPAVDisplay@2@PAVViewPort@2@PBVMaterial@2@PAVGeometryChunk@2@PBV?$"
    "FastDynamicArray@PAVDisplayLight@Jet@@$0N@U?$Less@PAVDisplayLight@Jet@@PAV12@@2@@2@PBVMatrix4@2@@Z";
constexpr const char* kAlphaMaskAdd =
    "?Add@AlphaMaskCache@Jet@@UAAXPBVMaterial@2@PAVGeometryChunk@2@PBV?$FastDynamicArray@PAVDisplayLight@Jet@@$0N@U?$"
    "Less@PAVDisplayLight@Jet@@PAV12@@2@@2@PBVMatrix4@2@@Z";
constexpr const char* kUnsortedAdd =
    "?Add@UnsortedAlphaBlendCache@Jet@@UAAXPBVMaterial@2@PAVGeometryChunk@2@PBV?$FastDynamicArray@PAVDisplayLight@"
    "Jet@@$0N@U?$Less@PAVDisplayLight@Jet@@PAV12@@2@@2@PBVMatrix4@2@M@Z";

using MaterialAddFn = void(__cdecl*)(void*, Display*, ViewPort*, const Material*, GeometryChunk*,
                                     const DisplayLightPtrList*, const Matrix4*);
using AlphaMaskAddFn = void(__cdecl*)(void*, const Material*, GeometryChunk*, const DisplayLightPtrList*,
                                      const Matrix4*);
using UnsortedAddFn = void(__cdecl*)(void*, const Material*, GeometryChunk*, const DisplayLightPtrList*,
                                     const Matrix4*, float);

// The four caches are U4's classes and their Add bodies are local; the differential test pins the routing by
// hooking the original module's Add and installing sentinel singletons there, so the call still goes through the
// original module's shared state. Switching these four to this module's singletons needs that test rewritten first.
void MaterialAdd(Display* display, ViewPort* view, const Material* material, GeometryChunk* chunk,
                 const DisplayLightPtrList* lights, const Matrix4* modelView)
{
    static MaterialAddFn const fn = OriginalProc<MaterialAddFn>(kMaterialAdd);
    if (fn) fn(OriginalData("?singleton@MaterialChunkCache@Jet@@2PAV12@A"), display, view, material, chunk, lights,
               modelView);
}
void AlphaBlendAdd(Display* display, ViewPort* view, const Material* material, GeometryChunk* chunk,
                   const DisplayLightPtrList* lights, const Matrix4* modelView)
{
    static MaterialAddFn const fn = OriginalProc<MaterialAddFn>(kAlphaBlendAdd);
    if (fn) fn(OriginalData("?singleton@AlphaBlendCache@Jet@@2PAV12@A"), display, view, material, chunk, lights,
               modelView);
}
void AlphaMaskAdd(const Material* material, GeometryChunk* chunk, const DisplayLightPtrList* lights,
                  const Matrix4* modelView)
{
    static AlphaMaskAddFn const fn = OriginalProc<AlphaMaskAddFn>(kAlphaMaskAdd);
    if (fn) fn(OriginalData("?singleton@AlphaMaskCache@Jet@@2PAV12@A"), material, chunk, lights, modelView);
}
void UnsortedAdd(const Material* material, GeometryChunk* chunk, const DisplayLightPtrList* lights,
                 const Matrix4* modelView, float distance)
{
    static UnsortedAddFn const fn = OriginalProc<UnsortedAddFn>(kUnsortedAdd);
    if (fn) fn(OriginalData("?singleton@UnsortedAlphaBlendCache@Jet@@2PAV12@A"), material, chunk, lights, modelView,
               distance);
}

// the chunk's opacity lives at +0x3c (protected in Geometry.h), the material's at +0x80
float ChunkOpacity(const GeometryChunk* chunk) { return Field<float>(chunk, 0x3c); }
float MaterialOpacity(const Material* material) { return Field<float>(material, 0x80); }

int BlendThreshold(const Material* material)
{
    return static_cast<int>(Field<float>(material, 0x20) * 100.0f + 0.1f);
}

// ---------------------------------------------------------------------------------------------------------------
// display slots the project header does not name; all are __cdecl with Display* first

void DisplaySetModelView(Display* display, const Matrix4* matrix) { VCall<void>(display, 0x54, matrix); }
void DisplaySetTextureMatrix(Display* display, const Matrix4* matrix) { VCall<void>(display, 0x74, 0, matrix); }
void DisplayTextureCoordGen(Display* display, int stage, unsigned mode, unsigned coordinates)
{
    display->SetTextureCoordGen(stage, static_cast<DTexGenMode>(mode), coordinates);
}

// Camera's view matrix: +0x25c dirty flag, +0x1b8 cached transform
const Matrix4* ViewMatrix(ViewPort* view)
{
    if (!view) return nullptr;
    Camera* const camera = Field<Camera*>(view, 0x20);
    if (!camera) return nullptr;
    if (Field<unsigned char>(camera, 0x25c)) {
        struct CameraAccess : Camera {
            void Update() const { UpdateCachedViewTransform(); }
        };
        reinterpret_cast<const CameraAccess*>(camera)->Update();
    }
    return &Field<Matrix4>(camera, 0x1b8);
}

const VertexProgram* ChunkProgram(GeometryChunk* chunk)
{
    return chunk ? chunk->GetVertexProgram(0) : nullptr;
}

// ---------------------------------------------------------------------------------------------------------------
// CreateGeometryChunk: the hint selects the U6 vertex template; the shader branch (system bit 0 + user bit 1 +
// user bit 3, display supports it) still belongs to the U6 shader chunks and goes to the original export.

struct HintTail {  // StandardChunkHint after the 0x10 base
    unsigned char bits;
    unsigned char padding[3];
    unsigned int a;
    unsigned int b;
    unsigned int c;
};

bool ShaderChunkPath(const Material* self, const GeometryChunkCreateHint* hint)
{
    if (!hint || !(hint->system & 1)) return false;
    const HintTail* const tail = reinterpret_cast<const HintTail*>(reinterpret_cast<const unsigned char*>(hint) + 0x10);
    if (!(tail->bits & 2) || (tail->bits & 4)) return false;
    void* const kernel = Field<void*>(const_cast<Material*>(self), 0x98);
    void* const context = kernel ? Field<void*>(kernel, 8) : nullptr;
    const Display* const display = context ? Field<Display*>(context, 0x7c) : nullptr;
    return display && Field<unsigned char>(display, 0x211) != 0;
}

template <class Chunk>
GeometryChunk* MakeChunk(unsigned int vertices, unsigned int primitives, unsigned int indices,
                         unsigned int faceNormals, unsigned int flags)
{
    void* const memory = Alloc::New(sizeof(Chunk));
    if (!memory) return nullptr;
    return ::new (memory) Chunk(vertices, primitives, indices, faceNormals, flags);
}

// the shared tail of every material's CreateGeometryChunk (0x1003dfce): version 1 or a normal chunk with user bit 1
// reserves the dynamic stream -2, user bit 0 selects the shared stream 0 or -3, and a 1-word size selects the index
// data path.
void ApplyHint(GeometryChunk* chunk, const GeometryChunkCreateHint* hint, bool normalChunk)
{
    if (!chunk || !hint) return;
    const HintTail* const tail = reinterpret_cast<const HintTail*>(reinterpret_cast<const unsigned char*>(hint) + 0x10);
    const bool bit0 = (hint->system & 1) && (tail->bits & 1);
    const bool bit1 = (hint->system & 1) && (tail->bits & 2);
    (void)normalChunk;
    const bool second = bit1;
    if (hint->version == 1 || (hint->version == 2 && second)) chunk->SetVertexDataDynamic(0, -2, true);
    if (bit0 || bit1) chunk->SetVertexDataShared(0, 0, false);
    if (hint->size == 1) chunk->SetIndexDataDynamic();
}

// the flat (plain template) body
GeometryChunk* FinishChunk(Material* self, GeometryChunk* chunk, unsigned int vertices, unsigned int primitives,
                           unsigned int indices, unsigned int faceNormals, Material::GeometryType type,
                           GeometryChunkCreateHint* hint, bool normalChunk)
{
    (void)self; (void)vertices; (void)primitives; (void)indices; (void)faceNormals; (void)type;
    ApplyHint(chunk, hint, normalChunk);
    return chunk;
}

using CreateGeometryChunkFn = GeometryChunk*(__cdecl*)(const void*, unsigned int, unsigned int, unsigned int,
                                                       unsigned int, Material::GeometryType, GeometryChunkCreateHint*);

GeometryChunk* OriginalCreateGeometryChunk(const char* name, const void* self, unsigned int vertices,
                                           unsigned int primitives, unsigned int indices, unsigned int faceNormals,
                                           Material::GeometryType type, GeometryChunkCreateHint* hint)
{
    static CreateGeometryChunkFn noTexture = OriginalProc<CreateGeometryChunkFn>(
        "?CreateGeometryChunk@NoTextureMaterial@Jet@@UBAPAVGeometryChunk@2@IIIIW4GeometryType@Material@2@PAV"
        "GeometryChunkCreateHint@2@@Z");
    static CreateGeometryChunkFn oneTexture = OriginalProc<CreateGeometryChunkFn>(
        "?CreateGeometryChunk@OneTextureMaterial@Jet@@UBAPAVGeometryChunk@2@IIIIW4GeometryType@Material@2@PAV"
        "GeometryChunkCreateHint@2@@Z");
    static CreateGeometryChunkFn oneTextureNoAlpha = OriginalProc<CreateGeometryChunkFn>(
        "?CreateGeometryChunk@OneTextureNoAlphaMaterial@Jet@@UBAPAVGeometryChunk@2@IIIIW4GeometryType@Material@2@"
        "PAVGeometryChunkCreateHint@2@@Z");
    static CreateGeometryChunkFn autoTex = OriginalProc<CreateGeometryChunkFn>(
        "?CreateGeometryChunk@AutoTexMaterial@Jet@@UBAPAVGeometryChunk@2@IIIIW4GeometryType@Material@2@PAV"
        "GeometryChunkCreateHint@2@@Z");
    static CreateGeometryChunkFn colorOne = OriginalProc<CreateGeometryChunkFn>(
        "?CreateGeometryChunk@ColorOneTextureMaterial@Jet@@UBAPAVGeometryChunk@2@IIIIW4GeometryType@Material@2@PAV"
        "GeometryChunkCreateHint@2@@Z");
    CreateGeometryChunkFn fn = nullptr;
    if (std::strstr(name, "NoTextureMaterial")) fn = noTexture;
    else if (std::strstr(name, "OneTextureNoAlphaMaterial")) fn = oneTextureNoAlpha;
    else if (std::strstr(name, "OneTextureMaterial")) fn = oneTexture;
    else if (std::strstr(name, "AutoTexMaterial")) fn = autoTex;
    else if (std::strstr(name, "ColorOneTextureMaterial")) fn = colorOne;
    return fn ? fn(self, vertices, primitives, indices, faceNormals, type, hint) : nullptr;
}

// ---------------------------------------------------------------------------------------------------------------
// GenericShader's texture ring: circular 0x14-byte blocks {next, prev, count, items[1]}, item {int type, Texture*}

struct RingBlock {
    RingBlock* next;
    RingBlock* prev;
    unsigned int count;
};
struct RingItem {
    int type;
    Texture* texture;
};
static_assert(sizeof(RingBlock) == 0x0c, "ring block head");

RingItem* Items(RingBlock* block) { return reinterpret_cast<RingItem*>(reinterpret_cast<unsigned char*>(block) + 0x0c); }
const RingItem* Items(const RingBlock* block)
{
    return reinterpret_cast<const RingItem*>(reinterpret_cast<const unsigned char*>(block) + 0x0c);
}

template <class F>
void ForEachRingItem(const void* head, F f)
{
    RingBlock* const first = const_cast<RingBlock*>(static_cast<const RingBlock*>(head));
    if (!first) return;
    RingBlock* block = first;
    for (;;) {
        RingItem* const items = Items(block);
        for (unsigned int i = 0; i < block->count; ++i) f(items[i]);
        block = block->next;
        if (block == first || !block) return;
    }
}

RingItem* FindRingItem(void* head, int type)
{
    RingItem* found = nullptr;
    ForEachRingItem(head, [&](RingItem& item) {
        if (!found && item.type == type) found = &item;
    });
    return found;
}

void AppendRingItem(void** headSlot, int type, Texture* texture)
{
    RingBlock* const head = static_cast<RingBlock*>(*headSlot);
    RingBlock* block;
    if (!head) {
        block = static_cast<RingBlock*>(Alloc::New(0x14));
        block->count = 0;
        *headSlot = block;
        block->next = block;
        block->prev = block;
    } else {
        block = head->prev;
        if (block->count == 1) {
            auto* const made = static_cast<RingBlock*>(Alloc::New(0x14));
            made->count = 0;
            made->next = head;
            made->prev = block;
            head->prev = made;
            block->next = made;
            block = made;
        }
    }
    RingItem* const item = Items(block) + block->count;
    ++block->count;
    item->type = type;
    item->texture = texture;
    if (texture) texture->AddReference();
}

Texture* GenericShaderGetTexture(const GenericShaderMaterial* self, int type)
{
    RingItem* const item = FindRingItem(const_cast<GenericShaderMaterial*>(self)->textureRing, type);
    return item ? item->texture : nullptr;
}

}  // namespace

// ---------------------------------------------------------------------------------------------------------------
// NoTextureMaterial

GeometryChunk* __cdecl NoTextureMaterial::CreateGeometryChunk(unsigned int vertices, unsigned int primitives,
                                                              unsigned int indices, unsigned int faceNormals,
                                                              GeometryType type, GeometryChunkCreateHint* hint) const
{
    if (ShaderChunkPath(this, hint))
        return OriginalCreateGeometryChunk("NoTextureMaterial", this, vertices, primitives, indices, faceNormals,
                                           type, hint);
    const unsigned int flags = MakeGeometryChunkFlags(type, hint);
    const bool nml = hint && (hint->system & 1) &&
                     (reinterpret_cast<const HintTail*>(reinterpret_cast<const unsigned char*>(hint) + 0x10)->bits & 1);
    GeometryChunk* const chunk = nml ? MakeChunk<PackedVertexPCGeometryNmlChunk>(vertices, primitives, indices,
                                                                                 faceNormals, flags)
                                     : MakeChunk<PackedVertexPNGeometryChunk>(vertices, primitives, indices,
                                                                              faceNormals, flags);
    return FinishChunk(const_cast<NoTextureMaterial*>(this), chunk, vertices, primitives, indices, faceNormals, type,
                       hint, nml);
}

void __cdecl NoTextureMaterial::CacheChunk(Display* display, ViewPort* view, GeometryChunk* chunk,
                                           const DisplayLightPtrList* lights, const Matrix4* modelView) const
{
    const float product = MaterialOpacity(this) * ChunkOpacity(chunk);
    if (1.0f <= product) {
        MaterialAdd(display, view, this, chunk, lights, modelView);
        return;
    }
    if (BlendThreshold(this) < 0x122)
        AlphaBlendAdd(display, view, this, chunk, lights, modelView);
    else
        UnsortedAdd(this, chunk, lights, modelView, 0.0f);
}

void __cdecl NoTextureMaterial::RenderChunk(Display* display, ViewPort* view, GeometryChunk* chunk,
                                            const DisplayLightPtrList* lights, const Matrix4* modelView,
                                            int pass) const
{
    (void)pass;
    const VertexProgram* const program = ChunkProgram(chunk);
    SetDisplayState(display);
    const Matrix4* viewMatrix = nullptr;
    float opacity;
    if (view) {
        opacity = ChunkOpacity(chunk) * MaterialOpacity(this);
        viewMatrix = ViewMatrix(view);
        SetLightingState(display, viewMatrix, chunk, program, lights, modelView, opacity, false);
    } else {
        opacity = MaterialOpacity(this) * ChunkOpacity(chunk);
        SetLightingState(display, nullptr, chunk, program, lights, modelView, opacity, false);
        viewMatrix = modelView;
    }
    DisplaySetModelView(display, modelView);
    if (chunk) chunk->RenderGeometry(display, view, program, true, 0);
    ClearDisplayState(display);
}

void __cdecl NoTextureMaterial::RenderNumChunks(Display* display, ViewPort* view, GeometryChunk** chunks,
                                                const DisplayLightPtrList** lights, const Matrix4** modelViews,
                                                unsigned int numChunks, int pass) const
{
    (void)pass;
    SetDisplayState(display);
    const Matrix4* viewMatrix = ViewMatrix(view);
    for (unsigned int i = 0; i < numChunks; ++i) {
        GeometryChunk* const chunk = chunks[i];
        const VertexProgram* const program = ChunkProgram(chunk);
        float opacity;
        const Matrix4* modelView = modelViews[i];
        if (view) {
            opacity = ChunkOpacity(chunk) * MaterialOpacity(this);
            SetLightingState(display, viewMatrix, chunk, program, lights[i], modelView, opacity, false);
        } else {
            opacity = MaterialOpacity(this) * ChunkOpacity(chunk);
            SetLightingState(display, nullptr, chunk, program, lights[i], modelView, opacity, false);
            viewMatrix = modelView;
        }
        DisplaySetModelView(display, modelView);
        if (chunk) chunk->RenderGeometry(display, view, program, true, 0);
    }
    ClearDisplayState(display);
}

void __cdecl NoTextureMaterial::RenderPrimitives(Display* display, ViewPort* view, GeometryChunk* chunk,
                                                 const DisplayLightPtrList* lights, const Matrix4* modelView,
                                                 unsigned int numIndices, const unsigned short* indices,
                                                 unsigned int numPrimitives, DPrimitive primitive, int pass) const
{
    (void)pass;
    const VertexProgram* const program = ChunkProgram(chunk);
    SetDisplayState(display);
    float opacity;
    const Matrix4* viewMatrix = nullptr;
    if (view) {
        opacity = ChunkOpacity(chunk) * MaterialOpacity(this);
        viewMatrix = ViewMatrix(view);
        SetLightingState(display, viewMatrix, chunk, program, lights, modelView, opacity, false);
    } else {
        opacity = MaterialOpacity(this) * ChunkOpacity(chunk);
        SetLightingState(display, nullptr, chunk, program, lights, modelView, opacity, false);
        viewMatrix = modelView;
    }
    DisplaySetModelView(display, modelView);
    if (chunk)
        chunk->RenderPrimitives(display, view, program, numIndices, indices, numPrimitives, primitive, true, 0);
    ClearDisplayState(display);
}

// ---------------------------------------------------------------------------------------------------------------
// OneTextureMaterial

GeometryChunk* __cdecl OneTextureMaterial::CreateGeometryChunk(unsigned int vertices, unsigned int primitives,
                                                               unsigned int indices, unsigned int faceNormals,
                                                               GeometryType type, GeometryChunkCreateHint* hint) const
{
    if (ShaderChunkPath(this, hint))
        return OriginalCreateGeometryChunk("OneTextureMaterial", this, vertices, primitives, indices, faceNormals,
                                           type, hint);
    const unsigned int flags = MakeGeometryChunkFlags(type, hint);
    const bool nml = hint && (hint->system & 1) &&
                     (reinterpret_cast<const HintTail*>(reinterpret_cast<const unsigned char*>(hint) + 0x10)->bits & 1);
    GeometryChunk* const chunk = nml ? MakeChunk<PackedVertexPCTGeometryNmlChunk>(vertices, primitives, indices,
                                                                                  faceNormals, flags)
                                     : MakeChunk<PackedVertexPNTGeometryChunk>(vertices, primitives, indices,
                                                                               faceNormals, flags);
    return FinishChunk(const_cast<OneTextureMaterial*>(this), chunk, vertices, primitives, indices, faceNormals, type,
                       hint, nml);
}

void __cdecl OneTextureMaterial::CacheChunk(Display* display, ViewPort* view, GeometryChunk* chunk,
                                            const DisplayLightPtrList* lights, const Matrix4* modelView) const
{
    OneTextureMaterial* const self = const_cast<OneTextureMaterial*>(this);
    if (self->alphaHint == static_cast<TextureAlpha>(-1)) {
        if (self->IsLoaded().code == 0) self->CacheAlphaHint();
    }
    const float product = MaterialOpacity(this) * ChunkOpacity(chunk);
    if (self->alphaHint != static_cast<TextureAlpha>(1) && 1.0f <= product) {
        if (self->alphaHint == static_cast<TextureAlpha>(2))
            AlphaMaskAdd(this, chunk, lights, modelView);
        else
            MaterialAdd(display, view, this, chunk, lights, modelView);
        return;
    }
    if (BlendThreshold(this) < 0x122)
        AlphaBlendAdd(display, view, this, chunk, lights, modelView);
    else
        UnsortedAdd(this, chunk, lights, modelView, 0.0f);
}

void __cdecl OneTextureMaterial::RenderChunk(Display* display, ViewPort* view, GeometryChunk* chunk,
                                             const DisplayLightPtrList* lights, const Matrix4* modelView,
                                             int pass) const
{
    (void)pass;
    const VertexProgram* const program = ChunkProgram(chunk);
    SetDisplayState(display);
    float opacity;
    const Matrix4* viewMatrix = nullptr;
    if (view) {
        opacity = ChunkOpacity(chunk) * MaterialOpacity(this);
        viewMatrix = ViewMatrix(view);
        SetLightingState(display, viewMatrix, chunk, program, lights, modelView, opacity, false);
    } else {
        opacity = MaterialOpacity(this) * ChunkOpacity(chunk);
        SetLightingState(display, nullptr, chunk, program, lights, modelView, opacity, false);
        viewMatrix = modelView;
    }
    DisplaySetModelView(display, modelView);
    if (chunk) chunk->RenderGeometry(display, view, program, true, 0);
    ClearDisplayState(display);
}

void __cdecl OneTextureMaterial::RenderNumChunks(Display* display, ViewPort* view, GeometryChunk** chunks,
                                                 const DisplayLightPtrList** lights, const Matrix4** modelViews,
                                                 unsigned int numChunks, int pass) const
{
    (void)pass;
    SetDisplayState(display);
    const Matrix4* cameraView = ViewMatrix(view);
    for (unsigned int i = 0; i < numChunks; ++i) {
        GeometryChunk* const chunk = chunks[i];
        const VertexProgram* const program = ChunkProgram(chunk);
        const Matrix4* const modelView = modelViews[i];
        float opacity;
        if (view) {
            opacity = ChunkOpacity(chunk) * MaterialOpacity(this);
            SetLightingState(display, cameraView, chunk, program, lights[i], modelView, opacity, false);
        } else {
            opacity = MaterialOpacity(this) * ChunkOpacity(chunk);
            SetLightingState(display, nullptr, chunk, program, lights[i], modelView, opacity, false);
        }
        DisplaySetModelView(display, modelView);
        if (chunk) chunk->RenderGeometry(display, view, program, true, 0);
    }
    ClearDisplayState(display);
}

void __cdecl OneTextureMaterial::RenderPrimitives(Display* display, ViewPort* view, GeometryChunk* chunk,
                                                  const DisplayLightPtrList* lights, const Matrix4* modelView,
                                                  unsigned int numIndices, const unsigned short* indices,
                                                  unsigned int numPrimitives, DPrimitive primitive, int pass) const
{
    (void)pass;
    const VertexProgram* const program = ChunkProgram(chunk);
    SetDisplayState(display);
    float opacity;
    const Matrix4* viewMatrix = nullptr;
    if (view) {
        opacity = ChunkOpacity(chunk) * MaterialOpacity(this);
        viewMatrix = ViewMatrix(view);
        SetLightingState(display, viewMatrix, chunk, program, lights, modelView, opacity, false);
    } else {
        opacity = MaterialOpacity(this) * ChunkOpacity(chunk);
        SetLightingState(display, nullptr, chunk, program, lights, modelView, opacity, false);
        viewMatrix = modelView;
    }
    DisplaySetModelView(display, modelView);
    if (chunk)
        chunk->RenderPrimitives(display, view, program, numIndices, indices, numPrimitives, primitive, true, 0);
    ClearDisplayState(display);
}

// ---------------------------------------------------------------------------------------------------------------
// OneTextureNoAlphaMaterial

GeometryChunk* __cdecl OneTextureNoAlphaMaterial::CreateGeometryChunk(
    unsigned int vertices, unsigned int primitives, unsigned int indices, unsigned int faceNormals, GeometryType type,
    GeometryChunkCreateHint* hint) const
{
    if (ShaderChunkPath(this, hint))
        return OriginalCreateGeometryChunk("OneTextureNoAlphaMaterial", this, vertices, primitives, indices,
                                           faceNormals, type, hint);
    const unsigned int flags = MakeGeometryChunkFlags(type, hint);
    const bool nml = hint && (hint->system & 1) &&
                     (reinterpret_cast<const HintTail*>(reinterpret_cast<const unsigned char*>(hint) + 0x10)->bits & 1);
    GeometryChunk* const chunk = nml ? MakeChunk<PackedVertexPCTGeometryNmlChunk>(vertices, primitives, indices,
                                                                                  faceNormals, flags)
                                     : MakeChunk<PackedVertexPNTGeometryChunk>(vertices, primitives, indices,
                                                                               faceNormals, flags);
    return FinishChunk(const_cast<OneTextureNoAlphaMaterial*>(this), chunk, vertices, primitives, indices,
                       faceNormals, type, hint, nml);
}

void __cdecl OneTextureNoAlphaMaterial::CacheChunk(Display* display, ViewPort* view, GeometryChunk* chunk,
                                                   const DisplayLightPtrList* lights,
                                                   const Matrix4* modelView) const
{
    MaterialAdd(display, view, this, chunk, lights, modelView);
}

void __cdecl OneTextureNoAlphaMaterial::RenderChunk(Display* display, ViewPort* view, GeometryChunk* chunk,
                                                    const DisplayLightPtrList* lights, const Matrix4* modelView,
                                                    int pass) const
{
    OneTextureMaterial::RenderChunk(display, view, chunk, lights, modelView, pass);
}

void __cdecl OneTextureNoAlphaMaterial::RenderNumChunks(Display* display, ViewPort* view, GeometryChunk** chunks,
                                                        const DisplayLightPtrList** lights,
                                                        const Matrix4** modelViews, unsigned int numChunks,
                                                        int pass) const
{
    OneTextureMaterial::RenderNumChunks(display, view, chunks, lights, modelViews, numChunks, pass);
}

void __cdecl OneTextureNoAlphaMaterial::RenderPrimitives(
    Display* display, ViewPort* view, GeometryChunk* chunk, const DisplayLightPtrList* lights,
    const Matrix4* modelView, unsigned int numIndices, const unsigned short* indices, unsigned int numPrimitives,
    DPrimitive primitive, int pass) const
{
    OneTextureMaterial::RenderPrimitives(display, view, chunk, lights, modelView, numIndices, indices, numPrimitives,
                                         primitive, pass);
}

// ---------------------------------------------------------------------------------------------------------------
// BillboardMaterial

void __cdecl BillboardMaterial::RenderChunk(Display* display, ViewPort* view, GeometryChunk* chunk,
                                            const DisplayLightPtrList* lights, const Matrix4* modelView,
                                            int pass) const
{
    (void)pass;
    const VertexProgram* const program = ChunkProgram(chunk);
    SetDisplayState(display);

    // the billboard keeps the translation row of the model-view matrix and replaces the rotation with identity
    Matrix4 billboard;
    std::memset(&billboard, 0, sizeof billboard);
    billboard.m[12] = modelView->m[12];
    billboard.m[13] = modelView->m[13];
    billboard.m[14] = modelView->m[14];
    billboard.m[0] = 1.0f;
    billboard.m[5] = 1.0f;
    billboard.m[10] = 1.0f;
    billboard.m[15] = 1.0f;
    DisplaySetModelView(display, &billboard);

    float opacity;
    if (view) {
        opacity = ChunkOpacity(chunk) * MaterialOpacity(this);
        const Matrix4* const viewMatrix = ViewMatrix(view);
        SetLightingState(display, viewMatrix, chunk, program, lights, &billboard, opacity, false);
    } else {
        opacity = MaterialOpacity(this) * ChunkOpacity(chunk);
        SetLightingState(display, nullptr, chunk, program, lights, &billboard, opacity, false);
    }
    if (chunk) chunk->RenderGeometry(display, view, program, true, 0);
    ClearDisplayState(display);
}

void __cdecl BillboardMaterial::RenderNumChunks(Display* display, ViewPort* view, GeometryChunk** chunks,
                                                const DisplayLightPtrList** lights, const Matrix4** modelViews,
                                                unsigned int numChunks, int pass) const
{
    (void)pass;
    SetDisplayState(display);
    const Matrix4* const cameraView = ViewMatrix(view);
    for (unsigned int i = 0; i < numChunks; ++i) {
        GeometryChunk* const chunk = chunks[i];
        const VertexProgram* const program = ChunkProgram(chunk);
        Matrix4 billboard;
        std::memset(&billboard, 0, sizeof billboard);
        billboard.m[12] = modelViews[i]->m[12];
        billboard.m[13] = modelViews[i]->m[13];
        billboard.m[14] = modelViews[i]->m[14];
        billboard.m[0] = 1.0f;
        billboard.m[5] = 1.0f;
        billboard.m[10] = 1.0f;
        billboard.m[15] = 1.0f;
        DisplaySetModelView(display, &billboard);
        float opacity;
        if (view) {
            opacity = ChunkOpacity(chunk) * MaterialOpacity(this);
            SetLightingState(display, cameraView, chunk, program, lights[i], &billboard, opacity, false);
        } else {
            opacity = MaterialOpacity(this) * ChunkOpacity(chunk);
            SetLightingState(display, nullptr, chunk, program, lights[i], &billboard, opacity, false);
        }
        if (chunk) chunk->RenderGeometry(display, view, program, true, 0);
    }
    ClearDisplayState(display);
}

void __cdecl BillboardMaterial::RenderPrimitives(Display* display, ViewPort* view, GeometryChunk* chunk,
                                                 const DisplayLightPtrList* lights, const Matrix4* modelView,
                                                 unsigned int numIndices, const unsigned short* indices,
                                                 unsigned int numPrimitives, DPrimitive primitive, int pass) const
{
    (void)pass;
    const VertexProgram* const program = ChunkProgram(chunk);
    SetDisplayState(display);
    Matrix4 billboard;
    std::memset(&billboard, 0, sizeof billboard);
    billboard.m[12] = modelView->m[12];
    billboard.m[13] = modelView->m[13];
    billboard.m[14] = modelView->m[14];
    billboard.m[0] = 1.0f;
    billboard.m[5] = 1.0f;
    billboard.m[10] = 1.0f;
    billboard.m[15] = 1.0f;
    DisplaySetModelView(display, &billboard);
    float opacity;
    if (view) {
        opacity = ChunkOpacity(chunk) * MaterialOpacity(this);
        SetLightingState(display, ViewMatrix(view), chunk, program, lights, &billboard, opacity, false);
    } else {
        opacity = MaterialOpacity(this) * ChunkOpacity(chunk);
        SetLightingState(display, nullptr, chunk, program, lights, &billboard, opacity, false);
    }
    if (chunk)
        chunk->RenderPrimitives(display, view, program, numIndices, indices, numPrimitives, primitive, true, 0);
    ClearDisplayState(display);
}

// ---------------------------------------------------------------------------------------------------------------
// ColorOneTextureMaterial (the render paths are the one-texture body; only the chunk type differs)

GeometryChunk* __cdecl ColorOneTextureMaterial::CreateGeometryChunk(
    unsigned int vertices, unsigned int primitives, unsigned int indices, unsigned int faceNormals, GeometryType type,
    GeometryChunkCreateHint* hint) const
{
    if (ShaderChunkPath(this, hint))
        return OriginalCreateGeometryChunk("ColorOneTextureMaterial", this, vertices, primitives, indices,
                                           faceNormals, type, hint);
    unsigned char adjusted[0x20];
    const GeometryChunkCreateHint* flagsHint = hint;
    if (hint) {
        std::memcpy(adjusted, hint, sizeof adjusted);
        if ((reinterpret_cast<const GeometryChunkCreateHint*>(adjusted)->system & 1) &&
            (reinterpret_cast<const HintTail*>(adjusted + 0x10)->bits & 1))
            reinterpret_cast<HintTail*>(adjusted + 0x10)->bits &= 0xfe;
        flagsHint = reinterpret_cast<const GeometryChunkCreateHint*>(adjusted);
    }
    const unsigned int flags = MakeGeometryChunkFlags(type, flagsHint);
    GeometryChunk* const chunk =
        MakeChunk<PackedVertexPCNTGeometryChunk>(vertices, primitives, indices, faceNormals, flags);
    if (chunk && hint) {
        const bool bit1 = (hint->system & 1) &&
                          (reinterpret_cast<const HintTail*>(reinterpret_cast<const unsigned char*>(hint) + 0x10)->bits & 2);
        if (hint->version == 1 || (hint->version == 2 && bit1)) chunk->SetVertexDataDynamic(0, -2, true);
        if (bit1) chunk->SetVertexDataShared(0, 0, false);
        if (hint->size == 1) chunk->SetIndexDataDynamic();
    }
    return chunk;
}

void __cdecl ColorOneTextureMaterial::RenderChunk(Display* display, ViewPort* view, GeometryChunk* chunk,
                                                  const DisplayLightPtrList* lights, const Matrix4* modelView,
                                                  int pass) const
{
    OneTextureMaterial::RenderChunk(display, view, chunk, lights, modelView, pass);
}
void __cdecl ColorOneTextureMaterial::RenderNumChunks(Display* display, ViewPort* view, GeometryChunk** chunks,
                                                      const DisplayLightPtrList** lights,
                                                      const Matrix4** modelViews, unsigned int numChunks,
                                                      int pass) const
{
    OneTextureMaterial::RenderNumChunks(display, view, chunks, lights, modelViews, numChunks, pass);
}
void __cdecl ColorOneTextureMaterial::RenderPrimitives(
    Display* display, ViewPort* view, GeometryChunk* chunk, const DisplayLightPtrList* lights,
    const Matrix4* modelView, unsigned int numIndices, const unsigned short* indices, unsigned int numPrimitives,
    DPrimitive primitive, int pass) const
{
    OneTextureMaterial::RenderPrimitives(display, view, chunk, lights, modelView, numIndices, indices, numPrimitives,
                                         primitive, pass);
}

// ---------------------------------------------------------------------------------------------------------------
// AutoTexMaterial

GeometryChunk* __cdecl AutoTexMaterial::CreateGeometryChunk(unsigned int vertices, unsigned int primitives,
                                                            unsigned int indices, unsigned int faceNormals,
                                                            GeometryType type, GeometryChunkCreateHint* hint) const
{
    if (ShaderChunkPath(this, hint))
        return OriginalCreateGeometryChunk("AutoTexMaterial", this, vertices, primitives, indices, faceNormals, type,
                                           hint);
    const unsigned int flags = MakeGeometryChunkFlags(type, hint);
    const bool nml = hint && (hint->system & 1) &&
                     (reinterpret_cast<const HintTail*>(reinterpret_cast<const unsigned char*>(hint) + 0x10)->bits & 1);
    GeometryChunk* const chunk = nml ? MakeChunk<PackedVertexPCGeometryNmlChunk>(vertices, primitives, indices,
                                                                                 faceNormals, flags)
                                     : MakeChunk<PackedVertexPNGeometryChunk>(vertices, primitives, indices,
                                                                              faceNormals, flags);
    return FinishChunk(const_cast<AutoTexMaterial*>(this), chunk, vertices, primitives, indices, faceNormals, type,
                       hint, nml);
}

void __cdecl AutoTexMaterial::SetDisplayState(Display* display) const
{
    display->SetTextureCoordGen(0, static_cast<DTexGenMode>(2), 2);
    const float a = textureGeneration[0];
    const float b = textureGeneration[1];
    const float c = textureGeneration[2];
    const float d = textureGeneration[3];
    Matrix4 matrix;
    std::memset(&matrix, 0, sizeof matrix);
    matrix.m[0] = a;
    matrix.m[5] = b;
    matrix.m[10] = 1.0f;
    matrix.m[12] = c * a;
    matrix.m[13] = d * b;
    matrix.m[15] = 1.0f;
    DisplaySetTextureMatrix(display, &matrix);
    OneTextureMaterial::SetDisplayState(display);
}

void __cdecl AutoTexMaterial::RenderChunk(Display* display, ViewPort* view, GeometryChunk* chunk,
                                          const DisplayLightPtrList* lights, const Matrix4* modelView,
                                          int pass) const
{
    OneTextureMaterial::RenderChunk(display, view, chunk, lights, modelView, pass);
}
void __cdecl AutoTexMaterial::RenderNumChunks(Display* display, ViewPort* view, GeometryChunk** chunks,
                                              const DisplayLightPtrList** lights, const Matrix4** modelViews,
                                              unsigned int numChunks, int pass) const
{
    OneTextureMaterial::RenderNumChunks(display, view, chunks, lights, modelViews, numChunks, pass);
}
void __cdecl AutoTexMaterial::RenderPrimitives(Display* display, ViewPort* view, GeometryChunk* chunk,
                                               const DisplayLightPtrList* lights, const Matrix4* modelView,
                                               unsigned int numIndices, const unsigned short* indices,
                                               unsigned int numPrimitives, DPrimitive primitive, int pass) const
{
    OneTextureMaterial::RenderPrimitives(display, view, chunk, lights, modelView, numIndices, indices, numPrimitives,
                                         primitive, pass);
}

// ---------------------------------------------------------------------------------------------------------------
// MVHSelectMaterial

void __cdecl MVHSelectMaterial::CacheChunk(Display* display, ViewPort* view, GeometryChunk* chunk,
                                           const DisplayLightPtrList* lights, const Matrix4* modelView) const
{
    OneTextureMaterial::CacheChunk(display, view, chunk, lights, modelView);
}

void __cdecl MVHSelectMaterial::RenderChunk(Display* display, ViewPort* view, GeometryChunk* chunk,
                                            const DisplayLightPtrList* lights, const Matrix4* modelView,
                                            int pass) const
{
    SetDisplayState(display);
    OneTextureMaterial::RenderChunk(display, view, chunk, lights, modelView, pass);
    ClearDisplayState(display);
}

void __cdecl MVHSelectMaterial::RenderNumChunks(Display* display, ViewPort* view, GeometryChunk** chunks,
                                                const DisplayLightPtrList** lights, const Matrix4** modelViews,
                                                unsigned int numChunks, int pass) const
{
    SetDisplayState(display);
    OneTextureMaterial::RenderNumChunks(display, view, chunks, lights, modelViews, numChunks, pass);
    ClearDisplayState(display);
}

void __cdecl MVHSelectMaterial::RenderPrimitives(
    Display* display, ViewPort* view, GeometryChunk* chunk, const DisplayLightPtrList* lights,
    const Matrix4* modelView, unsigned int numIndices, const unsigned short* indices, unsigned int numPrimitives,
    DPrimitive primitive, int pass) const
{
    SetDisplayState(display);
    OneTextureMaterial::RenderPrimitives(display, view, chunk, lights, modelView, numIndices, indices, numPrimitives,
                                         primitive, pass);
    ClearDisplayState(display);
}

// ---------------------------------------------------------------------------------------------------------------
// ReflectiveMaterial / GlossMaterial state

void __cdecl ReflectiveMaterial::Bind(Display* display, ViewPort*, unsigned int pass)
{
    if (pass == 0) SetDisplayState(display, false);
}

void __cdecl GlossMaterial::Bind(Display* display, ViewPort*, unsigned int pass)
{
    if (pass == 0) ReflectiveMaterial::SetDisplayState(display, false);
}

void __cdecl ReflectiveMaterial::SetDisplayState(Display* display, bool update) const
{
    OneTextureMaterial::SetDisplayState(display);
    // bit 2 follows the display's state byte +0x75 bit 0
    const unsigned char state = display->State()[0x75];
    unsigned char f = flags2;
    f = static_cast<unsigned char>((f & ~4u) | ((state & 1u) << 2));
    Field<unsigned char>(const_cast<ReflectiveMaterial*>(this), 0xb8) = f;

    if (contextCount == 0 || !cubeTexture.Get() || contextCount < 2) return;
    Texture* const cube = const_cast<ReflectiveMaterial*>(this)->cubeTexture.Get();
    cube->Bind(1);
    if (!cube->IsAvailable()) return;
    if (update) {
        if (cube->GetDimension() == static_cast<TextureDimension>(1)) {
            if (f & 2) display->SetTextureCoordGen(1, static_cast<DTexGenMode>(5), 3);
        } else if (f & 1) {
            display->SetTextureCoordGen(1, static_cast<DTexGenMode>(3), 2);
        }
        display->SetLocalViewSpecularMode(true);
    }
    for (unsigned int i = 0; i < contextCount; ++i) {
        const Color color{reflectivity, reflectivity, reflectivity, reflectivity};
        VCall<void>(display, 0x130, i, &color);
        VCall<void>(display, 0x128, contextData[i]);
    }
}

void __cdecl ReflectiveMaterial::InitInstance(ReflectiveMaterial& instance) const
{
    Texture* const cube = const_cast<ReflectiveMaterial*>(this)->cubeTexture.Get();
    if (cube) cube->AddReference();
    instance.cubeTexture = cube;
    instance.reflectivity = reflectivity;
    instance.contextCount = contextCount;
    instance.contextData = contextData;
    instance.flags2 = static_cast<unsigned char>((instance.flags2 & ~3u) | (flags2 & 3u));
    // the 0x40-byte cube-map matrix is copied through x87 (fld/fstp), which quiets a signalling NaN
    for (unsigned int i = 0; i < 16; ++i)
        Field<float>(&instance, 0xbc + i * 4) = X87(Field<float>(this, 0xbc + i * 4));
    Material::InitInstance(instance);
    Texture* const texture = const_cast<ReflectiveMaterial*>(this)->texture.Get();
    if (texture) texture->AddReference();
    instance.texture = texture;
}

void __cdecl GlossMaterial::InitInstance(GlossMaterial& instance) const
{
    ReflectiveMaterial::InitInstance(instance);
}

void __cdecl GlossMaterial::CacheChunk(Display* display, ViewPort* view, GeometryChunk* chunk,
                                       const DisplayLightPtrList* lights, const Matrix4* modelView) const
{
    if (!(flags2 & 1)) SetSphericalTexureCoordinates(chunk, modelView);
    OneTextureMaterial::CacheChunk(display, view, chunk, lights, modelView);
}

void __cdecl ReflectiveMaterial::SetSphericalTexureCoordinates(GeometryChunk* chunk, const Matrix4* matrix)
{
    if (!chunk || !matrix) return;
    // The original's item views return their {stride, data} pair through a hidden stack buffer (the record has a
    // non-trivial copy), so the two const views are called with the original's 4-argument ABI.
    struct ItemView {
        unsigned int stride;
        const void* data;
    };
    using ItemViewFn = void(__cdecl*)(void* self, ItemView* out, int type, int stream);
    ItemView position, normal, uv;
    std::memset(&position, 0, sizeof position);
    std::memset(&normal, 0, sizeof normal);
    std::memset(&uv, 0, sizeof uv);
    void** const vtable = *reinterpret_cast<void***>(chunk);
    reinterpret_cast<ItemViewFn>(vtable[0xc4 / 4])(chunk, &position, 1, 0);
    reinterpret_cast<ItemViewFn>(vtable[0xc4 / 4])(chunk, &normal, 4, 0);
    reinterpret_cast<ItemViewFn>(vtable[0xbc / 4])(chunk, &uv, 0x10, 0);
    if (!position.stride || !normal.data || !uv.data) return;

    const float* const m = matrix->m;
    const unsigned int count = chunk->GetNumVertices();
    const unsigned char* n = reinterpret_cast<const unsigned char*>(normal.data);
    const unsigned char* t = reinterpret_cast<const unsigned char*>(uv.data);
    // the position stream is what the original validates; it is not read in the loop
    (void)position;
    for (unsigned int i = 0; i < count; ++i) {
        const float* const nf = reinterpret_cast<const float*>(n);
        float* const tf = const_cast<float*>(reinterpret_cast<const float*>(t));
        // the original computes v first, then u, with this exact SSE operand order (0x1005aaea): every ADDSS
        // keeps the first product as its destination, which is what selects the NaN payload inside the loop
        __m128 v = _mm_mul_ss(_mm_load_ss(&nf[0]), _mm_load_ss(&m[1]));
        v = _mm_add_ss(v, _mm_mul_ss(_mm_load_ss(&nf[1]), _mm_load_ss(&m[5])));
        v = _mm_add_ss(v, _mm_mul_ss(_mm_load_ss(&nf[2]), _mm_load_ss(&m[9])));
        __m128 u = _mm_mul_ss(_mm_load_ss(&nf[1]), _mm_load_ss(&m[4]));
        u = _mm_add_ss(u, _mm_mul_ss(_mm_load_ss(&nf[2]), _mm_load_ss(&m[8])));
        u = _mm_add_ss(u, _mm_mul_ss(_mm_load_ss(&nf[0]), _mm_load_ss(&m[0])));
        const __m128 half = _mm_set_ss(0.5f);
        u = _mm_mul_ss(u, half);
        v = _mm_mul_ss(v, _mm_set_ss(-0.5f));
        u = _mm_add_ss(u, half);
        v = _mm_add_ss(v, half);
        _mm_store_ss(&tf[0], u);
        _mm_store_ss(&tf[1], v);
        n += normal.stride;
        t += uv.stride;
    }
    chunk->SetVertexDataDirty(true, 0, -2);
}

// ---------------------------------------------------------------------------------------------------------------
// GenericShaderMaterial

void __cdecl GenericShaderMaterial::CacheChunk(Display* display, ViewPort* view, GeometryChunk* chunk,
                                               const DisplayLightPtrList* lights, const Matrix4* modelView) const
{
    const float product = MaterialOpacity(this) * ChunkOpacity(chunk);
    if (alphaHint != static_cast<TextureAlpha>(1) && 1.0f <= product) {
        MaterialAddFn const fn = OriginalProc<MaterialAddFn>(kMaterialAdd);
        if (fn) fn(chunkCache, display, view, this, chunk, lights, modelView);
        return;
    }
    if (alphaBlendCache) {
        void* const cache = alphaBlendCache;
        void** const vtable = *reinterpret_cast<void***>(cache);
        reinterpret_cast<void(__cdecl*)(void*, Display*, ViewPort*, const Material*, GeometryChunk*,
                                         const DisplayLightPtrList*, const Matrix4*)>(vtable[0x24 / 4])(
            cache, display, view, this, chunk, lights, modelView);
    }
}

void __cdecl GenericShaderMaterial::RenderChunk(Display* display, ViewPort* view, GeometryChunk* chunk,
                                                const DisplayLightPtrList* lights, const Matrix4* modelView,
                                                int pass) const
{
    (void)pass;
    const VertexProgram* const program = ChunkProgram(chunk);
    SetDisplayState(display);
    float opacity;
    const Matrix4* viewMatrix = nullptr;
    if (view) {
        opacity = ChunkOpacity(chunk) * MaterialOpacity(this);
        viewMatrix = ViewMatrix(view);
        SetLightingState(display, viewMatrix, chunk, program, lights, modelView, opacity, false);
    } else {
        opacity = MaterialOpacity(this) * ChunkOpacity(chunk);
        SetLightingState(display, nullptr, chunk, program, lights, modelView, opacity, false);
        viewMatrix = modelView;
    }
    DisplaySetModelView(display, modelView);
    if (chunk) chunk->RenderGeometry(display, view, program, true, 0);
    ClearDisplayState(display);
}

void __cdecl GenericShaderMaterial::RenderNumChunks(Display* display, ViewPort* view, GeometryChunk** chunks,
                                                    const DisplayLightPtrList** lights, const Matrix4** modelViews,
                                                    unsigned int numChunks, int pass) const
{
    (void)pass;
    SetDisplayState(display);
    const Matrix4* const cameraView = ViewMatrix(view);
    for (unsigned int i = 0; i < numChunks; ++i) {
        GeometryChunk* const chunk = chunks[i];
        const VertexProgram* const program = ChunkProgram(chunk);
        const Matrix4* const modelView = modelViews[i];
        float opacity;
        if (view) {
            opacity = ChunkOpacity(chunk) * MaterialOpacity(this);
            SetLightingState(display, cameraView, chunk, program, lights[i], modelView, opacity, false);
        } else {
            opacity = MaterialOpacity(this) * ChunkOpacity(chunk);
            SetLightingState(display, nullptr, chunk, program, lights[i], modelView, opacity, false);
        }
        DisplaySetModelView(display, modelView);
        if (chunk) chunk->RenderGeometry(display, view, program, true, 0);
    }
    ClearDisplayState(display);
}

void __cdecl GenericShaderMaterial::RenderPrimitives(
    Display* display, ViewPort* view, GeometryChunk* chunk, const DisplayLightPtrList* lights,
    const Matrix4* modelView, unsigned int numIndices, const unsigned short* indices, unsigned int numPrimitives,
    DPrimitive primitive, int pass) const
{
    (void)pass;
    const VertexProgram* const program = ChunkProgram(chunk);
    SetDisplayState(display);
    float opacity;
    const Matrix4* viewMatrix = nullptr;
    if (view) {
        opacity = ChunkOpacity(chunk) * MaterialOpacity(this);
        viewMatrix = ViewMatrix(view);
        SetLightingState(display, viewMatrix, chunk, program, lights, modelView, opacity, false);
    } else {
        opacity = MaterialOpacity(this) * ChunkOpacity(chunk);
        SetLightingState(display, nullptr, chunk, program, lights, modelView, opacity, false);
        viewMatrix = modelView;
    }
    DisplaySetModelView(display, modelView);
    if (chunk)
        chunk->RenderPrimitives(display, view, program, numIndices, indices, numPrimitives, primitive, true, 0);
    ClearDisplayState(display);
}

void __cdecl GenericShaderMaterial::InitInstance(GenericShaderMaterial& instance) const
{
    Material::InitInstance(instance);
    ForEachRingItem(textureRing, [&](RingItem& item) {
        AppendRingItem(&instance.textureRing, item.type, item.texture);
    });
}

Material::Result __cdecl GenericShaderMaterial::SetTexture(int type, const ResourceDatabase* db,
                                                           const PString& name, float amount,
                                                           const String& request)
{
    if (static_cast<unsigned int>(type) > 0xf) return Result(2);
    TextureManager* const textures = Field<TextureManager*>(materialManager, 0xc);
    void** const vtable = *reinterpret_cast<void***>(textures);
    Texture* const texture = reinterpret_cast<Texture* (__cdecl*)(TextureManager*, const ResourceDatabase*,
                                                                  const PString*, const void*)>(vtable[3])(
        textures, db, &name, &request);
    RingItem* const item = FindRingItem(textureRing, type);
    if (item) {
        Texture* const old = item->texture;
        item->texture = texture;
        if (old) old->ReleaseReference();
    } else if (texture) {
        AppendRingItem(&textureRing, type, texture);
        texture->ReleaseReference();
    }
    if (!texture) return Result(3);
    texture->SetResourcePriority(X87(resourcePriority));
    if (type == 6) alphaHint = static_cast<TextureAlpha>(1);
    SetTextureDefinition(type, name, X87(amount));
    return Result(0);
}

Material::Result __cdecl GenericShaderMaterial::SetTexture(int type, const AutoReference<Texture>& reference,
                                                           float amount)
{
    if (static_cast<unsigned int>(type) > 0xf) return Result(2);
    Texture* const texture = const_cast<AutoReference<Texture>&>(reference).Get();
    if (!texture) return Result(3);
    RingItem* const item = FindRingItem(textureRing, type);
    if (item) {
        Texture* const old = item->texture;
        item->texture = texture;
        texture->AddReference();
        if (old) old->ReleaseReference();
    } else {
        AppendRingItem(&textureRing, type, texture);
    }
    texture->SetResourcePriority(X87(resourcePriority));
    if (type == 6) alphaHint = static_cast<TextureAlpha>(1);
    SetTextureDefinition(type, Field<PString>(texture, 0x10), X87(amount));
    return Result(0);
}

}  // namespace Jet

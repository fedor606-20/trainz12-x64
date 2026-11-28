// WaterSystem_7c U8b: WaterMaterialFresWave. Behaviour follows the TS12 assembly (addresses per function). The
// heavy rendering paths call the Display's raw vtable slots and the RenderSystem programs exactly where the original
// does; the matrix helpers reproduce the original's inlined row-major 4x4 multiply and transpose.
#include "WaterMaterialFresWave.h"

#include <jet/Bitmap.h>
#include <jet/FrameAnimation.h>
#include <jet/KernelStandard.h>
#include <jet/RenderSystem.h>
#include <jet/VCall.h>
#include <jet/VertexFormats.h>
#include <jet/WaterManagerConfig.h>
#include <jet/WaterTextureAnimation.h>

#include <cmath>
#include <cstring>
#include <new>

#pragma comment(lib, "D:/trainz12-decomp/implib/Render3DCore_7c.lib")

extern "C" void* WsMakeGeometryChunkFlagsImport;
extern "C" void* WsGetViewTransformImport;
extern "C" void* WsMaterialCreateByUidImport;
extern "C" void* WsMaterialSetLightingStateImport;
extern "C" void* WsMaterialSetTextureDefinitionImport;
extern "C" void* WsGetViewModeParamsImport;
extern "C" void* WsSetViewModeParamsImport;
#pragma comment(linker, "/alternatename:_WsMakeGeometryChunkFlagsImport=__imp_?MakeGeometryChunkFlags@Material@Jet@@SAIW4GeometryType@12@PBVGeometryChunkCreateHint@2@@Z")
#pragma comment(linker, "/alternatename:_WsGetViewTransformImport=__imp_?GetViewTransform@Camera@Jet@@QBAABVMatrix4@2@XZ")
#pragma comment(linker, "/alternatename:_WsMaterialCreateByUidImport=__imp_?CreateInstance@Material@Jet@@SAPAV12@ABVUId@2@PAVMaterialManager@2@@Z")
#pragma comment(linker, "/alternatename:_WsMaterialSetLightingStateImport=__imp_?SetLightingState@Material@Jet@@QBAXPAVDisplay@2@PBVMatrix4@2@PBVGeometryChunk@2@PBVVertexProgram@2@PBV?$FastDynamicArray@PAVDisplayLight@Jet@@$0N@U?$Less@PAVDisplayLight@Jet@@PAV12@@2@@2@1M_N@Z")
#pragma comment(linker, "/alternatename:_WsMaterialSetTextureDefinitionImport=__imp_?SetTextureDefinition@Material@Jet@@IAAXHABVPString@2@M@Z")
#pragma comment(linker, "/alternatename:_WsGetViewModeParamsImport=__imp_?GetViewModeParams@Display@Jet@@QBAXAAUViewModeParams@12@@Z")
#pragma comment(linker, "/alternatename:_WsSetViewModeParamsImport=__imp_?SetViewModeParams@Display@Jet@@QAAXABUViewModeParams@12@@Z")

namespace Jet {

namespace {

template <class Fn>
Fn BindImport(void* address)
{
    static_assert(sizeof(Fn) == sizeof(void*), "import pointer");
    Fn fn;
    std::memcpy(&fn, &address, sizeof fn);
    return fn;
}

using MakeGeometryChunkFlagsFn = unsigned int(__cdecl*)(Material::GeometryType, const GeometryChunkCreateHint*);
using GetViewTransformFn = const Matrix4*(__cdecl*)(const Camera*);
using CreateMaterialByUidFn = Material*(__cdecl*)(const UId&, MaterialManager*);

unsigned int MakeGeometryChunkFlags(Material::GeometryType type, const GeometryChunkCreateHint* hint)
{
    return reinterpret_cast<MakeGeometryChunkFlagsFn>(WsMakeGeometryChunkFlagsImport)(type, hint);
}

const Matrix4* GetViewTransform(const Camera* camera)
{
    return reinterpret_cast<GetViewTransformFn>(WsGetViewTransformImport)(camera);
}

Material* CreateMaterialByUid(const UId& uid, MaterialManager* manager)
{
    return reinterpret_cast<CreateMaterialByUidFn>(WsMaterialCreateByUidImport)(uid, manager);
}

// Display's view-mode block is 0x24 bytes (the original's ViewModeParams); the accessors are exported __cdecl
// members, called through their raw addresses.
struct WsViewModeParams {
    unsigned char data[0x24];
};

void GetViewModeParams(Display* display, WsViewModeParams& params)
{
    using Fn = void(__cdecl*)(const Display*, WsViewModeParams&);
    reinterpret_cast<Fn>(WsGetViewModeParamsImport)(display, params);
}

void SetViewModeParams(Display* display, const WsViewModeParams& params)
{
    using Fn = void(__cdecl*)(Display*, const WsViewModeParams&);
    reinterpret_cast<Fn>(WsSetViewModeParamsImport)(display, params);
}

// ---- the original's inlined Matrix4 helpers (FUN_10005180 and FUN_10005550) -------------------------------

// dest = a * b (row-major, the rows of a dotted with the columns of b). b may not alias dest.
void MultiplyMatrix(const Matrix4& a, Matrix4& b)
{
    Matrix4 r;
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            r.m[row * 4 + col] = a.m[row * 4 + 0] * b.m[0 * 4 + col] + a.m[row * 4 + 1] * b.m[1 * 4 + col]
                                 + a.m[row * 4 + 2] * b.m[2 * 4 + col] + a.m[row * 4 + 3] * b.m[3 * 4 + col];
        }
    }
    b = r;
}

void TransposeMatrix(Matrix4& m)
{
    for (int row = 0; row < 4; ++row) {
        for (int col = row + 1; col < 4; ++col) {
            const float t = m.m[row * 4 + col];
            m.m[row * 4 + col] = m.m[col * 4 + row];
            m.m[col * 4 + row] = t;
        }
    }
}

void CopyMatrix(Matrix4& dest, const Matrix4& source)
{
    std::memcpy(dest.m, source.m, sizeof dest.m);
}

// The two texture-stage constructors the factory uses: each static TextureArgs the original builds in its
// initializer functions. Sizes are 8-byte aligned as the original's statics are.
struct WsTextureArgs {
    alignas(8) unsigned char bytes[0x48];
};

WsTextureArgs g_stageArgs[20];

// one Display texture-stage node: {next, prev, ?} as the display builds it (opaque to the material)
using CreateStageFn = void*(__cdecl*)(const void*, int, const TextureArgs*, const TextureArgs*);
using DestroyStageFn = void(__cdecl*)(const void*, void*);

// The original's initializer table (0x1001a970..0x1001b1a0), in address order. Each entry is the TextureArgs
// constructor call: stage, multiplier, mode, function, four TexArg operands.
struct StageArgsDesc {
    int slot;
    int stage, multiplier, mode, fn;
    int minus[4], argMode[4], loc[4];
};

// (four TexArg operands per object; the values come from the original's FUN_1001a970..)
const StageArgsDesc kStageArgs[] = {
    {0, 0, 1, 0, 0, {0, 0, 0, 0}, {3, 3, 3, 0}, {3, 3, 3, 2}},
    {1, 0, 1, 1, 0, {0, 0, 0, 0}, {3, 3, 3, 1}, {3, 3, 3, 2}},
    {2, 1, 1, 0, 7, {0, 0, 0, 0}, {3, 0, 0, 0}, {3, 0, 0, 2}},
    {3, 1, 1, 1, 7, {0, 0, 0, 0}, {3, 1, 1, 1}, {3, 0, 0, 2}},
    {4, 0, 1, 0, 7, {0, 0, 0, 0}, {3, 0, 0, 0}, {3, 0, 1, 2}},
    {5, 0, 1, 1, 0, {0, 0, 0, 0}, {3, 3, 3, 3}, {3, 3, 3, 1}},
    {6, 0, 1, 0, 0, {0, 0, 0, 0}, {3, 3, 3, 3}, {3, 3, 3, 1}},
    {7, 0, 1, 1, 7, {0, 0, 1, 0}, {3, 1, 1, 1}, {3, 0, 0, 2}},
    {8, 1, 1, 0, 7, {0, 0, 0, 0}, {3, 1, 0, 0}, {3, 0, 1, 2}},
    {9, 1, 1, 1, 0, {0, 0, 0, 0}, {3, 3, 3, 3}, {3, 3, 3, 1}},
    {10, 0, 1, 0, 9, {0, 0, 0, 0}, {3, 3, 3, 0}, {3, 3, 3, 2}},
    {11, 0, 1, 1, 0, {0, 0, 0, 0}, {3, 3, 3, 3}, {3, 3, 3, 1}},
    {12, 1, 1, 0, 1, {0, 0, 0, 0}, {3, 3, 3, 0}, {3, 3, 1, 0}},
    {13, 1, 1, 1, 0, {0, 0, 0, 0}, {3, 3, 3, 3}, {3, 3, 3, 1}},
    {14, 0, 1, 0, 0, {0, 0, 0, 0}, {3, 3, 3, 3}, {3, 3, 3, 3}},
    {15, 0, 1, 0, 0, {0, 0, 0, 0}, {3, 3, 3, 3}, {3, 3, 3, 2}},
    {16, 0, 1, 1, 0, {0, 0, 0, 0}, {3, 3, 3, 3}, {3, 3, 3, 1}},
    {17, 1, 1, 0, 1, {0, 0, 0, 0}, {3, 3, 3, 0}, {3, 3, 2, 0}},
    {18, 1, 1, 1, 0, {0, 0, 0, 0}, {3, 3, 3, 3}, {3, 3, 3, 1}},
};

bool g_stageArgsReady;

void BuildStageArgs()
{
    if (g_stageArgsReady) return;
    // The original's statics were built by the module initializer; the imported constructor writes the fields.
    for (const StageArgsDesc& d : kStageArgs) {
        TextureArgs* const args = reinterpret_cast<TextureArgs*>(g_stageArgs[d.slot].bytes);
        TexArg a[4];
        for (int i = 0; i < 4; ++i) {
            a[i].minus = d.minus[i] != 0;
            a[i].pad[0] = a[i].pad[1] = a[i].pad[2] = 0;
            a[i].mode = static_cast<ModeType>(d.argMode[i]);
            a[i].loc = static_cast<Source>(d.loc[i]);
        }
        ::new (args) TextureArgs(d.stage, static_cast<unsigned int>(d.multiplier), static_cast<ModeType>(d.mode),
                                 static_cast<DTexFuncType>(d.fn), a[0], a[1], a[2], a[3]);
    }
    g_stageArgsReady = true;
}

// six vertex program names / three pixel program names the original interns
const PString& NameVsWave() { static const PString s("vsWaterMatWave"); return s; }
const PString& NameVsWaveAnim() { static const PString s("vsWaterMatWaveAnim"); return s; }
const PString& NameVsWaveRefl() { static const PString s("vsWaterMatWaveRefl"); return s; }
const PString& NameVsWaveAnimRefl() { static const PString s("vsWaterMatWaveAnimRefl"); return s; }
const PString& NameVsAnimRefl() { static const PString s("vsWaterMatAnimRefl"); return s; }
const PString& NameVsRefl() { static const PString s("vsWaterMatRefl"); return s; }
const PString& NameEmpty() { static const PString s; return s; }
const PString& NamePsWaveAnim() { static const PString s("psWaterMatWaveAnim"); return s; }
const PString& NamePsWaveRefl() { static const PString s("psWaterMatWaveRefl"); return s; }
const PString& NamePsWaveAnimRefl() { static const PString s("psWaterMatWaveAnimRefl"); return s; }

// the display state offsets the original reads raw (fog mode +0x78, fog color +0xa4, fog density +0x8c)
unsigned char* DisplayState(Display* display)
{
    return *reinterpret_cast<unsigned char**>(reinterpret_cast<unsigned char*>(display) + 0x54);
}

// ---- the water chunk templates ----------------------------------------------------------------------------

struct VertexBlock {
    unsigned int refs;
    unsigned int count;
    unsigned int itemSize;
    unsigned int capacity;
    unsigned char items[1];
};

VertexBlock* MakeBlock(unsigned int items, unsigned int itemSize)
{
    auto* block = static_cast<VertexBlock*>(Alloc::New(items * itemSize + 0x10));
    block->count = 0;
    block->refs = 1;
    block->count += items;
    return block;
}

void ReleaseBlock(VertexBlock*& slot)
{
    if (!slot) return;
    if (--slot->refs == 0) {
        Alloc::Delete(slot);
        slot = nullptr;
    }
}

// the camera a viewport exposes at +0x20 (the original passes its view transform to SetLightingState)
const Matrix4* ViewMatrix(ViewPort* view)
{
    if (!view) return nullptr;
    const Camera* const camera =
        *reinterpret_cast<Camera* const*>(reinterpret_cast<unsigned char*>(view) + 0x20);
    return camera ? GetViewTransform(camera) : nullptr;
}

void* BufferManager(Display* display)
{
    return *reinterpret_cast<void* const*>(reinterpret_cast<unsigned char*>(display) + 0x20c);
}

const unsigned short* IndexData(const void* block)
{
    return reinterpret_cast<const unsigned short*>(reinterpret_cast<std::uintptr_t>(block) + 0x10);
}

unsigned int VertexTypeStride(const VertexType* type)
{
    return **reinterpret_cast<const unsigned int* const*>(reinterpret_cast<const unsigned char*>(type) + 0x10);
}

// PackedVertexPA2A4T's offset table (0x1001dd68): item 1 at 0, item 24 at 12, item 25 at 20, item 15 at 36.
const int kPA2A4TOffsets[kVertexItemTypes] = {
    -1, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 36, -1, -1, -1, -1, -1, -1, -1, -1,
    12, 20, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};

template <class Vertex, class Base>
class GeometryChunkTemplate : public Base {
public:
    GeometryChunkTemplate() : block(nullptr) {}
    GeometryChunkTemplate(unsigned int vertices, unsigned int primitives, unsigned int indices,
                          unsigned int faceNormals, unsigned int flags)
        : Base(vertices, primitives, indices, faceNormals, flags)
    {
        block = MakeBlock(vertices, sizeof(Vertex));
    }
    ~GeometryChunkTemplate() override { ReleaseBlock(block); }

    GeometryChunk* __cdecl CreateInstance() const override
    {
        void* const memory = Alloc::New(0x60);
        if (!memory) return nullptr;
        auto* made = ::new (memory) GeometryChunkTemplate;
        this->InitInstance(*made);
        if (made->block != block) {
            ReleaseBlock(made->block);
            made->block = block;
            if (block) ++block->refs;
        }
        return made;
    }
    GeometryChunk* __cdecl CreateInstance(unsigned int vertices, unsigned int primitives, unsigned int indices,
                                          unsigned int faceNormals) const override
    {
        void* const memory = Alloc::New(0x60);
        if (!memory) return nullptr;
        return ::new (memory) GeometryChunkTemplate(vertices, primitives, indices, faceNormals, this->flags);
    }

    bool __cdecl InitializeBuffers(Display* display, bool upload) override
    {
        const unsigned long f = this->flags;
        const StaticBuffer* const had = this->GetStaticVertexBuffer(0, 0);
        const VertexType* const type = this->AssignVertexType(display);
        if (!(f & 0x10000) && !had) {
            if (!type) return false;
            auto* buffer = static_cast<StaticBuffer*>(VCall<void*>(display, 0x104, this->maxVertices,
                                                                    VertexTypeStride(type)));
            if (!buffer) return false;
            if (upload) {
                unsigned int out = 0;
                if (!VCall<unsigned char>(BufferManager(display), 0x20, type, buffer, this->GetVertexArray(0, 0),
                                          this->numVertices, 1, &out, 0)) {
                    buffer->ReleaseReference();
                    return false;
                }
                this->SetVertexDataDirty(false, 0, 0);
            } else {
                this->SetVertexDataDirty(true, 0, 0);
            }
            this->SetStaticVertexBuffer(buffer, 0, 0);
            buffer->ReleaseReference();
        }
        const StaticBuffer* const hadIndex = this->GetStaticIndexBuffer(0);
        if (!(f & 0x100) && !hadIndex) {
            const unsigned int count = this->indexArray.Count();
            auto* buffer = static_cast<StaticBuffer*>(VCall<void*>(display, 0x108, count, 0));
            if (!buffer) return false;
            if (upload) {
                unsigned int out = 0;
                const void* data = IndexData(this->indexArray.block);
                if (VCall<int>(BufferManager(display), 0x2c, buffer, data, this->numIndices, 1, &out) == -1) {
                    buffer->ReleaseReference();
                    return false;
                }
                this->SetIndexDataDirty(false);
            } else {
                this->SetIndexDataDirty(true);
            }
            this->SetStaticIndexBuffer(buffer, 0);
            buffer->ReleaseReference();
        }
        return true;
    }

    bool __cdecl LoadBuffers(Display* display) override
    {
        const unsigned long f = this->flags;
        void* const manager = BufferManager(display);
        if (!(f & 0x10000)) {
            const StaticBuffer* const buffer = this->GetStaticVertexBuffer(0, 0);
            if (!buffer) return false;
            unsigned int out = 0;
            if (!VCall<unsigned char>(manager, 0x20, this->vertexType, buffer, this->GetVertexArray(0, 0),
                                      this->numVertices, 1, &out, 0))
                return false;
            this->SetVertexDataDirty(false, 0, 0);
        }
        if (!(f & 0x100)) {
            const StaticBuffer* const buffer = this->GetStaticIndexBuffer(0);
            if (!buffer) return false;
            unsigned int out = 0;
            const void* data = IndexData(this->indexArray.block);
            if (VCall<int>(manager, 0x2c, buffer, data, this->numIndices, 1, &out) == -1) {
                buffer->ReleaseReference();
                return false;
            }
            this->SetIndexDataDirty(false);
        }
        return true;
    }

    void __cdecl DuplicateVertexArray(Display* display, int, int) override
    {
        if (!block) return;
        auto* copy = MakeBlock(this->maxVertices, sizeof(Vertex));
        std::memcpy(copy->items, block->items, this->maxVertices * sizeof(Vertex));
        if (block != copy) {
            ReleaseBlock(block);
            block = copy;
            ++copy->refs;
        }
        ReleaseBlock(copy);
        if (this->GetStaticVertexBuffer(0, 0)) {
            this->SetStaticVertexBuffer(nullptr, 0, 0);
            this->InitializeBuffers(display, false);
        }
    }

    void __cdecl DuplicateIndexArray(Display* display) override
    {
        GeometryChunk::DuplicateIndexArray(display);
        if (this->GetStaticIndexBuffer(0)) {
            this->SetStaticIndexBuffer(nullptr, 0);
            this->InitializeBuffers(display, false);
        }
    }

    void __cdecl ResizeVertexArray(Display* display, unsigned int n, int, int) override
    {
        const unsigned int keep = this->maxVertices < n ? this->maxVertices : n;
        auto* copy = MakeBlock(n, sizeof(Vertex));
        if (keep) std::memcpy(copy->items, block->items, keep * sizeof(Vertex));
        if (block != copy) {
            ReleaseBlock(block);
            block = copy;
            ++copy->refs;
        }
        this->maxVertices = n;
        this->numVertices = n;
        if (block && this->GetStaticVertexBuffer(0, 0)) {
            this->SetStaticVertexBuffer(nullptr, 0, 0);
            this->InitializeBuffers(display, false);
        }
        ReleaseBlock(copy);
    }

    void* __cdecl GetVertexArray(int, int) override
    {
        return reinterpret_cast<unsigned char*>(block) + 0x10;
    }
    const void* __cdecl GetVertexArray(int, int) const override
    {
        return reinterpret_cast<const unsigned char*>(block) + 0x10;
    }

    const VertexType* __cdecl AssignVertexType(Display* display) const override
    {
        if (!this->vertexType) {
            const VertexTypeID id = Vertex::GetVertexTypeID();
            this->SetVertexType(static_cast<VertexType*>(VCall<void*>(display, 0x10c, id)), 0);
        }
        return &this->GetVertexType(0);
    }

    VertexTypeID __cdecl GetVertexTypeID(int) const override { return Vertex::GetVertexTypeID(); }
    int __cdecl GetVertexSize(int) const override { return sizeof(Vertex); }
    int __cdecl GetItemOffset(VertexItemType type, int) const override
    {
        return Vertex::GetItemOffset(type);
    }

    template <class P>
    P ItemView(VertexItemType type) const
    {
        P r;
        const int offset = Vertex::GetItemOffset(type);
        if (offset < 0) {
            r.stride = 0;
            r.data = nullptr;
            return r;
        }
        r.stride = sizeof(Vertex);
        r.data = reinterpret_cast<decltype(r.data)>(reinterpret_cast<std::uintptr_t>(block) + 0x10 + offset);
        return r;
    }

#define JET_TEMPLATE_ITEM(Getter, Ptr, ConstPtr)                                          \
    GeometryChunk::Ptr __cdecl Getter(VertexItemType type, int) override                  \
    {                                                                                     \
        return ItemView<GeometryChunk::Ptr>(type);                                        \
    }                                                                                     \
    GeometryChunk::ConstPtr __cdecl Getter(VertexItemType type, int) const override       \
    {                                                                                     \
        return ItemView<GeometryChunk::ConstPtr>(type);                                   \
    }
    JET_TEMPLATE_ITEM(GetVector2Array, Vector2Ptr, ConstVector2Ptr)
    JET_TEMPLATE_ITEM(GetVector3Array, Vector3Ptr, ConstVector3Ptr)
    JET_TEMPLATE_ITEM(GetVector4Array, Vector4Ptr, ConstVector4Ptr)
    JET_TEMPLATE_ITEM(GetFloatArray, FloatPtr, ConstFloatPtr)
    JET_TEMPLATE_ITEM(GetUDwordArray, UDwordPtr, ConstUDwordPtr)
#undef JET_TEMPLATE_ITEM

    VertexBlock* block;  // +0x5c
};

// The local Page 4 record: position 0, attrib2 12, attrib4 20, texcoord 36, size 0x2c, vertex type 99.
struct PackedVertexPA2A4T {
    float v[11];
    static int GetItemOffset(VertexItemType type) { return kPA2A4TOffsets[type]; }
    static VertexTypeID GetVertexTypeID() { return static_cast<VertexTypeID>(99); }
};
static_assert(sizeof(PackedVertexPA2A4T) == 0x2c);

using ChunkPNTT = GeometryChunkTemplate<PackedVertexPNTT, GeometryChunkSingleStream>;
using ChunkPNT = GeometryChunkTemplate<PackedVertexPNT, GeometryChunkSingleStream>;
using ChunkPA2A4T = GeometryChunkTemplate<PackedVertexPA2A4T, GeometryChunkSingleStream>;

}  // namespace

const Matrix4* GetWaterViewTransform(const Camera* camera)
{
    return GetViewTransform(camera);
}

void SetWaterMaterialLightingState(const Material* material, Display* display, const Matrix4* modelView,
                                   const GeometryChunk* chunk, const VertexProgram* program,
                                   const DisplayLightPtrList* lights, const Matrix4* view, float opacity,
                                   bool twoSided)
{
    using Fn = void(__cdecl*)(const Material*, Display*, const Matrix4*, const GeometryChunk*,
                              const VertexProgram*, const DisplayLightPtrList*, const Matrix4*, float, bool);
    reinterpret_cast<Fn>(WsMaterialSetLightingStateImport)(material, display, modelView, chunk, program, lights,
                                                          view, opacity, twoSided);
}

void SetWaterMaterialTextureDefinition(const Material* material, int type, const PString& name, float weight)
{
    using Fn = void(__cdecl*)(const Material*, int, const PString&, float);
    reinterpret_cast<Fn>(WsMaterialSetTextureDefinitionImport)(material, type, name, weight);
}

// ---------------------------------------------------------------------------------------------------------------
// statics

const UId WaterMaterialFresWave::uid("4F7EA82E-1CB4-47b4-B1C2-2FCEA5A7D7B9");
const PString WaterMaterialFresWave::UNDECORATED_TYPE("WaterMaterialFresWave");
const PString WaterMaterialFresWave::DECORATED_TYPE("m.WaterMaterialFresWave");
const PString WaterMaterialFresWave::vertexName("WaterPA2A4T");
const PString WaterMaterialFresWave::vertexNamePTT("WaterPTT");

// 0x1000d5c0
WaterMaterialFresWave::WaterMaterialFresWave(MaterialManager* manager, void** stages)
    : WaterMaterial(manager)
    , vertexProgramWave(nullptr)
    , vertexProgramWaveAnim(nullptr)
    , vertexProgramWaveRefl(nullptr)
    , vertexProgramWaveAnimRefl(nullptr)
    , vertexProgramAnimRefl(nullptr)
    , vertexProgramRefl(nullptr)
    , currentVertexProgram(nullptr)
    , currentCombiner(nullptr)
    , currentReflectionProgram(nullptr)
    , textureStages(stages)
    , detailTexture(nullptr)
    , reflectionTexture(nullptr)
    , reflectionCamera(nullptr)
    , detailTarget(nullptr)
    , waterColorTarget(nullptr)
    , lightAnimTarget(nullptr)
    , vertexTypePA2A4T(nullptr)
    , vertexTypePTT(nullptr)
    , textureAnimation(nullptr)
    , frameAnimation(nullptr)
    , lastFrame(-1)
    , flags(0)
    , time(0.0f)
    , gridSpacing(1.0f)
    , refractiveTerm(0.02f)
    , uvOffsetX(0.0f)
    , uvOffsetY(0.0f)
    , uvVelocityX(0.0f)
    , uvVelocityY(0.0f)
    , reflectionType(-1)
    , scaleNormXY(1.0f)
    , reflectionAmount(0.0f)
    , waterColorDirty(true)
    , lightAnimFrame(0)
{
    std::memset(vertexProgramConstants, 0, sizeof vertexProgramConstants);
    std::memset(reflectionProgramConstants, 0, sizeof reflectionProgramConstants);
    combiners[0] = combiners[1] = combiners[2] = nullptr;
    flags = 0x27;  // bits 0,1,2,5 as the ctor writes (0xe7 & 0x27 after the object's prior bytes)
    lightAnimColor = Color{0.0f, 0.0f, 0.0f, 1.0f};
    lightAnimColor2 = Color{0.0f, 0.0f, 0.0f, 1.0f};
    lightAnimDirection = Vector3(0.0f, 0.0f, 0.0f);
}

// 0x1000d7d0
WaterMaterialFresWave::~WaterMaterialFresWave()
{
    VertexProgram* const programs[6] = {vertexProgramWave, vertexProgramWaveAnim, vertexProgramWaveRefl,
                                        vertexProgramWaveAnimRefl, vertexProgramAnimRefl, vertexProgramRefl};
    for (int i = 0; i < 6; ++i)
        if (programs[i]) programs[i]->ReleaseReference();
    for (TextureCombiner* const combiner : combiners)
        if (combiner) combiner->ReleaseReference();
    if (vertexTypePA2A4T) vertexTypePA2A4T->ReleaseReference();
    if (vertexTypePTT) vertexTypePTT->ReleaseReference();
    if (detailTarget) delete detailTarget;
    if (waterColorTarget) delete waterColorTarget;
    if (lightAnimTarget) delete lightAnimTarget;
    if (reflectionTexture) reflectionTexture->ReleaseReference();
    if (detailTexture) detailTexture->ReleaseReference();
}

// 0x1000d990
Material* __cdecl WaterMaterialFresWave::CreateInstance() const
{
    Material* const material = CreateMaterialByUid(uid, materialManager);
    if (material) InitInstance(*static_cast<WaterMaterialFresWave*>(material));
    return material;
}

// 0x1000d3c0
bool __cdecl WaterMaterialFresWave::operator==(const Material& other) const
{
    if (!dynamic_cast<const WaterMaterialFresWave*>(&other)) return false;
    return Equals(static_cast<const WaterMaterialFresWave&>(other));
}

// 0x10014100 (identical-code fold with the other concrete materials: the body just forwards to Material::Equals)
bool __cdecl WaterMaterialFresWave::Equals(const WaterMaterialFresWave& other) const
{
    return Material::Equals(other);
}

// 0x1000d3f0
void __cdecl WaterMaterialFresWave::SetResourcePriority(float priority) const
{
    const_cast<WaterMaterialFresWave*>(this)->resourcePriority = priority;
    if (detailTexture) detailTexture->SetResourcePriority(priority);
    if (reflectionTexture) reflectionTexture->SetResourcePriority(priority);
}

// 0x1000d440
const PString& __cdecl WaterMaterialFresWave::GetType() const { return UNDECORATED_TYPE; }

// 0x1000eb50
void __cdecl WaterMaterialFresWave::Activate(int)
{
    if (detailTexture) detailTexture->Activate(1);
    if (reflectionTexture) reflectionTexture->Activate(1);
}

// 0x1000eaa0
WaterMaterialFresWave::Result __cdecl WaterMaterialFresWave::DuplicateTexture(int type)
{
    if (type == 9) {
        if (!detailTexture) return Result(3);
        Texture* const copy = detailTexture->CreateInstance();
        detailTexture->ReleaseReference();
        detailTexture = copy;
        return Result(0);
    }
    if (type == 6) {
        if (!reflectionTexture) return Result(3);
        Texture* const copy = reflectionTexture->CreateInstance();
        reflectionTexture->ReleaseReference();
        reflectionTexture = copy;
        return Result(0);
    }
    return Result(2);
}

// 0x1000ef50 (both the const and non-const export)
Texture* __cdecl WaterMaterialFresWave::GetTexture(int type)
{
    if (type == 9) return detailTexture;
    if (type == 6) return reflectionTexture;
    return nullptr;
}
const Texture* __cdecl WaterMaterialFresWave::GetTexture(int type) const
{
    if (type == 9) return detailTexture;
    if (type == 6) return reflectionTexture;
    return nullptr;
}

// 0x1000ee10
WaterMaterialFresWave::Result __cdecl WaterMaterialFresWave::SetTexture(int type,
                                                                        const AutoReference<Texture>& texture,
                                                                        float amount)
{
    if (type == 9) {
        if (!texture.Get()) return Result(3);
        Texture* const old = detailTexture;
        detailTexture = texture.Get();
        if (detailTexture) detailTexture->AddReference();
        if (old) old->ReleaseReference();
        detailTexture->SetResourcePriority(resourcePriority);
        SetWaterMaterialTextureDefinition(this, type, detailTexture->GetResourceName(), 0.0f);
        return Result(0);
    }
    if (type == 6) {
        if (!texture.Get()) return Result(3);
        Texture* const old = reflectionTexture;
        reflectionTexture = texture.Get();
        if (reflectionTexture) reflectionTexture->AddReference();
        if (old) old->ReleaseReference();
        reflectionTexture->SetResourcePriority(resourcePriority);
        SetWaterMaterialTextureDefinition(this, type, reflectionTexture->GetResourceName(), amount);
        waterColorDirty = true;
        reflectionAmount = amount;
        return Result(0);
    }
    if (type == 1) return Result(0);
    return Result(2);
}

// 0x1000ecd0
WaterMaterialFresWave::Result __cdecl WaterMaterialFresWave::SetTexture(int type, const ResourceDatabase* database,
                                                                        const PString& name, float amount,
                                                                        const String& request)
{
    TextureManager* const manager = *reinterpret_cast<TextureManager**>(
        reinterpret_cast<unsigned char*>(materialManager) + 0xc);
    if (!manager) return Result(3);
    if (type == 9) {
        Texture* const texture = manager->Create(database, name, request);
        if (detailTexture) detailTexture->ReleaseReference();
        detailTexture = texture;
        if (!texture) return Result(3);
        texture->SetResourcePriority(resourcePriority);
        SetWaterMaterialTextureDefinition(this, type, name, amount);
        return Result(0);
    }
    if (type == 6) {
        if (amount == 0.0f) return Result(3);
        Texture* const texture = manager->Create(database, name, request);
        if (reflectionTexture) reflectionTexture->ReleaseReference();
        reflectionTexture = texture;
        if (!texture) return Result(3);
        texture->SetResourcePriority(resourcePriority);
        SetWaterMaterialTextureDefinition(this, type, name, amount);
        waterColorDirty = true;
        reflectionAmount = amount;
        return Result(0);
    }
    if (type == 1) return Result(0);
    return Result(2);
}

// 0x1000d450
WaterMaterialFresWave::Result __cdecl WaterMaterialFresWave::IsLoaded() const
{
    if (detailTexture && !detailTexture->IsAvailable()) return Result(4);
    if (reflectionTexture && !reflectionTexture->IsAvailable()) return Result(4);
    return Result(0);
}

// 0x1000d4a0
void WaterMaterialFresWave::SetRefractiveIndex(float value)
{
    refractiveTerm = static_cast<float>(((1.0 - value) * (1.0 - value)) / ((value + 1.0) * (value + 1.0)));
}

// 0x1000d4e0
void WaterMaterialFresWave::SetWaterColor(const Color& color)
{
    waterColor = color;
    waterColorDirty = true;
    opacity = waterColor.a;
}

// 0x1000d520
void WaterMaterialFresWave::EnableDetailAnimInterpolation(bool value)
{
    if (value && textureStages && (flags & 0x10))
        flags = static_cast<unsigned char>((flags ^ ((flags ^ 1) & 1)));
    else
        flags = static_cast<unsigned char>(flags & ~1);
}

// 0x1000d570
void WaterMaterialFresWave::SetUVScrollVelocity(Vector2 velocity)
{
    uvVelocityX = velocity.x;
    uvVelocityY = velocity.y;
}

// 0x1000d590
void __cdecl WaterMaterialFresWave::SetReflectionCamera(WaterReflectionCamera* camera) { reflectionCamera = camera; }

// 0x1000d290
int WaterMaterialFresWave::GetReflectionType() const { return reflectionType; }

// 0x1000d2a0
bool WaterMaterialFresWave::GetDetailAnim() const { return (flags >> 5) & 1; }

// 0x1000d2b0: the original returns the velocity pair, not the accumulated offsets
Vector2 WaterMaterialFresWave::GetUVScrollVelocity() const { return Vector2(uvVelocityX, uvVelocityY); }

// 0x1000d2d0
void WaterMaterialFresWave::SetScaleNormXY(float value) { scaleNormXY = value; }

// 0x1000d2f0
float WaterMaterialFresWave::GetScaleNormXY() const { return scaleNormXY; }

// 0x1000d300
void WaterMaterialFresWave::SetGridSpacing(float value) { gridSpacing = value; }

// 0x10011930
void WaterMaterialFresWave::Update(float delta)
{
    time += delta * 3.0f;
    const float x = uvOffsetX + uvVelocityX * delta;
    uvOffsetY += uvVelocityY * delta;
    uvOffsetX = x;
    if (uvOffsetX > 1.0f) uvOffsetX = x - 1.0f;
    if (uvOffsetY > 1.0f) uvOffsetY -= 1.0f;
}

// 0x1000e6c0
bool WaterMaterialFresWave::SetReflectionType(int type)
{
    if (reflectionType != type) {
        if (!(flags & 4)) {
            if (reflectionType != 0) waterColorDirty = true;
            reflectionType = 0;
            return type == 0;
        }
        if (type == 0) {
            waterColorDirty = true;
        } else if (type != 1) {
            if (type != 2) {
                if (type > 2) reflectionType = 2;
                InitVertexProgramms();
                return false;
            }
            if (!reflectionCamera) {
                InitVertexProgramms();
                return false;
            }
        }
        reflectionType = type;
        InitVertexProgramms();
    }
    return true;
}

// 0x1000e770
void WaterMaterialFresWave::SetDetailAnim(bool value)
{
    const unsigned char bit = value ? static_cast<unsigned char>((flags >> 4) & 1) : 0;
    flags = static_cast<unsigned char>((flags ^ ((bit << 5) ^ flags) & 0x20) & 0xff);
    InitVertexProgramms();
}

// 0x1000e7b0
void WaterMaterialFresWave::InitVertexProgramms()
{
    if (reflectionType == 0) {
        currentVertexProgram = (flags & 0x20) ? vertexProgramWaveAnim : vertexProgramWave;
        currentReflectionProgram = nullptr;
        currentCombiner = combiners[0];
    } else if (reflectionType == 1 || reflectionType == 2) {
        const bool detail = (flags & 0x20) != 0;
        currentVertexProgram = detail ? vertexProgramWaveAnimRefl : vertexProgramWaveRefl;
        currentReflectionProgram = detail ? vertexProgramAnimRefl : vertexProgramRefl;
        currentCombiner = detail ? combiners[2] : combiners[1];
    } else {
        return;
    }

    if (flags & 4) {
        auto constants = [this](VertexProgram* program, int* out) {
            if (!program) return;
            out[0] = program->GetLocalConstantNumber(PString("lightDir"));
            out[1] = program->GetLocalConstantNumber(PString("time"));
            out[2] = program->GetLocalConstantNumber(PString("tanu"));
            out[3] = program->GetLocalConstantNumber(PString("tanv"));
            out[4] = program->GetLocalConstantNumber(PString("scroll"));
            out[5] = program->GetLocalConstantNumber(PString("scale"));
            out[6] = program->GetLocalConstantNumber(PString("localToCube"));
            out[7] = program->GetLocalConstantNumber(PString("eyeToCube"));
            out[8] = program->GetLocalConstantNumber(PString("locCamPos"));
            out[9] = program->GetLocalConstantNumber(PString("localCamPos"));
            out[10] = program->GetLocalConstantNumber(PString("fresTerm"));
            out[11] = program->GetLocalConstantNumber(PString("lightColor"));
            out[12] = program->GetLocalConstantNumber(PString("ambColor"));
            out[13] = program->GetLocalConstantNumber(PString("waterColor"));
            out[14] = program->GetLocalConstantNumber(PString("refractVec"));
        };
        constants(currentVertexProgram, vertexProgramConstants);
        constants(currentReflectionProgram, reflectionProgramConstants);
    }
}

// 0x1000d9c0
void __cdecl WaterMaterialFresWave::InitInstance(WaterMaterialFresWave& instance) const
{
    WaterMaterial::InitInstance(instance);
    instance.detailTexture = detailTexture;
    if (detailTexture) detailTexture->AddReference();
    if (instance.detailTexture && instance.detailTexture != detailTexture) {
        // (the destination's old pointer is released by the base copy's state; the original releases it here)
    }
    instance.reflectionTexture = reflectionTexture;
    if (reflectionTexture) reflectionTexture->AddReference();
    instance.reflectionCamera = reflectionCamera;
    instance.vertexProgramWave = vertexProgramWave;
    instance.vertexProgramWaveAnim = vertexProgramWaveAnim;
    instance.vertexProgramWaveRefl = vertexProgramWaveRefl;
    instance.vertexProgramWaveAnimRefl = vertexProgramWaveAnimRefl;
    instance.vertexProgramAnimRefl = vertexProgramAnimRefl;
    instance.vertexProgramRefl = vertexProgramRefl;
    for (int i = 0; i < 6; ++i) {
        VertexProgram* const program = (&vertexProgramWave)[i];
        if (program) program->AddReference();
    }
    instance.detailTarget = detailTarget ? VCall<DisplayTexture*>(detailTarget, 0x28) : nullptr;
    instance.waterColorTarget = waterColorTarget ? VCall<DisplayTexture*>(waterColorTarget, 0x28) : nullptr;
    instance.lightAnimTarget = lightAnimTarget ? VCall<DisplayTexture*>(lightAnimTarget, 0x28) : nullptr;
    instance.currentVertexProgram = currentVertexProgram;
    instance.currentCombiner = currentCombiner;
    instance.currentReflectionProgram = currentReflectionProgram;
    for (int i = 0; i < 3; ++i) instance.combiners[i] = combiners[i];
    std::memcpy(instance.vertexProgramConstants, vertexProgramConstants, sizeof vertexProgramConstants);
    std::memcpy(instance.reflectionProgramConstants, reflectionProgramConstants,
                sizeof reflectionProgramConstants);
    instance.textureAnimation = textureAnimation;
    instance.frameAnimation = frameAnimation;
    instance.textureStages = textureStages;
    instance.refractiveTerm = refractiveTerm;
    instance.waterColor = waterColor;
    instance.lightColor = lightColor;
    instance.ambientColor = ambientColor;
    instance.lightDir = lightDir;
    instance.time = time;
    instance.gridSpacing = gridSpacing;
    {
        const unsigned char mask = 1 | 2 | 4 | 8 | 0x10 | 0x20;
        instance.flags = static_cast<unsigned char>((instance.flags & ~mask) | (flags & mask));
    }
    instance.uvOffsetX = uvOffsetX;
    instance.uvOffsetY = uvOffsetY;
    instance.uvVelocityX = uvVelocityX;
    instance.uvVelocityY = uvVelocityY;
    instance.reflectionType = reflectionType;
    instance.scaleNormXY = scaleNormXY;
    instance.reflectionAmount = reflectionAmount;
    instance.waterColorDirty = waterColorDirty;
    instance.lightAnimColor = lightAnimColor;
    instance.lightAnimColor2 = lightAnimColor2;
    instance.lightAnimDirection = lightAnimDirection;
    instance.lightAnimFrame = lightAnimFrame;
    instance.InitVertexProgramms();
}

// ---------------------------------------------------------------------------------------------------------------
// creation

// 0x10013800 / 0x1000eb90: the material's chunk, a GeometryChunkTemplate over the water vertex record.
GeometryChunk* CreateWaterGeometryChunk(bool waveVertexProgram, unsigned int numVertices,
                                        unsigned int numPrimitives, unsigned int numIndices,
                                        unsigned int numFaceNormals, Material::GeometryType type,
                                        GeometryChunkCreateHint* hint)
{
    const unsigned int flags = MakeGeometryChunkFlags(type, hint);
    void* const memory = Alloc::New(0x60);
    if (!memory) return nullptr;
    if (waveVertexProgram) {
        auto* chunk = ::new (memory) ChunkPA2A4T(numVertices, numPrimitives, numIndices, numFaceNormals, flags);
        if (hint) {
            if (*reinterpret_cast<const int*>(hint) == 1)
                VCall<void>(chunk, 100, 0, 0xfffffffe, 1);
            if (*reinterpret_cast<const int*>(reinterpret_cast<const unsigned char*>(hint) + 4) == 1)
                VCall<void>(chunk, 0x74);
        }
        return chunk;
    }
    auto* chunk = ::new (memory) ChunkPNTT(numVertices, numPrimitives, numIndices, numFaceNormals, flags);
    if (hint) {
        if (*reinterpret_cast<const int*>(hint) == 1)
            VCall<void>(chunk, 100, 0, 0xfffffffe, 1);
        if (*reinterpret_cast<const int*>(reinterpret_cast<const unsigned char*>(hint) + 4) == 1)
            VCall<void>(chunk, 0x74);
    }
    return chunk;
}

// OneTex's chunk: GeometryChunkTemplate<PackedVertexPNT, GeometryChunkSingleStream> (0x100138c0).
GeometryChunk* CreateOneTexGeometryChunk(unsigned int numVertices, unsigned int numPrimitives,
                                         unsigned int numIndices, unsigned int numFaceNormals,
                                         Material::GeometryType type, GeometryChunkCreateHint* hint)
{
    const unsigned int flags = MakeGeometryChunkFlags(type, hint);
    void* const memory = Alloc::New(0x60);
    if (!memory) return nullptr;
    auto* chunk = ::new (memory) ChunkPNT(numVertices, numPrimitives, numIndices, numFaceNormals, flags);
    if (hint) {
        if (*reinterpret_cast<const int*>(hint) == 1)
            VCall<void>(chunk, 100, 0, 0xfffffffe, 1);
        if (*reinterpret_cast<const int*>(reinterpret_cast<const unsigned char*>(hint) + 4) == 1)
            VCall<void>(chunk, 0x74);
    }
    return chunk;
}

// The factory's texture-stage nodes: Display slot 0x11c builds one from two static TextureArgs.
void* CreateWaterStageNode(Display* display, int flag, int firstArg, int secondArg)
{
    BuildStageArgs();
    const TextureArgs* const first = reinterpret_cast<const TextureArgs*>(g_stageArgs[firstArg].bytes);
    const TextureArgs* const second = reinterpret_cast<const TextureArgs*>(g_stageArgs[secondArg].bytes);
    return VCall<void*>(display, 0x11c, flag, first, second);
}

// 0x100036c0: the inherited six-argument wrapper forwards to the seven-argument virtual
GeometryChunk* __cdecl WaterMaterialFresWave::CreateGeometryChunk(unsigned int numVertices,
                                                                  unsigned int numPrimitives,
                                                                  unsigned int numIndices,
                                                                  unsigned int numFaceNormals,
                                                                  GeometryType type,
                                                                  GeometryChunkCreateHint* hint) const
{
    return CreateGeometryChunk(numVertices, numPrimitives, numIndices, numFaceNormals, type, hint, false);
}

// 0x1000eb90 inner: the wave (vertex-program) form
GeometryChunk* __cdecl WaterMaterialFresWave::CreateGeometryChunk(unsigned int numVertices,
                                                                  unsigned int numPrimitives,
                                                                  unsigned int numIndices,
                                                                  unsigned int numFaceNormals,
                                                                  GeometryType type,
                                                                  GeometryChunkCreateHint* hint,
                                                                  bool waveVertexProgram) const
{
    if (waveVertexProgram && (flags & 4)) {
        if (!vertexTypePA2A4T) const_cast<WaterMaterialFresWave*>(this)->vertexTypePA2A4T = VertexType::GetVertexType(vertexName);
    }
    return CreateWaterGeometryChunk(waveVertexProgram && (flags & 4), numVertices, numPrimitives, numIndices,
                                    numFaceNormals, type, hint);
}

// ---------------------------------------------------------------------------------------------------------------
// rendering entry points

// 0x100134f0
void __cdecl WaterMaterialFresWave::CacheChunk(Display* display, ViewPort* view, GeometryChunk* chunk,
                                               const DisplayLightPtrList* lights,
                                               const Matrix4* modelView) const
{
    if (renderCache) VCall<void>(renderCache, 0x24, display, view, this, chunk, lights, modelView);
}

// 0x1000ef80
void __cdecl WaterMaterialFresWave::RenderChunk(Display* display, ViewPort* view, GeometryChunk* chunk,
                                                const DisplayLightPtrList* lights, const Matrix4* modelView,
                                                int pass) const
{
    RenderNumChunks(display, view, &chunk, &lights, &modelView, 1, pass);
}

// 0x100091d0 shared body
void __cdecl WaterMaterialFresWave::Bind(Display*, ViewPort*, unsigned int) {}
void __cdecl WaterMaterialFresWave::Unbind(Display*, ViewPort*, unsigned int) {}

// 0x1000efb0
void __cdecl WaterMaterialFresWave::RenderNumChunks(Display* display, ViewPort* view, GeometryChunk** chunks,
                                                    const DisplayLightPtrList** lights,
                                                    const Matrix4** modelViews, unsigned int numChunks,
                                                    int pass) const
{
    const bool reflectDetail = reflectionType != 0 && detailTexture != nullptr;
    const int frame = *reinterpret_cast<int*>(reinterpret_cast<unsigned char*>(display) + 0x3c);
    if (frame != lastFrame) {
        const_cast<WaterMaterialFresWave*>(this)->lastFrame = frame;
        if ((flags & 0x20) && textureAnimation && (flags & 1) && detailTarget)
            CalculateDetailTexture(display);
        if (!reflectDetail && reflectionTexture && lightAnimTarget) {
            if (waterColorDirty || lightAnimTarget->GetDataLost()) CalculateWaterColorTexture(display);
        }
    }

    VCall<void>(display, 0xac, 1);  // SetTextureMode(1)
    if (!reflectDetail) {
        for (unsigned int i = 0; i < numChunks; ++i) {
            GeometryChunk* const chunk = chunks[i];
            if (!chunk) continue;
            const VertexProgram* const program = chunk->GetVertexProgram(0);
            const Matrix4* const viewMatrix = ViewMatrix(view);
            SetWaterMaterialLightingState(this, display, modelViews[i], chunk, program, lights[i], viewMatrix, opacity, (flags & 1) != 0);
            VCall<void>(display, 0x54, program);
            chunk->RenderGeometry(display, view, program, true, 0);
        }
    } else {
        for (unsigned int i = 0; i < numChunks; ++i) {
            GeometryChunk* const chunk = chunks[i];
            if (!chunk) continue;
            VertexProgram* const program = currentVertexProgram;
            const Matrix4* const viewMatrix = ViewMatrix(view);
            SetWaterMaterialLightingState(this, display, modelViews[i], chunk, program, lights[i], viewMatrix, opacity, (flags & 1) != 0);
            VCall<void>(display, 0x54, program);
            chunk->RenderGeometry(display, view, program, true, 0);
        }
    }
}

// 0x10013720
void __cdecl WaterMaterialFresWave::RenderPrimitives(Display* display, ViewPort* view, GeometryChunk* chunk,
                                                     const DisplayLightPtrList* lights,
                                                     const Matrix4* modelView, unsigned int numIndices,
                                                     const unsigned short* indices, unsigned int numPrimitives,
                                                     DPrimitive primitive, int pass) const
{
    const VertexProgram* const program = chunk ? chunk->GetVertexProgram(0) : nullptr;
    SetWaterMaterialLightingState(this, display, modelView, chunk, program, lights, ViewMatrix(view), opacity, (flags & 1) != 0);
    VCall<void>(display, 0x54, program);
    chunk->RenderPrimitives(display, view, program, numIndices, indices, numPrimitives, primitive, true, 0);
}

// ---------------------------------------------------------------------------------------------------------------
// textures and programs

// 0x1000dec0
bool WaterMaterialFresWave::Initialize(const ResourceDatabase* database, const WaterManagerConfig* config,
                                               FrameAnimation* animation, WaterTextureAnimation* textures,
                                               int detail)
{
    RenderKernel* const kernel = *reinterpret_cast<RenderKernel**>(
        reinterpret_cast<unsigned char*>(materialManager) + 8);
    Display* const display = *reinterpret_cast<Display**>(reinterpret_cast<unsigned char*>(kernel) + 0x7c);
    textureAnimation = textures;
    frameAnimation = animation;

    SetWaterColor(config->GetMaterialProperties().materialColor);
    ambientColor = config->GetMaterialProperties().ambientColor;
    lightColor = config->GetMaterialProperties().lightColor;
    lightDir = config->GetMaterialProperties().lightDir;
    SetRefractiveIndex(config->GetMaterialProperties().materialRI);
    lastFrame = -1;
    flags = static_cast<unsigned char>((flags ^ ((config->GetMaterialProperties().fres != 0 ? 2 : 0) ^ flags) & 2));

    const bool hasCapability27 = VCall<bool>(display, 0x1c, 0x1b);
    const bool detailStage = hasCapability27 && textureStages && textureStages[9];
    flags = static_cast<unsigned char>((flags ^ ((detailStage ? 1 : 0) << 3 ^ flags) & 8));

    bool animStages = animation && textures && *reinterpret_cast<float*>(reinterpret_cast<unsigned char*>(textures) + 8) > 0.0f
                      && VCall<bool>(display, 0x1c, 0x1c) && textureStages && textureStages[2]
                      && textureStages[6];
    flags = static_cast<unsigned char>((flags ^ ((animStages ? 1 : 0) << 4 ^ flags) & 0x10));

    const bool detailTextureStage = textureStages && textureStages[0];
    const unsigned char withoutBit0 = static_cast<unsigned char>(flags & ~1);
    flags = static_cast<unsigned char>(withoutBit0 | (detailTextureStage ? 1 : 0));
    flags = static_cast<unsigned char>((flags | (detailTextureStage ? 0x20 : 0)) & 0xff);
    if (!( (flags & 0x20) && (flags & 0x10) )) flags = static_cast<unsigned char>(flags & ~0x20);

    if (!VertexProgram::SupportedByPlatform(display, 1.0f) || !TextureCombiner::SupportedByPlatform(display, 1.0f))
        flags = static_cast<unsigned char>(flags & ~4);

    if (flags & 4) {
        VertexProgramManager* const programManager = static_cast<VertexProgramManager*>(VCall<void*>(display, 0x140));
        if (!programManager) return false;
        const DFogMode savedFog = *reinterpret_cast<DFogMode*>(DisplayState(display) + 0x78);
        display->SetFogMode(static_cast<DFogMode>(3));
        struct Programs {
            VertexProgram*& slot;
            const PString& name;
            VertexTypeID type;
        } programs[] = {{vertexProgramWave, NameVsWave(), static_cast<VertexTypeID>(0)},
                        {vertexProgramWaveAnim, NameVsWaveAnim(), static_cast<VertexTypeID>(99)},
                        {vertexProgramWaveRefl, NameVsWaveRefl(), static_cast<VertexTypeID>(99)},
                        {vertexProgramWaveAnimRefl, NameVsWaveAnimRefl(), static_cast<VertexTypeID>(99)},
                        {vertexProgramAnimRefl, NameVsAnimRefl(), static_cast<VertexTypeID>(99)},
                        {vertexProgramRefl, NameVsRefl(), static_cast<VertexTypeID>(99)}};
        for (const Programs& item : programs) {
            VertexProgram* program = nullptr;
            programManager->Create(kernel, program, item.type, true, item.name, NameEmpty(), nullptr, nullptr);
            item.slot = program;
            if (!program) flags = static_cast<unsigned char>(flags & ~4);
        }
        TextureCombinerManager* const combinerManager =
            static_cast<TextureCombinerManager*>(VCall<void*>(display, 0x138));
        if (combinerManager) {
            combinerManager->Create(kernel, combiners[0], NamePsWaveAnim(), nullptr);
            if (!combiners[0]) flags = static_cast<unsigned char>(flags & ~4);
            combinerManager->Create(kernel, combiners[1], NamePsWaveRefl(), nullptr);
            if (!combiners[1]) flags = static_cast<unsigned char>(flags & ~4);
            combinerManager->Create(kernel, combiners[2], NamePsWaveAnimRefl(), nullptr);
            if (!combiners[2]) flags = static_cast<unsigned char>(flags & ~4);
        }
        display->SetFogMode(savedFog);
    }

    unsigned char* const pixels = static_cast<unsigned char*>(Alloc::New(0x10000));
    const Size targetSize{0x80, 0x80};
    if ((flags & 1) && (flags & 0x10)) {
        DisplayFormat format(static_cast<DFormatId>(1));
        DisplayTexture* const texture = VCall<DisplayTexture*>(display, 0x98);
        detailTarget = texture;
        RenderTextureConstruct construct(static_cast<TextureTarget>(3), false, true, false, nullptr);
        if (!texture->Create(targetSize, format, static_cast<TextureDimension>(0), 1,
                             static_cast<TextureAutoMip>(0), 0, 1.0f, static_cast<TextureHint>(1),
                             static_cast<TextureFilter>(1), static_cast<TextureFilter>(1),
                             static_cast<TextureFilter>(1), static_cast<TextureWrap>(0),
                             static_cast<TextureWrap>(2), construct)) {
            Alloc::Delete(pixels);
            return false;
        }
        Bitmap bitmap(targetSize, static_cast<Bitmap::Format>(2), pixels);
        texture->Load(0, format, bitmap, 0);
        (void)database;
    }

    DisplayFormat colorFormat(static_cast<DFormatId>(5));
    if (flags & 0x10) {
        DisplayTexture* const texture = VCall<DisplayTexture*>(display, 0x98);
        waterColorTarget = texture;
        RenderTextureConstruct construct(static_cast<TextureTarget>(1), false, true, false, nullptr);
        if (!texture->Create(targetSize, colorFormat, static_cast<TextureDimension>(0), 1,
                             static_cast<TextureAutoMip>(0), 0, 1.0f, static_cast<TextureHint>(1),
                             static_cast<TextureFilter>(1), static_cast<TextureFilter>(1),
                             static_cast<TextureFilter>(1), static_cast<TextureWrap>(0),
                             static_cast<TextureWrap>(2), construct)) {
            Alloc::Delete(pixels);
            return false;
        }
        Bitmap bitmap(targetSize, static_cast<Bitmap::Format>(1), pixels);
        texture->Load(0, colorFormat, bitmap, 0);
    }
    if (textureStages && textureStages[4]) {
        DisplayTexture* const texture = VCall<DisplayTexture*>(display, 0x98);
        lightAnimTarget = texture;
        const Size lightSize{0x80, 0x80};
        RenderTextureConstruct construct(static_cast<TextureTarget>(1), false, true, false, nullptr);
        if (!texture->Create(lightSize, colorFormat, static_cast<TextureDimension>(0), 1,
                             static_cast<TextureAutoMip>(0), 0, 1.0f, static_cast<TextureHint>(1),
                             static_cast<TextureFilter>(1), static_cast<TextureFilter>(1),
                             static_cast<TextureFilter>(1), static_cast<TextureWrap>(0),
                             static_cast<TextureWrap>(2), construct)) {
            Alloc::Delete(pixels);
            return false;
        }
        Bitmap bitmap(lightSize, static_cast<Bitmap::Format>(1), pixels);
        texture->Load(0, colorFormat, bitmap, 0);
    }
    if (pixels) Alloc::Delete(pixels);
    SetReflectionType(detail);
    return true;
}

// 0x10010110
void __cdecl WaterMaterialFresWave::SetupVertexProgramConstants(VertexProgram* program, TextureCombiner* combiner,
                                                                Display* display, Camera* camera,
                                                                const DisplayLightPtrList* lights,
                                                                const Matrix4* modelView, const Matrix4& world,
                                                                const Matrix4& view, bool reflection) const
{
    // The original transforms the light direction through the camera matrix and then the model-view 3x3.
    Vector3 light = lightDir;
    Color lightColorValue = lightColor;
    Color ambientColorValue = ambientColor;
    if (!lights || lights->Count() == 0) {
        if (camera) {
            const float* const c = reinterpret_cast<const float*>(reinterpret_cast<const unsigned char*>(camera) + 0x238);
            const float x = c[0] * light.x + c[1] * light.y + c[2] * light.z;
            const float y = c[3] * light.x + c[4] * light.y + c[5] * light.z;
            const float z = c[6] * light.x + c[7] * light.y + c[8] * light.z;
            light = Vector3(x, y, z);
        }
    } else {
        DisplayLight* const light0 = lights->First() != lights->End() ? *lights->First() : nullptr;
        if (light0) {
            lightColorValue = light0->GetDiffuse();
            ambientColorValue = light0->GetAmbient();
            light = VCall<Vector3>(light0, 0x14);
        }
    }

    const Matrix4* const mv = modelView ? modelView : &world;
    const float* m = mv->m;
    const float lx = m[0] * light.x + m[1] * light.y + m[2] * light.z;
    const float ly = m[4] * light.x + m[5] * light.y + m[6] * light.z;
    const float lz = m[8] * light.x + m[9] * light.y + m[10] * light.z;

    const float fog = *reinterpret_cast<const float*>(DisplayState(display) + 0x8c);
    auto setConstant = [program](int reg, float x, float y, float z, float w) {
        if (reg < 0 || !program) return;
        const Vector4 value{x, y, z, w};
        program->SetLocalConstant(reg, value);
    };
    auto setColor = [program](int reg, const Color& color) {
        if (reg < 0 || !program) return;
        program->SetLocalConstant(reg, color);
    };

    if (!reflection) {
        setConstant(vertexProgramConstants[0], lx, ly, lz, 0.0f);
        if (vertexProgramConstants[11] >= 0) setConstant(vertexProgramConstants[11], fog, fog, fog, fog);
        if (flags & 0x20) {
            setConstant(vertexProgramConstants[4], uvOffsetX, uvOffsetY, 0.0f, 0.0f);
            if (combiner) {
                combiner->SetConstantRegister(0, waterColor);
                combiner->SetConstantRegister(1, lightColorValue);
                combiner->SetConstantRegister(2, ambientColorValue);
            }
        } else {
            setColor(vertexProgramConstants[10], lightColorValue);
            setColor(vertexProgramConstants[11], ambientColorValue);
            if (reflectionType == 0)
                setColor(vertexProgramConstants[12], waterColor);
            else if (combiner)
                combiner->SetConstantRegister(0, waterColor);
        }
        if (reflectionType == 0) return;
        setConstant(vertexProgramConstants[5], 1.0f / scaleNormXY, 1.0f / scaleNormXY, 1.0f, 1.0f);
        if (program) {
            const float tanu[4] = {1.0f, 1.0f, 1.0f, 1.0f};
            const float tanv[4] = {1.0f, 1.0f, 1.0f, 1.0f};
            VCall<void>(program, 0x64, vertexProgramConstants[6], tanu, 3);
            VCall<void>(program, 0x64, vertexProgramConstants[7], tanv, 3);
        }
        setConstant(vertexProgramConstants[10], lightColorValue.r, lightColorValue.g, lightColorValue.b, 1.0f);
        setConstant(vertexProgramConstants[15], 1.0f, 1.0f, IsReflectEyeVector() ? -1.0f : 1.0f, 1.0f);
        setConstant(vertexProgramConstants[9], refractiveTerm, 1.0f - refractiveTerm, 0.0f, 0.0f);
    } else {
        setConstant(reflectionProgramConstants[0], lx, ly, lz, 0.0f);
        if (reflectionProgramConstants[11] >= 0)
            setConstant(reflectionProgramConstants[11], fog, fog, fog, fog);
        if (!(flags & 0x20)) {
            setColor(reflectionProgramConstants[7], lightColorValue);
            setColor(reflectionProgramConstants[8], ambientColorValue);
            if (reflectionType == 0)
                setColor(reflectionProgramConstants[9], waterColor);
            else if (combiner)
                combiner->SetConstantRegister(0, waterColor);
        } else {
            setConstant(reflectionProgramConstants[1], uvOffsetX, uvOffsetY, 0.0f, 0.0f);
            if (combiner) {
                combiner->SetConstantRegister(0, waterColor);
                combiner->SetConstantRegister(1, lightColorValue);
                combiner->SetConstantRegister(2, ambientColorValue);
            }
        }
        if (reflectionType == 0) return;
        setConstant(reflectionProgramConstants[5], 1.0f / scaleNormXY, 1.0f / scaleNormXY, 1.0f, 1.0f);
        setConstant(reflectionProgramConstants[6], lightAnimDirection.x, lightAnimDirection.y,
                    lightAnimDirection.z, 1.0f);
        setConstant(reflectionProgramConstants[8], 1.0f, 1.0f, 1.0f, 1.0f);
        setConstant(reflectionProgramConstants[9], refractiveTerm, 1.0f - refractiveTerm, 0.0f, 0.0f);
    }
    (void)view;
    (void)reflectionAmount;
}

// ---------------------------------------------------------------------------------------------------------------
// the off-screen squares

// 0x10011150
void WaterMaterialFresWave::RenderSquare(Display* display) const
{
    static bool ready;
    static Vector4 vertices[4];
    if (!ready) {
        vertices[0] = Vector4{-1.0f, -1.0f, 0.0f, 1.0f};
        vertices[1] = Vector4{1.0f, -1.0f, 0.0f, 1.0f};
        vertices[2] = Vector4{1.0f, 1.0f, 0.0f, 1.0f};
        vertices[3] = Vector4{-1.0f, 1.0f, 0.0f, 1.0f};
        ready = true;
    }
    const int declaration = VCall<int>(display, 0x10c, 0x23, 5, 2, vertices, 4);
    VCall<void>(display, 0xe4, declaration);
}

// 0x10010f10
void WaterMaterialFresWave::CalculateDetailTexture(Display* display) const
{
    WsViewModeParams params;
    GetViewModeParams(display, params);
    display->SetRenderTarget(detailTarget, 0);
    display->Set2DView(-1.0f, 1.0f, -1.0f, 1.0f, -1000.0f, 1000.0f);
    WaterTextureAnimation* const animation = textureAnimation;
    if (!animation) return;
    const int frame = frameAnimation ? frameAnimation->GetCurrentFrame() : 0;
    if (Texture* const first = animation->GetTexture(frame)) first->Bind(0);
    display->SetCullMode(static_cast<DCull>(0));
    VCall<void>(display, 0xac, 1);
    display->SetDepthTestMode(false);
    display->SetLightingMode(false);
    display->SetAlphaBlendMode(false);
    VCall<void>(display, 0x50);
    if (Texture* const second = animation->GetTexture(frame + 1)) second->Bind(1);
    VCall<void>(display, 0x12c, 0);
    VCall<void>(display, 0x12c, 1);
    RenderSquare(display);
    VCall<void>(display, 0x12c, 0);
    VCall<void>(display, 0x12c, 1);
    display->SetAlphaBlendMode(false);
    display->SetLightingMode(false);
    display->SetDepthTestMode(false);
    display->SetRenderTarget(nullptr, 0);
    SetViewModeParams(display, params);
}

// 0x100112b0
void WaterMaterialFresWave::CalculateWaterColorTexture(Display* display) const
{
    Color const color{waterColor.r, waterColor.g, waterColor.b, 1.0f};
    const unsigned long converted = Display::ConvertColor(color);
    (void)converted;
    WsViewModeParams params;
    GetViewModeParams(display, params);
    display->SetRenderTarget(waterColorTarget, 0);
    display->Set2DView(-1.0f, 1.0f, -1.0f, 1.0f, -1000.0f, 1000.0f);
    if (reflectionTexture) {
        reflectionTexture->Bind(0);
        reflectionTexture->Bind(1);
    }
    display->SetCullMode(static_cast<DCull>(0));
    display->SetLightingMode(false);
    VCall<void>(display, 0xac, 1);
    display->SetDepthTestMode(false);
    display->SetAlphaBlendMode(false);
    VCall<void>(display, 0x50);
    const Color clearColor{0.0f, 0.0f, 0.0f, reflectionAmount};
    VCall<void>(display, 0x130, 0, &clearColor);
    VCall<void>(display, 0x128, textureStages[4]);
    VCall<void>(display, 0x128, textureStages[5]);
    const int declaration = VCall<int>(display, 0x10c, 0x13, 5, 2, nullptr, 4);
    VCall<void>(display, 0xe4, declaration);
    display->SetAlphaBlendMode(false);
    display->SetLightingMode(false);
    display->SetDepthTestMode(false);
    VCall<void>(display, 0x12c, 1);
    display->SetRenderTarget(nullptr, 0);
    SetViewModeParams(display, params);
    const_cast<WaterMaterialFresWave*>(this)->waterColorDirty = false;
}

// 0x10011590
void WaterMaterialFresWave::CalculateLightAnimTexture(Display* display) const
{
    const unsigned long converted = Display::ConvertColor(lightAnimColor);
    (void)converted;
    WsViewModeParams params;
    GetViewModeParams(display, params);
    display->SetRenderTarget(lightAnimTarget, 0);
    display->Set2DView(-1.0f, 1.0f, -1.0f, 1.0f, -1000.0f, 1000.0f);
    const Src::Factor savedSrc = static_cast<Src::Factor>(DisplayState(display)[0x68]);
    VCall<void>(display, 0x78, 0);
    VCall<void>(display, 0x12c, 0);
    VCall<void>(display, 0x78, 1);
    VCall<void>(display, 0x12c, 1);
    display->SetCullMode(static_cast<DCull>(0));
    VCall<void>(display, 0xac, 1);
    display->SetDepthTestMode(false);
    display->SetLightingMode(false);
    display->SetClearColor(lightAnimColor2);
    VCall<void>(display, 0x130, 1, lightAnimColor2);
    display->SetAlphaBlendMode(false);
    display->SetAlphaBlendFunction(savedSrc, static_cast<Dst::Factor>(1));
    if ((flags & 1) && detailTarget) {
        VCall<void>(detailTarget, 0x20, 0);
    } else if (textureStages && textureStages[2]) {
        VCall<void>(display, 0x128, textureStages[2]);
    }
    VCall<void>(display, 0x128, textureStages[4]);
    VCall<void>(display, 0x50);
    const Vector3 offset(-lightAnimDirection.x, -lightAnimDirection.y, -lightAnimDirection.z);
    VCall<void>(display, 0x134, 0, &offset);
    VCall<void>(display, 0x128, textureStages[3]);
    VCall<void>(display, 0x128, textureStages[4]);
    const int declaration = VCall<int>(display, 0x10c, 0x13, 5, 2, nullptr, 4);
    VCall<void>(display, 0xe4, declaration);
    VCall<void>(display, 0x12c, 0);
    VCall<void>(display, 0x12c, 1);
    display->SetAlphaBlendMode(false);
    display->SetAlphaBlendFunction(savedSrc, static_cast<Dst::Factor>(1));
    display->SetLightingMode(false);
    display->SetDepthTestMode(false);
    display->SetRenderTarget(nullptr, 0);
    SetViewModeParams(display, params);
}

}  // namespace Jet




















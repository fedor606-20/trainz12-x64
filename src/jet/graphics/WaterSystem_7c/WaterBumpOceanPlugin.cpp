// WaterSystem U7a/U8a: the four water material factories' lifetime, IsSupported and CreateInstance, and the
// geometry chunks the bump/ocean materials hand out. The shared MaterialPlugin base stays the original's
// (called through its imports); only the derived vptr and virtuals are local.
#include "WaterMaterialInternal.h"

#include <jet/Geometry.h>
#include <jet/RenderSystem.h>
#include <jet/VCall.h>
#include <jet/WaterMaterial.h>

#include <cstdint>
#include <cstring>
#include <new>

namespace {

using BaseCtor = void (__thiscall WsMaterialPluginAbi::*)(const Jet::PString&, const Jet::UId&);
using BaseCopy = void (__thiscall WsMaterialPluginAbi::*)(const WsMaterialPluginAbi&);
using BaseAssign = WsMaterialPluginAbi& (__thiscall WsMaterialPluginAbi::*)(const WsMaterialPluginAbi&);
using BaseDtor = void (__thiscall WsMaterialPluginAbi::*)();
using DeleteFn = void(__cdecl*)(void*);

template <class Fn>
void* SlotAddress(Fn fn)
{
    union { Fn member; void* code; } u;
    u.member = fn;
    return u.code;
}

// the factory pointer the original's PluginInstance::Assign stores at +8 (Material's PluginInstance part)
void AssignFactory(Jet::Material* object, const void* factory)
{
    auto* const slot = reinterpret_cast<void**>(reinterpret_cast<unsigned char*>(object) + 8);
    void* const old = *slot;
    *slot = const_cast<void*>(factory);
    if (*slot) Jet::VCall<void>(*slot, 0);  // PluginType::AddReference
    if (old) Jet::VCall<void>(old, 4);      // PluginType::ReleaseReference
}

// ---------------------------------------------------------------------------------------------------------------
// The geometry chunks the water materials create. The template keeps a CPU copy of the vertices; the rest of the
// chunk is RenderSystem's GeometryChunkSingleStream. Only the virtuals the water path uses are overridden.

struct WsVertexPCTUD { unsigned char data[0x18]; };
struct WsVertexPA1A2T { unsigned char data[0x34]; };

void* BufferManager(Jet::Display* display)
{
    return *reinterpret_cast<void* const*>(reinterpret_cast<unsigned char*>(display) + 0x20c);
}
unsigned int VertexTypeStride(const Jet::VertexType* type)
{
    return **reinterpret_cast<const unsigned int* const*>(reinterpret_cast<const unsigned char*>(type) + 0x10);
}
const unsigned short* IndexData(const void* block)
{
    return reinterpret_cast<const unsigned short*>(reinterpret_cast<std::uintptr_t>(block) + 0x10);
}

template <class P>
class WsChunkTemplate : public Jet::GeometryChunkSingleStream {
public:
    WsChunkTemplate(unsigned int vertices, unsigned int primitives, unsigned int indices, unsigned int faceNormals,
                    unsigned int flags)
        : GeometryChunkSingleStream(vertices, primitives, indices, faceNormals, flags)
    {
        CreateArray(vertices);
    }
    ~WsChunkTemplate() override { ReleaseArray(); }

    Jet::GeometryChunk* __cdecl CreateInstance() const override
    {
        void* const memory = Jet::Alloc::New(0x60);
        if (!memory) return nullptr;
        auto* const chunk = ::new (memory) WsChunkTemplate<P>(GetNumVertices(), GetNumPrimitives(), GetNumIndices(),
                                                             GetNumFaceNormals(), GetFlags());
        return chunk;
    }
    Jet::GeometryChunk* __cdecl CreateInstance(unsigned int vertices, unsigned int primitives,
                                               unsigned int indices, unsigned int faceNormals) const override
    {
        void* const memory = Jet::Alloc::New(0x60);
        if (!memory) return nullptr;
        return ::new (memory) WsChunkTemplate<P>(vertices, primitives, indices, faceNormals, GetFlags());
    }
    bool __cdecl InitializeBuffers(Jet::Display* display, bool upload) override
    {
        const unsigned long f = flags;
        const Jet::StaticBuffer* const had = GetStaticVertexBuffer(0, 0);
        const Jet::VertexType* const type = AssignVertexType(display);
        if (!(f & 0x10000) && !had) {
            if (!type) return false;
            auto* buffer = static_cast<Jet::StaticBuffer*>(Jet::VCall<void*>(display, 0x104, maxVertices,
                                                                            VertexTypeStride(type)));
            if (!buffer) return false;
            if (upload) {
                unsigned int out = 0;
                if (!Jet::VCall<unsigned char>(BufferManager(display), 0x20, type, buffer, GetVertexArray(0, 0),
                                               numVertices, 1, &out, 0)) {
                    buffer->ReleaseReference();
                    return false;
                }
                SetVertexDataDirty(false, 0, 0);
            } else {
                SetVertexDataDirty(true, 0, 0);
            }
            SetStaticVertexBuffer(buffer, 0, 0);
            buffer->ReleaseReference();
        }
        const Jet::StaticBuffer* const hadIndex = GetStaticIndexBuffer(0);
        if (!(f & 0x100) && !hadIndex) {
            const unsigned int count = indexArray.Count();
            auto* buffer = static_cast<Jet::StaticBuffer*>(Jet::VCall<void*>(display, 0x108, count, 0));
            if (!buffer) return false;
            if (upload) {
                unsigned int out = 0;
                if (Jet::VCall<int>(BufferManager(display), 0x2c, buffer, IndexData(indexArray.block), numIndices, 1,
                                    &out) == -1) {
                    buffer->ReleaseReference();
                    return false;
                }
                SetIndexDataDirty(false);
            } else {
                SetIndexDataDirty(true);
            }
            SetStaticIndexBuffer(buffer, 0);
            buffer->ReleaseReference();
        }
        return true;
    }

    bool __cdecl LoadBuffers(Jet::Display* display) override
    {
        const unsigned long f = flags;
        void* const manager = BufferManager(display);
        if (!(f & 0x10000)) {
            const Jet::StaticBuffer* const buffer = GetStaticVertexBuffer(0, 0);
            if (!buffer) return false;
            unsigned int out = 0;
            if (!Jet::VCall<unsigned char>(manager, 0x20, vertexType, buffer, GetVertexArray(0, 0), numVertices, 1,
                                           &out, 0))
                return false;
            SetVertexDataDirty(false, 0, 0);
        }
        if (!(f & 0x100)) {
            const Jet::StaticBuffer* const buffer = GetStaticIndexBuffer(0);
            if (!buffer) return false;
            unsigned int out = 0;
            if (Jet::VCall<int>(manager, 0x2c, buffer, IndexData(indexArray.block), numIndices, 1, &out) == -1) {
                buffer->ReleaseReference();
                return false;
            }
            SetIndexDataDirty(false);
        }
        return true;
    }
    void __cdecl DuplicateVertexArray(Jet::Display*, int, int) override
    {
        if (!arrayHead_) return;
        auto* const head = static_cast<Head*>(Jet::Alloc::New(0x10 + GetMaxVertices() * sizeof(P)));
        if (!head) return;
        head->refs = 1;
        head->count = GetMaxVertices();
        std::memcpy(head + 1, arrayHead_ + 1, GetMaxVertices() * sizeof(P));
        ReleaseArray();
        arrayHead_ = head;
    }
    void __cdecl DuplicateIndexArray(Jet::Display* display) override
    {
        Jet::GeometryChunk::DuplicateIndexArray(display);
    }
    void __cdecl ResizeVertexArray(Jet::Display*, unsigned int n, int, int) override
    {
        auto* const head = static_cast<Head*>(Jet::Alloc::New(0x10 + n * sizeof(P)));
        if (!head) return;
        head->refs = 1;
        head->count = n;
        const unsigned int copy = arrayHead_ && arrayHead_->count < n ? arrayHead_->count : n;
        if (arrayHead_) std::memcpy(head + 1, arrayHead_ + 1, copy * sizeof(P));
        ReleaseArray();
        arrayHead_ = head;
    }
    void* __cdecl GetVertexArray(int, int) override { return arrayHead_ ? arrayHead_ + 1 : nullptr; }
    const void* __cdecl GetVertexArray(int, int) const override { return arrayHead_ ? arrayHead_ + 1 : nullptr; }
    const Jet::VertexType* __cdecl AssignVertexType(Jet::Display* display) const override
    {
        if (!vertexType) {
            const_cast<WsChunkTemplate*>(this)->SetVertexType(
                static_cast<Jet::VertexType*>(Jet::VCall<void*>(display, 0x10c, static_cast<Jet::VertexTypeID>(99))), 0);
        }
        return &GetVertexType(0);
    }
    int __cdecl GetVertexSize(int) const override { return sizeof(P); }
    Jet::VertexTypeID __cdecl GetVertexTypeID(int) const override { return static_cast<Jet::VertexTypeID>(99); }
    int __cdecl GetItemOffset(Jet::VertexItemType type, int) const override { return ItemOffset(type); }

    Jet::GeometryChunk::Vector2Ptr __cdecl GetVector2Array(Jet::VertexItemType type, int) override
    {
        return Make<Jet::GeometryChunk::Vector2Ptr, Jet::Vector2>(type);
    }
    Jet::GeometryChunk::ConstVector2Ptr __cdecl GetVector2Array(Jet::VertexItemType type, int) const override
    {
        return Make<Jet::GeometryChunk::ConstVector2Ptr, const Jet::Vector2>(type);
    }
    Jet::GeometryChunk::Vector3Ptr __cdecl GetVector3Array(Jet::VertexItemType type, int) override
    {
        return Make<Jet::GeometryChunk::Vector3Ptr, Jet::Vector3>(type);
    }
    Jet::GeometryChunk::ConstVector3Ptr __cdecl GetVector3Array(Jet::VertexItemType type, int) const override
    {
        return Make<Jet::GeometryChunk::ConstVector3Ptr, const Jet::Vector3>(type);
    }
    Jet::GeometryChunk::Vector4Ptr __cdecl GetVector4Array(Jet::VertexItemType type, int) override
    {
        return Make<Jet::GeometryChunk::Vector4Ptr, Jet::Vector4>(type);
    }
    Jet::GeometryChunk::ConstVector4Ptr __cdecl GetVector4Array(Jet::VertexItemType type, int) const override
    {
        return Make<Jet::GeometryChunk::ConstVector4Ptr, const Jet::Vector4>(type);
    }
    Jet::GeometryChunk::FloatPtr __cdecl GetFloatArray(Jet::VertexItemType type, int) override
    {
        return Make<Jet::GeometryChunk::FloatPtr, float>(type);
    }
    Jet::GeometryChunk::ConstFloatPtr __cdecl GetFloatArray(Jet::VertexItemType type, int) const override
    {
        return Make<Jet::GeometryChunk::ConstFloatPtr, const float>(type);
    }
    Jet::GeometryChunk::UDwordPtr __cdecl GetUDwordArray(Jet::VertexItemType type, int index) override
    {
        return Make<Jet::GeometryChunk::UDwordPtr, unsigned long>(type);
    }
    Jet::GeometryChunk::ConstUDwordPtr __cdecl GetUDwordArray(Jet::VertexItemType type, int index) const override
    {
        return Make<Jet::GeometryChunk::ConstUDwordPtr, const unsigned long>(type);
    }

protected:
    struct Head {
        unsigned int refs;
        unsigned int count;
        unsigned int size;
        unsigned int pad;
    };

    void CreateArray(unsigned int count)
    {
        arrayHead_ = static_cast<Head*>(Jet::Alloc::New(0x10 + count * sizeof(P)));
        if (arrayHead_) {
            arrayHead_->refs = 1;
            arrayHead_->count = count;
            // +8/+c stay as Alloc::New left them: the original template never writes them.
        }
    }
    void ReleaseArray()
    {
        if (arrayHead_ && --arrayHead_->refs == 0) Jet::Alloc::Delete(arrayHead_);
        arrayHead_ = nullptr;
    }
    static int ItemOffset(Jet::VertexItemType type)
    {
        static const int offsets18[48] = {
            -1, 0, 12, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 16,
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, 0, -1, 0, -1, -1, -1, -1, -1, -1};
        static const int offsets34[48] = {
            -1, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 44,
            -1, -1, -1, -1, -1, -1, -1, -1, 12, 28, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
        const int* const table = sizeof(P) == 0x18 ? offsets18 : offsets34;
        const unsigned index = static_cast<unsigned>(type);
        return index < 48 ? table[index] : -1;
    }
    template <class Ptr, class T>
    Ptr Make(Jet::VertexItemType type) const
    {
        const int offset = ItemOffset(type);
        if (!arrayHead_ || offset < 0) return Ptr{0, nullptr};
        return Ptr{sizeof(P), reinterpret_cast<T*>(reinterpret_cast<unsigned char*>(arrayHead_ + 1) + offset)};
    }

    Head* arrayHead_;
};

template <class P>
Jet::GeometryChunk* CreateWaterChunk(Jet::VertexType* vertexType, unsigned int numVertices,
                                     unsigned int numPrimitives, unsigned int numIndices,
                                     unsigned int numFaceNormals, Jet::Material::GeometryType type,
                                     Jet::GeometryChunkCreateHint* hint, bool waveVertexProgram)
{
    const unsigned int flags = Jet::Material::MakeGeometryChunkFlags(type, hint);
    void* const memory = Jet::Alloc::New(0x60);
    if (!memory) return nullptr;
    auto* const chunk = ::new (memory) WsChunkTemplate<P>(numVertices, numPrimitives, numIndices, numFaceNormals, flags);
    Jet::VCall<void>(chunk, 0xa0, vertexType, 0);
    return chunk;
}

}  // namespace

namespace Jet {

GeometryChunk* WsCreateBumpGeometryChunk(const WaterBumpMaterial& material, unsigned int numVertices,
                                         unsigned int numPrimitives, unsigned int numIndices,
                                         unsigned int numFaceNormals, Material::GeometryType type,
                                         GeometryChunkCreateHint* hint, bool waveVertexProgram)
{
    return CreateWaterChunk<WsVertexPCTUD>(material.vertexType, numVertices, numPrimitives, numIndices, numFaceNormals, type, hint,
                                           waveVertexProgram);
}

GeometryChunk* WsCreateOceanGeometryChunk(const WaterOceanMaterial& material, unsigned int numVertices,
                                          unsigned int numPrimitives, unsigned int numIndices,
                                          unsigned int numFaceNormals, Material::GeometryType type,
                                          GeometryChunkCreateHint* hint, bool waveVertexProgram)
{
    return CreateWaterChunk<WsVertexPA1A2T>(material.vertexType, numVertices, numPrimitives, numIndices, numFaceNormals, type,
                                            hint, waveVertexProgram);
}

}  // namespace Jet

// 0x10007340 / 0x10015ad0: allocate the concrete material and give it this factory
class WsBumpFactoryCreate {
public:
    Jet::Material* __cdecl Create(Jet::MaterialManager* manager, Jet::RenderKernel*) const;
};
class WsOceanFactoryCreate {
public:
    Jet::Material* __cdecl Create(Jet::MaterialManager* manager, Jet::RenderKernel*) const;
};

Jet::Material* __cdecl WsBumpFactoryCreate::Create(Jet::MaterialManager* manager, Jet::RenderKernel*) const
{
    void* const memory = Jet::Alloc::New(0x138);
    Jet::Material* material = nullptr;
    if (memory) material = ::new (memory) Jet::WaterBumpMaterial(manager);
    if (material) AssignFactory(material, this);
    return material;
}

Jet::Material* __cdecl WsOceanFactoryCreate::Create(Jet::MaterialManager* manager, Jet::RenderKernel*) const
{
    void* const memory = Jet::Alloc::New(0x174);
    Jet::Material* material = nullptr;
    if (memory) material = ::new (memory) Jet::WaterOceanMaterial(manager);
    if (material) AssignFactory(material, this);
    return material;
}

namespace {
struct FactoryVtable {
    void* slots[6];
};
void SetFactorySlot5(void* table, void* fn)
{
    static_cast<FactoryVtable*>(table)->slots[5] = fn;
}
}  // namespace

// Called by WaterMaterialStatics.cpp after the mirrors are copied.
void WsInstallFactoryCreate()
{
    SetFactorySlot5(WsBumpVftable, SlotAddress(&WsBumpFactoryCreate::Create));
    SetFactorySlot5(WsOceanVftable, SlotAddress(&WsOceanFactoryCreate::Create));
}

// Unit U10 of Render3DCore_7c: Jet::RenderPrimitive, Render2DPrimitive and Render3DPrimitive, transcribed from the
// TS12 assembly. Layout and vtable offsets are in Render3DCorePrimitives.h.
//
// The two per-instance pointer blocks are AutoReferenceArray<AutoReference<T>>: {refs, count, unused[2], items}. The
// last reference releases every item through ReferenceCount::ReleaseReference and frees the block with Jet::Alloc
// (the originals 0x22540/0x225b0). RenderPrimitive::Init and Render are still the original's (they drive the material
// manager and the display); the vtable entries point into Render3DCore_7c_orig.dll through the import library.
#include <jet/Render3DCorePrimitives.h>

#include <windows.h>

#include <cstdio>
#include <cstring>
#include <new>

namespace Jet {

// ViewPoint is U8's class; only Add is reached from a primitive's Cull.
class ViewPoint {
public:
    void __cdecl Add(Element* element, const Matrix4& m);
};

namespace {

// x87 fld/fstp quiesces a signalling NaN, the integer copies in the original do not.
float X87Copy(float f)
{
    unsigned int u;
    std::memcpy(&u, &f, 4);
    if ((u & 0x7f800000u) == 0x7f800000u && (u & 0x007fffffu) && !(u & 0x00400000u)) u |= 0x00400000u;
    std::memcpy(&f, &u, 4);
    return f;
}

template <class T>
void ReleaseBlock(AutoReferenceArray<AutoReference<T>>& array)
{
    typename AutoReferenceArray<AutoReference<T>>::Block* const b = array.block;
    if (!b) return;
    if (--b->refs == 0) {
        for (unsigned int i = 0; i < b->count; ++i) {
            T* const item = b->data[i].Get();
            if (item) item->ReleaseReference();
        }
        Alloc::Delete(b);
    }
    array.block = nullptr;
}

template <class T>
void ShareBlock(AutoReferenceArray<AutoReference<T>>& to, const AutoReferenceArray<AutoReference<T>>& from)
{
    if (to.block == from.block) return;
    ReleaseBlock(to);
    to.block = from.block;
    if (to.block) ++to.block->refs;
}

// The original makes a block of `capacity` pointers with count 0 (only refs and count are written).
template <class T>
void MakeBlock(AutoReferenceArray<AutoReference<T>>& array, unsigned int capacity)
{
    using Block = typename AutoReferenceArray<AutoReference<T>>::Block;
    Block* const b = static_cast<Block*>(Alloc::New(capacity * sizeof(AutoReference<T>) + 0x10));
    b->refs = 1;
    b->count = 0;
    array.block = b;
}

// a Vector3 is a small enough aggregate to pass the same way the original does (by pointer to three floats)
bool Different(const Vector3& a, const Vector3& b)
{
    return !(a.x - b.x <= 0.01f && b.x - a.x <= 0.01f && a.y - b.y <= 0.01f && b.y - a.y <= 0.01f &&
             a.z - b.z <= 0.01f && b.z - a.z <= 0.01f);
}

}  // namespace

FastDynamicArray<DisplayLight*, 13, Less<DisplayLight*, DisplayLight*>> RenderPrimitive::lights;

// 0x683c0
RenderPrimitive::RenderPrimitive()
    : chunkArray(),
      materialArray(),
      geometryOrientation(),
      geometryPosition(),
      scale(),
      creationFlags(0),
      flags(8)
{
    renderFlags = 1;
    aliasCount = 0;
    aliasCapacity = 0;
    aliases = nullptr;
    geometryOrientation.x = 0.0f;
    geometryOrientation.y = 0.0f;
    geometryOrientation.z = 0.0f;
    geometryOrientation.w = 1.0f;
    geometryPosition.x = 0.0f;
    geometryPosition.y = 0.0f;
    geometryPosition.z = 0.0f;
    scale.x = 1.0f;
    scale.y = 1.0f;
    scale.z = 1.0f;
}

// 0x684a0: allocation is the number of chunk slots the original reserves
RenderPrimitive::RenderPrimitive(int allocation)
    : chunkArray(), materialArray(), geometryOrientation(), geometryPosition(), scale(), creationFlags(0), flags(8)
{
    renderFlags = 1;
    aliasCount = 0;
    aliasCapacity = 0;
    aliases = nullptr;
    MakeBlock(chunkArray, static_cast<unsigned int>(allocation));
    MakeBlock(materialArray, 1);
    geometryOrientation.x = 0.0f;
    geometryOrientation.y = 0.0f;
    geometryOrientation.z = 0.0f;
    geometryOrientation.w = 1.0f;
    geometryPosition.x = 0.0f;
    geometryPosition.y = 0.0f;
    geometryPosition.z = 0.0f;
    scale.x = 1.0f;
    scale.y = 1.0f;
    scale.z = 1.0f;
}

// 0x21cc0
RenderPrimitive::RenderPrimitive(const RenderPrimitive& o)
    : ViewPortParticle(o), kernel(o.kernel), chunkArray(), materialArray(), geometryOrientation(), geometryPosition(),
      scale(), color(), creationFlags(o.creationFlags), flags(o.flags & 0x0f)
{
    ShareBlock(chunkArray, o.chunkArray);
    ShareBlock(materialArray, o.materialArray);
    geometryOrientation.x = X87Copy(o.geometryOrientation.x);
    geometryOrientation.y = X87Copy(o.geometryOrientation.y);
    geometryOrientation.z = X87Copy(o.geometryOrientation.z);
    geometryOrientation.w = X87Copy(o.geometryOrientation.w);
    std::memcpy(&geometryPosition, &o.geometryPosition, sizeof geometryPosition);
    std::memcpy(&scale, &o.scale, sizeof scale);
    color.r = X87Copy(o.color.r);
    color.g = X87Copy(o.color.g);
    color.b = X87Copy(o.color.b);
    color.a = X87Copy(o.color.a);
}

// 0x21e90
RenderPrimitive& RenderPrimitive::operator=(const RenderPrimitive& o)
{
    ViewPortParticle::operator=(o);
    kernel = o.kernel;
    ShareBlock(chunkArray, o.chunkArray);
    ShareBlock(materialArray, o.materialArray);
    geometryOrientation.x = X87Copy(o.geometryOrientation.x);
    geometryOrientation.y = X87Copy(o.geometryOrientation.y);
    geometryOrientation.z = X87Copy(o.geometryOrientation.z);
    geometryOrientation.w = X87Copy(o.geometryOrientation.w);
    std::memcpy(&geometryPosition, &o.geometryPosition, sizeof geometryPosition);
    std::memcpy(&scale, &o.scale, sizeof scale);
    color.r = X87Copy(o.color.r);
    color.g = X87Copy(o.color.g);
    color.b = X87Copy(o.color.b);
    color.a = X87Copy(o.color.a);
    creationFlags = o.creationFlags;
    flags = (flags & 0xf0) | (o.flags & 0x0f);
    return *this;
}

// 0x68600
RenderPrimitive::~RenderPrimitive()
{
    ReleaseBlock(materialArray);
    ReleaseBlock(chunkArray);
}

// 0x68850
void RenderPrimitive::Cull(ViewPoint& view, const Matrix4& m)
{
    Material* const material = materialArray.block ? materialArray.block->data[0].Get() : nullptr;
    if (material) material->Activate(-1);
    view.Add(this, m);
}

// 0x22090
void RenderPrimitive::SetOpacity(float opacity)
{
    const unsigned int count = chunkArray.block ? chunkArray.block->count : 0;
    for (unsigned int i = 0; i < count; ++i) {
        GeometryChunk* const chunk = chunkArray.block->data[i].Get();
        if (chunk) chunk->SetOpacity(kernel ? kernel->GetDisplay() : nullptr, opacity);
    }
    for (Particle* child = _head; child; child = child->_next) child->SetOpacity(opacity);
}

// 0x22110
float RenderPrimitive::GetOpacity() const
{
    if (chunkArray.block && chunkArray.block->count) {
        GeometryChunk* const chunk = chunkArray.block->data[0].Get();
        if (chunk) return chunk->GetOpacity();
    }
    return 1.0f;
}

// 0x22270
void RenderPrimitive::SetScale(const Vector3& value)
{
    scale = value;
    if (Different(value, Vector3{1.0f, 1.0f, 1.0f})) flags |= 4;
    CalculatePrimitiveVolume();
}

// 0x222e0
const Vector3& RenderPrimitive::GetScale() const
{
    return scale;
}

// 0x22200
void RenderPrimitive::SetGeometryOrientation(const Quaternion& value)
{
    geometryOrientation = value;
    if (value.w != 1.0f) flags |= 1;
    CalculatePrimitiveVolume();
}

// 0x22260
const Quaternion& RenderPrimitive::GetGeometryOrientation() const
{
    return geometryOrientation;
}

// 0x22170
void RenderPrimitive::SetGeometryPosition(const Vector3& value)
{
    geometryPosition.x += value.x;
    geometryPosition.y += value.y;
    geometryPosition.z += value.z;
    if (Different(geometryPosition, Vector3{0.0f, 0.0f, 0.0f})) flags |= 2;
    CalculatePrimitiveVolume();
}

// 0x221f0
const Vector3& RenderPrimitive::GetGeometryPosition() const
{
    return geometryPosition;
}

// the base vtable's GetGeometryRadius is the module's shared zero stub (0x100079e0)
float RenderPrimitive::GetGeometryRadius() const
{
    return 0.0f;
}

// 0x22150: the base answers the origin, a shape overrides it
Vector3 RenderPrimitive::GetGeometryCenter() const
{
    return Vector3{0.0f, 0.0f, 0.0f};
}

// 0x68ba0: the bounding sphere from the primitive's centre and radius
void RenderPrimitive::CalculatePrimitiveVolume()
{
    const float radius = GetGeometryRadius();
    const Vector3 center = GetGeometryCenter();
    Sphere sphere;
    sphere.center = center;
    sphere.radius = radius;
    void* const volume = *reinterpret_cast<void* const*>(reinterpret_cast<char*>(this) + 0x4c);
    if (volume) {
        using Fn = void(__cdecl*)(void*, const Sphere*);
        void** const table = *reinterpret_cast<void***>(volume);
        reinterpret_cast<Fn>(table[0x1c / 4])(volume, &sphere);
    }
}

// 0x22330
int RenderPrimitive::GetCreationFlags() const
{
    return creationFlags;
}

// 0x22340
unsigned int RenderPrimitive::GetNumChunks() const
{
    return chunkArray.block ? chunkArray.block->count : 0;
}

// 0x22360
AutoReferenceArray<AutoReference<GeometryChunk>> RenderPrimitive::GetChunkArray()
{
    AutoReferenceArray<AutoReference<GeometryChunk>> result;
    if (chunkArray.block) {
        result.block = chunkArray.block;
        ++result.block->refs;
    }
    return result;
}

AutoReferenceArray<AutoReference<GeometryChunk>> const RenderPrimitive::GetChunkArray() const
{
    AutoReferenceArray<AutoReference<GeometryChunk>> result;
    if (chunkArray.block) {
        result.block = chunkArray.block;
        ++result.block->refs;
    }
    return result;
}

// 0x223a0
AutoReferenceArray<AutoReference<Material>> RenderPrimitive::GetMaterialArray()
{
    AutoReferenceArray<AutoReference<Material>> result;
    if (materialArray.block) {
        result.block = materialArray.block;
        ++result.block->refs;
    }
    return result;
}

AutoReferenceArray<AutoReference<Material>> const RenderPrimitive::GetMaterialArray() const
{
    AutoReferenceArray<AutoReference<Material>> result;
    if (materialArray.block) {
        result.block = materialArray.block;
        ++result.block->refs;
    }
    return result;
}

// 0x223e0
const Color& RenderPrimitive::GetColor() const
{
    return color;
}

// 0x222f0
void RenderPrimitive::SetTestCollision(bool state)
{
    if (state)
        creationFlags |= 8;
    else
        creationFlags &= ~8;
}

// 0x22310
bool RenderPrimitive::GetTestCollision() const
{
    return (creationFlags & 8) != 0;
}

// 0x223f0
void RenderPrimitive::SetDebugRender(bool state)
{
    if (state)
        flags |= 8;
    else
        flags &= static_cast<unsigned char>(~8);
}

// 0x22410
bool RenderPrimitive::GetDebugRender() const
{
    return (flags & 8) != 0;
}

// 0x68880: the primitive's own transform (orientation/position/scale) premultiplied into the caller's matrix
// (0x1ab30 writes the product back over its second operand), then every chunk is handed to the material's cache.
namespace {

// 0x1001ab30: out = local * modelView, row-major, with the assembly's per-element term order.
void MultiplyMatrices(Matrix4& out, const Matrix4& a, const Matrix4& b)
{
    float result[16];
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
            float value = SolidsDetail::MulSS(a.m[row * 4 + 1], b.m[4 + column]);
            value = SolidsDetail::AddSS(value, SolidsDetail::MulSS(a.m[row * 4 + 3], b.m[12 + column]));
            value = SolidsDetail::AddSS(value, SolidsDetail::MulSS(a.m[row * 4 + 2], b.m[8 + column]));
            value = SolidsDetail::AddSS(value, SolidsDetail::MulSS(a.m[row * 4 + 0], b.m[column]));
            result[row * 4 + column] = value;
        }
    }
    std::memcpy(out.m, result, sizeof result);
}

}  // namespace

void __cdecl RenderPrimitive::Render(Display* display, ViewPort& port, Matrix4& modelView)
{
    if ((flags & 1) != 0 || (flags & 6) != 0) {
        Matrix4 local;
        if ((flags & 1) != 0) {
            // the quaternion at +0x148 rotated into a row-vector matrix and scaled by +0x164.
            const float x = geometryOrientation.x;
            const float y = geometryOrientation.y;
            const float z = geometryOrientation.z;
            const float w = geometryOrientation.w;
            const float twoX = SolidsDetail::MulSS(x, 2.0f);
            const float twoY = SolidsDetail::MulSS(y, 2.0f);
            const float twoZ = SolidsDetail::MulSS(z, 2.0f);
            const float xx2 = SolidsDetail::MulSS(x, twoX);
            const float xz2 = SolidsDetail::MulSS(x, twoZ);
            const float zz2 = SolidsDetail::MulSS(z, twoZ);
            const float yz2 = SolidsDetail::MulSS(y, twoZ);
            const float xy2 = SolidsDetail::MulSS(x, twoY);
            const float wx2 = SolidsDetail::MulSS(w, twoX);
            const float wz2 = SolidsDetail::MulSS(w, twoZ);
            const float wy2 = SolidsDetail::MulSS(w, twoY);
            const float yy2 = SolidsDetail::MulSS(y, twoY);
            const float sx = scale.x;
            const float sy = scale.y;
            const float sz = scale.z;
            local.m[0] = SolidsDetail::MulSS(sx, SolidsDetail::SubSS(1.0f, SolidsDetail::AddSS(zz2, yy2)));
            local.m[1] = SolidsDetail::MulSS(sx, SolidsDetail::AddSS(wz2, xy2));
            local.m[2] = SolidsDetail::MulSS(sx, SolidsDetail::SubSS(xz2, wy2));
            local.m[3] = 0.0f;
            local.m[4] = SolidsDetail::MulSS(sy, SolidsDetail::SubSS(xy2, wz2));
            local.m[5] = SolidsDetail::MulSS(sy, SolidsDetail::SubSS(1.0f, SolidsDetail::AddSS(zz2, xx2)));
            local.m[6] = SolidsDetail::MulSS(sy, SolidsDetail::AddSS(wx2, yz2));
            local.m[7] = 0.0f;
            local.m[8] = SolidsDetail::MulSS(sz, SolidsDetail::AddSS(wy2, xz2));
            local.m[9] = SolidsDetail::MulSS(sz, SolidsDetail::SubSS(yz2, wx2));
            local.m[10] = SolidsDetail::MulSS(sz, SolidsDetail::SubSS(1.0f, SolidsDetail::AddSS(yy2, xx2)));
            local.m[11] = 0.0f;
            local.m[12] = geometryPosition.x;
            local.m[13] = geometryPosition.y;
            local.m[14] = geometryPosition.z;
            local.m[15] = 1.0f;
        } else {
            for (int i = 0; i < 16; ++i) local.m[i] = 0.0f;
            local.m[0] = scale.x;
            local.m[5] = scale.y;
            local.m[10] = scale.z;
            local.m[12] = geometryPosition.x;
            local.m[13] = geometryPosition.y;
            local.m[14] = geometryPosition.z;
            local.m[15] = 1.0f;
        }
        MultiplyMatrices(modelView, local, modelView);
    }

    const unsigned int count = chunkArray.block ? chunkArray.block->count : 0;
    if (count == 0) return;
    Material* const material = materialArray.block->data[0].Get();
    for (unsigned int i = 0; i < count; ++i) {
        GeometryChunk* const chunk = chunkArray.block->data[i].Get();
        material->CacheChunk(display, &port, chunk, &lights, &modelView);
    }
}

// 0x68ca0
GeometryChunk* RenderPrimitive::CreateGeometryChunk(unsigned int numVertices, unsigned int numPrimitives,
                                                    unsigned int numIndices, unsigned int numFaceNormals,
                                                    Material::GeometryType type, GeometryChunkCreateHint* hint)
{
    Material* const material = materialArray.block->data[0].Get();
    GeometryChunk* const chunk =
        material->CreateGeometryChunk(numVertices, numPrimitives, numIndices, numFaceNormals, type, hint);
    AutoReferenceArray<AutoReference<GeometryChunk>>::Block* const b = chunkArray.block;
    // the block's slots come from Alloc::New and are not constructed; the original stores the raw pointer
    using Ref = AutoReference<GeometryChunk>;
    ::new (static_cast<void*>(&b->data[b->count])) Ref();
    b->data[b->count] = chunk;
    ++b->count;
    if (chunk) {
        auto colors = chunk->GetVector3Array(static_cast<VertexItemType>(R3D_ITEM_COLOR3), 0);
        for (unsigned int i = 0; i < numVertices; ++i) colors.data[i] = Vector3{0.0f, 0.0f, 0.0f};
    }
    return chunk;
}

// 0x68d90
void RenderPrimitive::SetColor(const Color& value)
{
    color = value;
    const unsigned long packed = Display::ConvertColor(value);
    const unsigned int count = chunkArray.block ? chunkArray.block->count : 0;
    for (unsigned int i = 0; i < count; ++i) {
        GeometryChunk* const chunk = chunkArray.block->data[i].Get();
        if (!chunk) continue;
        auto colors = chunk->GetUDwordArray(static_cast<VertexItemType>(R3D_ITEM_COLOR), 0);
        const unsigned int numVertices = chunk->GetNumVertices();
        for (unsigned int v = 0; v < numVertices; ++v) colors.data[v] = packed;
    }
}

// 0x68e50
void RenderPrimitive::InitInstance(RenderPrimitive& instance) const
{
    ViewPortParticle::InitInstance(instance);
    instance.color = color;
    instance.geometryOrientation.x = X87Copy(geometryOrientation.x);
    instance.geometryOrientation.y = X87Copy(geometryOrientation.y);
    instance.geometryOrientation.z = X87Copy(geometryOrientation.z);
    instance.geometryOrientation.w = X87Copy(geometryOrientation.w);
    std::memcpy(&instance.geometryPosition, &geometryPosition, sizeof geometryPosition);
    std::memcpy(&instance.scale, &scale, sizeof scale);
    ShareBlock(instance.chunkArray, chunkArray);
    ShareBlock(instance.materialArray, materialArray);
    instance.creationFlags = creationFlags;
    instance.kernel = kernel;
    instance.flags = static_cast<unsigned char>((instance.flags & 0xf8) | (flags & 0x07));
}

// ---- Render2DPrimitive ------------------------------------------------------------------------------------------

// 0x5fad0
Render2DPrimitive::Render2DPrimitive() : RenderPrimitive() {}
// 0x5fb70
Render2DPrimitive::Render2DPrimitive(int allocation) : RenderPrimitive(allocation) {}

Render2DPrimitive::Render2DPrimitive(const Render2DPrimitive& o) : RenderPrimitive(o) {}

Render2DPrimitive::~Render2DPrimitive() {}

Render2DPrimitive& Render2DPrimitive::operator=(const Render2DPrimitive& o)
{
    RenderPrimitive::operator=(o);
    return *this;
}

// the base vtable entry is the zero stub
bool Render2DPrimitive::IsWithin(const Vector3&) const
{
    return false;
}

// ---- Render3DPrimitive ------------------------------------------------------------------------------------------

// 0x22430
Render3DPrimitive::Render3DPrimitive() : RenderPrimitive() {}

// 0x60bd0
Render3DPrimitive::Render3DPrimitive(int allocation) : RenderPrimitive(allocation) {}

Render3DPrimitive::Render3DPrimitive(const Render3DPrimitive& o) : RenderPrimitive(o) {}

// 0x22450 (the original folds the 2D and 3D assignments into one address)
Render3DPrimitive& Render3DPrimitive::operator=(const Render3DPrimitive& o)
{
    RenderPrimitive::operator=(o);
    return *this;
}

// 0x22420
Render3DPrimitive::~Render3DPrimitive() {}

}  // namespace Jet

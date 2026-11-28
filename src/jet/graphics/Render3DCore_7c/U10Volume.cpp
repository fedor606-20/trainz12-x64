// Unit U10 remainder: the primitive's oriented-box volume. RenderPrimitive::CalculateBoxVolume builds a
// ParticleOrientedBoundingBox from the chunk's position stream; every shape override in the original is a tail jump
// to it (0x65d00), so each exported override just calls the base helper.
//
// The flush-to-zero-free x87/SSE work is in OrientedBox::Init (U1c) and ParticleOrientedBoundingBox (U3); this file
// only follows the original's allocation/call order.
#include <jet/Render3DCorePrimitives.h>

#include <jet/Render3DCoreBounds.h>
#include <jet/Render3DCoreOrientedBox.h>

#include <new>

namespace Jet {

// 0x68c10
void RenderPrimitive::CalculateBoxVolume()
{
    void* const raw = Alloc::New(0xdc);
    ParticleOrientedBoundingBox* const volume =
        raw ? ::new (raw) ParticleOrientedBoundingBox(this) : nullptr;
    SetBoundingVolume(volume);

    GeometryChunk* const chunk = chunkArray.block ? chunkArray.block->data[0].Get() : nullptr;
    if (!chunk) return;
    const GeometryChunk::Vector3Ptr view =
        chunk->GetVector3Array(static_cast<VertexItemType>(R3D_ITEM_POSITION), 0);
    const VertexItemPtr<const Vector3, VIPConstHelper> items{view.stride, view.data};
    const OrientedBox box(chunk->GetNumVertices(), items);
    // the original's slot +0x24 is SetLocalVolume(const OrientedBox&)
    void* const bounds = *reinterpret_cast<void* const*>(reinterpret_cast<const char*>(this) + 0x4c);
    if (bounds) {
        using Fn = void(__cdecl*)(void*, const OrientedBox*);
        void** const table = *reinterpret_cast<void***>(bounds);
        reinterpret_cast<Fn>(table[0x24 / 4])(bounds, &box);
    }
}

// 0x65d00: the exported shape overrides are the same tail jump
void RenderBox::CalculatePrimitiveVolume() { CalculateBoxVolume(); }
void RenderLine::CalculatePrimitiveVolume() { CalculateBoxVolume(); }
void RenderPlane::CalculatePrimitiveVolume() { CalculateBoxVolume(); }
void RenderPolygon::CalculatePrimitiveVolume() { CalculateBoxVolume(); }
void RenderPolyline::CalculatePrimitiveVolume() { CalculateBoxVolume(); }
void RenderSpline::CalculatePrimitiveVolume() { CalculateBoxVolume(); }
void RenderTube::CalculatePrimitiveVolume() { CalculateBoxVolume(); }

}  // namespace Jet

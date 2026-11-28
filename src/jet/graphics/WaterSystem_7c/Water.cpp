// Unit U9 of WaterSystem_7c: Water. Behaviour follows the TS12 assembly (addresses per function). The tile and
// light lists are Jet::FastDynamicArray<...,13>; the original copies them chunk by chunk (AppendChunks) and
// clears them without touching the items, except that the destructor deletes every tile through its vtable.
#include <jet/Water.h>

#include <jet/Render3DCore.h>  // AxisAlignedBox
#include <jet/VCall.h>
#include <jet/WaterGeometry.h>
#include <jet/WaterTile.h>

#include <cfloat>

namespace Jet {

namespace {

// the light type is the DisplayLight's int at +4 (DLightTypes)
int LightType(const DisplayLight* light)
{
    return *reinterpret_cast<const int*>(reinterpret_cast<const char*>(light) + 4);
}

// the bounding volume's virtual Add(const AxisAlignedBox&), slot 8
void BoundingVolumeAdd(ParticleBoundingVolume* volume, const AxisAlignedBox& box)
{
    using AddFn = void(__cdecl*)(ParticleBoundingVolume*, const AxisAlignedBox&);
    (*reinterpret_cast<AddFn* const*>(volume))[8](volume, box);
}

}  // namespace

// 0x10003b40
Water::Water()
{
}

// 0x10003a00
Water::Water(const Water& o) : Particle(o)
{
    tiles.AppendChunks(o.tiles);
    lightList.AppendChunks(o.lightList);
}

// 0x10003b70: every tile is deleted through its own vtable (slot 0, flag 1), then both chunk lists are freed
Water::~Water()
{
    for (unsigned long i = 0; i < tiles.Count(); ++i) {
        WaterTile* tile = tiles[i];
        if (tile)
            VDelete(tile);
    }
    lightList.RemoveAll();
    tiles.RemoveAll();
}

// 0x10003a90: no self-assignment guard (the original clears the destination before copying)
Water& __thiscall Water::operator=(const Water& o)
{
    Particle::operator=(o);
    tiles.RemoveAll();
    tiles.AppendChunks(o.tiles);
    lightList.RemoveAll();
    lightList.AppendChunks(o.lightList);
    return *this;
}

// 0x10003b30
bool __thiscall Water::IsEmpty()
{
    return tiles.head == nullptr;
}

// 0x10003cf0: append the tile, then extend the bounding volume by this water's new overall box
void __cdecl Water::AddTile(WaterTile* tile)
{
    tiles.Add(tile);
    Vector3 min, max;
    GetAxisAlignedBoundingBox(min, max);
    AxisAlignedBox box(min, max);
    BoundingVolumeAdd(GetBoundingVolume(), box);
}

// 0x10003d50
void __cdecl Water::RemoveTile(WaterTile* tile)
{
    FastDynamicArray<WaterTile*, 13>::Iterator it = tiles.Find(tile);
    if (it != tiles.End())
        tiles.Remove(it);
    Vector3 min, max;
    GetAxisAlignedBoundingBox(min, max);
    AxisAlignedBox box(min, max);
    BoundingVolumeAdd(GetBoundingVolume(), box);
}

// 0x10003dd0: the light list holds at most one light; a rejected light empties it
bool __cdecl Water::SetLight(DisplayLight* light)
{
    if (!light) {
        if (lightList.head)
            lightList.RemoveAll();
        return true;
    }
    if (LightType(light) != DLIGHT_DEFAULT) {
        if (lightList.head)
            lightList.RemoveAll();
        return false;
    }
    if (lightList.head)
        lightList[0] = light;
    else
        lightList.Add(light);
    return true;
}

// 0x10003e40
DisplayLight* __cdecl Water::GetLight()
{
    if (!lightList.head)
        return nullptr;
    return lightList[0];
}

// 0x10003ea0: hand every tile its light list and cull it around this water's global position
void __cdecl Water::Cull(ViewPoint& view, const Matrix4& matrix)
{
    for (FastDynamicArray<WaterTile*, 13>::Iterator it = tiles.First(); it != tiles.End(); ++it) {
        WaterTile* tile = *it;
        tile->SetLight(&lightList);
        tile->CullTile(view, matrix, GetGlobalPosition());
    }
}

// 0x10003f20: the box around all tiles' geometry, rotated by the water particle's orientation and moved to its
// position. The accumulation and the nine rotation terms keep the assembly's operand and grouping order.
void __cdecl Water::GetAxisAlignedBoundingBox(Vector3& min, Vector3& max)
{
    float minX = FLT_MAX, minY = FLT_MAX, minZ = FLT_MAX;
    float maxX = -FLT_MAX, maxY = -FLT_MAX, maxZ = -FLT_MAX;

    for (FastDynamicArray<WaterTile*, 13>::Iterator it = tiles.First(); it != tiles.End(); ++it) {
        WaterTile* tile = *it;
        const Vector3& pos = tile->GetPosition();
        Vector3 bmin, bmax;
        tile->GetGeometry()->GetBoundingBox(bmin, bmax);

        float v = pos.x + bmax.x;
        if (maxX < v)
            maxX = v;
        v = pos.y + bmax.y;
        if (maxY < v)
            maxY = v;
        v = pos.z + bmax.z;
        if (maxZ < v)
            maxZ = v;
        v = bmin.x + pos.x;
        if (v < minX)
            minX = v;
        v = bmin.y + pos.y;
        if (v < minY)
            minY = v;
        v = bmin.z + pos.z;
        if (v < minZ)
            minZ = v;
    }

    if (minX == FLT_MAX) {
        min = Vector3(0.0f, 0.0f, 0.0f);
        max = Vector3(0.0f, 0.0f, 0.0f);
        return;
    }

    const Quaternion& q = GetOrientation();
    const Vector3& pos = GetPosition();
    const float tx = 2.0f * q.x, ty = 2.0f * q.y, tz = 2.0f * q.z;

    const float m00 = 1.0f - (ty * q.y + tz * q.z);
    const float m01 = q.x * ty - q.w * tz;
    const float m02 = q.x * tz + q.w * ty;
    const float m10 = q.w * tz + q.x * ty;
    const float m11 = 1.0f - (tx * q.x + tz * q.z);
    const float m12 = q.y * tz - tx * q.w;
    const float m20 = q.x * tz - q.w * ty;
    const float m21 = tx * q.w + q.y * tz;
    const float m22 = 1.0f - (tx * q.x + ty * q.y);

    min.x = pos.x + ((m00 * minX + m02 * minZ) + m01 * minY);
    min.y = pos.y + ((m11 * minY + m12 * minZ) + m10 * minX);
    min.z = pos.z + ((m22 * minZ + m21 * minY) + m20 * minX);
    max.x = pos.x + ((m00 * maxX + m02 * maxZ) + m01 * maxY);
    max.y = pos.y + ((m11 * maxY + m12 * maxZ) + m10 * maxX);
    max.z = pos.z + ((m22 * maxZ + m21 * maxY) + m20 * maxX);
}

}  // namespace Jet
